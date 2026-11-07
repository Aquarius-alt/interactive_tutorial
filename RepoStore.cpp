#include "RepoStore.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cwctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <knownfolders.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace fs = std::filesystem;

namespace {
    // UTF-8 -> wstring: std::filesystem::path(std::string) на MSVC читает ANSI, а SDL отдаёт UTF-8
    std::wstring Widen(const std::string& s) {
        if (s.empty()) return std::wstring();
        const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
        if (n <= 0) return std::wstring();
        std::wstring w(size_t(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
        return w;
    }

    std::string Narrow(const std::wstring& w) {
        if (w.empty()) return std::string();
        const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
        if (n <= 0) return std::string();
        std::string s(size_t(n), '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
        return s;
    }

    fs::path P(const std::string& utf8) { return fs::path(Widen(utf8)); }

    // имя файла/папки, допустимое в Windows
    std::string SafeName(const std::string& in, const std::string& fallback) {
        std::string s;
        for (unsigned char c : in) {
            if (c < 32 || c == '<' || c == '>' || c == ':' || c == '"' || c == '/' ||
                c == '\\' || c == '|' || c == '?' || c == '*')
                s.push_back('_');
            else
                s.push_back(char(c));
        }
        while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
        size_t b = 0;
        while (b < s.size() && s[b] == ' ') ++b;
        s.erase(0, b);

        // не обрезаем посередине многобайтового символа UTF-8
        const size_t kMax = 60;
        if (s.size() > kMax) {
            size_t cut = kMax;
            while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;
            s.resize(cut);
            while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
        }
        if (s.empty()) return fallback;

        // зарезервированные имена устройств (CON, NUL, COM1 ...)
        std::string up = s;
        for (char& c : up) c = char(std::toupper(static_cast<unsigned char>(c)));
        const size_t dot = up.find('.');
        if (dot != std::string::npos) up.resize(dot);
        static const char* bad[] = { "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5",
                                     "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4",
                                     "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
        for (const char* b2 : bad) if (up == b2) return "_" + s;
        return s;
    }

    std::string JsonEscape(const std::string& s) {
        std::string o;
        o.reserve(s.size() + 8);
        for (unsigned char c : s) {
            switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if (c < 32) {
                    static const char* hex = "0123456789abcdef";
                    o += "\\u00";
                    o.push_back(hex[c >> 4]);
                    o.push_back(hex[c & 15]);
                }
                else {
                    o.push_back(char(c));
                }
            }
        }
        return o;
    }

    std::string Q(const std::string& s) { return "\"" + JsonEscape(s) + "\""; }

    std::string Now() {
        const std::time_t t = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &t);
        char buf[32];
        std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tm);
        return buf;
    }

    bool WriteText(const fs::path& p, const std::string& text) {
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(text.data(), std::streamsize(text.size()));
        f.flush();
        return bool(f);
    }

    std::string Num(int i) {
        char b[16];
        std::snprintf(b, sizeof b, "%03d", i);
        return b;
    }

    // записывает список конспектов/тестов в <dir>\001_<имя>\...
    bool WriteItems(const fs::path& dir, const std::vector<TheoryItem>& items, bool withAnswers) {
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec) return false;

        for (size_t i = 0; i < items.size(); ++i) {
            const TheoryItem& it = items[i];
            const fs::path d = dir / P(Num(int(i) + 1) + "_" + SafeName(it.title, "untitled"));
            fs::create_directories(d, ec);
            if (ec) return false;

            // вложения: копируем рядом; одинаковые имена из разных папок получают числовой префикс
            std::vector<std::string> names;
            if (!it.files.empty()) {
                const fs::path fd = d / L"files";
                fs::create_directories(fd, ec);
                if (ec) return false;
                for (size_t k = 0; k < it.files.size(); ++k) {
                    const fs::path src = P(it.files[k]);
                    std::string name = SafeName(Narrow(src.filename().wstring()), "file");
                    if (fs::exists(fd / P(name), ec)) name = std::to_string(k + 1) + "_" + name;
                    fs::copy_file(src, fd / P(name), fs::copy_options::overwrite_existing, ec);
                    if (ec) return false;
                    names.push_back(name);
                }
            }

            if (!WriteText(d / L"content.txt", it.content)) return false;

            std::string j = "{\n";
            j += "  \"title\": " + Q(it.title) + ",\n";
            j += "  \"content\": " + Q(it.content) + ",\n";
            if (withAnswers) j += "  \"answers\": " + Q(it.answers) + ",\n";
            if (withAnswers) j += "  \"subjectTags\": " + Q(it.subjectTags) + ",\n  \"rank\": " + Q(it.rank) + ",\n";
            if (withAnswers) {                           // вопросы теста: два параллельных массива
                j += "  \"questions\": [";
                for (size_t k = 0; k < it.questions.size(); ++k) j += (k ? ", " : "") + Q(it.questions[k].text);
                j += "],\n  \"questionAnswers\": [";
                for (size_t k = 0; k < it.questions.size(); ++k) j += (k ? ", " : "") + Q(it.questions[k].answers);
                j += "],\n";
            }
            j += std::string("  \"madeWithAI\": ") + (it.madeWithAI ? "true" : "false") + ",\n";
            j += "  \"files\": [";
            for (size_t k = 0; k < names.size(); ++k) j += (k ? ", " : "") + Q(names[k]);
            j += "]\n}\n";
            if (!WriteText(d / L"item.json", j)) return false;
        }
        return true;
    }

    // читает JSON-строку; p стоит на открывающей кавычке, после вызова - за закрывающей
    std::string ReadQuoted(const std::string& j, size_t& p) {
        std::string out;
        for (++p; p < j.size() && j[p] != '"'; ++p) {
            if (j[p] != '\\' || p + 1 >= j.size()) { out.push_back(j[p]); continue; }
            const char c = j[++p];
            switch (c) {
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u':
                if (p + 4 < j.size()) {
                    out.push_back(char(std::strtol(j.substr(p + 1, 4).c_str(), nullptr, 16)));   // у нас только \u00XX
                    p += 4;
                }
                break;
            default: out.push_back(c);
            }
        }
        if (p < j.size()) ++p;                                  // пропустить закрывающую кавычку
        return out;
    }

    // позиция сразу после "key": (пробелы пропущены) или npos
    size_t JsonValuePos(const std::string& j, const std::string& key) {
        const std::string pat = "\"" + key + "\"";
        size_t p = j.find(pat);
        if (p == std::string::npos) return std::string::npos;
        p = j.find(':', p + pat.size());
        if (p == std::string::npos) return std::string::npos;
        for (++p; p < j.size() && (j[p] == ' ' || j[p] == '\n' || j[p] == '\r' || j[p] == '\t'); ++p) {}
        return p < j.size() ? p : std::string::npos;
    }

    // достаёт строковое поле "key": "..." из repo.json / item.json (файлы пишем сами, формат известен)
    std::string JsonString(const std::string& j, const std::string& key) {
        size_t p = JsonValuePos(j, key);
        if (p == std::string::npos || j[p] != '"') return std::string();
        return ReadQuoted(j, p);
    }

    // целое поле "key": 123 (нет поля или не число -> 0)
    int JsonInt(const std::string& j, const std::string& key) {
        const size_t p = JsonValuePos(j, key);
        if (p == std::string::npos) return 0;
        long long v = 0;
        for (size_t i = p; i < j.size() && j[i] >= '0' && j[i] <= '9'; ++i) v = std::min<long long>(1000000000LL, v * 10 + (j[i] - '0'));
        return int(v);
    }

    bool JsonBool(const std::string& j, const std::string& key) {
        const size_t p = JsonValuePos(j, key);
        return p != std::string::npos && j.compare(p, 4, "true") == 0;
    }

    std::vector<std::string> JsonStringArray(const std::string& j, const std::string& key) {
        std::vector<std::string> out;
        size_t p = JsonValuePos(j, key);
        if (p == std::string::npos || j[p] != '[') return out;
        for (++p; p < j.size() && j[p] != ']';) {
            if (j[p] == '"') out.push_back(ReadQuoted(j, p));
            else ++p;                                            // пробелы и запятые
        }
        return out;
    }

    std::string ReadFile(const fs::path& p) {
        std::ifstream f(p, std::ios::binary);
        if (!f) return std::string();
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }

    // читает <dir>\001_<имя>\item.json по порядку номеров; вложения - полные пути внутри папки
    std::vector<TheoryItem> ReadItems(const fs::path& dir, bool withAnswers) {
        std::vector<TheoryItem> items;
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) return items;

        std::vector<fs::path> dirs;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
            if (it->is_directory(ec)) dirs.push_back(it->path());
        std::sort(dirs.begin(), dirs.end());                    // 001_..., 002_... = порядок добавления

        for (const fs::path& d : dirs) {
            std::ifstream probe(d / L"item.json", std::ios::binary);
            if (!probe) continue;
            probe.close();
            const std::string j = ReadFile(d / L"item.json");

            TheoryItem it;
            it.title = JsonString(j, "title");
            it.content = JsonString(j, "content");
            if (withAnswers) it.answers = JsonString(j, "answers");
            if (withAnswers) {
                it.subjectTags = JsonString(j, "subjectTags");
                it.rank = JsonString(j, "rank");
                const std::vector<std::string> qt = JsonStringArray(j, "questions");
                const std::vector<std::string> qa = JsonStringArray(j, "questionAnswers");
                for (size_t k = 0; k < qt.size(); ++k)
                    it.questions.push_back({ qt[k], k < qa.size() ? qa[k] : std::string() });
            }
            it.madeWithAI = JsonBool(j, "madeWithAI");
            for (const std::string& name : JsonStringArray(j, "files")) {
                const fs::path fp = d / L"files" / P(name);
                if (fs::exists(fp, ec)) it.files.push_back(Narrow(fp.wstring()));
            }
            items.push_back(std::move(it));
        }
        return items;
    }

    // мини-игры: папка каждой игры копируется целиком в games\001_<имя>\, рядом пишется _repo_game.json
    // (title - название, entry - файл запуска). item.files[0] = папка игры.
    bool WriteGames(const fs::path& dir, const std::vector<TheoryItem>& games) {
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec) return false;

        for (size_t i = 0; i < games.size(); ++i) {
            const TheoryItem& g = games[i];
            if (g.files.empty()) continue;
            const fs::path d = dir / P(Num(int(i) + 1) + "_" + SafeName(g.title, "game"));
            fs::create_directories(d, ec);
            if (ec) return false;
            fs::copy(P(g.files[0]), d, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
            if (ec) return false;

            std::string j = "{\n";
            j += "  \"title\": " + Q(g.title) + ",\n";
            j += "  \"entry\": " + Q(g.content) + "\n}\n";
            if (!WriteText(d / L"_repo_game.json", j)) return false;
        }
        return true;
    }

    std::vector<TheoryItem> ReadGames(const fs::path& dir) {
        std::vector<TheoryItem> games;
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) return games;

        std::vector<fs::path> dirs;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
            if (it->is_directory(ec)) dirs.push_back(it->path());
        std::sort(dirs.begin(), dirs.end());

        for (const fs::path& d : dirs) {
            const std::string j = ReadFile(d / L"_repo_game.json");
            TheoryItem it;
            it.title = JsonString(j, "title");
            it.content = JsonString(j, "entry");
            if (it.title.empty()) it.title = Narrow(d.filename().wstring());
            if (it.content.empty()) it.content = "main.py";
            it.files.push_back(Narrow(d.wstring()));
            games.push_back(std::move(it));
        }
        return games;
    }
}

std::string RepoStore_Root() {
    PWSTR raw = nullptr;
    std::wstring docs;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &raw)) && raw)
        docs = raw;
    if (raw) CoTaskMemFree(raw);
    if (docs.empty()) return std::string();
    return Narrow((fs::path(docs) / L"myproject").wstring());
}

// Открывает в Проводнике папку текущего пользователя с сохранёнными репозиториями.
// Папка заданий на ранг лежит внутри папки пользователя, но без repo.json, поэтому
// RepoStore_List её репозиторием не считает.
bool RepoStore_OpenRanksFolder(const std::string& user) {
    try {
        const std::string rootStr = RepoStore_Root();
        if (rootStr.empty() || user.empty()) return false;

        const fs::path dir = P(rootStr) / P(SafeName(user, "user")) / L"_ranks";
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec) return false;

        const auto result = reinterpret_cast<INT_PTR>(
            ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        return result > 32;
    }
    catch (...) {
        return false;
    }
}

// Задание на ранг = тест (title, вопросы, теги предмета, звание, вложения), сохранённый одной папкой
// <Documents>\myproject\<логин>\_ranks\<название>\ (item.json, content.txt, files\).
// Запись идёт во временную папку и переименовывается только при успехе. Если задание с таким
// названием уже есть, добавляется " (2)", " (3)" и т.д.
bool RepoStore_SaveRankTask(const std::string& user, const TheoryItem& task) {
    try {
        const std::string rootStr = RepoStore_Root();
        if (rootStr.empty() || user.empty()) return false;

        std::error_code ec;
        const fs::path ranksDir = P(rootStr) / P(SafeName(user, "user")) / L"_ranks";
        fs::create_directories(ranksDir, ec);
        if (ec) return false;

        const std::string base = SafeName(task.title, "task");
        fs::path finalDir = ranksDir / P(base);
        for (int n = 2; fs::exists(finalDir, ec) && n < 1000; ++n)
            finalDir = ranksDir / P(base + " (" + std::to_string(n) + ")");
        if (fs::exists(finalDir, ec)) return false;

        fs::path tmp = finalDir;
        tmp += L".tmp";
        fs::remove_all(tmp, ec);

        // WriteItems кладёт задание в <tmp>\001_<имя>\; затем эту папку переносим на место итоговой
        if (!WriteItems(tmp, std::vector<TheoryItem>{ task }, true)) {
            fs::remove_all(tmp, ec);
            return false;
        }
        const fs::path inner = tmp / P(Num(1) + "_" + SafeName(task.title, "untitled"));
        fs::rename(inner, finalDir, ec);
        std::error_code e2;
        fs::remove_all(tmp, e2);
        return !ec;
    }
    catch (...) {
        return false;
    }
}

std::vector<RankTask> RepoStore_ListRankTasks(const std::string& user) {
    std::vector<RankTask> out;
    try {
        const std::string rootStr = RepoStore_Root();
        if (rootStr.empty() || user.empty()) return out;
        const fs::path dir = P(rootStr) / P(SafeName(user, "user")) / L"_ranks";

        // те же папки, в том же порядке, что пропускает ReadItems (только с item.json): берём из них ключи
        std::error_code ec;
        std::vector<fs::path> dirs;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
            if (it->is_directory(ec) && fs::exists(it->path() / L"item.json", ec)) dirs.push_back(it->path());
        std::sort(dirs.begin(), dirs.end());

        std::vector<TheoryItem> items = ReadItems(dir, true);
        for (size_t i = 0; i < items.size() && i < dirs.size(); ++i)
            out.push_back({ std::move(items[i]), Narrow(dirs[i].filename().wstring()) });
    }
    catch (...) {}
    return out;
}

bool RepoStore_OpenUserFolder(const std::string& user) {
    try {
        const std::string rootStr = RepoStore_Root();
        if (rootStr.empty() || user.empty()) return false;

        const fs::path userDir = P(rootStr) / P(SafeName(user, "user"));
        std::error_code ec;
        fs::create_directories(userDir, ec);
        if (ec) return false;

        const auto result = reinterpret_cast<INT_PTR>(
            ShellExecuteW(nullptr, L"open", userDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        return result > 32;
    }
    catch (...) {
        return false;
    }
}

bool RepoStore_Save(const std::string& user, const CreateData& d, std::string* savedPath,
                    const std::string& replaceDir) {
    try {
        const std::string rootStr = RepoStore_Root();
        if (rootStr.empty()) return false;

        std::error_code ec;
        const fs::path userDir = P(rootStr) / P(SafeName(user, "user"));
        fs::create_directories(userDir, ec);
        if (ec) return false;

        // правка: старая папка остаётся нетронутой до самого конца (вложения копируются из неё),
        // а её имя при выборе нового считается свободным
        const fs::path oldDir = replaceDir.empty() ? fs::path() : P(replaceDir);
        const bool editing = !oldDir.empty() && fs::is_directory(oldDir, ec);
        auto taken = [&](const fs::path& p) {
            if (editing && fs::equivalent(p, oldDir, ec)) return false;
            return fs::exists(p, ec);
        };

        // свободное имя: "Название", "Название (2)", ...
        const std::string base = SafeName(d.name, "repository");
        fs::path finalDir = userDir / P(base);
        for (int n = 2; taken(finalDir) && n < 1000; ++n)
            finalDir = userDir / P(base + " (" + std::to_string(n) + ")");
        if (taken(finalDir)) return false;

        // сначала во временную папку, потом переименование
        fs::path tmp = finalDir;
        tmp += L".tmp";
        fs::remove_all(tmp, ec);
        fs::create_directories(tmp, ec);
        if (ec) return false;

        auto fail = [&]() { std::error_code e2; fs::remove_all(tmp, e2); return false; };

        // картинка-обложка
        std::string coverName;
        if (!d.imagePath.empty()) {
            const fs::path src = P(d.imagePath);
            std::wstring ext = src.extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return wchar_t(std::towlower(c)); });
            if (ext != L".png" && ext != L".bmp") ext = L".png";
            coverName = "cover" + Narrow(ext);
            fs::copy_file(src, tmp / P(coverName), fs::copy_options::overwrite_existing, ec);
            if (ec) return fail();
        }

        if (!WriteItems(tmp / L"theory", d.theory, false)) return fail();
        if (!WriteItems(tmp / L"tests", d.tests, true)) return fail();
        if (!WriteGames(tmp / L"games", d.games)) return fail();

        // при правке сохраняем исходную дату создания и время repo.json (по нему сортируется список,
        // поэтому отредактированный репозиторий остаётся на своём месте, а не уезжает в конец)
        std::string created = Now();
        fs::file_time_type oldStamp{};
        bool haveStamp = false;
        if (editing) {
            const std::string old = JsonString(ReadFile(oldDir / L"repo.json"), "created");
            if (!old.empty()) created = old;
            oldStamp = fs::last_write_time(oldDir / L"repo.json", ec);
            haveStamp = !ec;
        }

        std::string j = "{\n";
        j += "  \"name\": " + Q(d.name) + ",\n";
        j += "  \"description\": " + Q(d.description) + ",\n";
        j += "  \"author\": " + Q(user) + ",\n";
        j += "  \"created\": " + Q(created) + ",\n";
        j += "  \"cover\": " + Q(coverName) + ",\n";
        j += "  \"mainXp\": " + std::to_string(std::max(0, d.mainXp)) + ",\n";
        j += "  \"subjectXp\": " + std::to_string(std::max(0, d.subjectXp)) + ",\n";
        j += "  \"subjectTag\": " + Q(d.subjectTag) + ",\n";
        j += "  \"theoryCount\": " + std::to_string(d.theory.size()) + ",\n";
        j += "  \"testsCount\": " + std::to_string(d.tests.size()) + ",\n";
        j += "  \"gamesCount\": " + std::to_string(d.games.size()) + "\n}\n";
        if (!WriteText(tmp / L"repo.json", j)) return fail();
        if (haveStamp) fs::last_write_time(tmp / L"repo.json", oldStamp, ec);

        if (editing) {
            // старую версию убираем только после того, как новая полностью записана;
            // если переименование не выйдет, старая возвращается на место
            fs::path bak = oldDir;
            bak += L".bak";
            fs::remove_all(bak, ec);
            fs::rename(oldDir, bak, ec);
            if (ec) return fail();
            fs::rename(tmp, finalDir, ec);
            if (ec) {
                std::error_code e2;
                fs::rename(bak, oldDir, e2);
                return fail();
            }
            fs::remove_all(bak, ec);                              // не критично, если не удалилось сейчас
        }
        else {
            fs::rename(tmp, finalDir, ec);
            if (ec) return fail();
        }

        if (savedPath) *savedPath = Narrow(finalDir.wstring());
        return true;
    }
    catch (...) {
        return false;
    }
}

std::vector<RepoInfo> RepoStore_List(const std::string& user) {
    std::vector<RepoInfo> out;
    try {
        const std::string rootStr = RepoStore_Root();
        if (rootStr.empty()) return out;

        std::error_code ec;
        const fs::path userDir = P(rootStr) / P(SafeName(user, "user"));
        if (!fs::is_directory(userDir, ec)) return out;

        for (fs::directory_iterator it(userDir, ec), end; !ec && it != end; it.increment(ec)) {
            const fs::path dir = it->path();
            if (!it->is_directory(ec) || dir.extension() == L".tmp" || dir.extension() == L".bak") continue;

            std::ifstream f(dir / L"repo.json", std::ios::binary);
            if (!f) continue;                                   // не репозиторий
            const std::string j((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

            RepoInfo info;
            info.name = JsonString(j, "name");
            info.dir = Narrow(dir.wstring());
            if (info.name.empty()) info.name = Narrow(dir.filename().wstring());
            const std::string cover = JsonString(j, "cover");
            if (!cover.empty() && fs::exists(dir / P(cover), ec)) info.coverPath = Narrow((dir / P(cover)).wstring());
            const auto t = fs::last_write_time(dir / L"repo.json", ec);
            if (!ec) info.created = (long long)t.time_since_epoch().count();
            out.push_back(std::move(info));
        }
        std::sort(out.begin(), out.end(), [](const RepoInfo& a, const RepoInfo& b) { return a.created < b.created; });
    }
    catch (...) {
    }
    return out;
}

bool RepoStore_Delete(const std::string& dir) {
    try {
        if (dir.empty()) return false;
        std::error_code ec;
        const fs::path p = P(dir);
        // защита: удаляем только папку, в которой лежит repo.json (то есть настоящий репозиторий)
        if (!fs::is_regular_file(p / L"repo.json", ec)) return false;
        fs::remove_all(p, ec);
        return !ec && !fs::exists(p, ec);
    }
    catch (...) {
        return false;
    }
}

bool RepoStore_Load(const std::string& dir, CreateData& out) {
    try {
        std::error_code ec;
        const fs::path p = P(dir);
        std::ifstream probe(p / L"repo.json", std::ios::binary);
        if (!probe) return false;
        probe.close();
        const std::string j = ReadFile(p / L"repo.json");

        CreateData d;
        d.name = JsonString(j, "name");
        if (d.name.empty()) d.name = Narrow(p.filename().wstring());
        d.description = JsonString(j, "description");
        d.mainXp = JsonInt(j, "mainXp");
        d.subjectXp = JsonInt(j, "subjectXp");
        d.subjectTag = JsonString(j, "subjectTag");
        const std::string cover = JsonString(j, "cover");
        if (!cover.empty() && fs::exists(p / P(cover), ec)) d.imagePath = Narrow((p / P(cover)).wstring());
        d.theory = ReadItems(p / L"theory", false);
        d.tests = ReadItems(p / L"tests", true);
        d.games = ReadGames(p / L"games");
        out = std::move(d);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool RepoStore_InspectGame(const std::string& folder, std::string* name, std::string* entry, std::string* error) {
    auto fail = [&](const char* msg) { if (error) *error = msg; return false; };
    try {
        fs::path dir = P(folder);
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) return fail("GAME FOLDER NOT FOUND");

        std::string ent = "main.py", nm;
        if (fs::is_regular_file(dir / L"manifest.json", ec)) {
            const std::string j = ReadFile(dir / L"manifest.json");
            nm = JsonString(j, "name");
            const std::string e2 = JsonString(j, "entry");
            if (!e2.empty()) ent = e2;
        }
        // файл запуска - путь внутри папки игры: без "..", без диска и без начального слэша
        if (ent.find("..") != std::string::npos || ent.find(':') != std::string::npos ||
            ent[0] == '/' || ent[0] == '\\')
            return fail("BAD ENTRY FILE");
        if (!fs::is_regular_file(dir / P(ent), ec))
            return fail(ent == "main.py" ? "MAIN.PY NOT FOUND" : "ENTRY FILE NOT FOUND");

        // защита от случайно выбранной огромной папки (рабочий стол, диск): копируем игры целиком
        const unsigned long long kMaxBytes = 50ull * 1024 * 1024;
        const size_t kMaxFiles = 500;
        unsigned long long total = 0;
        size_t count = 0;
        for (fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
             !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec)) continue;
            total += it->file_size(ec);
            if (++count > kMaxFiles || total > kMaxBytes) return fail("GAME FOLDER TOO LARGE");
        }

        if (nm.empty()) {
            if (!dir.has_filename()) dir = dir.parent_path();      // путь мог кончаться на слэш
            nm = Narrow(dir.filename().wstring());
        }
        if (nm.empty()) nm = "game";
        if (name) *name = nm;
        if (entry) *entry = ent;
        return true;
    }
    catch (...) {
        return fail("GAME FOLDER NOT FOUND");
    }
}

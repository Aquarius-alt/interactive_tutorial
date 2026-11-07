#include "Level.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {
    struct Tier {
        int         from;      // первый уровень группы
        long long   cost;      // опыт на один уровень внутри группы
        const char* en;
        const char* ru;
        unsigned char r, g, b; // цвет группы
    };

    const Tier kTiers[] = {
        {    1,  100, "NEWBIE",     "НОВИЧОК",    150, 170, 190 },
        {   10,  100, "LARPER",     "ЛАРПЕР",     120, 220, 140 },
        {   20,  200, "NORMIE",     "НОРМИС",      96, 228, 232 },
        {   50,  250, "BRONZE",     "БРОНЗА",     205, 127,  50 },
        {  100,  300, "SILVER",     "СЕРЕБРО",    205, 210, 220 },
        {  150,  500, "GOLD",       "ЗОЛОТО",     255, 205,  60 },
        {  200,  750, "PLATINUM",   "ПЛАТИНА",    140, 235, 225 },
        {  300, 1000, "DIAMOND",    "АЛМАЗ",      130, 170, 255 },
        {  400, 1500, "140 IQ",     "140IQ",      200, 120, 255 },
        {  500, 2000, "LEGEND",     "ЛЕГЕНДА",    255, 140,  60 },
        {  750, 2500, "DEITY",      "БОЖЕСТВО",   255, 235, 150 },
        { 1000, 3000, "PSYCHO",     "ПСИХ",       255,  70,  90 },
        { 2000, 4000, "CYBERPSYCHO","КИБЕРПСИХ",  255,  40, 200 },
        { 5000, 5000, "NOT HUMAN",  "НЕ ЧЕЛОВЕК", 255, 255, 255 },
    };
    const int kTierCount = int(sizeof(kTiers) / sizeof(kTiers[0]));

    const long long kMaxXp = 4000000000000LL;   // запас от переполнения

    std::string UserFile(const std::string& login, const char* prefix) {
        static const char* k = "0123456789abcdef";
        std::string hex;
        for (char c : login) {                  // как у аватара: hex логина без учёта регистра
            const unsigned char u = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(c)));
            hex.push_back(k[u >> 4]);
            hex.push_back(k[u & 15]);
        }
        char* p = SDL_GetPrefPath("myproject", "login");
        std::string s = p ? p : "";
        SDL_free(p);
        return s.empty() ? s : s + prefix + hex + ".dat";
    }

    std::string XpPath(const std::string& login) { return UserFile(login, "xp_"); }
}

LevelInfo Level_FromXp(long long totalXp) {
    long long rest = std::clamp<long long>(totalXp, 0, kMaxXp);

    LevelInfo info;
    for (int t = 0; t < kTierCount; ++t) {
        const Tier& tier = kTiers[t];
        info.tier = t;
        info.xpNeed = tier.cost;

        if (t + 1 < kTierCount) {
            const long long levels = kTiers[t + 1].from - tier.from;
            const long long span = levels * tier.cost;       // опыта на всю группу
            if (rest >= span) { rest -= span; continue; }    // группа пройдена целиком
        }
        info.level = tier.from + int(rest / tier.cost);
        info.xpInto = rest % tier.cost;
        return info;
    }
    return info;   // недостижимо: последняя группа без верхней границы
}

const char* Level_TierName(int tier, bool russian) {
    tier = std::clamp(tier, 0, kTierCount - 1);
    return russian ? kTiers[tier].ru : kTiers[tier].en;
}

void Level_TierColor(int tier, unsigned char& r, unsigned char& g, unsigned char& b) {
    tier = std::clamp(tier, 0, kTierCount - 1);
    r = kTiers[tier].r; g = kTiers[tier].g; b = kTiers[tier].b;
}

long long Level_GetXp(const std::string& login) {
    const std::string path = XpPath(login);
    if (path.empty()) return 0;
    std::ifstream in(path, std::ios::binary);
    long long xp = 0;
    if (!(in >> xp)) return 0;
    return std::clamp<long long>(xp, 0, kMaxXp);
}

bool Level_AddXp(const std::string& login, long long xp) {
    const std::string path = XpPath(login);
    if (path.empty() || xp <= 0) return false;
    const long long total = std::min(kMaxXp, Level_GetXp(login) + xp);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << total;
    out.flush();
    return out.good();
}

namespace {
    std::string Lower(std::string v) {
        for (char& c : v) c = char(std::tolower(static_cast<unsigned char>(c)));
        return v;
    }

    // в списке засчитанных уже есть такой ключ?
    bool IsClaimed(const std::string& path, const std::string& key) {
        std::ifstream in(path, std::ios::binary);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (Lower(line) == key) return true;
        }
        return false;
    }

    void Claim(const std::string& path, const std::string& key) {
        std::ofstream out(path, std::ios::binary | std::ios::app);
        if (out) { out << key << '\n'; out.flush(); }
    }

    std::string CleanTag(std::string t) {
        for (char& c : t) if (c == '\t' || c == '\r' || c == '\n') c = ' ';
        const size_t a = t.find_first_not_of(' ');
        if (a == std::string::npos) return "";
        return t.substr(a, t.find_last_not_of(' ') - a + 1);
    }

    // "math; Python" -> {"math", "Python"}: теги через ";" - разные; пустые и повторы (без учёта регистра) отбрасываются
    std::vector<std::string> SplitTags(const std::string& text) {
        std::vector<std::string> out;
        size_t start = 0;
        for (;;) {
            const size_t p = text.find(';', start);
            const std::string t = CleanTag(text.substr(start, p == std::string::npos ? std::string::npos : p - start));
            if (!t.empty()) {
                bool dup = false;
                for (const std::string& o : out) if (Lower(o) == Lower(t)) { dup = true; break; }
                if (!dup) out.push_back(t);
            }
            if (p == std::string::npos) break;
            start = p + 1;
        }
        return out;
    }

    bool SaveTags(const std::string& login, const std::vector<TagLevel>& tags) {
        const std::string path = UserFile(login, "tags_");
        if (path.empty()) return false;
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        for (const TagLevel& t : tags) out << t.xp << '\t' << t.tag << '\n';
        out.flush();
        return out.good();
    }
}

bool Level_AwardRepo(const std::string& login, const std::string& repoKey, long long xp) {
    const std::string path = UserFile(login, "xpdone_");
    if (path.empty() || repoKey.empty() || xp <= 0) return false;

    const std::string key = Lower(repoKey);
    if (IsClaimed(path, key)) return false;           // уже засчитан
    if (!Level_AddXp(login, xp)) return false;
    Claim(path, key);                                  // не записалось - редкий случай, опыт уже начислен
    return true;
}

std::vector<TagLevel> Level_GetTags(const std::string& login) {
    std::vector<TagLevel> tags;
    const std::string path = UserFile(login, "tags_");
    if (path.empty()) return tags;

    std::ifstream in(path, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        const long long xp = std::clamp<long long>(std::strtoll(line.substr(0, tab).c_str(), nullptr, 10), 0, kMaxXp);
        for (const std::string& name : SplitTags(line.substr(tab + 1))) {   // старая запись "a; b" делится на два тега
            bool merged = false;
            for (TagLevel& t : tags)
                if (Lower(t.tag) == Lower(name)) { t.xp = std::min(kMaxXp, t.xp + xp); merged = true; break; }
            if (!merged) tags.push_back({ name, xp });
        }
    }
    std::stable_sort(tags.begin(), tags.end(), [](const TagLevel& a, const TagLevel& b) { return a.xp > b.xp; });
    return tags;
}

bool Level_AwardSubject(const std::string& login, const std::string& repoKey,
                        const std::string& tag, long long xp) {
    const std::string path = UserFile(login, "xpdone_");
    const std::string clean = CleanTag(tag);
    if (path.empty() || repoKey.empty() || SplitTags(clean).empty() || xp <= 0) return false;

    const std::string key = Lower(repoKey) + "#subject";   // отдельная отметка: не мешает основному опыту
    if (IsClaimed(path, key)) return false;

    std::vector<TagLevel> tags = Level_GetTags(login);
    for (const std::string& name : SplitTags(clean)) {       // каждый тег из "a; b" получает полный опыт предмета
        bool found = false;
        for (TagLevel& t : tags)
            if (Lower(t.tag) == Lower(name)) { t.xp = std::min(kMaxXp, t.xp + xp); found = true; break; }
        if (!found) tags.push_back({ name, std::min(kMaxXp, xp) });
    }
    if (!SaveTags(login, tags)) return false;
    Claim(path, key);
    return true;
}

int Level_CompletedCount(const std::string& login) {
    const std::string path = UserFile(login, "xpdone_");
    if (path.empty()) return 0;
    std::ifstream in(path, std::ios::binary);
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line.size() >= 8 && line.compare(line.size() - 8, 8, "#subject") == 0) continue;   // отметка опыта предмета
        ++n;
    }
    return n;
}

// ---- Полученные ранги ----
namespace {
    // строка файла рангов "ключ \t теги \t звание" -> части; false = строка битая
    bool SplitRankLine(std::string line, std::string& key, std::string& tag, std::string& rank) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t p1 = line.find('\t');
        if (p1 == std::string::npos) return false;
        const size_t p2 = line.find('\t', p1 + 1);
        if (p2 == std::string::npos) return false;
        key = line.substr(0, p1);
        tag = line.substr(p1 + 1, p2 - p1 - 1);
        rank = line.substr(p2 + 1);
        return true;
    }
}

std::vector<RankEntry> Level_GetRanks(const std::string& login) {
    std::vector<RankEntry> out;
    const std::string path = UserFile(login, "ranks_");
    if (path.empty()) return out;
    std::ifstream in(path, std::ios::binary);
    std::string line, key, tag, rank;
    while (std::getline(in, line))
        if (SplitRankLine(line, key, tag, rank) && !rank.empty()) out.push_back({ tag, rank });
    return out;
}

bool Level_HasRank(const std::string& login, const std::string& taskKey) {
    const std::string path = UserFile(login, "ranks_");
    const std::string k = Lower(CleanTag(taskKey));
    if (path.empty() || k.empty()) return false;
    std::ifstream in(path, std::ios::binary);
    std::string line, key, tag, rank;
    while (std::getline(in, line))
        if (SplitRankLine(line, key, tag, rank) && key == k) return true;
    return false;
}

bool Level_AwardRank(const std::string& login, const std::string& taskKey,
                     const std::string& tag, const std::string& rank) {
    const std::string path = UserFile(login, "ranks_");
    const std::string k = Lower(CleanTag(taskKey));
    const std::string rk = CleanTag(rank), tg = CleanTag(tag);
    if (path.empty() || k.empty() || rk.empty()) return false;
    if (Level_HasRank(login, taskKey)) return false;
    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out) return false;
    out << k << '\t' << tg << '\t' << rk << '\n';
    out.flush();
    return out.good();
}

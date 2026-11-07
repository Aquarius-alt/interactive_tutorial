#include "TheoryDialog.h"
#include "TextButton.h"
#include "TextField.h"
#include "Text.h"
#include "TextArea.h"
#include "Localization.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <string>

namespace {
    const Uint8  kR = 96, kG = 228, kB = 232;     // цвет неона, как у кнопок
    const size_t kTitleMax = 60;
    const size_t kBodyMax = 3000;
    const size_t kMaxFiles = 10;
    const size_t kAnswersMax = 200;
    const size_t kTagsMax = 60;
    const size_t kRankMax = 30;

    bool        g_open = false;
    bool        g_laid = false;                   // true после первой отрисовки (до неё позиции элементов неверны)
    bool        g_init = false;
    SDL_Window* g_window = nullptr;

    TextButton  g_panel{ 0, 0, 100, 100, "" };    // неоновая рамка окна
    TextField   g_title;                          // (3) заголовок
    TextArea    g_body;                           // (4) содержание
    TextField   g_answers;                        // верные ответы (только в окне теста)
    TextField   g_tags;                           // теги предмета (только в окне теста)
    TextField   g_rank;                           // звание (только в окне теста)
    bool        g_withAnswers = false;            // true = окно теста
    TextButton  g_attach{ 0, 0, 140, 40, "ATTACH" };         // (5) добавить вложение
    TextButton  g_ai{ 0, 0, 230, 40, "MADE WITH AI" };       // (6) переключатель
    TextButton  g_add{ 0, 0, 120, 40, "ADD" };               // (7) добавить
    bool        g_aiOn = false;
    bool        g_editing = false;                // true = правим готовую теорию (кнопка SAVE)

    // ---- тест: вопросы внутри окна ----
    const size_t kQuestionMax = 600;
    const size_t kMaxQuestions = 100;
    const int    kPool = 64;                              // сколько строк вопросов можно нарисовать за раз
    std::vector<TestQuestion> g_questions;
    bool        g_qMode = false;                          // true = открыт редактор вопроса
    int         g_qIdx = -1;                              // какой вопрос правится (-1 = новый)
    int         g_qScroll = 0;                            // первая видимая строка списка (вопросы + "+")
    int         g_qVis = 0;                               // сколько строк вопросов нарисовано в последнем кадре
    bool        g_qPlusShown = false;
    TextButton  g_qBtn[64];                               // строки вопросов (переиспользуются при прокрутке)
    TextButton  g_qDel[64];                               // кнопки X
    std::string g_qLabel[64];
    TextButton  g_qPlus{ 0, 0, 100, 40, "" };             // "+" после последнего вопроса
    TextButton  g_qBack{ 0, 0, 120, 40, "BACK" };
    TextButton  g_qOk{ 0, 0, 120, 40, "OK" };

    std::vector<std::string> g_files;
    std::string g_err;
    float       g_mx = -1.f, g_my = -1.f;

    // файлы выбираются в системном диалоге; колбэк может прийти из другого потока,
    // поэтому пути передаём через мьютекс и разбираем в Theory_Poll() в основном потоке
    std::mutex               g_mutex;
    std::vector<std::string> g_pending;
    std::atomic<bool>        g_hasPending{ false };
    std::atomic<bool>        g_dialogOpen{ false };

    void SDLCALL OnFilesChosen(void*, const char* const* list, int) {
        if (list && list[0]) {                  // NULL = ошибка, пустой список = отмена
            std::lock_guard<std::mutex> lock(g_mutex);
            for (const char* const* p = list; *p; ++p) g_pending.emplace_back(*p);
            g_hasPending = true;
        }
        g_dialogOpen = false;
    }

    void ChooseFiles() {
        if (g_dialogOpen.exchange(true)) return;   // диалог уже открыт
        SDL_ShowOpenFileDialog(OnFilesChosen, nullptr, g_window, nullptr, 0, nullptr, true);   // любые файлы, можно несколько
    }

    void EnsureInit() {
        if (g_init) return;
        g_init = true;
        g_title.placeholder = UiText("TITLE", "ЗАГОЛОВОК");
        g_title.maxLen = kTitleMax;
        g_body.placeholder = UiText("CONTENT", "СОДЕРЖАНИЕ");
        g_body.maxLen = kBodyMax;
        g_answers.placeholder = UiText("CORRECT ANSWERS", "ПРАВИЛЬНЫЕ ОТВЕТЫ");
        g_answers.maxLen = kAnswersMax;
        g_tags.placeholder = UiText("SUBJECT TAGS", "ТЕГИ ПРЕДМЕТА");
        g_tags.maxLen = kTagsMax;
        g_rank.placeholder = UiText("RANK", "ЗВАНИЕ");
        g_rank.maxLen = kRankMax;
    }

    void Focus(int i) {
        g_title.focused = (i == 0);
        g_body.focused = (i == 1);
        g_answers.focused = (i == 2);
        g_tags.focused = (i == 3);
        g_rank.focused = (i == 4);
    }

    bool Inside(const TextButton& b, float mx, float my) {
        return mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h;
    }

    // имя файла без папок; встроенный шрифт умеет только ASCII, остальное заменяем на '?'
    std::string BaseName(const std::string& path) {
        const size_t i = path.find_last_of("/\\");
        std::string n = (i == std::string::npos) ? path : path.substr(i + 1);
        return n;
    }

    std::string AttachSummary(size_t maxChars) {
        if (g_files.empty()) return {};
        std::string s = std::string(UiText("FILES ", "ФАЙЛЫ ")) + std::to_string(g_files.size()) + ": ";
        for (size_t i = 0; i < g_files.size(); ++i) {
            if (i) s += ", ";
            s += BaseName(g_files[i]);
        }
        if (Utf8Count(s) > maxChars) {
            const size_t keep = maxChars > 3 ? maxChars - 3 : maxChars;
            size_t byteEnd = 0;
            size_t chars = 0;
            while (byteEnd < s.size() && chars < keep) {
                const unsigned char c = static_cast<unsigned char>(s[byteEnd]);
                byteEnd += (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : 4));
                ++chars;
            }
            s.resize(byteEnd);
            if (maxChars > 3) s += "...";
        }
        return s;
    }

    // ---------- тест: список вопросов и редактор вопроса ----------
    std::string OneLine(const std::string& t) {
        std::string o = t;
        for (char& c : o) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        return o;
    }

    void DrawPlusSign(SDL_Renderer* r, const TextButton& b) {
        const float cx = std::floor(b.x + b.w * 0.5f), cy = std::floor(b.y + b.h * 0.5f);
        const float len = 18.f, th = 3.f;
        const Uint8 R = b.hovered ? 200 : kR, G = b.hovered ? 255 : kG, B = b.hovered ? 255 : kB;
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (int i = 3; i >= 1; --i) {
            SDL_SetRenderDrawColor(r, R, G, B, Uint8(34.f * (1.f - i / 4.f)));
            const SDL_FRect hor = { cx - len / 2 - i, cy - th / 2 - i, len + 2.f * i, th + 2.f * i };
            const SDL_FRect ver = { cx - th / 2 - i, cy - len / 2 - i, th + 2.f * i, len + 2.f * i };
            SDL_RenderFillRect(r, &hor);
            SDL_RenderFillRect(r, &ver);
        }
        SDL_SetRenderDrawColor(r, R, G, B, 255);
        const SDL_FRect hor = { cx - len / 2, cy - th / 2, len, th };
        const SDL_FRect ver = { cx - th / 2, cy - len / 2, th, len };
        SDL_RenderFillRect(r, &hor);
        SDL_RenderFillRect(r, &ver);
    }

    // список вопросов между заголовком и нижним рядом кнопок: строки + в конце "+"; колесо мыши листает
    void DrawQuestionList(SDL_Renderer* r, float ix, float iw, float top, float bottom) {
        const float rowH = 40.f, rgap = 8.f, delW = 40.f;
        const int total = int(g_questions.size()) + 1;                 // +1 = строка с "+"
        int vis = int(std::floor((bottom - top + rgap) / (rowH + rgap)));
        vis = std::clamp(vis, 1, kPool);
        g_qScroll = std::clamp(g_qScroll, 0, std::max(0, total - vis));
        g_qVis = 0;
        g_qPlusShown = false;

        for (int k = 0; k < vis; ++k) {
            const int idx = g_qScroll + k;
            if (idx >= total) break;
            const float y = top + float(k) * (rowH + rgap);

            if (idx == int(g_questions.size())) {                     // "+"
                g_qPlus.x = ix; g_qPlus.y = y; g_qPlus.w = iw; g_qPlus.h = rowH;
                g_qPlus.hovered = Inside(g_qPlus, g_mx, g_my);
                TextButton_Draw(r, g_qPlus);
                DrawPlusSign(r, g_qPlus);
                g_qPlusShown = true;
            }
            else {
                const std::string t = OneLine(g_questions[size_t(idx)].text);
                g_qLabel[k] = std::to_string(idx + 1) + ". " + (t.empty() ? std::string("...") : t);
                g_qBtn[k].label = g_qLabel[k].c_str();
                g_qBtn[k].x = ix; g_qBtn[k].y = y; g_qBtn[k].w = std::max(40.f, iw - delW - 8.f); g_qBtn[k].h = rowH;
                g_qDel[k].label = "X";
                g_qDel[k].x = ix + iw - delW; g_qDel[k].y = y; g_qDel[k].w = delW; g_qDel[k].h = rowH;
                g_qBtn[k].hovered = Inside(g_qBtn[k], g_mx, g_my);
                g_qDel[k].hovered = Inside(g_qDel[k], g_mx, g_my);
                TextButton_Draw(r, g_qBtn[k]);
                TextButton_Draw(r, g_qDel[k]);
                g_qVis = k + 1;
            }
        }

        // подсказка, что список длиннее окна: счётчик "первая-последняя / всего" справа под списком
        if (total > vis) {
            const std::string s = std::to_string(g_qScroll + 1) + "-" + std::to_string(std::min(total, g_qScroll + vis)) +
                                  "/" + std::to_string(total);
            DrawUtf8Text(r, s, ix + iw - Utf8TextWidth(s, 12), bottom + 4.f, 12, kR, kG, kB, 170);
        }
    }

    void OpenQuestion(int idx) {
        g_qIdx = idx;
        g_qMode = true;
        g_body.text = (idx >= 0) ? g_questions[size_t(idx)].text : std::string();
        g_answers.text = (idx >= 0) ? g_questions[size_t(idx)].answers : std::string();
        g_body.scroll = 0;
        g_body.maxLen = kQuestionMax;
        g_err.clear();
        g_qBack.hovered = g_qOk.hovered = false;
        g_laid = false;                                // позиции редактора появятся после отрисовки
        Focus(1);
    }

    void ExitQuestion() {
        g_qMode = false;
        g_qIdx = -1;
        g_body.text.clear();
        g_answers.text.clear();
        g_body.scroll = 0;
        g_err.clear();
        g_laid = false;
        Focus(0);
    }

    void CommitQuestion() {
        if (g_body.text.find_first_not_of(" \n\r\t") == std::string::npos) {   // текст вопроса обязателен
            g_err = "ENTER QUESTION";
            Focus(1);
            return;
        }
        const TestQuestion q{ g_body.text, g_answers.text };
        if (g_qIdx >= 0 && g_qIdx < int(g_questions.size())) {
            g_questions[size_t(g_qIdx)] = q;
        }
        else {
            g_questions.push_back(q);
            g_qScroll = int(g_questions.size());                              // прокрутить так, чтобы "+" был виден
        }
        ExitQuestion();
    }

    void DrawQuestionEditor(SDL_Renderer* r, float py, float ph, float pad, float ix, float iw) {
        const float btnH = 40.f, gap = 12.f;
        const int number = (g_qIdx >= 0 ? g_qIdx : int(g_questions.size())) + 1;
        const std::string head = std::string(UiText("QUESTION ", "ВОПРОС ")) + std::to_string(number);
        DrawUtf8Text(r, head, ix, py + pad + 12.f, 16, kR, kG, kB, 255);

        const float rowY = py + ph - pad - btnH;
        g_answers.placeholder = UiText("CORRECT ANSWERS", "ПРАВИЛЬНЫЕ ОТВЕТЫ");
        g_body.placeholder = UiText("QUESTION", "ВОПРОС");
        g_answers.x = ix; g_answers.w = iw; g_answers.h = 40.f;
        g_answers.y = rowY - 14.f - g_answers.h;

        const float counterH = 22.f;
        const float by = py + pad + 40.f + 6.f;
        g_body.x = ix; g_body.y = by; g_body.w = iw;
        g_body.h = std::max(60.f, g_answers.y - 8.f - counterH - by);
        TextArea_Draw(r, g_body);
        TextField_Draw(r, g_answers);

        if (!g_err.empty()) {
            const std::string shownError = UiMessage(g_err);
            DrawUtf8TextFit(r, shownError, ix, g_body.y + g_body.h + 6.f + 4.f, std::max(16.f, iw - 90.f), 8, false, 255, 90, 90, 255);
        }

        float wb = 120.f, wo = 120.f;
        if (wb + wo + gap > iw) { wb = wo = std::max(30.f, std::floor((iw - gap) * 0.5f)); }
        g_qBack.label = UiText("BACK", "НАЗАД");
        g_qOk.label = UiText("OK", "ОК");
        g_qBack.x = ix; g_qBack.y = rowY; g_qBack.w = wb; g_qBack.h = btnH;
        g_qOk.x = ix + iw - wo; g_qOk.y = rowY; g_qOk.w = wo; g_qOk.h = btnH;
        g_qBack.hovered = Inside(g_qBack, g_mx, g_my);
        g_qOk.hovered = Inside(g_qOk, g_mx, g_my);
        TextButton_Draw(r, g_qBack);
        TextButton_Draw(r, g_qOk);
    }

    TheoryResult HandleQuestionEditor(const SDL_Event& e) {
        if (TextArea_Handle(g_body, e)) Focus(1);
        if (TextField_Handle(g_answers, e)) Focus(2);
        if (TextButton_Handle(g_qBack, e)) { ExitQuestion(); return TheoryResult::None; }
        if (TextButton_Handle(g_qOk, e)) { CommitQuestion(); return TheoryResult::None; }

        const bool backspace = e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_BACKSPACE;
        if (e.type == SDL_EVENT_TEXT_INPUT || backspace || IsEditShortcut(e)) {
            TextArea_Input(g_body, e);
            TextField_Input(g_answers, e);
            g_err.clear();
        }

        if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.key) {
            case SDLK_ESCAPE:
                ExitQuestion();                                // назад к тесту, вопрос не сохраняется
                break;
            case SDLK_TAB:
                Focus(g_body.focused ? 2 : 1);
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (g_body.focused && g_body.text.size() < g_body.maxLen) g_body.text.push_back('\n');
                break;
            }
        }
        return TheoryResult::None;
    }

    TheoryResult HandleTestOverview(const SDL_Event& e) {
        if (TextField_Handle(g_title, e)) Focus(0);
        if (TextField_Handle(g_tags, e)) Focus(3);
        if (TextField_Handle(g_rank, e)) Focus(4);
        if (TextButton_Handle(g_attach, e)) ChooseFiles();
        if (TextButton_Handle(g_ai, e)) g_aiOn = !g_aiOn;

        if (e.type == SDL_EVENT_MOUSE_WHEEL) {                 // колесо листает список вопросов
            if (e.wheel.y > 0) g_qScroll = std::max(0, g_qScroll - 1);
            else if (e.wheel.y < 0) ++g_qScroll;               // верхняя граница ограничивается при отрисовке
        }

        for (int k = 0; k < g_qVis; ++k) {
            const int idx = g_qScroll + k;
            if (idx >= int(g_questions.size())) break;
            if (TextButton_Handle(g_qDel[k], e)) {
                g_questions.erase(g_questions.begin() + idx);
                g_err.clear();
                return TheoryResult::None;
            }
            if (TextButton_Handle(g_qBtn[k], e)) {
                OpenQuestion(idx);
                return TheoryResult::None;
            }
        }
        if (g_qPlusShown && TextButton_Handle(g_qPlus, e)) {
            if (g_questions.size() < kMaxQuestions) OpenQuestion(-1);
            return TheoryResult::None;
        }

        if (TextButton_Handle(g_add, e)) {
            const bool blank = g_title.text.find_first_not_of(' ') == std::string::npos;
            if (blank) {                                       // заголовок обязателен
                g_err = "ENTER TITLE";
                Focus(0);
            }
            else if (g_questions.empty()) {                    // и хотя бы один вопрос
                g_err = "ADD A QUESTION";
            }
            else {
                g_err.clear();
                g_open = false;                                // данные остаются до следующего Theory_Open
                return TheoryResult::Added;
            }
        }

        const bool backspace = e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_BACKSPACE;
        if (e.type == SDL_EVENT_TEXT_INPUT || backspace || IsEditShortcut(e)) {
            TextField_Input(g_title, e);
            TextField_Input(g_tags, e);
            TextField_Input(g_rank, e);
            g_err.clear();
        }

        if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.key) {
            case SDLK_ESCAPE:
                g_open = false;
                return TheoryResult::Cancel;
            case SDLK_TAB:                                     // заголовок -> теги -> звание -> заголовок
                Focus(g_title.focused ? 3 : (g_tags.focused ? 4 : 0));
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (g_title.focused) Focus(3);
                else if (g_tags.focused) Focus(4);
                break;
            }
        }
        return TheoryResult::None;
    }
}

void Theory_Open(SDL_Window* window, bool withAnswers) {
    EnsureInit();
    g_window = window;
    g_withAnswers = withAnswers;
    g_title.text.clear();
    g_body.text.clear();
    g_answers.text.clear();
    g_tags.text.clear();
    g_rank.text.clear();
    g_body.scroll = 0;
    g_files.clear();
    g_aiOn = false;
    g_err.clear();
    g_questions.clear();
    g_qMode = false;
    g_qIdx = -1;
    g_qScroll = 0;
    g_qVis = 0;
    g_qPlusShown = false;
    g_body.maxLen = withAnswers ? kQuestionMax : kBodyMax;
    g_hasPending = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_pending.clear();
    }
    g_title.hovered = g_body.hovered = g_answers.hovered = g_tags.hovered = g_rank.hovered = false;
    g_attach.hovered = g_ai.hovered = g_add.hovered = false;
    g_mx = g_my = -1.f;
    g_laid = false;
    g_editing = false;
    g_open = true;
    Focus(0);
    SDL_StartTextInput(window);
}

void Theory_Edit(SDL_Window* window, const TheoryItem& item, bool withAnswers) {
    Theory_Open(window, withAnswers);             // сброс и открытие, затем подставляем данные
    g_editing = true;
    g_title.text = item.title;
    g_body.text = item.content;
    g_answers.text = item.answers;
    g_files = item.files;
    g_aiOn = item.madeWithAI;
    if (withAnswers) {
        g_questions = item.questions;
        // тест из старой версии (один текст + одна строка ответов) превращаем в один вопрос
        if (g_questions.empty() && (!item.content.empty() || !item.answers.empty()))
            g_questions.push_back({ item.content, item.answers });
        g_body.text.clear();                      // в окне теста поле содержания не используется
        g_answers.text.clear();
        g_tags.text = item.subjectTags;
        g_rank.text = item.rank;
    }
}

void Theory_Close() {
    g_open = false;
    g_editing = false;
    g_title.text.clear();
    g_body.text.clear();
    g_answers.text.clear();
    g_tags.text.clear();
    g_rank.text.clear();
    g_files.clear();
    g_err.clear();
    g_questions.clear();
    g_qMode = false;
    g_qIdx = -1;
    Focus(-1);
}

bool Theory_IsOpen() { return g_open; }

void Theory_Poll() {
    if (!g_hasPending.exchange(false)) return;

    std::vector<std::string> list;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        list.swap(g_pending);
    }
    if (!g_open) return;                         // окно уже закрыли: выбранное не нужно

    for (const std::string& p : list) {
        if (std::find(g_files.begin(), g_files.end(), p) != g_files.end()) continue;   // уже есть
        if (g_files.size() >= kMaxFiles) { g_err = "MAX 10 FILES"; break; }
        g_files.push_back(p);
    }
}

void Theory_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount) {
    if (!g_open) return;
    EnsureInit();

    // затемнение всего окна
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 175);
    SDL_RenderFillRect(r, nullptr);

    // (2) окно: почти на всё окно приложения; сверху остаётся полоса с заголовком экрана и кнопкой меню,
    //     снизу строка "by Aquarius / версия"
    const float side = (w >= 420) ? 18.f : 10.f;
    const float px = side, py = 44.f;
    const float pw = std::floor(std::max(120.f, float(w) - 2.f * side));
    const float ph = std::floor(std::max(160.f, float(h) - py - 30.f));
    const float pad = (w >= 420) ? 20.f : 12.f;
    const float ix = px + pad, iw = pw - 2.f * pad;
    const float btnH = 40.f;

    // фон окна (с отступом, чтобы не торчал за скруглёнными углами рамки)
    SDL_SetRenderDrawColor(r, 26, 12, 18, 255);
    const SDL_FRect bg = { px + 4, py + 4, pw - 8, ph - 8 };
    SDL_RenderFillRect(r, &bg);

    g_panel.x = px; g_panel.y = py; g_panel.w = pw; g_panel.h = ph;
    g_panel.hovered = false;
    TextButton_Draw(r, g_panel);

    // тест: редактор вопроса занимает всё окно
    if (g_withAnswers && g_qMode) {
        DrawQuestionEditor(r, py, ph, pad, ix, iw);
        nd[ndCount++] = { int(px), int(py), int(pw), int(ph) };
        g_laid = true;
        return;
    }

    // (3) заголовок
    g_title.placeholder = UiText("TITLE", "ЗАГОЛОВОК");
    g_body.placeholder = UiText("CONTENT", "СОДЕРЖАНИЕ");
    g_answers.placeholder = UiText("CORRECT ANSWERS", "ПРАВИЛЬНЫЕ ОТВЕТЫ");
    g_title.x = ix; g_title.y = py + pad; g_title.w = iw; g_title.h = 40.f;
    TextField_Draw(r, g_title);

    // нижний ряд кнопок: (5) слева, (6) по центру, (7) справа
    const float rowY = py + ph - pad - btnH;
    const float gap = 12.f;
    float wa = 140.f, wi = 230.f, wd = 120.f;
    g_attach.label = UiText("ATTACH", "ФАЙЛЫ");
    g_ai.label = UiText("MADE WITH AI", "СОЗДАНО ИИ");
    if (wa + wi + wd + 2.f * gap > iw) {              // узко: короткие подписи
        wa = 90.f; wi = 70.f; wd = 90.f;
        g_attach.label = UiText("FILE", "ФАЙЛ");
        g_ai.label = UiText("AI", "ИИ");
        if (wa + wi + wd + 2.f * gap > iw) {          // совсем узко: сжимаем все три
            const float k = std::max(30.f, iw - 2.f * gap) / (wa + wi + wd);
            wa = std::max(10.f, std::floor(wa * k));
            wi = std::max(10.f, std::floor(wi * k));
            wd = std::max(10.f, std::floor(wd * k));
        }
    }
    g_add.label = g_editing ? UiText("SAVE", "СОХР.") : UiText("ADD", "ДОБАВ.");
    g_attach.w = wa; g_ai.w = wi; g_add.w = wd;
    g_attach.h = g_ai.h = g_add.h = btnH;
    g_attach.y = g_ai.y = g_add.y = rowY;
    g_attach.x = ix;
    g_add.x = ix + iw - wd;
    {
        const float lo = g_attach.x + wa + gap;
        const float hi = std::max(lo, g_add.x - gap - wi);
        g_ai.x = std::floor(std::clamp(ix + (iw - wi) * 0.5f, lo, hi));
    }

    // (4) содержание: от заголовка до ряда кнопок (с запасом под строку со счётчиком)
    const float counterH = 22.f;
    const float by = g_title.y + g_title.h + 14.f;
    float testInfoY = 0.f;
    if (g_withAnswers) {                              // тест: вместо содержания список вопросов с "+"
        // под заголовком две строки: теги предмета и звание
        g_tags.placeholder = UiText("SUBJECT TAGS", "ТЕГИ ПРЕДМЕТА");
        g_rank.placeholder = UiText("RANK", "ЗВАНИЕ");
        g_tags.x = ix; g_tags.y = by; g_tags.w = iw; g_tags.h = 40.f;
        TextField_Draw(r, g_tags);
        g_rank.x = ix; g_rank.y = g_tags.y + g_tags.h + 10.f; g_rank.w = iw; g_rank.h = 40.f;
        TextField_Draw(r, g_rank);
        const float listTop = g_rank.y + g_rank.h + 14.f;

        testInfoY = rowY - 14.f - 18.f;               // строка для ошибки / списка вложений
        DrawQuestionList(r, ix, iw, listTop, std::max(listTop + 40.f, testInfoY - 8.f));
    }
    else {
        g_body.x = ix; g_body.y = by; g_body.w = iw;
        g_body.h = std::max(60.f, rowY - 14.f - counterH - by);
        TextArea_Draw(r, g_body);
    }

    // строка под полем слева: ошибка или список вложений (справа поле рисует счётчик символов)
    {
        const float infoY = g_withAnswers ? testInfoY : g_body.y + g_body.h + 6.f;
        const size_t maxChars = size_t(std::max(0.f, (iw - 90.f) / 8.f));
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        if (!g_err.empty()) {
            const std::string shownError = UiMessage(g_err);
            DrawUtf8TextFit(r, shownError, ix, infoY + 4.f, std::max(16.f, iw - 90.f), 8, false, 255, 90, 90, 255);
        }
        else {
            const std::string s = AttachSummary(maxChars);
            if (!s.empty()) {
                DrawUtf8Text(r, s, ix, infoY, 12, kR, kG, kB, 170);
            }
        }
    }

    // кнопки; переключатель в положении "вкл" залит и подсвечен, как выбранная вкладка
    g_attach.hovered = Inside(g_attach, g_mx, g_my);
    g_add.hovered = Inside(g_add, g_mx, g_my);
    TextButton_Draw(r, g_attach);
    TextButton_Draw(r, g_add);

    if (g_aiOn) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, kR, kG, kB, 40);
        const SDL_FRect fill = { g_ai.x + 4.f, g_ai.y + 4.f, g_ai.w - 8.f, g_ai.h - 8.f };
        SDL_RenderFillRect(r, &fill);
    }
    g_ai.hovered = g_aiOn || Inside(g_ai, g_mx, g_my);
    TextButton_Draw(r, g_ai);

    // всё окно без перетаскивания
    nd[ndCount++] = { int(px), int(py), int(pw), int(ph) };
    g_laid = true;
}

TheoryResult Theory_Handle(const SDL_Event& e) {
    if (!g_open) return TheoryResult::None;
    EnsureInit();

    // пока окно ни разу не нарисовано, позиции элементов не заданы: мышь не смотрим
    const bool mouse = e.type == SDL_EVENT_MOUSE_MOTION || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                       e.type == SDL_EVENT_MOUSE_WHEEL;
    if (!g_laid && mouse) return TheoryResult::None;

    if (e.type == SDL_EVENT_MOUSE_MOTION) { g_mx = e.motion.x; g_my = e.motion.y; }
    if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) { g_mx = g_my = -1.f; }

    if (g_withAnswers) return g_qMode ? HandleQuestionEditor(e) : HandleTestOverview(e);

    if (TextField_Handle(g_title, e)) Focus(0);
    if (TextArea_Handle(g_body, e)) Focus(1);
    if (g_withAnswers && TextField_Handle(g_answers, e)) Focus(2);
    if (TextButton_Handle(g_attach, e)) ChooseFiles();
    if (TextButton_Handle(g_ai, e)) g_aiOn = !g_aiOn;

    if (TextButton_Handle(g_add, e)) {
        const bool blank = g_title.text.find_first_not_of(' ') == std::string::npos;
        if (blank) {                                   // заголовок обязателен
            g_err = "ENTER TITLE";
            Focus(0);
        }
        else {
            g_err.clear();
            g_open = false;                            // данные остаются до следующего Theory_Open
            return TheoryResult::Added;
        }
    }

    // ввод символов; при правке убираем сообщение об ошибке
    const bool backspace = e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_BACKSPACE;
    if (e.type == SDL_EVENT_TEXT_INPUT || backspace || IsEditShortcut(e)) {
        TextField_Input(g_title, e);
        TextArea_Input(g_body, e);
        if (g_withAnswers) TextField_Input(g_answers, e);
        g_err.clear();
    }

    if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.key) {
        case SDLK_ESCAPE:
            g_open = false;
            return TheoryResult::Cancel;
        case SDLK_TAB:
            if (g_withAnswers) Focus(g_title.focused ? 1 : (g_body.focused ? 2 : 0));
            else               Focus(g_title.focused ? 1 : 0);
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (g_body.focused) {
                if (g_body.text.size() < g_body.maxLen) g_body.text.push_back('\n');   // новая строка
            }
            else if (g_title.focused) {
                Focus(1);                                                              // из заголовка в содержание
            }
            // в строке верных ответов Enter ничего не делает
            break;
        }
    }
    return TheoryResult::None;
}

TheoryItem Theory_Data() {
    TheoryItem it;
    it.title = g_title.text;
    it.files = g_files;
    it.madeWithAI = g_aiOn;
    if (g_withAnswers) {
        it.subjectTags = g_tags.text;
        it.rank = g_rank.text;
        // вопросы теста; content/answers собираем из них, чтобы экран просмотра репозитория показал весь тест
        it.questions = g_questions;
        for (size_t i = 0; i < g_questions.size(); ++i) {
            it.content += (i ? "\n" : "") + std::to_string(i + 1) + ". " + g_questions[i].text;
            if (!g_questions[i].answers.empty())
                it.answers += (it.answers.empty() ? "" : "; ") + std::to_string(i + 1) + ") " + g_questions[i].answers;
        }
    }
    else {
        it.content = g_body.text;
    }
    return it;
}

void Theory_Destroy() {
    TextButton_Destroy(g_panel);
    TextField_Destroy(g_title);
    TextArea_Destroy(g_body);
    TextField_Destroy(g_answers);
    TextField_Destroy(g_tags);
    TextField_Destroy(g_rank);
    TextButton_Destroy(g_attach);
    TextButton_Destroy(g_ai);
    TextButton_Destroy(g_add);
    for (int i = 0; i < kPool; ++i) { TextButton_Destroy(g_qBtn[i]); TextButton_Destroy(g_qDel[i]); }
    TextButton_Destroy(g_qPlus);
    TextButton_Destroy(g_qBack);
    TextButton_Destroy(g_qOk);
}

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

namespace {
    const Uint8  kR = 90, kG = 200, kB = 255;     // цвет неона, как у кнопок
    const size_t kTitleMax = 60;
    const size_t kBodyMax = 3000;
    const size_t kMaxFiles = 10;
    const size_t kAnswersMax = 200;

    bool        g_open = false;
    bool        g_laid = false;                   // true после первой отрисовки (до неё позиции элементов неверны)
    bool        g_init = false;
    SDL_Window* g_window = nullptr;

    TextButton  g_panel{ 0, 0, 100, 100, "" };    // неоновая рамка окна
    TextField   g_title;                          // (3) заголовок
    TextArea    g_body;                           // (4) содержание
    TextField   g_answers;                        // верные ответы (только в окне теста)
    bool        g_withAnswers = false;            // true = окно теста
    TextButton  g_attach{ 0, 0, 140, 40, "ATTACH" };         // (5) добавить вложение
    TextButton  g_ai{ 0, 0, 230, 40, "MADE WITH AI" };       // (6) переключатель
    TextButton  g_add{ 0, 0, 120, 40, "ADD" };               // (7) добавить
    bool        g_aiOn = false;
    bool        g_editing = false;                // true = правим готовую теорию (кнопка SAVE)

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
    }

    void Focus(int i) {
        g_title.focused = (i == 0);
        g_body.focused = (i == 1);
        g_answers.focused = (i == 2);
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
}

void Theory_Open(SDL_Window* window, bool withAnswers) {
    EnsureInit();
    g_window = window;
    g_withAnswers = withAnswers;
    g_title.text.clear();
    g_body.text.clear();
    g_answers.text.clear();
    g_body.scroll = 0;
    g_files.clear();
    g_aiOn = false;
    g_err.clear();
    g_hasPending = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_pending.clear();
    }
    g_title.hovered = g_body.hovered = g_answers.hovered = false;
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
}

void Theory_Close() {
    g_open = false;
    g_editing = false;
    g_title.text.clear();
    g_body.text.clear();
    g_answers.text.clear();
    g_files.clear();
    g_err.clear();
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
    SDL_SetRenderDrawColor(r, 22, 24, 30, 255);
    const SDL_FRect bg = { px + 4, py + 4, pw - 8, ph - 8 };
    SDL_RenderFillRect(r, &bg);

    g_panel.x = px; g_panel.y = py; g_panel.w = pw; g_panel.h = ph;
    g_panel.hovered = false;
    TextButton_Draw(r, g_panel);

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
    g_body.x = ix; g_body.y = by; g_body.w = iw;
    float bodyBottom = rowY - 14.f;                   // нижняя граница области содержания (вместе со счётчиком)
    if (g_withAnswers) {                              // тест: строка верных ответов над рядом кнопок
        g_answers.x = ix; g_answers.w = iw; g_answers.h = 40.f;
        g_answers.y = rowY - 14.f - g_answers.h;
        bodyBottom = g_answers.y - 8.f;
    }
    g_body.h = std::max(60.f, bodyBottom - counterH - by);
    TextArea_Draw(r, g_body);
    if (g_withAnswers) TextField_Draw(r, g_answers);

    // строка под полем слева: ошибка или список вложений (справа поле рисует счётчик символов)
    {
        const float infoY = g_body.y + g_body.h + 6.f;
        const size_t maxChars = size_t(std::max(0.f, (iw - 90.f) / 8.f));
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        if (!g_err.empty()) {
            const std::string shownError = UiMessage(g_err);
            DrawUtf8Text(r, shownError, ix, infoY, 12, 255, 90, 90, 255);
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
    if (e.type == SDL_EVENT_TEXT_INPUT || backspace) {
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
    return { g_title.text, g_body.text, g_withAnswers ? g_answers.text : std::string(), g_files, g_aiOn };
}

void Theory_Destroy() {
    TextButton_Destroy(g_panel);
    TextField_Destroy(g_title);
    TextArea_Destroy(g_body);
    TextField_Destroy(g_answers);
    TextButton_Destroy(g_attach);
    TextButton_Destroy(g_ai);
    TextButton_Destroy(g_add);
}

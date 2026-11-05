#include "CreateScreen.h"
#include "TextField.h"
#include "TextButton.h"
#include "Text.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

namespace {
    const Uint8 kR = 90, kG = 200, kB = 255;      // цвет неона, как у кнопок
    const size_t kNameMax = 40;
    const size_t kDescMax = 500;
    const float  kLineH = 20.f;                   // шаг строк описания (буквы 16 px при масштабе 2)

    // многострочное поле: рамку рисует TextButton
    struct TextArea {
        float x = 0, y = 0, w = 320, h = 120;
        bool focused = false, hovered = false;
        std::string text;
        TextButton box;
    };

    SDL_Window* g_window = nullptr;
    TextField   g_name;
    TextArea    g_desc;
    TextButton  g_pick{ 0, 0, 120, 120, "" };     // квадрат (1): картинка
    TextButton  g_back{ 0, 0, 110, 40, "BACK" };
    TextButton  g_ok{ 0, 0, 150, 40, "CREATE" };  // кнопка CREATE внизу справа

    // вкладки под аватаром: 0 = имя и описание, 1 = теория, 2 = тесты, 3 = игры
    const int   kTabs = 4;
    const char* kTabFull[4]  = { "INFO", "THEORY", "TESTS", "GAMES" };
    const char* kTabShort[4] = { "1", "2", "3", "4" };    // когда столбец слишком узкий для слов
    int         g_tab = 0;
    TextButton  g_tabBtn[4];
    TextButton  g_plusBtn{ 0, 0, 80, 80, "" };            // "+" на вкладках 1..3 (пока без действия)
    float       g_mx = -1.f, g_my = -1.f;                 // мышь: чтобы отличать наведение от выбора
    std::string g_err;
    std::string g_imgPath;
    bool        g_init = false;

    // выбранная картинка: квадрат 256x256 + текстура
    SDL_Surface*  g_img = nullptr;
    SDL_Texture*  g_imgTex = nullptr;
    SDL_Renderer* g_imgOwner = nullptr;

    // файл выбирается в системном диалоге; колбэк может прийти из другого потока,
    // поэтому путь передаём через мьютекс и разбираем в Create_Poll() в основном потоке
    std::mutex        g_mutex;
    std::string       g_pending;
    std::atomic<bool> g_hasPending{ false };
    std::atomic<bool> g_dialogOpen{ false };

    void SDLCALL OnFileChosen(void*, const char* const* list, int) {
        if (list && list[0]) {                  // NULL = ошибка, пустой список = отмена
            std::lock_guard<std::mutex> lock(g_mutex);
            g_pending = list[0];
            g_hasPending = true;
        }
        g_dialogOpen = false;
    }

    void ChooseFile() {
        if (g_dialogOpen.exchange(true)) return;   // диалог уже открыт
        static const SDL_DialogFileFilter filters[] = { { "Images (PNG, BMP)", "png;bmp" } };
        SDL_ShowOpenFileDialog(OnFileChosen, nullptr, g_window, filters, 1, nullptr, false);
    }

    void EnsureInit() {
        if (g_init) return;
        g_init = true;
        g_name.placeholder = "NAME";
        g_name.maxLen = kNameMax;
    }

    void Focus(int i) {
        g_name.focused = (i == 0);
        g_desc.focused = (i == 1);
    }

    // ---------- картинка ----------

    // любая картинка -> квадрат 256x256 RGBA32 (центральная часть). Исходную поверхность не трогает.
    SDL_Surface* MakeSquare256(SDL_Surface* src) {
        const int kSide = 256;
        SDL_Surface* cur = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGBA32);
        if (!cur) return nullptr;

        const int side = std::min(cur->w, cur->h);
        if (side < 1) { SDL_DestroySurface(cur); return nullptr; }

        if (cur->w != cur->h) {                  // обрезка по центру до квадрата
            SDL_Surface* sq = SDL_CreateSurface(side, side, SDL_PIXELFORMAT_RGBA32);
            if (!sq) { SDL_DestroySurface(cur); return nullptr; }
            const SDL_Rect rc = { (cur->w - side) / 2, (cur->h - side) / 2, side, side };
            SDL_SetSurfaceBlendMode(cur, SDL_BLENDMODE_NONE);
            SDL_BlitSurface(cur, &rc, sq, nullptr);
            SDL_DestroySurface(cur);
            cur = sq;
        }
        while (cur->w > kSide) {                 // сжатие по шагам вдвое для приличного качества
            const int next = std::max(kSide, cur->w / 2);
            SDL_Surface* sc = SDL_ScaleSurface(cur, next, next, SDL_SCALEMODE_LINEAR);
            SDL_DestroySurface(cur);
            if (!sc) return nullptr;
            cur = sc;
        }
        if (cur->w < kSide) {
            SDL_Surface* sc = SDL_ScaleSurface(cur, kSide, kSide, SDL_SCALEMODE_LINEAR);
            SDL_DestroySurface(cur);
            if (!sc) return nullptr;
            cur = sc;
        }
        return cur;
    }

    void FreeImage() {
        if (g_imgTex) SDL_DestroyTexture(g_imgTex);
        if (g_img) SDL_DestroySurface(g_img);
        g_imgTex = nullptr;
        g_img = nullptr;
        g_imgPath.clear();
    }

    bool SetImage(const std::string& path) {
        SDL_Surface* src = SDL_LoadSurface(path.c_str());   // PNG или BMP
        if (!src) return false;
        SDL_Surface* sq = MakeSquare256(src);
        SDL_DestroySurface(src);
        if (!sq) return false;
        FreeImage();
        g_img = sq;
        g_imgPath = path;
        return true;
    }

    // ---------- многострочное поле ----------

    // разбивает текст на строки шириной не больше cols символов (по возможности по словам)
    std::vector<std::string> Wrap(const std::string& s, size_t cols) {
        std::vector<std::string> lines(1);
        for (char c : s) {
            if (c == '\n') { lines.emplace_back(); continue; }
            if (lines.back().size() >= cols) {
                std::string carry;
                if (c != ' ') {
                    std::string& cur = lines.back();
                    const size_t sp = cur.find_last_of(' ');
                    if (sp != std::string::npos) { carry = cur.substr(sp + 1); cur.erase(sp + 1); }
                }
                lines.push_back(carry);
                if (c == ' ') continue;          // пробел на границе строки не переносим
            }
            lines.back().push_back(c);
        }
        if (lines.back().size() >= cols) lines.emplace_back();   // курсор уходит на новую строку
        return lines;
    }

    void Area_Draw(SDL_Renderer* r, TextArea& a) {
        a.box.x = a.x; a.box.y = a.y; a.box.w = a.w; a.box.h = a.h;
        a.box.label = "";
        a.box.hovered = a.focused || a.hovered;
        TextButton_Draw(r, a.box);

        const float scale = 2.0f;
        const float cs = float(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) * scale;   // 16 px
        const float padX = 12.f, padY = 10.f;
        const size_t cols = size_t(std::max(1, int((a.w - padX * 2) / cs)));
        const size_t rows = size_t(std::max(1, int((a.h - padY * 2 + (kLineH - cs)) / kLineH)));

        const std::vector<std::string> lines = Wrap(a.text, cols);
        const size_t first = lines.size() > rows ? lines.size() - rows : 0;   // всегда видим конец текста

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderScale(r, scale, scale);

        const float tx = (a.x + padX) / scale;
        if (a.text.empty()) {
            SDL_SetRenderDrawColor(r, kR, kG, kB, 90);                // подсказка бледная
            SDL_RenderDebugText(r, tx, (a.y + padY) / scale, "DESCRIPTION");
        }
        else {
            SDL_SetRenderDrawColor(r, 200, 240, 255, 255);
            for (size_t i = first; i < lines.size(); ++i)
                SDL_RenderDebugText(r, tx, (a.y + padY + float(i - first) * kLineH) / scale, lines[i].c_str());
        }

        if (a.focused && (SDL_GetTicks() / 500) % 2 == 0) {            // мигающий курсор
            const float cy = a.y + padY + float(lines.size() - 1 - first) * kLineH;
            SDL_SetRenderDrawColor(r, kR, kG, kB, 255);
            SDL_RenderDebugText(r, tx + float(lines.back().size()) * (cs / scale), cy / scale, "_");
        }
        SDL_SetRenderScale(r, 1.0f, 1.0f);

        // счётчик символов под полем, справа
        const std::string cnt = std::to_string(a.text.size()) + "/" + std::to_string(kDescMax);
        SDL_SetRenderDrawColor(r, kR, kG, kB, 90);
        SDL_RenderDebugText(r, a.x + a.w - float(cnt.size()) * 8.f, a.y + a.h + 6.f, cnt.c_str());
    }

    bool Area_Inside(const TextArea& a, float mx, float my) {
        return mx >= a.x && mx <= a.x + a.w && my >= a.y && my <= a.y + a.h;
    }

    bool Area_Handle(TextArea& a, const SDL_Event& e) {   // true = клик по полю
        if (e.type == SDL_EVENT_MOUSE_MOTION)
            a.hovered = Area_Inside(a, e.motion.x, e.motion.y);
        if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE)
            a.hovered = false;
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
            return Area_Inside(a, e.button.x, e.button.y);
        return false;
    }

    void Area_Input(TextArea& a, const SDL_Event& e) {    // символы и Backspace
        if (!a.focused) return;
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            for (const char* p = e.text.text; *p; ++p) {
                const unsigned char c = static_cast<unsigned char>(*p);
                // встроенный шрифт SDL умеет только ASCII
                if (c >= 32 && c < 127 && a.text.size() < kDescMax) a.text.push_back(char(c));
            }
        }
        else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_BACKSPACE) {
            if (!a.text.empty()) a.text.pop_back();
        }
    }

    // ---------- рисование мелочей ----------

    bool Inside(const TextButton& b, float mx, float my) {
        return mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h;
    }

    // плюс из двух полос с мягким свечением, как на главном экране; при наведении светлее
    void DrawPlusIcon(SDL_Renderer* r, const TextButton& b) {
        const float cx = std::floor(b.x + b.w * 0.5f), cy = std::floor(b.y + b.h * 0.5f);
        const float len = 34.f, th = 4.f;
        const Uint8 R = b.hovered ? 200 : kR, G = b.hovered ? 240 : kG, B = b.hovered ? 255 : kB;

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (int i = 5; i >= 1; --i) {
            SDL_SetRenderDrawColor(r, R, G, B, Uint8(34.f * (1.f - i / 6.f)));
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

    // заглушка в квадрате картинки: плюс и подпись AVATAR
    void DrawPlaceholder(SDL_Renderer* r, const TextButton& b) {
        const float S = b.w, cx = b.x + S * 0.5f, cy = b.y + S * 0.46f;
        const float len = S * 0.30f, th = std::max(3.f, S * 0.035f);
        const Uint8 R = b.hovered ? 200 : kR, G = b.hovered ? 240 : kG, B = b.hovered ? 255 : kB;

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, R, G, B, 255);
        const SDL_FRect hor = { std::floor(cx - len * 0.5f), std::floor(cy - th * 0.5f), std::ceil(len), std::ceil(th) };
        const SDL_FRect ver = { std::floor(cx - th * 0.5f), std::floor(cy - len * 0.5f), std::ceil(th), std::ceil(len) };
        SDL_RenderFillRect(r, &hor);
        SDL_RenderFillRect(r, &ver);

        const float scale = (S >= 110.f) ? 2.0f : 1.0f;
        const float cs = float(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) * scale;
        const char* t = "AVATAR";
        const float textW = float(std::strlen(t)) * cs;
        SDL_SetRenderScale(r, scale, scale);
        SDL_SetRenderDrawColor(r, R, G, B, 150);
        SDL_RenderDebugText(r, (cx - textW * 0.5f) / scale, (b.y + S * 0.74f) / scale, t);
        SDL_SetRenderScale(r, 1.0f, 1.0f);
    }

    void EnsureImageTexture(SDL_Renderer* r) {
        if (g_imgOwner != r) {                    // новый рендерер: текстуру пересоздать
            if (g_imgTex) SDL_DestroyTexture(g_imgTex);
            g_imgTex = nullptr;
            g_imgOwner = r;
        }
        if (g_img && !g_imgTex) g_imgTex = SDL_CreateTextureFromSurface(r, g_img);
    }
}

void Create_Open(SDL_Window* window) {
    EnsureInit();
    g_window = window;
    g_name.text.clear();
    g_desc.text.clear();
    g_err.clear();
    FreeImage();
    g_hasPending = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_pending.clear();
    }
    g_pick.hovered = g_back.hovered = g_ok.hovered = false;
    g_name.hovered = g_desc.hovered = false;
    g_tab = 0;
    g_plusBtn.hovered = false;
    for (TextButton& t : g_tabBtn) t.hovered = false;
    g_mx = g_my = -1.f;
    Focus(0);
    SDL_StartTextInput(window);
}

void Create_Close(SDL_Window* window) {
    SDL_StopTextInput(window);
    g_name.text.clear();
    g_desc.text.clear();
    g_err.clear();
    FreeImage();
    Focus(-1);
}

void Create_Poll() {
    if (!g_hasPending.exchange(false)) return;

    std::string path;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        path = std::move(g_pending);
        g_pending.clear();
    }
    g_err = SetImage(path) ? "" : "CANNOT LOAD IMAGE";
}

void Create_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount) {
    EnsureInit();

    // заголовок у самой рамки, как на остальных экранах
    const float titleScale = (w >= 520) ? 3.0f : 2.0f;
    const float titleCy = 5.f + float(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) * titleScale * 0.5f;
    DrawCenteredText(r, "CREATE", w, h, titleScale, titleCy - h * 0.5f);

    // раскладка: слева квадрат, справа название и под ним описание, внизу BACK и CREATE
    const float m = (w >= 420) ? 30.f : 16.f;
    const float top = 60.f;
    const float fieldH = 40.f;
    const float bottomY = float(h) - 24.f - g_ok.h;
    const float S = std::floor(std::min(120.f, std::max(48.f, w * 0.22f)));
    const float x0 = m + S + 20.f;
    const float rw = std::max(60.f, float(w) - x0 - m);

    // (1) квадрат с картинкой
    g_pick.x = m; g_pick.y = top; g_pick.w = g_pick.h = S;
    TextButton_Draw(r, g_pick);
    if (g_img) {
        EnsureImageTexture(r);
        if (g_imgTex) {
            const SDL_FRect dst = { m + 5.f, top + 5.f, S - 10.f, S - 10.f };
            SDL_RenderTexture(r, g_imgTex, nullptr, &dst);
        }
    }
    else {
        DrawPlaceholder(r, g_pick);
    }
    nd[ndCount++] = TextButton_Rect(g_pick);

    // вкладки под аватаром (на узком окне вместо слов цифры)
    const float tabsTop = top + S + 16.f;
    const float tabGap = 8.f;
    const float tabH = std::clamp((bottomY - 16.f - tabsTop - 3.f * tabGap) / 4.f, 24.f, 36.f);
    for (int i = 0; i < kTabs; ++i) {
        TextButton& t = g_tabBtn[i];
        t.x = m; t.y = tabsTop + float(i) * (tabH + tabGap); t.w = S; t.h = tabH;
        t.label = (S >= 100.f) ? kTabFull[i] : kTabShort[i];
        const bool selected = (i == g_tab);
        if (selected) {                                    // выбранная вкладка: мягкая заливка + яркая рамка
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, kR, kG, kB, 40);
            const SDL_FRect fill = { t.x + 4.f, t.y + 4.f, t.w - 8.f, t.h - 8.f };
            SDL_RenderFillRect(r, &fill);
        }
        t.hovered = selected || Inside(t, g_mx, g_my);
        TextButton_Draw(r, t);
        nd[ndCount++] = TextButton_Rect(t);
    }

    if (g_tab == 0) {
        // (1) название
        g_name.x = x0; g_name.y = top; g_name.w = rw; g_name.h = fieldH;
        TextField_Draw(r, g_name);
        nd[ndCount++] = TextField_Rect(g_name);

        // (1) описание: до нижнего ряда кнопок (с запасом под счётчик символов)
        const float dy = top + fieldH + 14.f;
        g_desc.x = x0; g_desc.y = dy; g_desc.w = rw;
        g_desc.h = std::max(60.f, bottomY - 22.f - dy);
        Area_Draw(r, g_desc);
        nd[ndCount++] = { int(g_desc.x), int(g_desc.y), int(g_desc.w), int(g_desc.h) };
    }
    else {
        // (2) теория, (3) тесты, (4) игры: пока только кнопка "+" в левом верхнем углу области
        g_plusBtn.x = x0; g_plusBtn.y = top;
        g_plusBtn.hovered = Inside(g_plusBtn, g_mx, g_my);
        TextButton_Draw(r, g_plusBtn);
        DrawPlusIcon(r, g_plusBtn);
        nd[ndCount++] = TextButton_Rect(g_plusBtn);
    }

    // кнопка CREATE справа, BACK слева
    g_back.x = m;                    g_back.y = bottomY;
    g_ok.x = float(w) - m - g_ok.w;  g_ok.y = bottomY;
    TextButton_Draw(r, g_back);
    TextButton_Draw(r, g_ok);
    nd[ndCount++] = TextButton_Rect(g_back);
    nd[ndCount++] = TextButton_Rect(g_ok);

    // ошибка между кнопками внизу
    if (!g_err.empty()) {
        const float scale = (w >= 560) ? 2.0f : 1.0f;
        DrawCenteredText(r, g_err.c_str(), w, h, scale, bottomY + g_ok.h * 0.5f - h * 0.5f, 255, 90, 90);
    }
}

CreateResult Create_Handle(const SDL_Event& e) {
    EnsureInit();

    if (e.type == SDL_EVENT_MOUSE_MOTION) { g_mx = e.motion.x; g_my = e.motion.y; }
    if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) { g_mx = g_my = -1.f; }

    // вкладки
    for (int i = 0; i < kTabs; ++i) {
        if (TextButton_Handle(g_tabBtn[i], e) && g_tab != i) {
            g_tab = i;
            g_name.hovered = g_desc.hovered = g_plusBtn.hovered = false;
        }
    }

    // содержимое вкладки (положение чужих элементов устарело, поэтому их события не смотрим)
    if (g_tab == 0) {
        if (TextField_Handle(g_name, e)) Focus(0);
        if (Area_Handle(g_desc, e)) Focus(1);
    }
    else {
        if (TextButton_Handle(g_plusBtn, e)) {
            // сюда: добавить теорию / тест / игру (g_tab = 1 / 2 / 3)
        }
    }

    if (TextButton_Handle(g_pick, e)) ChooseFile();
    if (TextButton_Handle(g_back, e)) return CreateResult::Back;
    if (TextButton_Handle(g_ok, e)) {
        const bool blank = g_name.text.find_first_not_of(' ') == std::string::npos;
        if (blank) {                                  // название живёт на вкладке INFO: показать её
            g_tab = 0;
            g_err = "ENTER NAME";
            Focus(0);
            return CreateResult::None;
        }
        g_err.clear();
        return CreateResult::Done;
    }

    // ввод символов (только на вкладке INFO); при правке убираем сообщение об ошибке
    const bool backspace = e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_BACKSPACE;
    if (g_tab == 0 && (e.type == SDL_EVENT_TEXT_INPUT || backspace)) {
        TextField_Input(g_name, e);
        Area_Input(g_desc, e);
        g_err.clear();
    }

    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_ESCAPE) return CreateResult::Back;

        if (g_tab == 0) {
            switch (e.key.key) {
            case SDLK_TAB:
                Focus(g_name.focused ? 1 : 0);
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (g_desc.focused) {
                    if (g_desc.text.size() < kDescMax) g_desc.text.push_back('\n');   // новая строка
                }
                else {
                    Focus(1);                                                         // из названия в описание
                }
                break;
            }
        }
    }
    return CreateResult::None;
}

CreateData Create_Data() {
    return { g_name.text, g_desc.text, g_imgPath };
}

void Create_Destroy() {
    FreeImage();
    TextField_Destroy(g_name);
    TextButton_Destroy(g_desc.box);
    TextButton_Destroy(g_pick);
    for (TextButton& t : g_tabBtn) TextButton_Destroy(t);
    TextButton_Destroy(g_plusBtn);
    TextButton_Destroy(g_back);
    TextButton_Destroy(g_ok);
}

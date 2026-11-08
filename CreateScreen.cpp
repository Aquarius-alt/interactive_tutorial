#include "CreateScreen.h"
#include "TextField.h"
#include "TextButton.h"
#include "TextArea.h"
#include "TheoryDialog.h"
#include "RepoStore.h"
#include "Text.h"
#include "Localization.h"
#include "Theme.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

namespace {
    const Uint8 &kR = Theme::Cyan.r, &kG = Theme::Cyan.g, &kB = Theme::Cyan.b;      // цвет неона, как у кнопок
    const size_t kNameMax = 40;
    const size_t kDescMax = 500;
    const size_t kTagMax = 24;
    const size_t kXpDigits = 9;                   // опыт: только цифры, до 9 знаков

    SDL_Window* g_window = nullptr;
    TextField   g_name;
    TextArea    g_desc;
    TextField   g_mainXp, g_subXp, g_tag;         // основной опыт, опыт предмета, тег предмета
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
    bool        g_editMode = false;                       // экран открыт для правки готового репозитория
    std::vector<TheoryItem> g_theory;                     // добавленные теории (кнопка "+" на вкладке THEORY)
    std::vector<TheoryItem> g_tests;                      // добавленные тесты (кнопка "+" на вкладке TESTS)
    std::vector<TheoryItem> g_games;                      // добавленные игры (кнопка "+" на вкладке GAMES): title, content = файл запуска, files[0] = папка
    int         g_gameEditIdx = -1;                       // какую игру заменяем (-1 = новая)
    int         g_scrollByTab[4] = { 0, 0, 0, 0 };        // положение ползунка у каждой вкладки (пока ушли на другую)

    bool IsGridTab() { return g_tab >= 1 && g_tab <= 3; }                       // THEORY, TESTS и GAMES: сетка карточек
    std::vector<TheoryItem>& Items() { return g_tab == 3 ? g_games : (g_tab == 2 ? g_tests : g_theory); }   // список текущей вкладки

    // ---------- сетка конспектов на вкладке THEORY ----------
    // 3 колонки x 2 видимых ряда = 6 мест. Конспекты идут по порядку, "+" всегда стоит сразу после последнего.
    // Если мест нужно больше 6, справа появляется ползунок: он листает сетку рядами.
    const int   kCols = 3, kVisRows = 2, kPerPage = kCols * kVisRows;
    const float kGap = 14.f, kBarGutter = 18.f;
    TextButton  g_card[kPerPage];                         // кнопки видимых конспектов (переиспользуются при прокрутке)
    int         g_visFirst = 0, g_visCards = 0;           // какой конспект в g_card[0] и сколько карточек нарисовано
    int         g_scrollRow = 0;                          // верхний видимый ряд
    SDL_FRect   g_gridRect = { 0, 0, 0, 0 };              // область сетки (для колеса мыши)
    SDL_FRect   g_track = { 0, 0, 0, 0 };                 // дорожка ползунка
    SDL_FRect   g_thumb = { 0, 0, 0, 0 };                 // сам ползунок
    bool        g_hasBar = false;
    bool        g_drag = false;                           // ползунок тащат мышью
    float       g_dragOff = 0.f;                          // смещение курсора от верха ползунка

    // ---------- меню конспекта: три точки (1) -> список (2) с EDIT и DELETE (3) ----------
    int         g_menuIdx = -1;                           // у какого конспекта открыто меню (-1 = закрыто)
    int         g_editIdx = -1;                           // какой конспект сейчас правится в окне теории (-1 = новый)
    bool        g_menuLaid = false;                       // true после отрисовки меню (до этого позиции пунктов неверны)
    SDL_FRect   g_menuRect = { 0, 0, 0, 0 };              // рамка выпадающего списка
    SDL_FRect   g_dotsRect = { 0, 0, 0, 0 };              // кнопка с точками у конспекта, у которого открыто меню
    TextButton  g_menuEdit{ 0, 0, 120, 34, "EDIT" };
    TextButton  g_menuDel{ 0, 0, 120, 34, "DELETE" };

    void CloseMenu() { g_menuIdx = -1; g_menuLaid = false; g_menuRect = { 0, 0, 0, 0 }; }

    // кнопка с тремя точками в правом верхнем углу карточки
    SDL_FRect DotsRect(const TextButton& b) { return { b.x + b.w - 34.f, b.y + 6.f, 28.f, 22.f }; }

    int TotalRows() { return (int(Items().size()) + 1 + kCols - 1) / kCols; }   // +1 = место под "+"
    int MaxScroll()  { return std::max(0, TotalRows() - kVisRows); }
    void ClampScroll() { g_scrollRow = std::clamp(g_scrollRow, 0, MaxScroll()); }

    // переход на другую вкладку: запоминаем положение ползунка этой и берём сохранённое у новой
    void SwitchTab(int i) {
        g_scrollByTab[g_tab] = g_scrollRow;
        g_tab = i;
        g_scrollRow = g_scrollByTab[i];
        g_drag = false;
        g_menuIdx = -1; g_menuLaid = false; g_menuRect = { 0, 0, 0, 0 };
        ClampScroll();
    }

    // после добавления прокрутить так, чтобы "+" (а с ним новый конспект) был виден
    void ScrollToPlus() {
        const int plusRow = int(Items().size()) / kCols;
        if (plusRow > g_scrollRow + kVisRows - 1) g_scrollRow = plusRow - (kVisRows - 1);
        ClampScroll();
    }

    // положение ползунка -> номер ряда
    void ScrollFromMouseY(float my) {
        const float travel = g_track.h - g_thumb.h;
        if (travel <= 0.f) return;
        const float t = std::clamp((my - g_dragOff - g_track.y) / travel, 0.f, 1.f);
        g_scrollRow = int(std::lround(t * float(MaxScroll())));
        ClampScroll();
    }

    bool InsideF(const SDL_FRect& f, float mx, float my) {
        return mx >= f.x && mx <= f.x + f.w && my >= f.y && my <= f.y + f.h;
    }

    // название переносим по словам, лишнее обрезаем с ".."
    std::vector<std::string> WrapTitle(const std::string& t, size_t maxChars, size_t maxLines) {
        std::vector<std::string> lines(1);
        maxChars = std::max<size_t>(3, maxChars);
        size_t pos = 0, column = 0;
        while (pos < t.size()) {
            const unsigned char c = static_cast<unsigned char>(t[pos]);
            size_t bytes = c < 0x80 ? 1 : ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : 4));
            if (pos + bytes > t.size()) bytes = 1;
            std::string ch = t.substr(pos, bytes); pos += bytes;
            if (ch == " ") {
                if (column && column < maxChars) { lines.back() += ch; ++column; }
                continue;
            }
            if (column >= maxChars) { lines.emplace_back(); column = 0; }
            lines.back() += ch; ++column;
        }
        while (!lines.empty() && lines.back().empty()) lines.pop_back();
        if (lines.size() > maxLines) { lines.resize(maxLines); lines.back() += ".."; }
        return lines;
    }

    // карточка конспекта: название, внизу пометки (AI, число вложений)
    // showDots: курсор над карточкой или у неё открыто меню -> рисуем три точки
    void DrawCard(SDL_Renderer* r, TextButton& b, const TheoryItem& it, bool showDots) {
        TextButton_Draw(r, b);

        const float scale = (b.w >= 110.f) ? 2.0f : 1.0f;
        const float cs = float(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) * scale;
        const float pad = 12.f;
        const size_t maxChars = size_t(std::max(3.f, (b.w - pad * 2.f - 30.f) / cs));   // справа место под точки
        const size_t maxLines = size_t(std::max(1.f, std::floor((b.h - pad * 2.f - 16.f) / (cs + 4.f))));
        const std::vector<std::string> lines = WrapTitle(it.title, maxChars, maxLines);

        for (size_t i = 0; i < lines.size(); ++i)
            DrawUtf8Text(r, lines[i], b.x + pad, b.y + pad + float(i) * (cs + 4.f),
                std::max(10, static_cast<int>(8.f * scale)), Theme::Ink.r, Theme::Ink.g, Theme::Ink.b, 255);

        std::string foot;
        if (g_tab == 3) {
            foot = it.content;                                 // у игры внизу карточки файл запуска (main.py)
        }
        else {
            if (it.madeWithAI) foot += UiText("AI ", "ИИ ");
            if (!it.files.empty()) foot += std::string(UiText("FILES:", "ФАЙЛЫ:")) + std::to_string(it.files.size());
        }
        if (!foot.empty()) {
            DrawUtf8Text(r, foot, b.x + pad, b.y + b.h - pad - 8.f, 8, kR, kG, kB, 160);
        }

        if (showDots) {                                        // (1) три точки
            const SDL_FRect d = DotsRect(b);
            const bool hot = InsideF(d, g_mx, g_my);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            if (hot) {                                         // под курсором: мягкая подложка и светлее
                SDL_SetRenderDrawColor(r, kR, kG, kB, 45);
                Theme::StyledFill(r, &d);
            }
            SDL_SetRenderDrawColor(r, hot ? Theme::CyanHot.r : kR, hot ? Theme::CyanHot.g : kG, Theme::CyanHot.b, 255);
            const float dot = 4.f, gap = 4.f;
            const float x0 = std::floor(d.x + (d.w - (3.f * dot + 2.f * gap)) * 0.5f);
            const float y0 = std::floor(d.y + (d.h - dot) * 0.5f);
            for (int i = 0; i < 3; ++i) {
                const SDL_FRect p = { x0 + float(i) * (dot + gap), y0, dot, dot };
                Theme::StyledFill(r, &p);
            }
        }
    }

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
        static const SDL_DialogFileFilter filtersEn[] = { { "Images (PNG, BMP)", "png;bmp" } };
        static const SDL_DialogFileFilter filtersRu[] = { { "Изображения (PNG, BMP)", "png;bmp" } };
        const SDL_DialogFileFilter* filters = UiRussianFlag() ? filtersRu : filtersEn;
        SDL_ShowOpenFileDialog(OnFileChosen, nullptr, g_window, filters, 1, nullptr, false);
    }

    // папка игры выбирается в системном диалоге; как и с картинкой, путь приходит из другого потока
    std::mutex        g_gameMutex;
    std::string       g_gamePending;
    std::atomic<bool> g_hasGame{ false };
    std::atomic<bool> g_gameDialogOpen{ false };

    void SDLCALL OnFolderChosen(void*, const char* const* list, int) {
        if (list && list[0]) {                  // NULL = ошибка, пустой список = отмена
            std::lock_guard<std::mutex> lock(g_gameMutex);
            g_gamePending = list[0];
            g_hasGame = true;
        }
        g_gameDialogOpen = false;
    }

    void ChooseGameFolder() {
        if (g_gameDialogOpen.exchange(true)) return;   // диалог уже открыт
        SDL_ShowOpenFolderDialog(OnFolderChosen, nullptr, g_window, nullptr, false);
    }

    void EnsureInit() {
        if (g_init) return;
        g_init = true;
        g_name.placeholder = "NAME";
        g_name.maxLen = kNameMax;
        g_desc.placeholder = "DESCRIPTION";
        g_desc.maxLen = kDescMax;
        g_mainXp.maxLen = g_subXp.maxLen = kXpDigits;
        g_tag.maxLen = kTagMax;
    }

    // поля вкладки INFO по порядку: 0 название, 1 основной опыт, 2 опыт предмета, 3 тег предмета, 4 описание
    const int kFields = 5;
    void Focus(int i) {
        g_name.focused = (i == 0);
        g_mainXp.focused = (i == 1);
        g_subXp.focused = (i == 2);
        g_tag.focused = (i == 3);
        g_desc.focused = (i == 4);
    }
    int FocusedField() {
        if (g_name.focused) return 0;
        if (g_mainXp.focused) return 1;
        if (g_subXp.focused) return 2;
        if (g_tag.focused) return 3;
        if (g_desc.focused) return 4;
        return 0;
    }
    // в полях опыта оставляем только цифры (вставка из буфера тоже)
    void KeepDigits(TextField& f) {
        std::string o;
        for (char ch : f.text) if (ch >= '0' && ch <= '9') o.push_back(ch);
        if (o.size() > kXpDigits) o.resize(kXpDigits);
        f.text = o;
    }
    int ParseXp(const std::string& t) {
        long long v = 0;
        for (char ch : t) if (ch >= '0' && ch <= '9') v = std::min<long long>(1000000000LL, v * 10 + (ch - '0'));
        return int(v);
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

    // ---------- рисование мелочей ----------

    bool Inside(const TextButton& b, float mx, float my) {
        return mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h;
    }

    // плюс из двух полос с мягким свечением, как на главном экране; при наведении светлее
    void DrawPlusIcon(SDL_Renderer* r, const TextButton& b) {
        const float cx = std::floor(b.x + b.w * 0.5f), cy = std::floor(b.y + b.h * 0.5f);
        const float len = 34.f, th = 4.f;
        const Uint8 R = b.hovered ? Theme::CyanHot.r : kR, G = b.hovered ? Theme::CyanHot.g : kG, B = b.hovered ? Theme::CyanHot.b : kB;

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (int i = 5; i >= 1; --i) {
            SDL_SetRenderDrawColor(r, R, G, B, Uint8(34.f * (1.f - i / 6.f)));
            const SDL_FRect hor = { cx - len / 2 - i, cy - th / 2 - i, len + 2.f * i, th + 2.f * i };
            const SDL_FRect ver = { cx - th / 2 - i, cy - len / 2 - i, th + 2.f * i, len + 2.f * i };
            Theme::StyledFill(r, &hor);
            Theme::StyledFill(r, &ver);
        }
        SDL_SetRenderDrawColor(r, R, G, B, 255);
        const SDL_FRect hor = { cx - len / 2, cy - th / 2, len, th };
        const SDL_FRect ver = { cx - th / 2, cy - len / 2, th, len };
        Theme::StyledFill(r, &hor);
        Theme::StyledFill(r, &ver);
    }

    // заглушка в квадрате картинки: плюс и подпись AVATAR
    void DrawPlaceholder(SDL_Renderer* r, const TextButton& b) {
        const float S = b.w, cx = b.x + S * 0.5f, cy = b.y + S * 0.46f;
        const float len = S * 0.30f, th = std::max(3.f, S * 0.035f);
        const Uint8 R = b.hovered ? Theme::CyanHot.r : kR, G = b.hovered ? Theme::CyanHot.g : kG, B = b.hovered ? Theme::CyanHot.b : kB;

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, R, G, B, 255);
        const SDL_FRect hor = { std::floor(cx - len * 0.5f), std::floor(cy - th * 0.5f), std::ceil(len), std::ceil(th) };
        const SDL_FRect ver = { std::floor(cx - th * 0.5f), std::floor(cy - len * 0.5f), std::ceil(th), std::ceil(len) };
        Theme::StyledFill(r, &hor);
        Theme::StyledFill(r, &ver);

        const int fontHeight = (S >= 110.f) ? 16 : 8;
        const std::string label = UiText("AVATAR", "АВАТАР");
        const float textW = Utf8TextWidth(label, fontHeight);
        DrawUtf8Text(r, label, cx - textW * 0.5f, b.y + S * 0.74f,
            fontHeight, R, G, B, 150);
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
    g_mainXp.text.clear(); g_subXp.text.clear(); g_tag.text.clear();
    g_err.clear();
    FreeImage();
    g_hasPending = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_pending.clear();
    }
    g_hasGame = false;
    {
        std::lock_guard<std::mutex> lock(g_gameMutex);
        g_gamePending.clear();
    }
    g_pick.hovered = g_back.hovered = g_ok.hovered = false;
    g_name.hovered = g_desc.hovered = g_mainXp.hovered = g_subXp.hovered = g_tag.hovered = false;
    g_tab = 0;
    g_plusBtn.hovered = false;
    for (TextButton& t : g_tabBtn) t.hovered = false;
    g_mx = g_my = -1.f;
    g_theory.clear();
    g_tests.clear();
    g_games.clear();
    g_gameEditIdx = -1;
    for (int& sr : g_scrollByTab) sr = 0;
    g_scrollRow = 0;
    g_drag = false;
    g_hasBar = false;
    g_editIdx = -1;
    g_editMode = false;
    g_ok.label = "CREATE";
    CloseMenu();
    Theory_Close();
    Focus(0);
    SDL_StartTextInput(window);
}

void Create_OpenEdit(SDL_Window* window, const CreateData& d) {
    Create_Open(window);                           // чистый экран, затем подставляем данные
    g_editMode = true;
    g_ok.label = "SAVE";
    g_name.text = d.name;
    g_desc.text = d.description;
    if (d.mainXp > 0) g_mainXp.text = std::to_string(d.mainXp);
    if (d.subjectXp > 0) g_subXp.text = std::to_string(d.subjectXp);
    g_tag.text = d.subjectTag;
    g_theory = d.theory;
    g_tests = d.tests;
    g_games = d.games;
    if (!d.imagePath.empty() && !SetImage(d.imagePath)) g_err = "CANNOT LOAD IMAGE";
}

void Create_Close(SDL_Window* window) {
    SDL_StopTextInput(window);
    g_name.text.clear();
    g_desc.text.clear();
    g_mainXp.text.clear(); g_subXp.text.clear(); g_tag.text.clear();
    g_err.clear();
    g_theory.clear();
    g_tests.clear();
    g_games.clear();
    g_gameEditIdx = -1;
    for (int& sr : g_scrollByTab) sr = 0;
    g_scrollRow = 0;
    g_drag = false;
    g_editIdx = -1;
    g_editMode = false;
    g_ok.label = "CREATE";
    CloseMenu();
    Theory_Close();
    FreeImage();
    Focus(-1);
}

void Create_Poll() {
    Theory_Poll();                                  // вложения, выбранные в окне теории

    if (g_hasGame.exchange(false)) {                // папка игры из системного диалога
        std::string folder;
        {
            std::lock_guard<std::mutex> lock(g_gameMutex);
            folder = std::move(g_gamePending);
            g_gamePending.clear();
        }
        std::string name, entry, error;
        if (!RepoStore_InspectGame(folder, &name, &entry, &error)) {
            g_err = error;                          // нет main.py, папка слишком большая и т. д.
        }
        else {
            TheoryItem it;
            it.title = name;
            it.content = entry;
            it.files.push_back(folder);
            if (g_gameEditIdx >= 0 && g_gameEditIdx < int(g_games.size())) {
                g_games[size_t(g_gameEditIdx)] = it;          // замена папки у существующей игры
            }
            else {
                g_games.push_back(it);
                if (g_tab == 3) ScrollToPlus();               // "+" уезжает на следующее место
            }
            g_err.clear();
        }
        g_gameEditIdx = -1;
    }

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

    // Все подписи обновляются каждый кадр, поэтому переключение языка действует сразу.
    g_back.label = UiText("BACK", "НАЗАД");
    g_ok.label = g_editMode ? UiText("SAVE", "СОХРАНИТЬ") : UiText("CREATE", "СОЗДАТЬ");
    g_ok.w = std::max(150.f, Utf8TextWidth(g_ok.label, 16) + 32.f);          // «СОХРАНИТЬ» не влезает в 150 px
    g_name.placeholder = UiText("NAME", "НАЗВАНИЕ");
    g_desc.placeholder = UiText("DESCRIPTION", "ОПИСАНИЕ");
    g_mainXp.placeholder = UiText("MAIN XP FOR COMPLETION", "ОСНОВНОЙ ОПЫТ ЗА ВЫПОЛНЕНИЕ");
    g_subXp.placeholder = UiText("SUBJECT XP", "ОПЫТ ПРЕДМЕТА");
    g_tag.placeholder = UiText("SUBJECT TAG", "ТЕГ ПРЕДМЕТА");
    g_menuEdit.label = UiText("EDIT", "ПРАВИТЬ");
    g_menuDel.label = UiText("DELETE", "УДАЛИТЬ");

    // заголовок у самой рамки, как на остальных экранах
    const float titleScale = (w >= 520) ? 3.0f : 2.0f;
    const float titleCy = 5.f + float(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) * titleScale * 0.5f;
    DrawCenteredText(r, g_editMode ? UiText("EDIT", "РЕДАКТИРОВАНИЕ") : UiText("CREATE", "СОЗДАНИЕ"), w, h, titleScale, titleCy - h * 0.5f);

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
        const char* kTabFullRu[4] = { "ОБЩЕЕ", "ТЕОРИЯ", "ТЕСТЫ", "ИГРЫ" };
        t.label = (S >= 100.f) ? (UiRussianFlag() ? kTabFullRu[i] : kTabFull[i]) : kTabShort[i];
        const bool selected = (i == g_tab);
        if (selected) {                                    // выбранная вкладка: мягкая заливка + яркая рамка
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, kR, kG, kB, 40);
            const SDL_FRect fill = { t.x + 4.f, t.y + 4.f, t.w - 8.f, t.h - 8.f };
            Theme::StyledFill(r, &fill);
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

        // основной опыт, опыт предмета и тег предмета: три строки под названием
        TextField* extra[3] = { &g_mainXp, &g_subXp, &g_tag };
        float ey = top + fieldH + 14.f;
        for (TextField* f : extra) {
            f->x = x0; f->y = ey; f->w = rw; f->h = fieldH;
            TextField_Draw(r, *f);
            nd[ndCount++] = TextField_Rect(*f);
            ey += fieldH + 14.f;
        }

        // (1) описание: до нижнего ряда кнопок (с запасом под счётчик символов)
        const float dy = ey;
        g_desc.x = x0; g_desc.y = dy; g_desc.w = rw;
        g_desc.h = std::max(60.f, bottomY - 22.f - dy);
        TextArea_Draw(r, g_desc);
        nd[ndCount++] = TextArea_Rect(g_desc);
    }
    else if (IsGridTab()) {
        // THEORY и TESTS: сетка 3x2. Конспекты идут по порядку, "+" стоит сразу после последнего (на месте, куда
        // добавится следующий). Больше 6 мест -> справа ползунок, сетка листается рядами.
        const float gx = x0, gy = top;
        const float gw = std::max(60.f, rw - kBarGutter);
        const float gh = std::max(60.f, bottomY - 22.f - top);
        const float cw = std::max(24.f, std::floor((gw - float(kCols - 1) * kGap) / float(kCols)));
        const float ch = std::max(24.f, std::floor((gh - float(kVisRows - 1) * kGap) / float(kVisRows)));
        g_gridRect = { gx, gy, gw, gh };

        ClampScroll();
        const int n = int(Items().size());
        const int first = g_scrollRow * kCols;                 // первое видимое место
        g_visFirst = first;
        g_visCards = 0;

        for (int slot = first; slot < first + kPerPage && slot <= n; ++slot) {
            const int rel = slot - first;
            const float sx = gx + float(rel % kCols) * (cw + kGap);
            const float sy = gy + float(rel / kCols) * (ch + kGap);

            if (slot < n) {                                    // конспект
                TextButton& b = g_card[g_visCards++];
                b.x = sx; b.y = sy; b.w = cw; b.h = ch;
                const bool menuHere = (slot == g_menuIdx);
                const bool underMenu = InsideF(g_menuRect, g_mx, g_my);          // курсор над выпадающим списком
                b.hovered = !g_drag && (menuHere || (!underMenu && Inside(b, g_mx, g_my)));
                DrawCard(r, b, Items()[size_t(slot)], b.hovered);
                if (menuHere) g_dotsRect = DotsRect(b);
            }
            else {                                             // "+" на следующем свободном месте
                g_plusBtn.x = sx; g_plusBtn.y = sy; g_plusBtn.w = cw; g_plusBtn.h = ch;
                g_plusBtn.hovered = !g_drag && !InsideF(g_menuRect, g_mx, g_my) && Inside(g_plusBtn, g_mx, g_my);
                TextButton_Draw(r, g_plusBtn);
                DrawPlusIcon(r, g_plusBtn);
            }
        }
        nd[ndCount++] = { int(gx), int(gy), int(gw), int(gh) };

        // (2) выпадающий список поверх сетки: рисуется после всех карточек. Если карточку прокрутили
        // за пределы видимых, либо её уже нет, меню закрываем.
        if (g_menuIdx >= 0 && (g_menuIdx >= n || g_menuIdx < first || g_menuIdx >= first + kPerPage))
            CloseMenu();
        if (g_menuIdx >= 0) {
            const float pad = 8.f, itemW = 140.f, itemH = 34.f, itemGap = 8.f;
            const float mw = itemW + 2.f * pad, mh = 2.f * itemH + itemGap + 2.f * pad;
            const SDL_FRect d = g_dotsRect;
            float mx = d.x + d.w - mw;                                   // правым краем под точки
            float my = d.y + d.h + 4.f;                                  // под кнопкой точек
            if (my + mh > float(h) - 8.f) my = d.y - 4.f - mh;           // не влезает снизу -> над кнопкой
            mx = std::clamp(mx, 6.f, std::max(6.f, float(w) - mw - 6.f));
            my = std::max(my, 6.f);
            g_menuRect = { mx, my, mw, mh };

            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, Theme::Panel.r, Theme::Panel.g, Theme::Panel.b, 255);                  // фон, чтобы текст карточек не просвечивал
            Theme::StyledFill(r, &g_menuRect);
            SDL_SetRenderDrawColor(r, kR, kG, kB, 120);
            Theme::StyledStroke(r, &g_menuRect);

            g_menuEdit.x = mx + pad; g_menuEdit.y = my + pad;
            g_menuDel.x = mx + pad;  g_menuDel.y = my + pad + itemH + itemGap;
            g_menuEdit.w = g_menuDel.w = itemW;
            g_menuEdit.h = g_menuDel.h = itemH;
            g_menuEdit.hovered = Inside(g_menuEdit, g_mx, g_my);
            g_menuDel.hovered = Inside(g_menuDel, g_mx, g_my);
            TextButton_Draw(r, g_menuEdit);
            TextButton_Draw(r, g_menuDel);
            g_menuLaid = true;
            nd[ndCount++] = { int(mx), int(my), int(mw), int(mh) };
        }

        // ползунок: только если мест больше 6
        g_hasBar = MaxScroll() > 0;
        if (g_hasBar) {
            g_track = { gx + gw + 8.f, gy, 6.f, gh };
            const float thumbH = std::max(28.f, g_track.h * float(kVisRows) / float(TotalRows()));
            const float travel = g_track.h - thumbH;
            g_thumb = { g_track.x, g_track.y + travel * float(g_scrollRow) / float(MaxScroll()), g_track.w, thumbH };

            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, kR, kG, kB, 40);        // дорожка
            Theme::StyledFill(r, &g_track);
            const bool hot = g_drag || InsideF(g_thumb, g_mx, g_my);
            for (int i = 3; i >= 1; --i) {                    // мягкое свечение вокруг ползунка
                SDL_SetRenderDrawColor(r, kR, kG, kB, Uint8((hot ? 40 : 26) * (1.f - i / 4.f)));
                const SDL_FRect gl = { g_thumb.x - i, g_thumb.y - i, g_thumb.w + 2.f * i, g_thumb.h + 2.f * i };
                Theme::StyledFill(r, &gl);
            }
            SDL_SetRenderDrawColor(r, hot ? Theme::CyanHot.r : kR, hot ? Theme::CyanHot.g : kG, Theme::CyanHot.b, 255);
            Theme::StyledFill(r, &g_thumb);
            nd[ndCount++] = { int(g_track.x) - 6, int(g_track.y), int(g_track.w) + 12, int(g_track.h) };
        }
    }
    else {
        // (4) игры: пока только кнопка "+" в левом верхнем углу области
        g_hasBar = false;
        g_plusBtn.x = x0; g_plusBtn.y = top; g_plusBtn.w = g_plusBtn.h = 80.f;
        g_plusBtn.hovered = Inside(g_plusBtn, g_mx, g_my);
        TextButton_Draw(r, g_plusBtn);
        DrawPlusIcon(r, g_plusBtn);
        nd[ndCount++] = TextButton_Rect(g_plusBtn);
    }

    // кнопка CREATE справа, BACK слева
    g_back.x = m;                    g_back.y = bottomY;
    g_ok.x = float(w) - 58.f - g_ok.w;  g_ok.y = bottomY;   // справа внизу окна стоит шестерня настроек: оставляем ей место
    TextButton_Draw(r, g_back);
    TextButton_Draw(r, g_ok);
    nd[ndCount++] = TextButton_Rect(g_back);
    nd[ndCount++] = TextButton_Rect(g_ok);

    // ошибка между кнопками внизу
    if (!g_err.empty()) {
        const float scale = (w >= 560) ? 2.0f : 1.0f;
        const std::string shownError = UiMessage(g_err);
        const float lo = g_back.x + g_back.w + 16.f, hi = g_ok.x - 16.f;      // свободное место между кнопками
        DrawUtf8TextFit(r, shownError, lo, bottomY + g_ok.h * 0.5f, hi - lo,
            std::max(8, static_cast<int>(8.f * scale)), true, 255, 90, 90, 255);
    }

    Theory_Draw(r, w, h, nd, ndCount);      // плавающее окно теории поверх экрана
}

CreateResult Create_Handle(const SDL_Event& e) {
    EnsureInit();

    if (e.type == SDL_EVENT_MOUSE_MOTION) { g_mx = e.motion.x; g_my = e.motion.y; }
    if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) { g_mx = g_my = -1.f; }

    // окно теории модальное: пока оно открыто, остальной экран событий не получает
    if (Theory_IsOpen()) {
        const TheoryResult tr = Theory_Handle(e);
        if (tr == TheoryResult::Added) {
            if (g_editIdx >= 0 && g_editIdx < int(Items().size())) {
                Items()[size_t(g_editIdx)] = Theory_Data();   // правка: заменяем конспект на месте
            }
            else {
                Items().push_back(Theory_Data());             // новый: встаёт на место "+", а "+" уезжает на следующее
                ScrollToPlus();
            }
            g_editIdx = -1;
        }
        else if (tr == TheoryResult::Cancel) {
            g_editIdx = -1;
        }
        return CreateResult::None;
    }

    // меню конспекта (3 точки -> EDIT / DELETE): пока открыто, оно первым получает клики и Esc
    if (IsGridTab() && g_menuIdx >= 0) {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
            CloseMenu();
            return CreateResult::None;
        }
        if (g_menuLaid) {
            if (TextButton_Handle(g_menuEdit, e)) {            // (3) редактировать: то же окно, но с данными
                const int idx = g_menuIdx;
                CloseMenu();
                if (idx >= 0 && idx < int(Items().size())) {
                    if (g_tab == 3) {                              // игра: выбрать другую папку вместо этой
                        g_gameEditIdx = idx;
                        ChooseGameFolder();
                    }
                    else {
                        g_editIdx = idx;
                        Theory_Edit(g_window, Items()[size_t(idx)], g_tab == 2);
                    }
                }
                return CreateResult::None;
            }
            if (TextButton_Handle(g_menuDel, e)) {             // (3) удалить
                const int idx = g_menuIdx;
                CloseMenu();
                if (idx >= 0 && idx < int(Items().size())) {
                    Items().erase(Items().begin() + idx);
                    ClampScroll();                              // рядов могло стать меньше
                }
                return CreateResult::None;
            }
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            const bool onMenu = InsideF(g_menuRect, e.button.x, e.button.y);
            const bool onDots = InsideF(g_dotsRect, e.button.x, e.button.y);
            if (onMenu) return CreateResult::None;             // клик в рамку между пунктами: ничего
            if (!onDots) CloseMenu();                          // клик мимо: закрыть (клик по точкам ниже переключит)
        }
    }

    // ползунок сетки конспектов: перетаскивание, клик по дорожке, колесо мыши
    if (IsGridTab() && g_hasBar) {
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            const SDL_FRect hit = { g_track.x - 6.f, g_track.y, g_track.w + 12.f, g_track.h };
            if (InsideF(hit, e.button.x, e.button.y)) {
                g_drag = true;
                g_dragOff = InsideF({ hit.x, g_thumb.y, hit.w, g_thumb.h }, e.button.x, e.button.y)
                          ? e.button.y - g_thumb.y            // схватили сам ползунок
                          : g_thumb.h * 0.5f;                 // клик по дорожке: ползунок прыгает под курсор
                ScrollFromMouseY(e.button.y);
                return CreateResult::None;
            }
        }
        if (e.type == SDL_EVENT_MOUSE_MOTION && g_drag) ScrollFromMouseY(e.motion.y);
        if (e.type == SDL_EVENT_MOUSE_WHEEL && e.wheel.y != 0.f &&
            (InsideF(g_gridRect, g_mx, g_my) || InsideF(g_track, g_mx, g_my)))
            { g_scrollRow += (e.wheel.y > 0.f) ? -1 : 1; ClampScroll(); }   // колесо вверх = ряд выше
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) g_drag = false;

    // вкладки
    for (int i = 0; i < kTabs; ++i) {
        if (TextButton_Handle(g_tabBtn[i], e) && g_tab != i) {
            SwitchTab(i);
            g_name.hovered = g_desc.hovered = g_plusBtn.hovered = g_mainXp.hovered = g_subXp.hovered = g_tag.hovered = false;
        }
    }

    // содержимое вкладки (положение чужих элементов устарело, поэтому их события не смотрим)
    if (g_tab == 0) {
        if (TextField_Handle(g_name, e)) Focus(0);
        if (TextField_Handle(g_mainXp, e)) Focus(1);
        if (TextField_Handle(g_subXp, e)) Focus(2);
        if (TextField_Handle(g_tag, e)) Focus(3);
        if (TextArea_Handle(g_desc, e)) Focus(4);
    }
    else {
        if (TextButton_Handle(g_plusBtn, e)) {
            if (g_tab == 3) {                          // GAMES: выбрать папку с игрой (main.py внутри)
                g_gameEditIdx = -1;
                ChooseGameFolder();
            }
            else if (IsGridTab()) {                    // THEORY / TESTS: плавающее окно (у теста есть строка верных ответов)
                g_editIdx = -1;
                Theory_Open(g_window, g_tab == 2);
            }
        }
        if (IsGridTab()) {
            for (int i = 0; i < g_visCards; ++i) {
                const int slot = g_visFirst + i;
                // (1) клик по трём точкам: открыть меню этого конспекта или закрыть, если оно уже открыто
                if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT &&
                    InsideF(DotsRect(g_card[i]), e.button.x, e.button.y)) {
                    if (g_menuIdx == slot) {
                        CloseMenu();
                    }
                    else {
                        g_menuIdx = slot;
                        g_menuLaid = false;                    // позиции пунктов появятся после ближайшей отрисовки
                        g_dotsRect = DotsRect(g_card[i]);
                    }
                    continue;
                }
                if (TextButton_Handle(g_card[i], e)) {
                    // сюда: открыть конспект g_theory[slot] (по клику на саму карточку)
                }
            }
        }
    }

    if (TextButton_Handle(g_pick, e)) ChooseFile();
    if (TextButton_Handle(g_back, e)) return CreateResult::Back;
    if (TextButton_Handle(g_ok, e)) {
        const bool blank = g_name.text.find_first_not_of(' ') == std::string::npos;
        if (blank) {                                  // название живёт на вкладке INFO: показать её
            SwitchTab(0);
            g_err = "ENTER NAME";
            Focus(0);
            return CreateResult::None;
        }
        g_err.clear();
        return CreateResult::Done;
    }

    // ввод символов (только на вкладке INFO); при правке убираем сообщение об ошибке
    const bool backspace = e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_BACKSPACE;
    if (g_tab == 0 && (e.type == SDL_EVENT_TEXT_INPUT || backspace || IsEditShortcut(e))) {
        TextField_Input(g_name, e);
        TextField_Input(g_mainXp, e);
        TextField_Input(g_subXp, e);
        TextField_Input(g_tag, e);
        KeepDigits(g_mainXp);
        KeepDigits(g_subXp);
        TextArea_Input(g_desc, e);
        g_err.clear();
    }

    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_ESCAPE) return CreateResult::Back;

        if (g_tab == 0) {
            switch (e.key.key) {
            case SDLK_TAB:
                Focus((FocusedField() + 1) % kFields);
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (g_desc.focused) {
                    if (g_desc.text.size() < kDescMax) g_desc.text.push_back('\n');   // новая строка
                }
                else {
                    Focus(FocusedField() + 1);                                        // к следующему полю, из тега в описание
                }
                break;
            }
        }
    }
    return CreateResult::None;
}

void Create_Resume(SDL_Window* window, const char* error) {
    g_window = window;
    g_err = error ? error : "";
    g_mx = g_my = -1.f;
    g_pick.hovered = g_back.hovered = g_ok.hovered = false;
    g_drag = false;
    CloseMenu();
    SDL_StartTextInput(window);
}

CreateData Create_Data() {
    CreateData d{ g_name.text, g_desc.text, g_imgPath, g_theory, g_tests, g_games };
    d.mainXp = ParseXp(g_mainXp.text);
    d.subjectXp = ParseXp(g_subXp.text);
    d.subjectTag = g_tag.text;
    return d;
}

void Create_Destroy() {
    FreeImage();
    TextField_Destroy(g_name);
    TextArea_Destroy(g_desc);
    TextField_Destroy(g_mainXp);
    TextField_Destroy(g_subXp);
    TextField_Destroy(g_tag);
    Theory_Destroy();
    TextButton_Destroy(g_pick);
    for (TextButton& t : g_tabBtn) TextButton_Destroy(t);
    TextButton_Destroy(g_plusBtn);
    for (TextButton& c : g_card) TextButton_Destroy(c);
    TextButton_Destroy(g_menuEdit);
    TextButton_Destroy(g_menuDel);
    TextButton_Destroy(g_back);
    TextButton_Destroy(g_ok);
}

#include "MainScreen.h"
#include "Avatar.h"
#include "ConfirmDialog.h"
#include "TextButton.h"
#include "Text.h"
#include "RepoStore.h"
#include "Localization.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace {
    const Uint8 kR = 90, kG = 200, kB = 255;   // цвет неона, как у кнопок

    // аватар: круг в левом верхнем углу
    const float kAvX = 20.f, kAvY = 16.f, kAvD = 40.f;

    // выпадающий список под аватаром: PROFILE, LOGOUT
    const float kItemH = 36.f, kItemGap = 8.f, kMenuPad = 10.f;
    const float kMenuW = 170.f;
    const float kMenuH = kMenuPad * 2 + kItemH * 2 + kItemGap;

    std::string g_login;
    bool        g_open = false;                 // список открыт
    bool        g_avHover = false;

    TextButton g_panel{ 0, 0, kMenuW, kMenuH, "" };              // неоновая рамка списка
    TextButton g_profile{ 0, 0, kMenuW - 20.f, kItemH, "PROFILE" };
    TextButton g_logout{ 0, 0, kMenuW - 20.f, kItemH, "LOGOUT" };

    // ---------- сетка репозиториев ----------
    // Карточки идут по порядку (слева направо, сверху вниз), "+" стоит сразу после последней.
    // Колонок и видимых рядов столько, сколько влезает в окно; лишние ряды листаются колесом
    // или ползунком справа. Размер карточки фиксированный, как у "+".
    const float kCardW = 150.f, kCardH = 176.f, kGap = 16.f;
    const float kGridX = 20.f, kGridY = 84.f, kBarGutter = 32.f;   // справа оставлен запас: внизу стоит шестерня настроек
    const float kCardPad = 16.f;

    struct RepoCard {
        std::string  name;
        std::string  dir;                // папка репозитория на диске
        SDL_Surface* img = nullptr;      // квадрат 128x128 (или nullptr, если обложки нет)
        SDL_Texture* tex = nullptr;
    };
    std::vector<RepoCard> g_repos;
    SDL_Renderer*         g_texOwner = nullptr;
    TextButton            g_card{ 0, 0, kCardW, kCardH, "" };     // рамка карточки (рисуется для каждой)

    // ---------- меню карточки (ЛКМ по репозиторию): DELETE, EDIT, OPEN ----------
    const float kCmItemW = 140.f, kCmItemH = 34.f, kCmGap = 8.f, kCmPad = 8.f;
    std::string g_delDir;                        // репозиторий, ожидающий подтверждения удаления (пусто = нет)
    int         g_cmIdx = -1;                    // у какой карточки открыто меню (-1 = закрыто)
    float       g_cmX = 0.f, g_cmY = 0.f;        // куда кликнули: левый верхний угол меню
    bool        g_cmLaid = false;                // true после отрисовки (до неё позиции пунктов неверны)
    SDL_FRect   g_cmRect = { 0, 0, 0, 0 };
    TextButton  g_cmDelete{ 0, 0, kCmItemW, kCmItemH, "DELETE" };
    TextButton  g_cmEdit{ 0, 0, kCmItemW, kCmItemH, "EDIT" };
    TextButton  g_cmOpen{ 0, 0, kCmItemW, kCmItemH, "OPEN" };
    std::string g_chosenDir;                     // папка для Main_RepoDir()

    struct VisCard { SDL_FRect rc; int slot; };  // карточки, нарисованные в этом кадре (для кликов)
    std::vector<VisCard> g_vis;

    void CloseCardMenu() { g_cmIdx = -1; g_cmLaid = false; g_cmRect = { 0, 0, 0, 0 }; }

    TextButton g_plus{ 0, 0, kCardW, kCardH, "" };               // "+": рамку рисует TextButton, сам плюс отдельно
    bool       g_plusVisible = false;                            // "+" сейчас на экране (не уехал при прокрутке)
    int        g_scrollRow = 0;
    int        g_cols = 1, g_visRows = 1;
    SDL_FRect  g_gridRect = { 0, 0, 0, 0 };
    SDL_FRect  g_track = { 0, 0, 0, 0 };
    SDL_FRect  g_thumb = { 0, 0, 0, 0 };
    bool       g_hasBar = false, g_drag = false;
    float      g_dragOff = 0.f, g_mx = -1.f, g_my = -1.f;

    int  TotalRows() { return (int(g_repos.size()) + 1 + g_cols - 1) / g_cols; }   // +1 = место под "+"
    int  MaxScroll() { return std::max(0, TotalRows() - g_visRows); }
    void ClampScroll() { g_scrollRow = std::clamp(g_scrollRow, 0, MaxScroll()); }

    bool InsideF(const SDL_FRect& f, float mx, float my) {
        return mx >= f.x && mx <= f.x + f.w && my >= f.y && my <= f.y + f.h;
    }

    void ScrollFromMouseY(float my) {
        const float travel = g_track.h - g_thumb.h;
        if (travel <= 0.f) return;
        const float t = std::clamp((my - g_dragOff - g_track.y) / travel, 0.f, 1.f);
        g_scrollRow = int(std::lround(t * float(MaxScroll())));
        ClampScroll();
    }

    void FreeRepos() {
        for (RepoCard& c : g_repos) {
            if (c.tex) SDL_DestroyTexture(c.tex);
            if (c.img) SDL_DestroySurface(c.img);
        }
        g_repos.clear();
    }

    // картинка -> центральный квадрат 128x128 RGBA32; nullptr при ошибке
    SDL_Surface* MakeCover(const std::string& path) {
        const int kSide = 128;
        SDL_Surface* src = SDL_LoadSurface(path.c_str());   // PNG или BMP
        if (!src) return nullptr;
        SDL_Surface* cur = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(src);
        if (!cur) return nullptr;

        const int side = std::min(cur->w, cur->h);
        if (side < 1) { SDL_DestroySurface(cur); return nullptr; }
        if (cur->w != cur->h) {                              // обрезка по центру до квадрата
            SDL_Surface* sq = SDL_CreateSurface(side, side, SDL_PIXELFORMAT_RGBA32);
            if (!sq) { SDL_DestroySurface(cur); return nullptr; }
            const SDL_Rect rc = { (cur->w - side) / 2, (cur->h - side) / 2, side, side };
            SDL_SetSurfaceBlendMode(cur, SDL_BLENDMODE_NONE);
            SDL_BlitSurface(cur, &rc, sq, nullptr);
            SDL_DestroySurface(cur);
            cur = sq;
        }
        while (cur->w > kSide) {                             // сжатие по шагам вдвое
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

    void LoadRepos() {
        FreeRepos();
        for (const RepoInfo& ri : RepoStore_List(g_login)) {
            RepoCard c;
            c.name = ri.name;
            c.dir = ri.dir;
            if (!ri.coverPath.empty()) c.img = MakeCover(ri.coverPath);
            g_repos.push_back(std::move(c));
        }
        g_scrollRow = 0;
        g_drag = false;
        CloseCardMenu();
    }

    void EnsureTextures(SDL_Renderer* r) {
        if (g_texOwner != r) {                               // новый рендерер: текстуры пересоздать
            for (RepoCard& c : g_repos) if (c.tex) { SDL_DestroyTexture(c.tex); c.tex = nullptr; }
            g_texOwner = r;
        }
        for (RepoCard& c : g_repos)
            if (c.img && !c.tex) c.tex = SDL_CreateTextureFromSurface(r, c.img);
    }

    // плавающее окно "+": две квадратные кнопки с подписями под ними
    const float kAddPad = 24.f;        // отступ кнопок от краёв окна
    const float kAddGap = 30.f;        // зазор между кнопками и по бокам
    const float kAddLabelH = 30.f;     // место под подпись (зазор + высота букв)
    bool       g_add = false;          // окно открыто
    TextButton g_addPanel{ 0, 0, 100, 100, "" };    // неоновая рамка окна
    TextButton g_import{ 0, 0, 100, 100, "" };      // подписи рисуются отдельно, под кнопками
    TextButton g_create{ 0, 0, 100, 100, "" };

    bool InAddPanel(float mx, float my) {
        return mx >= g_addPanel.x && mx <= g_addPanel.x + g_addPanel.w &&
            my >= g_addPanel.y && my <= g_addPanel.y + g_addPanel.h;
    }

    bool InAvatar(float mx, float my) {
        const float dx = mx - (kAvX + kAvD * 0.5f), dy = my - (kAvY + kAvD * 0.5f);
        return dx * dx + dy * dy <= (kAvD * 0.5f) * (kAvD * 0.5f);
    }

    bool InMenu(float mx, float my) {
        return mx >= g_panel.x && mx <= g_panel.x + g_panel.w &&
            my >= g_panel.y && my <= g_panel.y + g_panel.h;
    }

    // надпись от левого края; при нехватке места мельче, затем с обрезкой
    size_t Utf8ByteAt(const std::string& value, size_t charIndex) {
        size_t pos = 0;
        while (pos < value.size() && charIndex) {
            const unsigned char c = static_cast<unsigned char>(value[pos]);
            pos += c < 0x80 ? 1 : ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : 4));
            --charIndex;
        }
        return std::min(pos, value.size());
    }

    // Раньше ширина символа считалась как 9 px при реальных 16 px, и длинный логин заезжал на кнопки окна.
    void DrawLeftText(SDL_Renderer* r, const std::string& s, float x, float cy, float maxW) {
        DrawUtf8TextFit(r, s, x, cy, maxW, 16, false, kR, kG, kB, 255);
    }

    // maxW > 0: подпись подгоняется под эту ширину (мельче, затем с "..")
    void DrawCenterLabel(SDL_Renderer* r, const char* value, float cx, float cy, float scale, bool hovered,
        float maxW = 0.f) {
        const std::string s = value ? value : "";
        const int font = std::max(10, static_cast<int>(8.f * scale));
        if (maxW > 0.f) {
            DrawUtf8TextFit(r, s, cx - maxW * 0.5f, cy, maxW, font, true,
                hovered ? 200 : kR, hovered ? 240 : kG, 255, 255);
            return;
        }
        const float width = Utf8TextWidth(s, font);
        DrawUtf8Text(r, s, cx - width * 0.5f, cy - font * 0.5f, font,
            hovered ? 200 : kR, hovered ? 240 : kG, 255, 255);
    }

    // плюс из двух полос с мягким свечением; при наведении светлее
    void DrawPlus(SDL_Renderer* r, const TextButton& b) {
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

    // подпись по центру (cx), с мягким свечением; при наведении светлее

    // название под обложкой: до двух строк по центру, лишнее обрезаем с ".."
    void DrawCardName(SDL_Renderer* r, const std::string& name, float cx, float top, float maxW) {
        const size_t fit = static_cast<size_t>(std::max(3.f, std::floor(maxW / 9.f)));
        std::string first = name, second;
        if (Utf8Count(first) > fit) {
            size_t cut = Utf8ByteAt(first, fit);
            const size_t space = first.rfind(' ', cut);
            if (space != std::string::npos && space > 0) cut = space;
            second = first.substr(cut);
            first.resize(cut);
            while (!second.empty() && second.front() == ' ') second.erase(0, 1);
            if (Utf8Count(second) > fit) second.resize(Utf8ByteAt(second, fit > 2 ? fit - 2 : fit)), second += "..";
        }
        DrawCenterLabel(r, first.c_str(), cx, top + 8.f, 1.0f, false);
        if (!second.empty()) DrawCenterLabel(r, second.c_str(), cx, top + 24.f, 1.0f, false);
    }

    // карточка репозитория: рамка, обложка (или пустой квадрат) и название
    void DrawRepoCard(SDL_Renderer* r, const RepoCard& c, float x, float y, bool hot) {
        g_card.x = x; g_card.y = y; g_card.w = kCardW; g_card.h = kCardH;
        g_card.hovered = hot;                                     // под курсором или с открытым меню
        TextButton_Draw(r, g_card);

        const float S = kCardW - kCardPad * 2.f;
        const SDL_FRect box = { std::floor(x + kCardPad), std::floor(y + kCardPad), S, S };
        if (c.tex) {
            SDL_RenderTexture(r, c.tex, nullptr, &box);
        }
        else {                                                    // обложки нет: тонкий пустой квадрат
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, kR, kG, kB, 90);
            SDL_RenderRect(r, &box);
        }
        DrawCardName(r, c.name, x + kCardW * 0.5f, box.y + S + 12.f, kCardW - 20.f);
    }

    // Заливка скруглённого прямоугольника (по строкам): закрывает всё под рамкой вплоть до самого края,
    // но не торчит за скруглёнными углами. Нужна, чтобы карточка под списком не просвечивала.
    void FillRounded(SDL_Renderer* r, const SDL_FRect& rc, float rad) {
        const int rows = int(std::ceil(rc.h));
        for (int i = 0; i < rows; ++i) {
            const float fromEdge = std::min(float(i) + 0.5f, rc.h - float(i) - 0.5f);
            float inset = 0.f;
            if (fromEdge < rad) {
                const float dy = rad - fromEdge;
                inset = rad - std::sqrt(std::max(0.f, rad * rad - dy * dy));
            }
            const SDL_FRect line = { std::floor(rc.x + inset), std::floor(rc.y) + float(i),
                                     std::ceil(rc.w - 2.f * inset), 1.f };
            SDL_RenderFillRect(r, &line);
        }
    }

    void FillR(SDL_Renderer* r, float x, float y, float w, float h) {
        const SDL_FRect f = { std::floor(x), std::floor(y), std::ceil(w), std::ceil(h) };
        SDL_RenderFillRect(r, &f);
    }

    // значок IMPORT: стрелка вниз над лотком
    void DrawImportIcon(SDL_Renderer* r, const TextButton& b) {
        const float S = b.w, cx = b.x + S * 0.5f, cy = b.y + S * 0.5f;
        const float th = std::max(3.f, S * 0.045f);
        const Uint8 R = b.hovered ? 200 : kR, G = b.hovered ? 240 : kG, B = b.hovered ? 255 : kB;
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, R, G, B, 255);

        FillR(r, cx - th * 0.5f, cy - 0.27f * S, th, 0.30f * S);              // стержень
        const float base = cy - 0.02f * S, tip = cy + 0.17f * S, hw = 0.15f * S;
        for (float yy = base; yy < tip; yy += 1.f) {                          // наконечник (треугольник)
            const float k = hw * (tip - yy) / (tip - base);
            FillR(r, cx - k, yy, k * 2.f, 1.f);
        }
        const float tw = 0.26f * S, ty = cy + 0.27f * S;                      // лоток
        FillR(r, cx - tw, ty, tw * 2.f, th);
        FillR(r, cx - tw, ty - 0.13f * S, th, 0.13f * S + th);
        FillR(r, cx + tw - th, ty - 0.13f * S, th, 0.13f * S + th);
    }

    // значок CREATE: листок с плюсом
    void DrawCreateIcon(SDL_Renderer* r, const TextButton& b) {
        const float S = b.w, cx = b.x + S * 0.5f, cy = b.y + S * 0.5f;
        const float th = std::max(3.f, S * 0.045f);
        const Uint8 R = b.hovered ? 200 : kR, G = b.hovered ? 240 : kG, B = b.hovered ? 255 : kB;
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, R, G, B, 255);

        const float sw = 0.40f * S, sh = 0.54f * S, sx = cx - sw * 0.5f, sy = cy - sh * 0.5f;
        FillR(r, sx, sy, sw, th);                    // рамка листка
        FillR(r, sx, sy + sh - th, sw, th);
        FillR(r, sx, sy, th, sh);
        FillR(r, sx + sw - th, sy, th, sh);

        const float len = 0.20f * S;                 // плюс внутри
        FillR(r, cx - len * 0.5f, cy - th * 0.5f, len, th);
        FillR(r, cx - th * 0.5f, cy - len * 0.5f, th, len);
    }
}

void Main_Open(const std::string& login) {
    g_login = login;
    g_open = false;
    g_add = false;
    g_delDir.clear();
    Confirm_Close();
    g_avHover = false;
    g_profile.hovered = g_logout.hovered = g_plus.hovered = false;
    g_import.hovered = g_create.hovered = false;
    g_mx = g_my = -1.f;
    g_chosenDir.clear();
    LoadRepos();
}

std::string Main_RepoDir() {
    return g_chosenDir;
}

void Main_Reload() {
    LoadRepos();            // после сохранения нового репозитория
}

void Main_Close() {
    g_login.clear();
    g_open = false;
    g_add = false;
    g_drag = false;
    g_delDir.clear();
    Confirm_Close();
    g_chosenDir.clear();
    FreeRepos();
}

void Main_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount) {
    g_profile.label = UiText("PROFILE", "ПРОФИЛЬ");
    g_logout.label = UiText("LOGOUT", "ВЫЙТИ");
    g_cmDelete.label = UiText("DELETE", "УДАЛИТЬ");
    g_cmEdit.label = UiText("EDIT", "ПРАВИТЬ");
    g_cmOpen.label = UiText("OPEN", "ОТКРЫТЬ");

    // надпись как можно выше, у самой рамки окна (центр строки = 5 + половина высоты букв)
    const float titleScale = (w >= 520) ? 3.0f : 2.0f;
    const float titleTop = 5.f;
    const float titleCy = titleTop + float(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) * titleScale * 0.5f;
    DrawCenteredText(r, "by Aquarius", w, h, titleScale, titleCy - h * 0.5f);

    // сюда: содержимое главного экрана

    // аватар (картинка или пустой круг); светится ярче при наведении или открытом списке
    Avatar_Draw(r, kAvX + kAvD * 0.5f, kAvY + kAvD * 0.5f, kAvD, g_avHover || g_open);
    nd[ndCount++] = { int(kAvX) - 6, int(kAvY) - 6, int(kAvD) + 12, int(kAvD) + 12 };

    // логин справа от аватара; справа оставляем место под кнопку меню
    const float textX = kAvX + kAvD + 14.f;
    DrawLeftText(r, g_login, textX, kAvY + kAvD * 0.5f, float(w) - textX - 70.f);

    // сетка репозиториев; "+" стоит сразу после последней карточки (список аватара рисуется поверх)
    {
        const float gx = kGridX, gy = kGridY;
        const float availW = std::max(kCardW, float(w) - gx * 2.f - kBarGutter);
        const float availH = std::max(kCardH, float(h) - gy - 20.f);
        g_cols = std::max(1, int((availW + kGap) / (kCardW + kGap)));
        g_visRows = std::max(1, int((availH + kGap) / (kCardH + kGap)));
        g_gridRect = { gx, gy, float(g_cols) * (kCardW + kGap) - kGap, float(g_visRows) * (kCardH + kGap) - kGap };
        ClampScroll();
        EnsureTextures(r);

        const int n = int(g_repos.size());
        const int first = g_scrollRow * g_cols;
        g_plusVisible = false;
        g_vis.clear();
        const bool blocked = g_open || g_add || g_drag || Confirm_IsOpen();          // список аватара / окно "+" / ползунок: карточки не подсвечиваем
        for (int slot = first; slot < first + g_cols * g_visRows && slot <= n; ++slot) {
            const int rel = slot - first;
            const float sx = gx + float(rel % g_cols) * (kCardW + kGap);
            const float sy = gy + float(rel / g_cols) * (kCardH + kGap);
            if (slot < n) {
                const SDL_FRect rc = { sx, sy, kCardW, kCardH };
                g_vis.push_back({ rc, slot });
                const bool underMenu = InsideF(g_cmRect, g_mx, g_my);   // курсор над меню карточки
                const bool hot = (slot == g_cmIdx) || (!blocked && !underMenu && InsideF(rc, g_mx, g_my));
                DrawRepoCard(r, g_repos[size_t(slot)], sx, sy, hot);
            }
            else {                                                // "+" на следующем свободном месте
                g_plus.x = sx;
                g_plus.y = sy;
                g_plusVisible = true;
                TextButton_Draw(r, g_plus);
                DrawPlus(r, g_plus);
            }
        }
        nd[ndCount++] = { int(g_gridRect.x), int(g_gridRect.y), int(g_gridRect.w), int(g_gridRect.h) };

        // ползунок: только если ряды не помещаются
        g_hasBar = MaxScroll() > 0;
        if (g_hasBar) {
            g_track = { g_gridRect.x + g_gridRect.w + 8.f, gy, 6.f, g_gridRect.h };
            // дорожка не заезжает на шестерню настроек в правом нижнем углу
            const float gearTop = float(h) - 56.f;
            if (g_track.x + g_track.w + 6.f > float(w) - 52.f && gearTop - g_track.y >= 60.f && g_track.y + g_track.h > gearTop)
                g_track.h = gearTop - g_track.y;
            const float thumbH = std::max(28.f, g_track.h * float(g_visRows) / float(TotalRows()));
            const float travel = g_track.h - thumbH;
            g_thumb = { g_track.x, g_track.y + travel * float(g_scrollRow) / float(MaxScroll()), g_track.w, thumbH };

            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, kR, kG, kB, 40);            // дорожка
            SDL_RenderFillRect(r, &g_track);
            const bool hot = g_drag || InsideF(g_thumb, g_mx, g_my);
            for (int i = 3; i >= 1; --i) {                        // мягкое свечение вокруг ползунка
                SDL_SetRenderDrawColor(r, kR, kG, kB, Uint8((hot ? 40 : 26) * (1.f - i / 4.f)));
                const SDL_FRect gl = { g_thumb.x - i, g_thumb.y - i, g_thumb.w + 2.f * i, g_thumb.h + 2.f * i };
                SDL_RenderFillRect(r, &gl);
            }
            SDL_SetRenderDrawColor(r, hot ? 200 : kR, hot ? 240 : kG, 255, 255);
            SDL_RenderFillRect(r, &g_thumb);
            nd[ndCount++] = { int(g_track.x) - 6, int(g_track.y), int(g_track.w) + 12, int(g_track.h) };
        }
    }

    // меню карточки: рисуется поверх сетки. Если карточку прокрутили за край или её удалили - закрываем
    if (g_cmIdx >= 0) {
        bool visible = false;
        for (const VisCard& v : g_vis) if (v.slot == g_cmIdx) { visible = true; break; }
        if (!visible || g_open || g_add) CloseCardMenu();
    }
    if (g_cmIdx >= 0) {
        const float mw = kCmItemW + 2.f * kCmPad, mh = 3.f * kCmItemH + 2.f * kCmGap + 2.f * kCmPad;
        const float mx = std::clamp(g_cmX, 6.f, std::max(6.f, float(w) - mw - 6.f));      // не выходит за окно
        const float my = std::clamp(g_cmY, 6.f, std::max(6.f, float(h) - mh - 6.f));
        g_cmRect = { mx, my, mw, mh };

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 22, 24, 30, 255);                      // фон, чтобы карточки не просвечивали
        SDL_RenderFillRect(r, &g_cmRect);
        SDL_SetRenderDrawColor(r, kR, kG, kB, 120);
        SDL_RenderRect(r, &g_cmRect);

        g_cmDelete.x = g_cmEdit.x = g_cmOpen.x = mx + kCmPad;
        g_cmDelete.y = my + kCmPad;
        g_cmEdit.y = g_cmDelete.y + kCmItemH + kCmGap;
        g_cmOpen.y = g_cmEdit.y + kCmItemH + kCmGap;
        g_cmDelete.hovered = InsideF({ g_cmDelete.x, g_cmDelete.y, kCmItemW, kCmItemH }, g_mx, g_my);
        g_cmEdit.hovered   = InsideF({ g_cmEdit.x, g_cmEdit.y, kCmItemW, kCmItemH }, g_mx, g_my);
        g_cmOpen.hovered   = InsideF({ g_cmOpen.x, g_cmOpen.y, kCmItemW, kCmItemH }, g_mx, g_my);
        TextButton_Draw(r, g_cmDelete);
        TextButton_Draw(r, g_cmEdit);
        TextButton_Draw(r, g_cmOpen);
        g_cmLaid = true;
        nd[ndCount++] = { int(mx), int(my), int(mw), int(mh) };
    }

    // выпадающий список
    if (g_open) {
        g_panel.x = kAvX;
        g_panel.y = kAvY + kAvD + 10.f;
        g_panel.hovered = false;

        // фон списка: по форме скруглённой рамки до самого края (раньше отступ в 4 px пропускал угол карточки под списком)
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 22, 24, 30, 255);
        FillRounded(r, { g_panel.x + 1.f, g_panel.y + 1.f, g_panel.w - 2.f, g_panel.h - 2.f }, 9.f);
        TextButton_Draw(r, g_panel);

        g_profile.x = g_logout.x = g_panel.x + 10.f;
        g_profile.y = g_panel.y + kMenuPad;
        g_logout.y = g_profile.y + kItemH + kItemGap;
        TextButton_Draw(r, g_profile);
        TextButton_Draw(r, g_logout);

        nd[ndCount++] = TextButton_Rect(g_panel);
    }

    // плавающее окно "+": рисуется поверх всего остального
    if (g_add) {
        // сторона квадратной кнопки: до 120 px, меньше в маленьком окне
        float S = std::min(120.f, (float(w) - 40.f - 3.f * kAddGap) * 0.5f);
        S = std::min(S, float(h) - 60.f - 2.f * kAddPad - kAddLabelH);
        S = std::max(S, 30.f);
        S = std::floor(S);

        const float pw = 2.f * S + 3.f * kAddGap;
        const float ph = S + kAddLabelH + 2.f * kAddPad;
        const float px = std::floor((float(w) - pw) * 0.5f);
        const float py = std::floor((float(h) - ph) * 0.5f);

        // затемнение всего окна
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 175);
        SDL_RenderFillRect(r, nullptr);

        // фон окна (с отступом, чтобы не торчал за скруглёнными углами рамки)
        SDL_SetRenderDrawColor(r, 22, 24, 30, 255);
        const SDL_FRect bg = { px + 4, py + 4, pw - 8, ph - 8 };
        SDL_RenderFillRect(r, &bg);

        g_addPanel.x = px; g_addPanel.y = py; g_addPanel.w = pw; g_addPanel.h = ph;
        g_addPanel.hovered = false;
        TextButton_Draw(r, g_addPanel);

        g_import.w = g_import.h = g_create.w = g_create.h = S;
        g_import.x = px + kAddGap;
        g_create.x = g_import.x + S + kAddGap;
        g_import.y = g_create.y = py + kAddPad;
        TextButton_Draw(r, g_import);
        TextButton_Draw(r, g_create);
        DrawImportIcon(r, g_import);
        DrawCreateIcon(r, g_create);

        const float labelScale = (S >= 100.f) ? 2.0f : 1.0f;
        const float labelCy = g_import.y + S + kAddLabelH * 0.5f + 2.f;
        const float labelMaxW = S + kAddGap - 8.f;                 // не заходит на подпись соседней кнопки
        DrawCenterLabel(r, UiText("IMPORT", "ИМПОРТ"), g_import.x + S * 0.5f, labelCy, labelScale, g_import.hovered, labelMaxW);
        DrawCenterLabel(r, UiText("CREATE", "СОЗДАТЬ"), g_create.x + S * 0.5f, labelCy, labelScale, g_create.hovered, labelMaxW);

        // всё окно без перетаскивания
        nd[ndCount++] = { int(px), int(py), int(pw), int(ph) };
    }

    // окно "ARE YOU SURE?" перед удалением репозитория: поверх всего остального
    Confirm_Draw(r, w, h, nd, ndCount);
}

MainResult Main_Handle(const SDL_Event& e) {
    if (e.type == SDL_EVENT_MOUSE_MOTION) { g_mx = e.motion.x; g_my = e.motion.y; }

    // окно "+" модальное: события только ему
    if (g_add) {
        g_avHover = false;
        g_plus.hovered = false;

        if (TextButton_Handle(g_import, e)) { g_add = false; return MainResult::Import; }
        if (TextButton_Handle(g_create, e)) { g_add = false; return MainResult::Create; }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT &&
            !InAddPanel(e.button.x, e.button.y))
            g_add = false;                                                // клик мимо закрывает

        if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_ESCAPE)
            g_add = false;

        if (!g_add) g_import.hovered = g_create.hovered = false;
        return MainResult::None;
    }

    if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) { g_mx = g_my = -1.f; }

    // окно подтверждения удаления: пока открыто, все события идут только ему
    if (Confirm_IsOpen()) {
        g_avHover = false;
        g_plus.hovered = false;
        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) g_drag = false;

        const ConfirmResult cr = Confirm_Handle(e);
        if (cr != ConfirmResult::None) {                      // Yes или No: окно уже закрылось
            if (cr == ConfirmResult::Yes && !g_delDir.empty()) {
                RepoStore_Delete(g_delDir);
                const int keep = g_scrollRow;                 // после удаления остаёмся на том же месте списка
                LoadRepos();
                g_scrollRow = keep;
                ClampScroll();
            }
            g_delDir.clear();
        }
        return MainResult::None;
    }

    // меню карточки: пока открыто, первым получает клики и Esc
    if (g_cmIdx >= 0) {
        if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_ESCAPE) {
            CloseCardMenu();
            return MainResult::None;
        }
        if (g_cmLaid && g_cmIdx < int(g_repos.size())) {
            const std::string dir = g_repos[size_t(g_cmIdx)].dir;
            if (TextButton_Handle(g_cmDelete, e)) {
                CloseCardMenu();
                g_delDir = dir;                                  // удалим только после "YES" в окне подтверждения
                Confirm_Open();
                return MainResult::None;
            }
            if (TextButton_Handle(g_cmEdit, e)) {
                CloseCardMenu();
                g_chosenDir = dir;
                return MainResult::Edit;
            }
            if (TextButton_Handle(g_cmOpen, e)) {
                CloseCardMenu();
                g_chosenDir = dir;
                return MainResult::Open;                         // пока main.cpp ничего с этим не делает
            }
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            if (InsideF(g_cmRect, e.button.x, e.button.y)) return MainResult::None;   // рамка между пунктами
            CloseCardMenu();      // клик мимо: закрыть, а сам клик пойдёт дальше (можно сразу открыть другую карточку)
        }
        if (e.type == SDL_EVENT_MOUSE_WHEEL) CloseCardMenu();    // меню привязано к экрану, а карточки поедут
    }

    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        g_avHover = InAvatar(e.motion.x, e.motion.y);
    }
    if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE)
        g_avHover = false;

    if (g_open) {
        if (TextButton_Handle(g_profile, e)) {           // пункт Profile
            g_open = false;
            return MainResult::Profile;
        }
        if (TextButton_Handle(g_logout, e)) {            // пункт Logout
            g_open = false;
            return MainResult::Logout;
        }
    }
    else {
        g_profile.hovered = g_logout.hovered = false;
    }

    // ползунок сетки: перетаскивание, клик по дорожке, колесо мыши (пока список аватара закрыт)
    if (!g_open && g_hasBar) {
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            const SDL_FRect hit = { g_track.x - 6.f, g_track.y, g_track.w + 12.f, g_track.h };
            if (InsideF(hit, e.button.x, e.button.y)) {
                g_drag = true;
                g_dragOff = InsideF({ hit.x, g_thumb.y, hit.w, g_thumb.h }, e.button.x, e.button.y)
                          ? e.button.y - g_thumb.y            // схватили сам ползунок
                          : g_thumb.h * 0.5f;                 // клик по дорожке: ползунок прыгает под курсор
                ScrollFromMouseY(e.button.y);
                return MainResult::None;
            }
        }
        if (e.type == SDL_EVENT_MOUSE_MOTION && g_drag) ScrollFromMouseY(e.motion.y);
        if (e.type == SDL_EVENT_MOUSE_WHEEL && e.wheel.y != 0.f &&
            (InsideF(g_gridRect, g_mx, g_my) || InsideF(g_track, g_mx, g_my))) {
            g_scrollRow += (e.wheel.y > 0.f) ? -1 : 1;           // колесо вверх = ряд выше
            ClampScroll();
        }
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) g_drag = false;

    // "+" не реагирует, пока открыт список аватара (он перекрывает кнопку) или если уехал за край при прокрутке
    if (!g_open && g_plusVisible) {
        if (TextButton_Handle(g_plus, e)) {
            g_add = true;                                   // открыть окно IMPORT / CREATE
            g_import.hovered = g_create.hovered = false;
        }
    }
    else {
        g_plus.hovered = false;
    }

    // ЛКМ по карточке репозитория: открыть меню DELETE / EDIT / OPEN (не при открытом списке аватара)
    if (!g_open && !g_drag && e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        for (const VisCard& v : g_vis) {
            if (!InsideF(v.rc, e.button.x, e.button.y)) continue;
            if (v.slot >= int(g_repos.size())) break;
            g_cmIdx = v.slot;
            g_cmX = e.button.x;                                  // меню появляется у курсора
            g_cmY = e.button.y;
            g_cmLaid = false;                                    // позиции пунктов появятся после ближайшей отрисовки
            g_cmDelete.hovered = g_cmEdit.hovered = g_cmOpen.hovered = false;
            break;
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        if (InAvatar(e.button.x, e.button.y)) g_open = !g_open;          // клик по аватару
        else if (g_open && !InMenu(e.button.x, e.button.y)) g_open = false;   // клик мимо закрывает
    }

    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_ESCAPE)
        g_open = false;

    return MainResult::None;
}

bool Main_IsModalOpen() {
    return g_add || Confirm_IsOpen();
}

void Main_Destroy() {
    TextButton_Destroy(g_panel);
    TextButton_Destroy(g_profile);
    TextButton_Destroy(g_logout);
    TextButton_Destroy(g_plus);
    TextButton_Destroy(g_card);
    TextButton_Destroy(g_cmDelete);
    TextButton_Destroy(g_cmEdit);
    TextButton_Destroy(g_cmOpen);
    Confirm_Destroy();
    FreeRepos();
    TextButton_Destroy(g_addPanel);
    TextButton_Destroy(g_import);
    TextButton_Destroy(g_create);
}
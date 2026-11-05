#include "MainScreen.h"
#include "Avatar.h"
#include "TextButton.h"
#include "Text.h"
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

    // кнопка "+": рамку рисует TextButton, сам плюс рисуется отдельно
    const float kPlusX = 40.f, kPlusY = 128.f;
    TextButton g_plus{ 0, 0, 80, 80, "" };

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
    void DrawLeftText(SDL_Renderer* r, std::string s, float x, float cy, float maxW) {
        const float cs = float(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
        float scale = 2.0f;
        if (s.size() * cs * scale > maxW) scale = 1.0f;
        const size_t fit = size_t(std::max(1.f, maxW / (cs * scale)));
        if (s.size() > fit) s = s.substr(0, fit > 2 ? fit - 2 : fit) + (fit > 2 ? ".." : "");

        SDL_SetRenderScale(r, scale, scale);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, kR, kG, kB, 45);                // мягкое свечение, как у остальных надписей
        const float tx = x / scale, ty = (cy - cs * scale * 0.5f) / scale, o = 0.6f;
        SDL_RenderDebugText(r, tx - o, ty, s.c_str());
        SDL_RenderDebugText(r, tx + o, ty, s.c_str());
        SDL_RenderDebugText(r, tx, ty - o, s.c_str());
        SDL_RenderDebugText(r, tx, ty + o, s.c_str());
        SDL_SetRenderDrawColor(r, kR, kG, kB, 255);
        SDL_RenderDebugText(r, tx, ty, s.c_str());
        SDL_SetRenderScale(r, 1.0f, 1.0f);
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
    void DrawCenterLabel(SDL_Renderer* r, const char* s, float cx, float cy, float scale, bool hovered) {
        const float cs = float(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
        const float textW = float(SDL_strlen(s)) * cs * scale;
        const Uint8 R = hovered ? 200 : kR, G = hovered ? 240 : kG, B = hovered ? 255 : kB;

        SDL_SetRenderScale(r, scale, scale);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        const float tx = (cx - textW * 0.5f) / scale, ty = (cy - cs * scale * 0.5f) / scale, o = 0.6f;
        SDL_SetRenderDrawColor(r, R, G, B, 45);
        SDL_RenderDebugText(r, tx - o, ty, s);
        SDL_RenderDebugText(r, tx + o, ty, s);
        SDL_RenderDebugText(r, tx, ty - o, s);
        SDL_RenderDebugText(r, tx, ty + o, s);
        SDL_SetRenderDrawColor(r, R, G, B, 255);
        SDL_RenderDebugText(r, tx, ty, s);
        SDL_SetRenderScale(r, 1.0f, 1.0f);
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
    g_avHover = false;
    g_profile.hovered = g_logout.hovered = g_plus.hovered = false;
    g_import.hovered = g_create.hovered = false;
}

void Main_Close() {
    g_login.clear();
    g_open = false;
    g_add = false;
}

void Main_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount) {
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

    // кнопка "+" (под ней потом можно разместить что угодно; список аватара рисуется поверх)
    g_plus.x = kPlusX;
    g_plus.y = kPlusY;
    TextButton_Draw(r, g_plus);
    DrawPlus(r, g_plus);
    nd[ndCount++] = TextButton_Rect(g_plus);

    // выпадающий список
    if (g_open) {
        g_panel.x = kAvX;
        g_panel.y = kAvY + kAvD + 10.f;
        g_panel.hovered = false;

        // фон списка (с отступом, чтобы не торчал за скруглёнными углами рамки)
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 22, 24, 30, 255);
        const SDL_FRect bg = { g_panel.x + 4, g_panel.y + 4, g_panel.w - 8, g_panel.h - 8 };
        SDL_RenderFillRect(r, &bg);
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
        DrawCenterLabel(r, "IMPORT", g_import.x + S * 0.5f, labelCy, labelScale, g_import.hovered);
        DrawCenterLabel(r, "CREATE", g_create.x + S * 0.5f, labelCy, labelScale, g_create.hovered);

        // всё окно без перетаскивания
        nd[ndCount++] = { int(px), int(py), int(pw), int(ph) };
    }
}

MainResult Main_Handle(const SDL_Event& e) {
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

    if (e.type == SDL_EVENT_MOUSE_MOTION)
        g_avHover = InAvatar(e.motion.x, e.motion.y);
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

    // "+" не реагирует, пока открыт список аватара (он перекрывает кнопку)
    if (!g_open) {
        if (TextButton_Handle(g_plus, e)) {
            g_add = true;                                   // открыть окно IMPORT / CREATE
            g_import.hovered = g_create.hovered = false;
        }
    }
    else {
        g_plus.hovered = false;
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        if (InAvatar(e.button.x, e.button.y)) g_open = !g_open;          // клик по аватару
        else if (g_open && !InMenu(e.button.x, e.button.y)) g_open = false;   // клик мимо закрывает
    }

    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_ESCAPE)
        g_open = false;

    return MainResult::None;
}

void Main_Destroy() {
    TextButton_Destroy(g_panel);
    TextButton_Destroy(g_profile);
    TextButton_Destroy(g_logout);
    TextButton_Destroy(g_plus);
    TextButton_Destroy(g_addPanel);
    TextButton_Destroy(g_import);
    TextButton_Destroy(g_create);
}
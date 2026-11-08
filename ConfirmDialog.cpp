#include "ConfirmDialog.h"
#include "TextButton.h"
#include "Text.h"
#include "Localization.h"
#include "Theme.h"
#include <cmath>
#include <string>
#include <algorithm>

namespace {
    const float kPanelW = 440.f, kPanelH = 250.f;
    const Uint8 &kR = Theme::Cyan.r, &kG = Theme::Cyan.g, &kB = Theme::Cyan.b;

    bool g_open = false;
    TextButton g_panel{ 0, 0, kPanelW, kPanelH, "" };   // неоновая рамка панели
    TextButton g_no{ 0, 0, 150, 40, "NO" };
    TextButton g_yes{ 0, 0, 150, 40, "YES" };

    SDL_Texture* g_sprite = nullptr;
    SDL_Texture* g_qmark = nullptr;    // символ "?" из Press Start 2P, 16x16 пикселей
    SDL_Renderer* g_owner = nullptr;
    bool          g_spriteTried = false;
    float         g_aspect = 1.4f;

    // псевдослучайное число -1..1 из целого
    float Rand(Uint64 value) {
        Uint32 n = static_cast<Uint32>(value ^ (value >> 32));
        n ^= n >> 16; n *= 0x7feb352dU;
        n ^= n >> 15; n *= 0x846ca68bU;
        n ^= n >> 16;
        return (n & 0xFFFF) / 65535.f * 2.f - 1.f;
    }

    void EnsureResources(SDL_Renderer* r) {
        if (g_owner != r) {                       // новый рендерер: всё пересоздать
            if (g_sprite) SDL_DestroyTexture(g_sprite);
            if (g_qmark)  SDL_DestroyTexture(g_qmark);
            g_sprite = g_qmark = nullptr;
            g_spriteTried = false;
            g_owner = r;
        }

        if (!g_qmark) {
            g_qmark = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, 16, 16);
            SDL_SetTextureScaleMode(g_qmark, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(g_qmark, SDL_BLENDMODE_BLEND);

            SDL_Texture* prev = SDL_GetRenderTarget(r);
            SDL_SetRenderTarget(r, g_qmark);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
            SDL_RenderClear(r);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            // Берём вопросительный знак из того же Press Start 2P, что и остальной интерфейс.
            DrawUtf8Text(r, "?", 0.f, 0.f, 16, 255, 255, 255, 255);
            SDL_SetRenderTarget(r, prev);
        }

        if (!g_sprite && !g_spriteTried) {
            g_spriteTried = true;
            const char* base = SDL_GetBasePath();
            const std::string paths[2] = { std::string(base ? base : "") + "sprite.bmp",
                                           "sprite.bmp" };   // второй вариант: рабочая папка
            for (const std::string& path : paths) {
                if (SDL_Surface* s = SDL_LoadBMP(path.c_str())) {
                    g_sprite = SDL_CreateTextureFromSurface(r, s);
                    g_aspect = float(s->w) / float(s->h);
                    SDL_DestroySurface(s);
                    SDL_Log("sprite loaded: %s", path.c_str());
                    break;
                }
                SDL_Log("sprite NOT loaded from '%s': %s", path.c_str(), SDL_GetError());
            }
        }
    }

    // глючный знак вопроса: RGB-расслоение + периодические всплески
    void DrawGlitchQ(SDL_Renderer* r, float x, float y, float S) {
        const Uint64 now = SDL_GetTicks();
        const Uint64 step = now / 55;                       // глитч меняется ~18 раз/сек
        const Uint64 phase = now % 1700;
        const bool burst = phase < 240 || (phase > 900 && phase < 980);

        const float split = (burst ? 7.f : 2.f) + 1.5f * std::fabs(Rand(step * 3 + 1));
        const float jy = Rand(step * 5 + 2) * (burst ? 4.f : 1.f);

        // смещение каждой строки (только во время всплеска)
        float rowShift[16];
        for (int i = 0; i < 16; ++i)
            rowShift[i] = (burst && Rand(step * 7 + i) > 0.1f) ? Rand(step * 13 + i) * S * 1.3f : 0.f;

        auto layer = [&](Uint8 cr, Uint8 cg, Uint8 cb, Uint8 a, float dx, float dy) {
            SDL_SetTextureColorMod(g_qmark, cr, cg, cb);
            SDL_SetTextureAlphaMod(g_qmark, a);
            for (int i = 0; i < 16; ++i) {
                const SDL_FRect src = { 0.f, float(i), 16.f, 1.f };
                const SDL_FRect dst = { std::round(x + dx + rowShift[i]),
                                        std::round(y + dy + i * S), 16 * S, S };
                SDL_RenderTexture(r, g_qmark, &src, &dst);
            }
            };

        // цветные копии (сложением цветов)
        SDL_SetTextureBlendMode(g_qmark, SDL_BLENDMODE_ADD);
        layer(255, 40, 90, 200, -split, jy);
        layer(Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 200, split, -jy);

        // основной знак; во время всплеска иногда мерцает
        SDL_SetTextureBlendMode(g_qmark, SDL_BLENDMODE_BLEND);
        const Uint8 a = (burst && Rand(step * 11) > 0.55f) ? 110 : 255;
        layer(235, 250, 255, a, 0.f, 0.f);

        // полоса помех
        if (burst) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
            SDL_SetRenderDrawColor(r, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 120);
            const SDL_FRect bar = { x - 10, y + (Rand(step * 17) * 0.5f + 0.5f) * 16 * S, 16 * S + 20, 3 };
            SDL_RenderFillRect(r, &bar);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        }
    }
}

void Confirm_Open() {
    g_open = true;
    g_no.hovered = g_yes.hovered = false;
}

void Confirm_Close() { g_open = false; }
bool Confirm_IsOpen() { return g_open; }

void Confirm_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount) {
    if (!g_open) return;
    EnsureResources(r);

    // затемнение всего окна
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 175);
    SDL_RenderFillRect(r, nullptr);

    const float px = std::floor((w - kPanelW) * 0.5f);
    const float py = std::floor((h - kPanelH) * 0.5f);

    // фон панели (с отступом, чтобы не торчал за скруглёнными углами рамки)
    SDL_SetRenderDrawColor(r, Theme::Panel.r, Theme::Panel.g, Theme::Panel.b, 255);
    const SDL_FRect bg = { px + 4, py + 4, kPanelW - 8, kPanelH - 8 };
    Theme::StyledFill(r, &bg);

    g_panel.x = px; g_panel.y = py; g_panel.hovered = false;
    TextButton_Draw(r, g_panel);

    g_no.label = UiText("NO", "НЕТ");
    g_yes.label = UiText("YES", "ДА");
    DrawCenteredText(r, UiText("ARE YOU SURE?", "ВЫ УВЕРЕНЫ?"), w, h, 3.0f, py + 34.f - h * 0.5f);

    // средняя строка: спрайт + глючный знак вопроса
    const float rowTop = py + 70.f;
    const float sh = 100.f;
    const float sw = std::min(sh * g_aspect, 180.f);
    const float S = 5.5f;                      // 16 x 16 пиксельный глиф Press Start 2P
    const float qW = 16 * S;
    const float gap = 28.f;
    const float x0 = px + (kPanelW - (sw + gap + qW)) * 0.5f;

    const SDL_FRect sprite = { x0, rowTop, sw, sh };
    if (g_sprite) {
        SDL_RenderTexture(r, g_sprite, nullptr, &sprite);
    }
    else {                                       // нет sprite.bmp: заглушка
        SDL_SetRenderDrawColor(r, 40, 40, 50, 255);
        SDL_RenderFillRect(r, &sprite);
    }
    for (int i = 3; i >= 1; --i) {               // неоновая рамка вокруг спрайта
        SDL_SetRenderDrawColor(r, kR, kG, kB, Uint8(60 - i * 14));
        const SDL_FRect f = { sprite.x - i, sprite.y - i, sprite.w + 2 * i, sprite.h + 2 * i };
        Theme::StyledStroke(r, &f);
    }
    SDL_SetRenderDrawColor(r, kR, kG, kB, 255);
    Theme::StyledStroke(r, &sprite);

    DrawGlitchQ(r, x0 + sw + gap, rowTop + (sh - 16 * S) * 0.5f, S);

    // кнопки
    const float btnGap = 20.f;
    const float bx = px + (kPanelW - (g_no.w + btnGap + g_yes.w)) * 0.5f;
    const float by = py + kPanelH - g_no.h - 24.f;
    g_no.x = bx;                       g_no.y = by;
    g_yes.x = bx + g_no.w + btnGap;    g_yes.y = by;
    TextButton_Draw(r, g_no);
    TextButton_Draw(r, g_yes);

    // вся панель без перетаскивания окна
    nd[ndCount++] = { int(px), int(py), int(kPanelW), int(kPanelH) };
}

ConfirmResult Confirm_Handle(const SDL_Event& e) {
    if (!g_open) return ConfirmResult::None;

    if (TextButton_Handle(g_no, e)) { g_open = false; return ConfirmResult::No; }
    if (TextButton_Handle(g_yes, e)) { g_open = false; return ConfirmResult::Yes; }

    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
        if (e.key.key == SDLK_ESCAPE) { g_open = false; return ConfirmResult::No; }
        if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) {
            g_open = false;
            return ConfirmResult::Yes;
        }
    }
    return ConfirmResult::None;
}

void Confirm_Destroy() {
    if (g_sprite) SDL_DestroyTexture(g_sprite);
    if (g_qmark)  SDL_DestroyTexture(g_qmark);
    g_sprite = g_qmark = nullptr;
    g_owner = nullptr;
    TextButton_Destroy(g_panel);
    TextButton_Destroy(g_no);
    TextButton_Destroy(g_yes);
}
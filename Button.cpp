#include "Button.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace {
    const int   kPad = 10;                    // запас под свечение
    const Uint8 kR = 90, kG = 200, kB = 255;  // голубой
    const float kGlowStrength = 0.30f;        // яркость свечения (0..1)
    const float kGlowFalloff = 2.6f;         // ширина свечения
    const float kAnimTime = 0.25f;        // длительность превращения, сек

    Uint32 Pack(Uint8 a) { return (Uint32(a) << 24) | (Uint32(kB) << 16) | (Uint32(kG) << 8) | kR; }

    float Thickness(float s) { return std::max(1.5f, s * 0.08f); }

    // одна горизонтальная полоска-капсула; из неё собирается вся иконка
    void Rebuild(SDL_Renderer* r, MenuButton& b) {
        if (b.core) SDL_DestroyTexture(b.core);
        if (b.glow) SDL_DestroyTexture(b.glow);

        const float s = b.size;
        const float t = Thickness(s);
        const float half = (s - t) * 0.5f;

        const int tw = int(std::ceil(s)) + kPad * 2;
        const int th = int(std::ceil(t)) + kPad * 2;
        const float cx = tw * 0.5f, cy = th * 0.5f;   // центр полоски = центр текстуры

        std::vector<Uint32> coreBuf(size_t(tw) * th, 0), glowBuf(size_t(tw) * th, 0);

        for (int py = 0; py < th; ++py) {
            for (int px = 0; px < tw; ++px) {
                const float dx = std::max(std::fabs(px + 0.5f - cx) - half, 0.f);
                const float dy = py + 0.5f - cy;
                const float dist = std::sqrt(dx * dx + dy * dy) - t * 0.5f;

                const float c = std::clamp(0.5f - dist, 0.f, 1.f);
                if (c > 0.f) coreBuf[size_t(py) * tw + px] = Pack(Uint8(c * 255));

                if (dist > 0.f) {
                    const float g = kGlowStrength * std::exp(-dist / kGlowFalloff);
                    if (g > 0.004f) glowBuf[size_t(py) * tw + px] = Pack(Uint8(g * 255));
                }
            }
        }

        auto make = [&](std::vector<Uint32>& buf) {
            SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, tw, th);
            SDL_UpdateTexture(tex, nullptr, buf.data(), tw * 4);
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
            return tex;
            };
        b.core = make(coreBuf);
        b.glow = make(glowBuf);
        b.owner = r;
        b.builtSize = s;
    }

    void DrawBar(SDL_Renderer* r, MenuButton& b, float cx, float cy, double angle, float alpha) {
        if (alpha <= 0.f) return;

        float tw, th;
        SDL_GetTextureSize(b.core, &tw, &th);
        const SDL_FRect dst = { cx - tw * 0.5f, cy - th * 0.5f, tw, th };

        const Uint8 glowA = Uint8((b.hovered ? 255 : 190) * alpha);
        SDL_SetTextureAlphaMod(b.glow, glowA);
        SDL_RenderTextureRotated(r, b.glow, nullptr, &dst, angle, nullptr, SDL_FLIP_NONE);
        if (b.hovered)   // второй проход усиливает неон при наведении
            SDL_RenderTextureRotated(r, b.glow, nullptr, &dst, angle, nullptr, SDL_FLIP_NONE);

        SDL_SetTextureAlphaMod(b.core, Uint8(255 * alpha));
        SDL_RenderTextureRotated(r, b.core, nullptr, &dst, angle, nullptr, SDL_FLIP_NONE);
    }
}

void MenuButton_Draw(SDL_Renderer* r, MenuButton& b) {
    if (!b.core || b.owner != r || b.builtSize != b.size) Rebuild(r, b);

    // продвигаем анимацию по реальному времени
    const Uint64 now = SDL_GetTicks();
    if (b.lastTick == 0) b.lastTick = now;
    const float dt = std::min((now - b.lastTick) / 1000.0f, 0.05f);
    b.lastTick = now;

    const float target = b.open ? 1.f : 0.f;
    const float step = dt / kAnimTime;
    if (b.anim < target) b.anim = std::min(b.anim + step, target);
    else if (b.anim > target) b.anim = std::max(b.anim - step, target);

    const float a = b.anim;
    const float e = a * a * (3.f - 2.f * a);         // плавный старт и финиш

    const float t = Thickness(b.size);
    const float h = MenuButtonHeight(b.size);
    const float cx = b.x + b.size * 0.5f;
    const float midY = b.y + h * 0.5f;
    const float topY = b.y + t * 0.5f;
    const float botY = b.y + h - t * 0.5f;

    // верх и низ стягиваются к центру и поворачиваются, середина гаснет
    DrawBar(r, b, cx, topY + (midY - topY) * e, 45.0 * e, 1.f);
    DrawBar(r, b, cx, midY, 0.0, 1.f - e);
    DrawBar(r, b, cx, botY + (midY - botY) * e, -45.0 * e, 1.f);
}

static bool Inside(const MenuButton& b, float mx, float my) {
    const float m = 6.f; // область нажатия чуть больше иконки
    return mx >= b.x - m && mx <= b.x + b.size + m &&
        my >= b.y - m && my <= b.y + MenuButtonHeight(b.size) + m;
}

bool MenuButton_Handle(MenuButton& b, const SDL_Event& e) {
    if (e.type == SDL_EVENT_MOUSE_MOTION)
        b.hovered = Inside(b, e.motion.x, e.motion.y);
    if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE)
        b.hovered = false;
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
        return Inside(b, e.button.x, e.button.y);
    return false;
}

void MenuButton_Destroy(MenuButton& b) {
    if (b.core) SDL_DestroyTexture(b.core);
    if (b.glow) SDL_DestroyTexture(b.glow);
    b.core = b.glow = nullptr;
    b.owner = nullptr;
}
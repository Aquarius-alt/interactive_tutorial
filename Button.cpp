#include "Button.h"
#include "Theme.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace {
    const int   kPad = 10;                    // запас под свечение
    const Uint8 &kR = Theme::Cyan.r, &kG = Theme::Cyan.g, &kB = Theme::Cyan.b;  // голубой
    const float kGlowStrength = 0.30f;        // яркость свечения (0..1)
    const float kGlowFalloff = 2.6f;         // ширина свечения
    const float kAnimTime = 0.25f;        // длительность поворота, сек
    const float kPi = 3.14159265f;
    const int   kTeeth = 8;                   // зубцов у шестерни
    const double kTurnDeg = 90.0;             // на сколько доворачивается шестерня при открытии меню

    Uint32 Pack(Uint8 a) { return (Uint32(a) << 24) | 0x00FFFFFFu; }   // белая маска; цвет задаётся SDL_SetTextureColorMod (меняется со стилем)

    float Thickness(float s) { return std::max(1.5f, s * 0.08f); }

    struct Seg { float x1, y1, x2, y2; };

    float DistToSeg(float px, float py, const Seg& s) {
        const float dx = s.x2 - s.x1, dy = s.y2 - s.y1;
        const float l2 = dx * dx + dy * dy;
        float t = 0.f;
        if (l2 > 0.f) t = std::clamp(((px - s.x1) * dx + (py - s.y1) * dy) / l2, 0.f, 1.f);
        const float ox = px - (s.x1 + t * dx), oy = py - (s.y1 + t * dy);
        return std::sqrt(ox * ox + oy * oy);
    }

    // контур шестерни (зубцы) ломаной вокруг точки (cx, cy)
    void GearOutline(std::vector<Seg>& out, float cx, float cy, float Ro) {
        const float Rr = Ro * 0.76f;                  // радиус впадин
        const float pitch = 2.f * kPi / float(kTeeth);
        std::vector<std::pair<float, float>> pts;     // (радиус, угол)
        for (int i = 0; i < kTeeth; ++i) {
            const float a = float(i) * pitch;
            pts.push_back({ Rr, a - 0.34f * pitch });
            pts.push_back({ Ro, a - 0.19f * pitch });
            pts.push_back({ Ro, a });                  // середина вершины зубца (дуга)
            pts.push_back({ Ro, a + 0.19f * pitch });
            pts.push_back({ Rr, a + 0.34f * pitch });
            pts.push_back({ Rr, a + 0.50f * pitch });  // середина впадины (дуга)
        }
        for (size_t i = 0; i < pts.size(); ++i) {
            const auto& A = pts[i];
            const auto& B = pts[(i + 1) % pts.size()];
            out.push_back({ cx + A.first * std::cos(A.second), cy + A.first * std::sin(A.second),
                            cx + B.first * std::cos(B.second), cy + B.first * std::sin(B.second) });
        }
    }

    // две текстуры: чёткое ядро и мягкое свечение, как у остальных неоновых иконок
    void Rebuild(SDL_Renderer* r, MenuButton& b) {
        if (b.core) SDL_DestroyTexture(b.core);
        if (b.glow) SDL_DestroyTexture(b.glow);

        const float s = b.size;
        const float t = std::max(1.8f, s * 0.075f);     // толщина линии
        const int tw = int(std::ceil(s)) + kPad * 2;
        const int th = tw;
        const float cx = tw * 0.5f, cy = th * 0.5f;
        const float Ro = s * 0.5f - t * 0.5f - 0.5f;    // внешний радиус зубцов
        const float Rin = Ro * 0.36f;                   // радиус отверстия

        std::vector<Seg> segs;
        GearOutline(segs, cx, cy, Ro);

        std::vector<Uint32> coreBuf(size_t(tw) * th, 0), glowBuf(size_t(tw) * th, 0);
        for (int py = 0; py < th; ++py) {
            for (int px = 0; px < tw; ++px) {
                const float x = px + 0.5f, y = py + 0.5f;
                float d = 1000.f;
                for (const Seg& sg : segs) d = std::min(d, DistToSeg(x, y, sg));
                const float ox = x - cx, oy = y - cy;
                d = std::min(d, std::fabs(std::sqrt(ox * ox + oy * oy) - Rin));   // отверстие в центре

                const float dist = d - t * 0.5f;        // расстояние от края линии
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

    float tw, th;
    SDL_GetTextureSize(b.core, &tw, &th);
    const float cx = b.x + b.size * 0.5f, cy = b.y + b.size * 0.5f;
    const SDL_FRect dst = { cx - tw * 0.5f, cy - th * 0.5f, tw, th };
    const double angle = kTurnDeg * e;                // шестерня поворачивается при открытии / закрытии меню

    SDL_SetTextureColorMod(b.glow, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b);
    SDL_SetTextureColorMod(b.core, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b);
    SDL_SetTextureAlphaMod(b.glow, Uint8(b.hovered ? 255 : 190));
    SDL_RenderTextureRotated(r, b.glow, nullptr, &dst, angle, nullptr, SDL_FLIP_NONE);
    if (b.hovered)   // второй проход усиливает неон при наведении
        SDL_RenderTextureRotated(r, b.glow, nullptr, &dst, angle, nullptr, SDL_FLIP_NONE);

    SDL_SetTextureAlphaMod(b.core, 255);
    SDL_RenderTextureRotated(r, b.core, nullptr, &dst, angle, nullptr, SDL_FLIP_NONE);
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
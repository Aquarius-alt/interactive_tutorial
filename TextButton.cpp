#include "TextButton.h"
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace {
    const int   kPad = 12;
    const Uint8 kR = 90, kG = 200, kB = 255;
    const float kGlowStrength = 0.30f;
    const float kGlowFalloff = 3.0f;
    const float kLine = 1.5f;      // толщина контура
    const float kRound = 10.f;     // скругление углов
    const float kTextScale = 2.0f;

    Uint32 Pack(Uint8 a) { return (Uint32(a) << 24) | (Uint32(kB) << 16) | (Uint32(kG) << 8) | kR; }

    void Rebuild(SDL_Renderer* r, TextButton& b) {
        if (b.core) SDL_DestroyTexture(b.core);
        if (b.glow) SDL_DestroyTexture(b.glow);

        const int tw = int(std::ceil(b.w)) + kPad * 2;
        const int th = int(std::ceil(b.h)) + kPad * 2;
        const float cx = tw * 0.5f, cy = th * 0.5f;
        const float rad = std::min(kRound, std::min(b.w, b.h) * 0.5f);
        const float bx = b.w * 0.5f - rad, by = b.h * 0.5f - rad;

        std::vector<Uint32> coreBuf(size_t(tw) * th, 0), glowBuf(size_t(tw) * th, 0);

        for (int py = 0; py < th; ++py) {
            for (int px = 0; px < tw; ++px) {
                const float qx = std::fabs(px + 0.5f - cx) - bx;
                const float qy = std::fabs(py + 0.5f - cy) - by;
                const float ox = std::max(qx, 0.f), oy = std::max(qy, 0.f);
                const float d = std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.f) - rad;

                // контур
                const float c = std::clamp(0.5f - (std::fabs(d) - kLine * 0.5f), 0.f, 1.f);
                if (c > 0.f) coreBuf[size_t(py) * tw + px] = Pack(Uint8(c * 255));

                // свечение: снаружи сильнее, внутрь слабее
                const float k = d > 0.f ? 1.f : 0.5f;
                const float g = kGlowStrength * k * std::exp(-std::fabs(d) / kGlowFalloff);
                if (g > 0.004f) glowBuf[size_t(py) * tw + px] = Pack(Uint8(g * 255));
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
        b.bw = b.w;
        b.bh = b.h;
    }
}

void TextButton_Draw(SDL_Renderer* r, TextButton& b) {
    if (!b.core || b.owner != r || b.bw != b.w || b.bh != b.h) Rebuild(r, b);

    const SDL_FRect dst = { b.x - kPad, b.y - kPad, std::ceil(b.w) + kPad * 2, std::ceil(b.h) + kPad * 2 };

    SDL_SetTextureAlphaMod(b.glow, b.hovered ? 255 : 190);
    SDL_RenderTexture(r, b.glow, nullptr, &dst);
    if (b.hovered) SDL_RenderTexture(r, b.glow, nullptr, &dst);
    SDL_RenderTexture(r, b.core, nullptr, &dst);

    // подпись по центру кнопки
    const float cs = float(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
    const float textW = std::strlen(b.label) * cs * kTextScale;
    const float textH = cs * kTextScale;
    const float tx = (b.x + (b.w - textW) * 0.5f) / kTextScale;
    const float ty = (b.y + (b.h - textH) * 0.5f) / kTextScale;

    SDL_SetRenderScale(r, kTextScale, kTextScale);
    if (b.hovered) SDL_SetRenderDrawColor(r, 200, 240, 255, 255);   // при наведении текст светлее
    else           SDL_SetRenderDrawColor(r, kR, kG, kB, 255);
    SDL_RenderDebugText(r, tx, ty, b.label);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
}

static bool Inside(const TextButton& b, float mx, float my) {
    return mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h;
}

bool TextButton_Handle(TextButton& b, const SDL_Event& e) {
    if (e.type == SDL_EVENT_MOUSE_MOTION)
        b.hovered = Inside(b, e.motion.x, e.motion.y);
    if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE)
        b.hovered = false;
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
        return Inside(b, e.button.x, e.button.y);
    return false;
}

SDL_Rect TextButton_Rect(const TextButton& b) {
    return { int(b.x), int(b.y), int(b.w), int(b.h) };
}

void TextButton_Destroy(TextButton& b) {
    if (b.core) SDL_DestroyTexture(b.core);
    if (b.glow) SDL_DestroyTexture(b.glow);
    b.core = b.glow = nullptr;
    b.owner = nullptr;
}
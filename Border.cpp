#include "Border.h"
#include <SDL3/SDL.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace {
    struct BandPx {
        int   idx;    // индекс пикселя в буфере
        float d;      // глубина внутрь от края окна
        float phase;  // фаза вдоль периметра, считается один раз
    };

    const float kWaveAmp = 0.12f; // высота волны как доля от glow (0 = ровная граница)

    SDL_Texture* g_tex = nullptr;
    SDL_Renderer* g_texRenderer = nullptr;
    int g_w = 0, g_h = 0;

    std::vector<Uint32> g_buf;
    std::vector<BandPx> g_band;

    const float kPi = 3.14159265f;

    void Rebuild(SDL_Renderer* r, int w, int h, int radius, int thickness, int glow,
        float waveLength,
        unsigned char red, unsigned char green, unsigned char blue) {
        if (g_tex) SDL_DestroyTexture(g_tex);

        g_buf.assign(static_cast<size_t>(w) * h, 0);
        g_band.clear();

        const float rad = static_cast<float>(radius);
        const float hw = static_cast<float>(w) * 0.5f, hh = static_cast<float>(h) * 0.5f;
        const float ax = std::max(hw - rad, 0.f), ay = std::max(hh - rad, 0.f);
        const float L = kPi * 0.5f * rad;                 // длина дуги угла
        const float total = 4 * ax + 4 * ay + 4 * L;      // периметр
        const int   n = std::max(1, static_cast<int>(std::lround(total / waveLength)));
        const int   band = thickness + glow + radius + 1;

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (x > band && y > band && x < w - band && y < h - band) continue;

                const float cx = static_cast<float>(x) + 0.5f - hw, cy = static_cast<float>(y) + 0.5f - hh;
                const float acx = std::fabs(cx), acy = std::fabs(cy);

                // глубина внутрь от контура
                const float qx = acx - ax, qy = acy - ay;
                const float outside = std::sqrt(std::max(qx, 0.f) * std::max(qx, 0.f) +
                    std::max(qy, 0.f) * std::max(qy, 0.f));
                const float inside = std::min(std::max(qx, qy), 0.f);
                const float d = -(outside + inside - rad);
                if (d < 0.f) continue;

                const int idx = y * w + x;
                if (d < thickness) {
                    g_buf[idx] = (255u << 24) | (Uint32(blue) << 16) | (Uint32(green) << 8) | red;
                    continue;
                }
                if (d >= thickness + glow) continue;

                // положение s вдоль периметра (по часовой от начала верхней стороны)
                float s;
                if (acx > ax && acy > ay) {                       // угол
                    const float vx = cx - (cx < 0 ? -ax : ax);
                    const float vy = cy - (cy < 0 ? -ay : ay);
                    const float phi = std::atan2(vy, vx);
                    if (cx > 0 && cy < 0)      s = 2 * ax + (phi + kPi * 0.5f) * rad;                   // верх-право
                    else if (cx > 0)           s = 2 * ax + L + 2 * ay + phi * rad;                     // низ-право
                    else if (cy > 0)           s = 4 * ax + 2 * L + 2 * ay + (phi - kPi * 0.5f) * rad;  // низ-лево
                    else                       s = 4 * ax + 3 * L + 4 * ay + (phi + kPi) * rad;         // верх-лево
                }
                else if (qy > qx) {                             // верх или низ
                    if (cy < 0) s = std::clamp(cx, -ax, ax) + ax;
                    else        s = 2 * ax + 2 * L + 2 * ay + (ax - std::clamp(cx, -ax, ax));
                }
                else {                                          // право или лево
                    if (cx > 0) s = 2 * ax + L + (std::clamp(cy, -ay, ay) + ay);
                    else        s = 4 * ax + 3 * L + 2 * ay + (ay - std::clamp(cy, -ay, ay));
                }

                const float phase = 2.f * kPi * n * s / total;    // целое число волн по кругу
                g_band.push_back({ idx, d, phase });
            }
        }

        g_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
        SDL_SetTextureBlendMode(g_tex, SDL_BLENDMODE_BLEND);
        g_texRenderer = r;
        g_w = w;
        g_h = h;
    }
}

void DrawBorderGlow(SDL_Renderer* r, int w, int h, int radius, int thickness,
    int glow, int glowAlpha, float waveLength, float breathSpeed,
    float crawlSpeed,
    unsigned char red, unsigned char green, unsigned char blue) {
    if (w <= 0 || h <= 0) return;
    if (!g_tex || g_texRenderer != r || g_w != w || g_h != h)
        Rebuild(r, w, h, radius, thickness, glow, waveLength, red, green, blue);

    const float time = static_cast<float>(SDL_GetTicks()) / 1000.0f;
    const float k = std::sin(2.f * kPi * breathSpeed * time);   // дыхание k(t)
    const float shift = 2.f * kPi * crawlSpeed * time;          // ползание волн
    const Uint32 rgb = (Uint32(blue) << 16) | (Uint32(green) << 8) | red;

    for (const BandPx& p : g_band) {
        const float reach = glow * ((1.f - kWaveAmp) + kWaveAmp * k * std::sin(p.phase - shift));
        const float t = (p.d - thickness) / reach;
        Uint32 a = 0;
        if (t < 1.f) {
            const float f = 1.f - t;
            a = static_cast<Uint32>(glowAlpha * f * f);
        }
        g_buf[p.idx] = (a << 24) | rgb;
    }

    SDL_UpdateTexture(g_tex, nullptr, g_buf.data(), w * 4);
    SDL_RenderTexture(r, g_tex, nullptr, nullptr);
}

void DestroyBorderCache() {
    if (g_tex) SDL_DestroyTexture(g_tex);
    g_tex = nullptr;
    g_texRenderer = nullptr;
    g_w = g_h = 0;
    g_buf.clear();
    g_band.clear();
}
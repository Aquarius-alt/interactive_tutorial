#pragma once
// CuteScene.h - фон стиля PRO CUTE: случайные обои, мягкая вуаль, боке-огоньки,
// падающие лепестки сакуры, мерцающие звёздочки и всплывающие сердечки.
//
// Всё считается как функция времени (без массивов состояния), поэтому окно можно менять в размере,
// сворачивать и разворачивать - лепестки не «прыгают».
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <random>
#include <string>
#include "CuteGfx.h"
#include "Wallpapers.h"

namespace CuteScene {

struct Col { Uint8 r, g, b; };

inline Uint32 H32(Uint32 x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
inline float H01(Uint32 x) { return float(H32(x) & 0xFFFFFFu) / 16777215.f; }
inline float Lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float Sat(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

// Обои выбираются один раз за запуск программы (при первом кадре, когда SDL уже запущен).
// Номер прошлых обоев хранится в файле wallpaper.last рядом с настройками: подряд два запуска
// с одной и той же картинкой не бывает.
inline std::string LastFilePath() {
    char* pref = SDL_GetPrefPath("myproject", "settings");
    if (!pref) return {};
    std::string p(pref);
    SDL_free(pref);
    return p.empty() ? std::string() : p + "wallpaper.last";
}
inline int ReadLastWallpaper() {
    const std::string path = LastFilePath();
    if (path.empty()) return -1;
    size_t n = 0;
    void* data = SDL_LoadFile(path.c_str(), &n);
    if (!data) return -1;
    int v = -1;
    if (n > 0 && n < 8) { std::string s(static_cast<const char*>(data), n); v = std::atoi(s.c_str()); }
    SDL_free(data);
    return (v >= 0 && v < Wallpapers::kCount) ? v : -1;
}
inline void WriteLastWallpaper(int v) {
    const std::string path = LastFilePath();
    if (path.empty()) return;
    const std::string s = std::to_string(v);
    SDL_SaveFile(path.c_str(), s.data(), s.size());
}
inline int PickWallpaper() {
    std::random_device rd;
    const Uint32 seed = Uint32(rd()) ^ Uint32(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const int last = ReadLastWallpaper();
    int pick = int(H32(seed) % Uint32(Wallpapers::kCount));
    if (pick == last && Wallpapers::kCount > 1) pick = (pick + 1 + int(H32(seed ^ 0x9E3779B9u) % Uint32(Wallpapers::kCount - 1))) % Wallpapers::kCount;
    WriteLastWallpaper(pick);
    return pick;
}
inline int g_wallpaper = -1;   // -1 = ещё не выбраны (выбор делает Wallpaper())
inline int Wallpaper() {
    if (g_wallpaper < 0) g_wallpaper = PickWallpaper();
    return g_wallpaper;
}

inline SDL_Texture* g_tex = nullptr;
inline SDL_Renderer* g_texOwner = nullptr;
inline int g_texW = 0, g_texH = 0;
inline float g_luma = 0.5f;     // средняя яркость обоев: чем светлее, тем плотнее вуаль
inline bool g_tried = false;

inline void ReleaseWallpaper() {
    if (g_tex) SDL_DestroyTexture(g_tex);
    g_tex = nullptr; g_tried = false; g_texOwner = nullptr;
}

// Освободить всё (до SDL_DestroyRenderer).
inline void Release() { ReleaseWallpaper(); CuteGfx::ReleaseTextures(); }

inline void EnsureWallpaper(SDL_Renderer* r) {
    if (g_texOwner != r) { ReleaseWallpaper(); g_texOwner = r; }
    if (g_tex || g_tried) return;
    g_tried = true;   // и при неудаче: файл не читаем каждый кадр
    SDL_Surface* s = Wallpapers::Load(Wallpaper());
    if (!s) return;
    // средняя яркость по редкой сетке
    double sum = 0.0; long n = 0;
    const Uint8* base = static_cast<const Uint8*>(s->pixels);
    for (int y = 0; y < s->h; y += 6)
        for (int x = 0; x < s->w; x += 6) {
            const Uint8* p = base + size_t(y) * size_t(s->pitch) + size_t(x) * 4u;
            sum += (0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]) / 255.0; ++n;
        }
    g_luma = n ? float(sum / double(n)) : 0.5f;
    g_tex = SDL_CreateTextureFromSurface(r, s);
    g_texW = s->w; g_texH = s->h;
    SDL_DestroySurface(s);
    if (g_tex) {
        SDL_SetTextureBlendMode(g_tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(g_tex, SDL_SCALEMODE_LINEAR);
    }
}

// Прямоугольник с вертикальным градиентом цвета и прозрачности.
inline void VGradient(SDL_Renderer* r, float x, float y, float w, float h, Col top, Uint8 aTop, Col bot, Uint8 aBot) {
    const SDL_FColor c0{ top.r / 255.f, top.g / 255.f, top.b / 255.f, aTop / 255.f };
    const SDL_FColor c1{ bot.r / 255.f, bot.g / 255.f, bot.b / 255.f, aBot / 255.f };
    const SDL_Vertex v[4] = { { { x, y }, c0, { 0.f, 0.f } }, { { x + w, y }, c0, { 0.f, 0.f } },
                              { { x + w, y + h }, c1, { 0.f, 0.f } }, { { x, y + h }, c1, { 0.f, 0.f } } };
    const int idx[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);
}
inline void HGradient(SDL_Renderer* r, float x, float y, float w, float h, Col left, Uint8 aLeft, Col right, Uint8 aRight) {
    const SDL_FColor c0{ left.r / 255.f, left.g / 255.f, left.b / 255.f, aLeft / 255.f };
    const SDL_FColor c1{ right.r / 255.f, right.g / 255.f, right.b / 255.f, aRight / 255.f };
    const SDL_Vertex v[4] = { { { x, y }, c0, { 0.f, 0.f } }, { { x + w, y }, c1, { 0.f, 0.f } },
                              { { x + w, y + h }, c1, { 0.f, 0.f } }, { { x, y + h }, c0, { 0.f, 0.f } } };
    const int idx[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);
}

// Основа фона: градиент, обои (если включены) с медленным «дыханием» камеры, вуаль для читаемости, виньетка.
inline void DrawBackdrop(SDL_Renderer* r, int w, int h, Col top, Col bottom, Col veil, bool wallpaper) {
    const float fw = float(w), fh = float(h);
    const float t = float(SDL_GetTicks()) / 1000.f;
    VGradient(r, 0.f, 0.f, fw, fh, top, 255, bottom, 255);

    if (wallpaper) {
        EnsureWallpaper(r);
        if (g_tex && g_texW > 0 && g_texH > 0) {
            const Wallpapers::Info& wp = Wallpapers::kList[Wallpaper()];
            const float cover = std::max(fw / float(g_texW), fh / float(g_texH));
            const float s = cover * (1.05f + 0.014f * std::sin(t * 0.17f));      // чуть крупнее окна, камера дышит
            const float dw = float(g_texW) * s, dh = float(g_texH) * s;
            float dx = fw * 0.5f - wp.fx * dw, dy = fh * 0.5f - wp.fy * dh;       // «лицо» ближе к центру
            dx += std::sin(t * 0.11f) * std::min(14.f, std::max(0.f, dw - fw) * 0.4f);   // плавный дрейф
            dy += std::cos(t * 0.09f) * std::min(10.f, std::max(0.f, dh - fh) * 0.4f);
            dx = std::min(0.f, std::max(fw - dw, dx));                            // всегда покрывать окно
            dy = std::min(0.f, std::max(fh - dh, dy));
            const SDL_FRect dst{ dx, dy, dw, dh };
            SDL_SetTextureAlphaMod(g_tex, 255);
            SDL_SetTextureColorMod(g_tex, 255, 255, 255);
            SDL_SetTextureBlendMode(g_tex, SDL_BLENDMODE_BLEND);
            SDL_RenderTexture(r, g_tex, nullptr, &dst);

            // вуаль: тонкая, чтобы арт оставался ярким; чем светлее картинка, тем плотнее.
            // Читаемость текста держат сами панели (они плотные) и мягкая тень букв (Text.cpp).
            const float a = Lerpf(0.10f, 0.40f, Sat((g_luma - 0.30f) / 0.42f));
            VGradient(r, 0.f, 0.f, fw, fh, veil, Uint8(255.f * a * 0.80f), veil, Uint8(255.f * std::min(0.62f, a * 1.15f)));
        }
    }

    // виньетка: края чуть темнее, взгляд остаётся в центре
    const float vw = std::min(fw * 0.16f, 120.f), vh = std::min(fh * 0.16f, 90.f);
    const Col dark{ 28, 10, 34 };
    HGradient(r, 0.f, 0.f, vw, fh, dark, 70, dark, 0);
    HGradient(r, fw - vw, 0.f, vw, fh, dark, 0, dark, 70);
    VGradient(r, 0.f, 0.f, fw, vh, dark, 60, dark, 0);
    VGradient(r, 0.f, fh - vh, fw, vh, dark, 0, dark, 70);
}

// Положение «по кругу»: t в секундах, возвращает значение в [0, period).
inline float Wrap(double v, double period) {
    double m = std::fmod(v, period);
    if (m < 0.0) m += period;
    return float(m);
}

// Боке, лепестки, звёздочки, сердечки. Рисуется поверх обоев и под интерфейсом.
inline void DrawFalling(SDL_Renderer* r, int w, int h) {
    using namespace CuteGfx;
    const float fw = float(w), fh = float(h);
    const double t = double(SDL_GetTicks()) / 1000.0;
    const float k = std::min(1.7f, std::max(0.8f, fh / 640.f));   // масштаб под размер окна
    SDL_Texture* bokeh = BokehTex(r);
    SDL_Texture* petal = PetalTex(r);
    SDL_Texture* heart = HeartTex(r);
    SDL_Texture* spark = SparkleTex(r);
    if (!petal || !heart || !spark || !bokeh) return;

    // --- боке: тёплые огоньки, медленно плывут и пульсируют (аддитивно) ---
    static const Col kBokeh[4] = { { 255, 170, 205 }, { 255, 205, 170 }, { 205, 180, 255 }, { 255, 230, 190 } };
    for (Uint32 i = 0; i < 9; ++i) {
        const float size = Lerpf(46.f, 120.f, H01(i * 11u + 1u)) * k;
        const float px = Wrap(double(H01(i * 11u + 2u)) * fw + t * Lerpf(-4.f, 6.f, H01(i * 11u + 3u)), double(fw) + size) - size * 0.5f;
        const float py = Wrap(double(H01(i * 11u + 4u)) * fh - t * Lerpf(3.f, 9.f, H01(i * 11u + 5u)), double(fh) + size) - size * 0.5f;
        const float pulse = 0.65f + 0.35f * float(std::sin(t * Lerpf(0.3f, 0.8f, H01(i * 11u + 6u)) + H01(i * 11u + 7u) * 6.28f));
        const Col c = kBokeh[H32(i * 11u + 8u) & 3u];
        Sprite(r, bokeh, px, py, size, size, 0.0, c.r, c.g, c.b, Uint8(34.f * pulse), SDL_BLENDMODE_ADD);
    }

    // --- звёздочки: мерцают на месте ---
    for (Uint32 i = 0; i < 18; ++i) {
        const float x = H01(i * 7u + 101u) * fw, y = H01(i * 7u + 102u) * fh;
        const float size = Lerpf(8.f, 19.f, H01(i * 7u + 103u)) * k;
        float tw = 0.5f + 0.5f * float(std::sin(t * Lerpf(0.9f, 2.0f, H01(i * 7u + 104u)) + H01(i * 7u + 105u) * 6.28f));
        tw = tw * tw * tw;
        Sprite(r, spark, x, y, size * (0.6f + 0.5f * tw), size * (0.6f + 0.5f * tw), 0.0, 255, 236, 246, Uint8(40.f + 190.f * tw), SDL_BLENDMODE_ADD);
    }

    // --- сердечки: поднимаются и покачиваются ---
    for (Uint32 i = 0; i < 6; ++i) {
        const float size = Lerpf(12.f, 22.f, H01(i * 5u + 201u)) * k;
        const double cycle = double(fh) + 120.0 * k;
        const float y = fh + 60.f * k - Wrap(double(H01(i * 5u + 202u)) * cycle + t * Lerpf(14.f, 28.f, H01(i * 5u + 203u)) * k, cycle);
        const float x = H01(i * 5u + 204u) * fw + 20.f * k * float(std::sin(t * 0.8 + double(H01(i * 5u + 205u)) * 6.28));
        const float life = Sat(y / fh) ;   // у верхнего края растворяются
        Sprite(r, heart, x, y, size, size * 0.92f, 8.0 * std::sin(t * 0.9 + i), 255, Uint8(150 + 60 * (i & 1u)), Uint8(190 + 30 * (i & 1u)),
            Uint8(150.f * std::min(1.f, life * 3.f)));
    }

    // --- лепестки сакуры ---
    static const Col kTint[5] = { { 255, 255, 255 }, { 255, 232, 240 }, { 255, 214, 228 }, { 255, 240, 232 }, { 250, 220, 255 } };
    const int count = std::max(36, std::min(90, int(fw * fh / 9000.f)));
    for (int i = 0; i < count; ++i) {
        const Uint32 s = Uint32(i) * 16u + 1000u;
        const float sizeF = H01(s + 0);                      // 0 = далёкий и мелкий, 1 = близкий и крупный
        const float len = Lerpf(14.f, 36.f, sizeF * sizeF * 0.6f + sizeF * 0.4f) * k;
        const float vy = Lerpf(22.f, 58.f, sizeF) * k;
        const float vx = Lerpf(6.f, 22.f, H01(s + 1)) * k;   // ветер слева направо
        const float amp = Lerpf(16.f, 44.f, H01(s + 2)) * k;
        const float freq = Lerpf(0.5f, 1.2f, H01(s + 3));
        const float ph = H01(s + 4) * 6.2832f;
        const double cycleY = double(fh) + 2.0 * len + 60.0, cycleX = double(fw) + 2.0 * len + 120.0;
        const float y = Wrap(double(H01(s + 5)) * cycleY + t * vy, cycleY) - len - 30.f;
        const float x = Wrap(double(H01(s + 6)) * cycleX + t * vx, cycleX) - len - 60.f + amp * float(std::sin(t * freq + ph));
        const float spin = Lerpf(-60.f, 60.f, H01(s + 7));
        const double ang = double(H01(s + 8)) * 360.0 + double(spin) * t + 26.0 * std::sin(t * freq + ph + 1.3);
        const float flip = 0.30f + 0.70f * float(std::fabs(std::cos(t * Lerpf(0.7f, 1.6f, H01(s + 9)) + H01(s + 10) * 6.28)));   // «переворот» в воздухе
        const Col c = kTint[H32(s + 11) % 5u];
        const Uint8 a = Uint8(Lerpf(150.f, 245.f, sizeF));
        Sprite(r, petal, x, y, len * 0.72f * flip, len, ang, c.r, c.g, c.b, a);
    }
}

// Точка на скруглённом прямоугольнике: расстояние d вдоль периметра, по часовой от левого верхнего угла дуги.
inline SDL_FPoint PointOnRoundRect(float x, float y, float w, float h, float R, float d) {
    R = std::max(0.f, std::min(R, std::min(w, h) * 0.5f));
    const float sw = w - 2.f * R, sh = h - 2.f * R, arc = 0.5f * 3.14159265f * R;
    const float per = 2.f * sw + 2.f * sh + 4.f * arc;
    if (per < 1.f) return { x, y };
    d = std::fmod(d, per); if (d < 0.f) d += per;
    const float pi2 = 1.5707963f;
    if (d < sw) return { x + R + d, y };
    d -= sw;
    if (d < arc) { const float a = -pi2 + d / R; return { x + w - R + R * std::cos(a), y + R + R * std::sin(a) }; }
    d -= arc;
    if (d < sh) return { x + w, y + R + d };
    d -= sh;
    if (d < arc) { const float a = d / R; return { x + w - R + R * std::cos(a), y + h - R + R * std::sin(a) }; }
    d -= arc;
    if (d < sw) return { x + w - R - d, y + h };
    d -= sw;
    if (d < arc) { const float a = pi2 + d / R; return { x + R + R * std::cos(a), y + h - R + R * std::sin(a) }; }
    d -= arc;
    if (d < sh) return { x, y + h - R - d };
    d -= sh;
    const float a = 3.14159265f + d / R;
    return { x + R + R * std::cos(a), y + R + R * std::sin(a) };
}

inline float RoundRectPerimeter(float w, float h, float R) {
    R = std::max(0.f, std::min(R, std::min(w, h) * 0.5f));
    return 2.f * (w - 2.f * R) + 2.f * (h - 2.f * R) + 2.f * 3.14159265f * R;
}

} // namespace CuteScene

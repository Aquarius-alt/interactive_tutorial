#pragma once
// CuteGfx.h - графика стиля PRO CUTE: сглаженные скруглённые формы и мягкие спрайты.
//
// Почему не SDL_RenderGeometry: многоугольники SDL не сглажены, и скруглённые углы выглядели бы лесенкой.
// Поэтому формы один раз «печатаются» в маленькие RGBA-картинки с точным покрытием пикселей
// (расстояние до края, 4x4 суперсэмплинг для спрайтов), кэшируются как текстуры и рисуются
// «девятью кусками» (9-slice): четыре угла 1:1, четыре края и середина растягиваются.
// Цвет и прозрачность задаются SDL_SetTextureColorMod / AlphaMod, поэтому одна текстура
// годится для любого цвета.
//
// Верхняя часть файла (Bitmap, Make*, Slice9) не зависит от SDL: её можно собрать и проверить отдельно
// (#define CUTE_NO_SDL).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
#ifndef CUTE_NO_SDL
#include <SDL3/SDL.h>
#include <unordered_map>
#endif

namespace CuteGfx {

constexpr float kPi = 3.14159265358979f;

// ---------------------------------------------------------------------------
// Пиксели
// ---------------------------------------------------------------------------
struct Bitmap {
    int w = 0, h = 0;
    std::vector<std::uint8_t> rgba;   // байты R, G, B, A подряд (SDL_PIXELFORMAT_RGBA32)
    void Init(int W, int H) { w = W; h = H; rgba.assign(size_t(W) * size_t(H) * 4u, 0); }
    std::uint8_t* At(int x, int y) { return &rgba[(size_t(y) * size_t(w) + size_t(x)) * 4u]; }
    const std::uint8_t* At(int x, int y) const { return &rgba[(size_t(y) * size_t(w) + size_t(x)) * 4u]; }
};

inline float Sat(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline std::uint8_t ToByte(float v) { return std::uint8_t(Sat(v) * 255.f + 0.5f); }

// Расстояние со знаком до скруглённого прямоугольника с центром в нуле (внутри < 0).
inline float SdRoundRect(float px, float py, float hx, float hy, float r) {
    const float qx = std::fabs(px) - (hx - r), qy = std::fabs(py) - (hy - r);
    const float ox = std::max(qx, 0.f), oy = std::max(qy, 0.f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.f) - r;
}

inline void PutWhite(Bitmap& b, int x, int y, float cov) {
    std::uint8_t* p = b.At(x, y);
    p[0] = p[1] = p[2] = 255;
    p[3] = ToByte(cov);
}

// Заливка: квадрат (2R+2) со скруглёнными углами радиуса R на всю картинку.
inline Bitmap MakeRoundFill(int R) {
    Bitmap b; const int S = 2 * R + 2; b.Init(S, S);
    const float hs = float(S) * 0.5f;
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x)
            PutWhite(b, x, y, 0.5f - SdRoundRect(float(x) + 0.5f - hs, float(y) + 0.5f - hs, hs, hs, float(R)));
    return b;
}

// Контур толщиной t внутри той же формы (как SDL_RenderRect: линия идёт по внутренней кромке).
inline Bitmap MakeRoundRing(int R, int t) {
    Bitmap b; const int S = 2 * R + 2; b.Init(S, S);
    const float hs = float(S) * 0.5f, ht = float(t) * 0.5f;
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            const float sd = SdRoundRect(float(x) + 0.5f - hs, float(y) + 0.5f - hs, hs, hs, float(R));
            PutWhite(b, x, y, 0.5f - (std::fabs(sd + ht) - ht));
        }
    return b;
}

// Мягкое свечение снаружи формы с радиусом R: толщина ореола G. Внутри формы прозрачно.
inline Bitmap MakeRoundGlow(int R, int G) {
    Bitmap b; const int S = 2 * (R + G) + 2; b.Init(S, S);
    const float hs = float(S) * 0.5f, inner = float(R) + 1.f;
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            const float d = SdRoundRect(float(x) + 0.5f - hs, float(y) + 0.5f - hs, inner, inner, float(R));
            float a = 0.f;
            if (d > 0.f) { const float k = Sat(1.f - d / float(G)); a = k * k * (0.55f + 0.45f * k); }
            PutWhite(b, x, y, a);
        }
    return b;
}

// Круг с мягким краем (точки стёжки, блики).
inline Bitmap MakeDisc(int D) {
    Bitmap b; b.Init(D, D);
    const float c = float(D) * 0.5f;
    for (int y = 0; y < D; ++y)
        for (int x = 0; x < D; ++x) {
            const float d = std::hypot(float(x) + 0.5f - c, float(y) + 0.5f - c) - (c - 0.5f);
            PutWhite(b, x, y, 0.5f - d);
        }
    return b;
}

// Боке: полупрозрачный диск с чуть более ярким ободком, как огонёк лампы не в фокусе.
inline Bitmap MakeBokeh(int D) {
    Bitmap b; b.Init(D, D);
    const float c = float(D) * 0.5f;
    for (int y = 0; y < D; ++y)
        for (int x = 0; x < D; ++x) {
            const float d = std::hypot(float(x) + 0.5f - c, float(y) + 0.5f - c) / c;
            const float body = 0.30f + 0.70f * std::pow(Sat(d), 5.f);
            const float edge = Sat((1.f - d) * c * 0.9f);
            PutWhite(b, x, y, body * edge * 0.75f);
        }
    return b;
}

// ---------------------------------------------------------------------------
// Спрайты (суперсэмплинг 4x4). inside(x, y) -> bool, shade(x, y) -> RGB 0..255 в координатах спрайта.
// ---------------------------------------------------------------------------
struct Rgb3 { float r, g, b; };

template <class Inside, class Shade>
inline Bitmap RasterSprite(int W, int H, Inside inside, Shade shade, int ss = 4) {
    Bitmap b; b.Init(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            int hit = 0;
            for (int sy = 0; sy < ss; ++sy)
                for (int sx = 0; sx < ss; ++sx)
                    if (inside(float(x) + (float(sx) + 0.5f) / float(ss), float(y) + (float(sy) + 0.5f) / float(ss))) ++hit;
            if (!hit) continue;
            const Rgb3 c = shade(float(x) + 0.5f, float(y) + 0.5f);
            std::uint8_t* p = b.At(x, y);
            p[0] = ToByte(c.r / 255.f); p[1] = ToByte(c.g / 255.f); p[2] = ToByte(c.b / 255.f);
            p[3] = ToByte(float(hit) / float(ss * ss));
        }
    return b;
}

// Кривая «сердца»: пригодна и для сердечка, и для лепестка сакуры (выемка на конце, узкое основание).
// Нормализована в [-0.5, 0.5] по ширине и [0, 1] по высоте; y = 0 - остриё (основание), y = 1 - край с выемкой.
inline const std::vector<std::pair<float, float>>& HeartOutline() {
    static const std::vector<std::pair<float, float>> pts = [] {
        std::vector<std::pair<float, float>> v;
        const int N = 96;
        for (int i = 0; i < N; ++i) {
            const float th = 2.f * kPi * float(i) / float(N);
            const float x = 16.f * std::pow(std::sin(th), 3.f);
            const float y = 13.f * std::cos(th) - 5.f * std::cos(2.f * th) - 2.f * std::cos(3.f * th) - std::cos(4.f * th);
            v.push_back({ x / 32.f, (y + 17.f) / 29.f });   // x: -0.5..0.5, y: 0..~1
        }
        return v;
    }();
    return pts;
}

// Чётно-нечётный тест «точка внутри многоугольника».
inline bool PointInPoly(const std::vector<std::pair<float, float>>& poly, float x, float y) {
    bool in = false;
    const size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const float xi = poly[i].first, yi = poly[i].second, xj = poly[j].first, yj = poly[j].second;
        if (((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) in = !in;
    }
    return in;
}

// Лепесток сакуры W x H: выемка наверху, остриё внизу, градиент от светлого края к насыщенному основанию, прожилка.
inline Bitmap MakePetal(int W, int H) {
    const auto& poly = HeartOutline();
    const float sx = float(W) * 0.92f, sy = float(H) * 0.94f, ox = float(W) * 0.5f, oy = float(H) * 0.97f;
    auto inside = [&](float x, float y) {   // экранный y вниз: остриё у нижнего края
        return PointInPoly(poly, (x - ox) / sx, (oy - y) / sy);
    };
    auto shade = [&](float x, float y) {
        const float t = Sat((oy - y) / sy);                      // 0 у основания, 1 у края
        const float vein = std::exp(-std::pow((x - ox) / (float(W) * 0.05f), 2.f)) * (1.f - t) * 0.16f;
        const Rgb3 base{ 255.f, Lerp(176.f, 238.f, t), Lerp(204.f, 244.f, t) };
        return Rgb3{ base.r, base.g * (1.f - vein * 0.6f), base.b * (1.f - vein * 0.35f) };
    };
    return RasterSprite(W, H, inside, shade);
}

// Сердечко W x H с мягким бликом (серый тон: цвет задаётся ColorMod).
inline Bitmap MakeHeart(int W, int H) {
    const auto& poly = HeartOutline();
    const float sx = float(W) * 0.96f, sy = float(H) * 0.96f, ox = float(W) * 0.5f, oy = float(H) * 0.98f;
    auto inside = [&](float x, float y) { return PointInPoly(poly, (x - ox) / sx, (oy - y) / sy); };
    auto shade = [&](float x, float y) {
        const float gx = (x - float(W) * 0.32f) / (float(W) * 0.16f), gy = (y - float(H) * 0.28f) / (float(H) * 0.11f);
        const float gloss = std::exp(-(gx * gx + gy * gy));
        const float v = Lerp(205.f, 255.f, Sat(gloss * 1.2f)) - Sat((y / float(H)) - 0.55f) * 38.f;
        return Rgb3{ v, v, v };
    };
    return RasterSprite(W, H, inside, shade);
}

// Четырёхлучевая звёздочка D x D (вогнутые стороны), белая.
inline Bitmap MakeSparkle(int D) {
    const float c = float(D) * 0.5f, R = c - 0.8f;
    auto inside = [&](float x, float y) {
        const float ax = std::fabs(x - c) / R, ay = std::fabs(y - c) / R;
        return std::pow(ax, 0.55f) + std::pow(ay, 0.55f) <= 1.f;
    };
    auto shade = [&](float x, float y) {
        const float d = std::hypot(x - c, y - c) / R;
        const float v = Lerp(255.f, 232.f, Sat(d));
        return Rgb3{ 255.f, v, v };
    };
    return RasterSprite(D, D, inside, shade);
}

// Цветок сакуры D x D: пять лепестков с выемками, тёмная сердцевина и тычинки. Цвета запечены (ColorMod не нужен).
inline Bitmap MakeBlossom(int D) {
    const auto& poly = HeartOutline();
    const float c = float(D) * 0.5f, len = c * 0.97f, wid = len * 0.86f;
    auto inside = [&](float px, float py) {
        const float x = px - c, y = py - c;
        if (std::hypot(x, y) < len * 0.30f) return true;           // центр, чтобы между остриями лепестков не было щелей
        for (int k = 0; k < 5; ++k) {
            const float a = -2.f * kPi * float(k) / 5.f;            // поворот точки в систему лепестка
            const float rx = x * std::cos(a) - y * std::sin(a);
            const float ry = x * std::sin(a) + y * std::cos(a);     // лепесток смотрит вверх (ry < 0)
            if (PointInPoly(poly, rx / wid, (-ry) / len)) return true;
        }
        return false;
    };
    auto shade = [&](float px, float py) {
        const float x = px - c, y = py - c, d = std::hypot(x, y) / len;
        // тычинки: восемь точек вокруг центра
        for (int k = 0; k < 8; ++k) {
            const float a = 2.f * kPi * (float(k) + 0.5f) / 8.f;
            const float sxp = std::cos(a) * len * 0.22f, syp = std::sin(a) * len * 0.22f;
            if (std::hypot(x - sxp, y - syp) < len * 0.045f) return Rgb3{ 255.f, 226.f, 120.f };
        }
        const float core = Sat(1.f - d / 0.34f);                    // розово-красная сердцевина
        const float edge = Sat((d - 0.35f) / 0.65f);                // светлее к краю
        Rgb3 col{ 255.f, Lerp(190.f, 240.f, edge), Lerp(212.f, 246.f, edge) };
        col.r = Lerp(col.r, 232.f, core); col.g = Lerp(col.g, 96.f, core); col.b = Lerp(col.b, 140.f, core);
        return col;
    };
    return RasterSprite(D, D, inside, shade);
}

// ---------------------------------------------------------------------------
// 9-slice: раскладка кусков. blit(srcX, srcY, srcW, srcH, dstX, dstY, dstW, dstH).
// S - сторона текстуры, C - размер угла (середина S - 2C). Размер приёмника не меньше 2C по обеим осям.
// ---------------------------------------------------------------------------
template <class Blit>
inline void Slice9(Blit&& blit, int S, int C, float x, float y, float w, float h, bool center) {
    const int M = S - 2 * C;
    const float mw = w - 2.f * float(C), mh = h - 2.f * float(C);
    blit(0, 0, C, C, x, y, float(C), float(C));
    blit(S - C, 0, C, C, x + w - float(C), y, float(C), float(C));
    blit(0, S - C, C, C, x, y + h - float(C), float(C), float(C));
    blit(S - C, S - C, C, C, x + w - float(C), y + h - float(C), float(C), float(C));
    if (mw > 0.f) {
        blit(C, 0, M, C, x + float(C), y, mw, float(C));
        blit(C, S - C, M, C, x + float(C), y + h - float(C), mw, float(C));
    }
    if (mh > 0.f) {
        blit(0, C, C, M, x, y + float(C), float(C), mh);
        blit(S - C, C, C, M, x + w - float(C), y + float(C), float(C), mh);
    }
    if (center && mw > 0.f && mh > 0.f) blit(C, C, M, M, x + float(C), y + float(C), mw, mh);
}

// ---------------------------------------------------------------------------
// SDL: кэш текстур и рисование
// ---------------------------------------------------------------------------
#ifndef CUTE_NO_SDL

inline std::unordered_map<std::uint32_t, SDL_Texture*>& TexCache() {
    static std::unordered_map<std::uint32_t, SDL_Texture*> m;
    return m;
}
inline SDL_Renderer*& CacheOwner() { static SDL_Renderer* o = nullptr; return o; }

// Освободить все текстуры (до SDL_DestroyRenderer; также вызывается сам при смене рендерера).
inline void ReleaseTextures() {
    for (auto& kv : TexCache()) if (kv.second) SDL_DestroyTexture(kv.second);
    TexCache().clear();
    CacheOwner() = nullptr;
}

inline SDL_Texture* MakeTexture(SDL_Renderer* r, const Bitmap& b) {
    SDL_Surface* s = SDL_CreateSurfaceFrom(b.w, b.h, SDL_PIXELFORMAT_RGBA32,
        const_cast<std::uint8_t*>(b.rgba.data()), b.w * 4);
    SDL_Texture* t = s ? SDL_CreateTextureFromSurface(r, s) : nullptr;
    if (s) SDL_DestroySurface(s);
    if (t) {
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    }
    return t;
}

enum Kind : std::uint32_t { KFill = 1, KRing = 2, KGlow = 3, KPetal = 4, KHeart = 5, KSparkle = 6, KBlossom = 7, KDisc = 8, KBokeh = 9 };

inline std::uint32_t Key(Kind k, int a, int b = 0) { return (std::uint32_t(k) << 24) | (std::uint32_t(a & 0xFFF) << 12) | std::uint32_t(b & 0xFFF); }

// Текстура по ключу; make() вызывается один раз.
template <class Make>
inline SDL_Texture* Cached(SDL_Renderer* r, std::uint32_t key, Make make) {
    if (CacheOwner() != r) { ReleaseTextures(); CacheOwner() = r; }
    auto& m = TexCache();
    auto it = m.find(key);
    if (it != m.end()) return it->second;
    if (m.size() > 400) { ReleaseTextures(); CacheOwner() = r; }   // страховка от разрастания
    SDL_Texture* t = MakeTexture(r, make());
    m[key] = t;
    return t;
}

inline void SetMods(SDL_Texture* t, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca, SDL_BlendMode mode) {
    SDL_SetTextureColorMod(t, cr, cg, cb);
    SDL_SetTextureAlphaMod(t, ca);
    SDL_SetTextureBlendMode(t, mode);
}

// Целочисленная привязка прямоугольника к пикселям (иначе швы между кусками мерцают).
struct IRect { int x, y, w, h; };
inline IRect Snap(float x, float y, float w, float h) {
    const int ix = int(std::lround(x)), iy = int(std::lround(y));
    return { ix, iy, std::max(0, int(std::lround(x + w)) - ix), std::max(0, int(std::lround(y + h)) - iy) };
}

// Подходящий радиус: целый, не больше половины меньшей стороны.
inline int FitRadius(float radius, int w, int h) {
    return std::max(0, std::min(int(std::lround(radius)), std::min(w, h) / 2));
}

inline void DrawSlices(SDL_Renderer* r, SDL_Texture* t, int S, int C, float x, float y, float w, float h, bool center) {
    Slice9([&](int sx, int sy, int sw, int sh, float dx, float dy, float dw, float dh) {
        const SDL_FRect src{ float(sx), float(sy), float(sw), float(sh) };
        const SDL_FRect dst{ dx, dy, dw, dh };
        SDL_RenderTexture(r, t, &src, &dst);
    }, S, C, x, y, w, h, center);
}

// Заливка скруглённого прямоугольника. Возвращает false, если форма слишком мала для скругления.
inline bool RoundFill(SDL_Renderer* r, float x, float y, float w, float h, float radius,
    Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca, SDL_BlendMode mode = SDL_BLENDMODE_BLEND) {
    const IRect q = Snap(x, y, w, h);
    const int R = FitRadius(radius, q.w, q.h);
    if (R < 1 || ca == 0) return R >= 1;
    SDL_Texture* t = Cached(r, Key(KFill, R), [&] { return MakeRoundFill(R); });
    if (!t) return false;
    SetMods(t, cr, cg, cb, ca, mode);
    DrawSlices(r, t, 2 * R + 2, R, float(q.x), float(q.y), float(q.w), float(q.h), true);
    return true;
}

// Контур толщиной th (1..R).
inline bool RoundStroke(SDL_Renderer* r, float x, float y, float w, float h, float radius, int th,
    Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca, SDL_BlendMode mode = SDL_BLENDMODE_BLEND) {
    const IRect q = Snap(x, y, w, h);
    const int R = FitRadius(radius, q.w, q.h);
    if (R < 1) return false;
    if (ca == 0) return true;
    th = std::max(1, std::min(th, R));
    SDL_Texture* t = Cached(r, Key(KRing, R, th), [&] { return MakeRoundRing(R, th); });
    if (!t) return false;
    SetMods(t, cr, cg, cb, ca, mode);
    DrawSlices(r, t, 2 * R + 2, R, float(q.x), float(q.y), float(q.w), float(q.h), false);
    return true;
}

// Мягкий ореол вокруг формы шириной g пикселей.
inline void RoundGlow(SDL_Renderer* r, float x, float y, float w, float h, float radius, int g,
    Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca, SDL_BlendMode mode = SDL_BLENDMODE_BLEND) {
    const IRect q = Snap(x, y, w, h);
    const int R = FitRadius(radius, q.w, q.h);
    if (R < 1 || g < 1 || ca == 0) return;
    SDL_Texture* t = Cached(r, Key(KGlow, R, g), [&] { return MakeRoundGlow(R, g); });
    if (!t) return;
    SetMods(t, cr, cg, cb, ca, mode);
    DrawSlices(r, t, 2 * (R + g) + 2, R + g, float(q.x - g), float(q.y - g), float(q.w + 2 * g), float(q.h + 2 * g), false);
}

// Спрайт целиком, по центру (cx, cy), размер dw x dh, поворот в градусах.
inline void Sprite(SDL_Renderer* r, SDL_Texture* t, float cx, float cy, float dw, float dh, double angleDeg,
    Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca, SDL_BlendMode mode = SDL_BLENDMODE_BLEND) {
    if (!t || ca == 0) return;
    SetMods(t, cr, cg, cb, ca, mode);
    const SDL_FRect dst{ cx - dw * 0.5f, cy - dh * 0.5f, dw, dh };
    SDL_RenderTextureRotated(r, t, nullptr, &dst, angleDeg, nullptr, SDL_FLIP_NONE);
}

inline SDL_Texture* PetalTex(SDL_Renderer* r)   { return Cached(r, Key(KPetal, 40, 56),    [] { return MakePetal(40, 56); }); }
inline SDL_Texture* HeartTex(SDL_Renderer* r)   { return Cached(r, Key(KHeart, 48, 44),    [] { return MakeHeart(48, 44); }); }
inline SDL_Texture* SparkleTex(SDL_Renderer* r) { return Cached(r, Key(KSparkle, 48),      [] { return MakeSparkle(48); }); }
inline SDL_Texture* BlossomTex(SDL_Renderer* r) { return Cached(r, Key(KBlossom, 96),      [] { return MakeBlossom(96); }); }
inline SDL_Texture* DiscTex(SDL_Renderer* r)    { return Cached(r, Key(KDisc, 24),         [] { return MakeDisc(24); }); }
inline SDL_Texture* BokehTex(SDL_Renderer* r)   { return Cached(r, Key(KBokeh, 128),       [] { return MakeBokeh(128); }); }

#endif // CUTE_NO_SDL

} // namespace CuteGfx

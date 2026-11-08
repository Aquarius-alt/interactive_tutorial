#pragma once
// Тема «кибердек»: тёмно-бордовый фон, циан + коралл, угловатые HUD-рамки.
// Только заголовок (inline), в проект добавлять ничего не нужно.
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include "Text.h"

namespace Theme {

struct Rgb { Uint8 r, g, b; };

inline Rgb Cyan{ 96, 228, 232 };      // основной цвет линий и текста
inline Rgb CyanHot{ 200, 255, 255 };  // наведение / фокус
inline Rgb Coral{ 214, 84, 78 };      // акцент: предупреждения, активное действие
inline Rgb CoralHot{ 255, 125, 110 };
inline Rgb Panel{ 26, 12, 18 };       // заливка панелей и кнопок
inline Rgb PanelHi{ 46, 18, 26 };
inline Rgb BgTop{ 58, 17, 24 };       // фон: сверху тёмно-красный ...
inline Rgb BgBottom{ 11, 15, 25 };    // ... снизу почти чёрно-синий
inline Rgb Ink{ 205, 255, 255 };      // яркий основной текст (ввод, вопросы)
inline Rgb InkDim{ 150, 190, 215 };   // второстепенный текст (подписи, заголовки блоков)

inline void SetColor(SDL_Renderer* r, Rgb c, Uint8 a = 255) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, a);
}

// Шестиугольник: срезаны левый верхний и правый нижний углы (как у панелей в референсе).
// Возвращает число точек (6); в p должно быть место минимум на 7 (замыкающая точка).
inline int CutPoly(SDL_FPoint* p, float x, float y, float w, float h, float cut) {
    const float c = std::max(0.f, std::min(cut, std::min(w, h) * 0.5f));
    p[0] = { x + c, y };
    p[1] = { x + w, y };
    p[2] = { x + w, y + h - c };
    p[3] = { x + w - c, y + h };
    p[4] = { x, y + h };
    p[5] = { x, y + c };
    p[6] = p[0];
    return 6;
}

inline void FillPoly(SDL_Renderer* r, const SDL_FPoint* p, int n, Rgb c, Uint8 a = 255) {
    if (n < 3 || n > 8) return;
    const SDL_FColor col{ c.r / 255.f, c.g / 255.f, c.b / 255.f, a / 255.f };
    SDL_Vertex v[8];
    for (int i = 0; i < n; ++i) v[i] = SDL_Vertex{ p[i], col, SDL_FPoint{ 0.f, 0.f } };
    int idx[18];
    int m = 0;
    for (int i = 1; i < n - 1; ++i) { idx[m++] = 0; idx[m++] = i; idx[m++] = i + 1; }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, nullptr, v, n, idx, m);
}

inline void Rect(SDL_Renderer* r, float x, float y, float w, float h, Rgb c, Uint8 a = 255) {
    SetColor(r, c, a);
    const SDL_FRect q{ x, y, w, h };
    SDL_RenderFillRect(r, &q);
}

// Контур со срезанными углами, glow = слабое «свечение» двумя внешними контурами.
inline void CutOutline(SDL_Renderer* r, float x, float y, float w, float h, float cut,
    Rgb c, Uint8 a = 255, bool glow = false) {
    SDL_FPoint p[7];
    if (glow) {
        SetColor(r, c, Uint8(a * 0.16f));
        CutPoly(p, x - 2, y - 2, w + 4, h + 4, cut + 2);
        SDL_RenderLines(r, p, 7);
        SetColor(r, c, Uint8(a * 0.30f));
        CutPoly(p, x - 1, y - 1, w + 2, h + 2, cut + 1);
        SDL_RenderLines(r, p, 7);
    }
    SetColor(r, c, a);
    CutPoly(p, x, y, w, h, cut);
    SDL_RenderLines(r, p, 7);
}

inline void CutPanel(SDL_Renderer* r, float x, float y, float w, float h, float cut,
    Rgb fill, Uint8 fillA, Rgb line, Uint8 lineA, bool glow = false) {
    SDL_FPoint p[7];
    CutPoly(p, x, y, w, h, cut);
    FillPoly(r, p, 6, fill, fillA);
    CutOutline(r, x, y, w, h, cut, line, lineA, glow);
}

// Маленький залитый треугольник в правом верхнем углу (маркер кнопки из референса).
inline void CornerMarker(SDL_Renderer* r, float x, float y, float w, float size, Rgb c, Uint8 a = 255) {
    const SDL_FPoint t[3] = { { x + w - size, y }, { x + w, y }, { x + w, y + size } };
    FillPoly(r, t, 3, c, a);
}

// Уголки-скобки по периметру прямоугольника.
inline void Brackets(SDL_Renderer* r, float x, float y, float w, float h, float len,
    Rgb c, Uint8 a = 255) {
    SetColor(r, c, a);
    for (int k = 0; k < 2; ++k) {
        const float o = float(k);
        SDL_RenderLine(r, x, y + o, x + len, y + o);
        SDL_RenderLine(r, x + o, y, x + o, y + len);
        SDL_RenderLine(r, x + w - len, y + o, x + w, y + o);
        SDL_RenderLine(r, x + w - 1 - o, y, x + w - 1 - o, y + len);
        SDL_RenderLine(r, x, y + h - 1 - o, x + len, y + h - 1 - o);
        SDL_RenderLine(r, x + o, y + h - len, x + o, y + h);
        SDL_RenderLine(r, x + w - len, y + h - 1 - o, x + w, y + h - 1 - o);
        SDL_RenderLine(r, x + w - 1 - o, y + h - len, x + w - 1 - o, y + h);
    }
}

// Фон окна: градиент бордо -> тёмно-синий + редкие строки развёртки.
inline void DrawBackground(SDL_Renderer* r, int w, int h) {
    SDL_SetRenderDrawColor(r, BgBottom.r, BgBottom.g, BgBottom.b, 255);
    SDL_RenderClear(r);
    const SDL_FColor top{ BgTop.r / 255.f, BgTop.g / 255.f, BgTop.b / 255.f, 1.f };
    const SDL_FColor bot{ BgBottom.r / 255.f, BgBottom.g / 255.f, BgBottom.b / 255.f, 1.f };
    const float fw = float(w), fh = float(h);
    const SDL_Vertex v[4] = {
        { { 0.f, 0.f }, top, { 0.f, 0.f } }, { { fw, 0.f }, top, { 0.f, 0.f } },
        { { fw, fh }, bot, { 0.f, 0.f } },   { { 0.f, fh }, bot, { 0.f, 0.f } } };
    const int idx[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);

    SetColor(r, Rgb{ 0, 0, 0 }, 34);
    for (int y = 1; y < h; y += 4) SDL_RenderLine(r, 0.f, float(y), fw, float(y));
}

// Рамка всего окна: срезанные углы, скобки, линейка слева, бегущий «скан» сверху
// и служебная строка в левом верхнем углу.
inline void DrawHudFrame(SDL_Renderer* r, int w, int h, bool fullscreen = false) {
    const float fw = float(w), fh = float(h);
    const float m = fullscreen ? 4.f : 5.f;
    const float cut = 18.f;
    const float t = float(SDL_GetTicks()) / 1000.f;

    CutOutline(r, m, m, fw - 2 * m, fh - 2 * m, cut, Cyan, 120, true);
    Brackets(r, m + 3, m + 3, fw - 2 * (m + 3), fh - 2 * (m + 3), 14.f, Coral, 230);

    // линейка с засечками на левом краю
    SetColor(r, Cyan, 70);
    for (float y = 48.f; y < fh - 40.f; y += 10.f) {
        const bool major = std::fmod(y - 48.f, 50.f) < 1.f;
        SDL_RenderLine(r, m + 3, y, m + 3 + (major ? 9.f : 4.f), y);
    }

    // бегущий сегмент по верхней линии
    const float segLen = 90.f;
    const float span = std::max(1.f, fw - 2 * m - cut - segLen);
    const float sx = m + cut + std::fmod(t * 140.f, span);
    SetColor(r, CyanHot, 230);
    SDL_RenderLine(r, sx, m, sx + segLen, m);
    SetColor(r, Cyan, 90);
    SDL_RenderLine(r, sx, m + 1, sx + segLen, m + 1);

    // служебная строка (шрифт 8 px, не залезает на заголовки по центру)
    const float labelW = std::max(0.f, fw * 0.28f);
    if (labelW > 80.f)
        DrawUtf8TextFit(r, "CYBERDECK V.552 // ACCESS GRANTED", m + cut + 4.f, m + 11.f, labelW,
            8, false, Cyan.r, Cyan.g, Cyan.b, 120);
}

// ---------------------------------------------------------------------------
// Анимационные помощники
// ---------------------------------------------------------------------------
inline float Clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
inline float EaseOut(float t) { t = Clamp01(t); return 1.f - (1.f - t) * (1.f - t) * (1.f - t); }
inline Rgb Mix(Rgb a, Rgb b, float t) {
    t = Clamp01(t);
    return Rgb{ Uint8(a.r + (b.r - a.r) * t), Uint8(a.g + (b.g - a.g) * t), Uint8(a.b + (b.b - a.b) * t) };
}
inline Uint32 Hash32(Uint32 x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
inline float Hash01(Uint32 x) { return float(Hash32(x) & 0xFFFFu) / 65535.f; }

// Отсекает выпуклый многоугольник вертикальной линией: остаётся часть левее xmax.
inline int ClipPolyLeft(const SDL_FPoint* in, int n, float xmax, SDL_FPoint* out) {
    int m = 0;
    for (int i = 0; i < n; ++i) {
        const SDL_FPoint a = in[i], b = in[(i + 1) % n];
        const bool ain = a.x <= xmax, bin = b.x <= xmax;
        if (ain) out[m++] = a;
        if (ain != bin) {
            const float k = (xmax - a.x) / (b.x - a.x);
            out[m++] = { xmax, a.y + (b.y - a.y) * k };
        }
        if (m >= 8) break;
    }
    return m;
}

// ---------------------------------------------------------------------------
// Переход между экранами: «взлом» — экран сразу меняется, а поверх него
// тёмные полосы с глитчем открываются слева направо; в центре бежит счётчик.
// ---------------------------------------------------------------------------
inline Uint64 g_transStart = 0;
inline bool   g_transActive = false;
constexpr float kTransitionSec = 0.55f;

inline void TransitionStart() { g_transStart = SDL_GetTicks(); g_transActive = true; }

inline void DrawTransition(SDL_Renderer* r, int w, int h) {
    if (!g_transActive) return;
    const float el = float(SDL_GetTicks() - g_transStart) / 1000.f;
    const float p = el / kTransitionSec;
    if (p >= 1.f) { g_transActive = false; return; }

    const float fw = float(w), fh = float(h);
    const float stripW = 26.f;
    const int cols = std::max(1, int(std::ceil(fw / stripW)));
    const Rgb cover{ 14, 8, 14 };
    const Uint32 frame = Uint32(el * 30.f);   // глитч меняется 30 раз в секунду

    for (int i = 0; i < cols; ++i) {
        const float pos = cols > 1 ? float(i) / float(cols - 1) : 0.f;
        const float openAt = 0.12f + 0.55f * pos + 0.28f * Hash01(Uint32(i) * 977u + 13u);
        const float x = float(i) * stripW;
        if (p < openAt) {
            Rect(r, x, 0.f, stripW + 1.f, fh, cover, 245);
            // рваный коралловый блок внутри полосы: короткий глитч
            const Uint32 hh = Hash32(Uint32(i) * 31u + frame);
            if ((hh & 7u) == 0u) {
                const float by = float(hh % 97u) / 97.f * fh;
                Rect(r, x, by, stripW, 3.f + float(hh % 11u), Coral, 150);
            }
        } else if (p < openAt + 0.10f) {
            const float k = 1.f - (p - openAt) / 0.10f;   // передний край открывающейся полосы
            Rect(r, x, 0.f, 2.f, fh, CyanHot, Uint8(220 * k));
            Rect(r, x + 2.f, 0.f, 6.f, fh, Cyan, Uint8(60 * k));
        }
    }

    // строки развёртки поверх закрытой части
    SetColor(r, Rgb{ 0, 0, 0 }, 60);
    for (int y = 0; y < h; y += 4) SDL_RenderLine(r, 0.f, float(y), fw, float(y));

    // счётчик по центру, пока полосы закрыты
    if (p < 0.65f) {
        const int pct = int(Clamp01(p / 0.65f) * 100.f);
        const std::string label = "ACCESSING.. " + std::to_string(pct) + "%";
        const int font = fw >= 420.f ? 16 : 8;
        const float tw = Utf8TextWidth(label, font);
        const float tx = (fw - tw) * 0.5f, ty = fh * 0.5f - font * 0.5f;
        DrawUtf8Text(r, label, tx + 2.f, ty, font, Coral.r, Coral.g, Coral.b, 180);   // «рассинхрон» канала
        DrawUtf8Text(r, label, tx, ty, font, Cyan.r, Cyan.g, Cyan.b, 255);
        const float bw = std::min(tw, fw - 40.f);
        Rect(r, tx, ty + font + 8.f, bw, 3.f, Cyan, 60);
        Rect(r, tx, ty + font + 8.f, bw * Clamp01(p / 0.65f), 3.f, CyanHot, 255);
    }
}

} // namespace Theme

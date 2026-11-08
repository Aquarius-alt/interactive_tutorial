#pragma once
// Тема «кибердек»: тёмно-бордовый фон, циан + коралл, угловатые HUD-рамки.
// Только заголовок (inline), в проект добавлять ничего не нужно.
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include "Text.h"
#include "CuteGfx.h"
#include "CuteScene.h"

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

// Минимальный стиль: плоский фон, прямоугольники без срезов, без свечения, скобок, HUD-рамки и сканлайнов.
// Включается в Styles::Apply.
inline bool Minimal = false;

// ---------------------------------------------------------------------------
// Виды интерфейса (кнопка ВИД в редакторе стиля). Каждый вид - набор черт: форма среза углов,
// число слоёв свечения, скобки, маркер, глитч и т.д. Выбранный вид ставит Styles::Apply.
//   FLAT  - плоский минимализм (Minimal = true)
//   HUD   - классический кибердек: срезаны левый верхний и правый нижний углы, бирюзовое свечение
//   BLADE - острый «клинок»: зеркальные срезы, двойная линия, длинные скобки, диагональная штриховка
//   NEON  - мягкий неон: восьмиугольники, сильное дышащее свечение, бегущая искра, сетка на горизонте
//   REGAL - рыцарский: восьмиугольники, двойная золотая рамка, ромбы-орнаменты, ромбовая решётка (ASTOLFO)
//   CUTE  - PRO CUTE: никаких острых углов (скруглённые «леденцовые» кнопки, панели и поля, сглаженные
//           через CuteGfx.h), мягкое свечение, лепестки сакуры, боке, звёздочки и сердечки на фоне (CuteScene.h)
// ---------------------------------------------------------------------------
enum LookId { LookFlat = 0, LookHud = 1, LookBlade = 2, LookNeon = 3, LookRegal = 4, LookCute = 5, LookCount = 6 };
inline int Look = LookHud;

struct LookTraits {
    int   shape;       // 0: срезаны левый верх + правый низ, 1: правый верх + левый низ, 2: все четыре угла
    float cutMul;      // множитель среза углов
    int   glow;        // слоёв свечения у контура
    float glowGain;    // яркость свечения
    bool  brackets, marker, underline, glitch, scan, doubleLine;
};
inline const LookTraits& Traits() {
    static const LookTraits k[LookCount] = {
        //  shape cutMul glow gain  brackets marker underline glitch scan  double
        {   0,    0.f,   0,   0.f,  false,   false, false,    false, false, false },   // FLAT
        {   0,    1.f,   2,   1.f,  true,    true,  true,     true,  true,  false },   // HUD
        {   1,    1.7f,  0,   0.f,  true,    true,  true,     true,  false, true  },   // BLADE
        {   2,    0.6f,  4,   1.5f, false,   false, false,    false, false, false },   // NEON
        {   2,    1.f,   2,   0.8f, true,    true,  true,     false, false, true  },   // REGAL
        {   0,    0.f,   0,   0.f,  false,   false, false,    false, false, false },   // CUTE (рисуется отдельной веткой)
    };
    return k[(Look >= 0 && Look < LookCount) ? Look : LookHud];
}

inline void SetColor(SDL_Renderer* r, Rgb c, Uint8 a = 255) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, a);
}

// ---------------------------------------------------------------------------
// PRO CUTE: скруглённые формы. Всё рисуется через CuteGfx (сглаженные 9-slice текстуры), так что
// краёв-«лесенок» нет. Вне вида CUTE все помощники ниже ведут себя как обычные SDL-вызовы.
// ---------------------------------------------------------------------------
inline bool CuteWallpaper = false;   // true = на фоне случайные обои (задаёт стиль, Styles::Apply)
inline bool Cute() { return Look == LookCute && !Minimal; }

// Радиус для произвольного прямоугольника: тонкие полоски и ползунки - «таблетки», крупные - мягкие углы.
inline float CuteRadius(float w, float h) {
    const float m = std::min(w, h);
    if (m <= 14.f) return m * 0.5f;
    return std::min(std::max(m * 0.30f, 7.f), 22.f);
}
// Радиус по «срезу» из старого кода (cut): чем больше срез, тем мягче круглые углы.
inline float CuteCutRadius(float cut, float w, float h) {
    return std::max(0.f, std::min(std::max(8.f, cut * 1.5f + 4.f), std::min(w, h) * 0.5f));
}

// Замена SDL_RenderFillRect / SDL_RenderRect для панелей, полос, ползунков. Цвет и режим смешивания берутся
// из рендерера, как у оригинальных вызовов; q == nullptr (весь экран) и слишком мелкие формы рисуются как раньше.
inline void StyledFill(SDL_Renderer* r, const SDL_FRect* q) {
    if (!q || !Cute()) { SDL_RenderFillRect(r, q); return; }
    Uint8 cr = 0, cg = 0, cb = 0, ca = 255;
    SDL_GetRenderDrawColor(r, &cr, &cg, &cb, &ca);
    SDL_BlendMode bm = SDL_BLENDMODE_BLEND;
    SDL_GetRenderDrawBlendMode(r, &bm);
    if (!CuteGfx::RoundFill(r, q->x, q->y, q->w, q->h, CuteRadius(q->w, q->h), cr, cg, cb, ca, bm))
        SDL_RenderFillRect(r, q);
}
inline void StyledStroke(SDL_Renderer* r, const SDL_FRect* q) {
    if (!q || !Cute()) { SDL_RenderRect(r, q); return; }
    Uint8 cr = 0, cg = 0, cb = 0, ca = 255;
    SDL_GetRenderDrawColor(r, &cr, &cg, &cb, &ca);
    SDL_BlendMode bm = SDL_BLENDMODE_BLEND;
    SDL_GetRenderDrawBlendMode(r, &bm);
    if (!CuteGfx::RoundStroke(r, q->x, q->y, q->w, q->h, CuteRadius(q->w, q->h), 1, cr, cg, cb, ca, bm))
        SDL_RenderRect(r, q);
}

// Контур скруглённой панели, glow = мягкий ореол снаружи.
inline void CuteOutline(SDL_Renderer* r, float x, float y, float w, float h, float R, Rgb c, Uint8 a, bool glow, int th = 2) {
    if (glow) CuteGfx::RoundGlow(r, x, y, w, h, R, 9, c.r, c.g, c.b, Uint8(a * 0.30f));
    CuteGfx::RoundStroke(r, x, y, w, h, R, th, c.r, c.g, c.b, a);
}
// Панель: ореол, заливка, тонкий светлый блик вдоль верхней кромки и контур.
inline void CutePanel(SDL_Renderer* r, float x, float y, float w, float h, float R,
    Rgb fill, Uint8 fillA, Rgb line, Uint8 lineA, bool glow) {
    if (glow) CuteGfx::RoundGlow(r, x, y, w, h, R, 9, line.r, line.g, line.b, Uint8(lineA * 0.30f));
    CuteGfx::RoundFill(r, x, y, w, h, R, fill.r, fill.g, fill.b, fillA);
    if (w > 4.f * R && h > 3.f * R)   // «стекло»: светлая дуга под верхним краем
        CuteGfx::RoundFill(r, x + R * 0.9f, y + 2.f, w - R * 1.8f, 2.f, 1.f, 255, 255, 255, Uint8(fillA * 0.10f));
    CuteGfx::RoundStroke(r, x, y, w, h, R, (w > 90.f && h > 60.f) ? 2 : 1, line.r, line.g, line.b, lineA);
}

// Многоугольник со срезанными углами; какие углы срезаны, задаёт вид (Traits().shape).
// Возвращает число точек (6 или 8); в p должно быть место минимум на 9 (замыкающая точка).
inline int CutPoly(SDL_FPoint* p, float x, float y, float w, float h, float cut) {
    const LookTraits& T = Traits();
    const float c = Minimal ? 0.f : std::max(0.f, std::min(cut * T.cutMul, std::min(w, h) * 0.5f));
    if (T.shape == 2) {   // восьмиугольник
        p[0] = { x + c, y };         p[1] = { x + w - c, y };
        p[2] = { x + w, y + c };     p[3] = { x + w, y + h - c };
        p[4] = { x + w - c, y + h }; p[5] = { x + c, y + h };
        p[6] = { x, y + h - c };     p[7] = { x, y + c };
        p[8] = p[0];
        return 8;
    }
    if (T.shape == 1) {   // срезаны правый верхний и левый нижний
        p[0] = { x, y };             p[1] = { x + w - c, y };
        p[2] = { x + w, y + c };     p[3] = { x + w, y + h };
        p[4] = { x + c, y + h };     p[5] = { x, y + h - c };
        p[6] = p[0];
        return 6;
    }
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
    if (n < 3 || n > 12) return;
    const SDL_FColor col{ c.r / 255.f, c.g / 255.f, c.b / 255.f, a / 255.f };
    SDL_Vertex v[12];
    for (int i = 0; i < n; ++i) v[i] = SDL_Vertex{ p[i], col, SDL_FPoint{ 0.f, 0.f } };
    int idx[30];
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

// Как Rect, но в стиле PRO CUTE углы скруглены (полоски и ползунки становятся «таблетками»).
inline void SoftRect(SDL_Renderer* r, float x, float y, float w, float h, Rgb c, Uint8 a = 255) {
    SetColor(r, c, a);
    const SDL_FRect q{ x, y, w, h };
    StyledFill(r, &q);
}

// Контур со срезанными углами, glow = «свечение» несколькими внешними контурами (число слоёв задаёт вид).
inline void CutOutline(SDL_Renderer* r, float x, float y, float w, float h, float cut,
    Rgb c, Uint8 a = 255, bool glow = false) {
    if (Cute()) { CuteOutline(r, x, y, w, h, CuteCutRadius(cut, w, h), c, a, glow); return; }
    const LookTraits& T = Traits();
    SDL_FPoint p[9];
    if (glow && !Minimal) {
        for (int i = T.glow; i >= 1; --i) {
            const float k = std::min(1.f, T.glowGain * 0.44f / (float(i) + 0.5f));
            SetColor(r, c, Uint8(a * k));
            const int n = CutPoly(p, x - float(i), y - float(i), w + 2.f * float(i), h + 2.f * float(i), cut + float(i));
            SDL_RenderLines(r, p, n + 1);
        }
    }
    SetColor(r, c, a);
    int n = CutPoly(p, x, y, w, h, cut);
    SDL_RenderLines(r, p, n + 1);
    if (T.doubleLine && !Minimal && w > 14.f && h > 14.f) {   // тонкая внутренняя линия
        SetColor(r, c, Uint8(a * 0.38f));
        n = CutPoly(p, x + 3.f, y + 3.f, w - 6.f, h - 6.f, std::max(0.f, cut - 2.f));
        SDL_RenderLines(r, p, n + 1);
    }
}

inline void CutPanel(SDL_Renderer* r, float x, float y, float w, float h, float cut,
    Rgb fill, Uint8 fillA, Rgb line, Uint8 lineA, bool glow = false) {
    if (Cute()) { CutePanel(r, x, y, w, h, CuteCutRadius(cut, w, h), fill, fillA, line, lineA, glow); return; }
    SDL_FPoint p[9];
    const int n = CutPoly(p, x, y, w, h, cut);
    FillPoly(r, p, n, fill, fillA);
    CutOutline(r, x, y, w, h, cut, line, lineA, glow);
}

// Залитый ромб (орнамент вида REGAL).
inline void Diamond(SDL_Renderer* r, float cx, float cy, float rad, Rgb c, Uint8 a = 255) {
    const SDL_FPoint d[4] = { { cx, cy - rad }, { cx + rad, cy }, { cx, cy + rad }, { cx - rad, cy } };
    FillPoly(r, d, 4, c, a);
}

// Маркер кнопки: треугольник в углу (HUD: справа сверху, BLADE: слева сверху) или ромб (REGAL).
inline void CornerMarker(SDL_Renderer* r, float x, float y, float w, float size, Rgb c, Uint8 a = 255) {
    if (Minimal || !Traits().marker) return;
    if (Look == LookRegal) { Diamond(r, x + w - size, y + size, size * 0.34f, c, a); return; }
    if (Traits().shape == 1) {
        const SDL_FPoint t[3] = { { x, y }, { x + size, y }, { x, y + size } };
        FillPoly(r, t, 3, c, a);
        return;
    }
    const SDL_FPoint t[3] = { { x + w - size, y }, { x + w, y }, { x + w, y + size } };
    FillPoly(r, t, 3, c, a);
}

// Уголки-скобки по периметру прямоугольника (BLADE: длиннее, REGAL: короче и с ромбами в углах).
inline void Brackets(SDL_Renderer* r, float x, float y, float w, float h, float len,
    Rgb c, Uint8 a = 255) {
    if (Minimal || !Traits().brackets) return;
    if (Look == LookBlade) len *= 1.6f;
    else if (Look == LookRegal) len *= 0.7f;
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
    if (Look == LookRegal) {
        const float d = 3.2f;
        Diamond(r, x, y, d, c, a);         Diamond(r, x + w, y, d, c, a);
        Diamond(r, x, y + h, d, c, a);     Diamond(r, x + w, y + h, d, c, a);
    }
}

// Фон окна: градиент + оформление вида (HUD: строки развёртки, BLADE: диагональная штриховка,
// NEON: сетка на горизонте, REGAL: ромбовая решётка).
inline void DrawBackground(SDL_Renderer* r, int w, int h) {
    SDL_SetRenderDrawColor(r, BgBottom.r, BgBottom.g, BgBottom.b, 255);
    SDL_RenderClear(r);
    if (Minimal) return;   // один сплошной цвет, без градиента и украшений
    if (Cute()) {          // градиент, случайные обои, мягкая вуаль и виньетка
        const CuteScene::Col veil{ Uint8(BgBottom.r * 0.25f + BgTop.r * 0.75f), Uint8(BgBottom.g * 0.25f + BgTop.g * 0.75f),
                                   Uint8(BgBottom.b * 0.25f + BgTop.b * 0.75f) };
        CuteScene::DrawBackdrop(r, w, h, CuteScene::Col{ BgTop.r, BgTop.g, BgTop.b },
            CuteScene::Col{ BgBottom.r, BgBottom.g, BgBottom.b }, veil, CuteWallpaper);
        return;
    }
    const SDL_FColor top{ BgTop.r / 255.f, BgTop.g / 255.f, BgTop.b / 255.f, 1.f };
    const SDL_FColor bot{ BgBottom.r / 255.f, BgBottom.g / 255.f, BgBottom.b / 255.f, 1.f };
    const float fw = float(w), fh = float(h);
    const SDL_Vertex v[4] = {
        { { 0.f, 0.f }, top, { 0.f, 0.f } }, { { fw, 0.f }, top, { 0.f, 0.f } },
        { { fw, fh }, bot, { 0.f, 0.f } },   { { 0.f, fh }, bot, { 0.f, 0.f } } };
    const int idx[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);

    if (Traits().scan) {
        SetColor(r, Rgb{ 0, 0, 0 }, 34);
        for (int y = 1; y < h; y += 4) SDL_RenderLine(r, 0.f, float(y), fw, float(y));
    }
    if (Look == LookBlade) {
        SetColor(r, Rgb{ 0, 0, 0 }, 34);
        for (float d = -fh; d < fw; d += 20.f) SDL_RenderLine(r, d, 0.f, d + fh, fh);
    } else if (Look == LookNeon) {
        const float y0 = fh * 0.68f, cx = fw * 0.5f;
        SetColor(r, Coral, 30);
        for (int i = -10; i <= 10; ++i)
            SDL_RenderLine(r, cx + float(i) * fw * 0.03f, y0, cx + float(i) * fw * 0.16f, fh);
        for (int k = 1; k <= 8; ++k) {
            const float t = float(k) / 8.f;
            const float y = y0 + (fh - y0) * t * t;
            SDL_RenderLine(r, 0.f, y, fw, y);
        }
        SetColor(r, Coral, 70);
        SDL_RenderLine(r, 0.f, y0, fw, y0);
    } else if (Look == LookRegal) {
        SetColor(r, Cyan, 14);
        for (float d = -fh; d < fw; d += 34.f) {
            SDL_RenderLine(r, d, 0.f, d + fh, fh);
            SDL_RenderLine(r, d + fh, 0.f, d, fh);
        }
    }
}

// Лепестки сакуры, боке, звёздочки и сердечки: поверх фона, под интерфейсом (только вид CUTE).
inline void DrawBackgroundFx(SDL_Renderer* r, int w, int h) {
    if (!Cute()) return;
    CuteScene::DrawFalling(r, w, h);
}

// Точка на ломаной на расстоянии d вдоль периметра (замкнутой, p[n] = p[0]).
inline SDL_FPoint PointOnPoly(const SDL_FPoint* p, int n, float d) {
    float per = 0.f;
    for (int i = 0; i < n; ++i) per += std::hypot(p[i + 1].x - p[i].x, p[i + 1].y - p[i].y);
    if (per < 1.f) return p[0];
    d = std::fmod(d, per);
    if (d < 0.f) d += per;
    for (int i = 0; i < n; ++i) {
        const float len = std::hypot(p[i + 1].x - p[i].x, p[i + 1].y - p[i].y);
        if (d <= len && len > 0.f) {
            const float k = d / len;
            return { p[i].x + (p[i + 1].x - p[i].x) * k, p[i].y + (p[i + 1].y - p[i].y) * k };
        }
        d -= len;
    }
    return p[0];
}

// Рамка окна PRO CUTE: мягкое внутреннее свечение, двойной контур, «стежки» из точек, цветок сакуры сверху
// и бегущая искорка со шлейфом. Радиус согласован со скруглением самого окна (main.cpp: CurRadius).
inline void DrawCuteFrame(SDL_Renderer* r, int w, int h, bool fullscreen) {
    using namespace CuteGfx;
    const float fw = float(w), fh = float(h);
    const double t = double(SDL_GetTicks()) / 1000.0;
    const float m = fullscreen ? 5.f : 6.f;
    const float x = m, y = m, ww = fw - 2.f * m, hh = fh - 2.f * m;
    if (ww < 80.f || hh < 80.f) return;
    const float R = std::min(20.f, std::min(ww, hh) * 0.25f);
    const float pulse = 0.82f + 0.18f * float(std::sin(t * 1.6));

    // внутреннее свечение: ореол вокруг уменьшенного прямоугольника ложится вдоль рамки внутрь окна
    const float gi = 11.f;
    RoundGlow(r, x + gi, y + gi, ww - 2.f * gi, hh - 2.f * gi, std::max(4.f, R - gi * 0.5f), int(gi),
        Cyan.r, Cyan.g, Cyan.b, Uint8(46.f * pulse));

    // контур: широкий мягкий и тонкий яркий
    RoundStroke(r, x, y, ww, hh, R, 3, Cyan.r, Cyan.g, Cyan.b, Uint8(70.f * pulse));
    RoundStroke(r, x + 1.f, y + 1.f, ww - 2.f, hh - 2.f, R - 1.f, 1, CyanHot.r, CyanHot.g, CyanHot.b, 200);

    // «стежки»: пунктир внутри рамки
    if (SDL_Texture* dot = DiscTex(r)) {
        const float ix = x + 8.f, iy = y + 8.f, iw = ww - 16.f, ih = hh - 16.f, ir = std::max(3.f, R - 7.f);
        const float per = CuteScene::RoundRectPerimeter(iw, ih, ir);
        const int n = std::max(8, int(per / 11.f));
        for (int i = 0; i < n; ++i) {
            const SDL_FPoint p = CuteScene::PointOnRoundRect(ix, iy, iw, ih, ir, per * float(i) / float(n));
            Sprite(r, dot, p.x, p.y, 3.f, 3.f, 0.0, Cyan.r, Cyan.g, Cyan.b, 110);
        }
        // бегущая искорка со шлейфом
        const float rper = CuteScene::RoundRectPerimeter(ww, hh, R);
        const float d0 = float(std::fmod(t * 90.0, double(rper)));
        for (int i = 0; i < 14; ++i) {
            const SDL_FPoint p = CuteScene::PointOnRoundRect(x, y, ww, hh, R, d0 - float(i) * 5.f);
            const float k = 1.f - float(i) / 14.f;
            Sprite(r, dot, p.x, p.y, 2.f + 4.f * k, 2.f + 4.f * k, 0.0, CoralHot.r, CoralHot.g, CoralHot.b, Uint8(200.f * k * k),
                SDL_BLENDMODE_ADD);
        }
        const SDL_FPoint head = CuteScene::PointOnRoundRect(x, y, ww, hh, R, d0);
        if (SDL_Texture* sp = SparkleTex(r))
            Sprite(r, sp, head.x, head.y, 15.f, 15.f, double(t * 40.0), 255, 240, 246, 240, SDL_BLENDMODE_ADD);
    }

    // цветок сакуры по центру верхней кромки, слегка покачивается
    if (SDL_Texture* bl = BlossomTex(r)) {
        const float s = (fw < 360.f) ? 18.f : 26.f;
        Sprite(r, bl, fw * 0.5f, y, s, s, 10.0 * std::sin(t * 0.9), 255, 255, 255, 245);
    }
}

// Рамка всего окна: срезанные углы, скобки, линейка слева, бегущий «скан» сверху
// и служебная строка в левом верхнем углу.
inline void DrawHudFrame(SDL_Renderer* r, int w, int h, bool fullscreen = false) {
    if (Minimal) return;
    if (Cute()) { DrawCuteFrame(r, w, h, fullscreen); return; }
    const float fw = float(w), fh = float(h);
    const float m = fullscreen ? 4.f : 5.f;
    const float t = float(SDL_GetTicks()) / 1000.f;

    // ---- BLADE: зеркальные срезы, двойная линия, длинные скобки, линейка справа, быстрая полоса ----
    if (Look == LookBlade) {
        const float cut = 16.f * Traits().cutMul;
        CutOutline(r, m, m, fw - 2 * m, fh - 2 * m, 16.f, Cyan, 150, false);
        Brackets(r, m + 3, m + 3, fw - 2 * (m + 3), fh - 2 * (m + 3), 14.f, Coral, 230);
        SetColor(r, Cyan, 70);
        for (float y = 48.f; y < fh - 40.f; y += 10.f) {
            const bool major = std::fmod(y - 48.f, 50.f) < 1.f;
            SDL_RenderLine(r, fw - m - 3, y, fw - m - 3 - (major ? 9.f : 4.f), y);
        }
        const float segLen = 120.f;
        const float span = std::max(1.f, fw - 2 * m - cut - segLen);
        const float sx = m + cut + (span - std::fmod(t * 220.f, span));   // бежит справа налево
        Rect(r, sx, m - 1.f, segLen, 3.f, CyanHot, 235);
        Rect(r, sx + segLen, m - 1.f, 26.f, 3.f, Cyan, 70);
        const float labelW = std::max(0.f, fw * 0.28f);
        if (labelW > 80.f)
            DrawUtf8TextFit(r, "BLADE // ONLINE", m + 8.f, m + 11.f, labelW, 8, false, Cyan.r, Cyan.g, Cyan.b, 120);
        return;
    }

    // ---- NEON: дышащее свечение, внутренняя линия акцента, искра бежит по периметру ----
    if (Look == LookNeon) {
        const float pulse = 0.78f + 0.22f * std::sin(t * 1.8f);
        const float x = m, y = m, ww = fw - 2 * m, hh = fh - 2 * m;
        CutOutline(r, x, y, ww, hh, 20.f, Cyan, Uint8(200.f * pulse), true);
        CutOutline(r, x + 7.f, y + 7.f, ww - 14.f, hh - 14.f, 16.f, Coral, Uint8(80.f * pulse), false);
        SDL_FPoint p[9];
        const int n = CutPoly(p, x, y, ww, hh, 20.f);
        const float d0 = t * 280.f;
        for (int i = 0; i < 20; ++i) {
            const SDL_FPoint q = PointOnPoly(p, n, d0 - float(i) * 6.f);
            const Uint8 al = Uint8(255.f * (1.f - float(i) / 20.f));
            if (i == 0) Rect(r, q.x - 4.f, q.y - 4.f, 8.f, 8.f, Coral, 70);
            Rect(r, q.x - 1.5f, q.y - 1.5f, 3.f, 3.f, i == 0 ? CyanHot : Coral, al);
        }
        return;
    }

    // ---- REGAL: золотая двойная рамка, ромбы на срезах и по центру сверху и снизу ----
    if (Look == LookRegal) {
        const float x = m, y = m, ww = fw - 2 * m, hh = fh - 2 * m;
        CutOutline(r, x, y, ww, hh, 20.f, Cyan, 150, true);
        SDL_FPoint p[9];
        CutPoly(p, x, y, ww, hh, 20.f);
        const int diag[4][2] = { { 7, 0 }, { 1, 2 }, { 3, 4 }, { 5, 6 } };   // четыре среза
        for (const auto& d : diag) {
            const float mx = (p[d[0]].x + p[d[1]].x) * 0.5f, my = (p[d[0]].y + p[d[1]].y) * 0.5f;
            Diamond(r, mx, my, 4.5f, Coral, 235);
            Diamond(r, mx, my, 8.f, Coral, 60);
        }
        const float cx = fw * 0.5f;
        for (int side = 0; side < 2; ++side) {
            const float yy = side == 0 ? y : y + hh;
            SetColor(r, Coral, 190);
            SDL_RenderLine(r, cx - 56.f, yy, cx - 11.f, yy);
            SDL_RenderLine(r, cx + 11.f, yy, cx + 56.f, yy);
            Diamond(r, cx, yy, 5.f, Coral, 235);
        }
        Brackets(r, m + 3, m + 3, fw - 2 * (m + 3), fh - 2 * (m + 3), 14.f, Coral, 200);
        const float labelW = std::max(0.f, fw * 0.28f);
        if (labelW > 80.f)
            DrawUtf8TextFit(r, "ROSE KNIGHT // AT YOUR SERVICE", m + 26.f, m + 13.f, labelW, 8, false,
                Coral.r, Coral.g, Coral.b, 140);
        return;
    }

    // ---- HUD (классика) ----
    const float cut = 18.f;

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
        if (m >= 11) break;
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
    if (Minimal) {   // тихий переход: тёмная заливка плавно уходит
        Rect(r, 0.f, 0.f, fw, fh, BgBottom, Uint8(255.f * (1.f - EaseOut(p)) * 0.85f));
        return;
    }
    if (Cute()) {   // розовая дымка рассеивается, лепестки сдувает ветром, в центре крутится цветок
        using namespace CuteGfx;
        const float k = 1.f - EaseOut(Clamp01(p * 1.15f));
        Rect(r, 0.f, 0.f, fw, fh, Mix(BgTop, Rgb{ 255, 214, 232 }, 0.30f), Uint8(238.f * k));
        if (SDL_Texture* petal = PetalTex(r)) {
            for (Uint32 i = 0; i < 28u; ++i) {
                const float delay = Hash01(i * 13u + 1u) * 0.30f;
                const float q = Clamp01((p - delay) / 0.70f);
                if (q <= 0.f || q >= 1.f) continue;
                const float sz = 26.f + 34.f * Hash01(i * 13u + 2u);
                const float px = -sz + (fw + 2.f * sz) * q;
                const float py = fh * Hash01(i * 13u + 3u) + 26.f * std::sin(q * 9.f + float(i));
                Sprite(r, petal, px, py, sz * 0.72f, sz, double(q * 540.f) + double(i) * 47.0, 255, 226, 236,
                    Uint8(235.f * std::sin(q * kPi)));
            }
        }
        if (p < 0.7f) {
            const float cp = Clamp01(p / 0.7f), pop = 1.f - (1.f - cp) * (1.f - cp);
            if (SDL_Texture* bl = BlossomTex(r)) {
                const float s = 64.f * (0.70f + 0.30f * pop);
                Sprite(r, bl, fw * 0.5f, fh * 0.5f - 18.f, s, s, double(el) * 120.0, 255, 255, 255, Uint8(255.f * (1.f - cp * cp)));
            }
            const std::string label = std::string("LOADING.. ") + std::to_string(int(cp * 100.f)) + "%";
            const int font = fw >= 420.f ? 16 : 8;
            DrawUtf8Text(r, label, (fw - Utf8TextWidth(label, font)) * 0.5f, fh * 0.5f + 30.f, font, CyanHot.r, CyanHot.g, CyanHot.b,
                Uint8(255.f * (1.f - cp * cp)));
        }
        return;
    }
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
        static const char* const kWords[LookCount] = { "ACCESSING..", "ACCESSING..", "BREACHING..", "SYNCING..", "SUMMONING..", "LOADING.." };
        const std::string label = std::string(kWords[(Look >= 0 && Look < LookCount) ? Look : 1]) + " " + std::to_string(pct) + "%";
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

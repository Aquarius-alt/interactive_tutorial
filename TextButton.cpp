#include "TextButton.h"
#include "Text.h"
#include "Theme.h"
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace {
    const float kLabelPad = 12.f;   // отступ подписи от краёв кнопки
    const float kTextScale = 2.0f;
    const float kHoverSec = 0.18f;  // время заполнения кораллом
    const Uint64 kGlitchMs = 190;   // глитч после начала наведения
    const Uint64 kDecodeMs = 260;   // «расшифровка» подписи
    const Uint64 kFlashMs = 170;    // вспышка при клике

    std::vector<std::string> SplitUtf8(const std::string& s) {
        std::vector<std::string> out;
        for (size_t i = 0; i < s.size();) {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            const size_t len = c < 0x80 ? 1 : ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : 4));
            out.push_back(s.substr(i, len));
            i += len;
        }
        return out;
    }

    // Подпись «собирается» слева направо из случайных символов (цифры и знаки есть в Press Start 2P).
    std::string Decode(const std::string& label, float p, Uint32 seed) {
        static const char kGlyphs[] = "01#/<>%&$";
        const std::vector<std::string> ch = SplitUtf8(label);
        const size_t done = static_cast<size_t>(p * float(ch.size()));
        std::string out;
        for (size_t i = 0; i < ch.size(); ++i) {
            if (i < done || ch[i] == " ") out += ch[i];
            else out += kGlyphs[Theme::Hash32(seed + Uint32(i) * 7919u) % 9u];
        }
        return out;
    }
}

namespace {
    // Стиль PRO CUTE. Кнопка с подписью - «леденец»: тень-подушка, ягодная заливка, светлый блик сверху,
    // двойной контур; при наведении заливка розовеет, появляются ореол и искорка, кнопка чуть «вздувается»,
    // при клике сжимается и вспыхивает. Без подписи (поля ввода, панели) - спокойная скруглённая рамка.
    void DrawCuteControl(SDL_Renderer* r, const TextButton& b, bool field, float t, Uint64 now) {
        using Theme::Mix;
        const Theme::Rgb ring = Mix(Theme::Cyan, Theme::CyanHot, t);
        float x = b.x, y = b.y, w = b.w, h = b.h;

        if (field) {
            const float R = std::min(h * 0.5f, std::min(22.f, std::max(9.f, std::min(w, h) * 0.30f)));
            if (t > 0.01f) CuteGfx::RoundGlow(r, x, y, w, h, R, 8, Theme::Coral.r, Theme::Coral.g, Theme::Coral.b, Uint8(90.f * t));
            CuteGfx::RoundFill(r, x, y, w, h, R, Theme::Panel.r, Theme::Panel.g, Theme::Panel.b, 205);
            CuteGfx::RoundStroke(r, x, y, w, h, R, t > 0.5f ? 2 : 1, ring.r, ring.g, ring.b, Uint8(150.f + 105.f * t));
            return;
        }

        const bool clicking = b.clickTick && now - b.clickTick < kFlashMs;
        const float ck = clicking ? 1.f - float(now - b.clickTick) / float(kFlashMs) : 0.f;
        const float grow = 1.5f * t - 2.0f * ck;                 // вздувается при наведении, сжимается при клике
        x -= grow; y -= grow; w += 2.f * grow; h += 2.f * grow;
        const float R = std::min(h * 0.5f, 26.f);

        CuteGfx::RoundFill(r, x, y + 3.f, w, h, R, 22, 6, 32, 70);                                      // тень-подушка
        if (t > 0.01f) CuteGfx::RoundGlow(r, x, y, w, h, R, 10, Theme::Coral.r, Theme::Coral.g, Theme::Coral.b, Uint8(120.f * t));
        const Theme::Rgb fill = Mix(Mix(Theme::Panel, Theme::PanelHi, 0.45f), Theme::Coral, 0.62f * t);
        CuteGfx::RoundFill(r, x, y, w, h, R, fill.r, fill.g, fill.b, 240);
        if (w > h + 12.f)                                                                                // «желейный» блик
            CuteGfx::RoundFill(r, x + R * 0.7f, y + 3.f, w - R * 1.4f, std::max(3.f, h * 0.30f), std::max(1.5f, h * 0.15f),
                255, 255, 255, Uint8(34.f + 36.f * t));
        CuteGfx::RoundStroke(r, x, y, w, h, R, 2, ring.r, ring.g, ring.b, 235);
        if (ck > 0.f) CuteGfx::RoundFill(r, x, y, w, h, R, 255, 255, 255, Uint8(150.f * ck));
        if (t > 0.05f) {                                                                                 // искорка в углу
            const float tw = 0.75f + 0.25f * std::sin(float(now) * 0.012f);
            const float sz = (11.f + 6.f * t) * tw;
            CuteGfx::Sprite(r, CuteGfx::SparkleTex(r), x + w - R * 0.85f, y + 3.f, sz, sz, 0.0, 255, 240, 246, Uint8(235.f * t),
                SDL_BLENDMODE_ADD);
        }
    }
}

// Стиль «кибердек»: панель со срезанными углами и маркером-треугольником в углу.
// Анимации: плавная коралловая заливка слева направо, глитч-«призраки» и расшифровка подписи
// в начале наведения, световая вспышка при клике.
// Кнопка без подписи (поля ввода, панели) рисуется как спокойная рамка без маркера.
void TextButton_Draw(SDL_Renderer* r, TextButton& b) {
    using namespace Theme;
    const Uint64 now = SDL_GetTicks();
    const bool field = !b.label || !b.label[0];
    const bool hot = b.hovered;

    // время и плавное наведение
    if (b.lastTick == 0) b.lastTick = now;
    const float dt = std::min(float(now - b.lastTick) / 1000.f, 0.05f);
    b.lastTick = now;
    if (hot && !b.wasHovered) b.hoverStart = now;
    b.wasHovered = hot;
    const float step = dt / kHoverSec;
    b.hoverT = hot ? std::min(1.f, b.hoverT + step) : std::max(0.f, b.hoverT - step);
    const float t = EaseOut(b.hoverT);
    const Uint64 hAge = now - b.hoverStart;

    const float cut = std::min(10.f, std::min(b.w, b.h) * 0.28f);

    if (Cute()) {
        DrawCuteControl(r, b, field, t, now);
        if (!field) {
            const Rgb tc = Mix(Ink, Rgb{ 255, 255, 255 }, t);
            DrawUtf8TextFit(r, b.label ? b.label : "", b.x + kLabelPad, b.y + b.h * 0.5f, b.w - kLabelPad * 2.f,
                static_cast<int>(8.f * kTextScale), true, tc.r, tc.g, tc.b, 255);
        }
        return;
    }

    // Минимальный стиль: плоская прямоугольная кнопка, тонкая рамка, при наведении фон светлеет.
    // Без глитча, расшифровки подписи, маркера, штрихов и свечения.
    if (Minimal) {
        const Rgb fill = Mix(Panel, PanelHi, t);
        Rect(r, b.x, b.y, b.w, b.h, fill, 255);
        if (b.clickTick && now - b.clickTick < kFlashMs) {
            const float k = 1.f - float(now - b.clickTick) / float(kFlashMs);
            Rect(r, b.x, b.y, b.w, b.h, CyanHot, Uint8(40 * k));
        }
        const Rgb line = Mix(Rgb{ Uint8(Cyan.r * 0.45f), Uint8(Cyan.g * 0.45f), Uint8(Cyan.b * 0.45f) }, CyanHot, t);
        SetColor(r, line, 255);
        const SDL_FRect frame{ b.x + 0.5f, b.y + 0.5f, b.w - 1.f, b.h - 1.f };
        SDL_RenderRect(r, &frame);
        if (!field) {
            const Rgb tc = Mix(Cyan, CyanHot, t);
            DrawUtf8TextFit(r, b.label ? b.label : "", b.x + kLabelPad, b.y + b.h * 0.5f, b.w - kLabelPad * 2.f,
                static_cast<int>(8.f * kTextScale), true, tc.r, tc.g, tc.b, 255);
        }
        return;
    }

    if (field) {
        CutPanel(r, b.x, b.y, b.w, b.h, cut, Panel, 205, Mix(Cyan, CyanHot, t), Uint8(150 + 105 * t), hot);
        // коралловый штрих у нижнего края «выезжает» при наведении или фокусе
        if (Traits().underline) {
        const float len = std::min(24.f, b.w * 0.2f) + t * std::min(60.f, b.w * 0.4f);
        Rect(r, b.x + cut + 2.f, b.y + b.h - 1.f, len, 2.f, Coral, Uint8(170 + 85 * t));
        }
        return;
    }

    // основа и коралловая заливка до линии t
    SDL_FPoint poly[9];
    const int pn = CutPoly(poly, b.x, b.y, b.w, b.h, cut);
    FillPoly(r, poly, pn, Panel, 215);
    if (t > 0.01f) {
        const float edge = b.x + b.w * t;
        SDL_FPoint clipped[12];
        const int n = ClipPolyLeft(poly, pn, edge, clipped);
        if (n >= 3) FillPoly(r, clipped, n, Coral, 235);
        if (t < 0.99f) {   // яркий передний край заливки
            Rect(r, edge - 1.f, b.y + 1.f, 2.f, b.h - 2.f, CyanHot, 230);
        }
    }

    // глитч-«призраки» контура в начале наведения
    if (Traits().glitch && hot && hAge < kGlitchMs) {
        const float k = 1.f - float(hAge) / float(kGlitchMs);
        const Uint32 fr = Uint32(now / 35);
        const float off = 2.f + 3.f * Hash01(fr);
        CutOutline(r, b.x - off, b.y, b.w, b.h, cut, Coral, Uint8(140 * k));
        CutOutline(r, b.x + off, b.y, b.w, b.h, cut, Cyan, Uint8(140 * k));
        for (int i = 0; i < 2; ++i) {   // смещённые полоски
            const float sy = b.y + Hash01(fr * 3u + Uint32(i)) * (b.h - 4.f);
            const float sx = (Hash01(fr * 5u + Uint32(i)) - 0.5f) * 14.f;
            Rect(r, b.x + sx, sy, b.w, 2.f, CyanHot, Uint8(120 * k));
        }
    }

    const Rgb line = Mix(Cyan, CoralHot, t);
    CutOutline(r, b.x, b.y, b.w, b.h, cut, line, 255, true);
    CornerMarker(r, b.x, b.y, b.w, std::min(10.f, b.h * 0.3f), Mix(Cyan, CyanHot, t), 255);
    // нижний штрих растёт при наведении
    if (Traits().underline)
        Rect(r, b.x + cut + 2.f, b.y + b.h - 4.f, std::min(18.f, b.w * 0.15f) + t * std::min(30.f, b.w * 0.25f),
            2.f, Mix(Coral, CyanHot, t), 210);

    // вспышка при клике
    if (b.clickTick && now - b.clickTick < kFlashMs) {
        const float k = 1.f - float(now - b.clickTick) / float(kFlashMs);
        FillPoly(r, poly, pn, CyanHot, Uint8(190 * k));
    }

    // Подпись: в начале наведения «расшифровывается», дальше обычная.
    // Всегда внутри кнопки: сначала мельче (16 -> 8 px), потом обрезаем с "..".
    std::string label = b.label ? b.label : "";
    if (Traits().glitch && hot && hAge < kDecodeMs) label = Decode(label, float(hAge) / float(kDecodeMs), Uint32(now / 50));
    const Rgb tc = Mix(Cyan, Rgb{ 255, 240, 235 }, t);
    DrawUtf8TextFit(r, label, b.x + kLabelPad, b.y + b.h * 0.5f, b.w - kLabelPad * 2.f,
        static_cast<int>(8.f * kTextScale), true, tc.r, tc.g, tc.b, 255);
}

static bool Inside(const TextButton& b, float mx, float my) {
    return mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h;
}

bool TextButton_Handle(TextButton& b, const SDL_Event& e) {
    if (e.type == SDL_EVENT_MOUSE_MOTION)
        b.hovered = Inside(b, e.motion.x, e.motion.y);
    if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE)
        b.hovered = false;
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        const bool hit = Inside(b, e.button.x, e.button.y);
        if (hit) b.clickTick = SDL_GetTicks();
        return hit;
    }
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
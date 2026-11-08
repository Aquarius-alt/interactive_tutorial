#pragma once
// Заставка при запуске:
//   1) из темноты плавно проявляется иконка приложения;
//   2) под ней «печатается» подпись "by Aqua femboy~" (с мигающим курсором);
//   3) экран глючит в стиле Blackwall из Cyberpunk 2077 (рваные полосы, RGB-сдвиг,
//      красный шум-«пиксели», мерцание) и сквозь глитч проступает главное окно.
// Только заголовок (inline), в проект добавлять ничего не нужно (нужен SplashLogoData.h рядом).
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include "Text.h"
#include "Theme.h"
#include "SplashLogoData.h"

namespace Splash {

// ---- Тайминги (секунды от первого кадра) ----
constexpr float kLogoStart   = 0.50f;   // начало проявления иконки
constexpr float kLogoFadeSec = 1.50f;   // длительность проявления
constexpr float kTextStart   = 2.10f;   // начало печати подписи
constexpr float kCharSec     = 0.075f;  // секунд на символ
constexpr float kGlitchStart = 3.90f;   // начало глитча (если не пропущена заставка)
constexpr float kGlitchSec   = 1.35f;   // длительность глитча
constexpr const char* kCaption = "by Aqua femboy~";

inline bool          g_started = false;
inline bool          g_done = false;
inline Uint64        g_t0 = 0;
inline float         g_glitchAt = kGlitchStart;
inline SDL_Texture*  g_logo = nullptr;       // иконка 256x256
inline SDL_Texture*  g_splashTex = nullptr;  // кадр заставки (цель рендера)
inline SDL_Texture*  g_sceneTex = nullptr;   // кадр главного окна (цель рендера)
inline SDL_Renderer* g_owner = nullptr;
inline int           g_texW = 0, g_texH = 0;

inline bool Active() { return !g_done; }

// Клик / клавиша: сразу переходим к глитчу (если он ещё не начался).
inline void Skip() {
    if (g_done || !g_started) return;
    const float el = float(SDL_GetTicks() - g_t0) / 1000.f;
    if (el < g_glitchAt) g_glitchAt = el;
}

inline void FreeTargets() {
    if (g_splashTex) SDL_DestroyTexture(g_splashTex);
    if (g_sceneTex) SDL_DestroyTexture(g_sceneTex);
    g_splashTex = g_sceneTex = nullptr;
    g_texW = g_texH = 0;
}

// Вызвать перед SDL_DestroyRenderer
inline void Destroy() {
    FreeTargets();
    if (g_logo) SDL_DestroyTexture(g_logo);
    g_logo = nullptr;
    g_owner = nullptr;
}

inline void EnsureResources(SDL_Renderer* r, int w, int h, bool needTargets) {
    if (g_owner != r) {   // новый рендерер: всё пересоздаём
        FreeTargets();
        if (g_logo) SDL_DestroyTexture(g_logo);
        g_logo = nullptr;
        g_owner = r;
    }
    if (!g_logo) {
        g_logo = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
            kSplashLogoSize, kSplashLogoSize);
        if (g_logo) {
            SDL_UpdateTexture(g_logo, nullptr, kSplashLogoPixels, kSplashLogoSize * 4);
            SDL_SetTextureBlendMode(g_logo, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(g_logo, SDL_SCALEMODE_LINEAR);
        }
    }
    if (needTargets && (!g_splashTex || !g_sceneTex || g_texW != w || g_texH != h)) {
        FreeTargets();
        g_splashTex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
        g_sceneTex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
        for (SDL_Texture* t : { g_splashTex, g_sceneTex }) {
            if (!t) continue;
            SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
            SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
        }
        g_texW = w; g_texH = h;
    }
}

inline float Smooth(float t) { t = Theme::Clamp01(t); return t * t * (3.f - 2.f * t); }

// Кадр заставки: чёрный фон, иконка, печатающаяся подпись.
inline void DrawSplashFrame(SDL_Renderer* r, int w, int h, float el, float glitchAt) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 4, 2, 5, 255);
    SDL_RenderClear(r);

    const float fw = float(w), fh = float(h);
    const float size = std::max(64.f, std::min(240.f, std::min(fw, fh) * 0.34f));
    const int   font = fw >= 360.f ? 16 : 8;
    const std::string full = kCaption;
    const float textW = Utf8TextWidth(full, font);

    // иконка + подпись как единый блок по центру окна
    const float blockH = size + 26.f + float(font);
    const float logoY = (fh - blockH) * 0.5f;
    const float cx = fw * 0.5f;

    // появление из темноты: альфа + лёгкое «подплывание» масштаба
    const float k = Smooth((el - kLogoStart) / kLogoFadeSec);
    if (g_logo && k > 0.f) {
        const float s = size * (0.93f + 0.07f * k);
        const SDL_FRect dst{ cx - s * 0.5f, logoY + (size - s) * 0.5f, s, s };
        // мягкое красное свечение (увеличенная слабая копия)
        const float gs = s * 1.14f;
        const SDL_FRect glow{ cx - gs * 0.5f, logoY + (size - gs) * 0.5f, gs, gs };
        SDL_SetTextureAlphaMod(g_logo, Uint8(255.f * k * 0.14f));
        SDL_RenderTexture(r, g_logo, nullptr, &glow);
        SDL_SetTextureAlphaMod(g_logo, Uint8(255.f * k));
        SDL_RenderTexture(r, g_logo, nullptr, &dst);
    }

    // подпись: печатается по символу, курсор-блок мигает
    if (el >= kTextStart) {
        const int total = int(full.size());
        const int n = std::min(total, int((el - kTextStart) / kCharSec) + 1);
        const std::string typed = full.substr(0, size_t(n));
        const float tx = cx - textW * 0.5f;
        const float ty = logoY + size + 26.f;
        // кеш текста создаёт текстуру на каждую подстроку: для 15 символов это нормально
        DrawUtf8Text(r, typed, tx, ty, font, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 255);

        const bool typing = n < total;
        const bool blinkOn = typing || (int(el * 2.6f) % 2 == 0);
        const float fade = el < glitchAt ? 1.f : 0.f;
        if (blinkOn && fade > 0.f) {
            const float curX = tx + Utf8TextWidth(typed, font) + 2.f;
            Theme::Rect(r, curX, ty - 1.f, float(font) * 0.6f, float(font) + 2.f, Theme::CyanHot, 230);
        }
    }

    // лёгкие строки развёртки, как на остальных экранах
    if (!Theme::Minimal) {
        Theme::SetColor(r, Theme::Rgb{ 0, 0, 0 }, 40);
        for (int y = 1; y < h; y += 4) SDL_RenderLine(r, 0.f, float(y), fw, float(y));
    }
}

// Нарисовать горизонтальную полосу [y, y+bh) текстуры со сдвигом dx (с «заворотом» по краям).
inline void DrawBand(SDL_Renderer* r, SDL_Texture* tex, int w, float y, float bh, float dx,
    Uint8 cr, Uint8 cg, Uint8 cb, SDL_BlendMode mode) {
    SDL_SetTextureColorMod(tex, cr, cg, cb);
    SDL_SetTextureBlendMode(tex, mode);
    const SDL_FRect src{ 0.f, y, float(w), bh };
    SDL_FRect dst{ dx, y, float(w), bh };
    SDL_RenderTexture(r, tex, &src, &dst);
    if (dx > 0.5f)       { dst.x = dx - float(w); SDL_RenderTexture(r, tex, &src, &dst); }
    else if (dx < -0.5f) { dst.x = dx + float(w); SDL_RenderTexture(r, tex, &src, &dst); }
}

// Склейка двух кадров в стиле Blackwall. p = 0..1 — прогресс глитча, f — номер «кадра глитча» (30 в сек).
inline void DrawGlitch(SDL_Renderer* r, int w, int h, float p, Uint32 f) {
    using Theme::Hash01;
    using Theme::Hash32;
    const float fw = float(w), fh = float(h);
    const float I = std::sin(3.14159265f * Theme::Clamp01(p));        // сила глитча: 0 -> 1 -> 0
    const float q = Theme::Clamp01((p - 0.12f) / 0.76f);
    const float R = p >= 0.92f ? 1.f : q * q * (3.f - 2.f * q);       // доля полос, показывающих приложение

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    // 1) рваные горизонтальные полосы из двух кадров, со сдвигом и RGB-расщеплением
    float y = 0.f;
    Uint32 b = 0;
    while (y < fh) {
        const Uint32 s = Hash32(f * 7919u + b * 104729u + 17u);
        float bh = 2.f + std::floor(Hash01(s ^ 0x9Eu) * (6.f + fh * 0.09f * I));
        if (y + bh > fh) bh = fh - y;

        SDL_Texture* src = (Hash01(s) < R) ? g_sceneTex : g_splashTex;
        float dx = 0.f;
        if (Hash01(s ^ 0xA5u) < I * 0.65f)
            dx = std::floor((Hash01(s ^ 0x5Au) - 0.5f) * 2.f * fw * 0.20f * I);

        if (Hash01(s ^ 0x33u) < I * 0.40f) {
            // RGB-сдвиг: три аддитивных прохода по каналам
            const float sp = 3.f + Hash01(s ^ 0x77u) * 20.f * I;
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
            const SDL_FRect bg{ 0.f, y, fw, bh };
            SDL_RenderFillRect(r, &bg);
            DrawBand(r, src, w, y, bh, dx - sp, 255, 0, 0, SDL_BLENDMODE_ADD);
            DrawBand(r, src, w, y, bh, dx,      0, 255, 0, SDL_BLENDMODE_ADD);
            DrawBand(r, src, w, y, bh, dx + sp, 0, 0, 255, SDL_BLENDMODE_ADD);
        } else {
            DrawBand(r, src, w, y, bh, dx, 255, 255, 255, SDL_BLENDMODE_NONE);
        }
        y += bh;
        ++b;
    }
    // вернуть текстурам нормальные режимы
    for (SDL_Texture* t : { g_splashTex, g_sceneTex }) {
        SDL_SetTextureColorMod(t, 255, 255, 255);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
    }

    // 2) красный «пиксельный» шум Blackwall: блоки по сетке 8 px
    const int blocks = int(70.f * I) + (p < 0.9f ? 4 : 0);
    for (int i = 0; i < blocks; ++i) {
        const Uint32 s = Hash32(f * 2654435761u + Uint32(i) * 40503u + 99u);
        const float bw = 8.f * float(1 + int(Hash01(s) * 10.f));
        const float bhh = 8.f * float(1 + int(Hash01(s ^ 0x11u) * 3.f));
        const float bx = std::floor(Hash01(s ^ 0x22u) * fw / 8.f) * 8.f;
        const float by = std::floor(Hash01(s ^ 0x44u) * fh / 8.f) * 8.f;
        const float c = Hash01(s ^ 0x88u);
        Theme::Rgb col = c < 0.62f ? Theme::Rgb{ 255, 0, 30 }
                       : c < 0.88f ? Theme::Rgb{ 24, 0, 4 }
                       :             Theme::Rgb{ 255, 150, 150 };
        Theme::Rect(r, bx, by, bw, bhh, col, Uint8(90.f + 150.f * Hash01(s ^ 0xCCu)));
    }

    // 3) тонкие красные линии-разрывы на всю ширину
    const int lines = int(9.f * I);
    for (int i = 0; i < lines; ++i) {
        const Uint32 s = Hash32(f * 374761393u + Uint32(i) * 668265263u + 5u);
        Theme::Rect(r, 0.f, std::floor(Hash01(s) * fh), fw, 1.f + std::floor(Hash01(s ^ 0x1u) * 3.f),
            Theme::Rgb{ 255, 20, 40 }, 200);
    }

    // 4) красный оттенок на пике, вспышка в начале и редкие провалы яркости (мерцание)
    Theme::Rect(r, 0.f, 0.f, fw, fh, Theme::Rgb{ 255, 0, 20 }, Uint8(46.f * I));
    if (p < 0.05f) Theme::Rect(r, 0.f, 0.f, fw, fh, Theme::Rgb{ 255, 190, 190 }, Uint8(110.f * (1.f - p / 0.05f)));
    if (Hash01(f * 13u + 7u) < 0.20f * I) Theme::Rect(r, 0.f, 0.f, fw, fh, Theme::Rgb{ 0, 0, 0 }, 130);
}

// Рисует кадр заставки. Возвращает true, если кадр собран (вызывающий делает SDL_RenderPresent),
// и false, когда заставка закончилась — тогда рисуйте обычный кадр.
// drawScene — функция, рисующая главное окно во ТЕКУЩУЮ цель рендера (без SDL_RenderPresent).
inline bool Render(SDL_Renderer* r, void (*drawScene)()) {
    if (g_done) return false;
    if (!g_started) { g_started = true; g_t0 = SDL_GetTicks(); }

    int w = 0, h = 0;
    SDL_GetCurrentRenderOutputSize(r, &w, &h);
    if (w < 2 || h < 2) return true;

    const float el = float(SDL_GetTicks() - g_t0) / 1000.f;
    const float p = (el - g_glitchAt) / kGlitchSec;

    if (p >= 1.f) {
        g_done = true;
        FreeTargets();
        return false;   // глитч закончился: дальше обычная отрисовка
    }

    if (p < 0.f) {      // фаза 1-2: рисуем прямо на экран
        EnsureResources(r, w, h, false);
        DrawSplashFrame(r, w, h, el, g_glitchAt);
        return true;
    }

    // фаза 3: заставка и приложение рисуются в текстуры и склеиваются глитчем
    EnsureResources(r, w, h, true);
    if (!g_splashTex || !g_sceneTex) { g_done = true; return false; }   // нет render target: просто показываем приложение

    SDL_SetRenderTarget(r, g_splashTex);
    DrawSplashFrame(r, w, h, el, g_glitchAt);
    SDL_SetRenderTarget(r, g_sceneTex);
    drawScene();
    SDL_SetRenderTarget(r, nullptr);

    DrawGlitch(r, w, h, p, Uint32(el * 30.f));
    return true;
}

} // namespace Splash

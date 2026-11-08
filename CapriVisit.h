#pragma once
// Капри заглядывает раз в 30 минут работы приложения и говорит случайную фразу
// ("как дела?", "сходи разомнись" и т.п.). Не блокирует работу: клик по Капри или реплике закрывает её раньше времени.
// Только заголовок (inline); использует спрайты и перенос строк из Tutorial.h.
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "Tutorial.h"

namespace CapriVisit {

constexpr Uint64 kIntervalMs = 30ull * 60ull * 1000ull;   // как часто появляется (30 минут)
constexpr Uint64 kRetryMs    = 60ull * 1000ull;           // если сейчас неудобно (обучение, меню) - повтор через минуту
constexpr Uint64 kShowMs     = 9000;                      // сколько висит реплика после того, как допечаталась
constexpr float  kSlideSec   = 0.45f;

struct Phrase { Tutorial::Pose pose; const char* en; const char* ru; };

inline const Phrase kPhrases[] = {
    { Tutorial::Pose::Stand,   "Hey! How are you doing?",                          "Эй! Как дела?" },
    { Tutorial::Pose::Dance,   "Time to stretch! Stand up and roll your shoulders.", "Пора размяться! Встань и потяни плечи." },
    { Tutorial::Pose::Roam,    "Drink some water~",                                 "Выпей водички~" },
    { Tutorial::Pose::Stand,   "Rest your eyes: look far away for 20 seconds.",     "Дай глазам отдохнуть: посмотри вдаль 20 секунд." },
    { Tutorial::Pose::Excited, "Sit up straight! Your back is not a pretzel.",      "Сядь ровно! Спина - не крендель." },
    { Tutorial::Pose::Roam,    "You've been at it for a while. Short break?",       "Ты давно за работой. Сделаем перерыв?" },
    { Tutorial::Pose::Excited, "Proud of you. Keep going!",                         "Горжусь тобой. Так держать!" },
    { Tutorial::Pose::Stand,   "Did you save your work?",                           "Ты сохранил работу?" },
    { Tutorial::Pose::Roam,    "Open a window and let some fresh air in.",          "Открой окно, впусти свежий воздух." },
    { Tutorial::Pose::Dance,   "Go walk around a bit, I'll wait here~",             "Сходи, разомнись, я подожду~" },
    { Tutorial::Pose::Stand,   "How is your mood today?",                           "Какое у тебя сегодня настроение?" },
    { Tutorial::Pose::Excited, "Hey, hey! Don't forget to eat something!",          "Эй-эй! Не забудь что-нибудь съесть!" },
};
constexpr int kPhraseCount = int(sizeof(kPhrases) / sizeof(kPhrases[0]));

inline bool   g_enabled = true;    // выключатель в меню INTERFACE
inline bool   g_visible = false, g_leaving = false;
inline Uint64 g_nextTick = 0, g_showTick = 0, g_leaveTick = 0, g_doneTypingTick = 0;
inline int    g_phrase = -1, g_lastPhrase = -1;
inline Uint32 g_seed = 0;
inline SDL_FRect g_hit{ 0, 0, 0, 0 };   // зона клика (Капри + реплика) с прошлого кадра
inline bool   g_swallowUp = false;

inline bool Visible() { return g_visible; }

// Включить / выключить визиты. При выключении текущая реплика уходит; при включении отсчёт 30 минут идёт заново.
inline void SetEnabled(bool on) {
    if (g_enabled == on) return;
    g_enabled = on;
    if (!on) {
        if (g_visible && !g_leaving) { g_leaving = true; g_leaveTick = SDL_GetTicks(); }
    } else {
        g_nextTick = SDL_GetTicks() + kIntervalMs;
    }
}

inline void Pick() {
    if (g_seed == 0) g_seed = Uint32(SDL_GetTicks()) * 2654435761u + 12345u;
    int p;
    do {
        g_seed = g_seed * 1664525u + 1013904223u;
        p = int((g_seed >> 8) % Uint32(kPhraseCount));
    } while (p == g_lastPhrase && kPhraseCount > 1);
    g_phrase = g_lastPhrase = p;
}

// Показать прямо сейчас (для проверки; обычно вызывается таймером).
inline void ShowNow() {
    Pick();
    g_visible = true;
    g_leaving = false;
    g_showTick = SDL_GetTicks();
    g_doneTypingTick = 0;
}

inline int AppendNoDrag(SDL_Rect* nd, int count, int maxCount) {
    if (g_visible && count < maxCount)
        nd[count++] = { int(g_hit.x), int(g_hit.y), int(g_hit.w), int(g_hit.h) };
    return count;
}

// Клик по Капри / реплике закрывает её; событие в приложение не попадает.
inline bool Handle(const SDL_Event& e) {
    if (!g_visible && !g_swallowUp) return false;
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && g_swallowUp) { g_swallowUp = false; return true; }
    if (!g_visible || g_leaving) return false;
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        const float x = e.button.x, y = e.button.y;
        if (x >= g_hit.x && x < g_hit.x + g_hit.w && y >= g_hit.y && y < g_hit.y + g_hit.h) {
            g_leaving = true;
            g_leaveTick = SDL_GetTicks();
            g_swallowUp = true;
            return true;
        }
    }
    return false;
}

// Вызывать в конце кадра. blocked = сейчас нельзя отвлекать (заставка, обучение, открытое меню).
inline void Draw(SDL_Renderer* r, int w, int h, bool blocked) {
    const Uint64 now = SDL_GetTicks();
    if (g_nextTick == 0) g_nextTick = now + kIntervalMs;

    if (!g_visible) {
        if (g_enabled && now >= g_nextTick) {
            if (blocked || w < 420 || h < 300) g_nextTick = now + kRetryMs;   // слишком тесно или занято: чуть позже
            else ShowNow();
        }
        if (!g_visible) return;
    }

    const Phrase& ph = kPhrases[g_phrase];
    const std::string text = UiText(ph.en, ph.ru);
    const float fw = float(w), fh = float(h);
    const float t = float(now) / 1000.f;

    // фазы: выезд -> реплика -> уход
    float appear = Theme::EaseOut(float(now - g_showTick) / (kSlideSec * 1000.f));
    if (g_leaving) {
        const float x = float(now - g_leaveTick) / (kSlideSec * 1000.f);
        if (x >= 1.f) { g_visible = false; g_leaving = false; g_nextTick = now + kIntervalMs; g_hit = {}; return; }
        appear = std::min(appear, 1.f - Theme::EaseOut(x));
    } else if (g_doneTypingTick && now - g_doneTypingTick > kShowMs) {
        g_leaving = true;
        g_leaveTick = now;
    }

    // размеры: Капри справа над шестерёнкой, реплика слева от неё
    const float capriH = std::max(90.f, std::min(200.f, fh * 0.34f));
    SDL_Texture* tex = Tutorial::PoseTexture(r, ph.pose);
    if (!tex) return;
    const CapriData::Sprite& s = Tutorial::g_sprites[int(ph.pose)];
    const float capriW = float(s.w) * capriH / float(s.h);
    const float restX = fw - capriW - 22.f;
    const float capriX = restX + (1.f - appear) * (capriW + 40.f);
    const float capriY = fh - capriH - 74.f;

    const float padIn = 12.f;
    const float bubbleW = std::min(300.f, restX - 24.f);
    if (bubbleW < 140.f) { g_hit = {}; return; }
    int font = 16;
    std::vector<std::string> lines = Tutorial::Wrap(text, font, bubbleW - 2 * padIn);
    if (lines.size() > 4) { font = 8; lines = Tutorial::Wrap(text, font, bubbleW - 2 * padIn); }
    const float lineH = float(font) + (font == 16 ? 8.f : 5.f);
    const float bubbleH = padIn * 2 + lineH * float(lines.size());
    const float bx = restX - 8.f - bubbleW + (1.f - appear) * (bubbleW + capriW + 60.f);
    const float by = capriY + capriH * 0.25f - bubbleH * 0.5f;

    // Капри (смотрит влево, на экран) с лёгким покачиванием
    SDL_SetTextureAlphaMod(tex, Uint8(255.f * std::min(1.f, appear * 1.5f)));
    const SDL_FRect dst{ capriX, capriY + std::sin(t * 2.2f) * 3.f, capriW, capriH };
    SDL_RenderTextureRotated(r, tex, nullptr, &dst, 0.0, nullptr, SDL_FLIP_HORIZONTAL);

    // реплика
    const Uint8 ba = Uint8(255.f * appear);
    Theme::CutPanel(r, bx, by, bubbleW, bubbleH, 10.f, Theme::Panel, Uint8(240.f * appear), Theme::Cyan, ba, true);
    const SDL_FPoint tail[3] = { { bx + bubbleW, by + bubbleH * 0.5f - 8.f },
                                 { bx + bubbleW + 10.f, by + bubbleH * 0.5f },
                                 { bx + bubbleW, by + bubbleH * 0.5f + 8.f } };
    Theme::FillPoly(r, tail, 3, Theme::Cyan, Uint8(220.f * appear));

    const size_t sinceMs = size_t(now - g_showTick);
    size_t shown = sinceMs > size_t(kSlideSec * 1000.f) ? size_t(float(sinceMs - size_t(kSlideSec * 1000.f)) / 28.f) : 0;
    size_t total = 0;
    for (const auto& ln : lines) total += Utf8Count(ln);
    if (shown >= total) { shown = total; if (!g_doneTypingTick) g_doneTypingTick = now; }
    float ly = by + padIn;
    size_t left = shown;
    for (const auto& ln : lines) {
        if (left == 0) break;
        const size_t vis = std::min(left, Utf8Count(ln));
        const SDL_Rect clip{ int(bx + padIn), int(ly) - 2, int(float(vis) * float(font)), int(lineH) + 4 };
        SDL_SetRenderClipRect(r, &clip);
        DrawUtf8Text(r, ln, bx + padIn, ly, font, 200, 240, 255, ba);
        SDL_SetRenderClipRect(r, nullptr);
        left -= vis;
        ly += lineH;
    }

    g_hit = { std::min(bx, capriX) - 4.f, std::min(by, capriY) - 4.f,
              std::max(bx + bubbleW, capriX + capriW) - std::min(bx, capriX) + 8.f,
              std::max(by + bubbleH, capriY + capriH) - std::min(by, capriY) + 8.f };
}

} // namespace CapriVisit

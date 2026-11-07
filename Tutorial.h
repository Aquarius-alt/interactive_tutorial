#pragma once
// Обучение при первом запуске: Капри — проводник по интерфейсу.
// Поверх приложения затемнение (с «окном-прожектором» вокруг нужного элемента), Капри в одной из четырёх поз
// и реплика в рамке кибердека с печатающимся текстом. Кнопки BACK / NEXT / SKIP, клавиши Enter, Space, стрелки, Esc.
// Только заголовок (inline); нужен CapriData.h рядом. Запускается Tutorial::Start() (при первом запуске и из меню).
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "Text.h"
#include "Theme.h"
#include "TextButton.h"
#include "Localization.h"
#include "CapriData.h"

namespace Tutorial {

enum class Pose { Stand = 0, Excited = 1, Roam = 2, Dance = 3 };
enum class Target { None, SignButtons, WindowControls, Gear };

// Прямоугольники элементов интерфейса, которые main.cpp заполняет каждый кадр.
struct Targets {
    bool hasSign = false;       // на экране сейчас кнопки SIGN IN / SIGN UP
    SDL_Rect sign{ 0, 0, 0, 0 };
    SDL_Rect controls{ 0, 0, 0, 0 };   // кнопки окна: свернуть / во весь экран / закрыть
    SDL_Rect gear{ 0, 0, 0, 0 };       // шестерня настроек
};

struct Step {
    Pose pose;
    Target target;
    const char* titleEn; const char* titleRu;
    const char* bodyEn;  const char* bodyRu;
};

inline const Step kSteps[] = {
    { Pose::Stand, Target::None,
      "HI, I'M CAPRI", "ПРИВЕТ, Я КАПРИ",
      "I'll show you around. It takes a minute, and you can skip it any time.",
      "Я покажу тебе, что тут есть. Это займёт минутку, а пропустить можно в любой момент." },
    { Pose::Excited, Target::SignButtons,
      "START HERE", "НАЧНИ ОТСЮДА",
      "SIGN UP creates your account, SIGN IN lets you in. Registered once? Just sign in.",
      "РЕГИСТРАЦИЯ создаёт аккаунт, ВХОД открывает приложение. Уже регистрировался? Просто входи." },
    { Pose::Stand, Target::WindowControls,
      "WINDOW", "ОКНО",
      "Minimize, full screen (F11 works too) and close live up here.",
      "Свернуть, развернуть на весь экран (работает и F11) и закрыть - вот здесь." },
    { Pose::Roam, Target::Gear,
      "SETTINGS", "НАСТРОЙКИ",
      "The gear opens the menu: interface language ENG / RUS, and you can replay this tutorial there.",
      "Шестерня открывает меню: язык интерфейса ENG / RUS. Там же можно снова запустить это обучение." },
    { Pose::Excited, Target::None,
      "YOUR REPOSITORIES", "ТВОИ РЕПОЗИТОРИИ",
      "After login they are cards. Click a card for DELETE, EDIT or OPEN. The + card offers IMPORT or CREATE.",
      "После входа они лежат карточками. Клик по карточке: DELETE, EDIT или OPEN. Карточка + даёт IMPORT или CREATE." },
    { Pose::Stand, Target::None,
      "CREATING ONE", "СОЗДАНИЕ",
      "CREATE has four tabs: INFO (name, XP, description), THEORY, TESTS and GAMES. Fill them in and press CREATE.",
      "У CREATE четыре вкладки: INFO (название, опыт, описание), THEORY, TESTS и GAMES. Заполни и нажми CREATE." },
    { Pose::Roam, Target::None,
      "PROFILE AND RANKS", "ПРОФИЛЬ И РАНГИ",
      "Click your avatar for PROFILE or LOGOUT. In the profile you can pass rank tests and collect ranks.",
      "Клик по аватару: PROFILE или LOGOUT. В профиле можно проходить тесты на ранг и собирать ранги." },
    { Pose::Dance, Target::None,
      "THAT'S ALL~", "ВОТ И ВСЁ~",
      "Find me again in the menu under TUTORIAL. Have fun!",
      "Если что, я в меню, в пункте TUTORIAL. Удачи!" },
};
constexpr int kStepCount = int(sizeof(kSteps) / sizeof(kSteps[0]));

// ---- состояние ----
inline bool   g_active = false;
inline bool   g_exiting = false;
inline Uint64 g_enterTick = 0, g_exitTick = 0, g_stepTick = 0, g_poseTick = 0, g_lastTick = 0;
inline int    g_step = 0;
inline Pose   g_pose = Pose::Stand, g_prevPose = Pose::Stand;
inline bool   g_typingDone = false;
inline void (*g_onFinish)() = nullptr;   // вызывается при завершении и при SKIP (main.cpp сохраняет «обучение пройдено»)

// кадр-к-кадру
inline float g_hx = 0, g_hy = 0, g_hw = 0, g_hh = 0;
inline bool  g_hasHole = false;

inline TextButton g_btnNext{ 0, 0, 120, 34, "NEXT" };
inline TextButton g_btnBack{ 0, 0, 120, 34, "BACK" };
inline TextButton g_btnSkip{ 0, 0, 120, 34, "SKIP" };
inline bool g_showBack = false, g_showSkip = false;

inline SDL_Texture*  g_tex[4] = { nullptr, nullptr, nullptr, nullptr };
inline SDL_Renderer* g_owner = nullptr;

inline Targets g_targets;   // последние известные прямоугольники (их каждый кадр задаёт main.cpp через SetTargets)
inline void SetTargets(const Targets& t) { g_targets = t; }
inline bool Active() { return g_active; }

inline bool StepAvailable(int i, const Targets& t) {
    if (kSteps[i].target == Target::SignButtons) return t.hasSign;   // кнопок входа нет на других экранах
    return true;
}

inline void Finish(bool immediate) {
    if (!g_active) return;
    if (g_onFinish) g_onFinish();
    if (immediate) { g_active = false; g_exiting = false; return; }
    g_exiting = true;
    g_exitTick = SDL_GetTicks();
}

inline void SetStep(int i, const Targets& t, int dir) {
    while (i >= 0 && i < kStepCount && !StepAvailable(i, t)) i += dir;
    if (i >= kStepCount) { Finish(false); return; }
    if (i < 0) i = 0;
    g_step = i;
    g_stepTick = SDL_GetTicks();
    g_typingDone = false;
    if (kSteps[i].pose != g_pose) { g_prevPose = g_pose; g_pose = kSteps[i].pose; g_poseTick = g_stepTick; }
}

// Шаги про отсутствующие на экране элементы (кнопки входа) пропускаются; цели берутся из SetTargets().
inline void Start() {
    const Targets& t = g_targets;
    g_active = true;
    g_exiting = false;
    g_enterTick = SDL_GetTicks();
    g_hasHole = false;
    g_pose = g_prevPose = Pose(kSteps[0].pose);
    g_poseTick = 0;
    SetStep(0, t, 1);
}

// ---- вспомогательное ----
inline std::string Prefix(const std::string& s, size_t n) {
    size_t i = 0, c = 0;
    while (i < s.size() && c < n) {
        ++i;
        while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
        ++c;
    }
    return s.substr(0, i);
}

inline std::vector<std::string> Wrap(const std::string& text, int font, float maxW) {
    const size_t maxChars = size_t(std::max(1.f, std::floor(maxW / float(font))));
    std::vector<std::string> lines;
    std::string cur;
    size_t curCount = 0, pos = 0;
    while (pos <= text.size()) {
        size_t sp = text.find(' ', pos);
        if (sp == std::string::npos) sp = text.size();
        std::string word = text.substr(pos, sp - pos);
        pos = sp + 1;
        if (word.empty()) { if (sp >= text.size()) break; continue; }
        size_t wc = Utf8Count(word);
        while (wc > maxChars) {                       // слишком длинное слово: рубим по символам
            if (!cur.empty()) { lines.push_back(cur); cur.clear(); curCount = 0; }
            const std::string part = Prefix(word, maxChars);
            lines.push_back(part);
            word = word.substr(part.size());
            wc = Utf8Count(word);
        }
        if (cur.empty()) { cur = word; curCount = wc; }
        else if (curCount + 1 + wc <= maxChars) { cur += ' '; cur += word; curCount += 1 + wc; }
        else { lines.push_back(cur); cur = word; curCount = wc; }
        if (sp >= text.size()) break;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

inline SDL_Texture* PoseTexture(SDL_Renderer* r, Pose p) {
    if (g_owner != r) {
        for (auto& t : g_tex) { if (t) SDL_DestroyTexture(t); t = nullptr; }
        g_owner = r;
    }
    SDL_Texture*& tex = g_tex[int(p)];
    if (tex) return tex;
    const CapriData::Sprite& s = CapriData::kSprites[int(p)];
    std::vector<Uint32> px(size_t(s.w) * size_t(s.h), 0u);
    size_t at = 0;
    for (int i = 0; i < s.runCount && at < px.size(); ++i) {
        const Uint32 n = s.runs[i * 2], c = s.runs[i * 2 + 1];
        for (Uint32 k = 0; k < n && at < px.size(); ++k) px[at++] = c;
    }
    tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, s.w, s.h);
    if (!tex) return nullptr;
    SDL_UpdateTexture(tex, nullptr, px.data(), s.w * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
    return tex;
}

inline void Destroy() {
    for (auto& t : g_tex) { if (t) SDL_DestroyTexture(t); t = nullptr; }
    g_owner = nullptr;
    TextButton_Destroy(g_btnNext);
    TextButton_Destroy(g_btnBack);
    TextButton_Destroy(g_btnSkip);
}

inline bool Overlap(const SDL_FRect& a, const SDL_FRect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

// Дописывает в массив «зон без перетаскивания» кнопки обучения (позиции с прошлого кадра).
inline int AppendNoDrag(SDL_Rect* nd, int count, int maxCount) {
    TextButton* bs[3] = { &g_btnNext, &g_btnBack, &g_btnSkip };
    for (int i = 0; i < 3; ++i) {
        if ((i == 1 && !g_showBack) || (i == 2 && !g_showSkip)) continue;
        if (count < maxCount) nd[count++] = TextButton_Rect(*bs[i]);
    }
    return count;
}

// Нарисовать Капри: покачивание, глитч-«рассыпание» полосами при смене позы, отзеркаливание.
inline void DrawCapri(SDL_Renderer* r, float x, float y, float h, bool flip, float alpha, float t, Uint64 now) {
    const float sinceChange = g_poseTick ? float(now - g_poseTick) / 1000.f : 9.f;
    const bool glitching = sinceChange < 0.30f;
    const Pose shown = (glitching && sinceChange < 0.10f) ? g_prevPose : g_pose;
    SDL_Texture* tex = PoseTexture(r, shown);
    if (!tex) return;
    const CapriData::Sprite& s = CapriData::kSprites[int(shown)];
    const float scale = h / float(s.h);
    const float w = float(s.w) * scale;
    y += std::sin(t * 2.2f) * 3.f;                        // лёгкое «дыхание»
    SDL_SetTextureAlphaMod(tex, Uint8(255.f * std::min(1.f, std::max(0.f, alpha))));
    const SDL_FlipMode fm = flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    if (!glitching) {
        const SDL_FRect dst{ x, y, w, h };
        SDL_RenderTextureRotated(r, tex, nullptr, &dst, 0.0, nullptr, fm);
        return;
    }
    const float amp = (1.f - sinceChange / 0.30f) * 26.f;   // сдвиг полос затухает
    const Uint32 frame = Uint32(sinceChange * 40.f);
    const int strip = 6;
    for (int sy = 0; sy < s.h; sy += strip) {
        const int sh = std::min(strip, s.h - sy);
        const Uint32 hh = Theme::Hash32(frame * 7919u + Uint32(sy) * 31u + 3u);
        float dx = 0.f;
        if ((hh & 3u) != 0u) dx = (Theme::Hash01(hh) - 0.5f) * 2.f * amp;
        const SDL_FRect src{ 0.f, float(sy), float(s.w), float(sh) };
        const SDL_FRect dst{ x + dx, y + float(sy) * scale, w, float(sh) * scale + 0.5f };
        SDL_RenderTextureRotated(r, tex, &src, &dst, 0.0, nullptr, fm);
    }
}

struct Layout { float cx, cy, ch, cw; bool flip; SDL_FRect bubble; };

// Главная функция: вызывать в конце кадра (поверх всего приложения), только если Active().
inline void Draw(SDL_Renderer* r, int w, int h) {
    if (!g_active) return;
    const Targets& tg = g_targets;
    const Uint64 now = SDL_GetTicks();
    const float dt = g_lastTick ? std::min(0.1f, float(now - g_lastTick) / 1000.f) : 0.016f;
    g_lastTick = now;
    const float t = float(now) / 1000.f;
    const float fw = float(w), fh = float(h);

    // появление / уход
    float appear = Theme::EaseOut(float(now - g_enterTick) / 450.f);
    if (g_exiting) {
        const float x = float(now - g_exitTick) / 350.f;
        if (x >= 1.f) { g_active = false; g_exiting = false; return; }
        appear *= 1.f - x;
    }

    const Step& st = kSteps[g_step];

    // ---- цель-прожектор ----
    bool hasTarget = false;
    SDL_Rect tr{ 0, 0, 0, 0 };
    if (st.target == Target::SignButtons && tg.hasSign) { tr = tg.sign; hasTarget = true; }
    else if (st.target == Target::WindowControls) { tr = tg.controls; hasTarget = true; }
    else if (st.target == Target::Gear) { tr = tg.gear; hasTarget = true; }
    if (hasTarget && (tr.w <= 0 || tr.h <= 0)) hasTarget = false;

    const float pad = 8.f;
    const float tx = float(tr.x) - pad, ty = float(tr.y) - pad, tw = float(tr.w) + 2 * pad, th = float(tr.h) + 2 * pad;
    if (hasTarget) {
        if (!g_hasHole) { g_hx = tx; g_hy = ty; g_hw = tw; g_hh = th; g_hasHole = true; }
        else {
            const float k = 1.f - std::exp(-dt * 14.f);   // плавный переезд прожектора между целями
            g_hx += (tx - g_hx) * k; g_hy += (ty - g_hy) * k;
            g_hw += (tw - g_hw) * k; g_hh += (th - g_hh) * k;
        }
    } else g_hasHole = false;

    // ---- затемнение ----
    const Uint8 dim = Uint8(165.f * appear);
    const Theme::Rgb black{ 3, 1, 6 };
    if (g_hasHole) {
        const float x0 = std::max(0.f, g_hx), y0 = std::max(0.f, g_hy);
        const float x1 = std::min(fw, g_hx + g_hw), y1 = std::min(fh, g_hy + g_hh);
        Theme::Rect(r, 0.f, 0.f, fw, y0, black, dim);
        Theme::Rect(r, 0.f, y1, fw, fh - y1, black, dim);
        Theme::Rect(r, 0.f, y0, x0, y1 - y0, black, dim);
        Theme::Rect(r, x1, y0, fw - x1, y1 - y0, black, dim);
        const float pulse = 0.65f + 0.35f * std::sin(t * 5.f);
        Theme::CutOutline(r, g_hx, g_hy, g_hw, g_hh, 8.f, Theme::CyanHot, Uint8(255.f * pulse * appear), true);
        Theme::Brackets(r, g_hx - 4.f, g_hy - 4.f, g_hw + 8.f, g_hh + 8.f, 8.f, Theme::Coral, Uint8(230.f * appear));
    } else {
        Theme::Rect(r, 0.f, 0.f, fw, fh, black, Uint8(dim * 0.88f));
    }

    // ---- текст реплики и размеры ----
    const std::string title = UiText(st.titleEn, st.titleRu);
    const std::string body = UiText(st.bodyEn, st.bodyRu);
    const float padIn = 16.f, btnH = 34.f;

    float capriH = std::max(110.f, std::min(280.f, fh * 0.40f));
    const CapriData::Sprite& cs = CapriData::kSprites[int(g_pose)];
    float capriW = float(cs.w) * capriH / float(cs.h);
    float bubbleW = std::min(480.f, fw - capriW - 18.f - 8.f - 24.f);
    const bool narrow = bubbleW < 240.f;            // совсем узкое окно: реплика во всю ширину сверху
    if (narrow) { capriH = std::max(80.f, fh * 0.28f); capriW = float(cs.w) * capriH / float(cs.h); bubbleW = fw - 28.f; }

    const float textW = std::max(40.f, bubbleW - 2 * padIn);
    int bodyFont = 16;
    std::vector<std::string> lines = Wrap(body, bodyFont, textW);
    if (lines.size() > 5) { bodyFont = 8; lines = Wrap(body, bodyFont, textW); }
    const int titleFont = (Utf8TextWidth(title, 16) <= textW - 60.f) ? 16 : 8;
    const float lineH = float(bodyFont) + (bodyFont == 16 ? 8.f : 5.f);
    const float bubbleH = padIn + float(titleFont) + 12.f + lineH * float(lines.size()) + 12.f + btnH + padIn;

    // ---- раскладка: слева / справа / сверху, где не перекрывает цель ----
    Layout cand[3];
    const float margin = 18.f;
    cand[0] = { margin, fh - capriH - 14.f, capriH, capriW, false,
                { margin + capriW + 8.f, fh - bubbleH - 30.f, bubbleW, bubbleH } };
    cand[1] = { fw - margin - capriW, fh - capriH - 14.f, capriH, capriW, true,
                { fw - margin - capriW - 8.f - bubbleW, fh - bubbleH - 30.f, bubbleW, bubbleH } };
    cand[2] = { margin, fh - capriH - 14.f, capriH, capriW, false,
                { (fw - bubbleW) * 0.5f, 44.f, bubbleW, bubbleH } };
    if (narrow) { cand[0].bubble = { 14.f, 44.f, bubbleW, bubbleH }; cand[1] = cand[2] = cand[0]; }
    int pick = 0;
    if (hasTarget) {
        const SDL_FRect tf{ g_hx - 6.f, g_hy - 6.f, g_hw + 12.f, g_hh + 12.f };
        pick = 2;
        for (int i = 0; i < 3; ++i) {
            const SDL_FRect cr{ cand[i].cx, cand[i].cy, cand[i].cw, cand[i].ch };
            if (!Overlap(tf, cand[i].bubble) && !Overlap(tf, cr)) { pick = i; break; }
        }
    }
    const Layout& L = cand[pick];

    // ---- Капри: выезжает из-за края ----
    const float slide = (1.f - appear) * (L.cw + 60.f);
    const float cxDraw = L.flip ? L.cx + slide : L.cx - slide;
    DrawCapri(r, cxDraw, L.cy, L.ch, L.flip, std::min(1.f, appear * 1.5f), t, now);

    // ---- реплика ----
    const SDL_FRect& b = L.bubble;
    const Uint8 ba = Uint8(255.f * appear);
    Theme::CutPanel(r, b.x, b.y, b.w, b.h, 14.f, Theme::Panel, Uint8(246.f * appear), Theme::Cyan, ba, true);
    Theme::Brackets(r, b.x + 4.f, b.y + 4.f, b.w - 8.f, b.h - 8.f, 6.f, Theme::Coral, Uint8(200.f * appear));
    // хвостик реплики в сторону Капри
    if (!narrow && pick < 2) {
        const float side = L.flip ? b.x + b.w : b.x;
        const float dir = L.flip ? 1.f : -1.f;
        const float ty0 = b.y + b.h - 56.f;
        const SDL_FPoint tail[3] = { { side, ty0 }, { side + dir * 12.f, ty0 + 12.f }, { side, ty0 + 24.f } };
        Theme::FillPoly(r, tail, 3, Theme::Cyan, Uint8(220.f * appear));
    }

    const float sinceStep = float(now - g_stepTick) / 1000.f;
    DrawUtf8Text(r, title, b.x + padIn, b.y + padIn, titleFont, Theme::CyanHot.r, Theme::CyanHot.g, Theme::CyanHot.b, ba);
    const std::string counter = std::to_string(g_step + 1) + "/" + std::to_string(kStepCount);
    DrawUtf8Text(r, counter, b.x + b.w - padIn - Utf8TextWidth(counter, 8), b.y + padIn + 2.f, 8,
        Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, Uint8(160.f * appear));

    // печать текста по символам (тот же шрифт Press Start 2P)
    size_t shown = size_t(sinceStep / 0.016f);
    size_t total = 0;
    for (const auto& ln : lines) total += Utf8Count(ln);
    if (shown >= total) { shown = total; g_typingDone = true; }
    float ly = b.y + padIn + float(titleFont) + 12.f;
    size_t left = shown;
    for (const auto& ln : lines) {
        if (left == 0) break;
        const size_t c = Utf8Count(ln);
        const size_t vis = std::min(left, c);
        const SDL_Rect clip{ int(b.x + padIn), int(ly) - 2, int(float(vis) * float(bodyFont)), int(lineH) + 4 };
        SDL_SetRenderClipRect(r, &clip);              // строка рисуется целиком (кешируется), открывается слева направо
        DrawUtf8Text(r, ln, b.x + padIn, ly, bodyFont, 200, 240, 255, ba);
        SDL_SetRenderClipRect(r, nullptr);
        left -= vis;
        ly += lineH;
    }
    if (!g_typingDone && (int(t * 8.f) % 2 == 0)) {   // курсор, пока печатается
        Theme::Rect(r, b.x + b.w - padIn - 8.f, b.y + b.h - padIn - btnH - 14.f, 8.f, 8.f, Theme::CyanHot, 220);
    }

    // ---- кнопки ----
    const bool last = g_step == kStepCount - 1;
    g_btnNext.label = last ? UiText("DONE", "ГОТОВО") : UiText("NEXT", "ДАЛЕЕ");
    g_btnBack.label = UiText("BACK", "НАЗАД");
    g_btnSkip.label = UiText("SKIP", "ПРОПУСК");
    for (TextButton* tb : { &g_btnNext, &g_btnBack, &g_btnSkip }) {
        tb->h = btnH;
        tb->w = std::max(96.f, Utf8TextWidth(tb->label, 16) + 28.f);
    }
    g_showBack = g_step > 0;
    g_showSkip = !last;
    const float by = b.y + b.h - padIn - btnH;
    g_btnNext.x = b.x + b.w - padIn - g_btnNext.w;
    g_btnNext.y = by;
    g_btnBack.x = g_btnNext.x - 8.f - g_btnBack.w;
    g_btnBack.y = by;
    g_btnSkip.x = b.x + padIn;
    g_btnSkip.y = by;
    // при нехватке места SKIP и BACK не должны наезжать друг на друга: прячем BACK
    if (g_showSkip && g_showBack && g_btnBack.x < g_btnSkip.x + g_btnSkip.w + 8.f) g_showBack = false;
    if (g_showBack) TextButton_Draw(r, g_btnBack);
    if (g_showSkip) TextButton_Draw(r, g_btnSkip);
    TextButton_Draw(r, g_btnNext);
}

// Все события, пока обучение открыто, идут сюда (в приложение они не попадают).
inline void Handle(const SDL_Event& e) {
    if (!g_active || g_exiting) return;
    const Targets& tg = g_targets;

    // кнопки обрабатываются всегда (наведение), клик - только по видимым
    const bool next = TextButton_Handle(g_btnNext, e);
    const bool back = g_showBack && TextButton_Handle(g_btnBack, e);
    const bool skip = g_showSkip && TextButton_Handle(g_btnSkip, e);
    bool goNext = next, goBack = back, goSkip = skip;

    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
        switch (e.key.key) {
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: case SDLK_RIGHT: goNext = true; break;
        case SDLK_LEFT: case SDLK_BACKSPACE: goBack = g_step > 0; break;
        case SDLK_ESCAPE: goSkip = true; break;
        default: break;
        }
    }

    if (goSkip) { Finish(false); return; }
    if (goNext) {
        if (!g_typingDone) { g_typingDone = true; g_stepTick = SDL_GetTicks() - 60000; return; }   // первый клик дописывает текст
        if (g_step >= kStepCount - 1) Finish(false);
        else SetStep(g_step + 1, tg, 1);
        return;
    }
    if (goBack && g_step > 0) SetStep(g_step - 1, tg, -1);
}

} // namespace Tutorial

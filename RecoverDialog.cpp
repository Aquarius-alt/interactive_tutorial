#include "RecoverDialog.h"
#include "TextField.h"
#include "Text.h"
#include "UserStore.h"
#include "Localization.h"
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace {
    enum class Phase { Closed, Input, Scan, Hold, Result };

    // зелёный: рамка, текст и фон окна
    const Uint8 GR = 60, GG = 255, GB = 130;

    const float kScan = 2.4f;          // минимальная длительность поиска, сек
    const float kHold = 0.35f;         // пауза на 100% перед результатом
    const int   kTotalFiles = 1440;    // для надписи "624 / 1440 FILES" (как в референсе)
    const float kBarH = 34.f;
    const int   kMaxShown = 3;         // сколько аккаунтов показываем

    Phase     g_phase = Phase::Closed;
    TextField g_field;                 // хранит введённое слово; рисуется здесь же
    std::string g_err;
    Uint64    g_t0 = 0, g_holdT = 0;
    float     g_p = 0.f;               // показанный прогресс 0..1 (только растёт)

    std::thread       g_worker;
    std::atomic<int>  g_state{ 0 };    // 0 = ищет, 1 = готово
    std::vector<RecoveredAccount> g_found;   // пишет поток; читать только после state == 1

    SDL_Texture*  g_tex = nullptr;     // текст полосы: рисуется в текстуру и режется по границе заливки
    SDL_Renderer* g_owner = nullptr;
    int g_tw = 0, g_th = 0;

    struct Btn { float x = 0, y = 0, w = 0, h = 0; bool hover = false; };
    Btn g_a, g_b;                      // слева CANCEL, справа FIND / CLOSE

    float Sec(Uint64 now, Uint64 from) { return float(now - from) / 1000.f; }
    float Clamp01(float v) { return std::min(1.f, std::max(0.f, v)); }

    bool In(const Btn& b, float mx, float my) {
        return mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h;
    }

    // заполнение с остановками, как на 64% в референсе
    float FillCurve(float x) {
        static const float pts[7][2] = {
            {0.00f, 0.00f}, {0.18f, 0.22f}, {0.30f, 0.30f}, {0.55f, 0.62f},
            {0.68f, 0.64f}, {0.85f, 0.93f}, {1.00f, 1.00f} };
        x = Clamp01(x);
        for (int i = 1; i < 7; ++i) {
            if (x <= pts[i][0]) {
                const float k = (x - pts[i - 1][0]) / (pts[i][0] - pts[i - 1][0]);
                return pts[i - 1][1] + (pts[i][1] - pts[i - 1][1]) * k;
            }
        }
        return 1.f;
    }

    float TextW(const std::string& s, float sc) {
        return Utf8TextWidth(s, static_cast<int>(8.f * sc));
    }

    // самый крупный масштаб (2 .. 1), при котором строка влезает в avail
    float FitScale(const std::string& s, float avail) {
        float sc = 2.f;
        while (sc > 1.f && TextW(s, sc) > avail) sc -= 0.5f;
        return sc;
    }

    // надпись по координатам x, y (левый верхний угол) с мягким свечением
    void Txt(SDL_Renderer* r, const std::string& s, float x, float y, float sc,
             Uint8 R, Uint8 G, Uint8 B, Uint8 A = 255, bool glow = true) {
        const int fontHeight = static_cast<int>(8.f * sc);
        if (glow) {
            const Uint8 glowAlpha = static_cast<Uint8>(A * 45 / 255);
            const float offset = 0.6f * sc;
            DrawUtf8Text(r, s, x - offset, y, fontHeight, R, G, B, glowAlpha);
            DrawUtf8Text(r, s, x + offset, y, fontHeight, R, G, B, glowAlpha);
            DrawUtf8Text(r, s, x, y - offset, fontHeight, R, G, B, glowAlpha);
            DrawUtf8Text(r, s, x, y + offset, fontHeight, R, G, B, glowAlpha);
        }
        DrawUtf8Text(r, s, x, y, fontHeight, R, G, B, A);
    }

    // рамка в 2 пикселя с неоновым свечением снаружи
    void NeonRect(SDL_Renderer* r, const SDL_FRect& rc, Uint8 R, Uint8 G, Uint8 B, float strength = 1.f) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (int i = 6; i >= 1; --i) {
            const float a = std::min(255.f, 46.f * strength * (1.f - i / 7.f));
            SDL_SetRenderDrawColor(r, R, G, B, Uint8(a));
            const SDL_FRect g = { rc.x - i, rc.y - i, rc.w + 2.f * i, rc.h + 2.f * i };
            SDL_RenderRect(r, &g);
        }
        SDL_SetRenderDrawColor(r, R, G, B, 255);
        SDL_RenderRect(r, &rc);
        const SDL_FRect in = { rc.x + 1.f, rc.y + 1.f, rc.w - 2.f, rc.h - 2.f };
        SDL_RenderRect(r, &in);
    }

    void DrawBtn(SDL_Renderer* r, const Btn& b, const char* label) {
        const SDL_FRect rc = { b.x, b.y, b.w, b.h };
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, GR, GG, GB, b.hover ? 70 : 24);
        SDL_RenderFillRect(r, &rc);
        NeonRect(r, rc, GR, GG, GB, b.hover ? 1.6f : 1.f);

        // подпись всегда внутри кнопки: мельче, а если и так не влезает - с ".."
        const float sc = FitScale(label, b.w - 16.f);
        const int px = NormalizeTextFontHeight(static_cast<int>(8.f * sc));
        const std::string shown = Utf8Fit(label, b.w - 16.f, px);
        const float tw = TextW(shown, sc);
        const float ty = b.y + (b.h - float(px)) * 0.5f;
        if (b.hover) Txt(r, shown, b.x + (b.w - tw) * 0.5f, ty, sc, 210, 255, 225);
        else         Txt(r, shown, b.x + (b.w - tw) * 0.5f, ty, sc, GR, GG, GB);
    }

    void EnsureTex(SDL_Renderer* r, int W, int H) {
        if (g_tex && g_owner == r && g_tw == W && g_th == H) return;
        if (g_tex) SDL_DestroyTexture(g_tex);
        g_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, W, H);
        if (g_tex) SDL_SetTextureBlendMode(g_tex, SDL_BLENDMODE_BLEND);
        g_owner = r;
        g_tw = W;
        g_th = H;
    }

    // полоса как в референсе: сплошная заливка, надпись слева, проценты справа,
    // под полосой справа "624 / 1440 FILES". Текст на заливке тёмный, на пустой части цветной.
    void DrawStrip(SDL_Renderer* r, float x, float y, float W, const std::string& left,
                   float p, Uint8 R, Uint8 G, Uint8 B, bool live) {
        const int iw = int(W), ih = int(kBarH);
        EnsureTex(r, iw, ih);
        if (!g_tex) return;

        char pct[16];
        std::snprintf(pct, sizeof pct, "%d%%", int(p * 100.f + 0.0001f));

        // 1) белый текст в текстуру
        SDL_Texture* prev = SDL_GetRenderTarget(r);
        SDL_SetRenderTarget(r, g_tex);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
        SDL_RenderClear(r);
        const float ty = (ih - 16.f) * 0.5f;
        Txt(r, left, 12.f, ty, 2.f, 255, 255, 255, 255, false);
        Txt(r, pct, iw - 12.f - TextW(pct, 2.f), ty, 2.f, 255, 255, 255, 255, false);
        SDL_SetRenderTarget(r, prev);

        // 2) фон, заливка, рамка
        const SDL_FRect box = { x, y, W, kBarH };
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 6, 12, 12, 215);
        SDL_RenderFillRect(r, &box);

        const float fw = std::floor((W - 4.f) * p);
        if (fw >= 1.f) {
            SDL_SetRenderDrawColor(r, R, G, B, 255);
            const SDL_FRect fill = { x + 2.f, y + 2.f, fw, kBarH - 4.f };
            SDL_RenderFillRect(r, &fill);
            if (live && p < 1.f) {                                // яркая «голова»
                SDL_SetRenderDrawColor(r, 230, 255, 255, 255);
                const SDL_FRect head = { x + 2.f + fw - 2.f, y + 2.f, 2.f, kBarH - 4.f };
                SDL_RenderFillRect(r, &head);
            }
        }
        NeonRect(r, box, R, G, B, 1.f);

        // 3) текст: по заливке тёмным, дальше цветом полосы
        const float split = 2.f + fw;
        if (split > 0.f) {
            const SDL_FRect src = { 0.f, 0.f, split, float(ih) };
            const SDL_FRect dst = { x, y, split, float(ih) };
            SDL_SetTextureColorMod(g_tex, 6, 24, 20);
            SDL_RenderTexture(r, g_tex, &src, &dst);
        }
        if (split < float(iw)) {
            const SDL_FRect src = { split, 0.f, float(iw) - split, float(ih) };
            const SDL_FRect dst = { x + split, y, float(iw) - split, float(ih) };
            SDL_SetTextureColorMod(g_tex, R, G, B);
            SDL_RenderTexture(r, g_tex, &src, &dst);
        }

        // 4) "624 / 1440 FILES" под полосой справа
        char files[40];
        std::snprintf(files, sizeof files, "%d / %d %s", int(p * kTotalFiles), kTotalFiles, UiText("FILES", "ФАЙЛОВ"));
        Txt(r, files, x + W - TextW(files, 1.f) - 2.f, y + kBarH + 8.f, 1.f, R, G, B, 210, false);
    }

    void WipeField() {
        std::fill(g_field.text.begin(), g_field.text.end(), '\0');
        g_field.text.clear();
    }

    void WipeFound() {
        for (auto& a : g_found) std::fill(a.password.begin(), a.password.end(), '\0');
        g_found.clear();
    }

    void StartScan() {
        if (g_worker.joinable()) g_worker.join();
        WipeFound();
        g_state = 0;
        g_p = 0.f;
        g_err.clear();
        g_t0 = SDL_GetTicks();
        g_phase = Phase::Scan;

        // настоящий поиск идёт в фоне: на каждую запись один PBKDF2
        g_worker = std::thread([kw = g_field.text]() mutable {
            std::vector<RecoveredAccount> res = UserStore_Recover(kw);
            std::fill(kw.begin(), kw.end(), '\0');
            g_found = std::move(res);
            g_state = 1;
        });
    }

    void TrySearch() {
        if (g_field.text.empty()) { g_err = "ENTER KEYWORD"; return; }
        StartScan();
    }

    void Advance() {
        const Uint64 now = SDL_GetTicks();

        if (g_phase == Phase::Scan) {
            const float pt = FillCurve(Sec(now, g_t0) / kScan);
            const bool done = g_state.load() == 1;
            g_p = std::max(g_p, std::min(pt, done ? 1.f : 0.97f));   // 100% только когда поиск закончен
            if (g_p >= 1.f) {
                g_phase = Phase::Hold;
                g_holdT = now;
            }
        }
        else if (g_phase == Phase::Hold && Sec(now, g_holdT) >= kHold) {
            if (g_worker.joinable()) g_worker.join();
            if (g_found.empty()) {
                g_err = "KEYWORD NOT FOUND";        // назад к вводу, слово остаётся в поле
                g_phase = Phase::Input;
            }
            else {
                WipeField();
                g_phase = Phase::Result;
            }
            g_a.hover = g_b.hover = false;
        }
    }
}

void Recover_Open(SDL_Window* window) {
    if (g_worker.joinable()) g_worker.join();
    WipeFound();
    WipeField();
    g_field.placeholder = UiText("KEYWORD", "КЛЮЧЕВОЕ СЛОВО");
    g_field.password = false;
    g_field.focused = true;
    g_field.maxLen = 32;
    g_err.clear();
    g_p = 0.f;
    g_state = 0;
    g_a.hover = g_b.hover = false;
    g_phase = Phase::Input;
    SDL_StartTextInput(window);
}

void Recover_Close() {
    if (g_worker.joinable()) g_worker.join();
    WipeFound();
    WipeField();
    g_err.clear();
    g_phase = Phase::Closed;
}

bool Recover_IsOpen() { return g_phase != Phase::Closed; }

void Recover_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount) {
    if (g_phase == Phase::Closed) return;
    Advance();

    // g_found читаем только когда поток закончил (Result)
    const size_t cnt = (g_phase == Phase::Result) ? g_found.size() : 0;
    const size_t shown = std::min<size_t>(cnt, kMaxShown);
    const bool more = cnt > shown;

    // затемнение всего окна
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 175);
    SDL_RenderFillRect(r, nullptr);

    const float W = std::max(240.f, std::min(460.f, float(w) - 40.f));
    float H = 240.f;
    if (g_phase == Phase::Result) H = 66.f + shown * 76.f + (more ? 22.f : 0.f) + 70.f;
    const float px = std::floor((w - W) * 0.5f);
    const float py = std::floor((h - H) * 0.5f);
    const SDL_FRect panel = { px, py, W, H };

    // фон: тёмная подложка (чтобы не просвечивал экран под окном) и полупрозрачный зелёный
    SDL_SetRenderDrawColor(r, 0, 0, 0, 215);
    SDL_RenderFillRect(r, &panel);
    SDL_SetRenderDrawColor(r, GR, GG, GB, 46);
    SDL_RenderFillRect(r, &panel);
    NeonRect(r, panel, GR, GG, GB, 1.f);

    // заголовок
    g_field.placeholder = UiText("KEYWORD", "КЛЮЧЕВОЕ СЛОВО");
    const char* title = UiText("FORGOT PASSWORD", "ВОССТАНОВЛЕНИЕ ПАРОЛЯ");
    if (g_phase == Phase::Result) title = (cnt > 1) ? UiText("ACCOUNTS FOUND", "АККАУНТЫ НАЙДЕНЫ") : UiText("ACCOUNT FOUND", "АККАУНТ НАЙДЕН");
    float titleScale = W >= 400.f ? 3.0f : 2.0f;
    // Russian title is longer than the English one; reduce its pixel size only
    // when needed so it stays centered inside the dialog.
    while (titleScale > 1.0f && TextW(title, titleScale) > W - 40.f)
        titleScale -= 0.5f;
    DrawCenteredText(r, title, w, h, titleScale, py + 34.f - h * 0.5f, GR, GG, GB);

    const float bx = px + 40.f, bw = W - 80.f;    // общая «колонка» контента
    const float by = py + H - 24.f - 40.f;        // строка кнопок

    switch (g_phase) {
    case Phase::Input: {
        const std::string sub = UiText("ENTER YOUR KEYWORD", "ВВЕДИТЕ КЛЮЧЕВОЕ СЛОВО");
        const float subSc = FitScale(sub, W - 24.f);                       // «ВВЕДИТЕ КЛЮЧЕВОЕ СЛОВО» длиннее английского
        Txt(r, sub, px + (W - TextW(sub, subSc)) * 0.5f, py + 68.f + (2.f - subSc) * 4.f, subSc, GR, GG, GB, 170);

        // поле ввода
        const SDL_FRect fr = { bx, py + 96.f, bw, 40.f };
        SDL_SetRenderDrawColor(r, GR, GG, GB, 20);
        SDL_RenderFillRect(r, &fr);
        NeonRect(r, fr, GR, GG, GB, 1.3f);

        const float cs = float(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) * 2.f;
        const int maxVis = std::max(1, int((fr.w - 24.f) / cs) - 1);
        std::string shown_s = g_field.text;
        if (int(shown_s.size()) > maxVis) shown_s = shown_s.substr(shown_s.size() - maxVis);   // хвост
        const float ty = fr.y + (fr.h - cs) * 0.5f;
        if (g_field.text.empty()) {
            const std::string ph = g_field.placeholder ? g_field.placeholder : "";
            const float phSc = FitScale(ph, fr.w - 24.f);
            const int phPx = NormalizeTextFontHeight(static_cast<int>(8.f * phSc));
            Txt(r, Utf8Fit(ph, fr.w - 24.f, phPx), fr.x + 12.f, fr.y + (fr.h - float(phPx)) * 0.5f, phSc, GR, GG, GB, 90, false);
        }
        else {
            Txt(r, shown_s, fr.x + 12.f, ty, 2.f, 190, 255, 215, 255, false);
        }
        if ((SDL_GetTicks() / 500) % 2 == 0)                                 // мигающий курсор
            Txt(r, "_", fr.x + 12.f + TextW(shown_s, 2.f), ty, 2.f, GR, GG, GB, 255, false);

        if (!g_err.empty()) {
            const std::string shownError = UiMessage(g_err);
            const float sc = FitScale(shownError, W - 40.f);
            Txt(r, shownError, px + (W - TextW(shownError, sc)) * 0.5f, py + 150.f, sc, 255, 90, 90);
        }

        const float half = (bw - 16.f) * 0.5f;
        g_a = { bx, by, half, 40.f, g_a.hover };
        g_b = { bx + half + 16.f, by, half, 40.f, g_b.hover };
        DrawBtn(r, g_a, UiText("CANCEL", "ОТМЕНА"));
        DrawBtn(r, g_b, UiText("FIND", "НАЙТИ"));
        break;
    }
    case Phase::Scan:
    case Phase::Hold: {
        const bool hold = (g_phase == Phase::Hold);
        const bool miss = hold && g_found.empty();      // в Hold поток уже закончил
        Uint8 R = 90, G = 230, B = 255;                  // голубой, как на скрине
        std::string left;
        if (!hold) {
            const int dots = int(SDL_GetTicks() / 350) % 3 + 1;
            left = std::string(UiText("SCANNING ", "ПОИСК ")) + std::string(size_t(dots), '.');
        }
        else if (miss) { R = 255; G = 70;  B = 80;  left = UiText("NOT FOUND", "НЕ НАЙДЕНО"); }
        else           { R = GR;  G = GG;  B = GB;  left = UiText("FOUND", "НАЙДЕНО"); }

        DrawStrip(r, px + 30.f, py + 98.f, W - 60.f, left, g_p, R, G, B, !hold);
        break;
    }
    case Phase::Result: {
        const float tx = px + 36.f;
        const float avail = W - 72.f;
        for (size_t i = 0; i < shown; ++i) {
            const RecoveredAccount& a = g_found[i];
            const float y0 = py + 66.f + i * 76.f;

            Txt(r, UiText("LOGIN", "ЛОГИН"), tx, y0, 1.f, GR, GG, GB, 150, false);
            const float s1 = FitScale(a.login, avail);
            Txt(r, a.login, tx, y0 + 12.f, s1, GR, GG, GB);

            Txt(r, UiText("PASSWORD", "ПАРОЛЬ"), tx, y0 + 36.f, 1.f, GR, GG, GB, 150, false);
            if (a.hasPassword) {
                const float s2 = FitScale(a.password, avail);
                Txt(r, a.password, tx, y0 + 48.f, s2, GR, GG, GB);
            }
            else {
                Txt(r, UiText("NOT AVAILABLE", "НЕДОСТУПНО"), tx, y0 + 48.f, 2.f, GR, GG, GB, 110);
            }

            if (i + 1 < shown || more) {                 // разделитель
                SDL_SetRenderDrawColor(r, GR, GG, GB, 60);
                const SDL_FRect sep = { px + 20.f, y0 + 70.f, W - 40.f, 1.f };
                SDL_RenderFillRect(r, &sep);
            }
        }
        if (more) {
            char buf[40];
            std::snprintf(buf, sizeof buf, UiRussianFlag() ? "+ ЕЩЁ %d" : "+ %d MORE", int(cnt - shown));
            Txt(r, buf, tx, py + 66.f + shown * 76.f + 4.f, 1.f, GR, GG, GB, 150, false);
        }

        g_b = { bx, by, bw, 40.f, g_b.hover };
        DrawBtn(r, g_b, UiText("CLOSE", "ЗАКРЫТЬ"));
        break;
    }
    default: break;
    }

    // вся панель без перетаскивания окна
    nd[ndCount++] = { int(px) - 8, int(py) - 8, int(W) + 16, int(H) + 16 };
}

void Recover_Handle(const SDL_Event& e) {
    if (g_phase == Phase::Closed) return;
    if (g_phase == Phase::Scan || g_phase == Phase::Hold) return;   // идёт поиск: ввод игнорируется

    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        g_a.hover = (g_phase == Phase::Input) && In(g_a, e.motion.x, e.motion.y);
        g_b.hover = In(g_b, e.motion.x, e.motion.y);
    }
    if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) g_a.hover = g_b.hover = false;

    const bool click = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT;
    const bool key = e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat;
    const bool enter = key && (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER);
    const bool esc = key && e.key.key == SDLK_ESCAPE;

    if (g_phase == Phase::Input) {
        if (click && In(g_a, e.button.x, e.button.y)) { Recover_Close(); return; }
        if (click && In(g_b, e.button.x, e.button.y)) { TrySearch(); return; }
        if (esc) { Recover_Close(); return; }
        if (enter) { TrySearch(); return; }

        const bool backspace = e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_BACKSPACE;
        if (e.type == SDL_EVENT_TEXT_INPUT || backspace || IsEditShortcut(e)) {
            TextField_Input(g_field, e);
            g_err.clear();
        }
    }
    else if (g_phase == Phase::Result) {
        if ((click && In(g_b, e.button.x, e.button.y)) || esc || enter) Recover_Close();
    }
}

void Recover_Destroy() {
    Recover_Close();
    if (g_tex) SDL_DestroyTexture(g_tex);
    g_tex = nullptr;
    g_owner = nullptr;
    TextField_Destroy(g_field);
}

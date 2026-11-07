#include "SaveOverlay.h"
#include "UserStore.h"
#include "Text.h"
#include "Localization.h"
#include <atomic>
#include <thread>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <string>

namespace {
    enum class Phase { Idle, Intro, Fill, Hold, Outro };

    // тайминги, сек
    const float kLine = 0.20f;               // линия растягивается по X от центра в обе стороны
    const float kIntro = 0.45f;              // затем раскрывается по Y (как включение старого ТВ)
    const float kFill = 2.4f;                // минимальная длительность заполнения шкалы
    const float kHold = 0.6f;                // показ результата (SAVED / FAILED)
    const float kCollapse = 0.22f;           // выключение: схлопывание по Y
    const float kShrink = 0.18f;             // выключение: линия сужается к центру
    const float kOutro = kCollapse + kShrink;

    const float kStripH = 76.f;

    Phase  g_phase = Phase::Idle;
    Uint64 g_t0 = 0;                         // старт всей анимации
    Uint64 g_phaseT = 0;                     // старт текущей фазы
    float  g_p = 0.f;                        // показанный прогресс 0..1 (только растёт)
    bool   g_ok = false;

    std::string g_lbl[4] = { "LOCAL", "STORAGE", "PBKDF2", "SHA-256" };   // подписи в рамках
    bool        g_fade = false;                  // плавно затемнять окно (когда под полосой нет окна подтверждения)

    std::thread       g_worker;
    std::atomic<int>  g_state{ 0 };          // 0 = работает, 1 = записано, 2 = ошибка

    SDL_Texture*  g_tex = nullptr;           // полоса рисуется в текстуру и растягивается целиком
    SDL_Renderer* g_owner = nullptr;
    int g_tw = 0, g_th = 0;

    float Sec(Uint64 now, Uint64 from) { return float(now - from) / 1000.f; }
    float Clamp01(float v) { return std::min(1.f, std::max(0.f, v)); }
    float EaseOut(float t) { t = Clamp01(t); const float k = 1.f - t; return 1.f - k * k * k; }
    float EaseIn(float t) { t = Clamp01(t); return t * t * t; }

    // кривая заполнения с «задумчивыми» остановками, как на 64% в референсе
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

    void Join() {
        if (g_worker.joinable()) g_worker.join();
    }

    void Advance() {
        const Uint64 now = SDL_GetTicks();

        if (g_phase == Phase::Intro && Sec(now, g_t0) >= kIntro) {
            g_phase = Phase::Fill;
            g_phaseT = now;
        }
        if (g_phase == Phase::Fill) {
            const int st = g_state.load();
            if (st == 2) {                               // ошибка: шкала замирает
                g_ok = false;
                g_phase = Phase::Hold;
                g_phaseT = now;
            }
            else {
                const float pt = FillCurve(Sec(now, g_phaseT) / kFill);
                const float cap = (st == 0) ? 0.97f : 1.f;   // до конца записи 100% не показываем
                g_p = std::max(g_p, std::min(pt, cap));
                if (g_p >= 1.f) {
                    g_ok = true;
                    g_phase = Phase::Hold;
                    g_phaseT = now;
                }
            }
        }
        if (g_phase == Phase::Hold && Sec(now, g_phaseT) >= kHold) {
            g_phase = Phase::Outro;
            g_phaseT = now;
        }
    }

    void EnsureTexture(SDL_Renderer* r, int W, int H) {
        if (g_tex && g_owner == r && g_tw == W && g_th == H) return;
        if (g_tex) SDL_DestroyTexture(g_tex);
        g_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, W, H);
        g_owner = r;
        g_tw = W;
        g_th = H;
    }

    // содержимое полосы целиком (рисуется в текстуру W x H)
    void RenderStrip(SDL_Renderer* r, int W, int H) {
        const bool result = (g_phase == Phase::Hold || g_phase == Phase::Outro);
        Uint8 ar = 90, ag = 230, ab = 255;                       // голубой
        if (result) {
            if (g_ok) { ar = 60;  ag = 255; ab = 130; }          // зелёный
            else      { ar = 255; ag = 70;  ab = 80; }           // красный
        }

        SDL_Texture* prev = SDL_GetRenderTarget(r);
        SDL_SetRenderTarget(r, g_tex);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

        // фон
        SDL_SetRenderDrawColor(r, 10, 6, 10, 255);
        SDL_RenderClear(r);

        // сегментная шкала
        const float bx = 12.f, by = 12.f, bw = float(W) - 24.f, bh = 12.f;
        const int n = int(bw / 5.f);
        const int filled = int(g_p * n + 0.5f);
        if (filled > 0) {                                        // мягкое свечение под заполненной частью
            SDL_SetRenderDrawColor(r, ar, ag, ab, 30);
            const SDL_FRect glow = { bx - 3, by - 4, filled * 5.f + 3, bh + 8 };
            SDL_RenderFillRect(r, &glow);
        }
        for (int i = 0; i < n; ++i) {
            const SDL_FRect tick = { bx + i * 5.f, by, 3.f, bh };
            if (i < filled) {
                if (i == filled - 1 && !result) SDL_SetRenderDrawColor(r, 220, 255, 255, 255);   // яркая «голова»
                else                            SDL_SetRenderDrawColor(r, ar, ag, ab, 255);
            }
            else {
                SDL_SetRenderDrawColor(r, ar, ag, ab, 40);
            }
            SDL_RenderFillRect(r, &tick);
        }

        // нижний ряд: две подписи в рамках и текст по центру
        const float rowY = float(H) - 40.f;
        const SDL_FRect lb = { 12.f, rowY, 80.f, 28.f };
        const SDL_FRect rb = { float(W) - 92.f, rowY, 80.f, 28.f };
        SDL_SetRenderDrawColor(r, 255, 50, 60, 200);
        SDL_RenderRect(r, &lb);
        SDL_RenderRect(r, &rb);

        const std::string lbl0 = UiMessage(g_lbl[0]);
        const std::string lbl1 = UiMessage(g_lbl[1]);
        const std::string lbl2 = UiMessage(g_lbl[2]);
        const std::string lbl3 = UiMessage(g_lbl[3]);
        DrawUtf8Text(r, lbl0, lb.x + 8, rowY + 5, 8, 255, 90, 100, 235);
        DrawUtf8Text(r, lbl1, lb.x + 8, rowY + 16, 8, 255, 90, 100, 235);
        DrawUtf8Text(r, lbl2, rb.x + 8, rowY + 5, 8, 255, 90, 100, 235);
        DrawUtf8Text(r, lbl3, rb.x + 8, rowY + 16, 8, 255, 90, 100, 235);

        char buf[64];
        if (!result) std::snprintf(buf, sizeof buf, UiRussianFlag() ? "СОХРАНЕНИЕ %3d%%" : "SAVING %3d%%", int(g_p * 100.f));
        else if (g_ok) std::snprintf(buf, sizeof buf, UiText("SAVED 100%%", "СОХРАНЕНО 100%%"));
        else std::snprintf(buf, sizeof buf, UiText("SAVE FAILED", "ОШИБКА СОХРАНЕНИЯ"));
        DrawUtf8TextFit(r, buf, lb.x + lb.w + 12.f, rowY + 14.f, float(W) - 2.f * (lb.x + lb.w + 12.f),
            16, true, ar, ag, ab, 255);                    // между двумя рамками подписей

        // лёгкие «строки развёртки»
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 40);
        for (int y = 0; y < H; y += 3) {
            const SDL_FRect line = { 0.f, float(y), float(W), 1.f };
            SDL_RenderFillRect(r, &line);
        }

        // красная рамка
        SDL_SetRenderDrawColor(r, 255, 50, 60, 255);
        const SDL_FRect b0 = { 0.f, 0.f, float(W), float(H) };
        const SDL_FRect b1 = { 1.f, 1.f, float(W) - 2.f, float(H) - 2.f };
        SDL_RenderRect(r, &b0);
        SDL_RenderRect(r, &b1);

        SDL_SetRenderTarget(r, prev);
    }
}

namespace {
    void Begin(std::function<bool()> task, bool fade) {
        g_fade = fade;
        g_state = 0;
        g_p = 0.f;
        g_ok = false;
        g_t0 = SDL_GetTicks();
        g_phase = Phase::Intro;

        // настоящая работа идёт в фоне, полоса только показывает ход
        g_worker = std::thread([task = std::move(task)]() {
            bool ok = false;
            try { ok = task(); } catch (...) { ok = false; }
            g_state = ok ? 1 : 2;
        });
    }
}

void SaveOverlay_Start(const std::string& login, const std::string& password, const std::string& keyword) {
    if (g_phase != Phase::Idle) return;
    Join();

    g_lbl[0] = "LOCAL";  g_lbl[1] = "STORAGE";
    g_lbl[2] = "PBKDF2"; g_lbl[3] = "SHA-256";

    // хэширование занимает десятки миллисекунд
    Begin([login = login, password = password, keyword = keyword]() mutable {
        const bool ok = UserStore_Add(login, password, keyword);
        std::fill(password.begin(), password.end(), '\0');       // не держим копии в памяти
        std::fill(keyword.begin(), keyword.end(), '\0');
        return ok;
    }, false);
}

void SaveOverlay_StartTask(std::function<bool()> task,
                           const std::string& leftTop, const std::string& leftBottom,
                           const std::string& rightTop, const std::string& rightBottom) {
    if (g_phase != Phase::Idle) return;
    Join();

    g_lbl[0] = leftTop;  g_lbl[1] = leftBottom;
    g_lbl[2] = rightTop; g_lbl[3] = rightBottom;
    Begin(std::move(task), true);
}

bool SaveOverlay_Active() { return g_phase != Phase::Idle; }

SaveResult SaveOverlay_Poll() {
    if (g_phase == Phase::Idle) return SaveResult::None;
    Advance();
    if (g_phase == Phase::Outro && Sec(SDL_GetTicks(), g_phaseT) >= kOutro) {
        Join();
        g_phase = Phase::Idle;
        return g_ok ? SaveResult::Ok : SaveResult::Failed;
    }
    return SaveResult::None;
}

void SaveOverlay_Draw(SDL_Renderer* r, int w, int h) {
    if (g_phase == Phase::Idle) return;
    Advance();

    const Uint64 now = SDL_GetTicks();
    float lw = 1.f;        // ширина линии (0..1 от ширины полосы)
    float hf = 1.f;        // высота полосы (0..1)
    float dim = 1.f;       // затемнение окна
    float flash = 0.f;     // вспышка при раскрытии
    if (g_fade && g_phase == Phase::Intro)       // под полосой нет окна подтверждения: затемняем плавно
        dim = Clamp01(Sec(now, g_t0) / kIntro);

    if (g_phase == Phase::Intro) {
        const float t = Sec(now, g_t0);
        if (t < kLine) {
            lw = EaseOut(t / kLine);
            hf = 0.f;
        }
        else {
            const float k = (t - kLine) / (kIntro - kLine);
            hf = EaseOut(k);
            flash = 1.f - Clamp01(k);
        }
    }
    else if (g_phase == Phase::Outro) {
        const float u = Sec(now, g_phaseT);
        if (u < kCollapse) {
            hf = 1.f - EaseIn(u / kCollapse);
        }
        else {
            hf = 0.f;
            lw = 1.f - EaseIn((u - kCollapse) / kShrink);
        }
        dim = 1.f - Clamp01(u / kOutro);
    }

    // затемнение (экран уже был затемнён окном подтверждения, поэтому сразу на полную)
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, Uint8(175 * dim));
    SDL_RenderFillRect(r, nullptr);

    const float W = std::max(380.f, std::min(520.f, float(w) - 40.f));
    const float H = kStripH;
    const float cx = w * 0.5f, cy = h * 0.5f;

    if (hf > 0.f) {
        EnsureTexture(r, int(W), int(H));
        RenderStrip(r, int(W), int(H));

        const float dh = std::max(2.f, H * hf);
        const SDL_FRect dst = { std::round(cx - W * 0.5f), std::round(cy - dh * 0.5f), W, dh };

        // красное свечение вокруг
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (int i = 4; i >= 1; --i) {
            SDL_SetRenderDrawColor(r, 255, 40, 60, Uint8(40.f * hf * (1.f - i / 5.f)));
            const SDL_FRect g = { dst.x - i, dst.y - i, dst.w + 2.f * i, dst.h + 2.f * i };
            SDL_RenderRect(r, &g);
        }

        SDL_RenderTexture(r, g_tex, nullptr, &dst);

        if (flash > 0.f) {                                       // белая вспышка при включении
            SDL_SetRenderDrawColor(r, 200, 245, 255, Uint8(150.f * flash));
            SDL_RenderFillRect(r, &dst);
        }
    }

    // яркая линия: растёт от центра в обе стороны при включении, сужается при выключении
    if (hf < 1.f && lw > 0.f) {
        const float la = 1.f - hf;
        const float lwPx = W * lw;
        const float lx = std::round(cx - lwPx * 0.5f);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 90, 230, 255, Uint8(70.f * la));
        const SDL_FRect glow = { lx, cy - 4.f, lwPx, 8.f };
        SDL_RenderFillRect(r, &glow);
        SDL_SetRenderDrawColor(r, 230, 255, 255, Uint8(255.f * la));
        const SDL_FRect line = { lx, cy - 1.f, lwPx, 2.f };
        SDL_RenderFillRect(r, &line);
    }
}

void SaveOverlay_Destroy() {
    Join();
    if (g_tex) SDL_DestroyTexture(g_tex);
    g_tex = nullptr;
    g_owner = nullptr;
    g_phase = Phase::Idle;
}

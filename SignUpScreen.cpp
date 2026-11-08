#include "SignUpScreen.h"
#include "TextField.h"
#include "TextButton.h"
#include "Text.h"
#include "Theme.h"
#include "ConfirmDialog.h"
#include "SaveOverlay.h"
#include "UserStore.h"
#include "Localization.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace {
    const int kFields = 4;
    TextField  g_f[kFields];     // логин, пароль, повтор пароля, ключевое слово
    TextButton g_ok, g_back;
    std::string g_err;
    bool g_init = false;

    void EnsureInit() {
        if (g_init) return;
        g_init = true;
        const char* ph[kFields] = { UiText("LOGIN", "ЛОГИН"), UiText("PASSWORD", "ПАРОЛЬ"), UiText("REPEAT PASSWORD", "ПОВТОРИТЕ ПАРОЛЬ"), UiText("KEYWORD", "КЛЮЧЕВОЕ СЛОВО") };
        for (int i = 0; i < kFields; ++i) {
            g_f[i].placeholder = ph[i];
            g_f[i].password = (i == 1 || i == 2);
        }
        g_ok.label = UiText("SIGN UP", "СОЗДАТЬ");
        g_back.label = UiText("BACK", "НАЗАД");
    }

    void Focus(int i) {
        for (int k = 0; k < kFields; ++k) g_f[k].focused = (k == i);
    }

    int FocusIndex() {
        for (int k = 0; k < kFields; ++k) if (g_f[k].focused) return k;
        return -1;
    }

    bool Validate() {
        if (g_f[0].text.size() < 3) { g_err = "LOGIN TOO SHORT";         Focus(0); return false; }
        if (UserStore_Exists(g_f[0].text)) { g_err = "LOGIN ALREADY TAKEN"; Focus(0); return false; }
        if (g_f[1].text.size() < 6) { g_err = "PASSWORD TOO SHORT";      Focus(1); return false; }
        if (g_f[1].text != g_f[2].text) { g_err = "PASSWORDS DO NOT MATCH";  Focus(2); return false; }
        if (g_f[3].text.empty()) { g_err = "ENTER KEYWORD";           Focus(3); return false; }
        g_err.clear();
        return true;
    }

    // ---------- шкала надёжности пароля ----------
    enum class Strength { None, Weak, Medium, Strong };

    Strength PasswordStrength(const std::string& p) {
        if (p.empty()) return Strength::None;
        if (p.size() < 8) return Strength::Weak;
        bool special = false;
        for (unsigned char c : p)
            if (c > 32 && c < 127 && !std::isalnum(c)) { special = true; break; }
        return special ? Strength::Strong : Strength::Medium;
    }

    float  g_fill = 0.f;                       // текущее заполнение 0..1 (анимируется)
    float  g_col[3] = { 255, 60, 60 };         // текущий цвет (анимируется)
    Uint64 g_barTick = 0;

    void DrawStrengthBar(SDL_Renderer* r, float x, float y, float w, const std::string& pwd) {
        float targetFill = 0.f;
        float tc[3] = { g_col[0], g_col[1], g_col[2] };

        switch (PasswordStrength(pwd)) {
        case Strength::Weak:   targetFill = 1.f / 3; tc[0] = 255; tc[1] = 60;  tc[2] = 60; break;
        case Strength::Medium: targetFill = 2.f / 3; tc[0] = 255; tc[1] = 150; tc[2] = 30; break;
        case Strength::Strong: targetFill = 1.f;     tc[0] = 60;  tc[1] = 255; tc[2] = 110; break;
        default: break;                          // пусто: шкала плавно гаснет
        }

        // плавный переход по реальному времени
        const Uint64 now = SDL_GetTicks();
        if (g_barTick == 0) g_barTick = now;
        const float dt = std::min((now - g_barTick) / 1000.0f, 0.05f);
        g_barTick = now;
        const float k = std::min(1.f, dt * 10.f);

        g_fill += (targetFill - g_fill) * k;
        for (int i = 0; i < 3; ++i) g_col[i] += (tc[i] - g_col[i]) * k;

        if (g_fill < 0.01f) return;

        const float h = 4.f;
        const float fw = w * g_fill;
        const Uint8 R = Uint8(g_col[0]), G = Uint8(g_col[1]), B = Uint8(g_col[2]);

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

        // тусклая дорожка
        SDL_SetRenderDrawColor(r, R, G, B, 28);
        SDL_FRect track = { x, y, w, h };
        Theme::StyledFill(r, &track);

        // мягкое неоновое свечение: слои с убывающей прозрачностью
        const float pulse = 0.85f + 0.15f * std::sin(SDL_GetTicks() / 400.0f);
        for (int i = 8; i >= 1; --i) {
            const float a = 34.f * (1.f - i / 9.f) * pulse;
            SDL_SetRenderDrawColor(r, R, G, B, Uint8(a));
            SDL_FRect glow = { x - i, y - i, fw + i * 2, h + i * 2 };
            Theme::StyledFill(r, &glow);
        }

        // яркая сердцевина
        SDL_SetRenderDrawColor(r, R, G, B, 255);
        SDL_FRect core = { x, y, fw, h };
        Theme::StyledFill(r, &core);
    }
}

void SignUp_Open(SDL_Window* window) {
    EnsureInit();
    for (auto& f : g_f) { f.text.clear(); f.focused = false; }
    g_err.clear();
    Confirm_Close();
    Focus(0);
    SDL_StartTextInput(window);
}

void SignUp_Close(SDL_Window* window) {
    SDL_StopTextInput(window);
    for (auto& f : g_f) { f.text.clear(); f.focused = false; }   // пароль не должен висеть в памяти
    g_err.clear();
    Confirm_Close();
}

void SignUp_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount) {
    EnsureInit();

    const float fieldW = 320.f, fieldH = 40.f, gap = 14.f;
    const float titleH = 24.f, titleGap = 24.f, btnGap = 20.f;
    const float barGap = 10.f;   // доп. место под шкалу после поля пароля
    const float blockH = titleH + titleGap + kFields * fieldH + (kFields - 1) * gap
        + barGap + titleGap + fieldH;
    const float top = std::max(60.f, (h - blockH) * 0.5f);

    const char* ph[kFields] = { UiText("LOGIN", "ЛОГИН"), UiText("PASSWORD", "ПАРОЛЬ"), UiText("REPEAT PASSWORD", "ПОВТОРИТЕ ПАРОЛЬ"), UiText("KEYWORD", "КЛЮЧЕВОЕ СЛОВО") };
    for (int i = 0; i < kFields; ++i) g_f[i].placeholder = ph[i];
    g_ok.label = UiText("SIGN UP", "СОЗДАТЬ");
    g_back.label = UiText("BACK", "НАЗАД");
    DrawCenteredText(r, UiText("SIGN UP", "РЕГИСТРАЦИЯ"), w, h, 3.0f, top + titleH * 0.5f - h * 0.5f);

    const float x = (w - fieldW) * 0.5f;
    float y = top + titleH + titleGap;
    float barY = 0.f;

    for (int i = 0; i < kFields; ++i) {
        g_f[i].x = x; g_f[i].y = y; g_f[i].w = fieldW; g_f[i].h = fieldH;
        TextField_Draw(r, g_f[i]);
        nd[ndCount++] = TextField_Rect(g_f[i]);
        if (i == 1) barY = y + fieldH + 7.f;       // шкала под паролем
        y += fieldH + gap;
        if (i == 1) y += barGap;
    }

    DrawStrengthBar(r, x, barY, fieldW, g_f[1].text);

    y += titleGap - gap;   // чуть больше воздуха перед кнопками
    g_back.x = x;                         g_back.y = y;
    g_ok.x = x + g_back.w + btnGap;     g_ok.y = y;
    TextButton_Draw(r, g_back);
    TextButton_Draw(r, g_ok);
    nd[ndCount++] = TextButton_Rect(g_back);
    nd[ndCount++] = TextButton_Rect(g_ok);

    if (!g_err.empty()) {
        const std::string shownError = UiMessage(g_err);
        DrawCenteredText(r, shownError.c_str(), w, h, 2.0f, y + g_ok.h + 24.f - h * 0.5f, 255, 90, 90);
    }

    // окно "ARE YOU SURE?" рисуется поверх всего остального
    Confirm_Draw(r, w, h, nd, ndCount);

    // полоса сохранения поверх всего
    SaveOverlay_Draw(r, w, h);
}

SignUpResult SignUp_Handle(const SDL_Event& e) {
    EnsureInit();

    // пока идёт сохранение, ввод игнорируется
    if (SaveOverlay_Active()) return SignUpResult::None;

    // пока открыто окно подтверждения, все события идут только ему
    if (Confirm_IsOpen()) {
        if (Confirm_Handle(e) == ConfirmResult::Yes)
            SaveOverlay_Start(g_f[0].text, g_f[1].text, g_f[3].text);
        return SignUpResult::None;
    }

    for (int i = 0; i < kFields; ++i)
        if (TextField_Handle(g_f[i], e)) Focus(i);

    if (TextButton_Handle(g_back, e)) return SignUpResult::Back;
    if (TextButton_Handle(g_ok, e) && Validate()) {
        Confirm_Open();
        return SignUpResult::None;
    }

    // ввод символов; при правке убираем сообщение об ошибке
    const bool backspace = e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_BACKSPACE;
    if (e.type == SDL_EVENT_TEXT_INPUT || backspace || IsEditShortcut(e)) {
        for (auto& f : g_f) TextField_Input(f, e);
        g_err.clear();
    }

    if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.key) {
        case SDLK_ESCAPE:
            return SignUpResult::Back;
        case SDLK_TAB: {
            const int cur = FocusIndex();
            const int step = (e.key.mod & SDL_KMOD_SHIFT) ? -1 : 1;
            Focus(cur < 0 ? 0 : (cur + step + kFields) % kFields);
            break;
        }
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (Validate()) Confirm_Open();
            break;
        }
    }
    return SignUpResult::None;
}

SignUpData SignUp_Data() {
    return { g_f[0].text, g_f[1].text, g_f[3].text };
}

// звать каждый кадр: следит за окончанием сохранения
SignUpResult SignUp_Poll() {
    switch (SaveOverlay_Poll()) {
    case SaveResult::Ok:
        return SignUpResult::Done;
    case SaveResult::Failed:
        g_err = "SAVE FAILED";          // остаёмся на форме, можно попробовать ещё раз
        return SignUpResult::None;
    default:
        return SignUpResult::None;
    }
}

void SignUp_Destroy() {
    for (auto& f : g_f) TextField_Destroy(f);
    TextButton_Destroy(g_ok);
    TextButton_Destroy(g_back);
    Confirm_Destroy();
    SaveOverlay_Destroy();
}

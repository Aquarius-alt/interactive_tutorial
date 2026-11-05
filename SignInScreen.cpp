#include "SignInScreen.h"
#include "TextField.h"
#include "TextButton.h"
#include "Text.h"
#include "Localization.h"
#include <algorithm>

namespace {
    const int kFields = 2;
    TextField  g_f[kFields];          // логин, пароль
    TextButton g_ok, g_back, g_forgot;
    std::string g_err;
    bool g_init = false;

    void EnsureInit() {
        if (g_init) return;
        g_init = true;
        g_f[0].placeholder = UiText("LOGIN", "ЛОГИН");
        g_f[1].placeholder = UiText("PASSWORD", "ПАРОЛЬ");
        g_f[1].password = true;
        g_ok.label = UiText("SIGN IN", "ВОЙТИ");
        g_back.label = UiText("BACK", "НАЗАД");
        g_forgot.label = UiText("FORGOT?", "ЗАБЫЛИ?");
        g_forgot.w = 140; g_forgot.h = 32;
    }

    void Focus(int i) {
        for (int k = 0; k < kFields; ++k) g_f[k].focused = (k == i);
    }

    int FocusIndex() {
        for (int k = 0; k < kFields; ++k) if (g_f[k].focused) return k;
        return -1;
    }

    bool Validate() {
        if (g_f[0].text.empty()) { g_err = "ENTER LOGIN";    Focus(0); return false; }
        if (g_f[1].text.empty()) { g_err = "ENTER PASSWORD"; Focus(1); return false; }
        g_err.clear();
        return true;
    }

    void Wipe() {
        for (auto& f : g_f) {
            std::fill(f.text.begin(), f.text.end(), '\0');   // затираем перед очисткой
            f.text.clear();
            f.focused = false;
        }
        g_err.clear();
    }
}

void SignIn_Open(SDL_Window* window) {
    EnsureInit();
    Wipe();
    Focus(0);
    SDL_StartTextInput(window);
}

void SignIn_Close(SDL_Window* window) {
    SDL_StopTextInput(window);
    Wipe();
}

void SignIn_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount) {
    EnsureInit();

    const float fieldW = 320.f, fieldH = 40.f, gap = 14.f;
    const float titleH = 24.f, titleGap = 24.f, btnGap = 20.f;
    const float forgotGap = 10.f;
    const float blockH = titleH + titleGap + kFields * fieldH + (kFields - 1) * gap
        + forgotGap + g_forgot.h + titleGap + fieldH;
    const float top = std::max(60.f, (h - blockH) * 0.5f);

    g_f[0].placeholder = UiText("LOGIN", "ЛОГИН");
    g_f[1].placeholder = UiText("PASSWORD", "ПАРОЛЬ");
    g_ok.label = UiText("SIGN IN", "ВОЙТИ");
    g_back.label = UiText("BACK", "НАЗАД");
    g_forgot.label = UiText("FORGOT?", "ЗАБЫЛИ?");
    DrawCenteredText(r, UiText("SIGN IN", "ВХОД"), w, h, 3.0f, top + titleH * 0.5f - h * 0.5f);

    const float x = (w - fieldW) * 0.5f;
    float y = top + titleH + titleGap;

    for (int i = 0; i < kFields; ++i) {
        g_f[i].x = x; g_f[i].y = y; g_f[i].w = fieldW; g_f[i].h = fieldH;
        TextField_Draw(r, g_f[i]);
        nd[ndCount++] = TextField_Rect(g_f[i]);
        y += fieldH + gap;
    }

    // "FORGOT?" под паролем, у правого края поля
    y += forgotGap - gap;
    g_forgot.x = x + fieldW - g_forgot.w;
    g_forgot.y = y;
    TextButton_Draw(r, g_forgot);
    nd[ndCount++] = TextButton_Rect(g_forgot);
    y += g_forgot.h + titleGap;

    g_back.x = x;                      g_back.y = y;
    g_ok.x = x + g_back.w + btnGap;    g_ok.y = y;
    TextButton_Draw(r, g_back);
    TextButton_Draw(r, g_ok);
    nd[ndCount++] = TextButton_Rect(g_back);
    nd[ndCount++] = TextButton_Rect(g_ok);

    if (!g_err.empty()) {
        const std::string shownError = UiMessage(g_err);
        DrawCenteredText(r, shownError.c_str(), w, h, 2.0f, y + g_ok.h + 24.f - h * 0.5f, 255, 90, 90);
    }
}

SignInResult SignIn_Handle(const SDL_Event& e) {
    EnsureInit();

    for (int i = 0; i < kFields; ++i)
        if (TextField_Handle(g_f[i], e)) Focus(i);

    if (TextButton_Handle(g_back, e))   return SignInResult::Back;
    if (TextButton_Handle(g_forgot, e)) return SignInResult::Forgot;
    if (TextButton_Handle(g_ok, e) && Validate()) return SignInResult::Done;

    const bool backspace = e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_BACKSPACE;
    if (e.type == SDL_EVENT_TEXT_INPUT || backspace) {
        for (auto& f : g_f) TextField_Input(f, e);
        g_err.clear();
    }

    if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.key) {
        case SDLK_ESCAPE:
            return SignInResult::Back;
        case SDLK_TAB: {
            const int cur = FocusIndex();
            const int step = (e.key.mod & SDL_KMOD_SHIFT) ? -1 : 1;
            Focus(cur < 0 ? 0 : (cur + step + kFields) % kFields);
            break;
        }
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (Validate()) return SignInResult::Done;
            break;
        }
    }
    return SignInResult::None;
}

void SignIn_SetError(const std::string& msg) {
    EnsureInit();
    std::fill(g_f[1].text.begin(), g_f[1].text.end(), '\0');   // пароль вводится заново
    g_f[1].text.clear();
    g_err = msg;
    Focus(1);
}

SignInData SignIn_Data() {
    return { g_f[0].text, g_f[1].text };
}

void SignIn_Destroy() {
    for (auto& f : g_f) TextField_Destroy(f);
    TextButton_Destroy(g_ok);
    TextButton_Destroy(g_back);
    TextButton_Destroy(g_forgot);
}
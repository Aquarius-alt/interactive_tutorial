#include <SDL3/SDL.h>
#include <iostream>
#include <cstring>
#include "WindowFrame.h"
#include "Border.h"
#include "Button.h"
#include "Text.h"
#include "TextButton.h"
#include "SignUpScreen.h"
#include "SignInScreen.h"
#include "RecoverDialog.h"
#include "MainScreen.h"
#include "ProfileScreen.h"
#include "CreateScreen.h"
#include "Avatar.h"
#include "UserStore.h"
#include <algorithm>
#include <string>

enum class Screen { Login, SignIn, SignUp, Main, Profile, Create };
static Screen g_screen = Screen::Login;
static std::string g_user;   // логин вошедшего пользователя (как записан при регистрации)
static TextButton g_signIn{ 0, 0, 150, 40, "SIGN IN" };
static TextButton g_signUp{ 0, 0, 150, 40, "SIGN UP" };
static MenuButton g_menu{ 0, 16, 18 };   // x (считается сам), y, ширина иконки
static SDL_Renderer* g_renderer = nullptr;
static const int kRadius = 12;

static const char* kVersion = "v0.1";

// маленькая подпись по нижним углам: слева автор, справа версия
static void DrawFooter(int w, int h) {
    const float cs = float(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);   // 8 px при масштабе 1
    const float margin = 18.f;                                    // от краёв: не заходит на скруглённые углы
    const float y = h - 14.f - cs;
    const char* author = "by Aquarius";

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 90, 200, 255, 150);
    SDL_RenderDebugText(g_renderer, margin, y, author);
    SDL_RenderDebugText(g_renderer, w - margin - std::strlen(kVersion) * cs, y, kVersion);
}

static void Draw() {
    SDL_Rect nd[kMaxNoDragRects];
    SDL_SetRenderDrawColor(g_renderer, 30, 30, 30, 255);
    SDL_RenderClear(g_renderer);

    int w = 0, h = 0;
    SDL_GetCurrentRenderOutputSize(g_renderer, &w, &h);

    // кнопка меню у правого края
    const float margin = 20.f;
    g_menu.x = w - margin - g_menu.size;

    // подпись внизу на всех экранах, кроме главного (там "by Aquarius" уже вверху)
    if (g_screen != Screen::Main) DrawFooter(w, h);

    int ndCount = 0;
    nd[ndCount++] = { int(g_menu.x) - 6, int(g_menu.y) - 6,
                      int(g_menu.size) + 12, int(MenuButtonHeight(g_menu.size)) + 12 };

    if (!g_menu.open) {
        if (g_screen == Screen::Login) {
            DrawCenteredText(g_renderer, "LOGIN WINDOW", w, h, 3.0f, -60.0f);

            const float gap = 20.f;
            const float rowW = g_signIn.w + gap + g_signUp.w;
            g_signIn.x = (w - rowW) * 0.5f;
            g_signUp.x = g_signIn.x + g_signIn.w + gap;
            g_signIn.y = g_signUp.y = h * 0.5f + 10.f;

            TextButton_Draw(g_renderer, g_signIn);
            TextButton_Draw(g_renderer, g_signUp);

            nd[ndCount++] = TextButton_Rect(g_signIn);
            nd[ndCount++] = TextButton_Rect(g_signUp);
        }
        else if (g_screen == Screen::SignIn) {
            SignIn_Draw(g_renderer, w, h, nd, ndCount);
            Recover_Draw(g_renderer, w, h, nd, ndCount);   // окно FORGOT поверх экрана входа
        }
        else if (g_screen == Screen::Main) {
            Main_Draw(g_renderer, w, h, nd, ndCount);
        }
        else if (g_screen == Screen::Profile) {
            Profile_Draw(g_renderer, w, h, nd, ndCount);
        }
        else if (g_screen == Screen::Create) {
            Create_Draw(g_renderer, w, h, nd, ndCount);
        }
        else {
            SignUp_Draw(g_renderer, w, h, nd, ndCount);
        }
    }
    else {
        DrawCenteredText(g_renderer, "MENU", w, h, 3.0f);
    }

    SetNoDragRects(nd, ndCount);

    MenuButton_Draw(g_renderer, g_menu);
    DrawBorderGlow(g_renderer, w, h, kRadius, 1, 20, 120, 90.0f, 0.35f, 0.06f);

    SDL_RenderPresent(g_renderer);
}

static bool SDLCALL LiveResizeWatch(void* userdata, SDL_Event* e) {
    SDL_Window* window = static_cast<SDL_Window*>(userdata);

    if (e->type == SDL_EVENT_WINDOW_EXPOSED ||
        e->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
        e->type == SDL_EVENT_WINDOW_RESIZED) {

        if (SDL_GetWindowFromID(e->window.windowID) != window) return true;

        ApplyRoundRegion(window, kRadius);
        Draw();
    }
    return true;
}

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init error: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("myproject", 800, 600,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow error: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    g_renderer = SDL_CreateRenderer(window, nullptr);
    if (!g_renderer) {
        std::cerr << "SDL_CreateRenderer error: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    EnableWindowFrame(window, 0, 10);        // зоны кнопок выставляет Draw()
    SDL_SetWindowMinimumSize(window, 200, 150);
    ApplyRoundRegion(window, kRadius);

    // запомненный вход: сразу открываем главный экран
    {
        const std::string saved = Session_Load();
        if (!saved.empty()) {
            g_user = saved;
            Avatar_Load(g_user);
            Main_Open(g_user);
            g_screen = Screen::Main;
        }
    }

    SDL_AddEventWatch(LiveResizeWatch, window);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;

            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
                if (g_menu.open) g_menu.open = false;            // Esc закрывает меню
                else if (g_screen == Screen::Login) running = false;
                // на экранах входа/регистрации Esc обрабатывает сам экран (назад)
            }

            if (MenuButton_Handle(g_menu, e)) {
                g_menu.open = !g_menu.open;
            }

            if (!g_menu.open) {
                if (g_screen == Screen::Login) {
                    if (TextButton_Handle(g_signIn, e)) {
                        g_screen = Screen::SignIn;
                        SignIn_Open(window);
                    }
                    if (TextButton_Handle(g_signUp, e)) {
                        g_screen = Screen::SignUp;
                        SignUp_Open(window);
                    }
                }
                else if (g_screen == Screen::SignIn) {
                    if (Recover_IsOpen()) {
                        Recover_Handle(e);                 // модальное окно FORGOT: события только ему
                    }
                    else {
                        const SignInResult res = SignIn_Handle(e);
                        if (res == SignInResult::Forgot) {
                            Recover_Open(window);
                        }
                        else if (res == SignInResult::Done) {
                            SignInData d = SignIn_Data();
                            std::string user;
                            const bool ok = UserStore_Verify(d.login, d.password, &user);
                            std::fill(d.password.begin(), d.password.end(), '\0');   // копия пароля не нужна

                            if (ok) {
                                SignIn_Close(window);
                                g_user = user;
                                Session_Save(g_user);         // запомнить вход до следующего запуска
                                Avatar_Load(g_user);          // сохранённый аватар, если есть
                                Main_Open(g_user);
                                g_screen = Screen::Main;
                            }
                            else {
                                SignIn_SetError("WRONG LOGIN OR PASSWORD");   // остаёмся на экране входа
                            }
                        }
                        else if (res == SignInResult::Back) {
                            SignIn_Close(window);
                            g_screen = Screen::Login;
                        }
                    }
                }
                else if (g_screen == Screen::Main) {
                    const MainResult res = Main_Handle(e);
                    if (res == MainResult::Profile) {
                        Profile_Open(window, g_user);
                        g_screen = Screen::Profile;
                    }
                    else if (res == MainResult::Import) {
                        // сюда: действие кнопки IMPORT (окно "+")
                    }
                    else if (res == MainResult::Create) {
                        Create_Open(window);
                        g_screen = Screen::Create;
                    }
                    else if (res == MainResult::Logout) {
                        Session_Clear();                  // выход: при следующем запуске снова экран входа
                        Main_Close();
                        Avatar_Clear();
                        g_user.clear();
                        g_screen = Screen::Login;
                    }
                }
                else if (g_screen == Screen::Profile) {
                    if (Profile_Handle(e) == ProfileResult::Back) {
                        Profile_Close();
                        g_screen = Screen::Main;
                    }
                }
                else if (g_screen == Screen::Create) {
                    const CreateResult res = Create_Handle(e);
                    if (res == CreateResult::Done) {
                        const CreateData d = Create_Data();
                        // сюда: сохранить созданное (d.name, d.description, d.imagePath)
                        (void)d;
                        Create_Close(window);
                        g_screen = Screen::Main;
                    }
                    else if (res == CreateResult::Back) {
                        Create_Close(window);
                        g_screen = Screen::Main;
                    }
                }
                else {
                    const SignUpResult res = SignUp_Handle(e);
                    if (res != SignUpResult::None) {
                        if (res == SignUpResult::Done) {
                            const SignUpData d = SignUp_Data();
                            // сюда: сохранить пользователя (login, хэш пароля, хэш ключевого слова)
                            (void)d;
                        }
                        SignUp_Close(window);
                        g_screen = Screen::Login;
                    }

                    if (g_screen == Screen::SignUp && SignUp_Poll() == SignUpResult::Done) {
                        SignUp_Close(window);
                        g_screen = Screen::Login;
                    }
                }
            }
        }
        if (g_screen == Screen::Profile) Profile_Poll();   // файл, выбранный в диалоге аватара
        if (g_screen == Screen::Create) Create_Poll();     // файл, выбранный в диалоге картинки
        Draw();
    }

    SDL_RemoveEventWatch(LiveResizeWatch, window);
    Recover_Destroy();
    Profile_Destroy();
    Create_Destroy();
    Main_Destroy();
    Avatar_Destroy();
    SignIn_Destroy();
    SignUp_Destroy();
    TextButton_Destroy(g_signIn);
    TextButton_Destroy(g_signUp);
    MenuButton_Destroy(g_menu);
    DestroyBorderCache();
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
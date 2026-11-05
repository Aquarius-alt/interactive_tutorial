#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
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
#include "SaveOverlay.h"
#include "RepoStore.h"
#include "OpenedScreen.h"
#include "Localization.h"
#include <algorithm>
#include <string>
#include <vector>
#include <cmath>

enum class Screen { Login, SignIn, SignUp, Main, Profile, Create, Opened };
static Screen g_screen = Screen::Login;
static std::string g_user;   // логин вошедшего пользователя (как записан при регистрации)
static std::string g_editDir;   // папка репозитория, который сейчас правят на экране CREATE ("" = создание нового)
static bool g_repoSaving = false;   // идёт сохранение нового репозитория (полоса загрузки поверх главного экрана)
static TextButton g_signIn{ 0, 0, 150, 40, "SIGN IN" };
static TextButton g_signUp{ 0, 0, 150, 40, "SIGN UP" };
static MenuButton g_menu{ 0, 16, 18 };   // x (считается сам), y, ширина иконки
// Настройки меню приложения.
static bool g_menuInterfacePage = false;
static bool g_lngDropdownOpen = false;
static TextButton g_menuPanel{ 0, 0, 360, 230, "" };
static TextButton g_interfaceButton{ 0, 0, 280, 48, "INTERFACE" };
static TextButton g_interfaceBackButton{ 0, 0, 110, 38, "BACK" };
static TextButton g_lngButton{ 0, 0, 150, 42, "ENG  v" };
static TextButton g_lngEngOption{ 0, 0, 150, 38, "ENG" };
static TextButton g_lngRusOption{ 0, 0, 150, 38, "RUS" };
static SDL_Renderer* g_renderer = nullptr;
static SDL_Rect g_minimizeRect{ 0, 0, 28, 28 };
static SDL_Rect g_closeRect{ 0, 0, 28, 28 };
static bool g_minimizeHovered = false;
static bool g_closeHovered = false;
static const int kRadius = 12;

// Текстуры иконок управления окном используют ту же модель свечения,
// что и MENU: мягкий ореол отдельной текстурой + яркое ядро.
namespace {
    constexpr int kControlIconSize = 28;
    constexpr int kControlGlowPad = 10;
    constexpr Uint8 kControlR = 90;
    constexpr Uint8 kControlG = 200;
    constexpr Uint8 kControlB = 255;

    struct ControlIconTextures {
        SDL_Texture* core = nullptr;
        SDL_Texture* glow = nullptr;
        SDL_Renderer* owner = nullptr;
    };

    ControlIconTextures g_closeTextures;
    ControlIconTextures g_minimizeTextures;

    Uint32 PackControlPixel(Uint8 alpha) {
        return (Uint32(alpha) << 24) | (Uint32(kControlB) << 16) |
               (Uint32(kControlG) << 8) | Uint32(kControlR);
    }

    float DistanceToSegment(float px, float py, float x1, float y1,
                            float x2, float y2) {
        const float dx = x2 - x1;
        const float dy = y2 - y1;
        const float lengthSquared = dx * dx + dy * dy;
        float t = 0.0f;
        if (lengthSquared > 0.0f)
            t = std::clamp(((px - x1) * dx + (py - y1) * dy) / lengthSquared, 0.0f, 1.0f);
        const float nearestX = x1 + t * dx;
        const float nearestY = y1 + t * dy;
        const float ox = px - nearestX;
        const float oy = py - nearestY;
        return std::sqrt(ox * ox + oy * oy);
    }

    void DestroyControlIcon(ControlIconTextures& icon) {
        if (icon.core) SDL_DestroyTexture(icon.core);
        if (icon.glow) SDL_DestroyTexture(icon.glow);
        icon.core = nullptr;
        icon.glow = nullptr;
        icon.owner = nullptr;
    }

    void BuildControlIcon(SDL_Renderer* renderer, ControlIconTextures& icon, bool isClose) {
        if (icon.owner == renderer && icon.core && icon.glow) return;
        DestroyControlIcon(icon);

        const int textureSize = kControlIconSize + kControlGlowPad * 2;
        std::vector<Uint32> corePixels(size_t(textureSize) * textureSize, 0);
        std::vector<Uint32> glowPixels(size_t(textureSize) * textureSize, 0);

        for (int py = 0; py < textureSize; ++py) {
            for (int px = 0; px < textureSize; ++px) {
                const float x = static_cast<float>(px - kControlGlowPad) + 0.5f;
                const float y = static_cast<float>(py - kControlGlowPad) + 0.5f;
                float distance = 1000.0f;

                if (isClose) {
                    distance = std::min(
                        DistanceToSegment(x, y, 9.0f, 9.0f, 19.0f, 19.0f),
                        DistanceToSegment(x, y, 19.0f, 9.0f, 9.0f, 19.0f));
                } else {
                    distance = DistanceToSegment(x, y, 8.0f, 14.0f, 20.0f, 14.0f);
                }

                const float coreCoverage = std::clamp(1.25f - distance, 0.0f, 1.0f);
                if (coreCoverage > 0.0f) {
                    corePixels[size_t(py) * textureSize + px] =
                        PackControlPixel(static_cast<Uint8>(coreCoverage * 255.0f));
                }

                // Значения силы и затухания подобраны под MenuButton_Draw().
                const float glow = 0.30f * std::exp(-distance / 2.6f);
                if (glow > 0.004f) {
                    glowPixels[size_t(py) * textureSize + px] =
                        PackControlPixel(static_cast<Uint8>(glow * 255.0f));
                }
            }
        }

        icon.core = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC, textureSize, textureSize);
        icon.glow = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC, textureSize, textureSize);
        if (!icon.core || !icon.glow) {
            DestroyControlIcon(icon);
            return;
        }

        SDL_UpdateTexture(icon.core, nullptr, corePixels.data(), textureSize * 4);
        SDL_UpdateTexture(icon.glow, nullptr, glowPixels.data(), textureSize * 4);
        SDL_SetTextureBlendMode(icon.core, SDL_BLENDMODE_BLEND);
        SDL_SetTextureBlendMode(icon.glow, SDL_BLENDMODE_BLEND);
        icon.owner = renderer;
    }

    void DrawControlIcon(SDL_Renderer* renderer, ControlIconTextures& icon,
                         const SDL_Rect& rect, bool isClose, bool hovered) {
        BuildControlIcon(renderer, icon, isClose);
        if (!icon.core || !icon.glow) return;

        const int textureSize = kControlIconSize + kControlGlowPad * 2;
        const SDL_FRect destination{
            static_cast<float>(rect.x - kControlGlowPad),
            static_cast<float>(rect.y - kControlGlowPad),
            static_cast<float>(textureSize),
            static_cast<float>(textureSize)
        };

        SDL_SetTextureAlphaMod(icon.glow, hovered ? 255 : 190);
        SDL_RenderTexture(renderer, icon.glow, nullptr, &destination);
        if (hovered) SDL_RenderTexture(renderer, icon.glow, nullptr, &destination);

        SDL_SetTextureAlphaMod(icon.core, 255);
        SDL_RenderTexture(renderer, icon.core, nullptr, &destination);
    }
}

// Язык интерфейса хранится отдельно от сессии пользователя, поэтому
// настройка сохраняется и после выхода из аккаунта или перезапуска приложения.
static std::string LanguagePreferencePath() {
    char* prefPath = SDL_GetPrefPath("myproject", "settings");
    if (!prefPath) return {};

    std::string path(prefPath);
    SDL_free(prefPath);
    if (path.empty()) return {};
    return path + "language.cfg";
}

static void LoadLanguagePreference() {
    const std::string path = LanguagePreferencePath();
    if (path.empty()) {
        UiRussianFlag() = false;
        return;
    }

    std::ifstream input(path, std::ios::binary);
    std::string language;
    if (input && std::getline(input, language)) {
        if (!language.empty() && language.back() == '\r') language.pop_back();
        UiRussianFlag() = (language == "RUS");
    } else {
        UiRussianFlag() = false; // язык по умолчанию для первого запуска — ENG
    }
}

static void SaveLanguagePreference(bool russian) {
    const std::string path = LanguagePreferencePath();
    if (path.empty()) return;

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return;
    output << (russian ? "RUS" : "ENG");
    output.flush();
}

static void SetUiLanguage(bool russian) {
    UiRussianFlag() = russian;
    SaveLanguagePreference(russian);
}

static const char* kVersion = "v0.1";

// маленькая подпись по нижним углам: слева автор, справа версия
static void DrawFooter(int w, int h) {
    constexpr int fontHeight = 8;
    const float margin = 18.f;
    const float y = static_cast<float>(h) - 14.f - fontHeight;
    const std::string author = "by Aquarius";

    DrawUtf8Text(g_renderer, author, margin, y, fontHeight, 90, 200, 255, 150);
    DrawUtf8Text(g_renderer, kVersion,
        static_cast<float>(w) - margin - Utf8TextWidth(kVersion, fontHeight),
        y, fontHeight, 90, 200, 255, 150);
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

    // Кнопки управления безрамочным окном: слева от кнопки MENU.
    const int menuCenterX = static_cast<int>(std::lround(g_menu.x + g_menu.size * 0.5f));
    const int menuCenterY = static_cast<int>(std::lround(g_menu.y + MenuButtonHeight(g_menu.size) * 0.5f));
    const int controlY = menuCenterY - 14;
    // Центры CLOSE, MINIMIZE и MENU находятся на одной оси с шагом 34 px.
    g_minimizeRect = { menuCenterX - 34 - 14, controlY, 28, 28 };
    g_closeRect = { menuCenterX - 68 - 14, controlY, 28, 28 };
    nd[ndCount++] = g_minimizeRect;
    nd[ndCount++] = g_closeRect;

    if (!g_menu.open) {
        if (g_screen == Screen::Login) {
            g_signIn.label = UiText("SIGN IN", "ВХОД");
            g_signUp.label = UiText("SIGN UP", "РЕГИСТРАЦИЯ");
            DrawCenteredText(g_renderer, UiText("LOGIN WINDOW", "ОКНО ВХОДА"), w, h, 3.0f, -60.0f);

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
        else if (g_screen == Screen::Opened) {
            Opened_Draw(g_renderer, w, h, nd, ndCount);
        }
        else {
            SignUp_Draw(g_renderer, w, h, nd, ndCount);
        }
    }
    else {
        // Компактное меню приложения и отдельная вкладка INTERFACE.
        const float panelW = 360.f;
        const float panelH = g_menuInterfacePage ? 250.f : 190.f;
        const float px = (w - panelW) * 0.5f;
        const float py = (h - panelH) * 0.5f;
        g_menuPanel.x = px + 12.f;
        g_menuPanel.y = py + 34.f;
        g_menuPanel.w = panelW - 24.f;
        g_menuPanel.h = panelH - 46.f;
        TextButton_Draw(g_renderer, g_menuPanel);
        nd[ndCount++] = TextButton_Rect(g_menuPanel);
        DrawCenteredText(g_renderer, g_menuInterfacePage ? UiText("INTERFACE", "ИНТЕРФЕЙС") : UiText("MENU", "МЕНЮ"),
            w, h, 2.4f, py - h * 0.5f + 8.f);

        if (!g_menuInterfacePage) {
            g_interfaceButton.label = UiText("INTERFACE", "ИНТЕРФЕЙС");
            g_interfaceButton.x = px + 40.f;
            g_interfaceButton.y = py + 82.f;
            g_interfaceButton.w = panelW - 80.f;
            TextButton_Draw(g_renderer, g_interfaceButton);
            nd[ndCount++] = TextButton_Rect(g_interfaceButton);
        }
        else {
            g_interfaceBackButton.label = UiText("BACK", "НАЗАД");
            g_interfaceBackButton.x = px + 30.f;
            g_interfaceBackButton.y = py + 184.f;
            TextButton_Draw(g_renderer, g_interfaceBackButton);

            // Подпись настройки языка и выпадающий список.
            DrawUtf8Text(g_renderer, UiText("LNG", "ЯЗЫК"), px + 48.f, py + 91.f, 18, 90, 200, 255, 255);
            g_lngButton.x = px + 160.f;
            g_lngButton.y = py + 78.f;
            g_lngButton.label = UiRussianFlag() ? "RUS  v" : "ENG  v";
            TextButton_Draw(g_renderer, g_lngButton);
            nd[ndCount++] = TextButton_Rect(g_lngButton);
            nd[ndCount++] = TextButton_Rect(g_interfaceBackButton);

            if (g_lngDropdownOpen) {
                g_lngEngOption.x = g_lngButton.x;
                g_lngEngOption.y = g_lngButton.y + g_lngButton.h + 8.f;
                g_lngRusOption.x = g_lngButton.x;
                g_lngRusOption.y = g_lngEngOption.y + g_lngEngOption.h + 4.f;

                TextButton_Draw(g_renderer, g_lngEngOption);
                TextButton_Draw(g_renderer, g_lngRusOption);
                nd[ndCount++] = TextButton_Rect(g_lngEngOption);
                nd[ndCount++] = TextButton_Rect(g_lngRusOption);
            }
        }
    }

    // полоса сохранения репозитория: поверх главного экрана; во время неё кнопки окна не нажимаются
    if (g_repoSaving) {
        SaveOverlay_Draw(g_renderer, w, h);
        ndCount = 0;
        nd[ndCount++] = { 0, 0, 0, 0 };
    }

    SetNoDragRects(nd, ndCount);

    // CLOSE и MINIMIZE получают то же текстурное неоновое свечение, что и MENU.
    DrawControlIcon(g_renderer, g_minimizeTextures, g_minimizeRect, false, g_minimizeHovered);
    DrawControlIcon(g_renderer, g_closeTextures, g_closeRect, true, g_closeHovered);

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

    // Применяем сохранённый язык до первого кадра и открытия любого экрана.
    LoadLanguagePreference();

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

            if (e.type == SDL_EVENT_MOUSE_MOTION) {
                const SDL_Point mousePoint{ static_cast<int>(e.motion.x), static_cast<int>(e.motion.y) };
                g_closeHovered = SDL_PointInRect(&mousePoint, &g_closeRect);
                g_minimizeHovered = SDL_PointInRect(&mousePoint, &g_minimizeRect);
            } else if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
                g_closeHovered = false;
                g_minimizeHovered = false;
            }

            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                const int mx = static_cast<int>(e.button.x);
                const int my = static_cast<int>(e.button.y);
                const SDL_Point mousePoint{ mx, my };
                if (SDL_PointInRect(&mousePoint, &g_closeRect)) {
                    running = false;
                    continue;
                }
                if (SDL_PointInRect(&mousePoint, &g_minimizeRect)) {
                    SDL_MinimizeWindow(window);
                    continue;
                }
            }

            if (g_repoSaving) continue;   // пока идёт сохранение, ввод игнорируется

            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
                if (g_menu.open && g_lngDropdownOpen) g_lngDropdownOpen = false;
                else if (g_menu.open && g_menuInterfacePage) g_menuInterfacePage = false;
                else if (g_menu.open) g_menu.open = false;
                else if (g_screen == Screen::Login) running = false;
                // на экранах входа/регистрации Esc обрабатывает сам экран (назад)
            }

            if (MenuButton_Handle(g_menu, e)) {
                g_menu.open = !g_menu.open;
                g_menuInterfacePage = false;
                g_lngDropdownOpen = false;
            }

            // События меню настроек обрабатываются отдельно от экранов приложения.
            if (g_menu.open) {
                if (!g_menuInterfacePage) {
                    if (TextButton_Handle(g_interfaceButton, e))
                        g_menuInterfacePage = true;
                }
                else {
                    if (TextButton_Handle(g_interfaceBackButton, e)) {
                        g_menuInterfacePage = false;
                        g_lngDropdownOpen = false;
                    }
                    else if (TextButton_Handle(g_lngButton, e)) {
                        g_lngDropdownOpen = !g_lngDropdownOpen;
                    }
                    else if (g_lngDropdownOpen && TextButton_Handle(g_lngEngOption, e)) {
                        SetUiLanguage(false);
                        g_lngDropdownOpen = false;
                    }
                    else if (g_lngDropdownOpen && TextButton_Handle(g_lngRusOption, e)) {
                        SetUiLanguage(true);
                        g_lngDropdownOpen = false;
                    }
                }
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
                        // Открываем папку текущего пользователя с сохранёнными репозиториями.
                        RepoStore_OpenUserFolder(g_user);
                    }
                    else if (res == MainResult::Create) {
                        g_editDir.clear();
                        Create_Open(window);
                        g_screen = Screen::Create;
                    }
                    else if (res == MainResult::Edit) {
                        // пункт EDIT в меню карточки: окно CREATE с данными этого репозитория
                        CreateData d;
                        const std::string dir = Main_RepoDir();
                        if (RepoStore_Load(dir, d)) {
                            g_editDir = dir;
                            Create_OpenEdit(window, d);
                            g_screen = Screen::Create;
                        }
                    }
                    else if (res == MainResult::Open) {
                        Opened_Open(window, Main_RepoDir());
                        g_screen = Screen::Opened;
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
                else if (g_screen == Screen::Opened) {
                    if (Opened_Handle(window, e) == OpenedResult::Back) g_screen = Screen::Main;
                }
                else if (g_screen == Screen::Create) {
                    const CreateResult res = Create_Handle(e);
                    if (res == CreateResult::Done) {
                        // окно CREATE закрывается, поверх главного экрана идёт полоса загрузки,
                        // а данные в фоне записываются в Documents\myproject\<логин>\<название>
                        SDL_StopTextInput(window);
                        g_screen = Screen::Main;
                        g_repoSaving = true;
                        SaveOverlay_StartTask(
                            [user = g_user, d = Create_Data(), dir = g_editDir]() { return RepoStore_Save(user, d, nullptr, dir); },
                            "LOCAL", "STORAGE", "DOCS", "FOLDER");
                    }
                    else if (res == CreateResult::Back) {
                        Create_Close(window);
                        g_editDir.clear();
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
        if (g_repoSaving) {
            const SaveResult sr = SaveOverlay_Poll();
            if (sr == SaveResult::Ok) {
                Create_Close(window);                       // записано: поля и вложения больше не нужны
                g_editDir.clear();
                Main_Reload();                              // новый репозиторий появляется на главном экране
                g_repoSaving = false;
            }
            else if (sr == SaveResult::Failed) {
                g_repoSaving = false;                       // не записалось: возвращаем экран CREATE с введёнными данными
                Create_Resume(window, "SAVE FAILED");
                g_screen = Screen::Create;
            }
        }
        if (g_screen == Screen::Profile) Profile_Poll();   // файл, выбранный в диалоге аватара
        if (g_screen == Screen::Create) Create_Poll();     // файл, выбранный в диалоге картинки
        Draw();
    }

    SDL_RemoveEventWatch(LiveResizeWatch, window);
    SaveOverlay_Destroy();
    Recover_Destroy();
    Profile_Destroy();
    Create_Destroy();
    Opened_Destroy();
    Main_Destroy();
    Avatar_Destroy();
    SignIn_Destroy();
    SignUp_Destroy();
    TextButton_Destroy(g_signIn);
    TextButton_Destroy(g_signUp);
    TextButton_Destroy(g_menuPanel);
    TextButton_Destroy(g_interfaceButton);
    TextButton_Destroy(g_interfaceBackButton);
    TextButton_Destroy(g_lngButton);
    TextButton_Destroy(g_lngEngOption);
    TextButton_Destroy(g_lngRusOption);
    MenuButton_Destroy(g_menu);
    DestroyControlIcon(g_closeTextures);
    DestroyControlIcon(g_minimizeTextures);
    DestroyBorderCache();
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
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
#include "RanksScreen.h"
#include "CreateScreen.h"
#include "Avatar.h"
#include "UserStore.h"
#include "SaveOverlay.h"
#include "RepoStore.h"
#include "OpenedScreen.h"
#include "TheoryDialog.h"
#include "Localization.h"
#include "Theme.h"
#include "Logo.h"
#include "Splash.h"
#include "Tutorial.h"
#include "CapriVisit.h"
#include "Styles.h"
#include <algorithm>
#include <string>
#include <vector>
#include <cmath>

enum class Screen { Login, SignIn, SignUp, Main, Profile, Ranks, Create, Opened };
static Screen g_screen = Screen::Login;
static std::string g_user;   // логин вошедшего пользователя (как записан при регистрации)
static std::string g_editDir;   // папка репозитория, который сейчас правят на экране CREATE ("" = создание нового)
static bool g_repoSaving = false;   // идёт сохранение нового репозитория (полоса загрузки поверх главного экрана)
static TextButton g_signIn{ 0, 0, 150, 40, "SIGN IN" };
static TextButton g_signUp{ 0, 0, 150, 40, "SIGN UP" };
static MenuButton g_menu{ 0, 0, 26 };    // x и y считаются сами (правый нижний угол), диаметр шестерни
// Настройки меню приложения.
static bool g_menuInterfacePage = false;
static bool g_lngDropdownOpen = false;
static TextButton g_menuPanel{ 0, 0, 360, 230, "" };
static TextButton g_interfaceButton{ 0, 0, 280, 48, "INTERFACE" };
static TextButton g_tutorialButton{ 0, 0, 280, 48, "TUTORIAL" };
static TextButton g_interfaceBackButton{ 0, 0, 110, 38, "BACK" };
static TextButton g_styleButton{ 0, 0, 150, 42, "CAPRI" };
static TextButton g_capriToggle{ 0, 0, 150, 42, "ON" };
static TextButton g_lngButton{ 0, 0, 150, 42, "ENG  v" };
static TextButton g_lngEngOption{ 0, 0, 150, 38, "ENG" };
static TextButton g_lngRusOption{ 0, 0, 150, 38, "RUS" };
static SDL_Renderer* g_renderer = nullptr;
static SDL_Rect g_minimizeRect{ 0, 0, 28, 28 };
static SDL_Rect g_closeRect{ 0, 0, 28, 28 };
static bool g_minimizeHovered = false;
static bool g_closeHovered = false;
static const int kRadius = 0;   // угловатое окно в стиле кибердека
static SDL_Rect g_fullscreenRect{ 0, 0, 28, 28 };
static bool g_fullscreenHovered = false;
// Полноэкранный режим — дополнительная функция: приложение стартует в обычном окне,
// переключение кнопкой в правом верхнем углу или клавишей F11.
static bool g_fullscreen = false;
static int CurRadius() { return g_fullscreen ? 0 : kRadius; }
static int CurResizeBorder() { return g_fullscreen ? 0 : 10; }

// Текстуры иконок управления окном используют ту же модель свечения,
// что и MENU: мягкий ореол отдельной текстурой + яркое ядро.
namespace {
    constexpr int kControlIconSize = 28;
    constexpr int kControlGlowPad = 10;
    constexpr Uint8 kControlR = 96;
    constexpr Uint8 kControlG = 228;
    constexpr Uint8 kControlB = 232;

    struct ControlIconTextures {
        SDL_Texture* core = nullptr;
        SDL_Texture* glow = nullptr;
        SDL_Renderer* owner = nullptr;
    };

    ControlIconTextures g_closeTextures;
    ControlIconTextures g_minimizeTextures;
    ControlIconTextures g_fullscreenTextures;

    Uint32 PackControlPixel(Uint8 alpha) {
        (void)kControlR; (void)kControlG; (void)kControlB;
        return (Uint32(alpha) << 24) | 0x00FFFFFFu;   // белая маска; цвет задаётся SDL_SetTextureColorMod (меняется со стилем)
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

    void BuildControlIcon(SDL_Renderer* renderer, ControlIconTextures& icon, int kind) {
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

                if (kind == 2) {   // значок полного экрана: квадрат
                    distance = std::min(std::min(
                        DistanceToSegment(x, y, 8.0f, 8.0f, 20.0f, 8.0f),
                        DistanceToSegment(x, y, 20.0f, 8.0f, 20.0f, 20.0f)),
                        std::min(
                        DistanceToSegment(x, y, 20.0f, 20.0f, 8.0f, 20.0f),
                        DistanceToSegment(x, y, 8.0f, 20.0f, 8.0f, 8.0f)));
                } else if (kind == 1) {
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
                         const SDL_Rect& rect, int kind, bool hovered) {
        BuildControlIcon(renderer, icon, kind);
        if (!icon.core || !icon.glow) return;

        const int textureSize = kControlIconSize + kControlGlowPad * 2;
        const SDL_FRect destination{
            static_cast<float>(rect.x - kControlGlowPad),
            static_cast<float>(rect.y - kControlGlowPad),
            static_cast<float>(textureSize),
            static_cast<float>(textureSize)
        };

        SDL_SetTextureColorMod(icon.glow, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b);
        SDL_SetTextureColorMod(icon.core, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b);
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

// Флаг «обучение пройдено»: отдельный файл рядом с языком, чтобы не показывать обучение каждый запуск.
static std::string TutorialPreferencePath() {
    char* prefPath = SDL_GetPrefPath("myproject", "settings");
    if (!prefPath) return {};
    std::string path(prefPath);
    SDL_free(prefPath);
    if (path.empty()) return {};
    return path + "tutorial.cfg";
}

static bool TutorialAlreadyDone() {
    const std::string path = TutorialPreferencePath();
    if (path.empty()) return true;   // некуда сохранить флаг: не надоедаем обучением на каждом запуске
    std::ifstream input(path, std::ios::binary);
    std::string v;
    return input && std::getline(input, v) && v.rfind("DONE", 0) == 0;
}

static void SaveTutorialDone() {
    const std::string path = TutorialPreferencePath();
    if (path.empty()) return;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (output) output << "DONE";
}

// Настройка «Капри заглядывает раз в 30 минут» (ON / OFF) хранится рядом с языком.
static std::string CapriPreferencePath() {
    char* prefPath = SDL_GetPrefPath("myproject", "settings");
    if (!prefPath) return {};
    std::string path(prefPath);
    SDL_free(prefPath);
    if (path.empty()) return {};
    return path + "capri.cfg";
}

static void LoadCapriPreference() {
    const std::string path = CapriPreferencePath();
    if (path.empty()) return;
    std::ifstream input(path, std::ios::binary);
    std::string v;
    if (input && std::getline(input, v)) CapriVisit::g_enabled = (v.rfind("OFF", 0) != 0);
}

static void SetCapriVisits(bool on) {
    CapriVisit::SetEnabled(on);
    const std::string path = CapriPreferencePath();
    if (path.empty()) return;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (output) output << (on ? "ON" : "OFF");
}

// Стиль интерфейса (CAPRI / ASTOLFO) хранится рядом с языком.
static std::string StylePreferencePath() {
    char* prefPath = SDL_GetPrefPath("myproject", "settings");
    if (!prefPath) return {};
    std::string path(prefPath);
    SDL_free(prefPath);
    if (path.empty()) return {};
    return path + "style.cfg";
}

static void LoadStylePreference() {
    int index = 0;
    const std::string path = StylePreferencePath();
    if (!path.empty()) {
        std::ifstream input(path, std::ios::binary);
        std::string v;
        if (input && std::getline(input, v)) {
            while (!v.empty() && (v.back() == '\r' || v.back() == '\n')) v.pop_back();
            index = Styles::IndexById(v);
        }
    }
    Styles::Apply(index);
}

static void CycleStyle() {
    Styles::Cycle();
    const std::string path = StylePreferencePath();
    if (path.empty()) return;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (output) output << Styles::Current().id;
}

static const char* kVersion = "v1.0.0";

// маленькая подпись внизу: справа версия (слева теперь логотип, он рисуется в Draw())
static void DrawFooter(int w, int h) {
    constexpr int fontHeight = 8;
    const float y = static_cast<float>(h) - 14.f - fontHeight;
    // справа внизу стоит шестерня настроек: версия сдвинута левее неё
    const float versionRight = static_cast<float>(w) - 20.f - g_menu.size - 14.f;
    DrawUtf8Text(g_renderer, kVersion,
        versionRight - Utf8TextWidth(kVersion, fontHeight),
        y, fontHeight, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 150);
}

// Окно поверх экрана (FORGOT, новая теория / тест, IMPORT / CREATE, просмотр картинки): пока оно открыто,
// шестерня настроек лежит под ним и не нажимается.
static bool ModalOpen() {
    switch (g_screen) {
    case Screen::SignIn: return Recover_IsOpen();
    case Screen::Create: return Theory_IsOpen();
    case Screen::Main:   return Main_IsModalOpen();
    case Screen::Opened: return Opened_IsViewing();
    case Screen::Ranks:  return Ranks_IsModalOpen();
    default:             return false;
    }
}

// Рисует один кадр приложения в текущую цель рендера (без SDL_RenderPresent):
// заставка Splash рисует его в текстуру и склеивает с собой глитчем.
static void DrawScene() {
    SDL_Rect nd[kMaxNoDragRects];

    // смена экрана запускает анимацию перехода (экран меняется сразу, поверх открываются полосы)
    static Screen s_prevScreen = g_screen;
    if (s_prevScreen != g_screen) { Theme::TransitionStart(); s_prevScreen = g_screen; }
    int w = 0, h = 0;
    SDL_GetCurrentRenderOutputSize(g_renderer, &w, &h);
    Theme::DrawBackground(g_renderer, w, h);

    // кнопка настроек (шестерня): правый нижний угол
    const float margin = 20.f;
    g_menu.x = w - margin - g_menu.size;
    g_menu.y = h - margin - MenuButtonHeight(g_menu.size);

    // подпись внизу на всех экранах, кроме главного (там "by Aquarius" уже вверху)
    // (и пока открыто окно теории на весь экран: оно упирается в подпись)
    // логотип в левом нижнем углу на месте бывшей подписи «by Aquarius» (на всех экранах)
    if (!(g_screen == Screen::Create && Theory_IsOpen()) &&
        !(g_screen == Screen::Ranks && Ranks_IsModalOpen()))
        Logo::Draw(g_renderer, 26.f, float(h) - 26.f - 56.f, 56.f);   // чуть в стороне от угловых скобок рамки

    if (g_screen != Screen::Main && !(g_screen == Screen::Create && Theory_IsOpen()) &&
        !(g_screen == Screen::Ranks && Ranks_IsModalOpen())) DrawFooter(w, h);

    int ndCount = 0;
    nd[ndCount++] = { int(g_menu.x) - 6, int(g_menu.y) - 6,
                      int(g_menu.size) + 12, int(MenuButtonHeight(g_menu.size)) + 12 };

    // Кнопки управления безрамочным окном: в правом верхнем углу, на месте бывшей кнопки меню.
    // CLOSE у самого края, MINIMIZE левее него; центры на одной оси с шагом 34 px.
    const int controlCenterX = static_cast<int>(std::lround(float(w) - margin - 9.f));
    const int controlY = 8;
    g_closeRect = { controlCenterX - 14, controlY, 28, 28 };
    g_fullscreenRect = { controlCenterX - 34 - 14, controlY, 28, 28 };
    g_minimizeRect = { controlCenterX - 68 - 14, controlY, 28, 28 };
    nd[ndCount++] = g_minimizeRect;
    nd[ndCount++] = g_fullscreenRect;
    nd[ndCount++] = g_closeRect;

    // Шестерня рисуется до содержимого экрана: окна поверх экрана перекрывают её (и затемняют), а не наоборот.
    MenuButton_Draw(g_renderer, g_menu);

    if (!g_menu.open) {
        if (g_screen == Screen::Login) {
            g_signIn.label = UiText("SIGN IN", "ВХОД");
            g_signUp.label = UiText("SIGN UP", "РЕГИСТРАЦИЯ");
            g_signIn.w = g_signUp.w = std::max(150.f, Utf8TextWidth(g_signUp.label, 16) + 32.f);   // обе кнопки одной ширины
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
        else if (g_screen == Screen::Ranks) {
            Ranks_Draw(g_renderer, w, h, nd, ndCount);
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
        const float panelH = g_menuInterfacePage ? 306.f : 240.f;
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

            g_tutorialButton.label = UiText("TUTORIAL", "ОБУЧЕНИЕ");
            g_tutorialButton.x = px + 40.f;
            g_tutorialButton.y = py + 142.f;
            g_tutorialButton.w = panelW - 80.f;
            TextButton_Draw(g_renderer, g_tutorialButton);
            nd[ndCount++] = TextButton_Rect(g_tutorialButton);
        }
        else {
            g_interfaceBackButton.label = UiText("BACK", "НАЗАД");
            g_interfaceBackButton.x = px + 30.f;
            g_interfaceBackButton.y = py + 240.f;
            TextButton_Draw(g_renderer, g_interfaceBackButton);

            // Подпись настройки языка и выпадающий список.
            DrawUtf8Text(g_renderer, UiText("LNG", "ЯЗЫК"), px + 48.f, py + 91.f, 18, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 255);
            g_lngButton.x = px + 160.f;
            g_lngButton.y = py + 78.f;
            g_lngButton.label = UiRussianFlag() ? "RUS  v" : "ENG  v";
            TextButton_Draw(g_renderer, g_lngButton);
            nd[ndCount++] = TextButton_Rect(g_lngButton);
            nd[ndCount++] = TextButton_Rect(g_interfaceBackButton);

            // Капри: заглядывать раз в 30 минут или нет (при открытом списке языков строка скрыта под ним)
            if (!g_lngDropdownOpen) {
                DrawUtf8TextFit(g_renderer, UiText(Styles::Current().nameEn, Styles::Current().nameRu), px + 48.f, py + 143.f + 8.f, 106.f, 18, false, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 255);
                g_capriToggle.x = px + 160.f;
                g_capriToggle.y = py + 130.f;
                g_capriToggle.label = CapriVisit::g_enabled ? UiText("ON", "ВКЛ") : UiText("OFF", "ВЫКЛ");
                TextButton_Draw(g_renderer, g_capriToggle);
                nd[ndCount++] = TextButton_Rect(g_capriToggle);

                // Стиль интерфейса: кнопка переключает стили по кругу
                DrawUtf8Text(g_renderer, UiText("STYLE", "СТИЛЬ"), px + 48.f, py + 195.f, 18, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 255);
                g_styleButton.x = px + 160.f;
                g_styleButton.y = py + 182.f;
                g_styleButton.label = UiText(Styles::Current().nameEn, Styles::Current().nameRu);
                TextButton_Draw(g_renderer, g_styleButton);
                nd[ndCount++] = TextButton_Rect(g_styleButton);
            }

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

    if (Tutorial::Active()) ndCount = Tutorial::AppendNoDrag(nd, ndCount, kMaxNoDragRects);
    ndCount = CapriVisit::AppendNoDrag(nd, ndCount, kMaxNoDragRects);
    SetNoDragRects(nd, ndCount);

    // CLOSE и MINIMIZE получают то же текстурное неоновое свечение, что и MENU.
    DrawControlIcon(g_renderer, g_minimizeTextures, g_minimizeRect, 0, g_minimizeHovered);
    DrawControlIcon(g_renderer, g_fullscreenTextures, g_fullscreenRect, 2, g_fullscreenHovered);
    DrawControlIcon(g_renderer, g_closeTextures, g_closeRect, 1, g_closeHovered);

    Theme::DrawTransition(g_renderer, w, h);
    Theme::DrawHudFrame(g_renderer, w, h, g_fullscreen);

    // обучение с Капри: поверх всего, с «прожектором» на нужном элементе
    {
        Tutorial::Targets tg;
        tg.hasSign = (g_screen == Screen::Login) && !g_menu.open;
        if (tg.hasSign) {
            const SDL_Rect a = TextButton_Rect(g_signIn), b = TextButton_Rect(g_signUp);
            const int x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y);
            tg.sign = { x0, y0, std::max(a.x + a.w, b.x + b.w) - x0, std::max(a.y + a.h, b.y + b.h) - y0 };
        }
        tg.controls = { g_minimizeRect.x, g_minimizeRect.y, g_closeRect.x + g_closeRect.w - g_minimizeRect.x, g_closeRect.h };
        tg.gear = { int(g_menu.x), int(g_menu.y), int(g_menu.size), int(MenuButtonHeight(g_menu.size)) };
        Tutorial::SetTargets(tg);
        Tutorial::Draw(g_renderer, w, h);

        // раз в 30 минут Капри заглядывает с фразой ("как дела?", "сходи разомнись"...)
        CapriVisit::Draw(g_renderer, w, h, Tutorial::Active() || g_menu.open || g_repoSaving || ModalOpen());
    }
}

static void Draw() {
    // при запуске: иконка из темноты -> "by Aqua femboy~" -> глитч Blackwall -> главное окно
    if (Splash::Render(g_renderer, DrawScene)) {
        SDL_RenderPresent(g_renderer);
        return;
    }
    DrawScene();
    SDL_RenderPresent(g_renderer);
}

static bool SDLCALL LiveResizeWatch(void* userdata, SDL_Event* e) {
    SDL_Window* window = static_cast<SDL_Window*>(userdata);

    if (e->type == SDL_EVENT_WINDOW_EXPOSED ||
        e->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
        e->type == SDL_EVENT_WINDOW_RESIZED) {

        if (SDL_GetWindowFromID(e->window.windowID) != window) return true;

        ApplyRoundRegion(window, CurRadius());
        Draw();
    }
    return true;
}

static void ToggleFullscreen(SDL_Window* window) {
    g_fullscreen = !g_fullscreen;
    SDL_SetWindowFullscreen(window, g_fullscreen);
    EnableWindowFrame(window, 0, CurResizeBorder());
    ApplyRoundRegion(window, CurRadius());
}

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init error: " << SDL_GetError() << '\n';
        return 1;
    }

    // Применяем сохранённый язык до первого кадра и открытия любого экрана.
    LoadLanguagePreference();
    LoadCapriPreference();
    LoadStylePreference();   // палитра и спрайты проводника до первого кадра

    SDL_Window* window = SDL_CreateWindow("aquadeck", 800, 600,
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

    EnableWindowFrame(window, 0, CurResizeBorder());        // зоны кнопок выставляет Draw()
    SDL_SetWindowMinimumSize(window, 200, 150);
    ApplyRoundRegion(window, CurRadius());
    // «загрузку» при запуске теперь показывает заставка Splash (глитч Blackwall)

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

    Tutorial::g_onFinish = SaveTutorialDone;
    bool tutorialChecked = false;

    bool running = true;
    while (running) {
        // первый запуск: как только заставка закончилась, Капри начинает обучение
        if (!tutorialChecked && !Splash::Active()) {
            tutorialChecked = true;
            if (!TutorialAlreadyDone()) Tutorial::Start();
        }
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;

            // F11: переключение полноэкранного режима
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F11 && !e.key.repeat) {
                ToggleFullscreen(window);
                continue;
            }

            // пока играет заставка, ввод не доходит до приложения; клик или клавиша ускоряют её (сразу глитч)
            if (Splash::Active()) {
                if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) Splash::Skip();
                continue;
            }

            if (e.type == SDL_EVENT_MOUSE_MOTION) {
                const SDL_Point mousePoint{ static_cast<int>(e.motion.x), static_cast<int>(e.motion.y) };
                g_closeHovered = SDL_PointInRect(&mousePoint, &g_closeRect);
                g_minimizeHovered = SDL_PointInRect(&mousePoint, &g_minimizeRect);
                g_fullscreenHovered = SDL_PointInRect(&mousePoint, &g_fullscreenRect);
            } else if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
                g_closeHovered = false;
                g_minimizeHovered = false;
                g_fullscreenHovered = false;
            }

            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                const int mx = static_cast<int>(e.button.x);
                const int my = static_cast<int>(e.button.y);
                const SDL_Point mousePoint{ mx, my };
                if (SDL_PointInRect(&mousePoint, &g_closeRect)) {
                    running = false;
                    continue;
                }
                if (SDL_PointInRect(&mousePoint, &g_fullscreenRect)) {
                    ToggleFullscreen(window);
                    continue;
                }
                if (SDL_PointInRect(&mousePoint, &g_minimizeRect)) {
                    SDL_MinimizeWindow(window);
                    continue;
                }
            }

            // пока идёт обучение, все события получает только оно (кнопки окна выше всё равно работают)
            if (Tutorial::Active()) { Tutorial::Handle(e); continue; }
            if (CapriVisit::Handle(e)) continue;   // клик по Капри закрывает её фразу

            if (g_repoSaving) continue;   // пока идёт сохранение, ввод игнорируется

            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
                if (g_menu.open && g_lngDropdownOpen) g_lngDropdownOpen = false;
                else if (g_menu.open && g_menuInterfacePage) g_menuInterfacePage = false;
                else if (g_menu.open) g_menu.open = false;
                else if (g_screen == Screen::Login) running = false;
                // на экранах входа/регистрации Esc обрабатывает сам экран (назад)
            }

            if (ModalOpen()) g_menu.hovered = false;
            if (!ModalOpen() && MenuButton_Handle(g_menu, e)) {
                g_menu.open = !g_menu.open;
                g_menuInterfacePage = false;
                g_lngDropdownOpen = false;
            }

            // События меню настроек обрабатываются отдельно от экранов приложения.
            if (g_menu.open) {
                if (!g_menuInterfacePage) {
                    if (TextButton_Handle(g_interfaceButton, e))
                        g_menuInterfacePage = true;
                    if (TextButton_Handle(g_tutorialButton, e)) {
                        g_menu.open = false;            // закрыть меню и запустить обучение заново
                        auto tg = Tutorial::g_targets;  // кнопки входа видны, если сейчас экран входа (меню уже закрыто)
                        tg.hasSign = (g_screen == Screen::Login);
                        Tutorial::SetTargets(tg);
                        Tutorial::Start();
                    }
                }
                else {
                    if (TextButton_Handle(g_interfaceBackButton, e)) {
                        g_menuInterfacePage = false;
                        g_lngDropdownOpen = false;
                    }
                    else if (!g_lngDropdownOpen && TextButton_Handle(g_capriToggle, e)) {
                        SetCapriVisits(!CapriVisit::g_enabled);
                    }
                    else if (!g_lngDropdownOpen && TextButton_Handle(g_styleButton, e)) {
                        CycleStyle();
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
                        Opened_Open(window, Main_RepoDir(), g_user);
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
                    const ProfileResult pr = Profile_Handle(e);
                    if (pr == ProfileResult::Back) {
                        Profile_Close();
                        g_screen = Screen::Main;
                    }
                    else if (pr == ProfileResult::Ranks) {
                        Ranks_Open(window, g_user);
                        g_screen = Screen::Ranks;
                    }
                }
                else if (g_screen == Screen::Ranks) {
                    if (Ranks_Handle(e) == RanksResult::Back) {
                        Profile_Open(window, g_user);      // вернулись в профиль: перечитать опыт и достижения
                        g_screen = Screen::Profile;
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
        if (g_screen == Screen::Ranks) Ranks_Poll();       // вложения из окна теста
        if (g_screen == Screen::Profile) Profile_Poll();   // файл, выбранный в диалоге аватара
        if (g_screen == Screen::Create) Create_Poll();     // файл, выбранный в диалоге картинки
        Draw();
    }

    SDL_RemoveEventWatch(LiveResizeWatch, window);
    SaveOverlay_Destroy();
    Recover_Destroy();
    Profile_Destroy();
    Ranks_Destroy();
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
    TextButton_Destroy(g_tutorialButton);
    TextButton_Destroy(g_interfaceBackButton);
    TextButton_Destroy(g_lngButton);
    TextButton_Destroy(g_capriToggle);
    TextButton_Destroy(g_styleButton);
    TextButton_Destroy(g_lngEngOption);
    TextButton_Destroy(g_lngRusOption);
    MenuButton_Destroy(g_menu);
    DestroyControlIcon(g_closeTextures);
    DestroyControlIcon(g_minimizeTextures);
    DestroyControlIcon(g_fullscreenTextures);
    Logo::Destroy();
    Splash::Destroy();
    Tutorial::Destroy();
    DestroyBorderCache();
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
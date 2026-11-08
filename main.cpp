#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <algorithm>
#include "TextField.h"
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
#include "Achievements.h"
#include "Styles.h"
#include <algorithm>
#include <string>
#include <vector>
#include <cmath>
#include <mutex>
#include <atomic>
#include <functional>

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
// Редактор своего стиля (меню -> ИНТЕРФЕЙС -> СОЗДАТЬ СТИЛЬ).
static bool g_menuCustomPage = false;
static bool g_customTextWasActive = false;   // был ли включён ввод текста до открытия редактора
static int g_customFocus = -1;               // какое из 4 полей цвета в фокусе (-1 = никакое)
static TextButton g_customButton{ 0, 0, 280, 42, "CREATE STYLE" };
static TextButton g_customBackButton{ 0, 0, 110, 38, "BACK" };
static TextButton g_customResetButton{ 0, 0, 174, 38, "RESET" };
static TextButton g_customFontButton{ 0, 0, 194, 40, "HELVETICA" };
static TextButton g_customLookButton{ 0, 0, 194, 40, "FLAT" };
static TextButton g_customBgButton{ 0, 0, 150, 40, "CHOOSE" };       // выбрать фоновую картинку
static TextButton g_customBgClearButton{ 0, 0, 36, 40, "X" };          // убрать картинку
static SDL_FRect  g_bgSlider{ 0, 0, 0, 0 };                            // ползунок непрозрачности (для мыши)
static bool       g_bgDrag = false;                                    // ползунок тянется мышью
static TextField g_customField[4];
static TextField g_customName;                // поле названия стиля (индекс фокуса 4)
static TextButton g_customSaveButton{ 0, 0, 140, 38, "SAVE" };
static TextButton g_customDeleteButton{ 0, 0, 140, 38, "DELETE" };
static Uint64 g_saveFlashTick = 0;            // момент нажатия SAVE (кнопка на секунду показывает «готово»)
static Uint64 g_deleteArmTick = 0;            // первое нажатие DELETE: ждём подтверждения вторым нажатием
// Цветовой круг (HSV): открывается кликом по образцу цвета.
static int g_pickerIdx = -1;                 // какой цвет правим (0..3), -1 = круг закрыт
static float g_ph = 0.f, g_ps = 0.f, g_pv = 1.f;   // оттенок 0..360, насыщенность и яркость 0..1
static int g_pickDrag = 0;                   // 0 = нет, 1 = тащим по кругу, 2 = тащим ползунок яркости
static SDL_Texture* g_wheelTex = nullptr;    // круг оттенков при яркости 1 (яркость делается ColorMod)
static SDL_Renderer* g_wheelOwner = nullptr;
static SDL_FRect g_swatchRect[4];            // образцы цвета (клик открывает круг)
static float g_wheelCx = 0.f, g_wheelCy = 0.f;
static SDL_FRect g_vSlider{ 0, 0, 0, 0 };
static TextButton g_pickerDoneButton{ 0, 0, 294, 38, "OK" };
constexpr float kWheelR = 80.f;
static TextButton g_lngEngOption{ 0, 0, 150, 38, "ENG" };
static TextButton g_lngRusOption{ 0, 0, 150, 38, "RUS" };
static SDL_Renderer* g_renderer = nullptr;
static SDL_Rect g_minimizeRect{ 0, 0, 28, 28 };
static SDL_Rect g_closeRect{ 0, 0, 28, 28 };
static bool g_minimizeHovered = false;
static bool g_closeHovered = false;
static const int kRadius = 0;   // угловатое окно в стиле кибердека
static const int kCuteRadius = 26;   // у стиля PRO CUTE углы окна круглые
static SDL_Rect g_fullscreenRect{ 0, 0, 28, 28 };
static bool g_fullscreenHovered = false;
// Полноэкранный режим — дополнительная функция: приложение стартует в обычном окне,
// переключение кнопкой в правом верхнем углу или клавишей F11.
static bool g_fullscreen = false;
static int CurRadius() { return g_fullscreen ? 0 : (Theme::Cute() ? kCuteRadius : kRadius); }
static SDL_Window* g_window = nullptr;
// Скругление самого окна зависит от стиля: следим за ним каждый кадр (смена стиля, редактор стилей).
static void SyncWindowShape() {
    static int s_radius = -1;
    const int rad = CurRadius();
    if (g_window && rad != s_radius) { s_radius = rad; ApplyRoundRegion(g_window, rad); }
}
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

// Стиль интерфейса (CAPRI / ASTOLFO / MINIMAL) хранится рядом с языком.
static std::string StylePreferencePath() {
    char* prefPath = SDL_GetPrefPath("myproject", "settings");
    if (!prefPath) return {};
    std::string path(prefPath);
    SDL_free(prefPath);
    if (path.empty()) return {};
    return path + "style.cfg";
}

static void LoadCustomStyle();   // определены ниже (после сохранения стиля)
static void LoadSavedStyles();
static void SeedBuiltinPresets();
static void RefreshStyleAchStats();

static void LoadStylePreference() {
    int index = 0;
    LoadSavedStyles();
    SeedBuiltinPresets();   // один раз добавляет готовые HUD-пресеты
    RefreshStyleAchStats();
    LoadCustomStyle();   // слот CUSTOM должен быть готов до выбора стиля
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

static void SaveStylePreference() {
    const std::string path = StylePreferencePath();
    if (path.empty()) return;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (output) output << Styles::Current().id;
}

// Кнопка STYLE листает по кругу: встроенные стили, затем сохранённые игроком.
static void CycleStyle();

// Свой стиль: custom.cfg хранит стиль, который сейчас редактируется / применён,
// styles.cfg - библиотеку сохранённых игроком стилей. Строки вида  bg=#202020
static std::string PrefFilePath(const char* file) {
    char* prefPath = SDL_GetPrefPath("myproject", "settings");
    if (!prefPath) return {};
    std::string path(prefPath);
    SDL_free(prefPath);
    if (path.empty()) return {};
    return path + file;
}

static void WriteSpec(std::ostream& o, const Styles::CustomSpec& c) {
    o << "name=" << c.name << "\n"
      << "bg=" << Styles::ToHex(c.bg) << "\n"
      << "text=" << Styles::ToHex(c.text) << "\n"
      << "accent=" << Styles::ToHex(c.accent) << "\n"
      << "panel=" << Styles::ToHex(c.panel) << "\n"
      << "font=" << c.font << "\n"
      << "look=" << Styles::kLookIds[std::clamp(c.look, 0, Theme::LookCount - 1)] << "\n"
      << "bgalpha=" << c.bgAlpha << "\n";
    if (!c.bgImage.empty()) o << "bgimage=" << c.bgImage << "\n";
}

static void ApplySpecLine(Styles::CustomSpec& c, const std::string& key, const std::string& val) {
    Theme::Rgb col;
    if (key == "name") c.name = val;
    else if (key == "bg" && Styles::ParseHex(val, col)) c.bg = col;
    else if (key == "text" && Styles::ParseHex(val, col)) c.text = col;
    else if (key == "accent" && Styles::ParseHex(val, col)) c.accent = col;
    else if (key == "panel" && Styles::ParseHex(val, col)) c.panel = col;
    else if (key == "font") {
        if (val == "pixel") c.font = 0;                 // старый формат файла
        else if (val == "helvetica") c.font = 1;
        else if (!val.empty() && std::isdigit(static_cast<unsigned char>(val[0])))
            c.font = std::clamp(std::atoi(val.c_str()), 0, TextDefaults::kFontCount - 1);
    }
    else if (key == "look") c.look = Styles::LookFromId(val);
    else if (key == "bgimage") c.bgImage = val;
    else if (key == "bgalpha") c.bgAlpha = std::clamp(std::atoi(val.c_str()), 0, 100);
}

static bool SplitLine(std::string line, std::string& key, std::string& val) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    const size_t eq = line.find('=');
    if (eq == std::string::npos) return false;
    key = line.substr(0, eq); val = line.substr(eq + 1);
    return true;
}

// ---------------------------------------------------------------------------
// Фоновая картинка своего стиля (меню -> ИНТЕРФЕЙС -> СОЗДАТЬ СТИЛЬ -> ФОТО / ПРОЗРАЧНОСТЬ).
// Выбранный PNG / BMP не ссылается на исходный файл: он уменьшается (до 1920 px по большей стороне)
// и сохраняется копией bg_XXXXXXXX.bmp в папке настроек, так что стиль не ломается, если оригинал
// перенесут или удалят. Копия, на которую больше не ссылается ни один стиль, удаляется.
// ---------------------------------------------------------------------------
static SDL_Texture* g_bgTex = nullptr;
static std::string  g_bgTexKey;           // имя файла, для которого сейчас создана текстура
static int          g_bgTexW = 0, g_bgTexH = 0;
static std::mutex   g_bgMutex;            // путь приходит из потока системного диалога
static std::string  g_bgPending;
static std::atomic<bool> g_bgHasPending{ false };
static std::atomic<bool> g_bgDialogOpen{ false };
static Uint64       g_bgErrTick = 0;      // когда не удалось открыть картинку (кнопка секунду показывает ошибку)

static void SDLCALL OnBgChosen(void*, const char* const* list, int) {
    if (list && list[0]) {   // NULL = ошибка, пустой список = отмена
        std::lock_guard<std::mutex> lock(g_bgMutex);
        g_bgPending = list[0];
        g_bgHasPending = true;
    }
    g_bgDialogOpen = false;
}

static void ChooseBackgroundFile(SDL_Window* window) {
    if (g_bgDialogOpen.exchange(true)) return;   // диалог уже открыт
    static const SDL_DialogFileFilter filtersEn[] = { { "Images (PNG, BMP)", "png;bmp" } };
    static const SDL_DialogFileFilter filtersRu[] = { { "Изображения (PNG, BMP)", "png;bmp" } };
    SDL_ShowOpenFileDialog(OnBgChosen, nullptr, window, UiRussianFlag() ? filtersRu : filtersEn, 1, nullptr, false);
}

// Файл картинки ещё нужен? (на него ссылается рабочий или любой сохранённый стиль)
static bool BgFileInUse(const std::string& name) {
    if (name.empty()) return false;
    if (Styles::g_custom.bgImage == name) return true;
    for (const auto& c : Styles::g_saved) if (c.bgImage == name) return true;
    return false;
}

// Вызывать после того, как стиль перестал ссылаться на name: удаляет файл, если он больше никому не нужен.
static void ReleaseBgFile(const std::string& name) {
    if (name.empty() || BgFileInUse(name)) return;
    const std::string path = PrefFilePath(name.c_str());
    if (!path.empty()) SDL_RemovePath(path.c_str());
}

// Читает PNG / BMP, уменьшает и сохраняет копию. Возвращает имя файла копии ("" = не вышло).
static std::string ImportBackground(const std::string& srcPath) {
    SDL_Surface* src = SDL_LoadSurface(srcPath.c_str());
    if (!src) { SDL_Log("background: cannot load '%s': %s", srcPath.c_str(), SDL_GetError()); return {}; }
    SDL_Surface* img = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(src);
    if (!img) return {};
    const int maxDim = 1920;
    if (img->w > maxDim || img->h > maxDim) {
        const float k = float(maxDim) / float(std::max(img->w, img->h));
        SDL_Surface* scaled = SDL_ScaleSurface(img, std::max(1, int(float(img->w) * k)), std::max(1, int(float(img->h) * k)),
                                               SDL_SCALEMODE_LINEAR);
        SDL_DestroySurface(img);
        img = scaled;
        if (!img) return {};
    }
    // имя уникально для каждого выбора, чтобы разные стили не делили один файл по ошибке
    const unsigned long long h = (unsigned long long)std::hash<std::string>{}(srcPath) ^ (SDL_GetTicks() * 2654435761ULL);
    char name[40];
    std::snprintf(name, sizeof name, "bg_%08x.bmp", unsigned(h & 0xFFFFFFFFu));
    const std::string path = PrefFilePath(name);
    const bool ok = !path.empty() && SDL_SaveBMP(img, path.c_str());
    SDL_DestroySurface(img);
    if (!ok) SDL_Log("background: cannot save '%s': %s", path.c_str(), SDL_GetError());
    return ok ? std::string(name) : std::string();
}

static void SaveCustomStyle();

// Каждый кадр (из основного потока): подхватывает картинку, выбранную в диалоге.
static void PollBackgroundPick() {
    if (!g_bgHasPending.exchange(false)) return;
    std::string path;
    { std::lock_guard<std::mutex> lock(g_bgMutex); path = g_bgPending; }
    const std::string name = ImportBackground(path);
    if (name.empty()) { g_bgErrTick = SDL_GetTicks(); return; }
    const std::string old = Styles::g_custom.bgImage;
    Styles::g_custom.bgImage = name;
    if (Styles::g_custom.bgAlpha <= 0) Styles::g_custom.bgAlpha = 40;   // иначе картинку не видно
    ReleaseBgFile(old);
    SaveCustomStyle();
}

static void ClearBackground() {
    const std::string old = Styles::g_custom.bgImage;
    Styles::g_custom.bgImage.clear();
    ReleaseBgFile(old);
    SaveCustomStyle();
}

// Рисует картинку поверх фона окна (режим «заполнить»: масштаб до покрытия окна, лишнее обрезается по центру).
// Только для слота CUSTOM; встроенные стили картинок не имеют.
static void DrawCustomBackground(int w, int h) {
    if (Styles::g_current != Styles::kCustomIndex) return;
    const Styles::CustomSpec& c = Styles::g_custom;
    if (c.bgImage.empty() || c.bgAlpha <= 0) return;
    if (g_bgTexKey != c.bgImage) {
        if (g_bgTex) { SDL_DestroyTexture(g_bgTex); g_bgTex = nullptr; }
        g_bgTexKey = c.bgImage;   // ключ ставится и при неудаче, чтобы не читать файл каждый кадр
        const std::string path = PrefFilePath(c.bgImage.c_str());
        if (!path.empty()) {
            if (SDL_Surface* s = SDL_LoadBMP(path.c_str())) {
                g_bgTex = SDL_CreateTextureFromSurface(g_renderer, s);
                g_bgTexW = s->w; g_bgTexH = s->h;
                SDL_DestroySurface(s);
            }
        }
    }
    if (!g_bgTex || g_bgTexW <= 0 || g_bgTexH <= 0) return;
    const float scale = std::max(float(w) / float(g_bgTexW), float(h) / float(g_bgTexH));
    const float dw = float(g_bgTexW) * scale, dh = float(g_bgTexH) * scale;
    const SDL_FRect dst{ (float(w) - dw) * 0.5f, (float(h) - dh) * 0.5f, dw, dh };
    SDL_SetTextureBlendMode(g_bgTex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(g_bgTex, Uint8(c.bgAlpha * 255 / 100));
    SDL_RenderTexture(g_renderer, g_bgTex, nullptr, &dst);
}

static void SaveCustomStyle() {
    const std::string path = PrefFilePath("custom.cfg");
    if (path.empty()) return;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (output) WriteSpec(output, Styles::g_custom);
}

static void LoadCustomStyle() {
    const std::string path = PrefFilePath("custom.cfg");
    if (!path.empty()) {
        std::ifstream input(path, std::ios::binary);
        std::string line, key, val;
        while (input && std::getline(input, line))
            if (SplitLine(line, key, val)) ApplySpecLine(Styles::g_custom, key, val);
    }
    Styles::RebuildCustom();
}

static void SaveSavedStyles() {
    const std::string path = PrefFilePath("styles.cfg");
    if (path.empty()) return;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return;
    for (const auto& c : Styles::g_saved) { output << "[style]\n"; WriteSpec(output, c); }
}

static void LoadSavedStyles() {
    Styles::g_saved.clear();
    const std::string path = PrefFilePath("styles.cfg");
    if (path.empty()) return;
    std::ifstream input(path, std::ios::binary);
    std::string line, key, val;
    while (input && std::getline(input, line)) {
        if (line.rfind("[style]", 0) == 0) {
            Styles::CustomSpec c; c.name.clear();
            Styles::g_saved.push_back(c);
        }
        else if (!Styles::g_saved.empty() && SplitLine(line, key, val))
            ApplySpecLine(Styles::g_saved.back(), key, val);
    }
    for (auto& c : Styles::g_saved) if (c.name.empty()) c.name = "CUSTOM";
}

// Один раз добавляет встроенные пресеты (палитра + вид HUD) в библиотеку сохранённых стилей.
// Маркер presets.v2 нужен, чтобы удалённый игроком пресет не возвращался при каждом запуске.
// Если пресет с таким именем уже есть (его добавила прошлая версия), ему только ставится новый вид,
// цвета и шрифт игрока не трогаются.
static int SavedIndexByName(const std::string& name);
static void SeedBuiltinPresets() {
    const std::string marker = PrefFilePath("presets.v2");
    if (marker.empty()) return;
    { std::ifstream probe(marker, std::ios::binary); if (probe) return; }
    for (const auto& preset : Styles::BuiltinPresets()) {
        const int idx = SavedIndexByName(preset.name);
        if (idx >= 0) Styles::g_saved[size_t(idx)].look = preset.look;
        else if (Styles::g_saved.size() < 24) Styles::g_saved.push_back(preset);
    }
    SaveSavedStyles();
    std::ofstream out(marker, std::ios::binary | std::ios::trunc);
    if (out) out << "2";
}

static int SavedIndexByName(const std::string& name) {
    for (size_t i = 0; i < Styles::g_saved.size(); ++i)
        if (Styles::g_saved[i].name == name) return int(i);
    return -1;
}

static void CycleStyle() {
    const int total = Styles::kCustomIndex + int(Styles::g_saved.size());
    int pos;
    if (Styles::g_current < Styles::kCustomIndex) pos = Styles::g_current;
    else {
        const int si = SavedIndexByName(Styles::g_custom.name);
        pos = si >= 0 ? Styles::kCustomIndex + si : total - 1;   // несохранённый черновик: дальше идём на первый стиль
    }
    const int next = (pos + 1) % total;
    if (next < Styles::kCustomIndex) {
        Styles::Apply(next);
        SaveStylePreference();
    }
    else {
        const std::string oldBg = Styles::g_custom.bgImage;
        Styles::g_custom = Styles::g_saved[size_t(next - Styles::kCustomIndex)];   // сохранённый стиль становится рабочим
        ReleaseBgFile(oldBg);   // картинка несохранённого черновика больше не нужна
        Styles::RebuildCustom();
        Styles::Apply(Styles::kCustomIndex);
        SaveStylePreference();
        SaveCustomStyle();
    }
}

static std::string TrimName(std::string n) {
    while (!n.empty() && n.back() == ' ') n.pop_back();
    size_t a = 0; while (a < n.size() && n[a] == ' ') ++a;
    n.erase(0, a);
    return n.empty() ? std::string("CUSTOM") : n;
}

// Для достижений: сколько собственных стилей (без встроенных пресетов) и сколько разных видов среди них.
static void RefreshStyleAchStats() {
    std::vector<std::string> builtin;
    for (const auto& b : Styles::BuiltinPresets()) builtin.push_back(b.name);
    long long count = 0;
    bool seen[Theme::LookCount] = {};
    for (const auto& c : Styles::g_saved) {
        if (std::find(builtin.begin(), builtin.end(), c.name) != builtin.end()) continue;
        ++count;
        seen[std::clamp(c.look, 0, Theme::LookCount - 1)] = true;
    }
    long long looks = 0;
    for (bool b : seen) if (b) ++looks;
    g_achUserStyles = count;
    g_achUserLooks = looks;
}

// Сохранить текущий стиль в библиотеку под его названием (стиль с таким же названием перезаписывается).
static void SaveCurrentStyle() {
    Styles::g_custom.name = TrimName(Styles::g_custom.name);
    g_customName.text = Styles::g_custom.name;
    const int idx = SavedIndexByName(Styles::g_custom.name);
    if (idx >= 0) Styles::g_saved[idx] = Styles::g_custom;
    else if (Styles::g_saved.size() < 24) Styles::g_saved.push_back(Styles::g_custom);
    Styles::RebuildCustom();
    SaveSavedStyles();
    SaveCustomStyle();
    RefreshStyleAchStats();
}

static bool DeleteCurrentStyle() {
    const int idx = SavedIndexByName(TrimName(Styles::g_custom.name));
    if (idx < 0) return false;
    const std::string oldBg = Styles::g_saved[size_t(idx)].bgImage;
    Styles::g_saved.erase(Styles::g_saved.begin() + idx);
    ReleaseBgFile(oldBg);
    SaveSavedStyles();
    RefreshStyleAchStats();
    return true;
}

// Поля цветов берут текст из текущих значений стиля.
static void SyncCustomFields() {
    const Styles::CustomSpec& c = Styles::g_custom;
    const Theme::Rgb cols[4] = { c.bg, c.text, c.accent, c.panel };
    for (int i = 0; i < 4; ++i) {
        g_customField[i].text = Styles::ToHex(cols[i]);
        g_customField[i].maxLen = 7;
        g_customField[i].placeholder = "#RRGGBB";
        g_customField[i].focused = false;
        g_customField[i].undo = UndoHistory{};
    }
    g_customName.text = c.name;
    g_customName.maxLen = 16;
    g_customName.placeholder = "NAME";
    g_customName.focused = false;
    g_customName.undo = UndoHistory{};
    g_customFocus = -1;
}

static void OpenCustomPage(SDL_Window* window) {
    Styles::RebuildCustom();
    Styles::Apply(Styles::kCustomIndex);   // редактируем сразу на живом стиле
    SaveStylePreference();
    g_customTextWasActive = SDL_TextInputActive(window);
    SDL_StartTextInput(window);
    SyncCustomFields();
    g_menuCustomPage = true;
}

static void LeaveCustomPage(SDL_Window* window) {
    if (!g_menuCustomPage) return;
    g_menuCustomPage = false;
    g_pickerIdx = -1; g_pickDrag = 0;
    g_customFocus = -1;
    for (auto& f : g_customField) f.focused = false;
    g_customName.focused = false;
    g_deleteArmTick = 0;
    g_bgDrag = false;
    if (!g_customTextWasActive) SDL_StopTextInput(window);   // вернуть ввод текста как было
    SaveCustomStyle();
}

// Оставляет в поле только "#" и до 6 hex-цифр.
static void SanitizeHexField(TextField& f) {
    std::string d;
    for (char ch : f.text)
        if (std::isxdigit(static_cast<unsigned char>(ch))) d += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    if (d.size() > 6) d.resize(6);
    f.text = "#" + d;
}

static const char* kVersion = "v1.1.0";

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

static Theme::Rgb HsvToRgb(float h, float sat, float val) {
    h = std::fmod(h, 360.f); if (h < 0.f) h += 360.f;
    const float c = val * sat, hh = h / 60.f;
    const float x = c * (1.f - std::fabs(std::fmod(hh, 2.f) - 1.f)), m = val - c;
    float r = 0, g = 0, b = 0;
    if (hh < 1) { r = c; g = x; } else if (hh < 2) { r = x; g = c; } else if (hh < 3) { g = c; b = x; }
    else if (hh < 4) { g = x; b = c; } else if (hh < 5) { r = x; b = c; } else { r = c; b = x; }
    auto to8 = [](float v) { return Uint8(std::lround(std::clamp(v, 0.f, 1.f) * 255.f)); };
    return Theme::Rgb{ to8(r + m), to8(g + m), to8(b + m) };
}

static void RgbToHsv(Theme::Rgb c, float& h, float& sat, float& val) {
    const float r = c.r / 255.f, g = c.g / 255.f, b = c.b / 255.f;
    const float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b)), d = mx - mn;
    val = mx; sat = mx > 0.f ? d / mx : 0.f;
    if (d <= 0.f) return;   // серый: оттенок оставляем прежним
    if (mx == r) h = 60.f * std::fmod((g - b) / d, 6.f);
    else if (mx == g) h = 60.f * ((b - r) / d + 2.f);
    else h = 60.f * ((r - g) / d + 4.f);
    if (h < 0.f) h += 360.f;
}

// Круг оттенков (угол = оттенок, радиус = насыщенность), создаётся один раз.
static SDL_Texture* EnsureWheelTexture() {
    if (g_wheelTex && g_wheelOwner == g_renderer) return g_wheelTex;
    if (g_wheelTex) SDL_DestroyTexture(g_wheelTex);
    g_wheelTex = nullptr; g_wheelOwner = g_renderer;
    const int n = int(kWheelR * 2.f);
    std::vector<Uint32> px(size_t(n) * n, 0);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            const float dx = float(x) + 0.5f - kWheelR, dy = float(y) + 0.5f - kWheelR;
            const float rad = std::sqrt(dx * dx + dy * dy);
            const float cover = std::clamp(kWheelR - rad + 0.5f, 0.f, 1.f);   // сглаженный край
            if (cover <= 0.f) continue;
            float hue = std::atan2(dy, dx) * 57.29578f; if (hue < 0.f) hue += 360.f;
            const Theme::Rgb c = HsvToRgb(hue, std::min(1.f, rad / kWheelR), 1.f);
            const Uint8 a = Uint8(std::lround(cover * 255.f));
            px[size_t(y) * n + x] = Uint32(c.r) | (Uint32(c.g) << 8) | (Uint32(c.b) << 16) | (Uint32(a) << 24);   // RGBA32 в памяти: R,G,B,A
        }
    g_wheelTex = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, n, n);
    if (g_wheelTex) {
        SDL_UpdateTexture(g_wheelTex, nullptr, px.data(), n * 4);
        SDL_SetTextureBlendMode(g_wheelTex, SDL_BLENDMODE_BLEND);
    }
    return g_wheelTex;
}

static void PickerApply() {
    if (g_pickerIdx < 0) return;
    const Theme::Rgb c = HsvToRgb(g_ph, g_ps, g_pv);
    Theme::Rgb* targets[4] = { &Styles::g_custom.bg, &Styles::g_custom.text, &Styles::g_custom.accent, &Styles::g_custom.panel };
    *targets[g_pickerIdx] = c;
    g_customField[g_pickerIdx].text = Styles::ToHex(c);   // поле hex остаётся в синхроне
    Styles::RebuildCustom();
}

static void OpenPicker(int idx) {
    const Theme::Rgb cols[4] = { Styles::g_custom.bg, Styles::g_custom.text, Styles::g_custom.accent, Styles::g_custom.panel };
    RgbToHsv(cols[idx], g_ph, g_ps, g_pv);
    g_pickerIdx = idx; g_pickDrag = 0; g_customFocus = -1;
    for (auto& f : g_customField) f.focused = false;
}

static void ClosePicker() {
    if (g_pickerIdx < 0) return;
    g_pickerIdx = -1; g_pickDrag = 0;
    SaveCustomStyle();
}

static void DrawColorPicker(float px, float py, SDL_Rect* nd, int& ndCount) {
    const char* labels[4] = { UiText("BG", "ФОН"), UiText("TEXT", "ТЕКСТ"), UiText("ACCENT", "АКЦЕНТ"), UiText("PANEL", "ПАНЕЛЬ") };
    DrawUtf8Text(g_renderer, labels[g_pickerIdx], px + 30.f, py + 52.f, 16, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 255);

    g_wheelCx = px + 30.f + kWheelR; g_wheelCy = py + 120.f + kWheelR;
    if (SDL_Texture* tex = EnsureWheelTexture()) {
        const Uint8 vb = Uint8(std::lround(g_pv * 255.f));
        SDL_SetTextureColorMod(tex, vb, vb, vb);   // яркость: затемняем круг
        const SDL_FRect dst{ g_wheelCx - kWheelR, g_wheelCy - kWheelR, kWheelR * 2.f, kWheelR * 2.f };
        SDL_RenderTexture(g_renderer, tex, nullptr, &dst);
    }
    // маркер на круге
    const float ang = g_ph * 0.01745329f;
    const float mx = g_wheelCx + std::cos(ang) * g_ps * kWheelR, my = g_wheelCy + std::sin(ang) * g_ps * kWheelR;
    const SDL_FRect outer{ mx - 6.f, my - 6.f, 12.f, 12.f }, inner{ mx - 5.f, my - 5.f, 10.f, 10.f };
    Theme::SetColor(g_renderer, Theme::Rgb{ 0, 0, 0 }, 255); SDL_RenderRect(g_renderer, &outer);
    Theme::SetColor(g_renderer, Theme::Rgb{ 255, 255, 255 }, 255); SDL_RenderRect(g_renderer, &inner);

    // ползунок яркости: от чистого цвета (сверху) до чёрного (снизу)
    g_vSlider = { px + 214.f, py + 120.f, 28.f, kWheelR * 2.f };
    {
        const Theme::Rgb top = HsvToRgb(g_ph, g_ps, 1.f);
        const SDL_FColor ct{ top.r / 255.f, top.g / 255.f, top.b / 255.f, 1.f }, cb{ 0.f, 0.f, 0.f, 1.f };
        const SDL_Vertex v[4] = {
            { { g_vSlider.x, g_vSlider.y }, ct, { 0.f, 0.f } }, { { g_vSlider.x + g_vSlider.w, g_vSlider.y }, ct, { 0.f, 0.f } },
            { { g_vSlider.x + g_vSlider.w, g_vSlider.y + g_vSlider.h }, cb, { 0.f, 0.f } },
            { { g_vSlider.x, g_vSlider.y + g_vSlider.h }, cb, { 0.f, 0.f } } };
        const int idx[6] = { 0, 1, 2, 0, 2, 3 };
        SDL_RenderGeometry(g_renderer, nullptr, v, 4, idx, 6);
        Theme::SetColor(g_renderer, Theme::Cyan, 200); SDL_RenderRect(g_renderer, &g_vSlider);
        const float vy = g_vSlider.y + (1.f - g_pv) * g_vSlider.h;
        const SDL_FRect knob{ g_vSlider.x - 3.f, vy - 2.f, g_vSlider.w + 6.f, 4.f };
        Theme::SetColor(g_renderer, Theme::Rgb{ 0, 0, 0 }, 255); SDL_RenderRect(g_renderer, &knob);
        const SDL_FRect knobIn{ knob.x + 1.f, knob.y + 1.f, knob.w - 2.f, knob.h - 2.f };
        Theme::SetColor(g_renderer, Theme::Rgb{ 255, 255, 255 }, 255); SDL_RenderRect(g_renderer, &knobIn);
    }

    // предпросмотр и код цвета
    const Theme::Rgb cur = HsvToRgb(g_ph, g_ps, g_pv);
    Theme::SoftRect(g_renderer, px + 262.f, py + 120.f, 62.f, 62.f, cur, 255);
    Theme::SetColor(g_renderer, Theme::Cyan, 200);
    const SDL_FRect pf{ px + 262.5f, py + 120.5f, 61.f, 61.f }; Theme::StyledStroke(g_renderer, &pf);
    DrawUtf8Text(g_renderer, Styles::ToHex(cur), px + 30.f, py + 300.f, 16, Theme::Ink.r, Theme::Ink.g, Theme::Ink.b, 255);

    g_pickerDoneButton.label = "OK";
    g_pickerDoneButton.x = px + 30.f; g_pickerDoneButton.y = py + 350.f;
    TextButton_Draw(g_renderer, g_pickerDoneButton);
    nd[ndCount++] = TextButton_Rect(g_pickerDoneButton);
}

// События круга. Возвращает true, если событие обработано.
static bool PickerHandle(const SDL_Event& e) {
    if (TextButton_Handle(g_pickerDoneButton, e)) { ClosePicker(); return true; }
    auto fromWheel = [&](float mx, float my) {
        const float dx = mx - g_wheelCx, dy = my - g_wheelCy;
        const float rad = std::sqrt(dx * dx + dy * dy);
        g_ps = std::min(1.f, rad / kWheelR);
        if (rad > 1.f) { g_ph = std::atan2(dy, dx) * 57.29578f; if (g_ph < 0.f) g_ph += 360.f; }
        PickerApply();
    };
    auto fromSlider = [&](float my) {
        g_pv = 1.f - std::clamp((my - g_vSlider.y) / g_vSlider.h, 0.f, 1.f);
        PickerApply();
    };
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        const float mx = e.button.x, my = e.button.y;
        const float dx = mx - g_wheelCx, dy = my - g_wheelCy;
        if (dx * dx + dy * dy <= (kWheelR + 4.f) * (kWheelR + 4.f)) { g_pickDrag = 1; fromWheel(mx, my); return true; }
        if (mx >= g_vSlider.x - 4.f && mx <= g_vSlider.x + g_vSlider.w + 4.f &&
            my >= g_vSlider.y - 4.f && my <= g_vSlider.y + g_vSlider.h + 4.f) { g_pickDrag = 2; fromSlider(my); return true; }
    }
    else if (e.type == SDL_EVENT_MOUSE_MOTION && g_pickDrag) {
        if (g_pickDrag == 1) fromWheel(e.motion.x, e.motion.y); else fromSlider(e.motion.y);
        return true;
    }
    else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT && g_pickDrag) {
        g_pickDrag = 0;
        SaveCustomStyle();
        return true;
    }
    return false;
}

// Страница редактора своего стиля: название, 4 цвета (с образцами), шрифт, вид, сохранить / удалить / назад / сброс.
static TextField* CustomFieldPtr(int i) { return i == 4 ? &g_customName : &g_customField[i]; }

static void DrawCustomPage(float px, float py, SDL_Rect* nd, int& ndCount) {
    using Styles::g_custom;
    if (g_pickerIdx >= 0) { DrawColorPicker(px, py, nd, ndCount); return; }
    const Uint64 now = SDL_GetTicks();
    const Theme::Rgb cols[4] = { g_custom.bg, g_custom.text, g_custom.accent, g_custom.panel };
    const char* labels[6] = { UiText("NAME", "ИМЯ"), UiText("BG", "ФОН"), UiText("TEXT", "ТЕКСТ"),
                              UiText("ACCENT", "АКЦЕНТ"), UiText("PANEL", "ПАНЕЛЬ"), UiText("FONT", "ШРИФТ") };
    auto rowY = [&](int r) { return py + 78.f + 42.f * float(r); };
    auto label = [&](const char* t, float y) {
        DrawUtf8Text(g_renderer, t, px + 28.f, y + 12.f, 16, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 255);
    };

    // название
    label(labels[0], rowY(0));
    g_customName.x = px + 130.f; g_customName.y = rowY(0); g_customName.w = 194.f; g_customName.h = 40.f;
    g_customName.focused = (g_customFocus == 4);
    TextField_Draw(g_renderer, g_customName);
    nd[ndCount++] = TextField_Rect(g_customName);

    // 4 цвета: поле hex + образец (клик по образцу открывает круг)
    for (int i = 0; i < 4; ++i) {
        const float y = rowY(i + 1);
        label(labels[i + 1], y);
        TextField& f = g_customField[i];
        f.x = px + 130.f; f.y = y; f.w = 150.f; f.h = 40.f;
        f.focused = (g_customFocus == i);
        TextField_Draw(g_renderer, f);
        nd[ndCount++] = TextField_Rect(f);
        g_swatchRect[i] = { px + 288.f, y, 36.f, 40.f };
        Theme::SoftRect(g_renderer, px + 288.f, y, 36.f, 40.f, cols[i], 255);
        Theme::SetColor(g_renderer, Theme::Cyan, 200);
        const SDL_FRect frame{ px + 288.5f, y + 0.5f, 35.f, 39.f };
        Theme::StyledStroke(g_renderer, &frame);
    }

    // шрифт: кнопка листает список шрифтов
    label(labels[5], rowY(5));
    g_customFontButton.x = px + 130.f; g_customFontButton.y = rowY(5);
    g_customFontButton.label = TextDefaults::kFontNames[std::clamp(g_custom.font, 0, TextDefaults::kFontCount - 1)];
    TextButton_Draw(g_renderer, g_customFontButton);
    nd[ndCount++] = TextButton_Rect(g_customFontButton);

    // вид
    label(UiText("LOOK", "ВИД"), rowY(6));
    g_customLookButton.x = px + 130.f; g_customLookButton.y = rowY(6);
    g_customLookButton.label = UiText(Styles::kLookEn[std::clamp(g_custom.look, 0, Theme::LookCount - 1)],
                                      Styles::kLookRu[std::clamp(g_custom.look, 0, Theme::LookCount - 1)]);
    TextButton_Draw(g_renderer, g_customLookButton);
    nd[ndCount++] = TextButton_Rect(g_customLookButton);

    // фоновая картинка: выбрать / убрать
    const bool bgErr = (g_bgErrTick != 0 && now - g_bgErrTick < 1500);
    DrawUtf8TextFit(g_renderer, UiText("IMAGE", "ФОТО"), px + 28.f, rowY(7) + 20.f, 96.f, 16, false,
        Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 255);
    g_customBgButton.x = px + 130.f; g_customBgButton.y = rowY(7);
    g_customBgButton.label = bgErr ? UiText("ERROR", "ОШИБКА")
        : (g_custom.bgImage.empty() ? UiText("CHOOSE", "ВЫБРАТЬ") : UiText("CHANGE", "СМЕНИТЬ"));
    TextButton_Draw(g_renderer, g_customBgButton);
    nd[ndCount++] = TextButton_Rect(g_customBgButton);
    g_customBgClearButton.x = px + 288.f; g_customBgClearButton.y = rowY(7);
    TextButton_Draw(g_renderer, g_customBgClearButton);
    nd[ndCount++] = TextButton_Rect(g_customBgClearButton);

    // непрозрачность картинки: ползунок 0..100 %
    DrawUtf8TextFit(g_renderer, UiText("OPACITY", "ПРОЗРАЧНОСТЬ"), px + 28.f, rowY(8) + 20.f, 96.f, 16, false,
        Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 255);
    g_bgSlider = { px + 130.f, rowY(8), 194.f, 40.f };
    {
        const bool has = !g_custom.bgImage.empty();
        const Uint8 dim = has ? 255 : 110;   // без картинки ползунок тусклый, но двигать его можно
        Theme::CutPanel(g_renderer, g_bgSlider.x, g_bgSlider.y, g_bgSlider.w, g_bgSlider.h, 8.f,
            Theme::Panel, 205, Theme::Cyan, Uint8(dim * 0.7f), false);
        const float tx0 = g_bgSlider.x + 12.f, tw = g_bgSlider.w - 24.f, ty = g_bgSlider.y + 29.f;
        const float frac = float(std::clamp(g_custom.bgAlpha, 0, 100)) / 100.f;
        Theme::SoftRect(g_renderer, tx0, ty - 1.f, tw, 3.f, Theme::Cyan, Uint8(dim * 0.25f));
        Theme::SoftRect(g_renderer, tx0, ty - 1.f, tw * frac, 3.f, Theme::CyanHot, dim);
        Theme::SoftRect(g_renderer, tx0 + tw * frac - 3.f, ty - 7.f, 6.f, 14.f, g_bgDrag ? Theme::CyanHot : Theme::Cyan, dim);
        DrawUtf8Text(g_renderer, std::to_string(g_custom.bgAlpha) + "%", g_bgSlider.x + 12.f, g_bgSlider.y + 7.f, 8,
            Theme::Ink.r, Theme::Ink.g, Theme::Ink.b, dim);
        nd[ndCount++] = SDL_Rect{ int(g_bgSlider.x), int(g_bgSlider.y), int(g_bgSlider.w), int(g_bgSlider.h) };
    }

    // сохранить / удалить (удаление просит подтверждения вторым нажатием)
    const float y1 = py + 460.f, y2 = py + 504.f;
    const bool saved = (g_saveFlashTick != 0 && now - g_saveFlashTick < 1200);
    const bool armed = (g_deleteArmTick != 0 && now - g_deleteArmTick < 2500);
    g_customSaveButton.label = saved ? UiText("DONE", "ГОТОВО") : UiText("SAVE", "СОХРАНИТЬ");
    g_customSaveButton.x = px + 30.f; g_customSaveButton.y = y1;
    g_customDeleteButton.label = armed ? UiText("SURE?", "ТОЧНО?") : UiText("DELETE", "УДАЛИТЬ");
    g_customDeleteButton.x = px + 184.f; g_customDeleteButton.y = y1;
    g_customBackButton.label = UiText("BACK", "НАЗАД");
    g_customBackButton.x = px + 30.f; g_customBackButton.y = y2; g_customBackButton.w = 140.f;
    g_customResetButton.label = UiText("RESET", "СБРОС");
    g_customResetButton.x = px + 184.f; g_customResetButton.y = y2; g_customResetButton.w = 140.f;
    TextButton_Draw(g_renderer, g_customSaveButton);
    TextButton_Draw(g_renderer, g_customDeleteButton);
    TextButton_Draw(g_renderer, g_customBackButton);
    TextButton_Draw(g_renderer, g_customResetButton);
    nd[ndCount++] = TextButton_Rect(g_customSaveButton);
    nd[ndCount++] = TextButton_Rect(g_customDeleteButton);
    nd[ndCount++] = TextButton_Rect(g_customBackButton);
    nd[ndCount++] = TextButton_Rect(g_customResetButton);
}

// Рисует один кадр приложения в текущую цель рендера (без SDL_RenderPresent):
// заставка Splash рисует его в текстуру и склеивает с собой глитчем.
static void DrawScene() {
    SDL_Rect nd[kMaxNoDragRects];
    SyncWindowShape();

    // смена экрана запускает анимацию перехода (экран меняется сразу, поверх открываются полосы)
    static Screen s_prevScreen = g_screen;
    if (s_prevScreen != g_screen) { Theme::TransitionStart(); s_prevScreen = g_screen; }
    int w = 0, h = 0;
    SDL_GetCurrentRenderOutputSize(g_renderer, &w, &h);
    PollBackgroundPick();   // картинка, выбранная в диалоге редактора стиля
    Theme::DrawBackground(g_renderer, w, h);
    DrawCustomBackground(w, h);
    Theme::DrawBackgroundFx(g_renderer, w, h);   // PRO CUTE: лепестки сакуры, боке, звёздочки и сердечки под интерфейсом

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
        const float panelH = g_menuCustomPage ? 568.f : (g_menuInterfacePage ? 356.f : 240.f);
        const float px = (w - panelW) * 0.5f;
        const float py = (h - panelH) * 0.5f;
        g_menuPanel.x = px + 12.f;
        g_menuPanel.y = py + 34.f;
        g_menuPanel.w = panelW - 24.f;
        g_menuPanel.h = panelH - 46.f;
        TextButton_Draw(g_renderer, g_menuPanel);
        nd[ndCount++] = TextButton_Rect(g_menuPanel);
        DrawCenteredText(g_renderer, g_menuCustomPage ? UiText("CUSTOM STYLE", "СВОЙ СТИЛЬ") : (g_menuInterfacePage ? UiText("INTERFACE", "ИНТЕРФЕЙС") : UiText("MENU", "МЕНЮ")),
            w, h, 2.4f, py - h * 0.5f + 8.f);

        if (g_menuCustomPage) {
            DrawCustomPage(px, py, nd, ndCount);
        }
        else if (!g_menuInterfacePage) {
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
            g_interfaceBackButton.y = py + 296.f;
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
                DrawUtf8TextFit(g_renderer, UiText(Styles::GuideEn(Styles::Current()), Styles::GuideRu(Styles::Current())), px + 48.f, py + 143.f + 8.f, 106.f, 18, false, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 255);
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

                // Свой стиль: открывает редактор цветов, шрифта и вида
                g_customButton.label = UiText("CREATE STYLE", "СОЗДАТЬ СТИЛЬ");
                g_customButton.x = px + 40.f;
                g_customButton.y = py + 234.f;
                TextButton_Draw(g_renderer, g_customButton);
                nd[ndCount++] = TextButton_Rect(g_customButton);
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

    g_window = window;
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
                else if (g_menu.open && g_menuCustomPage && g_pickerIdx >= 0) ClosePicker();
                else if (g_menu.open && g_menuCustomPage) LeaveCustomPage(window);
                else if (g_menu.open && g_menuInterfacePage) g_menuInterfacePage = false;
                else if (g_menu.open) g_menu.open = false;
                else if (g_screen == Screen::Login) running = false;
                // на экранах входа/регистрации Esc обрабатывает сам экран (назад)
            }

            if (ModalOpen()) g_menu.hovered = false;
            if (!ModalOpen() && MenuButton_Handle(g_menu, e)) {
                LeaveCustomPage(window);
                g_menu.open = !g_menu.open;
                g_menuInterfacePage = false;
                g_lngDropdownOpen = false;
            }

            // События меню настроек обрабатываются отдельно от экранов приложения.
            if (g_menu.open) {
                if (g_menuCustomPage && g_pickerIdx >= 0) {
                    PickerHandle(e);
                }
                else if (g_menuCustomPage) {
                    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                        for (int i = 0; i < 4; ++i) {
                            const SDL_FRect& q = g_swatchRect[i];
                            if (e.button.x >= q.x && e.button.x <= q.x + q.w && e.button.y >= q.y && e.button.y <= q.y + q.h)
                                OpenPicker(i);
                        }
                    }
                    // ползунок непрозрачности фоновой картинки
                    {
                        auto setAlphaFromX = [&](float mx) {
                            const float tx0 = g_bgSlider.x + 12.f, tw = g_bgSlider.w - 24.f;
                            Styles::g_custom.bgAlpha = std::clamp(int(std::lround((mx - tx0) / tw * 100.f)), 0, 100);
                        };
                        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT &&
                            e.button.x >= g_bgSlider.x && e.button.x <= g_bgSlider.x + g_bgSlider.w &&
                            e.button.y >= g_bgSlider.y && e.button.y <= g_bgSlider.y + g_bgSlider.h) {
                            g_bgDrag = true;
                            setAlphaFromX(e.button.x);
                        }
                        else if (e.type == SDL_EVENT_MOUSE_MOTION && g_bgDrag) setAlphaFromX(e.motion.x);
                        else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && g_bgDrag) { g_bgDrag = false; SaveCustomStyle(); }
                    }
                    // поля: 0..3 цвета (hex), 4 название. Клик ставит фокус, изменения применяются сразу.
                    for (int i = 0; i < 5; ++i)
                        if (TextField_Handle(*CustomFieldPtr(i), e)) g_customFocus = i;
                    for (int i = 0; i < 5; ++i) CustomFieldPtr(i)->focused = (g_customFocus == i);
                    if (g_customFocus >= 0) {
                        TextField& f = *CustomFieldPtr(g_customFocus);
                        const std::string before = f.text;
                        TextField_Input(f, e);
                        if (f.text != before) {
                            if (g_customFocus == 4) {
                                Styles::g_custom.name = f.text;
                                Styles::RebuildCustom();
                                SaveCustomStyle();
                            }
                            else {
                                SanitizeHexField(f);
                                Theme::Rgb col;
                                if (Styles::ParseHex(f.text, col)) {
                                    Theme::Rgb* targets[4] = { &Styles::g_custom.bg, &Styles::g_custom.text,
                                                               &Styles::g_custom.accent, &Styles::g_custom.panel };
                                    *targets[g_customFocus] = col;
                                    Styles::RebuildCustom();
                                    SaveCustomStyle();
                                }
                            }
                        }
                    }
                    if (TextButton_Handle(g_customBackButton, e)) {
                        LeaveCustomPage(window);
                    }
                    else if (TextButton_Handle(g_customSaveButton, e)) {
                        SaveCurrentStyle();
                        g_saveFlashTick = SDL_GetTicks();
                        g_deleteArmTick = 0;
                    }
                    else if (TextButton_Handle(g_customDeleteButton, e)) {
                        const Uint64 now = SDL_GetTicks();
                        if (g_deleteArmTick != 0 && now - g_deleteArmTick < 2500) {
                            DeleteCurrentStyle();   // второе нажатие: удаляем из сохранённых (рабочий стиль остаётся на экране)
                            g_deleteArmTick = 0;
                        }
                        else g_deleteArmTick = now;
                    }
                    else if (TextButton_Handle(g_customBgButton, e)) {
                        ChooseBackgroundFile(window);
                    }
                    else if (TextButton_Handle(g_customBgClearButton, e)) {
                        ClearBackground();
                    }
                    else if (TextButton_Handle(g_customResetButton, e)) {
                        const std::string oldBg = Styles::g_custom.bgImage;
                        Styles::g_custom = Styles::CustomSpec{};
                        ReleaseBgFile(oldBg);
                        Styles::RebuildCustom();
                        SaveCustomStyle();
                        SyncCustomFields();
                        g_deleteArmTick = 0;
                    }
                    else if (TextButton_Handle(g_customFontButton, e)) {
                        Styles::g_custom.font = (Styles::g_custom.font + 1) % TextDefaults::kFontCount;
                        Styles::RebuildCustom();
                        SaveCustomStyle();
                    }
                    else if (TextButton_Handle(g_customLookButton, e)) {
                        Styles::g_custom.look = (Styles::g_custom.look + 1) % Theme::LookCount;   // FLAT > HUD > BLADE > NEON > REGAL
                        Styles::RebuildCustom();
                        SaveCustomStyle();
                    }
                }
                else if (!g_menuInterfacePage) {
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
                    else if (!g_lngDropdownOpen && TextButton_Handle(g_customButton, e)) {
                        g_lngDropdownOpen = false;
                        OpenCustomPage(window);
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
    TextButton_Destroy(g_customButton);
    TextButton_Destroy(g_customBackButton);
    TextButton_Destroy(g_customResetButton);
    TextButton_Destroy(g_customBgButton);
    TextButton_Destroy(g_customBgClearButton);
    if (g_bgTex) { SDL_DestroyTexture(g_bgTex); g_bgTex = nullptr; }
    TextButton_Destroy(g_customFontButton);
    TextButton_Destroy(g_customLookButton);
    TextButton_Destroy(g_pickerDoneButton);
    TextButton_Destroy(g_customSaveButton);
    TextButton_Destroy(g_customDeleteButton);
    TextField_Destroy(g_customName);
    if (g_wheelTex) { SDL_DestroyTexture(g_wheelTex); g_wheelTex = nullptr; }
    for (auto& f : g_customField) TextField_Destroy(f);
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
    CuteScene::Release();   // текстуры PRO CUTE (обои, лепестки, формы) - до уничтожения рендерера
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
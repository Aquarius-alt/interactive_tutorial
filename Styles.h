#pragma once
// Стили интерфейса. Выбор: меню -> INTERFACE -> STYLE (кнопка переключает стили по кругу), хранится в style.cfg.
// Стиль задаёт палитру Theme.h (фон, линии, акценты, панели) и набор спрайтов проводника.
//
//   CAPRI    - нынешний стиль приложения.
//   MINIMAL  - плоский тёмно-серый фон, белый текст, шрифт Helvetica, без декоративных эффектов (kStyles[2]).
//   PRO CUTE - самый «ламповый» стиль: пастельно-розовая палитра, вид Theme::LookCute (все углы скруглены,
//              кнопки-«леденцы», мягкое свечение), на фоне случайные обои (папка wallpapers/, список в Wallpapers.h)
//              и падающие лепестки сакуры (CuteScene.h). Проводник - Астольфо.
//   ASTOLFO  - палитра по арту персонажа: розовые волосы, белый плащ с мехом, бордовая подкладка, золотая
//              отделка, чёрная форма, фиалковые глаза. Значения лежат в kStyles[1]; проводник в обучении и в
//              случайных визитах - сам Астольфо (спрайты в AstolfoData.h, имя берётся из nameEn / nameRu).
//              Всё, что читает цвета из Theme:: (фон, HUD-рамка, переходы, заставка, обучение, экраны, кнопки,
//              поля ввода), переключается само. Семантические цвета (зелёный = успех, красный = ошибка,
//              цвета рангов и групп уровней) намеренно общие для обоих стилей.
#include <SDL3/SDL.h>
#include <cstdio>
#include <string>
#include <vector>
#include "Theme.h"
#include "Tutorial.h"
#include "AstolfoData.h"

namespace Styles {

struct Style {
    const char* id;                 // для файла настроек
    const char* nameEn; const char* nameRu;
    Theme::Rgb cyan, cyanHot, coral, coralHot, panel, panelHi, bgTop, bgBottom, ink, inkDim;
    const CapriData::Sprite* sprites;   // 4 позы: Stand, Excited, Roam, Dance (как в CapriData.h)
    bool minimal = false;               // true = плоский минималистичный вид (без HUD-рамок, свечения, срезанных углов)
    int font = 0;                       // шрифт интерфейса: номер в TextDefaults::kFontNames (0 = Press Start 2P)
    int look = Theme::LookHud;          // вид HUD (Theme::LookId); при minimal = true не используется
    bool wallpaper = false;             // true = на фоне случайные обои из Wallpapers.h (только вид CUTE)
};

inline Style kStyles[] = {   // не const: слот CUSTOM меняется редактором стиля
    // 0: CAPRI (как сейчас)
    { "CAPRI", "CAPRI", "КАПРИ",
      { 96, 228, 232 }, { 200, 255, 255 }, { 214, 84, 78 }, { 255, 125, 110 },
      { 26, 12, 18 }, { 46, 18, 26 }, { 58, 17, 24 }, { 11, 15, 25 },
      { 205, 255, 255 }, { 150, 190, 215 },
      CapriData::kSprites },

    // 1: ASTOLFO - палитра с арта персонажа (адаптирована под тёмный интерфейс)
    //   cyan/cyanHot   -> розовые волосы и белый мех плаща (основные линии и текст, наведение / фокус)
    //   coral/coralHot -> золотая отделка доспехов (акценты, активное действие)
    //   panel/panelHi  -> чёрная форма с винным оттенком (заливка панелей и кнопок)
    //   bgTop/bgBottom -> бордовая подкладка плаща сверху, почти чёрный баклажан снизу
    //   ink/inkDim     -> тёплый белый (ввод, вопросы) и пыльно-розовый (подписи, заголовки блоков)
    { "ASTOLFO", "ASTOLFO", "АСТОЛЬФО",
      { 255, 163, 190 }, { 255, 228, 238 }, { 224, 178, 92 }, { 255, 215, 130 },
      { 22, 10, 18 }, { 52, 20, 38 }, { 92, 22, 44 }, { 16, 8, 20 },
      { 255, 240, 244 }, { 214, 150, 178 },
      AstolfoData::kSprites, false, 0, Theme::LookRegal },   // 4 позы Астольфо (AstolfoData.h); вид REGAL: золотая рамка с ромбами

    // 2: MINIMAL - максимум минимализма: тёмно-серый плоский фон, белый текст, шрифт Helvetica.
    //   Акцентных цветов нет: cyan/coral = белый и светло-серый, поэтому линии, подписи и наведение монохромные.
    //   Проводник остаётся Капри (спрайты общие с первым стилем).
    { "MINIMAL", "MINIMAL", "МИНИМАЛ",
      { 255, 255, 255 }, { 255, 255, 255 }, { 200, 200, 200 }, { 255, 255, 255 },
      { 38, 38, 38 }, { 54, 54, 54 }, { 30, 30, 30 }, { 30, 30, 30 },
      { 255, 255, 255 }, { 170, 170, 170 },
      CapriData::kSprites, true, 1 },

    // 3: PRO CUTE - пастельная «лампа»: розовый текст и линии, горячий розовый акцент, сливовые панели.
    //   cyan/cyanHot   -> нежно-розовые линии и текст, наведение / фокус почти белые
    //   coral/coralHot -> яркий «конфетный» розовый (активное действие, заливка кнопок при наведении)
    //   panel/panelHi  -> тёплая сливовая «ягода»: на ней светлый текст читается на любых обоях
    //   bgTop/bgBottom -> сиреневый верх и тёмно-сливовый низ (видны, пока обои не загружены)
    //   ink/inkDim     -> тёплый белый и пыльно-розовый
    { "PROCUTE", "PRO CUTE", "ПРО КЬЮТ",
      { 255, 205, 226 }, { 255, 240, 247 }, { 255, 128, 176 }, { 255, 170, 204 },
      { 52, 24, 58 }, { 86, 38, 88 }, { 150, 86, 150 }, { 58, 30, 78 },
      { 255, 244, 250 }, { 232, 176, 210 },
      AstolfoData::kSprites, false, 0, Theme::LookCute, true },

    // 4: CUSTOM - свой стиль. Значения ниже только заглушка: реальные цвета берутся из g_custom (RebuildCustom),
    //   их задаёт игрок в меню -> ИНТЕРФЕЙС -> СОЗДАТЬ СТИЛЬ, хранятся в custom.cfg.
    { "CUSTOM", "CUSTOM", "СВОЙ",
      { 255, 255, 255 }, { 120, 170, 255 }, { 120, 170, 255 }, { 170, 205, 255 },
      { 44, 44, 44 }, { 58, 58, 58 }, { 32, 32, 32 }, { 32, 32, 32 },
      { 255, 255, 255 }, { 175, 175, 175 },
      CapriData::kSprites, true, 1 },
};
constexpr int kCustomIndex = 4;
constexpr int kStyleCount = int(sizeof(kStyles) / sizeof(kStyles[0]));

inline int g_current = 0;

// Имя проводника: у стилей со спрайтами Капри (CAPRI, MINIMAL, свои стили) это всегда Капри,
// а не название стиля.
// Спрайты Астольфо (ASTOLFO, PRO CUTE) - проводник Астольфо, имя не совпадает с названием стиля.
inline const char* GuideEn(const Style& s) {
    return s.sprites == CapriData::kSprites ? "CAPRI" : (s.sprites == AstolfoData::kSprites ? "ASTOLFO" : s.nameEn);
}
inline const char* GuideRu(const Style& s) {
    return s.sprites == CapriData::kSprites ? "КАПРИ" : (s.sprites == AstolfoData::kSprites ? "АСТОЛЬФО" : s.nameRu);
}

inline const Style& Current() { return kStyles[g_current]; }

inline void Apply(int index) {
    if (index < 0 || index >= kStyleCount) index = 0;
    g_current = index;
    const Style& s = kStyles[index];
    Theme::Cyan = s.cyan;       Theme::CyanHot = s.cyanHot;
    Theme::Coral = s.coral;     Theme::CoralHot = s.coralHot;
    Theme::Panel = s.panel;     Theme::PanelHi = s.panelHi;
    Theme::BgTop = s.bgTop;     Theme::BgBottom = s.bgBottom;
    Theme::Ink = s.ink;         Theme::InkDim = s.inkDim;
    Theme::Minimal = s.minimal;
    Theme::Look = s.minimal ? Theme::LookFlat : s.look;   // вид HUD: HUD / BLADE / NEON / REGAL / CUTE
    Theme::CuteWallpaper = s.wallpaper && !s.minimal;      // случайные обои только у встроенного PRO CUTE
    TextDefaults::FontId = s.font;           // шрифт интерфейса (кэш текста учитывает номер шрифта)
    TextDefaults::Helvetica = s.font != 0;
    // цвета текста по умолчанию (когда вызов DrawUtf8Text / DrawCenteredText не передал свои)
    TextDefaults::AccentR = s.cyan.r; TextDefaults::AccentG = s.cyan.g; TextDefaults::AccentB = s.cyan.b;
    TextDefaults::InkR = s.ink.r;     TextDefaults::InkG = s.ink.g;     TextDefaults::InkB = s.ink.b;
    Tutorial::SetSprites(s.sprites);
    Tutorial::SetGuideName(GuideEn(s), GuideRu(s));   // проводник = персонаж стиля (CAPRI / ASTOLFO)
}

// ---------------------------------------------------------------------------
// Свой стиль: игрок задаёт 4 цвета, шрифт и вид (плоский / HUD), остальное выводится из них.
//   text   -> линии, подписи, текст кнопок и ввода (cyan, ink)
//   accent -> наведение, фокус, акценты (cyanHot, coral)
//   panel  -> заливка кнопок и панелей
//   bg     -> фон окна
// ---------------------------------------------------------------------------
struct CustomSpec {
    Theme::Rgb bg{ 32, 32, 32 }, text{ 255, 255, 255 }, accent{ 120, 170, 255 }, panel{ 44, 44, 44 };
    std::string name = "CUSTOM";   // название стиля (его видно на кнопке STYLE и в списке сохранённых)
    int font = 1;                  // номер шрифта в TextDefaults::kFontNames
    std::string bgImage;          // имя файла фоновой картинки в папке настроек (bg_XXXXXXXX.bmp); пусто = без картинки
    int bgAlpha = 40;             // непрозрачность картинки в процентах (0 = не видно, 100 = полностью)
    int look = Theme::LookFlat;   // вид: Theme::LookId (0 = плоский, 1 = HUD, 2 = BLADE, 3 = NEON, 4 = REGAL, 5 = CUTE)
};

// Названия видов: для файла настроек и для кнопки ВИД в редакторе.
inline const char* const kLookIds[Theme::LookCount] = { "flat", "hud", "blade", "neon", "regal", "cute" };
inline const char* const kLookEn[Theme::LookCount]  = { "FLAT", "HUD", "BLADE", "NEON", "REGAL", "CUTE" };
inline const char* const kLookRu[Theme::LookCount]  = { "ПЛОСКИЙ", "HUD", "КЛИНОК", "НЕОН", "РЫЦАРСКИЙ", "МИЛЫЙ" };
inline int LookFromId(const std::string& v) {
    for (int i = 0; i < Theme::LookCount; ++i) if (v == kLookIds[i]) return i;
    return Theme::LookFlat;
}
inline CustomSpec g_custom;                 // стиль, который сейчас редактируется / применён
inline std::vector<CustomSpec> g_saved;     // сохранённые игроком стили (файл styles.cfg)

inline bool ParseHex(const std::string& s, Theme::Rgb& out) {
    std::string d;
    for (char ch : s) if (ch != '#') d += ch;
    if (d.size() != 6) return false;
    unsigned v = 0;
    for (char ch : d) {
        int x;
        if (ch >= '0' && ch <= '9') x = ch - '0';
        else if (ch >= 'a' && ch <= 'f') x = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F') x = ch - 'A' + 10;
        else return false;
        v = v * 16 + unsigned(x);
    }
    out = Theme::Rgb{ Uint8(v >> 16), Uint8(v >> 8), Uint8(v) };
    return true;
}

inline std::string ToHex(Theme::Rgb c) {
    char b[8];
    std::snprintf(b, sizeof b, "#%02X%02X%02X", unsigned(c.r), unsigned(c.g), unsigned(c.b));
    return b;
}

// Пересобирает слот CUSTOM из g_custom; если он сейчас активен, сразу применяет (живой предпросмотр).
inline void RebuildCustom() {
    Style& s = kStyles[kCustomIndex];
    const CustomSpec& c = g_custom;
    const Theme::Rgb white{ 255, 255, 255 };
    s.cyan = c.text;            s.cyanHot = c.accent;
    s.coral = c.accent;         s.coralHot = Theme::Mix(c.accent, white, 0.35f);
    s.panel = c.panel;          s.panelHi = Theme::Mix(c.panel, c.accent, 0.18f);
    s.bgTop = c.bg;             s.bgBottom = c.bg;
    s.ink = c.text;             s.inkDim = Theme::Mix(c.text, c.bg, 0.4f);
    s.minimal = (c.look == Theme::LookFlat);   s.look = c.look;   s.font = c.font;
    const char* nm = c.name.empty() ? "CUSTOM" : c.name.c_str();   // указатель живёт, пока не изменено g_custom.name
    s.nameEn = nm;
    s.nameRu = (c.name.empty() || c.name == "CUSTOM") ? "СВОЙ" : c.name.c_str();
    if (g_current == kCustomIndex) Apply(kCustomIndex);
}

// ---------------------------------------------------------------------------
// Готовые HUD-пресеты для редактора стилей. При первом запуске (см. SeedBuiltinPresets в main.cpp)
// они попадают в библиотеку сохранённых стилей: их можно листать кнопкой STYLE, править и удалять.
// ---------------------------------------------------------------------------
inline CustomSpec MakePreset(const char* name, Theme::Rgb bg, Theme::Rgb text, Theme::Rgb accent,
                             Theme::Rgb panel, int font, int look) {
    CustomSpec c;
    c.name = name; c.bg = bg; c.text = text; c.accent = accent; c.panel = panel;
    c.font = font; c.look = look;
    return c;
}

inline std::vector<CustomSpec> BuiltinPresets() {
    return {
        // неоновый нуар: почти чёрный фиолет, горячая маджента, пиксельный шрифт, вид NEON
        MakePreset("NEON NOIR",   { 18, 9, 31 },  { 242, 233, 255 }, { 255, 46, 151 }, { 30, 15, 51 }, 0, Theme::LookNeon),
        // фосфорный терминал: тёмно-зелёный экран, мятный текст, лаймовый акцент, Consolas, вид BLADE
        MakePreset("PHOSPHOR",    { 4, 18, 10 },  { 124, 255, 178 }, { 216, 255, 62 }, { 10, 36, 22 }, 8, Theme::LookBlade),
        // розовый рыцарь (Астольфо): бордовая подкладка, розоватый мех, золото доспехов, Georgia, вид REGAL
        MakePreset("ROSE KNIGHT", { 58, 16, 32 }, { 255, 228, 238 }, { 224, 178, 92 }, { 36, 12, 24 }, 6, Theme::LookRegal),
    };
}

inline void Cycle() { Apply((g_current + 1) % kStyleCount); }

inline int IndexById(const std::string& id) {
    for (int i = 0; i < kStyleCount; ++i) if (id == kStyles[i].id) return i;
    return 0;
}

} // namespace Styles

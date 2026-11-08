#pragma once
// Стили интерфейса. Выбор: меню -> INTERFACE -> STYLE (кнопка переключает стили по кругу), хранится в style.cfg.
// Стиль задаёт палитру Theme.h (фон, линии, акценты, панели) и набор спрайтов проводника.
//
//   CAPRI    - нынешний стиль приложения.
//   ASTOLFO  - палитра по арту персонажа: розовые волосы, белый плащ с мехом, бордовая подкладка, золотая
//              отделка, чёрная форма, фиалковые глаза. Значения лежат в kStyles[1]; проводник в обучении и в
//              случайных визитах - сам Астольфо (спрайты в AstolfoData.h, имя берётся из nameEn / nameRu).
//              Всё, что читает цвета из Theme:: (фон, HUD-рамка, переходы, заставка, обучение, экраны, кнопки,
//              поля ввода), переключается само. Семантические цвета (зелёный = успех, красный = ошибка,
//              цвета рангов и групп уровней) намеренно общие для обоих стилей.
#include <SDL3/SDL.h>
#include <string>
#include "Theme.h"
#include "Tutorial.h"
#include "AstolfoData.h"

namespace Styles {

struct Style {
    const char* id;                 // для файла настроек
    const char* nameEn; const char* nameRu;
    Theme::Rgb cyan, cyanHot, coral, coralHot, panel, panelHi, bgTop, bgBottom, ink, inkDim;
    const CapriData::Sprite* sprites;   // 4 позы: Stand, Excited, Roam, Dance (как в CapriData.h)
};

inline const Style kStyles[] = {
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
      AstolfoData::kSprites },   // 4 позы Астольфо (AstolfoData.h): обучение и случайные визиты проводника
};
constexpr int kStyleCount = int(sizeof(kStyles) / sizeof(kStyles[0]));

inline int g_current = 0;

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
    // цвета текста по умолчанию (когда вызов DrawUtf8Text / DrawCenteredText не передал свои)
    TextDefaults::AccentR = s.cyan.r; TextDefaults::AccentG = s.cyan.g; TextDefaults::AccentB = s.cyan.b;
    TextDefaults::InkR = s.ink.r;     TextDefaults::InkG = s.ink.g;     TextDefaults::InkB = s.ink.b;
    Tutorial::SetSprites(s.sprites);
    Tutorial::SetGuideName(s.nameEn, s.nameRu);   // проводник = персонаж стиля (CAPRI / ASTOLFO)
}

inline void Cycle() { Apply((g_current + 1) % kStyleCount); }

inline int IndexById(const std::string& id) {
    for (int i = 0; i < kStyleCount; ++i) if (id == kStyles[i].id) return i;
    return 0;
}

} // namespace Styles

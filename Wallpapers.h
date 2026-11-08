#pragma once
// Обои стиля PRO CUTE. Девять артов зашиты в exe как ресурсы (aquadeck.rc, RCDATA 301..309),
// исходные PNG лежат в папке wallpapers/. Если ресурса нет (сборка без .rc), берётся файл
// wallpapers/wpNN.png рядом с exe.
//
// Чтобы добавить свои обои: положите PNG в wallpapers/, добавьте строку в aquadeck.rc
// (следующий номер RCDATA) и запись в kList ниже (fx, fy - какая точка картинки должна оказаться
// в центре окна, когда картинку приходится обрезать: 0..1).
#include <SDL3/SDL.h>

namespace Wallpapers {

struct Info {
    const char* file;   // имя файла в wallpapers/
    int resId;          // номер ресурса RCDATA
    float fx, fy;       // «лицо» на картинке (доля ширины и высоты)
};

inline constexpr Info kList[] = {
    { "wp01.png", 301, 0.36f, 0.48f },   // поле цветов
    { "wp02.png", 302, 0.50f, 0.42f },   // пиксель-арт, розовый фон
    { "wp03.png", 303, 0.50f, 0.50f },   // REC-рамка
    { "wp04.png", 304, 0.56f, 0.50f },   // пиксель-арт, фиолетовый фон
    { "wp05.png", 305, 0.50f, 0.42f },   // кимоно
    { "wp06.png", 306, 0.50f, 0.42f },   // салют на жёлтом
    { "wp07.png", 307, 0.50f, 0.50f },   // розовый постер
    { "wp08.png", 308, 0.50f, 0.46f },   // кролик в худи
    { "wp09.png", 309, 0.38f, 0.50f },   // цветок и небо
};
inline constexpr int kCount = int(sizeof(kList) / sizeof(kList[0]));

// RGBA32-поверхность с картинкой (освобождать SDL_DestroySurface) или nullptr.
SDL_Surface* Load(int index);

} // namespace Wallpapers

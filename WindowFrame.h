#pragma once
#include <SDL3/SDL_rect.h>

struct SDL_Window;

// Сколько зон "без перетаскивания" можно передать в SetNoDragRects за кадр.
// Массив nd[] в main.cpp Draw() должен быть такого же размера: экраны пишут в него nd[ndCount++].
constexpr int kMaxNoDragRects = 32;

bool EnableWindowFrame(SDL_Window* window, int dragHeight = 0, int resizeBorder = 8);
void EnableRoundedCorners(SDL_Window* window);
void ApplyRoundRegion(SDL_Window* window, int radius);
void SetNoDragRects(const SDL_Rect* rects, int count);
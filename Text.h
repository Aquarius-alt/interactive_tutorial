#pragma once
#include <SDL3/SDL.h>

// Рисует надпись по центру окна (w, h - размер области вывода).
// scale - размер букв: 2 мелко, 3 средне, 4 крупно
void DrawCenteredText(SDL_Renderer* r, const char* text, int w, int h,
    float scale = 3.0f, float yOffset = 0.0f,
    Uint8 red = 90, Uint8 green = 200, Uint8 blue = 255);

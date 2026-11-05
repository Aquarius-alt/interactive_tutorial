#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <cstddef>

void DrawCenteredText(SDL_Renderer* r, const char* text, int w, int h,
    float scale = 3.0f, float yOffset = 0.0f,
    Uint8 red = 90, Uint8 green = 200, Uint8 blue = 255);

// UTF-8 helpers shared by all editable text controls.
void DrawUtf8Text(SDL_Renderer* renderer, const std::string& text,
    float x, float y, int fontHeight,
    Uint8 red = 200, Uint8 green = 240, Uint8 blue = 255, Uint8 alpha = 255);
int NormalizeTextFontHeight(int fontHeight);
float Utf8TextWidth(const std::string& text, int fontHeight);
size_t Utf8Count(const std::string& text);
void Utf8PopBack(std::string& text);
void Utf8AppendLimited(std::string& target, const char* input, size_t maxCharacters);

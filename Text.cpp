#include "Text.h"
#include <cstring>

void DrawCenteredText(SDL_Renderer* r, const char* text, int w, int h,
    float scale, float yOffset, Uint8 red, Uint8 green, Uint8 blue) {
    const float cs = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
    const float textW = std::strlen(text) * cs * scale;
    const float textH = cs * scale;

    // координаты делим на scale, потому что масштаб применяется к ним
    const float x = (w - textW) * 0.5f / scale;
    const float y = ((h - textH) * 0.5f + yOffset) / scale;

    SDL_SetRenderScale(r, scale, scale);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // мягкое свечение: слабые копии со смещением
    SDL_SetRenderDrawColor(r, red, green, blue, 45);
    const float o = 0.6f;
    SDL_RenderDebugText(r, x - o, y, text);
    SDL_RenderDebugText(r, x + o, y, text);
    SDL_RenderDebugText(r, x, y - o, text);
    SDL_RenderDebugText(r, x, y + o, text);

    // сама надпись
    SDL_SetRenderDrawColor(r, red, green, blue, 255);
    SDL_RenderDebugText(r, x, y, text);

    SDL_SetRenderScale(r, 1.0f, 1.0f);   // вернуть масштаб, чтобы не сломать остальное
}
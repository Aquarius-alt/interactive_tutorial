#pragma once
#include <SDL3/SDL.h>

struct TextButton {
    float x = 0, y = 0, w = 150, h = 40;
    const char* label = "";
    bool hovered = false;

    // внутреннее (не трогать)
    SDL_Texture* core = nullptr;
    SDL_Texture* glow = nullptr;
    SDL_Renderer* owner = nullptr;
    float         bw = 0, bh = 0;

    // анимация (внутреннее): плавное наведение, глитч при наведении, вспышка при клике
    float  hoverT = 0;          // 0..1, заполнение кораллом слева направо
    bool   wasHovered = false;
    Uint64 hoverStart = 0;      // момент начала наведения (глитч и «расшифровка» подписи)
    Uint64 lastTick = 0;
    Uint64 clickTick = 0;       // момент клика (вспышка)
};

void TextButton_Draw(SDL_Renderer* r, TextButton& b);
bool TextButton_Handle(TextButton& b, const SDL_Event& e); // true = клик ЛКМ
SDL_Rect TextButton_Rect(const TextButton& b);             // для зоны без перетаскивания
void TextButton_Destroy(TextButton& b);                    // до SDL_DestroyRenderer
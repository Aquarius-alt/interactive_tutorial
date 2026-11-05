#pragma once
#include <SDL3/SDL.h>

struct MenuButton {
    float x = 0, y = 0;      // левый верхний угол иконки
    float size = 28;         // ширина иконки, px
    bool  hovered = false;
    bool  open = false;      // false = полоски, true = крестик (экран меню)

    // внутреннее (не трогать)
    float         anim = 0;  // 0..1 прогресс превращения
    Uint64        lastTick = 0;
    SDL_Texture* core = nullptr;
    SDL_Texture* glow = nullptr;
    SDL_Renderer* owner = nullptr;
    float         builtSize = 0;
};

inline float MenuButtonHeight(float size) { return size * 0.7f; }

void MenuButton_Draw(SDL_Renderer* r, MenuButton& b);
bool MenuButton_Handle(MenuButton& b, const SDL_Event& e); // true = клик ЛКМ
void MenuButton_Destroy(MenuButton& b);                    // до SDL_DestroyRenderer
#pragma once
#include <SDL3/SDL.h>
#include "TextButton.h"
#include "Text.h"
#include <string>

struct TextField {
    float x = 0, y = 0, w = 320, h = 40;
    const char* placeholder = "";   // серая подсказка, пока поле пустое
    bool password = false;          // true = вместо символов звёздочки
    bool focused = false;
    bool hovered = false;
    size_t maxLen = 32;
    std::string text;
    UndoHistory undo;               // история для Ctrl+Z (внутреннее)

    TextButton box;                 // рамка с неоном (внутреннее)
};

void TextField_Draw(SDL_Renderer* r, TextField& f);
bool TextField_Handle(TextField& f, const SDL_Event& e);  // true = клик по полю
void TextField_Input(TextField& f, const SDL_Event& e);   // ввод, Backspace, Ctrl+V / Ctrl+C / Ctrl+Z
SDL_Rect TextField_Rect(const TextField& f);
void TextField_Destroy(TextField& f);
#pragma once
#include <SDL3/SDL.h>
#include "TextButton.h"
#include <string>

// Многострочное текстовое поле (рамка с неоном, перенос строк, мигающий курсор).
// Показывает конец текста; колесом мыши над полем можно листать вверх.
// Внизу справа, под рамкой, рисуется счётчик символов "n/max".

struct TextArea {
    float x = 0, y = 0, w = 320, h = 120;
    const char* placeholder = "";   // бледная подсказка, пока поле пустое
    size_t maxLen = 500;
    bool focused = false;
    bool hovered = false;
    std::string text;
    int scroll = 0;                 // на сколько строк прокручено вверх от конца текста

    TextButton box;                 // рамка с неоном (внутреннее)
};

void     TextArea_Draw(SDL_Renderer* r, TextArea& a);
bool     TextArea_Handle(TextArea& a, const SDL_Event& e);   // true = клик по полю (и колесо над полем)
void     TextArea_Input(TextArea& a, const SDL_Event& e);    // ввод символов и Backspace
SDL_Rect TextArea_Rect(const TextArea& a);
void     TextArea_Destroy(TextArea& a);                      // до SDL_DestroyRenderer

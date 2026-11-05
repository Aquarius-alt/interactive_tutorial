#pragma once
#include <SDL3/SDL.h>
#include <string>

// Аватар текущего пользователя: круглая картинка (или пустой круг) с неоновым контуром.
// Картинка хранится как %APPDATA%\myproject\login\avatar_<hex логина>.bmp (квадрат 256x256).
// Нужен SDL 3.4+ (SDL_LoadSurface читает PNG и BMP).

bool Avatar_Load(const std::string& login);        // подхватить сохранённый аватар; false = его нет
bool Avatar_SetFromFile(const std::string& login,  // PNG/BMP: обрезать до квадрата, сохранить, показать
                        const std::string& imagePath);
bool Avatar_Has();
void Avatar_Clear();                               // при выходе из аккаунта (файл остаётся)

// Рисует аватар диаметром d с центром в (cx, cy). lit = контур светится ярче.
void Avatar_Draw(SDL_Renderer* r, float cx, float cy, float d, bool lit);

void Avatar_Destroy();                             // до SDL_DestroyRenderer

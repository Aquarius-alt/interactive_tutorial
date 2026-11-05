#pragma once
#include <SDL3/SDL.h>
#include <string>

// Экран профиля: крупный аватар, логин, кнопка добавления/смены аватара и BACK.

enum class ProfileResult { None, Back };

void          Profile_Open(SDL_Window* window, const std::string& login);
void          Profile_Close();
void          Profile_Poll();    // каждый кадр: подхватывает файл, выбранный в диалоге
void          Profile_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount);
ProfileResult Profile_Handle(const SDL_Event& e);   // Back = кнопка BACK или Esc
void          Profile_Destroy();                    // до SDL_DestroyRenderer

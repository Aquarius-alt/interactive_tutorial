#pragma once
#include <SDL3/SDL.h>

// Мини-окно "FORGOT PASSWORD": ввод кодового слова -> полоса поиска -> логин и пароль.
// Окно модальное: пока оно открыто, все события должны идти только ему.

void Recover_Open(SDL_Window* window);   // открыть (включает ввод текста)
void Recover_Close();                    // закрыть и затереть данные
bool Recover_IsOpen();
void Recover_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount);
void Recover_Handle(const SDL_Event& e);
void Recover_Destroy();                  // до SDL_DestroyRenderer

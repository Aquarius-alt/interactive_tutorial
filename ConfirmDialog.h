#pragma once
#include <SDL3/SDL.h>

enum class ConfirmResult { None, Yes, No };

void          Confirm_Open();
void          Confirm_Close();
bool          Confirm_IsOpen();
void          Confirm_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount);
ConfirmResult Confirm_Handle(const SDL_Event& e);
void          Confirm_Destroy();   // до SDL_DestroyRenderer
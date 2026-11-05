#pragma once
#include <SDL3/SDL.h>
#include <string>

enum class SignUpResult { None, Back, Done };

struct SignUpData {
    std::string login, password, keyword;
};

void SignUp_Open(SDL_Window* window);    // при входе на экран
void SignUp_Close(SDL_Window* window);   // при выходе (очищает поля)
void SignUp_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount);
SignUpResult SignUp_Handle(const SDL_Event& e);
SignUpData SignUp_Data();                // данные после результата Done
void SignUp_Destroy();                   // до SDL_DestroyRenderer
SignUpResult SignUp_Poll();   // каждый кадр: завершение сохранения
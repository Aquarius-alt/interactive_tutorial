#pragma once
#include <SDL3/SDL.h>
#include <string>

enum class SignInResult { None, Back, Done, Forgot };

struct SignInData {
    std::string login, password;
};

void SignIn_Open(SDL_Window* window);    // при входе на экран
void SignIn_Close(SDL_Window* window);   // при выходе (очищает поля)
void SignIn_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount);
SignInResult SignIn_Handle(const SDL_Event& e);
void SignIn_SetError(const std::string& msg);   // показать ошибку и очистить пароль (вход не удался)
SignInData SignIn_Data();                // данные после результата Done
void SignIn_Destroy();                   // до SDL_DestroyRenderer
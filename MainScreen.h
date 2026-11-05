#pragma once
#include <SDL3/SDL.h>
#include <string>

// Главный экран после успешного входа. Пока пустой: надпись "by Aquarius" вверху, аватар слева
// сверху, рядом логин, по клику на аватар выпадает список: PROFILE и LOGOUT.
// Под аватаром кнопка "+": открывает плавающее окно с двумя кнопками, IMPORT и CREATE.
// Окно закрывается по Esc, по клику мимо него или по выбору одной из кнопок.

enum class MainResult { None, Import, Create, Profile, Logout };

void       Main_Open(const std::string& login);   // при входе на экран
void       Main_Close();                          // при выходе из аккаунта
void       Main_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount);
MainResult Main_Handle(const SDL_Event& e);       // Profile / Logout = пункты списка аватара, Import / Create = кнопки окна "+"
void       Main_Destroy();                        // до SDL_DestroyRenderer
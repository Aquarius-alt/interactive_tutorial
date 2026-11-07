#pragma once
#include <SDL3/SDL.h>
#include <string>

// Главный экран после успешного входа. Пока пустой: надпись "by Aquarius" вверху, аватар слева
// сверху, рядом логин, по клику на аватар выпадает список: PROFILE и LOGOUT.
// Под аватаром сетка репозиториев пользователя: карточка = обложка + название (пока без действия по клику),
// сразу после последней карточки стоит "+"; ряды, не влезающие в окно, листаются колесом или ползунком.
// Кнопка "+" открывает плавающее окно с двумя кнопками, IMPORT и CREATE.
// Окно закрывается по Esc, по клику мимо него или по выбору одной из кнопок.

// Клик ЛКМ по карточке репозитория открывает маленькое меню: DELETE, EDIT, OPEN.
//   DELETE - сначала окно "ARE YOU SURE?" (как в SignUp), после YES удаляет папку репозитория с диска
//            (внутри экрана, список обновляется сам);
//   EDIT   - MainResult::Edit, папку репозитория отдаёт Main_RepoDir();
//   OPEN   - MainResult::Open (пока ничего не делает), папка там же.
// Меню закрывается по Esc, по клику мимо него или по выбору пункта.

enum class MainResult { None, Import, Create, Profile, Logout, Edit, Open };

void       Main_Open(const std::string& login);   // при входе на экран
void       Main_Reload();                         // перечитать репозитории с диска (после сохранения нового)
void       Main_Close();                          // при выходе из аккаунта
std::string Main_RepoDir();                       // папка репозитория, выбранного в меню (после Edit / Open)
void       Main_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount);
MainResult Main_Handle(const SDL_Event& e);       // Profile / Logout = пункты списка аватара, Import / Create = кнопки окна "+"
bool       Main_IsModalOpen();                    // открыто окно "+" (IMPORT / CREATE) поверх экрана
void       Main_Destroy();                        // до SDL_DestroyRenderer
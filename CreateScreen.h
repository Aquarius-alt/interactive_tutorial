#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "TheoryDialog.h"

// Экран создания (кнопка CREATE в окне "+" на главном экране):
//   слева квадрат для картинки (клик = выбрать PNG/BMP) и под ним четыре вкладки:
//     INFO   - строка названия и поле описания (несколько строк, Enter = новая строка)
//     THEORY - теоретическая часть: кнопка "+" открывает плавающее окно (TheoryDialog):
//              заголовок, содержание, вложения, переключатель "сделано с помощью ИИ", кнопка ADD
//     TESTS  - тесты: та же сетка и окно, что у теории, плюс строка "верные ответы"
//     GAMES  - игры (пока только кнопка "+")
//   внизу BACK и CREATE. Если название пустое, CREATE возвращает на вкладку INFO.

enum class CreateResult { None, Back, Done };

struct CreateData {
    std::string name;          // название (не пустое)
    std::string description;   // описание (может быть пустым, строки разделены '\n')
    std::string imagePath;     // путь к выбранной картинке ("" = не выбрана)
    std::vector<TheoryItem> theory;   // теории, добавленные через "+" на вкладке THEORY
    std::vector<TheoryItem> tests;    // тесты, добавленные через "+" на вкладке TESTS (у каждого заполнено поле answers)
};

void         Create_Open(SDL_Window* window);    // при входе на экран
void         Create_OpenEdit(SDL_Window* window, const CreateData& d);   // правка готового репозитория: те же поля, но заполнены
                                                                         // (заголовок EDIT, вместо CREATE кнопка SAVE)
void         Create_Close(SDL_Window* window);   // при выходе (очищает поля)
void         Create_Poll();                      // каждый кадр: подхватывает файл из диалога
void         Create_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount);
CreateResult Create_Handle(const SDL_Event& e);  // Back = кнопка BACK или Esc, Done = кнопка CREATE
void         Create_Resume(SDL_Window* window, const char* error);   // вернуться на экран после неудачного сохранения (поля не тронуты)
CreateData   Create_Data();                      // данные после результата Done
void         Create_Destroy();                   // до SDL_DestroyRenderer

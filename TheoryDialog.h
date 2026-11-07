#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>

// Плавающее окно "новая теория" (вкладка THEORY экрана CREATE, кнопка "+"):
//   сверху заголовок, под ним большое поле содержания,
//   внизу три кнопки: ATTACH (добавить вложения), MADE WITH AI (переключатель), ADD (добавить).
// Окно ТЕСТА (withAnswers = true) устроено иначе: заголовок, под ним список вопросов с кнопкой "+"
// (клик по вопросу = правка, X = удалить), внизу ATTACH / MADE WITH AI / ADD.
// Под заголовком теста две строки: теги предмета и звание (необязательные). Кнопка "+" открывает
// редактор вопроса в том же окне: текст вопроса, верные ответы, BACK и OK.
// Окно модальное: пока оно открыто, все события должны идти только ему.
// Esc закрывает окно без сохранения. Клик мимо окна ничего не делает (чтобы не потерять текст).

// Вопрос внутри теста: текст и верные ответы (одна строка).
struct TestQuestion {
    std::string text;
    std::string answers;
};

struct TheoryItem {
    std::string              title;
    std::string              content;       // строки разделены '\n'
    std::string              answers;       // верные ответы (только у тестов, одна строка)
    std::vector<std::string> files;         // пути к вложениям
    bool                     madeWithAI = false;
    std::string              subjectTags;   // только у тестов: теги предмета (одна строка, например "ИНФОРМАТИКА, МАТЕМАТИКА")
    std::string              rank;          // только у тестов: звание
    std::vector<TestQuestion> questions;    // только у тестов: вопросы, добавленные кнопкой "+" внутри окна теста
                                            // (content/answers у теста собираются из них для просмотра)
};

enum class TheoryResult { None, Cancel, Added };

void         Theory_Open(SDL_Window* window, bool withAnswers = false);   // открыть пустым; withAnswers = окно теста (есть строка верных ответов)
void         Theory_Edit(SDL_Window* window, const TheoryItem& item, bool withAnswers = false);   // открыть с готовыми данными (кнопка SAVE вместо ADD)
void         Theory_Close();                    // закрыть и сбросить всё
bool         Theory_IsOpen();
void         Theory_Poll();                     // каждый кадр: подхватывает файлы, выбранные в диалоге
void         Theory_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount);
TheoryResult Theory_Handle(const SDL_Event& e); // Added = нажато ADD (данные: Theory_Data), Cancel = Esc
TheoryItem   Theory_Data();                     // данные после результата Added
void         Theory_Destroy();                  // до SDL_DestroyRenderer

#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <functional>

enum class SaveResult { None, Ok, Failed };

// Запускает реальное сохранение в фоновом потоке и анимацию полосы загрузки.
void       SaveOverlay_Start(const std::string& login, const std::string& password,
                             const std::string& keyword);
// Та же полоса, но для любой фоновой задачи (task вызывается в отдельном потоке, true = успех).
// Четыре подписи рисуются в двух рамках по краям полосы (до 9 символов в строке).
void       SaveOverlay_StartTask(std::function<bool()> task,
                                 const std::string& leftTop, const std::string& leftBottom,
                                 const std::string& rightTop, const std::string& rightBottom);
bool       SaveOverlay_Active();                          // идёт анимация/сохранение
void       SaveOverlay_Draw(SDL_Renderer* r, int w, int h);
SaveResult SaveOverlay_Poll();                            // звать каждый кадр; Ok/Failed вернётся один раз
void       SaveOverlay_Destroy();                         // до SDL_DestroyRenderer

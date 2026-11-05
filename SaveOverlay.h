#pragma once
#include <SDL3/SDL.h>
#include <string>

enum class SaveResult { None, Ok, Failed };

// Запускает реальное сохранение в фоновом потоке и анимацию полосы загрузки.
void       SaveOverlay_Start(const std::string& login, const std::string& password,
                             const std::string& keyword);
bool       SaveOverlay_Active();                          // идёт анимация/сохранение
void       SaveOverlay_Draw(SDL_Renderer* r, int w, int h);
SaveResult SaveOverlay_Poll();                            // звать каждый кадр; Ok/Failed вернётся один раз
void       SaveOverlay_Destroy();                         // до SDL_DestroyRenderer

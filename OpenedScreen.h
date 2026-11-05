#pragma once
#include <SDL3/SDL.h>
#include <string>

enum class OpenedResult { None, Back };
void Opened_Open(SDL_Window* window, const std::string& repoDir);
void Opened_Close(SDL_Window* window);
void Opened_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount);
OpenedResult Opened_Handle(SDL_Window* window, const SDL_Event& e);
void Opened_Destroy();

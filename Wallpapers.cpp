#include "Wallpapers.h"
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Wallpapers {

static SDL_Surface* FromMemory(const void* data, size_t size) {
    SDL_IOStream* io = SDL_IOFromConstMem(data, size);
    if (!io) return nullptr;
    return SDL_LoadSurface_IO(io, true);   // PNG / BMP, поток закрывается сам
}

SDL_Surface* Load(int index) {
    if (index < 0 || index >= kCount) return nullptr;
    const Info& wp = kList[index];
    SDL_Surface* src = nullptr;

#ifdef _WIN32
    // 1) ресурс, вшитый в exe
    if (HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(wp.resId), RT_RCDATA)) {
        if (HGLOBAL mem = LoadResource(nullptr, res)) {
            const void* data = LockResource(mem);
            const DWORD size = SizeofResource(nullptr, res);
            if (data && size) src = FromMemory(data, size);
        }
    }
#endif
    // 2) запасной путь: файл рядом с exe
    if (!src) {
        const char* base = SDL_GetBasePath();
        const std::string path = std::string(base ? base : "") + "wallpapers/" + wp.file;
        src = SDL_LoadSurface(path.c_str());
    }
    if (!src) { SDL_Log("wallpaper %d: cannot load: %s", index, SDL_GetError()); return nullptr; }

    SDL_Surface* rgba = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(src);
    return rgba;
}

} // namespace Wallpapers

#include "WindowFrame.h"
#include <SDL3/SDL.h>

#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

namespace {
    struct Config {
        int dragHeight;
        int resizeBorder;
        SDL_Rect nd[kMaxNoDragRects];
        int ndCount;
    };

    Config g_cfg{ 0, 8,{}, 0 };

    HWND GetHwnd(SDL_Window* w) {
        return (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(w),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    }

    SDL_HitTestResult SDLCALL HitTest(SDL_Window* win, const SDL_Point* p, void* data) {
        const Config& cfg = *static_cast<Config*>(data);

        int w = 0, h = 0;
        SDL_GetWindowSize(win, &w, &h);

        const bool left = p->x < cfg.resizeBorder;
        const bool right = p->x >= w - cfg.resizeBorder;
        const bool top = p->y < cfg.resizeBorder;
        const bool bottom = p->y >= h - cfg.resizeBorder;

        if (top && left)     return SDL_HITTEST_RESIZE_TOPLEFT;
        if (top && right)    return SDL_HITTEST_RESIZE_TOPRIGHT;
        if (bottom && left)  return SDL_HITTEST_RESIZE_BOTTOMLEFT;
        if (bottom && right) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
        if (top)             return SDL_HITTEST_RESIZE_TOP;
        if (bottom)          return SDL_HITTEST_RESIZE_BOTTOM;
        if (left)            return SDL_HITTEST_RESIZE_LEFT;
        if (right)           return SDL_HITTEST_RESIZE_RIGHT;

        // зоны кнопок: перетаскивание отключено
        for (int i = 0; i < cfg.ndCount; ++i) {
            const SDL_Rect& r = cfg.nd[i];
            if (p->x >= r.x && p->x < r.x + r.w &&
                p->y >= r.y && p->y < r.y + r.h)
                return SDL_HITTEST_NORMAL;
        }

        if (cfg.dragHeight <= 0 || p->y < cfg.dragHeight)
            return SDL_HITTEST_DRAGGABLE;

        return SDL_HITTEST_NORMAL;
    }
}

void ApplyRoundRegion(SDL_Window* window, int radius) {
    HWND hwnd = GetHwnd(window);
    if (!hwnd) return;
    if (radius <= 0) { SetWindowRgn(hwnd, nullptr, TRUE); return; }   // без скругления (полный экран)
    RECT r;
    GetClientRect(hwnd, &r);
    HRGN rgn = CreateRoundRectRgn(0, 0, r.right + 1, r.bottom + 1, radius * 2, radius * 2);
    SetWindowRgn(hwnd, rgn, TRUE); // система сама освободит rgn
}

void EnableRoundedCorners(SDL_Window* window) {
    HWND hwnd = GetHwnd(window);
    if (!hwnd) return;
    int pref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &pref, sizeof(pref));
}

bool EnableWindowFrame(SDL_Window* window, int dragHeight, int resizeBorder) {
    g_cfg.dragHeight = dragHeight;
    g_cfg.resizeBorder = resizeBorder;
    return SDL_SetWindowHitTest(window, HitTest, &g_cfg);
}

void SetNoDragRects(const SDL_Rect* rects, int count) {
    g_cfg.ndCount = count > kMaxNoDragRects ? kMaxNoDragRects : count;
    for (int i = 0; i < g_cfg.ndCount; ++i) g_cfg.nd[i] = rects[i];
}
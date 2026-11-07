#pragma once
// Логотип приложения (рисуется в левом нижнем углу вместо подписи «by Aquarius»).
#include <SDL3/SDL.h>
#include "LogoData.h"

namespace Logo {
inline SDL_Texture* g_tex = nullptr;
inline SDL_Renderer* g_owner = nullptr;

inline void Draw(SDL_Renderer* r, float x, float y, float size, Uint8 alpha = 235) {
    if (!g_tex || g_owner != r) {
        if (g_tex) SDL_DestroyTexture(g_tex);
        g_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, kLogoSize, kLogoSize);
        if (!g_tex) return;
        SDL_UpdateTexture(g_tex, nullptr, kLogoPixels, kLogoSize * 4);
        SDL_SetTextureBlendMode(g_tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(g_tex, SDL_SCALEMODE_LINEAR);
        g_owner = r;
    }
    SDL_SetTextureAlphaMod(g_tex, alpha);
    const SDL_FRect dst{ x, y, size, size };
    SDL_RenderTexture(r, g_tex, nullptr, &dst);
}

// Вызвать перед SDL_DestroyRenderer
inline void Destroy() {
    if (g_tex) SDL_DestroyTexture(g_tex);
    g_tex = nullptr;
    g_owner = nullptr;
}
} // namespace Logo

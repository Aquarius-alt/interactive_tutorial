#pragma once

struct SDL_Renderer;

// Рамка + подсветка внутрь, граница которой дышит как k(t)*sin(x + сдвиг).
// glow        - максимальная глубина подсветки, px
// glowAlpha   - яркость у самой рамки (0..255)
// waveLength  - примерная длина одной волны вдоль периметра, px
// breathSpeed - скорость дыхания, циклов в секунду (0.35 = спокойно)
// crawlSpeed  - скорость ползания волн вдоль рамки, оборотов волны в секунду
//               (0.05 = очень медленно, отрицательное = в другую сторону)
void DrawBorderGlow(SDL_Renderer* r, int w, int h, int radius, int thickness,
    int glow = 15, int glowAlpha = 150,
    float waveLength = 10.0f, float breathSpeed = 0.1f,
    float crawlSpeed = 0.1f,
    unsigned char red = 255, unsigned char green = 0, unsigned char blue = 0);

// Вызвать перед SDL_DestroyRenderer
void DestroyBorderCache();
#include "Avatar.h"
#include "Theme.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>

namespace {
    const Uint8 &kR = Theme::Cyan.r, &kG = Theme::Cyan.g, &kB = Theme::Cyan.b;   // цвет неона, как у кнопок
    const int   kPad = 12;                      // запас вокруг круга под свечение
    const int   kBig = 256;                     // размер сохранённого квадрата
    const int   kSmall = 80;                    // версия для маленького аватара (чтобы не мылилось)

    // ---------- картинка ----------
    SDL_Surface* g_big = nullptr;               // 256x256, уже с круглой маской
    SDL_Surface* g_small = nullptr;             // 80x80, тоже с маской
    SDL_Texture* g_bigTex = nullptr;
    SDL_Texture* g_smallTex = nullptr;
    SDL_Renderer* g_texOwner = nullptr;

    // ---------- неоновые кольца (по одному на каждый диаметр) ----------
    struct Ring { int d; SDL_Texture* core; SDL_Texture* glow; };
    std::vector<Ring> g_rings;
    SDL_Renderer* g_ringOwner = nullptr;

    std::string AvatarPath(const std::string& login) {
        static const char* k = "0123456789abcdef";
        std::string hex;
        for (char c : login) {                  // имя файла: hex логина без учёта регистра
            const unsigned char u = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(c)));
            hex.push_back(k[u >> 4]);
            hex.push_back(k[u & 15]);
        }
        char* p = SDL_GetPrefPath("myproject", "login");
        std::string s = p ? p : "";
        SDL_free(p);
        return s.empty() ? s : s + "avatar_" + hex + ".bmp";
    }

    // умножает альфу на круг с мягким краем (поверхность RGBA32: байты R, G, B, A)
    void ApplyCircle(SDL_Surface* s) {
        const float c = s->w * 0.5f, rad = s->w * 0.5f;
        for (int y = 0; y < s->h; ++y) {
            Uint8* row = static_cast<Uint8*>(s->pixels) + size_t(y) * s->pitch;
            for (int x = 0; x < s->w; ++x) {
                const float dx = x + 0.5f - c, dy = y + 0.5f - c;
                const float k = std::clamp(rad - std::sqrt(dx * dx + dy * dy) + 0.5f, 0.f, 1.f);
                row[x * 4 + 3] = Uint8(row[x * 4 + 3] * k);
            }
        }
    }

    // любая картинка -> квадрат 256x256 RGBA32 (центральная часть, сжатие по шагам вдвое
    // для приличного качества). Исходную поверхность не трогает.
    SDL_Surface* MakeSquare(SDL_Surface* src) {
        SDL_Surface* cur = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGBA32);
        if (!cur) return nullptr;

        const int side = std::min(cur->w, cur->h);
        if (side < 1) { SDL_DestroySurface(cur); return nullptr; }

        if (cur->w != cur->h) {                  // обрезка по центру до квадрата
            SDL_Surface* sq = SDL_CreateSurface(side, side, SDL_PIXELFORMAT_RGBA32);
            if (!sq) { SDL_DestroySurface(cur); return nullptr; }
            const SDL_Rect rc = { (cur->w - side) / 2, (cur->h - side) / 2, side, side };
            SDL_SetSurfaceBlendMode(cur, SDL_BLENDMODE_NONE);   // копировать как есть, вместе с альфой
            SDL_BlitSurface(cur, &rc, sq, nullptr);
            SDL_DestroySurface(cur);
            cur = sq;
        }

        while (cur->w > kBig) {
            const int next = std::max(kBig, cur->w / 2);
            SDL_Surface* sc = SDL_ScaleSurface(cur, next, next, SDL_SCALEMODE_LINEAR);
            SDL_DestroySurface(cur);
            if (!sc) return nullptr;
            cur = sc;
        }
        if (cur->w < kBig) {                     // маленькая картинка: увеличиваем
            SDL_Surface* sc = SDL_ScaleSurface(cur, kBig, kBig, SDL_SCALEMODE_LINEAR);
            SDL_DestroySurface(cur);
            if (!sc) return nullptr;
            cur = sc;
        }
        return cur;
    }

    void FreeTextures() {
        if (g_bigTex) SDL_DestroyTexture(g_bigTex);
        if (g_smallTex) SDL_DestroyTexture(g_smallTex);
        g_bigTex = g_smallTex = nullptr;
    }

    void FreeSurfaces() {
        FreeTextures();
        if (g_big) SDL_DestroySurface(g_big);
        if (g_small) SDL_DestroySurface(g_small);
        g_big = g_small = nullptr;
    }

    // sq: готовый квадрат 256x256 без маски (им владеет вызывающий)
    bool Install(SDL_Surface* sq) {
        SDL_Surface* big = SDL_DuplicateSurface(sq);
        SDL_Surface* small = SDL_ScaleSurface(sq, kSmall, kSmall, SDL_SCALEMODE_LINEAR);
        if (!big || !small) {
            if (big) SDL_DestroySurface(big);
            if (small) SDL_DestroySurface(small);
            return false;
        }
        ApplyCircle(big);
        ApplyCircle(small);
        FreeSurfaces();
        g_big = big;
        g_small = small;
        return true;
    }

    void EnsureTextures(SDL_Renderer* r) {
        if (g_texOwner != r) { FreeTextures(); g_texOwner = r; }
        if (g_big && !g_bigTex) {
            g_bigTex = SDL_CreateTextureFromSurface(r, g_big);
            if (g_bigTex) SDL_SetTextureBlendMode(g_bigTex, SDL_BLENDMODE_BLEND);
        }
        if (g_small && !g_smallTex) {
            g_smallTex = SDL_CreateTextureFromSurface(r, g_small);
            if (g_smallTex) SDL_SetTextureBlendMode(g_smallTex, SDL_BLENDMODE_BLEND);
        }
    }

    Uint32 Pack(Uint8 a) { return (Uint32(a) << 24) | 0x00FFFFFFu; }   // белая маска; цвет задаётся SDL_SetTextureColorMod (меняется со стилем)

    void FreeRings() {
        for (Ring& g : g_rings) {
            if (g.core) SDL_DestroyTexture(g.core);
            if (g.glow) SDL_DestroyTexture(g.glow);
        }
        g_rings.clear();
    }

    Ring& GetRing(SDL_Renderer* r, int d) {
        if (g_ringOwner != r) { FreeRings(); g_ringOwner = r; }
        for (Ring& g : g_rings) if (g.d == d) return g;

        const int sz = d + kPad * 2;
        const float c = sz * 0.5f, rad = d * 0.5f;
        std::vector<Uint32> core(size_t(sz) * sz, 0), glow(size_t(sz) * sz, 0);

        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                const float dx = x + 0.5f - c, dy = y + 0.5f - c;
                const float dist = std::sqrt(dx * dx + dy * dy) - rad;       // расстояние до окружности

                const float line = std::clamp(0.5f - (std::fabs(dist) - 0.75f), 0.f, 1.f);   // контур 1.5 px
                if (line > 0.f) core[size_t(y) * sz + x] = Pack(Uint8(line * 255));

                const float k = dist > 0.f ? 1.f : 0.5f;                     // снаружи сильнее
                const float g = 0.30f * k * std::exp(-std::fabs(dist) / 3.0f);
                if (g > 0.004f) glow[size_t(y) * sz + x] = Pack(Uint8(g * 255));
            }
        }

        auto make = [&](std::vector<Uint32>& buf) {
            SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, sz, sz);
            SDL_UpdateTexture(t, nullptr, buf.data(), sz * 4);
            SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
            return t;
            };
        g_rings.push_back({ d, make(core), make(glow) });
        return g_rings.back();
    }
}

bool Avatar_Load(const std::string& login) {
    Avatar_Clear();
    const std::string path = AvatarPath(login);
    if (path.empty()) return false;

    SDL_Surface* src = SDL_LoadBMP(path.c_str());   // файла может не быть: это нормально
    if (!src) return false;
    SDL_Surface* sq = MakeSquare(src);
    SDL_DestroySurface(src);
    if (!sq) return false;

    const bool ok = Install(sq);
    SDL_DestroySurface(sq);
    return ok;
}

bool Avatar_SetFromFile(const std::string& login, const std::string& imagePath) {
    SDL_Surface* src = SDL_LoadSurface(imagePath.c_str());   // PNG или BMP
    if (!src) {
        SDL_Log("avatar: cannot load '%s': %s", imagePath.c_str(), SDL_GetError());
        return false;
    }
    SDL_Surface* sq = MakeSquare(src);
    SDL_DestroySurface(src);
    if (!sq) return false;

    const std::string path = AvatarPath(login);
    if (path.empty() || !SDL_SaveBMP(sq, path.c_str()))
        SDL_Log("avatar: cannot save '%s': %s", path.c_str(), SDL_GetError());   // покажем, но после перезапуска пропадёт

    const bool ok = Install(sq);
    SDL_DestroySurface(sq);
    return ok;
}

bool Avatar_Has() { return g_big != nullptr; }

void Avatar_Clear() { FreeSurfaces(); }

void Avatar_Draw(SDL_Renderer* r, float cx, float cy, float d, bool lit) {
    const int di = std::max(8, int(std::lround(d)));
    const float half = di * 0.5f;

    // картинка внутри круга
    if (g_big) {
        EnsureTextures(r);
        SDL_Texture* tex = (di <= 100 && g_smallTex) ? g_smallTex : g_bigTex;
        if (tex) {
            const SDL_FRect dst = { cx - half, cy - half, float(di), float(di) };
            SDL_RenderTexture(r, tex, nullptr, &dst);
        }
    }

    // неоновый контур
    Ring& rg = GetRing(r, di);
    const SDL_FRect ring = { cx - half - kPad, cy - half - kPad, float(di + kPad * 2), float(di + kPad * 2) };
    SDL_SetTextureColorMod(rg.glow, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b);
    SDL_SetTextureColorMod(rg.core, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b);
    SDL_SetTextureAlphaMod(rg.glow, lit ? 255 : 190);
    SDL_RenderTexture(r, rg.glow, nullptr, &ring);
    if (lit) SDL_RenderTexture(r, rg.glow, nullptr, &ring);
    SDL_RenderTexture(r, rg.core, nullptr, &ring);
}

void Avatar_Destroy() {
    FreeSurfaces();
    FreeRings();
    g_texOwner = nullptr;
    g_ringOwner = nullptr;
}

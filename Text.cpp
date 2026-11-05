#include "Text.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <string>

#pragma comment(lib, "Gdi32.lib")

namespace {
struct CachedText { SDL_Texture* texture = nullptr; int w = 0, h = 0; };
std::map<std::string, CachedText> g_cache;
SDL_Renderer* g_renderer = nullptr;

std::wstring ToWide(const std::string& value) {
    if (value.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (n <= 0) return L"?";
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), out.data(), n);
    return out;
}

void DrawRaw(SDL_Renderer* r, const std::string& s, float x, float y,
    int fontHeight, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) {
    if (!r || s.empty()) return;
    if (g_renderer != r) {
        for (auto& item : g_cache) if (item.second.texture) SDL_DestroyTexture(item.second.texture);
        g_cache.clear();
        g_renderer = r;
    }
    const std::string key = s + "\x1f" + std::to_string(fontHeight) + "\x1f" +
        std::to_string(red) + "," + std::to_string(green) + "," + std::to_string(blue);
    auto it = g_cache.find(key);
    if (it == g_cache.end()) {
        const std::wstring wide = ToWide(s);
        HDC dc = CreateCompatibleDC(nullptr);
        if (!dc) return;
        HFONT font = CreateFontW(-std::max(8, fontHeight), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN,
            L"Press Start 2P");
        HGDIOBJ oldFont = SelectObject(dc, font);
        SIZE extent{};
        GetTextExtentPoint32W(dc, wide.c_str(), static_cast<int>(wide.size()), &extent);
        const int tw = static_cast<int>(std::max<LONG>(1, extent.cx + 4));
        const int th = std::max(fontHeight + 5, static_cast<int>(extent.cy) + 4);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = tw;
        info.bmiHeader.biHeight = -th;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!bitmap || !pixels) {
            if (bitmap) DeleteObject(bitmap);
            SelectObject(dc, oldFont); DeleteObject(font); DeleteDC(dc); return;
        }
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        std::memset(pixels, 0, static_cast<size_t>(tw) * th * 4);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        RECT rect{ 0, 0, tw, th };
        DrawTextW(dc, wide.c_str(), static_cast<int>(wide.size()), &rect,
            DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        auto* bytes = static_cast<Uint8*>(pixels);
        for (int i = 0; i < tw * th; ++i) {
            const Uint8 coverage = bytes[i * 4];
            bytes[i * 4 + 0] = blue;
            bytes[i * 4 + 1] = green;
            bytes[i * 4 + 2] = red;
            bytes[i * 4 + 3] = coverage;
        }
        SDL_Texture* texture = SDL_CreateTexture(r, SDL_PIXELFORMAT_BGRA32,
            SDL_TEXTUREACCESS_STATIC, tw, th);
        if (texture) {
            SDL_UpdateTexture(texture, nullptr, pixels, tw * 4);
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        }
        SelectObject(dc, oldBitmap); DeleteObject(bitmap);
        SelectObject(dc, oldFont); DeleteObject(font); DeleteDC(dc);
        if (!texture) return;
        it = g_cache.emplace(key, CachedText{ texture, tw, th }).first;
    }
    SDL_SetTextureAlphaMod(it->second.texture, alpha);
    SDL_FRect dst{ x, y, static_cast<float>(it->second.w), static_cast<float>(it->second.h) };
    SDL_RenderTexture(r, it->second.texture, nullptr, &dst);
}
}

int NormalizeTextFontHeight(int fontHeight) {
    // Press Start 2P is a bitmap-style font; its cleanest sizes are multiples of 8 px.
    return std::max(8, (std::max(8, fontHeight) / 8) * 8);
}

float Utf8TextWidth(const std::string& text, int fontHeight) {
    // Press Start 2P is monospaced: one character cell is one em wide.
    return static_cast<float>(Utf8Count(text)) *
        static_cast<float>(NormalizeTextFontHeight(fontHeight));
}

void DrawUtf8Text(SDL_Renderer* renderer, const std::string& text, float x, float y,
    int fontHeight, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) {
    DrawRaw(renderer, text, x, y, NormalizeTextFontHeight(fontHeight), red, green, blue, alpha);
}

size_t Utf8Count(const std::string& text) {
    size_t count = 0;
    for (unsigned char c : text) if ((c & 0xC0) != 0x80) ++count;
    return count;
}

void Utf8PopBack(std::string& text) {
    if (text.empty()) return;
    size_t pos = text.size() - 1;
    while (pos > 0 && (static_cast<unsigned char>(text[pos]) & 0xC0) == 0x80) --pos;
    text.erase(pos);
}

void Utf8AppendLimited(std::string& target, const char* input, size_t maxCharacters) {
    if (!input || !*input) return;
    const size_t current = Utf8Count(target);
    size_t allowed = current < maxCharacters ? maxCharacters - current : 0;
    const char* p = input;
    while (*p && allowed) {
        const unsigned char c = static_cast<unsigned char>(*p);
        size_t bytes = c < 0x80 ? 1 : ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : ((c & 0xF8) == 0xF0 ? 4 : 1)));
        bool valid = true;
        for (size_t i = 1; i < bytes; ++i) if (p[i] == '\0' || (static_cast<unsigned char>(p[i]) & 0xC0) != 0x80) valid = false;
        if (!valid) bytes = 1;
        target.append(p, bytes);
        p += bytes;
        --allowed;
    }
}

void DrawCenteredText(SDL_Renderer* r, const char* text, int w, int h,
    float scale, float yOffset, Uint8 red, Uint8 green, Uint8 blue) {
    const std::string value = text ? text : "";
    const int fontHeight = NormalizeTextFontHeight(std::max(8, static_cast<int>(8.0f * scale)));
    const float approxWidth = Utf8TextWidth(value, fontHeight);
    DrawUtf8Text(r, value, (static_cast<float>(w) - approxWidth) * 0.5f,
        (static_cast<float>(h) - fontHeight) * 0.5f + yOffset,
        fontHeight, red, green, blue, 255);
}

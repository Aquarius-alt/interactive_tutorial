#include "Text.h"
#include "Theme.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cmath>
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

// Helvetica в Windows подменяется на Arial (так задано в FontSubstitutes), кириллица там есть.
// Размер Helvetica берём чуть больше размера ячейки Press Start 2P: его буквы заметно мельче.
int HelveticaPx(int fontHeight) { return std::max(11, (fontHeight * 9 + 4) / 8); }

HFONT MakeFont(int fontHeight, bool helv) {
    if (helv)   // любой пропорциональный шрифт из TextDefaults::kFontFaces
        return CreateFontW(-HelveticaPx(fontHeight), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, VARIABLE_PITCH | FF_SWISS,
            TextDefaults::FontFace());
    return CreateFontW(-std::max(8, fontHeight), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN,
        L"Press Start 2P");
}

// Ширина строки в пикселях для пропорционального шрифта (с кэшем, GDI дорогой).
float MeasureHelvetica(const std::string& text, int fontHeight) {
    if (text.empty()) return 0.f;
    static std::map<std::string, float> cache;
    const std::string key = std::to_string(TextDefaults::FontId) + "\x1e" + std::to_string(fontHeight) + "\x1f" + text;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    float width = 0.f;
    HDC dc = CreateCompatibleDC(nullptr);
    if (dc) {
        HFONT font = MakeFont(fontHeight, true);
        HGDIOBJ old = SelectObject(dc, font);
        const std::wstring wide = ToWide(text);
        SIZE extent{};
        GetTextExtentPoint32W(dc, wide.c_str(), static_cast<int>(wide.size()), &extent);
        width = static_cast<float>(extent.cx);
        SelectObject(dc, old); DeleteObject(font); DeleteDC(dc);
    }
    if (cache.size() > 4000) cache.clear();
    cache.emplace(key, width);
    return width;
}

void DrawRaw(SDL_Renderer* r, const std::string& s, float x, float y,
    int fontHeight, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) {
    if (!r || s.empty()) return;
    if (g_renderer != r) {
        for (auto& item : g_cache) if (item.second.texture) SDL_DestroyTexture(item.second.texture);
        g_cache.clear();
        g_renderer = r;
    }
    const bool helv = TextDefaults::Helvetica;
    const std::string key = std::to_string(TextDefaults::FontId) + "\x1e" + s + "\x1f" + std::to_string(fontHeight) + "\x1f" +
        std::to_string(red) + "," + std::to_string(green) + "," + std::to_string(blue);
    auto it = g_cache.find(key);
    if (it == g_cache.end()) {
        const std::wstring wide = ToWide(s);
        HDC dc = CreateCompatibleDC(nullptr);
        if (!dc) return;
        HFONT font = MakeFont(fontHeight, helv);
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
    SDL_Texture* tex = it->second.texture;
    const float tw = static_cast<float>(it->second.w), th = static_cast<float>(it->second.h);
    if (Theme::Cute() && alpha > 40) {   // PRO CUTE: мягкая сливовая тень под буквами - читаются на любых обоях
        SDL_SetTextureColorMod(tex, 40, 12, 52);
        SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(alpha * 0.55f));
        const SDL_FRect shadow{ x + 1.f, y + (fontHeight >= 16 ? 2.f : 1.f), tw, th };
        SDL_RenderTexture(r, tex, nullptr, &shadow);
        SDL_SetTextureColorMod(tex, 255, 255, 255);
    }
    SDL_SetTextureAlphaMod(tex, alpha);
    SDL_FRect dst{ x, y, tw, th };
    SDL_RenderTexture(r, tex, nullptr, &dst);
}
}

int NormalizeTextFontHeight(int fontHeight) {
    // Press Start 2P is a bitmap-style font; its cleanest sizes are multiples of 8 px.
    return std::max(8, (std::max(8, fontHeight) / 8) * 8);
}

float Utf8TextWidth(const std::string& text, int fontHeight) {
    if (TextDefaults::Helvetica) return MeasureHelvetica(text, NormalizeTextFontHeight(fontHeight));
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

int FitFontHeight(const std::string& text, float maxWidth, int preferred) {
    int font = NormalizeTextFontHeight(preferred);
    while (font > 8 && Utf8TextWidth(text, font) > maxWidth) font -= 8;
    return font;
}

std::string Utf8Fit(const std::string& text, float maxWidth, int fontHeight) {
    if (TextDefaults::Helvetica) {
        if (Utf8TextWidth(text, fontHeight) <= maxWidth) return text;
        std::string cut = text;
        while (!cut.empty() && Utf8TextWidth(cut + "..", fontHeight) > maxWidth) Utf8PopBack(cut);
        return cut + "..";
    }
    const float cell = static_cast<float>(NormalizeTextFontHeight(fontHeight));
    const size_t fit = static_cast<size_t>(std::max(1.f, std::floor(maxWidth / cell)));
    const size_t count = Utf8Count(text);
    if (count <= fit) return text;
    const size_t keep = fit > 2 ? fit - 2 : fit;
    size_t pos = 0, left = keep;
    while (pos < text.size() && left) {
        const unsigned char c = static_cast<unsigned char>(text[pos]);
        pos += c < 0x80 ? 1 : ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : 4));
        --left;
    }
    std::string out = text.substr(0, std::min(pos, text.size()));
    if (fit > 2) out += "..";
    return out;
}

void DrawUtf8TextFit(SDL_Renderer* r, const std::string& text, float boxX, float centerY, float boxW,
    int preferredFont, bool centered, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) {
    if (text.empty()) return;
    const float avail = std::max(8.f, boxW);
    const int font = FitFontHeight(text, avail, preferredFont);
    const std::string shown = Utf8Fit(text, avail, font);
    const float width = Utf8TextWidth(shown, font);
    const float x = centered ? boxX + (boxW - width) * 0.5f : boxX;
    DrawUtf8Text(r, shown, x, centerY - font * 0.5f, font, red, green, blue, alpha);
}

bool IsPasteEvent(const SDL_Event& e) {
    if (e.type != SDL_EVENT_KEY_DOWN) return false;
    // scancode, not keycode: with a Russian layout the V key produces a Cyrillic keycode
    if (e.key.scancode == SDL_SCANCODE_V && (e.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI))) return true;
    if (e.key.scancode == SDL_SCANCODE_INSERT && (e.key.mod & SDL_KMOD_SHIFT)) return true;
    return false;
}

bool IsCopyEvent(const SDL_Event& e) {
    if (e.type != SDL_EVENT_KEY_DOWN) return false;
    if (e.key.scancode == SDL_SCANCODE_C && (e.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI))) return true;
    if (e.key.scancode == SDL_SCANCODE_INSERT && (e.key.mod & SDL_KMOD_CTRL)) return true;
    return false;
}

bool IsUndoEvent(const SDL_Event& e) {
    return e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_Z &&
        (e.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) && !(e.key.mod & SDL_KMOD_SHIFT);
}

bool IsEditShortcut(const SDL_Event& e) {
    return IsPasteEvent(e) || IsCopyEvent(e) || IsUndoEvent(e);
}

void CopyText(const std::string& text) {
    if (!text.empty()) SDL_SetClipboardText(text.c_str());
}

void UndoBefore(UndoHistory& h, const std::string& text, int kind) {
    const Uint64 now = SDL_GetTicks();
    if (text != h.last) {                         // text was changed from outside (e.g. Enter -> newline)
        h.stack.push_back(h.last);
        h.stack.push_back(text);
    }
    else if (!((kind == 1 || kind == 2) && kind == h.kind && now - h.time < 1000)) {
        h.stack.push_back(text);                  // new undo step
    }
    h.kind = kind;
    h.time = now;
    if (h.stack.size() > 100) h.stack.erase(h.stack.begin());
}

void UndoCommit(UndoHistory& h, const std::string& text) {
    if (!h.stack.empty() && h.stack.back() == text) h.stack.pop_back();   // nothing actually changed
    h.last = text;
}

bool UndoApply(UndoHistory& h, std::string& text) {
    if (text != h.last) {                         // revert an outside edit first
        text = h.last;
        h.kind = 0;
        return true;
    }
    if (h.stack.empty()) return false;
    text = h.stack.back();
    h.stack.pop_back();
    h.last = text;
    h.kind = 0;
    return true;
}

bool Utf8PasteLimited(std::string& target, size_t maxCharacters, bool multiline) {
    if (!SDL_HasClipboardText()) return true;
    char* raw = SDL_GetClipboardText();
    if (!raw) return true;
    std::string clean;
    clean.reserve(std::strlen(raw));
    for (const char* p = raw; *p; ++p) {
        const char c = *p;
        if (c == '\r') {
            if (p[1] == '\n') continue;          // CRLF -> one line break
            clean.push_back(multiline ? '\n' : ' ');
        }
        else if (c == '\n') clean.push_back(multiline ? '\n' : ' ');
        else if (c == '\t') clean.push_back(' ');
        else if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) continue;   // other control characters
        else clean.push_back(c);
    }
    SDL_free(raw);
    Utf8AppendLimited(target, clean.c_str(), maxCharacters);
    return true;
}

void DrawCenteredText(SDL_Renderer* r, const char* text, int w, int h,
    float scale, float yOffset, Uint8 red, Uint8 green, Uint8 blue) {
    std::string value = text ? text : "";
    const float avail = std::max(16.f, static_cast<float>(w) - 24.f);          // поля по бокам окна
    const int fontHeight = FitFontHeight(value, avail,
        NormalizeTextFontHeight(std::max(8, static_cast<int>(8.0f * scale))));
    value = Utf8Fit(value, avail, fontHeight);
    const float approxWidth = Utf8TextWidth(value, fontHeight);
    DrawUtf8Text(r, value, (static_cast<float>(w) - approxWidth) * 0.5f,
        (static_cast<float>(h) - fontHeight) * 0.5f + yOffset,
        fontHeight, red, green, blue, 255);
}

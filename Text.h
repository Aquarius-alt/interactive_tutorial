#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <cstddef>
#include <vector>

// Цвета текста по умолчанию (когда вызывающий код не передал свои). Меняются при смене стиля (Styles::Apply).
namespace TextDefaults {
    inline Uint8 AccentR = 96, AccentG = 228, AccentB = 232;   // = Theme::Cyan
    inline Uint8 InkR = 200, InkG = 240, InkB = 255;           // обычный светлый текст
    // Шрифты интерфейса. 0 = Press Start 2P (моноширинный, пиксельный), остальные - обычные пропорциональные
    // шрифты Windows (кириллица есть у всех). Если шрифта нет в системе, Windows подставит похожий.
    inline const wchar_t* const kFontFaces[] = {
        L"Press Start 2P", L"Helvetica", L"Segoe UI", L"Verdana", L"Tahoma",
        L"Trebuchet MS", L"Georgia", L"Times New Roman", L"Consolas", L"Courier New" };
    inline const char* const kFontNames[] = {
        "PIXEL", "HELVETICA", "SEGOE UI", "VERDANA", "TAHOMA",
        "TREBUCHET", "GEORGIA", "TIMES", "CONSOLAS", "COURIER" };
    constexpr int kFontCount = int(sizeof(kFontNames) / sizeof(kFontNames[0]));
    inline int FontId = 0;           // номер текущего шрифта (его задаёт стиль, Styles::Apply)
    inline bool Helvetica = false;   // true = любой пропорциональный шрифт (FontId != 0), false = Press Start 2P
    inline const wchar_t* FontFace() { return kFontFaces[(FontId >= 0 && FontId < kFontCount) ? FontId : 0]; }
}

void DrawCenteredText(SDL_Renderer* r, const char* text, int w, int h,
    float scale = 3.0f, float yOffset = 0.0f,
    Uint8 red = TextDefaults::AccentR, Uint8 green = TextDefaults::AccentG, Uint8 blue = TextDefaults::AccentB);

// UTF-8 helpers shared by all editable text controls.
void DrawUtf8Text(SDL_Renderer* renderer, const std::string& text,
    float x, float y, int fontHeight,
    Uint8 red = TextDefaults::InkR, Uint8 green = TextDefaults::InkG, Uint8 blue = TextDefaults::InkB, Uint8 alpha = 255);
int NormalizeTextFontHeight(int fontHeight);
float Utf8TextWidth(const std::string& text, int fontHeight);
size_t Utf8Count(const std::string& text);
void Utf8PopBack(std::string& text);
void Utf8AppendLimited(std::string& target, const char* input, size_t maxCharacters);

// ---- Fitting text into a given width (localized strings can be much longer) ----
// Largest font height (multiple of 8, <= preferred, >= 8) at which text fits maxWidth.
int FitFontHeight(const std::string& text, float maxWidth, int preferred);
// Cuts text and adds ".." so it fits maxWidth at fontHeight (text unchanged if it already fits).
std::string Utf8Fit(const std::string& text, float maxWidth, int fontHeight);
// Draws text inside a box of width boxW starting at boxX: shrinks the font first, then cuts with "..".
// centered = true centers the text in the box; y is the vertical CENTER of the line.
void DrawUtf8TextFit(SDL_Renderer* r, const std::string& text, float boxX, float centerY, float boxW,
    int preferredFont, bool centered, Uint8 red = TextDefaults::InkR, Uint8 green = TextDefaults::InkG, Uint8 blue = TextDefaults::InkB, Uint8 alpha = 255);

// Clipboard paste: Ctrl+V / Cmd+V / Shift+Insert (works on any keyboard layout, e.g. Russian).
bool IsPasteEvent(const SDL_Event& e);
// Appends clipboard text to target, up to maxCharacters in total.
// multiline = false: line breaks and tabs become spaces (for single-line fields).
// Returns true if the event was a paste shortcut (even if the clipboard was empty).
bool Utf8PasteLimited(std::string& target, size_t maxCharacters, bool multiline);

// Ctrl+C / Cmd+C / Ctrl+Insert and Ctrl+Z / Cmd+Z (scancode based: any keyboard layout).
bool IsCopyEvent(const SDL_Event& e);
bool IsUndoEvent(const SDL_Event& e);
bool IsEditShortcut(const SDL_Event& e);      // paste, copy or undo
void CopyText(const std::string& text);       // puts text on the clipboard (ignored if empty)

// Undo history for one text control.
struct UndoHistory {
    std::vector<std::string> stack;
    std::string last;       // text right after our last change (to notice outside edits)
    int kind = 0;           // 1 = typing, 2 = deleting, 3 = other (paste); typing/deleting runs merge
    Uint64 time = 0;
};
void UndoBefore(UndoHistory& h, const std::string& text, int kind);   // call BEFORE changing text
void UndoCommit(UndoHistory& h, const std::string& text);             // call AFTER changing text
bool UndoApply(UndoHistory& h, std::string& text);                    // Ctrl+Z; false = nothing to undo

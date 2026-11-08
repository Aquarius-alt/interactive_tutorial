#include "TextField.h"
#include "Text.h"
#include "Theme.h"
#include <algorithm>

void TextField_Draw(SDL_Renderer* r, TextField& f) {
    // рамка: при фокусе или наведении светится ярче
    f.box.x = f.x; f.box.y = f.y; f.box.w = f.w; f.box.h = f.h;
    f.box.label = "";
    f.box.hovered = f.focused || f.hovered;
    TextButton_Draw(r, f.box);

    const float padX = 12.f;
    const int fontHeight = std::max(10, static_cast<int>(f.h * 0.42f));
    std::string shown = f.password ? std::string(Utf8Count(f.text), '*') : f.text;
    const size_t maxVis = static_cast<size_t>(std::max(1, int((f.w - padX * 2) / NormalizeTextFontHeight(fontHeight))));
    if (Utf8Count(shown) > maxVis) {
        size_t pos = 0, skip = Utf8Count(shown) - maxVis;
        while (pos < shown.size() && skip) {
            const unsigned char c = static_cast<unsigned char>(shown[pos]);
            pos += c < 0x80 ? 1 : ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : 4));
            --skip;
        }
        shown.erase(0, pos);
    }
    const float tx = f.x + padX;
    const float ty = f.y + (f.h - fontHeight) * 0.5f;
    if (f.text.empty()) DrawUtf8TextFit(r, f.placeholder, tx, f.y + f.h * 0.5f, f.w - padX * 2.f,
        fontHeight, false, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 90);          // подсказка не вылезает за рамку при любом языке
    else DrawUtf8Text(r, shown, tx, ty, fontHeight, Theme::Ink.r, Theme::Ink.g, Theme::Ink.b, 255);

    if (f.focused && (SDL_GetTicks() / 500) % 2 == 0) {
        const float cursorX = tx + Utf8TextWidth(shown, fontHeight);
        DrawUtf8Text(r, "_", cursorX, ty, fontHeight, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 255);
    }
}

static bool Inside(const TextField& f, float mx, float my) {
    return mx >= f.x && mx <= f.x + f.w && my >= f.y && my <= f.y + f.h;
}

bool TextField_Handle(TextField& f, const SDL_Event& e) {
    if (e.type == SDL_EVENT_MOUSE_MOTION)
        f.hovered = Inside(f, e.motion.x, e.motion.y);
    if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE)
        f.hovered = false;
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
        return Inside(f, e.button.x, e.button.y);
    return false;
}

void TextField_Input(TextField& f, const SDL_Event& e) {
    if (!f.focused) return;
    if (e.type == SDL_EVENT_TEXT_INPUT) {
        UndoBefore(f.undo, f.text, 1);
        Utf8AppendLimited(f.text, e.text.text, f.maxLen);
        UndoCommit(f.undo, f.text);
    }
    else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_BACKSPACE) {
        UndoBefore(f.undo, f.text, 2);
        Utf8PopBack(f.text);
        UndoCommit(f.undo, f.text);
    }
    else if (IsPasteEvent(e)) {
        UndoBefore(f.undo, f.text, 3);
        Utf8PasteLimited(f.text, f.maxLen, false);
        UndoCommit(f.undo, f.text);
    }
    else if (IsCopyEvent(e)) {
        if (!f.password) CopyText(f.text);          // из поля пароля не копируем
    }
    else if (IsUndoEvent(e)) {
        UndoApply(f.undo, f.text);
    }
}

SDL_Rect TextField_Rect(const TextField& f) {
    return { int(f.x), int(f.y), int(f.w), int(f.h) };
}

void TextField_Destroy(TextField& f) {
    TextButton_Destroy(f.box);
}
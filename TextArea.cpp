#include "TextArea.h"
#include "Text.h"
#include <algorithm>
#include <vector>

namespace {
    const Uint8 kR = 90, kG = 200, kB = 255;      // цвет неона, как у кнопок
    const float kLineH = 20.f;                    // шаг строк (буквы 16 px при масштабе 2)

    // разбивает текст на строки шириной не больше cols символов (по возможности по словам)
    std::vector<std::string> Wrap(const std::string& s, size_t cols) {
        std::vector<std::string> lines(1);
        size_t i = 0, column = 0;
        while (i < s.size()) {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            size_t bytes = c < 0x80 ? 1 : ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : 4));
            if (i + bytes > s.size()) bytes = 1;
            const std::string ch = s.substr(i, bytes);
            i += bytes;
            if (ch == "\n") { lines.emplace_back(); column = 0; continue; }
            if (column >= cols) { lines.emplace_back(); column = 0; }
            lines.back() += ch;
            ++column;
        }
        if (column >= cols) lines.emplace_back();
        return lines;
    }

    bool Inside(const TextArea& a, float mx, float my) {
        return mx >= a.x && mx <= a.x + a.w && my >= a.y && my <= a.y + a.h;
    }
}

void TextArea_Draw(SDL_Renderer* r, TextArea& a) {
    a.box.x = a.x; a.box.y = a.y; a.box.w = a.w; a.box.h = a.h;
    a.box.label = "";
    a.box.hovered = a.focused || a.hovered;
    TextButton_Draw(r, a.box);

    const int fontHeight = std::max(10, static_cast<int>(16.0f));
    const float padX = 12.f, padY = 10.f, lineH = 20.f;
    const size_t cols = static_cast<size_t>(std::max(1, int((a.w - padX * 2) / NormalizeTextFontHeight(fontHeight))));
    const size_t rows = static_cast<size_t>(std::max(1, int((a.h - padY * 2) / lineH)));
    const std::vector<std::string> lines = Wrap(a.text, cols);
    const size_t maxFirst = lines.size() > rows ? lines.size() - rows : 0;
    a.scroll = std::clamp(a.scroll, 0, static_cast<int>(maxFirst));
    const size_t first = maxFirst - static_cast<size_t>(a.scroll);

    if (a.text.empty()) DrawUtf8TextFit(r, a.placeholder, a.x + padX, a.y + padY + fontHeight * 0.5f, a.w - padX * 2.f,
        fontHeight, false, kR, kG, kB, 90);
    else {
        const size_t last = std::min(lines.size(), first + rows);
        for (size_t i = first; i < last; ++i)
            DrawUtf8Text(r, lines[i], a.x + padX, a.y + padY + static_cast<float>(i - first) * lineH,
                fontHeight, 200, 240, 255, 255);
    }

    if (a.focused && a.scroll == 0 && (SDL_GetTicks() / 500) % 2 == 0) {
        const float cy = a.y + padY + static_cast<float>(lines.size() - 1 - first) * lineH;
        DrawUtf8Text(r, "_", a.x + padX + Utf8TextWidth(lines.back(), fontHeight),
            cy, fontHeight, kR, kG, kB, 255);
    }
    const std::string cnt = std::to_string(Utf8Count(a.text)) + "/" + std::to_string(a.maxLen);
    DrawUtf8Text(r, cnt, a.x + a.w - Utf8TextWidth(cnt, 8), a.y + a.h + 6.f,
        8, kR, kG, kB, 90);
}

bool TextArea_Handle(TextArea& a, const SDL_Event& e) {
    if (e.type == SDL_EVENT_MOUSE_MOTION)
        a.hovered = Inside(a, e.motion.x, e.motion.y);
    if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE)
        a.hovered = false;
    if (e.type == SDL_EVENT_MOUSE_WHEEL && a.hovered)             // колесо вверх = к началу текста
        a.scroll = std::max(0, a.scroll + int(e.wheel.y * 3.f));
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
        return Inside(a, e.button.x, e.button.y);
    return false;
}

void TextArea_Input(TextArea& a, const SDL_Event& e) {
    if (!a.focused) return;
    if (e.type == SDL_EVENT_TEXT_INPUT) {
        UndoBefore(a.undo, a.text, 1);
        Utf8AppendLimited(a.text, e.text.text, a.maxLen);
        UndoCommit(a.undo, a.text);
        a.scroll = 0;
    }
    else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_BACKSPACE) {
        UndoBefore(a.undo, a.text, 2);
        Utf8PopBack(a.text);
        UndoCommit(a.undo, a.text);
        a.scroll = 0;
    }
    else if (IsPasteEvent(e)) {
        UndoBefore(a.undo, a.text, 3);
        Utf8PasteLimited(a.text, a.maxLen, true);
        UndoCommit(a.undo, a.text);
        a.scroll = 0;
    }
    else if (IsCopyEvent(e)) {
        CopyText(a.text);
    }
    else if (IsUndoEvent(e)) {
        if (UndoApply(a.undo, a.text)) a.scroll = 0;
    }
}

SDL_Rect TextArea_Rect(const TextArea& a) {
    return { int(a.x), int(a.y), int(a.w), int(a.h) };
}

void TextArea_Destroy(TextArea& a) {
    TextButton_Destroy(a.box);
}

#include "TextField.h"

void TextField_Draw(SDL_Renderer* r, TextField& f) {
    // рамка: при фокусе или наведении светится ярче
    f.box.x = f.x; f.box.y = f.y; f.box.w = f.w; f.box.h = f.h;
    f.box.label = "";
    f.box.hovered = f.focused || f.hovered;
    TextButton_Draw(r, f.box);

    const float scale = 2.0f;
    const float cs = float(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) * scale;
    const float padX = 12.f;
    const int maxVis = int((f.w - padX * 2) / cs) - 1;      // сколько символов помещается

    std::string shown = f.password ? std::string(f.text.size(), '*') : f.text;
    if (int(shown.size()) > maxVis) shown = shown.substr(shown.size() - maxVis);  // показываем хвост

    const float tx = (f.x + padX) / scale;
    const float ty = (f.y + (f.h - cs) * 0.5f) / scale;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderScale(r, scale, scale);

    if (f.text.empty()) {
        SDL_SetRenderDrawColor(r, 90, 200, 255, 90);        // подсказка бледная
        SDL_RenderDebugText(r, tx, ty, f.placeholder);
    }
    else {
        SDL_SetRenderDrawColor(r, 200, 240, 255, 255);
        SDL_RenderDebugText(r, tx, ty, shown.c_str());
    }

    // мигающий курсор
    if (f.focused && (SDL_GetTicks() / 500) % 2 == 0) {
        SDL_SetRenderDrawColor(r, 90, 200, 255, 255);
        SDL_RenderDebugText(r, tx + shown.size() * (cs / scale), ty, "_");
    }

    SDL_SetRenderScale(r, 1.0f, 1.0f);
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
        for (const char* p = e.text.text; *p; ++p) {
            const unsigned char c = static_cast<unsigned char>(*p);
            // встроенный шрифт SDL умеет только ASCII
            if (c >= 32 && c < 127 && f.text.size() < f.maxLen)
                f.text.push_back(char(c));
        }
    }
    else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_BACKSPACE) {
        if (!f.text.empty()) f.text.pop_back();
    }
}

SDL_Rect TextField_Rect(const TextField& f) {
    return { int(f.x), int(f.y), int(f.w), int(f.h) };
}

void TextField_Destroy(TextField& f) {
    TextButton_Destroy(f.box);
}
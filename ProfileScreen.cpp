#include "ProfileScreen.h"
#include "Avatar.h"
#include "TextButton.h"
#include "Text.h"
#include "Localization.h"
#include <algorithm>
#include <atomic>
#include <mutex>

namespace {
    SDL_Window* g_window = nullptr;
    std::string g_login;
    std::string g_err;

    TextButton g_back{ 0, 0, 110, 40, "BACK" };
    TextButton g_change{ 0, 0, 240, 40, "ADD AVATAR" };

    // файл выбирается в системном диалоге; его колбэк может прийти из другого потока,
    // поэтому путь передаём через мьютекс и разбираем в Profile_Poll() в основном потоке
    std::mutex        g_mutex;
    std::string       g_pending;
    std::atomic<bool> g_hasPending{ false };
    std::atomic<bool> g_dialogOpen{ false };

    void SDLCALL OnFileChosen(void*, const char* const* list, int) {
        if (list && list[0]) {                  // NULL = ошибка, пустой список = отмена
            std::lock_guard<std::mutex> lock(g_mutex);
            g_pending = list[0];
            g_hasPending = true;
        }
        g_dialogOpen = false;
    }

    void ChooseFile() {
        if (g_dialogOpen.exchange(true)) return;   // диалог уже открыт
        static const SDL_DialogFileFilter filtersEn[] = { { "Images (PNG, BMP)", "png;bmp" } };
        static const SDL_DialogFileFilter filtersRu[] = { { "Изображения (PNG, BMP)", "png;bmp" } };
        const SDL_DialogFileFilter* filters = UiRussianFlag() ? filtersRu : filtersEn;
        SDL_ShowOpenFileDialog(OnFileChosen, nullptr, g_window, filters, 1, nullptr, false);
    }
}

void Profile_Open(SDL_Window* window, const std::string& login) {
    g_window = window;
    g_login = login;
    g_err.clear();
    g_back.hovered = g_change.hovered = false;
    g_hasPending = false;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_pending.clear();
}

void Profile_Close() {
    g_login.clear();
    g_err.clear();
}

void Profile_Poll() {
    if (!g_hasPending.exchange(false)) return;

    std::string path;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        path = std::move(g_pending);
        g_pending.clear();
    }
    g_err = Avatar_SetFromFile(g_login, path) ? "" : "CANNOT LOAD IMAGE";
}

void Profile_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount) {
    // заголовок у самой рамки, как "by Aquarius" на главном экране
    const float titleScale = (w >= 520) ? 3.0f : 2.0f;
    const float titleCy = 5.f + float(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) * titleScale * 0.5f;
    DrawCenteredText(r, UiText("PROFILE", "ПРОФИЛЬ"), w, h, titleScale, titleCy - h * 0.5f);

    // BACK слева сверху
    g_back.label = UiText("BACK", "НАЗАД");
    g_back.x = 20.f;
    g_back.y = 16.f + 20.f;
    TextButton_Draw(r, g_back);
    nd[ndCount++] = TextButton_Rect(g_back);

    // крупный аватар по центру, под ним логин и кнопка
    const float d = std::min(160.f, std::max(64.f, float(h) - 290.f));
    const float nameH = 24.f, gap1 = 20.f, gap2 = 24.f, btnH = 40.f;
    const float blockH = d + gap1 + nameH + gap2 + btnH;
    const float top = std::max(80.f, (h - blockH) * 0.5f);
    const float cx = w * 0.5f;

    Avatar_Draw(r, cx, top + d * 0.5f, d, false);

    // логин: крупно, а длинный сжимаем, чтобы влез в окно
    const float cs = float(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
    float nameScale = 3.0f;
    while (nameScale > 1.0f && g_login.size() * cs * nameScale > float(w) - 40.f) nameScale -= 0.5f;
    const float nameCy = top + d + gap1 + nameH * 0.5f;
    DrawCenteredText(r, g_login.c_str(), w, h, nameScale, nameCy - h * 0.5f);

    g_change.w = UiRussianFlag() ? 280.f : 240.f;
    g_change.label = Avatar_Has() ? UiText("CHANGE AVATAR", "СМЕНИТЬ АВАТАР") : UiText("ADD AVATAR", "ДОБАВИТЬ АВАТАР");
    g_change.x = cx - g_change.w * 0.5f;
    g_change.y = top + d + gap1 + nameH + gap2;
    TextButton_Draw(r, g_change);
    nd[ndCount++] = TextButton_Rect(g_change);

    if (!g_err.empty()) {
        const std::string shownError = UiMessage(g_err);
        DrawCenteredText(r, shownError.c_str(), w, h, 2.0f, g_change.y + btnH + 24.f - h * 0.5f, 255, 90, 90);
    }
}

ProfileResult Profile_Handle(const SDL_Event& e) {
    if (TextButton_Handle(g_back, e)) return ProfileResult::Back;
    if (TextButton_Handle(g_change, e)) ChooseFile();

    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_ESCAPE)
        return ProfileResult::Back;

    return ProfileResult::None;
}

void Profile_Destroy() {
    TextButton_Destroy(g_back);
    TextButton_Destroy(g_change);
}

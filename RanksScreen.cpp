#include "RanksScreen.h"
#include "TextButton.h"
#include "TextField.h"
#include "TheoryDialog.h"
#include "Text.h"
#include "Localization.h"
#include "RepoStore.h"
#include "Level.h"
#include "Theme.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace {
    // Пароль на создание задания. Проверка только на стороне программы: от случайного нажатия
    // защищает, от человека с доступом к exe - нет.
    const char kCreatePassword[] = "Serebr200801";

    const Uint8 &kR = Theme::Cyan.r, &kG = Theme::Cyan.g, &kB = Theme::Cyan.b;
    const float kCardW = 150.f, kCardH = 176.f;      // карточка "+", как на главном экране
    const float kPwW = 440.f, kPwH = 230.f;          // окно пароля

    SDL_Window* g_window = nullptr;
    std::string g_login;
    std::string g_err;                               // ошибка открытия папки (под IMPORT)
    std::string g_pwErr;                             // ошибка в окне пароля

    TextButton g_back{ 0, 0, 110, 40, "BACK" };
    TextButton g_import{ 0, 0, 200, 40, "IMPORT" };
    TextButton g_create{ 0, 0, kCardW, kCardH, "" }; // карточка "создать задание"

    // созданные задания: карточки рядом с "+" (подпись - название, звание и теги предмета)
    const int  kMaxCards = 60;
    std::vector<RankTask> g_tasks;
    std::vector<char>     g_done;                  // g_done[i] = ранг за задание i уже получен
    TextButton g_cards[60];

    void ReloadTasks() {
        g_tasks = RepoStore_ListRankTasks(g_login);
        g_done.assign(g_tasks.size(), 0);
        for (size_t i = 0; i < g_tasks.size(); ++i) g_done[i] = Level_HasRank(g_login, g_tasks[i].key) ? 1 : 0;
    }

    // ---------- прохождение задания ----------
    // Окно поверх экрана: вопросы по одному (PREV / NEXT), под вопросом поле ответа, на последнем вопросе FINISH.
    // Задание пройдено, если верны ответы на все вопросы, у которых задан верный ответ. Тогда запись
    // "теги - звание" добавляется в ранги профиля (один раз на задание). Неверно - можно попробовать ещё раз.
    bool        g_testOpen = false;
    int         g_testIdx = -1;                    // номер задания в g_tasks
    std::vector<TestQuestion> g_tq;
    std::vector<std::string>  g_tans;              // ответы по вопросам
    int         g_qi = 0;                          // текущий вопрос
    int         g_stage = 0;                       // 0 = отвечаем, 1 = пройдено, 2 = не пройдено
    int         g_good = 0, g_checked = 0;         // верных / проверяемых
    bool        g_newRank = false;                 // ранг выдан только что
    std::string g_rankLine;                        // "теги - звание"
    std::string g_tErr;
    TextField   g_tField;
    TextButton  g_tPanel{ 0, 0, 100, 100, "" };
    TextButton  g_tClose{ 0, 0, 130, 40, "CLOSE" };
    TextButton  g_tPrev{ 0, 0, 130, 40, "PREV" };
    TextButton  g_tNext{ 0, 0, 130, 40, "NEXT" };
    TextButton  g_tRetry{ 0, 0, 130, 40, "RETRY" };

    // сравнение без учёта регистра и лишних пробелов (как при проверке тестов в репозитории)
    std::wstring NormAnswer(const std::string& in) {
        std::wstring w;
        if (!in.empty()) {
            const int n = MultiByteToWideChar(CP_UTF8, 0, in.data(), int(in.size()), nullptr, 0);
            if (n > 0) {
                w.assign(size_t(n), L'\0');
                MultiByteToWideChar(CP_UTF8, 0, in.data(), int(in.size()), w.data(), n);
            }
        }
        std::wstring o;
        bool sp = false;
        for (wchar_t c : w) {
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') { sp = !o.empty(); continue; }
            if (sp) { o.push_back(L' '); sp = false; }
            o.push_back(wchar_t(std::towlower(c)));
        }
        return o;
    }

    // 1 = верно, 0 = неверно (в том числе пустой ответ), -1 = верный ответ в задании не задан.
    // В строке верных ответов можно перечислить несколько допустимых через ; или |
    int CheckAnswer(const std::string& user, const std::string& correct) {
        const std::wstring c = NormAnswer(correct);
        if (c.empty()) return -1;
        const std::wstring u = NormAnswer(user);
        if (u.empty()) return 0;
        if (u == c) return 1;
        std::wstring part;
        for (size_t i = 0; i <= c.size(); ++i) {
            if (i == c.size() || c[i] == L';' || c[i] == L'|') {
                size_t b = 0, e = part.size();
                while (b < e && part[b] == L' ') ++b;
                while (e > b && part[e - 1] == L' ') --e;
                if (e > b && part.compare(b, e - b, u) == 0) return 1;
                part.clear();
            }
            else part.push_back(c[i]);
        }
        return 0;
    }

    // перенос по словам на строки шириной maxW; \n начинает новую строку, лишнее режется ".."
    std::vector<std::string> WrapText(const std::string& text, float maxW, int fontH, size_t maxLines) {
        std::vector<std::string> lines;
        size_t start = 0;
        while (start <= text.size()) {
            size_t nl = text.find('\n', start);
            if (nl == std::string::npos) nl = text.size();
            const std::string para = text.substr(start, nl - start);
            start = nl + 1;

            std::string cur, word;
            auto flush = [&]() {
                if (word.empty()) return;
                const std::string cand = cur.empty() ? word : cur + " " + word;
                if (cur.empty() || Utf8TextWidth(cand, fontH) <= maxW) cur = cand;
                else { lines.push_back(cur); cur = word; }
                word.clear();
            };
            for (char ch : para) { if (ch == ' ' || ch == '\r') flush(); else word.push_back(ch); }
            flush();
            lines.push_back(cur);                                   // пустая строка тоже сохраняется (абзац)
            if (nl == text.size()) break;
        }
        if (lines.size() > maxLines) {
            lines.resize(maxLines);
            lines.back() = Utf8Fit(lines.back() + "...", maxW, fontH);
        }
        for (std::string& l : lines) l = Utf8Fit(l, maxW, fontH);
        return lines;
    }

    void OpenTest(int idx) {
        const TheoryItem& it = g_tasks[size_t(idx)].item;
        g_tq = it.questions;
        if (g_tq.empty() && (!it.content.empty() || !it.answers.empty())) g_tq.push_back({ it.content, it.answers });
        if (g_tq.empty()) { g_err = "TASK HAS NO QUESTIONS"; return; }
        g_err.clear();
        g_testIdx = idx;
        g_tans.assign(g_tq.size(), std::string());
        g_qi = 0;
        g_stage = 0;
        g_good = g_checked = 0;
        g_newRank = false;
        g_rankLine.clear();
        g_tErr.clear();
        g_tField.text.clear();
        g_tField.maxLen = 200;
        g_tField.focused = true;
        g_tField.hovered = false;
        g_tClose.hovered = g_tPrev.hovered = g_tNext.hovered = g_tRetry.hovered = false;
        g_testOpen = true;
        SDL_StartTextInput(g_window);
    }

    void CloseTest() {
        g_testOpen = false;
        g_tField.focused = false;
        g_tField.text.clear();
        SDL_StopTextInput(g_window);
    }

    void GoQuestion(int n) {
        g_tans[size_t(g_qi)] = g_tField.text;
        g_qi = std::clamp(n, 0, int(g_tq.size()) - 1);
        g_tField.text = g_tans[size_t(g_qi)];
        g_tField.focused = true;
    }

    void Finish() {
        g_tans[size_t(g_qi)] = g_tField.text;
        g_good = g_checked = 0;
        for (size_t i = 0; i < g_tq.size(); ++i) {
            const int s = CheckAnswer(g_tans[i], g_tq[i].answers);
            if (s >= 0) { ++g_checked; if (s == 1) ++g_good; }
        }
        g_tField.focused = false;
        if (g_checked > 0 && g_good == g_checked) {
            const TheoryItem& it = g_tasks[size_t(g_testIdx)].item;
            g_newRank = false;
            g_rankLine.clear();
            if (!it.rank.empty()) {
                g_newRank = Level_AwardRank(g_login, g_tasks[size_t(g_testIdx)].key, it.subjectTags, it.rank);
                g_rankLine = it.subjectTags.empty() ? it.rank : it.subjectTags + " - " + it.rank;
                ReloadTasks();                                   // карточка получит отметку "пройдено"
            }
            g_stage = 1;
        }
        else {
            g_stage = 2;
        }
    }

    void NextClicked() {
        if (g_qi + 1 < int(g_tq.size())) GoQuestion(g_qi + 1);
        else Finish();
    }

    void Retry() {
        g_tans.assign(g_tq.size(), std::string());
        g_qi = 0;
        g_stage = 0;
        g_tField.text.clear();
        g_tField.focused = true;
    }

    bool       g_pwOpen = false;
    TextField  g_pw;
    TextButton g_pwPanel{ 0, 0, kPwW, kPwH, "" };
    TextButton g_pwOk{ 0, 0, 150, 40, "OK" };
    TextButton g_pwCancel{ 0, 0, 150, 40, "CANCEL" };

    void OpenPassword() {
        g_pwOpen = true;
        g_pwErr.clear();
        g_pw.text.clear();
        g_pw.password = true;
        g_pw.maxLen = 32;
        g_pw.w = kPwW - 80.f;
        g_pw.h = 40.f;
        g_pw.focused = true;
        g_pw.hovered = false;
        g_pwOk.hovered = g_pwCancel.hovered = false;
        SDL_StartTextInput(g_window);
    }

    void ClosePassword() {
        g_pwOpen = false;
        g_pw.text.clear();
        g_pw.focused = false;
        g_pwErr.clear();
        SDL_StopTextInput(g_window);
    }

    void SubmitPassword() {
        if (g_pw.text == kCreatePassword) {
            ClosePassword();
            Theory_Open(g_window, true);            // пока "создать задание" = окно создания теста
        }
        else {
            g_pwErr = "WRONG PASSWORD";
            g_pw.text.clear();
        }
    }

    void FillR(SDL_Renderer* r, float x, float y, float w, float h) {
        const SDL_FRect q{ x, y, w, h };
        SDL_RenderFillRect(r, &q);
    }
    // фон окна-панели: в стиле PRO CUTE скруглённый, чтобы не торчать за углами рамки
    void FillPanel(SDL_Renderer* r, float x, float y, float w, float h) {
        const SDL_FRect q{ x, y, w, h };
        Theme::StyledFill(r, &q);
    }
}

void Ranks_Open(SDL_Window* window, const std::string& login) {
    g_window = window;
    g_login = login;
    g_err.clear();
    g_pwOpen = false;
    g_pw.text.clear();
    g_pw.focused = false;
    g_back.hovered = g_import.hovered = g_create.hovered = false;
    g_testOpen = false;
    ReloadTasks();
}

void Ranks_Poll() {
    if (Theory_IsOpen()) Theory_Poll();
}

bool Ranks_IsModalOpen() {
    return g_pwOpen || Theory_IsOpen() || g_testOpen;
}

void Ranks_Draw(SDL_Renderer* r, int w, int h, SDL_Rect* nd, int& ndCount) {
    // заголовок у самой рамки, как на экране профиля
    const float titleScale = (w >= 520) ? 3.0f : 2.0f;
    const float titleCy = 5.f + float(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) * titleScale * 0.5f;
    DrawCenteredText(r, UiText("RANKS", "РАНГИ"), w, h, titleScale, titleCy - h * 0.5f);

    g_back.label = UiText("BACK", "НАЗАД");
    g_back.x = 20.f;
    g_back.y = 16.f + 20.f;
    TextButton_Draw(r, g_back);
    nd[ndCount++] = TextButton_Rect(g_back);

    // IMPORT под кнопкой BACK: открывает папку, куда кладут задание на ранг
    g_import.label = UiText("IMPORT", "ИМПОРТ");
    g_import.x = 20.f;
    g_import.y = g_back.y + 40.f + 20.f;
    TextButton_Draw(r, g_import);
    nd[ndCount++] = TextButton_Rect(g_import);

    if (!g_err.empty())
        DrawUtf8TextFit(r, UiMessage(g_err), 20.f, g_import.y + 40.f + 24.f, float(w) - 40.f, 16, false, 255, 90, 90, 255);

    // карточка "+": создать задание (закрыто паролем)
    g_create.x = 20.f;
    g_create.y = g_import.y + 40.f + 56.f;
    TextButton_Draw(r, g_create);
    nd[ndCount++] = TextButton_Rect(g_create);
    {
        const float cx = g_create.x + kCardW * 0.5f, cy = g_create.y + kCardH * 0.5f - 12.f;
        const float len = 40.f, th = 4.f;
        const bool lit = g_create.hovered;
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, kR, kG, kB, lit ? 255 : 200);
        FillR(r, cx - len * 0.5f, cy - th * 0.5f, len, th);
        FillR(r, cx - th * 0.5f, cy - len * 0.5f, th, len);
        DrawUtf8TextFit(r, UiText("CREATE TASK", "СОЗДАТЬ ЗАДАНИЕ"), g_create.x + 8.f,
            g_create.y + kCardH - 28.f, kCardW - 16.f, 16, true, Theme::InkDim.r, Theme::InkDim.g, Theme::InkDim.b, 255);
    }

    // карточки созданных заданий: сетка справа от "+", переносится по ширине окна
    {
        const float gap = 16.f;
        const int cols = std::max(1, int(std::floor((float(w) - 40.f + gap) / (kCardW + gap))));
        const int shown = std::min<int>(int(g_tasks.size()), kMaxCards);
        for (int i = 0; i < shown; ++i) {
            const int slot = i + 1;                                   // слот 0 занимает "+"
            const float cx0 = 20.f + float(slot % cols) * (kCardW + gap);
            const float cy0 = g_create.y + float(slot / cols) * (kCardH + gap);
            TextButton& c = g_cards[i];
            c.x = cx0; c.y = cy0; c.w = kCardW; c.h = kCardH;
            TextButton_Draw(r, c);
            nd[ndCount++] = TextButton_Rect(c);

            const TheoryItem& t = g_tasks[size_t(i)].item;
            const float tx = cx0 + 8.f, tw = kCardW - 16.f;
            DrawUtf8TextFit(r, t.title, tx, cy0 + 34.f, tw, 16, true, kR, kG, kB, 255);
            const std::string rank = t.rank.empty() ? std::string(UiText("NO RANK", "БЕЗ ЗВАНИЯ")) : t.rank;
            DrawUtf8TextFit(r, rank, tx, cy0 + 84.f, tw, 16, true, Theme::Ink.r, Theme::Ink.g, Theme::Ink.b, t.rank.empty() ? 110 : 255);
            if (!t.subjectTags.empty())
                DrawUtf8TextFit(r, t.subjectTags, tx, cy0 + 122.f, tw, 16, true, Theme::InkDim.r, Theme::InkDim.g, Theme::InkDim.b, 255);
            if (g_done[size_t(i)])
                DrawUtf8TextFit(r, UiText("DONE", "ПРОЙДЕНО"), tx, cy0 + kCardH - 22.f, tw, 16, true, 120, 220, 140, 255);
            else
                DrawUtf8TextFit(r, std::to_string(t.questions.size()) + UiText(" Q", " ВОПР."),
                    tx, cy0 + kCardH - 22.f, tw, 8, true, 110, 140, 160, 255);
        }
    }

    // окно пароля поверх экрана
    if (g_pwOpen) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 175);
        SDL_RenderFillRect(r, nullptr);

        const float px = std::floor((float(w) - kPwW) * 0.5f), py = std::floor((float(h) - kPwH) * 0.5f);
        SDL_SetRenderDrawColor(r, Theme::Panel.r, Theme::Panel.g, Theme::Panel.b, 255);
        FillPanel(r, px + 4.f, py + 4.f, kPwW - 8.f, kPwH - 8.f);
        g_pwPanel.x = px; g_pwPanel.y = py; g_pwPanel.hovered = false;
        TextButton_Draw(r, g_pwPanel);
        nd[ndCount++] = TextButton_Rect(g_pwPanel);

        DrawUtf8TextFit(r, UiText("ENTER PASSWORD", "ВВЕДИТЕ ПАРОЛЬ"), px + 20.f, py + 36.f, kPwW - 40.f, 24, true, kR, kG, kB, 255);

        g_pw.placeholder = UiText("PASSWORD", "ПАРОЛЬ");
        g_pw.x = px + 40.f;
        g_pw.y = py + 66.f;
        TextField_Draw(r, g_pw);

        if (!g_pwErr.empty())
            DrawUtf8TextFit(r, UiMessage(g_pwErr), px + 40.f, py + 124.f, kPwW - 80.f, 16, true, 255, 90, 90, 255);

        g_pwOk.label = "OK";
        g_pwCancel.label = UiText("CANCEL", "ОТМЕНА");
        const float gap = 20.f, bx = px + (kPwW - (g_pwCancel.w + gap + g_pwOk.w)) * 0.5f, by = py + kPwH - 40.f - 24.f;
        g_pwCancel.x = bx;                         g_pwCancel.y = by;
        g_pwOk.x = bx + g_pwCancel.w + gap;        g_pwOk.y = by;
        TextButton_Draw(r, g_pwCancel);
        TextButton_Draw(r, g_pwOk);
    }

    // окно прохождения задания
    if (g_testOpen && g_testIdx >= 0 && g_testIdx < int(g_tasks.size())) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 175);
        SDL_RenderFillRect(r, nullptr);

        const float side = (w >= 420) ? 18.f : 10.f;
        const float px = side, py = 44.f;
        const float pw = std::floor(std::max(200.f, float(w) - 2.f * side));
        const float ph = std::floor(std::max(240.f, float(h) - py - 30.f));
        const float pad = (w >= 420) ? 20.f : 12.f;
        const float ix = px + pad, iw = pw - 2.f * pad;
        const float btnH = 40.f, gap = 12.f;

        SDL_SetRenderDrawColor(r, Theme::Panel.r, Theme::Panel.g, Theme::Panel.b, 255);
        FillPanel(r, px + 4.f, py + 4.f, pw - 8.f, ph - 8.f);
        g_tPanel.x = px; g_tPanel.y = py; g_tPanel.w = pw; g_tPanel.h = ph; g_tPanel.hovered = false;
        TextButton_Draw(r, g_tPanel);
        nd[ndCount++] = TextButton_Rect(g_tPanel);

        const TheoryItem& task = g_tasks[size_t(g_testIdx)].item;
        DrawUtf8TextFit(r, task.title, ix, py + pad + 14.f, iw, 24, false, kR, kG, kB, 255);

        const float rowY = py + ph - pad - btnH;
        float bw = 130.f;
        if (3.f * bw + 2.f * gap > iw) bw = std::max(30.f, std::floor((iw - 2.f * gap) / 3.f));
        g_tClose.label = UiText("CLOSE", "ЗАКРЫТЬ");
        g_tPrev.label = UiText("PREV", "НАЗАД");
        g_tRetry.label = UiText("TRY AGAIN", "ЕЩЁ РАЗ");
        g_tClose.w = g_tPrev.w = g_tNext.w = g_tRetry.w = bw;
        g_tClose.h = g_tPrev.h = g_tNext.h = g_tRetry.h = btnH;
        g_tClose.y = g_tPrev.y = g_tNext.y = g_tRetry.y = rowY;
        g_tClose.x = ix;
        g_tNext.x = g_tRetry.x = ix + iw - bw;
        g_tPrev.x = std::floor(ix + (iw - bw) * 0.5f);

        if (g_stage == 0) {
            const bool last = (g_qi + 1 >= int(g_tq.size()));
            g_tNext.label = last ? UiText("FINISH", "ЗАВЕРШИТЬ") : UiText("NEXT", "ДАЛЕЕ");

            const std::string head = std::string(UiText("QUESTION ", "ВОПРОС ")) + std::to_string(g_qi + 1) + "/" + std::to_string(g_tq.size());
            DrawUtf8Text(r, head, ix, py + pad + 40.f, 16, Theme::InkDim.r, Theme::InkDim.g, Theme::InkDim.b, 255);

            g_tField.placeholder = UiText("ANSWER", "ОТВЕТ");
            g_tField.x = ix; g_tField.w = iw; g_tField.h = 40.f;
            g_tField.y = rowY - 14.f - g_tField.h;

            const float qTop = py + pad + 70.f;
            const float qBottom = g_tField.y - 14.f;
            const int maxLines = std::max(1, int((qBottom - qTop) / 24.f));
            const std::vector<std::string> lines = WrapText(g_tq[size_t(g_qi)].text, iw, 16, size_t(maxLines));
            for (size_t k = 0; k < lines.size(); ++k)
                DrawUtf8Text(r, lines[k], ix, qTop + float(k) * 24.f, 16, Theme::Ink.r, Theme::Ink.g, Theme::Ink.b, 255);

            TextField_Draw(r, g_tField);

            TextButton_Draw(r, g_tClose);
            if (g_qi > 0) TextButton_Draw(r, g_tPrev);
            TextButton_Draw(r, g_tNext);
        }
        else {
            const float cy = py + ph * 0.38f;
            if (g_stage == 1) {
                DrawUtf8TextFit(r, UiText("TASK COMPLETED", "ЗАДАНИЕ ПРОЙДЕНО"), ix, cy, iw, 24, true, 120, 220, 140, 255);
                if (!g_rankLine.empty()) {
                    DrawUtf8TextFit(r, g_newRank ? UiText("RANK RECEIVED", "РАНГ ПОЛУЧЕН") : UiText("RANK ALREADY RECEIVED", "РАНГ УЖЕ ПОЛУЧЕН"),
                        ix, cy + 44.f, iw, 16, true, Theme::InkDim.r, Theme::InkDim.g, Theme::InkDim.b, 255);
                    DrawUtf8TextFit(r, g_rankLine, ix, cy + 84.f, iw, 24, true, kR, kG, kB, 255);
                }
                else {
                    DrawUtf8TextFit(r, UiText("THIS TASK GIVES NO RANK", "ЗА ЭТО ЗАДАНИЕ РАНГ НЕ ПРЕДУСМОТРЕН"),
                        ix, cy + 44.f, iw, 16, true, 110, 140, 160, 255);
                }
            }
            else {
                DrawUtf8TextFit(r, UiText("NOT PASSED", "НЕ ПРОЙДЕНО"), ix, cy, iw, 24, true, 255, 90, 90, 255);
                if (g_checked == 0)
                    DrawUtf8TextFit(r, UiText("NO CORRECT ANSWERS SET IN THIS TASK", "В ЗАДАНИИ НЕ ЗАДАНЫ ВЕРНЫЕ ОТВЕТЫ"),
                        ix, cy + 44.f, iw, 16, true, 110, 140, 160, 255);
                else
                    DrawUtf8TextFit(r, std::string(UiText("CORRECT: ", "ВЕРНО: ")) + std::to_string(g_good) + "/" + std::to_string(g_checked),
                        ix, cy + 44.f, iw, 16, true, Theme::Ink.r, Theme::Ink.g, Theme::Ink.b, 255);
            }
            TextButton_Draw(r, g_tClose);
            if (g_stage == 2 && g_checked > 0) TextButton_Draw(r, g_tRetry);
        }
    }

    // окно создания теста (открывается после верного пароля)
    Theory_Draw(r, w, h, nd, ndCount);
}

RanksResult Ranks_Handle(const SDL_Event& e) {
    // окно прохождения задания: все события только ему
    if (g_testOpen) {
        if (g_stage == 0) {
            if (TextField_Handle(g_tField, e)) g_tField.focused = true;
            TextField_Input(g_tField, e);
            if (TextButton_Handle(g_tClose, e)) CloseTest();
            else if (g_qi > 0 && TextButton_Handle(g_tPrev, e)) GoQuestion(g_qi - 1);
            else if (TextButton_Handle(g_tNext, e)) NextClicked();
            else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                if (e.key.key == SDLK_ESCAPE) CloseTest();
                else if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) NextClicked();
            }
        }
        else {
            if (TextButton_Handle(g_tClose, e)) CloseTest();
            else if (g_stage == 2 && g_checked > 0 && TextButton_Handle(g_tRetry, e)) Retry();
            else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_ESCAPE) CloseTest();
        }
        return RanksResult::None;
    }

    // окно теста: все события только ему; Added = сохранить задание в папку _ranks, Cancel = закрыть без сохранения
    if (Theory_IsOpen()) {
        const TheoryResult tr = Theory_Handle(e);
        if (tr == TheoryResult::Added) {
            const TheoryItem task = Theory_Data();                    // до Theory_Close: он сбрасывает поля
            Theory_Close();
            if (RepoStore_SaveRankTask(g_login, task)) { g_err.clear(); ReloadTasks(); }
            else g_err = "CANNOT SAVE TASK";
        }
        else if (tr == TheoryResult::Cancel) {
            Theory_Close();
        }
        return RanksResult::None;
    }

    // окно пароля
    if (g_pwOpen) {
        if (TextField_Handle(g_pw, e)) g_pw.focused = true;
        TextField_Input(g_pw, e);
        if (TextButton_Handle(g_pwOk, e)) SubmitPassword();
        else if (TextButton_Handle(g_pwCancel, e)) ClosePassword();
        else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
            if (e.key.key == SDLK_ESCAPE) ClosePassword();
            else if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) SubmitPassword();
        }
        return RanksResult::None;
    }

    if (TextButton_Handle(g_back, e)) return RanksResult::Back;
    if (TextButton_Handle(g_import, e)) {
        g_err = RepoStore_OpenRanksFolder(g_login) ? "" : "CANNOT OPEN FOLDER";
        ReloadTasks();                                                // подхватить задания, положенные в папку вручную
    }
    if (TextButton_Handle(g_create, e)) OpenPassword();
    for (int i = 0; i < std::min<int>(int(g_tasks.size()), kMaxCards); ++i)
        if (TextButton_Handle(g_cards[i], e)) { OpenTest(i); break; }       // клик по карточке = пройти задание

    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_ESCAPE)
        return RanksResult::Back;
    return RanksResult::None;
}

void Ranks_Destroy() {
    TextButton_Destroy(g_back);
    TextButton_Destroy(g_import);
    TextButton_Destroy(g_create);
    for (int i = 0; i < kMaxCards; ++i) TextButton_Destroy(g_cards[i]);
    TextButton_Destroy(g_tPanel);
    TextButton_Destroy(g_tClose);
    TextButton_Destroy(g_tPrev);
    TextButton_Destroy(g_tNext);
    TextButton_Destroy(g_tRetry);
    TextField_Destroy(g_tField);
    TextButton_Destroy(g_pwPanel);
    TextButton_Destroy(g_pwOk);
    TextButton_Destroy(g_pwCancel);
    TextField_Destroy(g_pw);
}

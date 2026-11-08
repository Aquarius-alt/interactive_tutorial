#include "ProfileScreen.h"
#include "Avatar.h"
#include "TextButton.h"
#include "Text.h"
#include "Localization.h"
#include "Level.h"
#include "Achievements.h"
#include "RepoStore.h"
#include "Theme.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <mutex>
#include <vector>

namespace {
    SDL_Window* g_window = nullptr;
    std::string g_login;
    std::string g_err;
    LevelInfo   g_level;          // уровень и опыт на текущем уровне (читаются при открытии)
    long long   g_totalXp = 0;

    // уровни по тегам предметов (список справа под основным уровнем)
    struct TagRow { std::string tag; LevelInfo info; };
    std::vector<TagRow> g_tags;
    std::vector<RankEntry> g_rankList;     // полученные ранги ("тег - звание")
    float    g_listScroll = 0.f;          // на сколько прокручен список (пиксели)
    float    g_listMax = 0.f;             // максимум прокрутки (считается при рисовании)
    SDL_FRect g_listRect{ 0, 0, 0, 0 };   // область списка (для колёсика мыши)

    const float kRowH = 76.f, kRowGap = 8.f;

    // достижения: горизонтальная лента справа от основного уровня. Статистика и список считаются
    // при открытии экрана; открытые идут первыми.
    const float kAchCardW = 190.f, kAchCardH = 84.f, kAchGap = 10.f;
    AchStats  g_achStats;
    std::vector<int> g_achOrder;          // номера достижений: сначала открытые
    int       g_achUnlocked = 0;
    float     g_achScroll = 0.f;          // сдвиг ленты по X (пиксели)
    float     g_achMax = 0.f;
    SDL_FRect g_achRect{ 0, 0, 0, 0 };    // область ленты (для колёсика и перетаскивания)
    bool      g_achDrag = false;          // лента тянется мышью

    void LoadAchievements(const std::string& login, long long totalXp, int level) {
        AchStats st;
        st.v[Stat_ReposDone] = Level_CompletedCount(login);
        st.v[Stat_ReposCreated] = (long long)RepoStore_List(login).size();
        st.v[Stat_Level] = level;
        st.v[Stat_TotalXp] = totalXp;
        const std::vector<TagLevel> tags = Level_GetTags(login);
        st.v[Stat_TagCount] = (long long)tags.size();
        for (const TagLevel& t : tags)
            st.v[Stat_TopTagLevel] = std::max<long long>(st.v[Stat_TopTagLevel], Level_FromXp(t.xp).level);
        st.v[Stat_HasAvatar] = Avatar_Has() ? 1 : 0;
        g_achStats = st;

        g_achOrder.clear();
        g_achUnlocked = 0;
        for (int i = 0; i < Ach_Count(); ++i) if (Ach_Unlocked(st, i)) { g_achOrder.push_back(i); ++g_achUnlocked; }
        for (int i = 0; i < Ach_Count(); ++i) if (!Ach_Unlocked(st, i)) g_achOrder.push_back(i);
        g_achScroll = 0.f;
        g_achDrag = false;
    }

    // переносит text по словам на строки шириной maxW (шрифт fontH), не больше maxLines; хвост режется ".."
    std::vector<std::string> WrapText(const std::string& text, float maxW, int fontH, size_t maxLines) {
        std::vector<std::string> lines;
        std::string cur, word;
        auto flush = [&]() {
            if (word.empty()) return;
            const std::string cand = cur.empty() ? word : cur + " " + word;
            if (cur.empty() || Utf8TextWidth(cand, fontH) <= maxW) cur = cand;
            else { lines.push_back(cur); cur = word; }
            word.clear();
        };
        for (char c : text) { if (c == ' ') flush(); else word.push_back(c); }
        flush();
        if (!cur.empty()) lines.push_back(cur);
        if (lines.size() > maxLines) {
            lines.resize(maxLines);
            lines.back() = Utf8Fit(lines.back() + "...", maxW, fontH);
        }
        for (std::string& l : lines) l = Utf8Fit(l, maxW, fontH);
        return lines;
    }

    void LoadTags(const std::string& login) {
        g_tags.clear();
        for (const TagLevel& t : Level_GetTags(login))
            g_tags.push_back({ t.tag, Level_FromXp(t.xp) });
        g_listScroll = 0.f;
    }

    void FillRect(SDL_Renderer* r, const SDL_FRect& q, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
        SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
        SDL_RenderFillRect(r, &q);
    }

    // ---------- рамки рангов ----------
    // звание -> ступень рамки: 0 нет, 1 A (бронза), 2 A+ (серебро), 3 S (золото), 4 S+ (алмаз), 5 S++ / S+++ (божественная, огонь),
    // 6 B (зелёная), 7 C (оранжевая), 8 D (красная), 9 F (чёрная).
    // Пробелы и регистр не важны; русские А, В, С считаются латинскими A, B, C.
    int RankTier(const std::string& rank) {
        std::string n;
        for (size_t i = 0; i < rank.size(); ++i) {
            const unsigned char c = static_cast<unsigned char>(rank[i]);
            if (c == ' ' || c == '\t') continue;
            if ((c == 0xD0 || c == 0xD1) && i + 1 < rank.size()) {                // кириллические двойники латиницы
                const unsigned char d = static_cast<unsigned char>(rank[i + 1]);
                char lat = 0;
                if (c == 0xD0 && (d == 0x90 || d == 0xB0)) lat = 'A';     // А а
                else if (c == 0xD0 && d == 0x92)           lat = 'B';     // В
                else if ((c == 0xD0 && d == 0xA1) || (c == 0xD1 && d == 0x81)) lat = 'C';   // С с
                if (lat) { n.push_back(lat); ++i; continue; }
            }
            n.push_back(char(std::toupper(c)));
        }
        if (n == "A") return 1;
        if (n == "A+") return 2;
        if (n == "S") return 3;
        if (n == "S+") return 4;
        if (n == "S++" || n == "S+++") return 5;
        if (n == "B") return 6;
        if (n == "C") return 7;
        if (n == "D") return 8;
        if (n == "F") return 9;
        return 0;
    }

    void RankTierColor(int tier, Uint8& cr, Uint8& cg, Uint8& cb) {
        switch (tier) {
        case 1:  cr = 205; cg = 127; cb = 50;  break;    // бронза
        case 2:  cr = 205; cg = 210; cb = 225; break;    // серебро
        case 3:  cr = 255; cg = 205; cb = 60;  break;    // золото
        case 4:  cr = 120; cg = 225; cb = 255; break;    // алмаз
        case 6:  cr = 90;  cg = 210; cb = 110; break;    // B: зелёная
        case 7:  cr = 255; cg = 145; cb = 40;  break;    // C: оранжевая
        case 8:  cr = 235; cg = 65;  cb = 65;  break;    // D: красная
        case 9:  cr = 12;  cg = 12;  cb = 14;  break;    // F: чёрная
        default: cr = 255; cg = 214; cb = 64;  break;    // божественная: жёлтое пламя
        }
    }

    // рамка вокруг ранга: бронза / серебро / золото / алмаз / божественная (анимируется по времени)
    void DrawRankFrame(SDL_Renderer* r, int tier, const SDL_FRect& f) {
        const float t = float(SDL_GetTicks());
        Uint8 cr, cg, cb;
        RankTierColor(tier, cr, cg, cb);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

        // фон с лёгким оттенком цвета рамки
        if (tier == 9) FillRect(r, f, 8, 8, 10, 255);                                          // F: почти чёрный фон
        else           FillRect(r, f, Uint8(22 + cr / 14), Uint8(24 + cg / 14), Uint8(30 + cb / 14), 255);

        // внутренний нижний подсвет у божественной (будто пламя освещает рамку)
        if (tier == 5) {
            const float pulse = 0.65f + 0.35f * std::sin(t * 0.006f);
            for (int i = 0; i < 8; ++i) {
                const SDL_FRect row{ f.x + 2.f, f.y + f.h - 3.f - float(i), f.w - 4.f, 1.f };
                FillRect(r, row, 255, 150, 30, Uint8(70.f * pulse * (1.f - float(i) / 8.f)));
            }
        }

        // свечение снаружи: у золота слабое, у алмаза голубое, у божественной сильное и пульсирует
        if (tier >= 3 && tier <= 5) {
            const float pulse = (tier == 5) ? 0.7f + 0.3f * std::sin(t * 0.007f) : 0.8f + 0.2f * std::sin(t * 0.003f);
            const float base = (tier == 5) ? 90.f : 55.f;
            for (int i = 1; i <= 4; ++i) {
                SDL_SetRenderDrawColor(r, cr, cg, cb, Uint8(base * pulse * (1.f - float(i) / 5.f)));
                const SDL_FRect g{ f.x - float(i), f.y - float(i), f.w + 2.f * float(i), f.h + 2.f * float(i) };
                SDL_RenderRect(r, &g);
            }
        }

        // блик, пробегающий по рамке: серебро - белый, алмаз - голубой и быстрее, золото - тёплый
        if (tier == 2 || tier == 3 || tier == 4) {
            const float speed = (tier == 4) ? 0.12f : 0.07f;
            const float span = f.w + 60.f;
            const float pos = std::fmod(t * speed + float(tier) * 37.f, span) - 30.f;
            const float x0 = std::max(f.x + 2.f, f.x + pos - 8.f), x1 = std::min(f.x + f.w - 2.f, f.x + pos + 8.f);
            if (x1 > x0) {
                const SDL_FRect band{ x0, f.y + 2.f, x1 - x0, f.h - 4.f };
                if (tier == 4)      FillRect(r, band, 220, 250, 255, 80);
                else if (tier == 3) FillRect(r, band, 255, 235, 160, 55);
                else                FillRect(r, band, 255, 255, 255, 60);
            }
        }

        // сама рамка: две линии (у алмаза и божественной ярче и переливается)
        Uint8 br = cr, bg2 = cg, bb = cb;
        if (tier == 4) {
            const float k = 0.5f + 0.5f * std::sin(t * 0.005f);
            br = Uint8(120 + 100 * k); bg2 = Uint8(225 + 30 * k); bb = 255;
        }
        SDL_SetRenderDrawColor(r, br, bg2, bb, 255);
        SDL_RenderRect(r, &f);
        const SDL_FRect in{ f.x + 1.f, f.y + 1.f, f.w - 2.f, f.h - 2.f };
        SDL_SetRenderDrawColor(r, br, bg2, bb, tier == 1 ? 150 : 200);
        SDL_RenderRect(r, &in);
        if (tier == 9) {                                                                       // чёрная рамка на тёмном фоне: тонкий серый контур снаружи, чтобы была видна
            const SDL_FRect out{ f.x - 1.f, f.y - 1.f, f.w + 2.f, f.h + 2.f };
            SDL_SetRenderDrawColor(r, 95, 95, 102, 255);
            SDL_RenderRect(r, &out);
        }

        // огонь: языки пламени растут вверх от верхней кромки рамки, плюс искры по бокам
        if (tier == 5) {
            for (float x = f.x + 2.f; x < f.x + f.w - 4.f; x += 3.f) {
                const float edge = std::min(1.f, std::min(x - f.x, f.x + f.w - x) / 14.f);       // у краёв пламя ниже
                const float ph = t * 0.011f + x * 0.30f;
                const float wob = 0.6f + 0.4f * std::sin(t * 0.007f + x * 0.11f);
                const float hgt = std::max(1.f, (2.f + 7.f * (0.5f + 0.5f * std::sin(ph + std::sin(t * 0.005f + x * 0.9f) * 2.f))) * wob * (0.5f + 0.5f * edge));
                FillRect(r, { x, f.y - hgt, 3.f, hgt }, 255, 110, 20, 150);                       // внешний оранжевый слой
                FillRect(r, { x, f.y - hgt * 0.62f, 3.f, hgt * 0.62f }, 255, 195, 40, 200);       // жёлтая середина
                FillRect(r, { x, f.y - hgt * 0.30f, 3.f, hgt * 0.30f }, 255, 245, 170, 230);      // светлое ядро
            }
            for (int s = 0; s < 6; ++s) {                                                         // искры: поднимаются и гаснут
                const float life = std::fmod(t * 0.0013f + float(s) * 0.37f, 1.f);
                const float sx = f.x + 6.f + std::fmod(float(s) * 53.7f + 11.f, std::max(8.f, f.w - 12.f));
                const float sy = f.y - 4.f - life * 12.f;
                FillRect(r, { sx, sy, 2.f, 2.f }, 255, 225, 90, Uint8(220.f * (1.f - life)));
            }
        }
    }

    // одна строка: (1) тег, (2) название уровня, (3) квадратик с уровнем и шкала опыта по этому тегу
    void DrawTagRow(SDL_Renderer* r, const TagRow& row, float x, float y, float w) {
        Uint8 tr, tg, tb;
        Level_TierColor(row.info.tier, tr, tg, tb);

        const SDL_FRect bg{ x, y, w, kRowH };
        FillRect(r, bg, Theme::Panel.r, Theme::Panel.g, Theme::Panel.b, 255);
        SDL_SetRenderDrawColor(r, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 70);
        SDL_RenderRect(r, &bg);

        const float pad = 10.f;
        const float c1 = std::clamp(w * 0.22f, 110.f, 220.f);    // колонка тега
        const float c2 = std::clamp(w * 0.20f, 120.f, 240.f);    // колонка названия уровня

        // (1) тег
        const SDL_FRect tagBox{ x + pad, y + pad, c1 - pad, kRowH - 2.f * pad };
        FillRect(r, tagBox, 30, 34, 44, 255);
        SDL_SetRenderDrawColor(r, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 200);
        SDL_RenderRect(r, &tagBox);
        DrawUtf8TextFit(r, row.tag, tagBox.x + 6.f, tagBox.y + tagBox.h * 0.5f, tagBox.w - 12.f, 24, true, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 255);

        // (2) название уровня
        const float x2 = x + c1 + 16.f;
        DrawUtf8TextFit(r, UiText(Level_TierName(row.info.tier, false), Level_TierName(row.info.tier, true)),
            x2, y + kRowH * 0.5f, c2 - 16.f, 24, false, tr, tg, tb, 255);

        // (3) квадратик с номером уровня и шкала опыта
        const float x3 = x + c1 + c2 + 8.f;
        const float sq = kRowH - 2.f * pad;
        if (x3 + sq + 90.f <= x + w) {
            const SDL_FRect box{ x3, y + pad, sq, sq };
            FillRect(r, box, Theme::Panel.r, Theme::Panel.g, Theme::Panel.b, 255);
            SDL_SetRenderDrawColor(r, tr, tg, tb, 255);
            SDL_RenderRect(r, &box);
            DrawUtf8TextFit(r, std::to_string(row.info.level), box.x + 3.f, box.y + sq * 0.5f, sq - 6.f, 24, true, tr, tg, tb, 255);

            const float bx = x3 + sq + 12.f, bw = x + w - pad - bx;
            const SDL_FRect bar{ bx, y + pad + 6.f, bw, 18.f };
            FillRect(r, bar, Theme::Panel.r, Theme::Panel.g, Theme::Panel.b, 255);
            SDL_SetRenderDrawColor(r, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 110);
            SDL_RenderRect(r, &bar);
            const float frac = row.info.xpNeed > 0 ? float(double(row.info.xpInto) / double(row.info.xpNeed)) : 0.f;
            if (frac > 0.f)
                FillRect(r, { bar.x + 3.f, bar.y + 3.f, (bar.w - 6.f) * std::min(frac, 1.f), bar.h - 6.f }, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 210);
            const std::string xpText = std::to_string(row.info.xpInto) + " / " + std::to_string(row.info.xpNeed) + " XP";
            DrawUtf8TextFit(r, xpText, bx, bar.y + bar.h + 16.f, bw, 16, false, Theme::InkDim.r, Theme::InkDim.g, Theme::InkDim.b, 255);
        }
    }

    void DrawAchievements(SDL_Renderer* r, float x, float y, float w, float h) {
        g_achRect = { x, y, w, h };
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        FillRect(r, g_achRect, 26, 28, 36, 255);
        SDL_SetRenderDrawColor(r, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 110);
        SDL_RenderRect(r, &g_achRect);

        const std::string head = std::string(UiText("ACHIEVEMENTS", "ДОСТИЖЕНИЯ")) + "  " +
                                 std::to_string(g_achUnlocked) + " / " + std::to_string(Ach_Count());
        DrawUtf8TextFit(r, head, x + 14.f, y + 17.f, w - 28.f, 16, false, Theme::InkDim.r, Theme::InkDim.g, Theme::InkDim.b, 255);

        const float pad = 12.f;
        const int count = int(g_achOrder.size());
        const SDL_FRect view{ x + 1.f, y + 30.f, w - 2.f, h - 31.f };
        const float contentW = pad * 2.f + float(count) * (kAchCardW + kAchGap) - kAchGap;
        g_achMax = std::max(0.f, contentW - view.w);
        g_achScroll = std::clamp(g_achScroll, 0.f, g_achMax);

        const SDL_Rect clip{ int(view.x), int(view.y), int(view.w), int(view.h) };
        SDL_SetRenderClipRect(r, &clip);
        float cx = view.x + pad - g_achScroll;
        const float cy = view.y + 4.f;
        for (int k = 0; k < count; ++k, cx += kAchCardW + kAchGap) {
            if (cx + kAchCardW < view.x || cx > view.x + view.w) continue;   // вне видимой части
            const int i = g_achOrder[size_t(k)];
            const AchDef& d = Ach_Def(i);
            const bool open = Ach_Unlocked(g_achStats, i);
            const Uint8 cr = open ? d.r : 90, cg = open ? d.g : 105, cb = open ? d.b : 120;   // закрытые - серые

            const SDL_FRect card{ cx, cy, kAchCardW, kAchCardH };
            FillRect(r, card, Theme::Panel.r, Theme::Panel.g, Theme::Panel.b, 255);
            SDL_SetRenderDrawColor(r, cr, cg, cb, open ? 220 : 90);
            SDL_RenderRect(r, &card);

            const float tx = card.x + 8.f, tw = card.w - 16.f;
            DrawUtf8TextFit(r, UiText(d.titleEn, d.titleRu), tx, card.y + 14.f, tw, 16, false, cr, cg, cb, 255);
            const std::vector<std::string> lines = WrapText(UiText(d.descEn, d.descRu), tw, 8, 3);
            for (size_t li = 0; li < lines.size(); ++li)
                DrawUtf8Text(r, lines[li], tx, card.y + 28.f + float(li) * 12.f, 8, open ? 190 : 120, open ? 215 : 135, open ? 230 : 150, 255);

            // мини-шкала прогресса к порогу
            const SDL_FRect bar{ tx, card.y + card.h - 11.f, tw, 4.f };
            FillRect(r, bar, 40, 44, 56, 255);
            const float frac = float(double(Ach_Value(g_achStats, i)) / double(d.target));
            if (frac > 0.f) FillRect(r, { bar.x, bar.y, bar.w * std::min(frac, 1.f), bar.h }, cr, cg, cb, open ? 255 : 150);
        }
        SDL_SetRenderClipRect(r, nullptr);

        if (g_achMax > 0.f) {                                                // полоса прокрутки внизу
            const float trackX = view.x + pad, trackW = view.w - 2.f * pad, trackY = y + h - 9.f;
            const float thumbW = std::max(24.f, trackW * view.w / contentW);
            const float thumbX = trackX + (trackW - thumbW) * (g_achScroll / g_achMax);
            FillRect(r, { thumbX, trackY, thumbW, 4.f }, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 160);
        }
    }

    void DrawTagList(SDL_Renderer* r, float x, float y, float w, float h) {
        g_listRect = { x, y, w, h };
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        FillRect(r, g_listRect, 26, 28, 36, 255);
        SDL_SetRenderDrawColor(r, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 110);
        SDL_RenderRect(r, &g_listRect);

        // заголовок списка
        const float headH = 34.f;
        DrawUtf8TextFit(r, UiText("SUBJECT LEVELS", "УРОВНИ ПО ПРЕДМЕТАМ"), x + 14.f, y + headH * 0.5f + 2.f, w - 28.f, 16, false, Theme::InkDim.r, Theme::InkDim.g, Theme::InkDim.b, 255);

        const SDL_FRect view{ x + 1.f, y + headH, w - 2.f, h - headH - 1.f };
        if (g_tags.empty()) {
            g_listMax = 0.f;
            DrawUtf8TextFit(r, UiText("NOTHING HERE YET - COMPLETE REPOSITORIES WITH A SUBJECT TAG",
                                      "ПОКА ПУСТО - ВЫПОЛНЯЙТЕ РЕПОЗИТОРИИ С ТЕГОМ ПРЕДМЕТА"),
                view.x + 14.f, view.y + 30.f, view.w - 28.f, 16, false, 110, 140, 160, 255);
            return;
        }

        const float contentH = float(g_tags.size()) * (kRowH + kRowGap) + kRowGap;
        g_listMax = std::max(0.f, contentH - view.h);
        g_listScroll = std::clamp(g_listScroll, 0.f, g_listMax);

        const SDL_Rect clip{ int(view.x), int(view.y), int(view.w), int(view.h) };
        SDL_SetRenderClipRect(r, &clip);
        const float rowW = view.w - 2.f * kRowGap - (g_listMax > 0.f ? 8.f : 0.f);
        float ry = view.y + kRowGap - g_listScroll;
        for (const TagRow& row : g_tags) {
            if (ry + kRowH >= view.y && ry <= view.y + view.h)      // рисуем только видимые строки
                DrawTagRow(r, row, view.x + kRowGap, ry, rowW);
            ry += kRowH + kRowGap;
        }
        SDL_SetRenderClipRect(r, nullptr);

        if (g_listMax > 0.f) {                                       // полоса прокрутки
            const float trackH = view.h - 2.f * kRowGap;
            const float thumbH = std::max(24.f, trackH * view.h / contentH);
            const float thumbY = view.y + kRowGap + (trackH - thumbH) * (g_listScroll / g_listMax);
            FillRect(r, { view.x + view.w - 8.f, thumbY, 4.f, thumbH }, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 160);
        }
    }

    TextButton g_back{ 0, 0, 110, 40, "BACK" };
    TextButton g_change{ 0, 0, 240, 40, "ADD AVATAR" };
    TextButton g_ranks{ 0, 0, 240, 40, "GET RANK" };     // открывает экран рангов

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
    g_totalXp = Level_GetXp(login);
    g_level = Level_FromXp(g_totalXp);
    LoadTags(login);
    g_rankList = Level_GetRanks(login);
    LoadAchievements(login, g_totalXp, g_level.level);
    g_back.hovered = g_change.hovered = g_ranks.hovered = false;
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
    if (g_err.empty()) LoadAchievements(g_login, g_totalXp, g_level.level);   // достижение "лицо профиля"
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

    // аватар, логин и кнопка "редактировать профиль" - блок в левом верхнем углу под кнопкой BACK
    const float d = std::min(160.f, std::max(64.f, float(h) - 290.f));
    const float nameH = 24.f, gap1 = 20.f, gap2 = 24.f, btnH = 40.f;
    const float top = 96.f;

    g_change.label = UiText("EDIT PROFILE", "РЕДАКТИРОВАТЬ ПРОФИЛЬ");
    g_change.w = std::min(float(w) - 60.f, Utf8TextWidth(g_change.label, 16) + 40.f);
    const float colW = std::max(g_change.w, d);          // ширина блока: по самому широкому элементу
    const float cx = 30.f + colW * 0.5f;

    Avatar_Draw(r, cx, top + d * 0.5f, d, false);

    // логин по центру блока: крупный шрифт сжимается, если логин длиннее блока
    const float nameCy = top + d + gap1 + nameH * 0.5f;
    DrawUtf8TextFit(r, g_login, 30.f, nameCy, colW, 24, true, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 255);

    g_change.x = cx - g_change.w * 0.5f;
    g_change.y = top + d + gap1 + nameH + gap2;
    TextButton_Draw(r, g_change);
    nd[ndCount++] = TextButton_Rect(g_change);

    // основной уровень: справа от аватара. Сверху квадратик с номером уровня и название группы,
    // под ними шкала опыта до следующего уровня
    {
        const float x0 = 30.f + colW + 50.f;
        const float rowW = std::min(360.f, float(w) - x0 - 30.f);
        if (rowW >= 160.f) {
            const float sq = 72.f, bx = x0, by = top;
            Uint8 tr, tg, tb;
            Level_TierColor(g_level.tier, tr, tg, tb);

            // квадратик: тёмная заливка, мягкое свечение и контур цвета группы
            const SDL_FRect box{ bx, by, sq, sq };
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, Theme::Panel.r, Theme::Panel.g, Theme::Panel.b, 255);
            SDL_RenderFillRect(r, &box);
            for (int i = 3; i >= 1; --i) {
                const SDL_FRect g{ bx - i, by - i, sq + 2.f * i, sq + 2.f * i };
                SDL_SetRenderDrawColor(r, tr, tg, tb, Uint8(26 / i));
                SDL_RenderRect(r, &g);
            }
            SDL_SetRenderDrawColor(r, tr, tg, tb, 255);
            SDL_RenderRect(r, &box);
            const SDL_FRect inner{ bx + 1.f, by + 1.f, sq - 2.f, sq - 2.f };
            SDL_SetRenderDrawColor(r, tr, tg, tb, 120);
            SDL_RenderRect(r, &inner);
            DrawUtf8TextFit(r, std::to_string(g_level.level), bx + 6.f, by + sq * 0.5f, sq - 12.f, 32, true, tr, tg, tb, 255);

            // справа от квадратика: название группы и подпись "уровень"
            const float tx = bx + sq + 16.f, tw = rowW - sq - 16.f;
            DrawUtf8TextFit(r, UiText(Level_TierName(g_level.tier, false), Level_TierName(g_level.tier, true)),
                tx, by + 22.f, tw, 24, false, tr, tg, tb, 255);
            const std::string lvlLabel = std::string(UiText("LEVEL ", "УРОВЕНЬ ")) + std::to_string(g_level.level);
            DrawUtf8TextFit(r, lvlLabel, tx, by + 54.f, tw, 16, false, Theme::InkDim.r, Theme::InkDim.g, Theme::InkDim.b, 255);

            // шкала опыта под ними
            const SDL_FRect bar{ bx, by + sq + 16.f, rowW, 22.f };
            SDL_SetRenderDrawColor(r, Theme::Panel.r, Theme::Panel.g, Theme::Panel.b, 255);
            SDL_RenderFillRect(r, &bar);
            SDL_SetRenderDrawColor(r, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 110);
            SDL_RenderRect(r, &bar);
            const float frac = g_level.xpNeed > 0 ? float(double(g_level.xpInto) / double(g_level.xpNeed)) : 0.f;
            if (frac > 0.f) {
                const SDL_FRect fill{ bar.x + 3.f, bar.y + 3.f, (bar.w - 6.f) * std::min(frac, 1.f), bar.h - 6.f };
                SDL_SetRenderDrawColor(r, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 210);
                SDL_RenderFillRect(r, &fill);
            }
            const std::string xpText = std::to_string(g_level.xpInto) + " / " + std::to_string(g_level.xpNeed) + " XP";
            DrawUtf8TextFit(r, xpText, bx, bar.y + bar.h + 18.f, rowW, 16, false, Theme::InkDim.r, Theme::InkDim.g, Theme::InkDim.b, 255);
        }
    }

    // достижения: лента справа от основного уровня, двигается по X (колёсико или перетаскивание)
    {
        const float x0 = 30.f + colW + 50.f;
        const float ax = x0 + 360.f + 56.f, ay = top - 16.f;
        const float aw = float(w) - 30.f - ax, ah = 132.f;
        if (aw >= 200.f) {
            DrawAchievements(r, ax, ay, aw, ah);
            nd[ndCount++] = { int(ax), int(ay), int(aw), int(ah) };   // чтобы перетаскивание ленты не двигало окно
        } else {
            g_achRect = { 0, 0, 0, 0 };
            g_achDrag = false;
        }
    }

    // список уровней по тегам предметов: под основным уровнем, до правого и нижнего края
    {
        const float x0 = 30.f + colW + 50.f;
        const float ly = top + 72.f + 16.f + 22.f + 18.f + 28.f;     // под шкалой основного уровня
        const float lw = float(w) - 30.f - x0, lh = float(h) - 40.f - ly;
        if (lw >= 300.f && lh >= 120.f) DrawTagList(r, x0, ly, lw, lh);
        else g_listRect = { 0, 0, 0, 0 };
    }

    // кнопка "получить ранг" и под ней список рангов (заготовка) - в левом столбце под "редактировать профиль"
    {
        g_ranks.label = UiText("GET RANK", "ПОЛУЧИТЬ РАНГ");
        g_ranks.w = colW;
        g_ranks.x = 30.f;
        g_ranks.y = g_change.y + btnH + 56.f;
        TextButton_Draw(r, g_ranks);
        nd[ndCount++] = TextButton_Rect(g_ranks);

        const float ly = g_ranks.y + btnH + 16.f, lh = float(h) - 40.f - ly;
        if (lh >= 90.f) {
            const SDL_FRect box{ 30.f, ly, colW, lh };
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            FillRect(r, box, 26, 28, 36, 255);
            SDL_SetRenderDrawColor(r, Theme::Cyan.r, Theme::Cyan.g, Theme::Cyan.b, 110);
            SDL_RenderRect(r, &box);
            DrawUtf8TextFit(r, UiText("RANKS", "РАНГИ"), 44.f, ly + 17.f, colW - 28.f, 16, false, Theme::InkDim.r, Theme::InkDim.g, Theme::InkDim.b, 255);
            if (g_rankList.empty()) {
                DrawUtf8TextFit(r, UiText("NO RANKS YET", "РАНГОВ ПОКА НЕТ"), 44.f, ly + 52.f, colW - 28.f, 16, false, 110, 140, 160, 255);
            }
            else {
                // строки "тег - звание"; ранги A, A+, S, S+, S++ получают рамку (бронза, серебро, золото, алмаз, огонь);
                // если строки не помещаются, последняя показывает, сколько скрыто
                const float rowH = 42.f, frameH = 30.f, flameRoom = 10.f;
                const float listTop = ly + 40.f;
                const int fit = std::max(1, int((ly + lh - 8.f - listTop) / rowH));
                const int total = int(g_rankList.size());
                const int shown = (total <= fit) ? total : std::max(1, fit - 1);
                const float fx = 40.f, fw = colW - 20.f;
                for (int i = 0; i < shown; ++i) {
                    const RankEntry& re = g_rankList[size_t(i)];
                    const std::string line = re.tag.empty() ? re.rank : re.tag + " - " + re.rank;
                    const float fy = listTop + flameRoom + float(i) * rowH;
                    const SDL_FRect frame{ fx, fy, fw, frameH };
                    const int tier = RankTier(re.rank);
                    if (tier > 0) {
                        Uint8 tr2, tg2, tb2;
                        RankTierColor(tier, tr2, tg2, tb2);
                        if (tier == 9) { tr2 = 175; tg2 = 175; tb2 = 182; }                       // текст на чёрной рамке светло-серый
                        DrawRankFrame(r, tier, frame);
                        DrawUtf8TextFit(r, line, fx + 10.f, fy + frameH * 0.5f, fw - 20.f, 16, false, tr2, tg2, tb2, 255);
                    }
                    else {
                        DrawUtf8TextFit(r, line, fx + 10.f, fy + frameH * 0.5f, fw - 20.f, 16, false, Theme::Ink.r, Theme::Ink.g, Theme::Ink.b, 255);
                    }
                }
                if (total > shown)
                    DrawUtf8TextFit(r, "+" + std::to_string(total - shown), fx + 10.f,
                        listTop + flameRoom + float(shown) * rowH + frameH * 0.5f, fw - 20.f, 16, false, 110, 140, 160, 255);
            }
        }
    }

    if (!g_err.empty()) {
        const std::string shownError = UiMessage(g_err);
        DrawUtf8TextFit(r, shownError, 30.f, g_change.y + btnH + 24.f, colW, 16, true, 255, 90, 90, 255);
    }
}

ProfileResult Profile_Handle(const SDL_Event& e) {
    if (TextButton_Handle(g_back, e)) return ProfileResult::Back;
    if (TextButton_Handle(g_change, e)) ChooseFile();
    if (TextButton_Handle(g_ranks, e)) return ProfileResult::Ranks;

    // лента достижений: колёсико (и вертикальное, и горизонтальное) над ней или перетаскивание левой кнопкой
    if (g_achRect.w > 0.f) {
        auto inAch = [](float mx, float my) {
            return mx >= g_achRect.x && mx <= g_achRect.x + g_achRect.w &&
                   my >= g_achRect.y && my <= g_achRect.y + g_achRect.h;
        };
        if (e.type == SDL_EVENT_MOUSE_WHEEL && inAch(e.wheel.mouse_x, e.wheel.mouse_y)) {
            const float d = (e.wheel.x != 0.f) ? -e.wheel.x : e.wheel.y;
            g_achScroll = std::clamp(g_achScroll - d * 60.f, 0.f, g_achMax);
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT &&
                 inAch(e.button.x, e.button.y))
            g_achDrag = true;
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT)
            g_achDrag = false;
        else if (e.type == SDL_EVENT_MOUSE_MOTION && g_achDrag)
            g_achScroll = std::clamp(g_achScroll - e.motion.xrel, 0.f, g_achMax);
    }

    if (e.type == SDL_EVENT_MOUSE_WHEEL && g_listRect.w > 0.f) {      // колёсико над списком = прокрутка
        const float mx = e.wheel.mouse_x, my = e.wheel.mouse_y;
        if (mx >= g_listRect.x && mx <= g_listRect.x + g_listRect.w &&
            my >= g_listRect.y && my <= g_listRect.y + g_listRect.h)
            g_listScroll = std::clamp(g_listScroll - e.wheel.y * 40.f, 0.f, g_listMax);
    }

    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_ESCAPE)
        return ProfileResult::Back;

    return ProfileResult::None;
}

void Profile_Destroy() {
    TextButton_Destroy(g_back);
    TextButton_Destroy(g_change);
    TextButton_Destroy(g_ranks);
}

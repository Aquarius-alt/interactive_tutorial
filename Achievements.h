#pragma once
#include <vector>

// Достижения. Каждое - порог по одной из величин профиля (статистика считается из данных на диске,
// отдельно "выдавать" достижения не нужно: открылось, как только величина дошла до порога).
enum AchStat {
    Stat_ReposDone,      // выполненных репозиториев (с опытом за выполнение)
    Stat_ReposCreated,   // созданных репозиториев
    Stat_Level,          // основной уровень
    Stat_TotalXp,        // весь основной опыт
    Stat_TagCount,       // тегов предметов, по которым есть опыт
    Stat_TopTagLevel,    // самый высокий уровень среди тегов
    Stat_HasAvatar,      // есть аватар (0/1)
    Stat_Count
};

struct AchStats { long long v[Stat_Count] = {}; };

struct AchDef {
    const char* titleEn; const char* titleRu;
    const char* descEn;  const char* descRu;
    AchStat stat;
    long long target;
    unsigned char r, g, b;       // цвет открытого достижения
};

int          Ach_Count();
const AchDef& Ach_Def(int i);
long long    Ach_Value(const AchStats& s, int i);    // текущее значение величины (не больше target)
bool         Ach_Unlocked(const AchStats& s, int i);

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
    Stat_RanksCount,     // полученных рангов
    Stat_TopRankTier,    // лучший ранг: 1 A, 2 A+, 3 S, 4 S+, 5 S++/S+++ (B, C, D, F не считаются)
    Stat_TagsLv10,       // предметов, по которым достигнут 10 уровень
    Stat_TheoryCount,    // теорий во всех репозиториях
    Stat_TestCount,      // тестов во всех репозиториях
    Stat_GameCount,      // мини-игр во всех репозиториях
    Stat_QuestionCount,  // вопросов во всех тестах
    Stat_FileCount,      // вложений в теориях и тестах
    Stat_ReposWithCover, // репозиториев с обложкой
    Stat_FullStackRepos, // репозиториев, где есть и теория, и тесты, и игры
    Stat_MaxRepoItems,   // больше всего материалов (теории + тесты + игры) в одном репозитории
    Stat_MaxRepoXp,      // самый большой основной опыт за один репозиторий
    Stat_AiItems,        // материалов с пометкой "сделано с помощью ИИ"
    Stat_HandItems,      // материалов без этой пометки
    Stat_UserStyles,     // собственных стилей интерфейса в библиотеке (без встроенных пресетов)
    Stat_UserLooks,      // разных видов (FLAT / HUD / BLADE / NEON / REGAL) среди собственных стилей
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

// Величины, которые считает не экран профиля, а main.cpp (стили интерфейса): обновляются при запуске
// и при сохранении / удалении стиля.
inline long long g_achUserStyles = 0;
inline long long g_achUserLooks = 0;

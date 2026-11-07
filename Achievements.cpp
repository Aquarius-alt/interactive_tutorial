#include "Achievements.h"
#include <algorithm>

namespace {
    const AchDef kDefs[] = {
        // выполненные репозитории
        { "FIRST STEP",   "ПЕРВЫЙ ШАГ",     "Complete your first repository",        "Выполни свой первый репозиторий",          Stat_ReposDone,    1,  120, 220, 140 },
        { "DILIGENT",     "ПРИЛЕЖНЫЙ",      "Complete 5 repositories",               "Выполни 5 репозиториев",                   Stat_ReposDone,    5,   96, 228, 232 },
        { "MARATHONER",   "МАРАФОНЕЦ",      "Complete 10 repositories",              "Выполни 10 репозиториев",                  Stat_ReposDone,   10,  255, 205,  60 },
        { "UNSTOPPABLE",  "НЕОСТАНОВИМЫЙ",  "Complete 25 repositories",              "Выполни 25 репозиториев",                  Stat_ReposDone,   25,  255, 140,  60 },
        { "COMPLETIONIST","ЗАВЕРШИТЕЛЬ",    "Complete 100 repositories",             "Выполни 100 репозиториев",                 Stat_ReposDone,  100,  255,  70,  90 },

        // созданные репозитории
        { "CREATOR",      "СОЗДАТЕЛЬ",      "Create your first repository",         "Создай свой первый репозиторий",           Stat_ReposCreated, 1,  130, 170, 255 },
        { "BUILDER",      "СТРОИТЕЛЬ",      "Create 5 repositories",                 "Создай 5 репозиториев",                    Stat_ReposCreated, 5,  140, 235, 225 },
        { "ARCHITECT",    "АРХИТЕКТОР",     "Create 10 repositories",                "Создай 10 репозиториев",                   Stat_ReposCreated,10,  200, 120, 255 },
        { "LIBRARIAN",    "БИБЛИОТЕКАРЬ",   "Create 30 repositories",                "Создай 30 репозиториев",                   Stat_ReposCreated,30,  255, 235, 150 },

        // уровень
        { "LARPER",       "ЛАРПЕР",         "Reach level 10",                        "Достигни 10 уровня",                       Stat_Level,       10,  120, 220, 140 },
        { "NORMIE",       "НОРМИС",         "Reach level 20",                        "Достигни 20 уровня",                       Stat_Level,       20,   96, 228, 232 },
        { "BRONZE",       "БРОНЗА",         "Reach level 50",                        "Достигни 50 уровня",                       Stat_Level,       50,  205, 127,  50 },
        { "SILVER",       "СЕРЕБРО",        "Reach level 100",                       "Достигни 100 уровня",                      Stat_Level,      100,  205, 210, 220 },
        { "GOLD",         "ЗОЛОТО",         "Reach level 150",                       "Достигни 150 уровня",                      Stat_Level,      150,  255, 205,  60 },
        { "PLATINUM",     "ПЛАТИНА",        "Reach level 200",                       "Достигни 200 уровня",                      Stat_Level,      200,  140, 235, 225 },
        { "DIAMOND",      "АЛМАЗ",          "Reach level 300",                       "Достигни 300 уровня",                      Stat_Level,      300,  130, 170, 255 },
        { "140 IQ",       "140IQ",          "Reach level 400",                       "Достигни 400 уровня",                      Stat_Level,      400,  200, 120, 255 },
        { "LEGEND",       "ЛЕГЕНДА",        "Reach level 500",                       "Достигни 500 уровня",                      Stat_Level,      500,  255, 140,  60 },
        { "DEITY",        "БОЖЕСТВО",       "Reach level 750",                       "Достигни 750 уровня",                      Stat_Level,      750,  255, 235, 150 },
        { "PSYCHO",       "ПСИХ",           "Reach level 1000",                      "Достигни 1000 уровня",                     Stat_Level,     1000,  255,  70,  90 },
        { "CYBERPSYCHO",  "КИБЕРПСИХ",      "Reach level 2000",                      "Достигни 2000 уровня",                     Stat_Level,     2000,  255,  40, 200 },
        { "NOT HUMAN",    "НЕ ЧЕЛОВЕК",     "Reach level 5000",                      "Достигни 5000 уровня",                     Stat_Level,     5000,  255, 255, 255 },

        // опыт
        { "FIRST BLOOD",  "ПЕРВАЯ КРОВЬ",   "Earn 100 XP",                           "Заработай 100 опыта",                      Stat_TotalXp,    100,  150, 170, 190 },
        { "GRINDER",      "ГРИНДЕР",        "Earn 1,000 XP",                         "Заработай 1 000 опыта",                    Stat_TotalXp,   1000,   96, 228, 232 },
        { "XP HOARDER",   "НАКОПИТЕЛЬ",     "Earn 10,000 XP",                        "Заработай 10 000 опыта",                   Stat_TotalXp,  10000,  255, 205,  60 },
        { "XP OVERFLOW",  "ПЕРЕПОЛНЕНИЕ",   "Earn 100,000 XP",                       "Заработай 100 000 опыта",                  Stat_TotalXp, 100000,  255,  40, 200 },

        // предметы (теги)
        { "CURIOUS",      "ЛЮБОПЫТНЫЙ",     "Study 3 different subjects",            "Изучи 3 разных предмета",                  Stat_TagCount,     3,  120, 220, 140 },
        { "POLYMATH",     "ПОЛИМАТ",        "Study 5 different subjects",            "Изучи 5 разных предметов",                 Stat_TagCount,     5,  140, 235, 225 },
        { "ENCYCLOPEDIA", "ЭНЦИКЛОПЕДИЯ",   "Study 10 different subjects",           "Изучи 10 разных предметов",                Stat_TagCount,    10,  200, 120, 255 },
        { "SPECIALIST",   "СПЕЦИАЛИСТ",     "Reach level 10 in any subject",         "Достигни 10 уровня в любом предмете",      Stat_TopTagLevel, 10,   96, 228, 232 },
        { "PROFESSIONAL", "ПРОФИ",          "Reach level 50 in any subject",         "Достигни 50 уровня в любом предмете",      Stat_TopTagLevel, 50,  205, 127,  50 },
        { "MASTER",       "МАСТЕР",         "Reach level 100 in any subject",        "Достигни 100 уровня в любом предмете",     Stat_TopTagLevel,100,  255, 205,  60 },
        { "GRANDMASTER",  "ГРОССМЕЙСТЕР",   "Reach level 300 in any subject",        "Достигни 300 уровня в любом предмете",     Stat_TopTagLevel,300,  130, 170, 255 },

        // профиль
        { "FACE OF PROFILE","ЛИЦО ПРОФИЛЯ", "Set a profile picture",                 "Поставь аватар в профиле",                 Stat_HasAvatar,    1,  255,  90,  90 },
    };
    const int kCount = int(sizeof(kDefs) / sizeof(kDefs[0]));
}

int Ach_Count() { return kCount; }

const AchDef& Ach_Def(int i) { return kDefs[std::clamp(i, 0, kCount - 1)]; }

long long Ach_Value(const AchStats& s, int i) {
    const AchDef& d = Ach_Def(i);
    return std::min(s.v[d.stat], d.target);
}

bool Ach_Unlocked(const AchStats& s, int i) {
    const AchDef& d = Ach_Def(i);
    return s.v[d.stat] >= d.target;
}

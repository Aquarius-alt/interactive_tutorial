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

        // выполненные репозитории (ещё)
        { "WARMED UP",     "РАЗМИНКА",        "Complete 3 repositories",               "Выполни 3 репозитория",                    Stat_ReposDone,    3,  140, 235, 160 },
        { "HALF CENTURY",  "ПОЛТИННИК",       "Complete 50 repositories",              "Выполни 50 репозиториев",                  Stat_ReposDone,   50,  255, 170,  60 },
        { "WORKHORSE",     "ТРУДОГОЛИК",      "Complete 250 repositories",             "Выполни 250 репозиториев",                 Stat_ReposDone,  250,  255,  50,  90 },

        // созданные репозитории (ещё)
        { "TRILOGY",       "ТРИЛОГИЯ",        "Create 3 repositories",                 "Создай 3 репозитория",                     Stat_ReposCreated, 3,  120, 200, 255 },
        { "ARCHIVIST",     "АРХИВАРИУС",      "Create 50 repositories",                "Создай 50 репозиториев",                   Stat_ReposCreated,50,  255, 215, 130 },
        { "ALEXANDRIA",    "АЛЕКСАНДРИЯ",     "Create 100 repositories",               "Создай 100 репозиториев",                  Stat_ReposCreated,100, 255, 150,  90 },

        // опыт (ещё)
        { "SPARK",         "ИСКРА",           "Earn 500 XP",                           "Заработай 500 опыта",                      Stat_TotalXp,    500,  255, 225, 120 },
        { "XP STREAM",     "ПОТОК ОПЫТА",     "Earn 5,000 XP",                         "Заработай 5 000 опыта",                    Stat_TotalXp,   5000,  120, 230, 255 },
        { "XP SINGULARITY","СИНГУЛЯРНОСТЬ",   "Earn 1,000,000 XP",                     "Заработай 1 000 000 опыта",                Stat_TotalXp, 1000000, 255, 255, 255 },

        // предметы (ещё)
        { "FIRST SUBJECT", "ПЕРВЫЙ ПРЕДМЕТ",  "Study your first subject",              "Начни изучать первый предмет",             Stat_TagCount,     1,  150, 235, 150 },
        { "UNIVERSITY",    "УНИВЕРСИТЕТ",     "Study 20 different subjects",           "Изучи 20 разных предметов",                Stat_TagCount,    20,  255, 200, 120 },
        { "ADEPT",         "АДЕПТ",           "Reach level 20 in any subject",         "Достигни 20 уровня в любом предмете",      Stat_TopTagLevel, 20,  120, 220, 200 },
        { "VIRTUOSO",      "ВИРТУОЗ",         "Reach level 150 in any subject",        "Достигни 150 уровня в любом предмете",     Stat_TopTagLevel,150,  255, 190,  80 },
        { "SAGE",          "МУДРЕЦ",          "Reach level 500 in any subject",        "Достигни 500 уровня в любом предмете",     Stat_TopTagLevel,500,  220, 140, 255 },
        { "DOUBLE MAJOR",  "ДВА ПРОФИЛЯ",     "Reach level 10 in 2 subjects",          "Достигни 10 уровня в 2 предметах",         Stat_TagsLv10,     2,  140, 220, 255 },
        { "TRIPLE THREAT", "ТРОЙНАЯ УГРОЗА",  "Reach level 10 in 3 subjects",          "Достигни 10 уровня в 3 предметах",         Stat_TagsLv10,     3,  255, 160, 120 },
        { "ALL-ROUNDER",   "УНИВЕРСАЛ",       "Reach level 10 in 5 subjects",          "Достигни 10 уровня в 5 предметах",         Stat_TagsLv10,     5,  255, 225,  90 },

        // ранги
        { "RANKED",        "В РЕЙТИНГЕ",      "Earn your first rank",                  "Получи свой первый ранг",                  Stat_RanksCount,   1,  150, 230, 150 },
        { "CLIMBER",       "СКАЛОЛАЗ",        "Earn 3 ranks",                          "Получи 3 ранга",                           Stat_RanksCount,   3,  110, 220, 230 },
        { "RANK HUNTER",   "ОХОТНИК ЗА РАНГАМИ","Earn 10 ranks",                      "Получи 10 рангов",                         Stat_RanksCount,  10,  255, 190,  70 },
        { "RANK COLLECTOR","КОЛЛЕКЦИОНЕР РАНГОВ","Earn 25 ranks",                      "Получи 25 рангов",                         Stat_RanksCount,  25,  255, 120, 150 },
        { "A-CLASS",       "КЛАСС A",         "Earn an A rank or higher",              "Получи ранг A или выше",                   Stat_TopRankTier,  1,  205, 127,  50 },
        { "S-CLASS",       "КЛАСС S",         "Earn an S rank or higher",              "Получи ранг S или выше",                   Stat_TopRankTier,  3,  255, 205,  60 },
        { "S+ ELITE",      "ЭЛИТА S+",        "Earn an S+ rank or higher",             "Получи ранг S+ или выше",                  Stat_TopRankTier,  4,  120, 225, 255 },
        { "DIVINE RANK",   "БОЖЕСТВЕННЫЙ РАНГ","Earn an S++ rank",                     "Получи ранг S++",                          Stat_TopRankTier,  5,  255, 120, 255 },

        // теория, тесты, игры
        { "AUTHOR",        "АВТОР",           "Write 1 theory item",                   "Напиши 1 теорию",                          Stat_TheoryCount,  1,  140, 220, 255 },
        { "LECTURER",      "ЛЕКТОР",          "Write 10 theory items",                 "Напиши 10 теорий",                         Stat_TheoryCount, 10,  140, 235, 200 },
        { "PROFESSOR",     "ПРОФЕССОР",       "Write 50 theory items",                 "Напиши 50 теорий",                         Stat_TheoryCount, 50,  255, 215, 120 },
        { "EXAMINER",      "ЭКЗАМЕНАТОР",     "Create 1 test",                         "Создай 1 тест",                            Stat_TestCount,    1,  255, 170, 130 },
        { "INQUISITOR",    "ИНКВИЗИТОР",      "Create 10 tests",                       "Создай 10 тестов",                         Stat_TestCount,   10,  255, 120, 110 },
        { "DEAN",          "ДЕКАН",           "Create 50 tests",                       "Создай 50 тестов",                         Stat_TestCount,   50,  255,  70, 120 },
        { "GAME MAKER",    "ГЕЙМДЕВ",         "Add 1 mini-game",                       "Добавь 1 мини-игру",                       Stat_GameCount,    1,  130, 255, 170 },
        { "ARCADE",        "АРКАДА",          "Add 5 mini-games",                      "Добавь 5 мини-игр",                        Stat_GameCount,    5,  100, 235, 255 },
        { "GAME STUDIO",   "СТУДИЯ ИГР",      "Add 15 mini-games",                     "Добавь 15 мини-игр",                       Stat_GameCount,   15,  255, 100, 220 },
        { "QUIZMASTER",    "ВИКТОРИНЩИК",     "Write 10 test questions",               "Составь 10 вопросов в тестах",             Stat_QuestionCount,10, 150, 220, 255 },
        { "TRIVIA KING",   "КОРОЛЬ ВИКТОРИН", "Write 50 test questions",               "Составь 50 вопросов в тестах",             Stat_QuestionCount,50, 255, 205,  90 },
        { "SPHINX",        "СФИНКС",          "Write 200 test questions",              "Составь 200 вопросов в тестах",            Stat_QuestionCount,200,230, 150, 255 },

        // оформление и наполнение репозиториев
        { "COVER ART",     "ОБЛОЖКА",         "Give a repository a cover picture",     "Поставь обложку репозиторию",              Stat_ReposWithCover,1, 255, 150, 190 },
        { "GALLERY",       "ГАЛЕРЕЯ",         "Give 10 repositories a cover picture",  "Поставь обложку 10 репозиториям",          Stat_ReposWithCover,10,255, 190, 220 },
        { "PAPERCLIP",     "СКРЕПКА",         "Attach a file to a theory or test",     "Прикрепи файл к теории или тесту",         Stat_FileCount,    1,  170, 190, 210 },
        { "FILE CABINET",  "КАРТОТЕКА",       "Attach 50 files",                       "Прикрепи 50 файлов",                       Stat_FileCount,   50,  200, 215, 235 },
        { "FULL STACK",    "ФУЛЛСТЕК",        "One repository with theory, tests and games", "Репозиторий с теорией, тестами и играми сразу", Stat_FullStackRepos, 1, 120, 255, 200 },
        { "FULL STACK PRO","ФУЛЛСТЕК ПРО",    "5 repositories with theory, tests and games", "5 репозиториев с теорией, тестами и играми", Stat_FullStackRepos, 5, 255, 215, 100 },
        { "BIG REPO",      "БОЛЬШОЙ РЕПО",    "10 materials in one repository",        "10 материалов в одном репозитории",        Stat_MaxRepoItems,10,  140, 210, 255 },
        { "MEGA REPO",     "МЕГА-РЕПО",       "30 materials in one repository",        "30 материалов в одном репозитории",        Stat_MaxRepoItems,30,  255, 140, 140 },
        { "HIGH STAKES",   "ВЫСОКИЕ СТАВКИ",  "Make a repository worth 100 XP",        "Сделай репозиторий на 100 опыта",          Stat_MaxRepoXp,  100,  255, 200,  90 },
        { "JACKPOT",       "ДЖЕКПОТ",         "Make a repository worth 1,000 XP",      "Сделай репозиторий на 1 000 опыта",        Stat_MaxRepoXp, 1000,  255, 235,  60 },

        // ИИ и ручная работа
        { "CO-PILOT",      "ВТОРОЙ ПИЛОТ",    "Mark 1 item as made with AI",           "Отметь 1 материал как сделанный с ИИ",     Stat_AiItems,      1,  120, 200, 255 },
        { "CYBORG",        "КИБОРГ",          "Mark 20 items as made with AI",         "Отметь 20 материалов как сделанные с ИИ",  Stat_AiItems,     20,  200, 120, 255 },
        { "HANDCRAFTED",   "РУЧНАЯ РАБОТА",   "Make 25 items without AI",              "Сделай 25 материалов без ИИ",              Stat_HandItems,   25,  230, 190, 130 },
        { "ARTISAN",       "РЕМЕСЛЕННИК",     "Make 100 items without AI",             "Сделай 100 материалов без ИИ",             Stat_HandItems,  100,  255, 170,  90 },

        // оформление интерфейса
        { "DESIGNER",      "ДИЗАЙНЕР",        "Save your own interface style",         "Сохрани свой стиль интерфейса",            Stat_UserStyles,   1,  255, 130, 200 },
        { "FASHIONISTA",   "МОДНИК",          "Save 5 of your own styles",             "Сохрани 5 своих стилей",                   Stat_UserStyles,   5,  255, 160, 230 },
        { "LOOK HUNTER",   "ОХОТНИК ЗА ВИДАМИ","Use 3 different looks in your styles", "Используй 3 разных вида в своих стилях",  Stat_UserLooks,    3,  120, 255, 230 },
        { "FULL SPECTRUM", "ВЕСЬ СПЕКТР",     "Use all 5 looks in your styles",        "Используй все 5 видов в своих стилях",     Stat_UserLooks,    5,  255, 255, 255 },

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

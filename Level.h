#pragma once
#include <string>
#include <vector>

// Основной уровень пользователя. Опыт копится за выполнение репозиториев (mainXp из repo.json),
// уровень считается из суммарного опыта. Стоимость уровня зависит от группы:
//   1-9 новичок(100), 10-19 ларпер(100), 20-49 нормис(200), 50-99 бронза(250), 100-149 серебро(300),
//   150-199 золото(500), 200-299 платина(750), 300-399 алмаз(1000), 400-499 140iq(1500),
//   500-749 легенда(2000), 750-999 божество(2500), 1000-1999 псих(3000), 2000-4999 киберпсих(4000),
//   5000+ не человек(5000).
// Граница группы входит в новую группу (с 10-го уровня - уже ларпер). Стоимость - это опыт,
// нужный, чтобы с текущего уровня подняться на следующий.
// Опыт хранится в %APPDATA%\myproject\login\xp_<hex логина>.dat (одно число).

struct LevelInfo {
    int       level = 1;      // текущий уровень (с 1)
    int       tier = 0;       // индекс группы
    long long xpInto = 0;     // опыта набрано на текущем уровне
    long long xpNeed = 100;   // опыта нужно, чтобы перейти на следующий
};

LevelInfo   Level_FromXp(long long totalXp);
const char* Level_TierName(int tier, bool russian);
void        Level_TierColor(int tier, unsigned char& r, unsigned char& g, unsigned char& b);

long long Level_GetXp(const std::string& login);                 // суммарный опыт (0, если файла нет)
bool      Level_AddXp(const std::string& login, long long xp);   // прибавить опыт; true = записано

// Зачёт опыта за выполненный репозиторий: один раз на репозиторий (ключ - имя его папки).
// true = опыт только что начислен; false = уже начислялся раньше, xp <= 0 или не записалось.
// Список зачтённых репозиториев: %APPDATA%\myproject\login\xpdone_<hex логина>.dat
bool      Level_AwardRepo(const std::string& login, const std::string& repoKey, long long xp);

// ---- Уровни по тегам предметов ----
// Опыт предмета (subjectXp репозитория) копится отдельно по тегу (без учёта регистра) и считается
// по той же шкале уровней, что и основной. Файл: %APPDATA%\myproject\login\tags_<hex логина>.dat
struct TagLevel {
    std::string tag;       // как записан впервые
    long long   xp = 0;
};
std::vector<TagLevel> Level_GetTags(const std::string& login);   // от большего опыта к меньшему

// Зачёт опыта предмета за выполненный репозиторий (один раз на репозиторий и логин).
// Теги через ";" ("math; python") - разные теги, каждый получает полный xp.
// true = начислено; пустой тег или xp <= 0 - ничего не делает.
bool Level_AwardSubject(const std::string& login, const std::string& repoKey,
                        const std::string& tag, long long xp);

// Сколько репозиториев засчитано как выполненные (с ненулевым опытом за выполнение).
int Level_CompletedCount(const std::string& login);

// ---- Полученные ранги ----
// Ранг выдаётся за верно пройденное задание на ранг (экран "Ранги"): запись "тег - звание".
// Одно задание (ключ - имя его папки в _ranks) даёт ранг один раз.
// Файл: %APPDATA%\myproject\login\ranks_<hex логина>.dat (строка: ключ \t теги \t звание)
struct RankEntry {
    std::string tag;       // теги предмета (как в задании; может быть пустым)
    std::string rank;      // звание
};
std::vector<RankEntry> Level_GetRanks(const std::string& login);   // в порядке получения
bool Level_HasRank(const std::string& login, const std::string& taskKey);   // ранг за это задание уже получен?
bool Level_AwardRank(const std::string& login, const std::string& taskKey,
                     const std::string& tag, const std::string& rank);     // true = выдан только что; false = уже был, звание пусто или не записалось

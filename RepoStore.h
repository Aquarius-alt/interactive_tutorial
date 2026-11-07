#pragma once
#include <string>
#include <vector>
#include "CreateScreen.h"

// Сохранение созданного репозитория на диск (только Windows).
// Корень: <Documents>\myproject\<логин>\<название репозитория>\
//   repo.json            - название, описание, дата, счётчики
//   cover.png / .bmp     - выбранная картинка (если была)
//   theory\001_<имя>\    - item.json, content.txt, files\ (вложения)
//   tests\001_<имя>\     - то же + верные ответы в item.json
//   games\001_<имя>\     - папка мини-игры целиком (main.py, manifest.json, assets...) + _repo_game.json (название, файл запуска)
// Папка Documents определяется через Known Folders, поэтому работает и при переносе в OneDrive.
// Если репозиторий с таким названием уже есть, добавляется " (2)", " (3)" и т.д.
// Запись идёт во временную папку и переименовывается только при успехе, поэтому
// при ошибке наполовину записанных репозиториев не остаётся.

std::string RepoStore_Root();
bool        RepoStore_OpenRanksFolder(const std::string& user);                                   // <Documents>\myproject\<логин>\_ranks: сюда кладут задания на ранг (создаётся при первом открытии)
bool        RepoStore_SaveRankTask(const std::string& user, const TheoryItem& task);              // записывает задание на ранг в <Documents>\myproject\<логин>\_ranks\<название>\ (формат теста: item.json, content.txt, files\)
struct RankTask {
    TheoryItem  item;   // заголовок, вопросы с верными ответами, теги предмета, звание
    std::string key;    // имя папки задания в _ranks (по нему засчитывается полученный ранг)
};
std::vector<RankTask> RepoStore_ListRankTasks(const std::string& user);                           // все задания из _ranks (и созданные в программе, и положенные туда вручную через IMPORT)
bool        RepoStore_OpenUserFolder(const std::string& user);                                    // <Documents>\myproject
bool        RepoStore_Save(const std::string& user, const CreateData& d,
                           std::string* savedPath = nullptr,     // true = записано; вызывать можно из фонового потока
                           const std::string& replaceDir = "");  // не пусто = правка: новая версия заменяет эту папку
                                                                 // (дата создания и место в списке сохраняются)


// Краткие сведения о сохранённом репозитории (для карточек на главном экране).
struct RepoInfo {
    std::string name;         // название из repo.json (если не прочиталось - имя папки)
    std::string dir;          // полный путь к папке репозитория
    std::string coverPath;    // полный путь к cover.png / cover.bmp ("" = обложки нет)
    long long   created = 0;  // время изменения repo.json - для сортировки (старые первыми)
};

// Все репозитории пользователя (<Documents>\myproject\<логин>\*). Папки без repo.json и
// недописанные *.tmp пропускаются. Порядок: от старых к новым, поэтому новый встаёт перед "+".
std::vector<RepoInfo> RepoStore_List(const std::string& user);

// Удаляет папку репозитория целиком (вместе с теорией, тестами и вложениями). true = удалено.
bool RepoStore_Delete(const std::string& dir);

// Читает сохранённый репозиторий обратно в CreateData (для правки на экране CREATE).
// Вложения и обложка указывают на файлы внутри папки dir. false = не прочитался.
bool RepoStore_Load(const std::string& dir, CreateData& out);

// Проверяет папку с мини-игрой перед добавлением: есть ли файл запуска (по умолчанию main.py;
// если рядом лежит manifest.json, берётся его "entry" и "name") и не слишком ли она большая.
// true = можно добавлять (name - название игры, entry - файл запуска); false = error с причиной (английский текст).
bool RepoStore_InspectGame(const std::string& folder, std::string* name, std::string* entry, std::string* error);

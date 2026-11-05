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
//   games\               - пока пустая папка (игр ещё нет)
// Папка Documents определяется через Known Folders, поэтому работает и при переносе в OneDrive.
// Если репозиторий с таким названием уже есть, добавляется " (2)", " (3)" и т.д.
// Запись идёт во временную папку и переименовывается только при успехе, поэтому
// при ошибке наполовину записанных репозиториев не остаётся.

std::string RepoStore_Root();
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

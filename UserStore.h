#pragma once
#include <string>
#include <vector>

// Локальное хранилище пользователей (только Windows, использует CNG и DPAPI).
// Файл: %APPDATA%\myproject\login\users.dat
// Формат строки: логин \t соль:хэш_пароля \t соль:хэш_слова \t итерации \t пароль_DPAPI
// Последнее поле (пароль, зашифрованный DPAPI под текущего пользователя Windows)
// нужно только для восстановления через FORGOT; у старых записей его нет.

bool UserStore_Exists(const std::string& login);   // логин уже занят? (без учёта регистра)
bool UserStore_Add(const std::string& login,
                   const std::string& password,
                   const std::string& keyword);    // true = записано
std::string UserStore_Path();                      // полный путь к users.dat

// Проверка логина и пароля (логин без учёта регистра). true = верно.
// canonicalLogin (необязательно) получает логин так, как он записан при регистрации.
// Медленная (PBKDF2): блокирует на десятки миллисекунд.
bool UserStore_Verify(const std::string& login, const std::string& password,
                      std::string* canonicalLogin = nullptr);

struct RecoveredAccount {
    std::string login;
    std::string password;       // пусто, если hasPassword == false
    bool hasPassword = false;   // false: запись создана старой версией, пароль не сохранён
};

// Ищет все аккаунты, у которых кодовое слово совпадает (без учёта регистра).
// Медленная (PBKDF2 на каждую запись): вызывать из фонового потока.
std::vector<RecoveredAccount> UserStore_Recover(const std::string& keyword);

// Запомненный вход: логин последнего вошедшего хранится в session.dat рядом с users.dat.
// Это удобство, а не защита: кто может писать в папку профиля, тот может подставить любой логин.
bool        Session_Save(const std::string& login);   // true = записано
std::string Session_Load();    // "" = сохранённого входа нет (или такого аккаунта уже нет)
void        Session_Clear();   // при выходе из аккаунта

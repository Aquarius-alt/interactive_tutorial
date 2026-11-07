#pragma once

#include <string>

// Shared runtime language flag. The language can be changed from MENU -> INTERFACE.
inline bool& UiRussianFlag() {
    static bool russian = false;
    return russian;
}

inline const char* UiText(const char* english, const char* russian) {
    return UiRussianFlag() ? russian : english;
}

// Translate application-generated status/error text. User-created content, names,
// account logins and repository titles are intentionally returned unchanged.
inline std::string UiMessage(const std::string& english) {
    if (!UiRussianFlag()) return english;
    if (english == "ENTER LOGIN") return "ВВЕДИТЕ ЛОГИН";
    if (english == "ENTER PASSWORD") return "ВВЕДИТЕ ПАРОЛЬ";
    if (english == "WRONG LOGIN OR PASSWORD") return "НЕВЕРНЫЙ ЛОГИН ИЛИ ПАРОЛЬ";
    if (english == "ENTER KEYWORD") return "ВВЕДИТЕ КЛЮЧЕВОЕ СЛОВО";
    if (english == "KEYWORD NOT FOUND") return "КЛЮЧЕВОЕ СЛОВО НЕ НАЙДЕНО";
    if (english == "LOGIN TOO SHORT") return "СЛИШКОМ КОРОТКИЙ ЛОГИН";
    if (english == "LOGIN ALREADY TAKEN") return "ЛОГИН УЖЕ ЗАНЯТ";
    if (english == "PASSWORD TOO SHORT") return "СЛИШКОМ КОРОТКИЙ ПАРОЛЬ";
    if (english == "PASSWORDS DO NOT MATCH") return "ПАРОЛИ НЕ СОВПАДАЮТ";
    if (english == "SAVE FAILED") return "ОШИБКА СОХРАНЕНИЯ";
    if (english == "CANNOT LOAD IMAGE") return "НЕ УДАЛОСЬ ЗАГРУЗИТЬ ИЗОБРАЖЕНИЕ";
    if (english == "WRONG PASSWORD") return "НЕВЕРНЫЙ ПАРОЛЬ";
    if (english == "CANNOT OPEN FOLDER") return "НЕ УДАЛОСЬ ОТКРЫТЬ ПАПКУ";
    if (english == "CANNOT SAVE TASK") return "НЕ УДАЛОСЬ СОХРАНИТЬ ЗАДАНИЕ";
    if (english == "TASK HAS NO QUESTIONS") return "В ЗАДАНИИ НЕТ ВОПРОСОВ";
    if (english == "MAX 10 FILES") return "МАКСИМУМ 10 ФАЙЛОВ";
    if (english == "ENTER TITLE") return "ВВЕДИТЕ ЗАГОЛОВОК";
    if (english == "ENTER NAME") return "ВВЕДИТЕ НАЗВАНИЕ";
    if (english == "ENTER QUESTION") return "ВВЕДИТЕ ВОПРОС";
    if (english == "MAIN.PY NOT FOUND") return "MAIN.PY НЕ НАЙДЕН В ПАПКЕ";
    if (english == "ENTRY FILE NOT FOUND") return "ФАЙЛ ЗАПУСКА ИГРЫ НЕ НАЙДЕН";
    if (english == "BAD ENTRY FILE") return "НЕВЕРНЫЙ ФАЙЛ ЗАПУСКА В MANIFEST";
    if (english == "GAME FOLDER TOO LARGE") return "ПАПКА ИГРЫ СЛИШКОМ БОЛЬШАЯ";
    if (english == "GAME FOLDER NOT FOUND") return "ПАПКА ИГРЫ НЕ НАЙДЕНА";
    if (english == "ADD A QUESTION") return "ДОБАВЬТЕ ВОПРОС";
    if (english == "LOCAL") return "ЛОКАЛ.";
    if (english == "STORAGE") return "ХРАН.";
    if (english == "DOCS") return "ДОКИ";
    if (english == "FOLDER") return "ПАПКА";
    if (english == "FILES") return "ФАЙЛЫ";
    return english;
}

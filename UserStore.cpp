#include "UserStore.h"
#include <SDL3/SDL.h>
#include <fstream>
#include <cctype>
#include <cstdlib>
#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace {
    const ULONGLONG kIterations = 120000;   // чем больше, тем дороже перебор
    const ULONG kSaltLen = 16, kHashLen = 32;
    const ULONGLONG kMaxIterations = 10000000;   // защита от битой строки в файле

    std::string Lower(std::string s) {
        for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    std::string Hex(const unsigned char* d, size_t n) {
        static const char* k = "0123456789abcdef";
        std::string s;
        s.reserve(n * 2);
        for (size_t i = 0; i < n; ++i) {
            s.push_back(k[d[i] >> 4]);
            s.push_back(k[d[i] & 15]);
        }
        return s;
    }

    int HexVal(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    bool Unhex(const std::string& s, std::vector<unsigned char>& out) {
        out.clear();
        if (s.size() % 2) return false;
        out.reserve(s.size() / 2);
        for (size_t i = 0; i < s.size(); i += 2) {
            const int hi = HexVal(s[i]), lo = HexVal(s[i + 1]);
            if (hi < 0 || lo < 0) return false;
            out.push_back(static_cast<unsigned char>(hi * 16 + lo));
        }
        return true;
    }

    std::vector<std::string> Split(const std::string& line, char sep) {
        std::vector<std::string> parts;
        size_t start = 0;
        for (;;) {
            const size_t p = line.find(sep, start);
            if (p == std::string::npos) { parts.push_back(line.substr(start)); break; }
            parts.push_back(line.substr(start, p - start));
            start = p + 1;
        }
        return parts;
    }

    // PBKDF2-HMAC-SHA256: secret + соль -> hash (kHashLen байт)
    bool Derive(const std::string& secret, const unsigned char* salt, ULONG saltLen,
                ULONGLONG iterations, unsigned char* hash) {
        BCRYPT_ALG_HANDLE alg = nullptr;
        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr,
                                        BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0)
            return false;

        const NTSTATUS st = BCryptDeriveKeyPBKDF2(
            alg,
            reinterpret_cast<PUCHAR>(const_cast<char*>(secret.data())), static_cast<ULONG>(secret.size()),
            const_cast<PUCHAR>(salt), saltLen, iterations, hash, kHashLen, 0);
        BCryptCloseAlgorithmProvider(alg, 0);
        return st >= 0;
    }

    // PBKDF2-HMAC-SHA256 со случайной солью -> "соль:хэш" (hex)
    bool HashSecret(const std::string& secret, std::string& out) {
        unsigned char salt[kSaltLen], hash[kHashLen];
        if (BCryptGenRandom(nullptr, salt, kSaltLen, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
            return false;
        if (!Derive(secret, salt, kSaltLen, kIterations, hash))
            return false;

        out = Hex(salt, kSaltLen) + ":" + Hex(hash, kHashLen);
        SecureZeroMemory(hash, sizeof(hash));
        return true;
    }

    // проверка секрета по записи "соль:хэш" (сравнение за постоянное время)
    bool VerifySecret(const std::string& secret, const std::string& saltHash, ULONGLONG iterations) {
        if (iterations == 0 || iterations > kMaxIterations) return false;
        const size_t colon = saltHash.find(':');
        if (colon == std::string::npos) return false;

        std::vector<unsigned char> salt, want;
        if (!Unhex(saltHash.substr(0, colon), salt) || !Unhex(saltHash.substr(colon + 1), want))
            return false;
        if (salt.empty() || want.size() != kHashLen) return false;

        unsigned char got[kHashLen];
        if (!Derive(secret, salt.data(), static_cast<ULONG>(salt.size()), iterations, got))
            return false;

        unsigned char diff = 0;
        for (ULONG i = 0; i < kHashLen; ++i) diff |= static_cast<unsigned char>(got[i] ^ want[i]);
        SecureZeroMemory(got, sizeof(got));
        return diff == 0;
    }

    // DPAPI: шифрует пароль ключом текущего пользователя Windows -> hex
    bool ProtectSecret(const std::string& secret, std::string& outHex) {
        DATA_BLOB in, out;
        in.cbData = static_cast<DWORD>(secret.size());
        in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(secret.data()));
        out.cbData = 0;
        out.pbData = nullptr;
        if (!CryptProtectData(&in, L"myproject", nullptr, nullptr, nullptr,
                              CRYPTPROTECT_UI_FORBIDDEN, &out))
            return false;
        outHex = Hex(out.pbData, out.cbData);
        LocalFree(out.pbData);
        return true;
    }

    bool UnprotectSecret(const std::string& hex, std::string& out) {
        std::vector<unsigned char> raw;
        if (!Unhex(hex, raw) || raw.empty()) return false;

        DATA_BLOB in, res;
        in.cbData = static_cast<DWORD>(raw.size());
        in.pbData = raw.data();
        res.cbData = 0;
        res.pbData = nullptr;
        if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                                CRYPTPROTECT_UI_FORBIDDEN, &res))
            return false;
        out.assign(reinterpret_cast<const char*>(res.pbData), res.cbData);
        SecureZeroMemory(res.pbData, res.cbData);
        LocalFree(res.pbData);
        return true;
    }
}

std::string UserStore_Path() {
    char* p = SDL_GetPrefPath("myproject", "login");   // создаёт папку, если её нет
    std::string s = p ? p : "";
    SDL_free(p);
    return s.empty() ? s : s + "users.dat";
}

bool UserStore_Exists(const std::string& login) {
    const std::string path = UserStore_Path();
    if (path.empty()) return false;

    std::ifstream in(path, std::ios::binary);
    const std::string want = Lower(login);
    std::string line;
    while (std::getline(in, line)) {
        const size_t tab = line.find('\t');
        if (tab != std::string::npos && Lower(line.substr(0, tab)) == want)
            return true;
    }
    return false;
}

bool UserStore_Add(const std::string& login, const std::string& password, const std::string& keyword) {
    const std::string path = UserStore_Path();
    if (path.empty() || UserStore_Exists(login)) return false;

    std::string pw, kw, blob;
    if (!HashSecret(password, pw)) return false;
    if (!HashSecret(Lower(keyword), kw)) return false;   // слово без учёта регистра
    if (!ProtectSecret(password, blob)) blob.clear();    // не вышло: аккаунт без восстановления пароля

    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out) return false;
    out << login << '\t' << pw << '\t' << kw << '\t' << kIterations;
    if (!blob.empty()) out << '\t' << blob;
    out << '\n';
    out.flush();
    return out.good();
}

bool UserStore_Verify(const std::string& login, const std::string& password, std::string* canonicalLogin) {
    const std::string path = UserStore_Path();
    if (path.empty() || login.empty() || password.empty()) return false;

    std::ifstream in(path, std::ios::binary);
    const std::string want = Lower(login);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        const std::vector<std::string> f = Split(line, '\t');
        if (f.size() < 4 || Lower(f[0]) != want) continue;

        const ULONGLONG iterations = std::strtoull(f[3].c_str(), nullptr, 10);
        if (!VerifySecret(password, f[1], iterations)) return false;   // логин уникален: дальше не ищем
        if (canonicalLogin) *canonicalLogin = f[0];
        return true;
    }
    return false;
}

std::vector<RecoveredAccount> UserStore_Recover(const std::string& keyword) {
    std::vector<RecoveredAccount> found;
    const std::string path = UserStore_Path();
    if (path.empty() || keyword.empty()) return found;

    std::ifstream in(path, std::ios::binary);
    const std::string want = Lower(keyword);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        const std::vector<std::string> f = Split(line, '\t');
        if (f.size() < 4) continue;

        const ULONGLONG iterations = std::strtoull(f[3].c_str(), nullptr, 10);
        if (!VerifySecret(want, f[2], iterations)) continue;

        RecoveredAccount acc;
        acc.login = f[0];
        if (f.size() >= 5 && UnprotectSecret(f[4], acc.password))
            acc.hasPassword = true;
        found.push_back(std::move(acc));
    }
    return found;
}

// ---------- запомненный вход ----------

namespace {
    std::string SessionPath() {
        char* p = SDL_GetPrefPath("myproject", "login");
        std::string s = p ? p : "";
        SDL_free(p);
        return s.empty() ? s : s + "session.dat";
    }

    // логин так, как он записан в users.dat; пусто, если такого аккаунта нет
    std::string CanonicalLogin(const std::string& login) {
        const std::string path = UserStore_Path();
        if (path.empty() || login.empty()) return "";

        std::ifstream in(path, std::ios::binary);
        const std::string want = Lower(login);
        std::string line;
        while (std::getline(in, line)) {
            const size_t tab = line.find('\t');
            if (tab != std::string::npos && Lower(line.substr(0, tab)) == want)
                return line.substr(0, tab);
        }
        return "";
    }
}

bool Session_Save(const std::string& login) {
    const std::string path = SessionPath();
    if (path.empty()) return false;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << login;
    out.flush();
    return out.good();
}

std::string Session_Load() {
    const std::string path = SessionPath();
    if (path.empty()) return "";

    std::ifstream in(path, std::ios::binary);
    std::string line;
    if (!std::getline(in, line)) return "";
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return CanonicalLogin(line);
}

void Session_Clear() {
    const std::string path = SessionPath();
    if (!path.empty()) std::remove(path.c_str());
}

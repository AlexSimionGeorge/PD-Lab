// Tema 1 - Enumerare valori dintr-o subcheie Registry folosind Win32 API (MSVC).
// Compilare: cl /EHsc /W4 tema1.cpp advapi32.lib
// Utilizare:  tema1.exe <HIVE> <subkey_path>
//   HIVE: HKLM | HKCU | HKCR | HKU | HKCC
// Exemplu:    tema1.exe HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static HKEY ParseHive(const char* name) {
    if (_stricmp(name, "HKLM") == 0 || _stricmp(name, "HKEY_LOCAL_MACHINE") == 0) return HKEY_LOCAL_MACHINE;
    if (_stricmp(name, "HKCU") == 0 || _stricmp(name, "HKEY_CURRENT_USER") == 0) return HKEY_CURRENT_USER;
    if (_stricmp(name, "HKCR") == 0 || _stricmp(name, "HKEY_CLASSES_ROOT") == 0) return HKEY_CLASSES_ROOT;
    if (_stricmp(name, "HKU")  == 0 || _stricmp(name, "HKEY_USERS") == 0)        return HKEY_USERS;
    if (_stricmp(name, "HKCC") == 0 || _stricmp(name, "HKEY_CURRENT_CONFIG") == 0) return HKEY_CURRENT_CONFIG;
    return NULL;
}

static const char* TypeName(DWORD type) {
    switch (type) {
        case REG_NONE:                       return "REG_NONE";
        case REG_SZ:                         return "REG_SZ";
        case REG_EXPAND_SZ:                  return "REG_EXPAND_SZ";
        case REG_BINARY:                     return "REG_BINARY";
        case REG_DWORD:                      return "REG_DWORD";
        case REG_DWORD_BIG_ENDIAN:           return "REG_DWORD_BIG_ENDIAN";
        case REG_LINK:                       return "REG_LINK";
        case REG_MULTI_SZ:                   return "REG_MULTI_SZ";
        case REG_RESOURCE_LIST:              return "REG_RESOURCE_LIST";
        case REG_FULL_RESOURCE_DESCRIPTOR:   return "REG_FULL_RESOURCE_DESCRIPTOR";
        case REG_RESOURCE_REQUIREMENTS_LIST: return "REG_RESOURCE_REQUIREMENTS_LIST";
        case REG_QWORD:                      return "REG_QWORD";
        default:                             return "UNKNOWN";
    }
}

static void PrintHex(const BYTE* data, DWORD size) {
    const DWORD kMax = 64;
    DWORD n = size < kMax ? size : kMax;
    for (DWORD i = 0; i < n; ++i) {
        std::printf("%02X ", data[i]);
    }
    if (size > kMax) std::printf("... (%lu bytes total)", size);
}

static void PrintMultiSz(const char* data, DWORD size) {
    // MULTI_SZ: sequence of null-terminated strings, ended by an extra null.
    const char* p = data;
    const char* end = data + size;
    bool first = true;
    while (p < end && *p) {
        if (!first) std::printf(" | ");
        std::printf("\"%s\"", p);
        first = false;
        p += std::strlen(p) + 1;
    }
    if (first) std::printf("(empty)");
}

static void PrintValue(DWORD type, const BYTE* data, DWORD size) {
    switch (type) {
        case REG_SZ:
        case REG_EXPAND_SZ:
        case REG_LINK: {
            // Data may or may not be null-terminated; print safely.
            std::string s(reinterpret_cast<const char*>(data), size);
            // Strip trailing nulls.
            while (!s.empty() && s.back() == '\0') s.pop_back();
            std::printf("\"%s\"", s.c_str());
            break;
        }
        case REG_DWORD:
        case REG_DWORD_BIG_ENDIAN: {
            if (size >= sizeof(DWORD)) {
                DWORD v = *reinterpret_cast<const DWORD*>(data);
                if (type == REG_DWORD_BIG_ENDIAN) v = _byteswap_ulong(v);
                std::printf("%lu (0x%08lX)", v, v);
            } else {
                std::printf("(truncated dword)");
            }
            break;
        }
        case REG_QWORD: {
            if (size >= sizeof(ULONGLONG)) {
                ULONGLONG v = *reinterpret_cast<const ULONGLONG*>(data);
                std::printf("%llu (0x%016llX)", v, v);
            } else {
                std::printf("(truncated qword)");
            }
            break;
        }
        case REG_MULTI_SZ: {
            PrintMultiSz(reinterpret_cast<const char*>(data), size);
            break;
        }
        case REG_BINARY:
        default: {
            PrintHex(data, size);
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::fprintf(stderr, "Utilizare: %s <HIVE> <subkey_path>\n", argv[0]);
        std::fprintf(stderr, "  HIVE: HKLM | HKCU | HKCR | HKU | HKCC\n");
        std::fprintf(stderr, "Exemplu:   %s HKLM \"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\"\n", argv[0]);
        return 1;
    }

    HKEY hive = ParseHive(argv[1]);
    if (hive == NULL) {
        std::fprintf(stderr, "Hive necunoscut: %s\n", argv[1]);
        return 1;
    }
    const char* subkey = argv[2];

    HKEY hKey = NULL;
    LONG rc = RegOpenKeyExA(hive, subkey, 0, KEY_READ, &hKey);
    if (rc != ERROR_SUCCESS) {
        std::fprintf(stderr, "RegOpenKeyExA a esuat pentru \"%s\\%s\" (cod: %ld)\n",
                     argv[1], subkey, rc);
        return 2;
    }

    DWORD cValues = 0, cbMaxName = 0, cbMaxData = 0;
    rc = RegQueryInfoKeyA(hKey, NULL, NULL, NULL, NULL, NULL, NULL,
                          &cValues, &cbMaxName, &cbMaxData, NULL, NULL);
    if (rc != ERROR_SUCCESS) {
        std::fprintf(stderr, "RegQueryInfoKeyA a esuat (cod: %ld)\n", rc);
        RegCloseKey(hKey);
        return 3;
    }

    std::printf("Cheie: %s\\%s\n", argv[1], subkey);
    std::printf("Numar de valori: %lu\n", cValues);
    std::printf("--------------------------------------------------\n");

    if (cValues == 0) {
        std::printf("(subcheia nu contine valori)\n");
        RegCloseKey(hKey);
        return 0;
    }

    std::vector<char> nameBuf(cbMaxName + 1);
    std::vector<BYTE> dataBuf(cbMaxData + 1);

    for (DWORD i = 0; i < cValues; ++i) {
        DWORD nameLen = static_cast<DWORD>(nameBuf.size());
        DWORD dataLen = static_cast<DWORD>(dataBuf.size());
        DWORD type = 0;

        rc = RegEnumValueA(hKey, i, nameBuf.data(), &nameLen, NULL, &type,
                           dataBuf.data(), &dataLen);
        if (rc != ERROR_SUCCESS) {
            std::fprintf(stderr, "RegEnumValueA index %lu a esuat (cod: %ld)\n", i, rc);
            continue;
        }

        const char* displayName = (nameLen == 0) ? "(Default)" : nameBuf.data();
        std::printf("[%lu] %s\n", i, displayName);
        std::printf("    Tip:    %s\n", TypeName(type));
        std::printf("    Marime: %lu bytes\n", dataLen);
        std::printf("    Data:   ");
        PrintValue(type, dataBuf.data(), dataLen);
        std::printf("\n\n");
    }

    RegCloseKey(hKey);
    return 0;
}

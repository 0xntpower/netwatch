#include "stdafx.h"
#include "Settings.h"
#include "StringConversion.h"

#include <cstring>

#pragma comment(lib, "advapi32.lib")

namespace netwatch::util {

namespace {

// Opened and closed per call. This runs a handful of times at startup and
// shutdown, never in the refresh path, so keeping a handle around would buy
// nothing and would need lifetime management.
HKEY OpenForRead() {
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, Settings::kKeyPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return nullptr;
    }
    return key;
}

HKEY OpenForWrite() {
    HKEY key = nullptr;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, Settings::kKeyPath, 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return nullptr;
    }
    return key;
}

} // namespace

int Settings::GetInt(const wchar_t* name, int fallback) {
    HKEY key = OpenForRead();
    if (key == nullptr) {
        return fallback;
    }

    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    const LONG result = ::RegQueryValueExW(key, name, nullptr, &type,
        reinterpret_cast<LPBYTE>(&value), &size);
    ::RegCloseKey(key);

    if (result != ERROR_SUCCESS || type != REG_DWORD) {
        return fallback;
    }
    return static_cast<int>(value);
}

void Settings::SetInt(const wchar_t* name, int value) {
    HKEY key = OpenForWrite();
    if (key == nullptr) {
        return;
    }
    const DWORD stored = static_cast<DWORD>(value);
    ::RegSetValueExW(key, name, 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&stored), sizeof(stored));
    ::RegCloseKey(key);
}

std::string Settings::GetString(const wchar_t* name, const std::string& fallback) {
    HKEY key = OpenForRead();
    if (key == nullptr) {
        return fallback;
    }

    DWORD size = 0;
    DWORD type = 0;
    if (::RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
        type != REG_SZ || size < sizeof(wchar_t)) {
        ::RegCloseKey(key);
        return fallback;
    }

    std::wstring buffer(size / sizeof(wchar_t), L'\0');
    const LONG result = ::RegQueryValueExW(key, name, nullptr, nullptr,
        reinterpret_cast<LPBYTE>(buffer.data()), &size);
    ::RegCloseKey(key);

    if (result != ERROR_SUCCESS) {
        return fallback;
    }

    // RegQueryValueEx reports the size including any trailing null it stored.
    buffer.resize(::wcsnlen(buffer.c_str(), buffer.size()));
    return StringConversion::WideToNarrow(buffer);
}

void Settings::SetString(const wchar_t* name, const std::string& value) {
    HKEY key = OpenForWrite();
    if (key == nullptr) {
        return;
    }
    const std::wstring wide = StringConversion::NarrowToWide(value);
    ::RegSetValueExW(key, name, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(wide.c_str()),
        static_cast<DWORD>((wide.size() + 1) * sizeof(wchar_t)));
    ::RegCloseKey(key);
}

std::vector<int> Settings::GetIntArray(const wchar_t* name) {
    HKEY key = OpenForRead();
    if (key == nullptr) {
        return {};
    }

    DWORD size = 0;
    DWORD type = 0;
    if (::RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
        type != REG_BINARY || size == 0 || (size % sizeof(int)) != 0) {
        ::RegCloseKey(key);
        return {};
    }

    std::vector<int> values(size / sizeof(int));
    const LONG result = ::RegQueryValueExW(key, name, nullptr, nullptr,
        reinterpret_cast<LPBYTE>(values.data()), &size);
    ::RegCloseKey(key);

    if (result != ERROR_SUCCESS) {
        return {};
    }
    return values;
}

void Settings::SetIntArray(const wchar_t* name, const std::vector<int>& values) {
    if (values.empty()) {
        return;
    }
    SetBinary(name, values.data(), static_cast<DWORD>(values.size() * sizeof(int)));
}

bool Settings::GetBinary(const wchar_t* name, void* buffer, unsigned long size) {
    HKEY key = OpenForRead();
    if (key == nullptr) {
        return false;
    }

    DWORD actual = size;
    DWORD type = 0;
    const LONG result = ::RegQueryValueExW(key, name, nullptr, &type,
        static_cast<LPBYTE>(buffer), &actual);
    ::RegCloseKey(key);

    // An exact size match matters here: the only binary value is a
    // WINDOWPLACEMENT, and a partial read would be garbage.
    return result == ERROR_SUCCESS && type == REG_BINARY && actual == size;
}

void Settings::SetBinary(const wchar_t* name, const void* buffer, unsigned long size) {
    HKEY key = OpenForWrite();
    if (key == nullptr) {
        return;
    }
    ::RegSetValueExW(key, name, 0, REG_BINARY,
        static_cast<const BYTE*>(buffer), size);
    ::RegCloseKey(key);
}

} // namespace netwatch::util

// Settings.h : persisted user state
//
// Window placement, column layout, sort order and the option toggles all reset
// on every launch otherwise. Remembering them is what every long-lived Windows
// tool does, and its absence is felt the second time the app is opened.
//
// HKCU only, so nothing here needs elevation and nothing is written outside the
// user's own hive.
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>

namespace netwatch::util {

class Settings {
public:
    static constexpr const wchar_t* kKeyPath = L"Software\\ntpower\\NetWatch";

    static int GetInt(const wchar_t* name, int fallback);
    static void SetInt(const wchar_t* name, int value);

    static std::string GetString(const wchar_t* name, const std::string& fallback = {});
    static void SetString(const wchar_t* name, const std::string& value);

    // Small integer arrays, used for column widths and visibility.
    static std::vector<int> GetIntArray(const wchar_t* name);
    static void SetIntArray(const wchar_t* name, const std::vector<int>& values);

    static bool GetBinary(const wchar_t* name, void* buffer, unsigned long size);
    static void SetBinary(const wchar_t* name, const void* buffer, unsigned long size);
};

} // namespace netwatch::util

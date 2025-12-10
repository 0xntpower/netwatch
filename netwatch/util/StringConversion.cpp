#include "stdafx.h"
#include "StringConversion.h"

namespace netwatch::util {

std::string StringConversion::WideToNarrow(const std::wstring& wideStr) {
    if (wideStr.empty()) {
        return std::string();
    }

    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(),
        static_cast<int>(wideStr.length()), nullptr, 0, nullptr, nullptr);

    if (sizeNeeded <= 0) {
        return std::string();
    }

    std::string narrowStr(sizeNeeded, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(),
        static_cast<int>(wideStr.length()), &narrowStr[0], sizeNeeded, nullptr, nullptr);

    return narrowStr;
}

std::string StringConversion::WideToNarrow(const wchar_t* wideStr) {
    if (wideStr == nullptr || wideStr[0] == L'\0') {
        return std::string();
    }

    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1,
        nullptr, 0, nullptr, nullptr);

    if (sizeNeeded <= 0) {
        return std::string();
    }

    std::string narrowStr(sizeNeeded - 1, '\0'); // -1 to exclude null terminator
    WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, &narrowStr[0],
        sizeNeeded, nullptr, nullptr);

    return narrowStr;
}

std::wstring StringConversion::NarrowToWide(const std::string& narrowStr) {
    if (narrowStr.empty()) {
        return std::wstring();
    }

    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, narrowStr.c_str(),
        static_cast<int>(narrowStr.length()), nullptr, 0);

    if (sizeNeeded <= 0) {
        return std::wstring();
    }

    std::wstring wideStr(sizeNeeded, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, narrowStr.c_str(),
        static_cast<int>(narrowStr.length()), &wideStr[0], sizeNeeded);

    return wideStr;
}

std::wstring StringConversion::NarrowToWide(const char* narrowStr) {
    if (narrowStr == nullptr || narrowStr[0] == '\0') {
        return std::wstring();
    }

    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, narrowStr, -1, nullptr, 0);

    if (sizeNeeded <= 0) {
        return std::wstring();
    }

    std::wstring wideStr(sizeNeeded - 1, L'\0'); // -1 to exclude null terminator
    MultiByteToWideChar(CP_UTF8, 0, narrowStr, -1, &wideStr[0], sizeNeeded);

    return wideStr;
}

} // namespace netwatch::util

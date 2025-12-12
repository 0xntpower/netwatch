#pragma once

#include <string>
#include <Windows.h>

namespace netwatch::util {

class StringConversion {
public:
    [[nodiscard]] static std::string WideToNarrow(const std::wstring& wideStr);
    [[nodiscard]] static std::string WideToNarrow(const wchar_t* wideStr);
    [[nodiscard]] static std::wstring NarrowToWide(const std::string& narrowStr);
    [[nodiscard]] static std::wstring NarrowToWide(const char* narrowStr);
};

} // namespace netwatch::util

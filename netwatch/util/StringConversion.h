#pragma once

#include <string>
#include <Windows.h>

namespace netwatch::util {

/// <summary>
/// Utility class for string conversions between wide and narrow strings
/// </summary>
class StringConversion {
public:
    /// <summary>
    /// Converts a wide string (TCHAR/wchar_t) to a narrow string (UTF-8)
    /// </summary>
    /// <param name="wideStr">The wide string to convert</param>
    /// <returns>UTF-8 encoded narrow string</returns>
    static std::string WideToNarrow(const std::wstring& wideStr);

    /// <summary>
    /// Converts a wide string (TCHAR/wchar_t) to a narrow string (UTF-8)
    /// </summary>
    /// <param name="wideStr">The wide string to convert (null-terminated)</param>
    /// <returns>UTF-8 encoded narrow string</returns>
    static std::string WideToNarrow(const wchar_t* wideStr);

    /// <summary>
    /// Converts a narrow string (UTF-8) to a wide string (wchar_t)
    /// </summary>
    /// <param name="narrowStr">The narrow string to convert</param>
    /// <returns>Wide string</returns>
    static std::wstring NarrowToWide(const std::string& narrowStr);

    /// <summary>
    /// Converts a narrow string (UTF-8) to a wide string (wchar_t)
    /// </summary>
    /// <param name="narrowStr">The narrow string to convert (null-terminated)</param>
    /// <returns>Wide string</returns>
    static std::wstring NarrowToWide(const char* narrowStr);
};

} // namespace netwatch::util

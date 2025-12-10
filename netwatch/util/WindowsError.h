#pragma once

#include <string>
#include <Windows.h>

namespace netwatch::util {

/// <summary>
/// Utility class for formatting Windows error messages
/// </summary>
class WindowsError {
public:
    /// <summary>
    /// Gets the error message for a Windows error code
    /// </summary>
    /// <param name="errorCode">The error code (use GetLastError())</param>
    /// <returns>Formatted error message string</returns>
    static std::string GetErrorMessage(DWORD errorCode);

    /// <summary>
    /// Gets the error message for the last Windows error
    /// </summary>
    /// <returns>Formatted error message string</returns>
    static std::string GetLastErrorMessage();
};

} // namespace netwatch::util

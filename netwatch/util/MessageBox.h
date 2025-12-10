#pragma once

#include <string>
#include <Windows.h>

namespace netwatch::util {

/// <summary>
/// Utility class for displaying professional, Microsoft-style message boxes
/// </summary>
class MessageBox {
public:
    /// <summary>
    /// Shows an informational message
    /// </summary>
    static int ShowInfo(HWND hWnd, const std::string& message, const std::string& title = "NetWatch");

    /// <summary>
    /// Shows a warning message
    /// </summary>
    static int ShowWarning(HWND hWnd, const std::string& message, const std::string& title = "NetWatch");

    /// <summary>
    /// Shows an error message
    /// </summary>
    static int ShowError(HWND hWnd, const std::string& message, const std::string& title = "NetWatch");

    /// <summary>
    /// Shows a confirmation dialog (Yes/No)
    /// </summary>
    /// <returns>IDYES or IDNO</returns>
    static int ShowConfirm(HWND hWnd, const std::string& message, const std::string& title = "NetWatch");

    /// <summary>
    /// Shows a confirmation dialog (Yes/No) with warning icon
    /// </summary>
    /// <returns>IDYES or IDNO</returns>
    static int ShowConfirmWarning(HWND hWnd, const std::string& message, const std::string& title = "NetWatch");
};

} // namespace netwatch::util

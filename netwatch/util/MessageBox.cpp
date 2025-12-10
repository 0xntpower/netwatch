#include "stdafx.h"
#include "MessageBox.h"

namespace netwatch::util {

int MessageBox::ShowInfo(HWND hWnd, const std::string& message, const std::string& title) {
    return ::MessageBoxA(hWnd, message.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
}

int MessageBox::ShowWarning(HWND hWnd, const std::string& message, const std::string& title) {
    return ::MessageBoxA(hWnd, message.c_str(), title.c_str(), MB_OK | MB_ICONWARNING);
}

int MessageBox::ShowError(HWND hWnd, const std::string& message, const std::string& title) {
    return ::MessageBoxA(hWnd, message.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
}

int MessageBox::ShowConfirm(HWND hWnd, const std::string& message, const std::string& title) {
    return ::MessageBoxA(hWnd, message.c_str(), title.c_str(), MB_YESNO | MB_ICONQUESTION);
}

int MessageBox::ShowConfirmWarning(HWND hWnd, const std::string& message, const std::string& title) {
    return ::MessageBoxA(hWnd, message.c_str(), title.c_str(), MB_YESNO | MB_ICONWARNING);
}

} // namespace netwatch::util

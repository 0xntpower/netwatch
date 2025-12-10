#include "stdafx.h"
#include "WindowsError.h"

namespace netwatch::util {

std::string WindowsError::GetErrorMessage(DWORD errorCode) {
    if (errorCode == 0) {
        return "No error";
    }

    LPSTR messageBuffer = nullptr;
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0,
        nullptr
    );

    std::string message;
    if (size > 0 && messageBuffer != nullptr) {
        message = std::string(messageBuffer, size);

        // Remove trailing newlines and carriage returns
        while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
            message.pop_back();
        }
    }
    else {
        message = "Unknown error";
    }

    // Free the buffer
    if (messageBuffer != nullptr) {
        LocalFree(messageBuffer);
    }

    return message;
}

std::string WindowsError::GetLastErrorMessage() {
    return GetErrorMessage(GetLastError());
}

} // namespace netwatch::util

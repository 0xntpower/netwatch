#pragma once

#include <stdexcept>
#include <system_error>
#include <string>
#include <windows.h>

namespace netwatch::util {

struct AppError : std::runtime_error {
    std::error_code code;
    explicit AppError(std::string message, std::error_code ec = {})
        : std::runtime_error(std::move(message)), code(ec) {}
};

[[nodiscard]] inline HRESULT HResultFromErrorCode(const std::error_code& ec) noexcept {
    if (!ec) return S_OK;
    if (ec.category() == std::system_category())
        return HRESULT_FROM_WIN32(ec.value());
    return E_FAIL;
}

inline void ThrowIfFailedHRESULT(HRESULT hr, const char* msg = nullptr) {
    if (FAILED(hr)) {
        std::error_code ec(hr & 0x0000FFFF, std::system_category());
        throw AppError(msg ? msg : "HRESULT failed", ec);
    }
}

inline void ThrowIfWin32Error(DWORD errorCode, const char* msg = nullptr) {
    if (errorCode != ERROR_SUCCESS) {
        std::error_code ec(static_cast<int>(errorCode), std::system_category());
        throw AppError(msg ? msg : "Win32 API failed", ec);
    }
}

inline void ThrowLastWin32Error(const char* msg = nullptr) {
    DWORD errorCode = ::GetLastError();
    if (errorCode != ERROR_SUCCESS) {
        ThrowIfWin32Error(errorCode, msg);
    }
}

inline void LogError(const char* message, const std::error_code& ec = {}) noexcept {
    if (ec) {
        ::OutputDebugStringA("[ERROR] ");
        ::OutputDebugStringA(message);
        ::OutputDebugStringA(" - Error code: ");
        ::OutputDebugStringA(std::to_string(ec.value()).c_str());
        ::OutputDebugStringA("\n");
    } else {
        ::OutputDebugStringA("[ERROR] ");
        ::OutputDebugStringA(message);
        ::OutputDebugStringA("\n");
    }
}

inline void LogError(const char* context, const std::exception& e) noexcept {
    ::OutputDebugStringA("[ERROR] ");
    if (context) {
        ::OutputDebugStringA(context);
        ::OutputDebugStringA(": ");
    }
    ::OutputDebugStringA(e.what());
    ::OutputDebugStringA("\n");
}

} // namespace netwatch::util

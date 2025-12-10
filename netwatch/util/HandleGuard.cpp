#include "stdafx.h"
#include "HandleGuard.h"

namespace netwatch::util {

HandleGuard::HandleGuard(HANDLE h) noexcept : handle_(h) {}

HandleGuard::~HandleGuard() {
    if (Valid()) {
        CloseHandle(handle_);
    }
}

HandleGuard::HandleGuard(HandleGuard&& other) noexcept : handle_(other.Release()) {}

HandleGuard& HandleGuard::operator=(HandleGuard&& other) noexcept {
    if (this != &other) {
        Reset(other.Release());
    }
    return *this;
}

bool HandleGuard::Valid() const noexcept {
    return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
}

HANDLE HandleGuard::Get() const noexcept {
    return handle_;
}

HANDLE HandleGuard::Release() noexcept {
    HANDLE h = handle_;
    handle_ = nullptr;
    return h;
}

void HandleGuard::Reset(HANDLE h) noexcept {
    if (Valid()) {
        CloseHandle(handle_);
    }
    handle_ = h;
}

} // namespace netwatch::util

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../stdafx.h"

namespace netwatch::util {

class HandleGuard {
public:
    explicit HandleGuard(HANDLE h = nullptr) noexcept;
    ~HandleGuard();

    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;
    HandleGuard(HandleGuard&& other) noexcept;
    HandleGuard& operator=(HandleGuard&& other) noexcept;

    [[nodiscard]] bool Valid() const noexcept;
    [[nodiscard]] HANDLE Get() const noexcept;
    HANDLE Release() noexcept;
    void Reset(HANDLE h = nullptr) noexcept;

private:
    HANDLE handle_;
};

} // namespace netwatch::util

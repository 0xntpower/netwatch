#pragma once

#include <string>
#include <cstdint>

namespace netwatch::system {

class ProcessInfo {
public:
    [[nodiscard]] static std::string GetName(uint32_t pid);
};

} // namespace netwatch::system

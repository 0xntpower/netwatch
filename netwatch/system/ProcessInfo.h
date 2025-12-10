#pragma once

#include <string>
#include <cstdint>

namespace netwatch::system {

// Query process information such as name
class ProcessInfo {
public:
    static std::string GetName(uint32_t pid);
};

} // namespace netwatch::system

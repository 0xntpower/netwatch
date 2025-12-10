#pragma once

#include <string>
#include <cstdint>

namespace netwatch::system {

// Resolve process integrity level (System, High, Medium, Low)
class IntegrityLevelResolver {
public:
    static std::string Resolve(uint32_t pid);
};

} // namespace netwatch::system

#pragma once

#include <string>
#include <cstdint>

namespace netwatch::system {

// Resolve process integrity level (System, High, Medium, Low)
class IntegrityLevelResolver {
public:
    // Takes an already-open process handle so callers that need other
    // information about the same process do not have to open it twice.
    // The handle needs PROCESS_QUERY_LIMITED_INFORMATION.
    static std::string Resolve(HANDLE process);
};

} // namespace netwatch::system

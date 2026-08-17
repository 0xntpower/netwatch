// ProcessCache.h : per-process metadata, resolved once and reused
//
// The connection tables hand back one row per endpoint, and a single process
// routinely owns dozens of them. Resolving the process name, integrity level
// and image security flags per *row* meant three OpenProcess calls and a full
// memory-mapping of the executable for every endpoint, every refresh tick.
//
// None of that data changes for the lifetime of a process, so it is resolved
// once per PID and kept until the PID disappears.
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace netwatch::system {

struct ProcessDetails {
    std::string name;            // Base executable name, e.g. "chrome.exe"
    std::string integrityLevel;  // System / High / Medium / Low, or "Denied"
    std::string architecture;    // x64 / x86 / ARM64
    std::string executablePath;  // Full path, or empty when not readable
    std::string depStatus;       // Enabled / Disabled / N/A
    std::string aslrStatus;      // Enabled / Disabled / N/A
    std::string cfgStatus;       // Enabled / Disabled / N/A
    std::string safeSehStatus;   // Enabled / Disabled / N/A

    // Index into the shell's system image list, or -1 when the executable's
    // icon could not be resolved.
    int iconIndex = -1;
};

// The shell's shared small-icon image list. Valid once at least one icon has
// been resolved. Never destroy it: it belongs to the shell, not to us.
HIMAGELIST SystemSmallImageList();

class ProcessCache {
public:
    // Call at the start of each enumeration pass. Refreshes the PID to name map
    // from a single toolhelp snapshot, which unlike OpenProcess works for
    // almost every process without elevation.
    void BeginPass();

    // Resolve a PID, filling the cache on first sight.
    const ProcessDetails& Get(uint32_t pid);

    // Call at the end of a pass to drop processes that have exited.
    void EndPass();

private:
    struct Entry {
        ProcessDetails details;
        // Snapshot name recorded when the entry was filled. A PID that comes
        // back under a different name has been recycled, so the entry is stale.
        std::string identity;
        bool seenThisPass = false;
    };

    ProcessDetails Resolve(uint32_t pid) const;

    std::unordered_map<uint32_t, Entry> cache_;
    std::unordered_map<uint32_t, std::string> snapshotNames_;
    ProcessDetails unknown_;
};

} // namespace netwatch::system

#pragma once

#include <cstdint>
#include <string>

namespace netwatch::util {

// Connection statistics for TCP connections
struct ConnectionStats {
    uint64_t sentPackets = 0;
    uint64_t sentBytes = 0;
    uint64_t rcvdPackets = 0;
    uint64_t rcvdBytes = 0;
};

// Network endpoint entry with process and connection information
struct EndpointEntry {
    std::string processName;
    uint32_t pid = 0;
    std::string protocol;
    std::string integrityLevel;
    std::string localAddress;
    uint16_t localPort = 0;
    std::string remoteAddress;
    uint16_t remotePort = 0;
    std::string state;
    ConnectionStats stats;

    // Whether this endpoint is actually talking to a peer. Set by the
    // enumerators, which know the protocol semantics. Do not re-derive it by
    // string-matching remoteAddress: TCP uses "0.0.0.0" for an unbound peer and
    // UDP uses "*", so any single comparison silently misses one of them.
    bool hasRemote = false;

    // TCP byte counters require ESTATS collection, which needs elevation. False
    // means "not measured", which is different from a genuine zero.
    bool statsAvailable = false;

    // Security-related process information
    std::string architecture;      // x86 or x64
    std::string depStatus;         // DEP/NX status (Enabled/Disabled/N/A)
    std::string aslrStatus;        // ASLR status (Enabled/Disabled/N/A)
    std::string executablePath;    // Full path to the executable
    std::string cfgStatus;         // Control Flow Guard status (Enabled/Disabled/N/A)
    std::string safeSehStatus;     // SafeSEH status (Enabled/Disabled/N/A)

    // Index into the shell's system image list, or -1 when unresolved.
    int iconIndex = -1;

    // Display state for GUI color coding
    enum class DisplayState {
        Normal = 0,
        New = 1,
        Closed = 2,
        Modified = 3
    };
    DisplayState displayState = DisplayState::Normal;
};

} // namespace netwatch::util

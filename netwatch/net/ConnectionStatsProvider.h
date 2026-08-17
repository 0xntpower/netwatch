#pragma once

#include "../util/Types.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <tcpestats.h>

namespace netwatch::net {

// Per-connection TCP byte and segment counters.
//
// These come from ESTATS, which is off by default. Collection has to be enabled
// on each connection before anything can be read back, and enabling it needs
// administrator rights. Without that, every read returns zeros, which is why
// these columns used to look like real measurements of nothing.
class ConnectionStatsProvider {
public:
    // Returns false when ESTATS is unavailable, so the caller can tell "not
    // measured" apart from a connection that genuinely moved no bytes.
    static bool GetTcpStats(const MIB_TCPROW& row, util::ConnectionStats& out);
    static bool GetTcp6Stats(const MIB_TCP6ROW& row, util::ConnectionStats& out);

    // True once the process has been seen to successfully enable collection.
    // Used to label the Bytes columns rather than to gate the reads.
    static bool CollectionAvailable();
};

} // namespace netwatch::net

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

// Retrieve per-connection TCP statistics (requires admin privileges)
class ConnectionStatsProvider {
public:
    static util::ConnectionStats GetTcpStats(const MIB_TCPROW& row);
    static util::ConnectionStats GetTcp6Stats(const MIB_TCP6ROW& row);
};

} // namespace netwatch::net

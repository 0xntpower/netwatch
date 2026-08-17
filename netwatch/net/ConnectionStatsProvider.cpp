#include "stdafx.h"
#include "ConnectionStatsProvider.h"

#include <atomic>

namespace netwatch::net {

namespace {

// Set once the first SetPerTcpConnectionEStats call succeeds. Read by the UI to
// decide whether the Bytes columns are meaningful.
std::atomic<bool> g_collectionAvailable{false};

// Turn on ESTATS data collection for a connection. Reads return zeros until
// this succeeds, and it needs administrator rights.
//
// Collection only accumulates from the moment it is enabled, so a connection
// seen for the first time reports 0 on that tick and real numbers afterwards.
template <typename RowT, typename SetFn>
bool EnableCollection(RowT& row, SetFn setFn) {
    TCP_ESTATS_DATA_RW_v0 rw = {};
    rw.EnableCollection = TRUE;

    const DWORD result = setFn(&row, TcpConnectionEstatsData,
        reinterpret_cast<PUCHAR>(&rw), 0, sizeof(rw), 0);

    if (result == NO_ERROR) {
        g_collectionAvailable.store(true, std::memory_order_relaxed);
        return true;
    }
    return false;
}

void CopyRod(const TCP_ESTATS_DATA_ROD_v0& rod, util::ConnectionStats& out) {
    out.sentBytes = rod.DataBytesOut;
    out.rcvdBytes = rod.DataBytesIn;
    out.sentPackets = rod.DataSegsOut;
    out.rcvdPackets = rod.DataSegsIn;
}

} // namespace

bool ConnectionStatsProvider::CollectionAvailable() {
    return g_collectionAvailable.load(std::memory_order_relaxed);
}

bool ConnectionStatsProvider::GetTcpStats(const MIB_TCPROW& row, util::ConnectionStats& out) {
    MIB_TCPROW mutableRow = row;

    TCP_ESTATS_DATA_ROD_v0 rod = {};
    DWORD result = GetPerTcpConnectionEStats(
        &mutableRow, TcpConnectionEstatsData,
        nullptr, 0, 0,
        nullptr, 0, 0,
        reinterpret_cast<PUCHAR>(&rod), 0, sizeof(rod));

    if (result != NO_ERROR) {
        // Not collecting yet. Turn it on and try once more, so the counters
        // start accumulating for the next tick.
        if (!EnableCollection(mutableRow, SetPerTcpConnectionEStats)) {
            return false;
        }
        result = GetPerTcpConnectionEStats(
            &mutableRow, TcpConnectionEstatsData,
            nullptr, 0, 0,
            nullptr, 0, 0,
            reinterpret_cast<PUCHAR>(&rod), 0, sizeof(rod));
        if (result != NO_ERROR) {
            return false;
        }
    }

    g_collectionAvailable.store(true, std::memory_order_relaxed);
    CopyRod(rod, out);
    return true;
}

bool ConnectionStatsProvider::GetTcp6Stats(const MIB_TCP6ROW& row, util::ConnectionStats& out) {
    MIB_TCP6ROW mutableRow = row;

    TCP_ESTATS_DATA_ROD_v0 rod = {};
    DWORD result = GetPerTcp6ConnectionEStats(
        &mutableRow, TcpConnectionEstatsData,
        nullptr, 0, 0,
        nullptr, 0, 0,
        reinterpret_cast<PUCHAR>(&rod), 0, sizeof(rod));

    if (result != NO_ERROR) {
        if (!EnableCollection(mutableRow, SetPerTcp6ConnectionEStats)) {
            return false;
        }
        result = GetPerTcp6ConnectionEStats(
            &mutableRow, TcpConnectionEstatsData,
            nullptr, 0, 0,
            nullptr, 0, 0,
            reinterpret_cast<PUCHAR>(&rod), 0, sizeof(rod));
        if (result != NO_ERROR) {
            return false;
        }
    }

    g_collectionAvailable.store(true, std::memory_order_relaxed);
    CopyRod(rod, out);
    return true;
}

} // namespace netwatch::net

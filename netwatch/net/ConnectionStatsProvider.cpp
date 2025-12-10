#include "stdafx.h"
#include "ConnectionStatsProvider.h"

namespace netwatch::net {

util::ConnectionStats ConnectionStatsProvider::GetTcpStats(const MIB_TCPROW& row) {
    util::ConnectionStats stats;
    MIB_TCPROW mutableRow = row;

    TCP_ESTATS_DATA_ROD_v0 dataRod = {};
    ULONG rodSize = sizeof(dataRod);

    DWORD result = GetPerTcpConnectionEStats(
        &mutableRow, TcpConnectionEstatsData,
        nullptr, 0, 0,
        nullptr, 0, 0,
        reinterpret_cast<PUCHAR>(&dataRod), 0, rodSize
    );

    if (result == NO_ERROR) {
        stats.sentBytes = dataRod.DataBytesOut;
        stats.rcvdBytes = dataRod.DataBytesIn;
        stats.sentPackets = dataRod.DataSegsOut;
        stats.rcvdPackets = dataRod.DataSegsIn;
    }

    return stats;
}

util::ConnectionStats ConnectionStatsProvider::GetTcp6Stats(const MIB_TCP6ROW& row) {
    util::ConnectionStats stats;
    MIB_TCP6ROW mutableRow = row;

    TCP_ESTATS_DATA_ROD_v0 dataRod = {};
    ULONG rodSize = sizeof(dataRod);

    DWORD result = GetPerTcp6ConnectionEStats(
        &mutableRow, TcpConnectionEstatsData,
        nullptr, 0, 0,
        nullptr, 0, 0,
        reinterpret_cast<PUCHAR>(&dataRod), 0, rodSize
    );

    if (result == NO_ERROR) {
        stats.sentBytes = dataRod.DataBytesOut;
        stats.rcvdBytes = dataRod.DataBytesIn;
        stats.sentPackets = dataRod.DataSegsOut;
        stats.rcvdPackets = dataRod.DataSegsIn;
    }

    return stats;
}

} // namespace netwatch::net

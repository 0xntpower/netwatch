#include "stdafx.h"
#include "TcpEnumerator.h"
#include "ConnectionStatsProvider.h"
#include "../util/AddressFormatter.h"

#include <cstring>

namespace netwatch::net {

namespace {

// Copy the resolved per-process fields onto an endpoint row.
void ApplyProcessDetails(util::EndpointEntry& entry, const system::ProcessDetails& details) {
    entry.processName = details.name;
    entry.integrityLevel = details.integrityLevel;
    entry.architecture = details.architecture;
    entry.depStatus = details.depStatus;
    entry.aslrStatus = details.aslrStatus;
    entry.executablePath = details.executablePath;
    entry.cfgStatus = details.cfgStatus;
    entry.safeSehStatus = details.safeSehStatus;
    entry.iconIndex = details.iconIndex;
}

// A TCP row has a real peer once it is past LISTEN. Listening sockets report a
// remote address of 0.0.0.0 with port 0, which is not a peer.
bool TcpHasRemote(DWORD state, DWORD remotePort) {
    return state != MIB_TCP_STATE_LISTEN && remotePort != 0;
}

} // namespace

std::vector<util::EndpointEntry> TcpEnumerator::Enumerate(system::ProcessCache& processes) {
    std::vector<util::EndpointEntry> entries;
    EnumerateIPv4(entries, processes);
    EnumerateIPv6(entries, processes);
    return entries;
}

void TcpEnumerator::EnumerateIPv4(std::vector<util::EndpointEntry>& entries, system::ProcessCache& processes) {
    ULONG size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    auto* table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buffer.data());

    if (GetExtendedTcpTable(table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) {
        return;
    }

    entries.reserve(entries.size() + table->dwNumEntries);

    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const auto& row = table->table[i];

        util::EndpointEntry entry;
        entry.pid = row.dwOwningPid;
        entry.protocol = "TCP";
        entry.localAddress = util::AddressFormatter::FormatIPv4(row.dwLocalAddr);
        entry.localPort = ntohs(static_cast<uint16_t>(row.dwLocalPort));
        entry.remoteAddress = util::AddressFormatter::FormatIPv4(row.dwRemoteAddr);
        entry.remotePort = ntohs(static_cast<uint16_t>(row.dwRemotePort));
        entry.state = util::AddressFormatter::TcpStateToString(row.dwState);
        entry.hasRemote = TcpHasRemote(row.dwState, entry.remotePort);

        ApplyProcessDetails(entry, processes.Get(row.dwOwningPid));

        MIB_TCPROW statsRow = {};
        statsRow.dwLocalAddr = row.dwLocalAddr;
        statsRow.dwLocalPort = row.dwLocalPort;
        statsRow.dwRemoteAddr = row.dwRemoteAddr;
        statsRow.dwRemotePort = row.dwRemotePort;
        statsRow.dwState = row.dwState;
        entry.statsAvailable = ConnectionStatsProvider::GetTcpStats(statsRow, entry.stats);

        entries.push_back(std::move(entry));
    }
}

void TcpEnumerator::EnumerateIPv6(std::vector<util::EndpointEntry>& entries, system::ProcessCache& processes) {
    ULONG size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    auto* table = reinterpret_cast<MIB_TCP6TABLE_OWNER_PID*>(buffer.data());

    if (GetExtendedTcpTable(table, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) {
        return;
    }

    entries.reserve(entries.size() + table->dwNumEntries);

    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const auto& row = table->table[i];

        util::EndpointEntry entry;
        entry.pid = row.dwOwningPid;
        entry.protocol = "TCPv6";

        IN6_ADDR localAddr = {};
        std::memcpy(&localAddr, row.ucLocalAddr, sizeof(localAddr));
        entry.localAddress = util::AddressFormatter::FormatIPv6(localAddr);
        entry.localPort = ntohs(static_cast<uint16_t>(row.dwLocalPort));

        IN6_ADDR remoteAddr = {};
        std::memcpy(&remoteAddr, row.ucRemoteAddr, sizeof(remoteAddr));
        entry.remoteAddress = util::AddressFormatter::FormatIPv6(remoteAddr);
        entry.remotePort = ntohs(static_cast<uint16_t>(row.dwRemotePort));
        entry.state = util::AddressFormatter::TcpStateToString(row.dwState);
        entry.hasRemote = TcpHasRemote(row.dwState, entry.remotePort);

        ApplyProcessDetails(entry, processes.Get(row.dwOwningPid));

        MIB_TCP6ROW statsRow = {};
        std::memcpy(&statsRow.LocalAddr, row.ucLocalAddr, sizeof(IN6_ADDR));
        statsRow.dwLocalScopeId = row.dwLocalScopeId;
        statsRow.dwLocalPort = row.dwLocalPort;
        std::memcpy(&statsRow.RemoteAddr, row.ucRemoteAddr, sizeof(IN6_ADDR));
        statsRow.dwRemoteScopeId = row.dwRemoteScopeId;
        statsRow.dwRemotePort = row.dwRemotePort;
        statsRow.State = static_cast<MIB_TCP_STATE>(row.dwState);
        entry.statsAvailable = ConnectionStatsProvider::GetTcp6Stats(statsRow, entry.stats);

        entries.push_back(std::move(entry));
    }
}

} // namespace netwatch::net

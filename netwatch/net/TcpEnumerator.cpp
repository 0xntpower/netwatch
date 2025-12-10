#include "stdafx.h"
#include "TcpEnumerator.h"
#include "ConnectionStatsProvider.h"
#include "../system/IntegrityLevelResolver.h"
#include "../system/ProcessInfo.h"
#include "../util/AddressFormatter.h"
#include "../ProcessUtil.hpp"

#include <cstring>

namespace netwatch::net {

std::vector<util::EndpointEntry> TcpEnumerator::Enumerate() {
    std::vector<util::EndpointEntry> entries;
    EnumerateIPv4(entries);
    EnumerateIPv6(entries);
    return entries;
}

void TcpEnumerator::EnumerateIPv4(std::vector<util::EndpointEntry>& entries) {
    ULONG size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    auto* table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buffer.data());

    if (GetExtendedTcpTable(table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) {
        return;
    }

    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const auto& row = table->table[i];

        util::EndpointEntry entry;
        entry.pid = row.dwOwningPid;
        entry.processName = system::ProcessInfo::GetName(row.dwOwningPid);
        entry.protocol = "TCP";
        entry.integrityLevel = system::IntegrityLevelResolver::Resolve(row.dwOwningPid);
        entry.localAddress = util::AddressFormatter::FormatIPv4(row.dwLocalAddr);
        entry.localPort = ntohs(static_cast<uint16_t>(row.dwLocalPort));
        entry.remoteAddress = util::AddressFormatter::FormatIPv4(row.dwRemoteAddr);
        entry.remotePort = ntohs(static_cast<uint16_t>(row.dwRemotePort));
        entry.state = util::AddressFormatter::TcpStateToString(row.dwState);

        MIB_TCPROW statsRow = {};
        statsRow.dwLocalAddr = row.dwLocalAddr;
        statsRow.dwLocalPort = row.dwLocalPort;
        statsRow.dwRemoteAddr = row.dwRemoteAddr;
        statsRow.dwRemotePort = row.dwRemotePort;
        statsRow.dwState = row.dwState;
        entry.stats = ConnectionStatsProvider::GetTcpStats(statsRow);

        // Get security information
        auto secInfo = util::GetProcessSecurityInfo(row.dwOwningPid);
        entry.architecture = secInfo.architecture;
        entry.depStatus = secInfo.depStatus;
        entry.aslrStatus = secInfo.aslrStatus;
        entry.executablePath = secInfo.executablePath;
        entry.cfgStatus = secInfo.cfgStatus;
        entry.safeSehStatus = secInfo.safeSehStatus;

        entries.push_back(std::move(entry));
    }
}

void TcpEnumerator::EnumerateIPv6(std::vector<util::EndpointEntry>& entries) {
    ULONG size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    auto* table = reinterpret_cast<MIB_TCP6TABLE_OWNER_PID*>(buffer.data());

    if (GetExtendedTcpTable(table, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) {
        return;
    }

    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const auto& row = table->table[i];

        util::EndpointEntry entry;
        entry.pid = row.dwOwningPid;
        entry.processName = system::ProcessInfo::GetName(row.dwOwningPid);
        entry.protocol = "TCPv6";
        entry.integrityLevel = system::IntegrityLevelResolver::Resolve(row.dwOwningPid);
        IN6_ADDR localAddr = {};
        std::memcpy(&localAddr, row.ucLocalAddr, sizeof(localAddr));
        entry.localAddress = util::AddressFormatter::FormatIPv6(localAddr);
        entry.localPort = ntohs(static_cast<uint16_t>(row.dwLocalPort));
        IN6_ADDR remoteAddr = {};
        std::memcpy(&remoteAddr, row.ucRemoteAddr, sizeof(remoteAddr));
        entry.remoteAddress = util::AddressFormatter::FormatIPv6(remoteAddr);
        entry.remotePort = ntohs(static_cast<uint16_t>(row.dwRemotePort));
        entry.state = util::AddressFormatter::TcpStateToString(row.dwState);

        MIB_TCP6ROW statsRow = {};
        std::memcpy(&statsRow.LocalAddr, row.ucLocalAddr, sizeof(IN6_ADDR));
        statsRow.dwLocalScopeId = row.dwLocalScopeId;
        statsRow.dwLocalPort = row.dwLocalPort;
        std::memcpy(&statsRow.RemoteAddr, row.ucRemoteAddr, sizeof(IN6_ADDR));
        statsRow.dwRemoteScopeId = row.dwRemoteScopeId;
        statsRow.dwRemotePort = row.dwRemotePort;
        statsRow.State = static_cast<MIB_TCP_STATE>(row.dwState);
        entry.stats = ConnectionStatsProvider::GetTcp6Stats(statsRow);

        // Get security information
        auto secInfo = util::GetProcessSecurityInfo(row.dwOwningPid);
        entry.architecture = secInfo.architecture;
        entry.depStatus = secInfo.depStatus;
        entry.aslrStatus = secInfo.aslrStatus;
        entry.executablePath = secInfo.executablePath;
        entry.cfgStatus = secInfo.cfgStatus;
        entry.safeSehStatus = secInfo.safeSehStatus;

        entries.push_back(std::move(entry));
    }
}

} // namespace netwatch::net

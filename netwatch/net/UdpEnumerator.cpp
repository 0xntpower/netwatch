#include "stdafx.h"
#include "UdpEnumerator.h"
#include "../system/IntegrityLevelResolver.h"
#include "../system/ProcessInfo.h"
#include "../util/AddressFormatter.h"
#include "../ProcessUtil.hpp"

#include <cstring>

namespace netwatch::net {

std::vector<util::EndpointEntry> UdpEnumerator::Enumerate() {
    std::vector<util::EndpointEntry> entries;
    EnumerateIPv4(entries);
    EnumerateIPv6(entries);
    return entries;
}

void UdpEnumerator::EnumerateIPv4(std::vector<util::EndpointEntry>& entries) {
    ULONG size = 0;
    GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    auto* table = reinterpret_cast<MIB_UDPTABLE_OWNER_PID*>(buffer.data());

    if (GetExtendedUdpTable(table, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) != NO_ERROR) {
        return;
    }

    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const auto& row = table->table[i];

        util::EndpointEntry entry;
        entry.pid = row.dwOwningPid;
        entry.processName = system::ProcessInfo::GetName(row.dwOwningPid);
        entry.protocol = "UDP";
        entry.integrityLevel = system::IntegrityLevelResolver::Resolve(row.dwOwningPid);
        entry.localAddress = util::AddressFormatter::FormatIPv4(row.dwLocalAddr);
        entry.localPort = ntohs(static_cast<uint16_t>(row.dwLocalPort));
        entry.remoteAddress = "*";
        entry.remotePort = 0;
        entry.state = "";

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

void UdpEnumerator::EnumerateIPv6(std::vector<util::EndpointEntry>& entries) {
    ULONG size = 0;
    GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    auto* table = reinterpret_cast<MIB_UDP6TABLE_OWNER_PID*>(buffer.data());

    if (GetExtendedUdpTable(table, &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0) != NO_ERROR) {
        return;
    }

    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const auto& row = table->table[i];

        util::EndpointEntry entry;
        entry.pid = row.dwOwningPid;
        entry.processName = system::ProcessInfo::GetName(row.dwOwningPid);
        entry.protocol = "UDPv6";
        entry.integrityLevel = system::IntegrityLevelResolver::Resolve(row.dwOwningPid);
        IN6_ADDR localAddr = {};
        std::memcpy(&localAddr, row.ucLocalAddr, sizeof(localAddr));
        entry.localAddress = util::AddressFormatter::FormatIPv6(localAddr);
        entry.localPort = ntohs(static_cast<uint16_t>(row.dwLocalPort));
        entry.remoteAddress = "*";
        entry.remotePort = 0;
        entry.state = "";

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

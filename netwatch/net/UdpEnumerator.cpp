#include "stdafx.h"
#include "UdpEnumerator.h"
#include "../util/AddressFormatter.h"

#include <cstring>

namespace netwatch::net {

namespace {

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

} // namespace

std::vector<util::EndpointEntry> UdpEnumerator::Enumerate(system::ProcessCache& processes) {
    std::vector<util::EndpointEntry> entries;
    EnumerateIPv4(entries, processes);
    EnumerateIPv6(entries, processes);
    return entries;
}

void UdpEnumerator::EnumerateIPv4(std::vector<util::EndpointEntry>& entries, system::ProcessCache& processes) {
    ULONG size = 0;
    GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    auto* table = reinterpret_cast<MIB_UDPTABLE_OWNER_PID*>(buffer.data());

    if (GetExtendedUdpTable(table, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) != NO_ERROR) {
        return;
    }

    entries.reserve(entries.size() + table->dwNumEntries);

    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const auto& row = table->table[i];

        util::EndpointEntry entry;
        entry.pid = row.dwOwningPid;
        entry.protocol = "UDP";
        entry.localAddress = util::AddressFormatter::FormatIPv4(row.dwLocalAddr);
        entry.localPort = ntohs(static_cast<uint16_t>(row.dwLocalPort));
        entry.remoteAddress = "*";
        entry.remotePort = 0;
        entry.state = "";
        // UDP_TABLE_OWNER_PID carries no peer information, so every row here is
        // an unconnected endpoint as far as this view is concerned.
        entry.hasRemote = false;

        ApplyProcessDetails(entry, processes.Get(row.dwOwningPid));

        entries.push_back(std::move(entry));
    }
}

void UdpEnumerator::EnumerateIPv6(std::vector<util::EndpointEntry>& entries, system::ProcessCache& processes) {
    ULONG size = 0;
    GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    auto* table = reinterpret_cast<MIB_UDP6TABLE_OWNER_PID*>(buffer.data());

    if (GetExtendedUdpTable(table, &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0) != NO_ERROR) {
        return;
    }

    entries.reserve(entries.size() + table->dwNumEntries);

    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const auto& row = table->table[i];

        util::EndpointEntry entry;
        entry.pid = row.dwOwningPid;
        entry.protocol = "UDPv6";

        IN6_ADDR localAddr = {};
        std::memcpy(&localAddr, row.ucLocalAddr, sizeof(localAddr));
        entry.localAddress = util::AddressFormatter::FormatIPv6(localAddr);
        entry.localPort = ntohs(static_cast<uint16_t>(row.dwLocalPort));
        entry.remoteAddress = "*";
        entry.remotePort = 0;
        entry.state = "";
        entry.hasRemote = false;

        ApplyProcessDetails(entry, processes.Get(row.dwOwningPid));

        entries.push_back(std::move(entry));
    }
}

} // namespace netwatch::net

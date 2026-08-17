#pragma once

#include "../util/Types.h"
#include "../system/ProcessCache.h"

#include <vector>

namespace netwatch::net {

// Enumerate UDP endpoints (IPv4 and IPv6)
class UdpEnumerator {
public:
    static std::vector<util::EndpointEntry> Enumerate(system::ProcessCache& processes);

private:
    static void EnumerateIPv4(std::vector<util::EndpointEntry>& entries, system::ProcessCache& processes);
    static void EnumerateIPv6(std::vector<util::EndpointEntry>& entries, system::ProcessCache& processes);
};

} // namespace netwatch::net

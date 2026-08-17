#pragma once

#include "../util/Types.h"
#include "../system/ProcessCache.h"

#include <vector>

namespace netwatch::net {

// Enumerate TCP connections (IPv4 and IPv6)
class TcpEnumerator {
public:
    // The cache is supplied by the caller so a single refresh pass shares one
    // set of resolved process metadata across TCP and UDP.
    static std::vector<util::EndpointEntry> Enumerate(system::ProcessCache& processes);

private:
    static void EnumerateIPv4(std::vector<util::EndpointEntry>& entries, system::ProcessCache& processes);
    static void EnumerateIPv6(std::vector<util::EndpointEntry>& entries, system::ProcessCache& processes);
};

} // namespace netwatch::net

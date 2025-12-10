#pragma once

#include "../util/Types.h"

#include <vector>

namespace netwatch::net {

// Enumerate UDP endpoints (IPv4 and IPv6)
class UdpEnumerator {
public:
    static std::vector<util::EndpointEntry> Enumerate();

private:
    static void EnumerateIPv4(std::vector<util::EndpointEntry>& entries);
    static void EnumerateIPv6(std::vector<util::EndpointEntry>& entries);
};

} // namespace netwatch::net

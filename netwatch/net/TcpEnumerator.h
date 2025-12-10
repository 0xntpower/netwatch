#pragma once

#include "../util/Types.h"

#include <vector>

namespace netwatch::net {

// Enumerate TCP connections (IPv4 and IPv6)
class TcpEnumerator {
public:
    static std::vector<util::EndpointEntry> Enumerate();

private:
    static void EnumerateIPv4(std::vector<util::EndpointEntry>& entries);
    static void EnumerateIPv6(std::vector<util::EndpointEntry>& entries);
};

} // namespace netwatch::net

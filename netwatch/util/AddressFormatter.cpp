#include "stdafx.h"
#include "AddressFormatter.h"

#include <format>

namespace netwatch::util {

std::string AddressFormatter::FormatIPv4(uint32_t addr) {
    if (addr == 0) {
        return "0.0.0.0";
    }

    IN_ADDR inAddr;
    inAddr.S_un.S_addr = addr;
    char buffer[INET_ADDRSTRLEN] = {};
    InetNtopA(AF_INET, &inAddr, buffer, sizeof(buffer));
    return buffer;
}

std::string AddressFormatter::FormatIPv6(const IN6_ADDR& addr) {
    // Check if this is the IPv6 loopback address (::1)
    bool isLoopback = true;
    for (int i = 0; i < 15; i++) {
        if (addr.u.Byte[i] != 0) {
            isLoopback = false;
            break;
        }
    }
    if (isLoopback && addr.u.Byte[15] == 1) {
        return "127.0.0.1";
    }

    // Check if this is the IPv6 unspecified address (::)
    bool isUnspecified = true;
    for (int i = 0; i < 16; i++) {
        if (addr.u.Byte[i] != 0) {
            isUnspecified = false;
            break;
        }
    }
    if (isUnspecified) {
        return "0.0.0.0";
    }

    char buffer[INET6_ADDRSTRLEN] = {};
    InetNtopA(AF_INET6, &addr, buffer, sizeof(buffer));
    return buffer;
}

std::string AddressFormatter::TcpStateToString(uint32_t state) {
    switch (state) {
    case MIB_TCP_STATE_CLOSED:
        return "CLOSED";
    case MIB_TCP_STATE_LISTEN:
        return "LISTENING";
    case MIB_TCP_STATE_SYN_SENT:
        return "SYN_SENT";
    case MIB_TCP_STATE_SYN_RCVD:
        return "SYN_RCVD";
    case MIB_TCP_STATE_ESTAB:
        return "ESTABLISHED";
    case MIB_TCP_STATE_FIN_WAIT1:
        return "FIN_WAIT1";
    case MIB_TCP_STATE_FIN_WAIT2:
        return "FIN_WAIT2";
    case MIB_TCP_STATE_CLOSE_WAIT:
        return "CLOSE_WAIT";
    case MIB_TCP_STATE_CLOSING:
        return "CLOSING";
    case MIB_TCP_STATE_LAST_ACK:
        return "LAST_ACK";
    case MIB_TCP_STATE_TIME_WAIT:
        return "TIME_WAIT";
    case MIB_TCP_STATE_DELETE_TCB:
        return "DELETE_TCB";
    default:
        return std::format("STATE_{}", state);
    }
}

} // namespace netwatch::util

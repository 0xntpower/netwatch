#pragma once

#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace netwatch::util {

// Format IP addresses and TCP states to human-readable strings
class AddressFormatter {
public:
    static std::string FormatIPv4(uint32_t addr);
    static std::string FormatIPv6(const IN6_ADDR& addr);
    static std::string TcpStateToString(uint32_t state);
};

} // namespace netwatch::util

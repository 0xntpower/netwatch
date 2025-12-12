#pragma once

#include "stdafx.h"
#include <string>

namespace netwatch::util {
    struct ProcessSecurityInfo {
        std::string architecture;
        std::string depStatus;
        std::string aslrStatus;
        std::string executablePath;
        std::string cfgStatus;
        std::string safeSehStatus;
    };

    [[nodiscard]] BOOL TerminateTargetProcess(DWORD dwProcessId);
    [[nodiscard]] ProcessSecurityInfo GetProcessSecurityInfo(DWORD dwProcessId);
    [[nodiscard]] bool CloseNetworkConnection(DWORD dwLocalAddr, DWORD dwLocalPort, DWORD dwRemoteAddr, DWORD dwRemotePort, std::string& errorMessage);
}

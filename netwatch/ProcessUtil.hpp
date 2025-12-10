#pragma once
#pragma once
#pragma once

#include "stdafx.h"
#include <string>

namespace netwatch::util {
    // Process security information structure
    struct ProcessSecurityInfo {
        std::string architecture;      // x86 or x64
        std::string depStatus;         // DEP/NX status (Enabled/Disabled/N/A)
        std::string aslrStatus;        // ASLR status (Enabled/Disabled/N/A)
        std::string executablePath;    // Full path to the executable
        std::string cfgStatus;         // Control Flow Guard status (Enabled/Disabled/N/A)
        std::string safeSehStatus;     // SafeSEH status (Enabled/Disabled/N/A)
    };

    BOOL TerminateTargetProcess(DWORD dwProcessId);
    ProcessSecurityInfo GetProcessSecurityInfo(DWORD dwProcessId);
    bool CloseNetworkConnection(DWORD dwLocalAddr, DWORD dwLocalPort, DWORD dwRemoteAddr, DWORD dwRemotePort, std::string& errorMessage);
}

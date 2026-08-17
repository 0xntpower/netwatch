#pragma once

#include "stdafx.h"
#include <string>

namespace netwatch::util {

    [[nodiscard]] BOOL TerminateTargetProcess(DWORD dwProcessId);
    [[nodiscard]] bool CloseNetworkConnection(DWORD dwLocalAddr, DWORD dwLocalPort, DWORD dwRemoteAddr, DWORD dwRemotePort, std::string& errorMessage);
}

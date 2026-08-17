#include "stdafx.h"
#include "ProcessUtil.hpp"
#include "util/WindowsError.h"
#include "util/HandleGuard.h"
#include "util/Error.h"

namespace netwatch::util {

    BOOL TerminateTargetProcess(DWORD dwProcessId) {
        HandleGuard hProcess(OpenProcess(PROCESS_TERMINATE, FALSE, dwProcessId));
        if (!hProcess.Valid()) {
            return FALSE;
        }

        return TerminateProcess(hProcess.Get(), 1);
    }

    bool CloseNetworkConnection(DWORD dwLocalAddr, DWORD dwLocalPort, DWORD dwRemoteAddr, DWORD dwRemotePort, std::string& errorMessage) {
        MIB_TCPROW tcpRow = {};
        tcpRow.dwState = MIB_TCP_STATE_DELETE_TCB;
        tcpRow.dwLocalAddr = dwLocalAddr;
        tcpRow.dwLocalPort = dwLocalPort;
        tcpRow.dwRemoteAddr = dwRemoteAddr;
        tcpRow.dwRemotePort = dwRemotePort;

        DWORD dwResult = SetTcpEntry(&tcpRow);
        if (dwResult != NO_ERROR) {
            errorMessage = WindowsError::GetErrorMessage(dwResult);
            return false;
        }

        return true;
    }

}

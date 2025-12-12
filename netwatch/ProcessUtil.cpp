#include "stdafx.h"
#include "ProcessUtil.hpp"
#include "util/WindowsError.h"
#include "util/HandleGuard.h"
#include "util/Error.h"
#include <psapi.h>
#include <winternl.h>

#pragma comment(lib, "psapi.lib")

namespace netwatch::util {

    BOOL TerminateTargetProcess(DWORD dwProcessId) {
        HandleGuard hProcess(OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, FALSE, dwProcessId));
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

    ProcessSecurityInfo GetProcessSecurityInfo(DWORD dwProcessId) {
        ProcessSecurityInfo info;
        info.architecture = "N/A";
        info.depStatus = "N/A";
        info.aslrStatus = "N/A";
        info.executablePath = "N/A";
        info.cfgStatus = "N/A";
        info.safeSehStatus = "N/A";

        if (dwProcessId == 0 || dwProcessId == 4) {
            info.executablePath = (dwProcessId == 0) ? "System Idle Process" : "System";
            return info;
        }

        HandleGuard hProcess(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcessId));
        if (!hProcess.Valid()) {
            return info;
        }

        WCHAR exePath[MAX_PATH] = { 0 };
        DWORD pathLen = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess.Get(), 0, exePath, &pathLen)) {
            char narrowPath[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, exePath, -1, narrowPath, MAX_PATH, NULL, NULL);
            info.executablePath = narrowPath;
        }

        BOOL isWow64 = FALSE;
        if (IsWow64Process(hProcess.Get(), &isWow64)) {
            if (isWow64) {
                info.architecture = "x86";
            }
            else {
                SYSTEM_INFO si;
                GetNativeSystemInfo(&si);
                if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
                    info.architecture = "x64";
                }
                else {
                    info.architecture = "x86";
                }
            }
        }

        DWORD depFlags = 0;
        BOOL depPermanent = FALSE;
        if (GetProcessDEPPolicy(hProcess.Get(), &depFlags, &depPermanent)) {
            if (depFlags & PROCESS_DEP_ENABLE) {
                info.depStatus = "Enabled";
            }
            else {
                info.depStatus = "Disabled";
            }
        }

        if (info.executablePath != "N/A") {
            HandleGuard hFile(CreateFileW(exePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL));
            if (hFile.Valid() && hFile.Get() != INVALID_HANDLE_VALUE) {
                HandleGuard hMapping(CreateFileMappingW(hFile.Get(), NULL, PAGE_READONLY, 0, 0, NULL));
                if (hMapping.Valid()) {
                    LPVOID lpBase = MapViewOfFile(hMapping.Get(), FILE_MAP_READ, 0, 0, 0);
                    if (lpBase) {
                        PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)lpBase;
                        if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
                            PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)lpBase + dosHeader->e_lfanew);
                            if (ntHeaders->Signature == IMAGE_NT_SIGNATURE) {
                                WORD dllCharacteristics = ntHeaders->OptionalHeader.DllCharacteristics;

                                if (dllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE) {
                                    info.aslrStatus = "Enabled";
                                }
                                else {
                                    info.aslrStatus = "Disabled";
                                }

                                if (dllCharacteristics & IMAGE_DLLCHARACTERISTICS_GUARD_CF) {
                                    info.cfgStatus = "Enabled";
                                }
                                else {
                                    info.cfgStatus = "Disabled";
                                }

                                if (ntHeaders->FileHeader.Machine == IMAGE_FILE_MACHINE_I386) {
                                    DWORD loadConfigRVA = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress;
                                    DWORD loadConfigSize = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].Size;

                                    if (loadConfigRVA != 0 && loadConfigSize != 0) {
                                        // RVA to file offset conversion
                                        PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);
                                        DWORD fileOffset = 0;
                                        for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
                                            if (loadConfigRVA >= section[i].VirtualAddress &&
                                                loadConfigRVA < section[i].VirtualAddress + section[i].SizeOfRawData) {
                                                fileOffset = loadConfigRVA - section[i].VirtualAddress + section[i].PointerToRawData;
                                                break;
                                            }
                                        }

                                        if (fileOffset != 0) {
                                            PIMAGE_LOAD_CONFIG_DIRECTORY32 loadConfig = (PIMAGE_LOAD_CONFIG_DIRECTORY32)((BYTE*)lpBase + fileOffset);
                                            if (loadConfig->SEHandlerTable != 0 && loadConfig->SEHandlerCount > 0) {
                                                info.safeSehStatus = "Enabled";
                                            }
                                            else {
                                                info.safeSehStatus = "Disabled";
                                            }
                                        }
                                        else {
                                            info.safeSehStatus = "Disabled";
                                        }
                                    }
                                    else {
                                        info.safeSehStatus = "Disabled";
                                    }
                                }
                                else {
                                    info.safeSehStatus = "N/A";
                                }
                            }
                        }
                        UnmapViewOfFile(lpBase);
                    }
                }
            }
        }

        return info;
    }

}

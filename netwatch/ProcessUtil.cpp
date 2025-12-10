#include "stdafx.h"
#include "ProcessUtil.hpp"
#include "util/WindowsError.h"
#include <psapi.h>
#include <winternl.h>

#pragma comment(lib, "psapi.lib")

namespace netwatch::util {

    BOOL TerminateTargetProcess(DWORD dwProcessId) {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, FALSE, dwProcessId);
        if (!hProcess) {
            return false;
        }

        BOOL result = TerminateProcess(hProcess, 1);

        CloseHandle(hProcess);

        return result;
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

        // Handle special cases
        if (dwProcessId == 0 || dwProcessId == 4) {
            info.executablePath = (dwProcessId == 0) ? "System Idle Process" : "System";
            return info;
        }

        // Open process with required permissions
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcessId);
        if (!hProcess) {
            return info;
        }

        // Get executable path
        WCHAR exePath[MAX_PATH] = { 0 };
        DWORD pathLen = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, exePath, &pathLen)) {
            // Convert wide string to narrow string
            char narrowPath[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, exePath, -1, narrowPath, MAX_PATH, NULL, NULL);
            info.executablePath = narrowPath;
        }

        // Determine architecture
        BOOL isWow64 = FALSE;
        if (IsWow64Process(hProcess, &isWow64)) {
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

        // Check DEP status
        DWORD depFlags = 0;
        BOOL depPermanent = FALSE;
        if (GetProcessDEPPolicy(hProcess, &depFlags, &depPermanent)) {
            if (depFlags & PROCESS_DEP_ENABLE) {
                info.depStatus = "Enabled";
            }
            else {
                info.depStatus = "Disabled";
            }
        }

        // Read PE headers to check ASLR, SafeSEH, and CFG
        if (info.executablePath != "N/A") {
            HANDLE hFile = CreateFileW(exePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                HANDLE hMapping = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
                if (hMapping) {
                    LPVOID lpBase = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
                    if (lpBase) {
                        PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)lpBase;
                        if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
                            PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)lpBase + dosHeader->e_lfanew);
                            if (ntHeaders->Signature == IMAGE_NT_SIGNATURE) {
                                WORD dllCharacteristics = ntHeaders->OptionalHeader.DllCharacteristics;

                                // Check ASLR (Dynamic Base)
                                if (dllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE) {
                                    info.aslrStatus = "Enabled";
                                }
                                else {
                                    info.aslrStatus = "Disabled";
                                }

                                // Check CFG (Control Flow Guard)
                                if (dllCharacteristics & IMAGE_DLLCHARACTERISTICS_GUARD_CF) {
                                    info.cfgStatus = "Enabled";
                                }
                                else {
                                    info.cfgStatus = "Disabled";
                                }

                                // Check SafeSEH (only for x86 binaries)
                                if (ntHeaders->FileHeader.Machine == IMAGE_FILE_MACHINE_I386) {
                                    // SafeSEH is indicated by the presence of the Load Config Directory
                                    // and the SE Handler Table
                                    DWORD loadConfigRVA = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress;
                                    DWORD loadConfigSize = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].Size;

                                    if (loadConfigRVA != 0 && loadConfigSize != 0) {
                                        // Convert RVA to file offset
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
                                    // SafeSEH is only applicable to x86 binaries
                                    info.safeSehStatus = "N/A";
                                }
                            }
                        }
                        UnmapViewOfFile(lpBase);
                    }
                    CloseHandle(hMapping);
                }
                CloseHandle(hFile);
            }
        }

        CloseHandle(hProcess);
        return info;
    }

}

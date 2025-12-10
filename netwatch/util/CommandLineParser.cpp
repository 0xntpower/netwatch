#include "stdafx.h"
#include "CommandLineParser.h"

#include <sstream>

namespace netwatch::util {

namespace {
    std::string TCharToString(LPCTSTR str) {
        if (!str) return {};
#ifdef UNICODE
        int size = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string result(size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, str, -1, result.data(), size, nullptr, nullptr);
        return result;
#else
        return std::string(str);
#endif
    }
} // anonymous namespace

CommandLineOptions CommandLineParser::Parse(LPCTSTR lpCmdLine) {
    CommandLineOptions opts;

    if (!lpCmdLine || _tcslen(lpCmdLine) == 0) {
        return opts;
    }

    std::string cmdLine = TCharToString(lpCmdLine);
    std::istringstream iss(cmdLine);
    std::string token;

    while (iss >> token) {
        if (token == "--filter" || token == "-f") {
            if (iss >> token) {
                opts.processFilter = token;
            }
        }
        else if (token == "--help" || token == "-h" || token == "/?") {
            opts.showHelp = true;
        }
    }

    return opts;
}

void CommandLineParser::ShowUsage() {
    MessageBox(nullptr,
        _T("NetWatch - Network Connection Monitor\n\n")
        _T("Usage:\n")
        _T("  netwatch.exe [options]\n\n")
        _T("Options:\n")
        _T("  --filter <name>, -f <name>    Filter by process name\n")
        _T("  --help, -h, /?                Show this help\n\n")
        _T("Examples:\n")
        _T("  netwatch.exe --filter chrome\n")
        _T("  netwatch.exe -f svchost"),
        _T("NetWatch Usage"),
        MB_OK | MB_ICONINFORMATION);
}

} // namespace netwatch::util

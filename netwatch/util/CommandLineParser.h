#pragma once

#include <string>

namespace netwatch::util {

// Command-line options for NetWatch
struct CommandLineOptions {
    std::string processFilter;
    bool showHelp = false;
};

// Parse command-line arguments
class CommandLineParser {
public:
    static CommandLineOptions Parse(LPCTSTR lpCmdLine);
    static void ShowUsage();
};

} // namespace netwatch::util

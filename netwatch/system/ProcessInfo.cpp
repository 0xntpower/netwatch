#include "stdafx.h"
#include "ProcessInfo.h"
#include "../util/HandleGuard.h"
#include "../util/StringConversion.h"

#include <string>

namespace netwatch::system {

namespace {
    constexpr uint32_t kSystemIdlePid = 0;
    constexpr uint32_t kSystemPid = 4;
} // anonymous namespace

std::string ProcessInfo::GetName(uint32_t pid) {
    if (pid == kSystemIdlePid) return "System Idle Process";
    if (pid == kSystemPid) return "System";

    util::HandleGuard process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!process.Valid()) {
        return "AccessDenied";
    }

    DWORD size = MAX_PATH;
    std::wstring buffer(size, L'\0');

    if (QueryFullProcessImageNameW(process.Get(), 0, buffer.data(), &size)) {
        buffer.resize(size);
        auto pos = buffer.find_last_of(L'\\');
        if (pos != std::wstring::npos) {
            buffer = buffer.substr(pos + 1);
        }
        return util::StringConversion::WideToNarrow(buffer);
    }
    return "Unknown";
}

} // namespace netwatch::system

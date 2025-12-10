#include "stdafx.h"
#include "IntegrityLevelResolver.h"
#include "../util/HandleGuard.h"

#include <format>
#include <sddl.h>
#include <vector>

#pragma comment(lib, "advapi32.lib")

namespace netwatch::system {

namespace {
    std::string RidToString(uint32_t rid) {
        if (rid >= SECURITY_MANDATORY_SYSTEM_RID) return "System";
        if (rid >= SECURITY_MANDATORY_HIGH_RID) return "High";
        if (rid >= SECURITY_MANDATORY_MEDIUM_RID) return "Medium";
        if (rid >= SECURITY_MANDATORY_LOW_RID) return "Low";
        return std::format("RID:{}", rid);
    }
} // anonymous namespace

std::string IntegrityLevelResolver::Resolve(uint32_t pid) {
    util::HandleGuard process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!process.Valid()) {
        return "AccessDenied";
    }

    util::HandleGuard token;
    HANDLE rawToken = nullptr;
    if (!OpenProcessToken(process.Get(), TOKEN_QUERY, &rawToken)) {
        return "AccessDenied";
    }
    token.Reset(rawToken);

    DWORD size = 0;
    GetTokenInformation(token.Get(), TokenIntegrityLevel, nullptr, 0, &size);
    if (size == 0) {
        return "Unknown";
    }

    std::vector<BYTE> buffer(size);
    auto* til = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer.data());

    if (!GetTokenInformation(token.Get(), TokenIntegrityLevel, til, size, &size)) {
        return "Unknown";
    }

    DWORD rid = *GetSidSubAuthority(til->Label.Sid,
        static_cast<DWORD>(*GetSidSubAuthorityCount(til->Label.Sid) - 1));

    return RidToString(rid);
}

} // namespace netwatch::system

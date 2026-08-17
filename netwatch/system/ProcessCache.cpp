#include "stdafx.h"
#include "ProcessCache.h"
#include "IntegrityLevelResolver.h"
#include "../util/HandleGuard.h"
#include "../util/StringConversion.h"

#include <tlhelp32.h>
#include <shellapi.h>
#include <atomic>

#pragma comment(lib, "shell32.lib")

namespace netwatch::system {

namespace {

// Captured the first time an icon is resolved. The shell owns this handle, so
// the list control must be created with LVS_SHAREIMAGELISTS to stop it being
// destroyed along with the window.
std::atomic<HIMAGELIST> g_systemImageList{nullptr};

// Ask the shell for the executable's icon index. This is the same call Explorer
// makes, so extracted icons, overlays and per-file resources all come out right,
// and the bitmaps are already at the correct DPI.
int ResolveIconIndex(const wchar_t* path) {
    SHFILEINFOW info = {};
    const auto result = ::SHGetFileInfoW(path, 0, &info, sizeof(info),
        SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
    if (result == 0) {
        return -1;
    }

    g_systemImageList.store(reinterpret_cast<HIMAGELIST>(result), std::memory_order_release);
    return info.iIcon;
}

constexpr uint32_t kSystemIdlePid = 0;
constexpr uint32_t kSystemPid = 4;

constexpr const char* kNotApplicable = "N/A";
constexpr const char* kEnabled = "Enabled";
constexpr const char* kDisabled = "Disabled";

// IsWow64Process2 is Windows 10 1511+. Resolving it dynamically keeps the
// binary loadable on the older systems the manifest still claims, and it is the
// only API that can tell an ARM64 process apart from an x64 one.
using PfnIsWow64Process2 = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);

PfnIsWow64Process2 GetIsWow64Process2() {
    static PfnIsWow64Process2 fn = reinterpret_cast<PfnIsWow64Process2>(
        ::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "IsWow64Process2"));
    return fn;
}

const char* MachineToArch(USHORT machine) {
    switch (machine) {
    case IMAGE_FILE_MACHINE_I386:  return "x86";
    case IMAGE_FILE_MACHINE_AMD64: return "x64";
    case IMAGE_FILE_MACHINE_ARM64: return "ARM64";
    case IMAGE_FILE_MACHINE_ARMNT: return "ARM";
    default:                       return kNotApplicable;
    }
}

std::string ResolveArchitecture(HANDLE process) {
    if (auto fn = GetIsWow64Process2()) {
        USHORT processMachine = IMAGE_FILE_MACHINE_UNKNOWN;
        USHORT nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
        if (fn(process, &processMachine, &nativeMachine)) {
            // processMachine is UNKNOWN when the process is not emulated, in
            // which case it runs as the host architecture.
            return MachineToArch(processMachine == IMAGE_FILE_MACHINE_UNKNOWN
                ? nativeMachine
                : processMachine);
        }
    }

    // Pre-Windows 10 fallback. Cannot distinguish ARM64, but those systems do
    // not run ARM64 either.
    BOOL isWow64 = FALSE;
    if (!::IsWow64Process(process, &isWow64)) {
        return kNotApplicable;
    }
    if (isWow64) {
        return "x86";
    }

    SYSTEM_INFO si = {};
    ::GetNativeSystemInfo(&si);
    return (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) ? "x64" : "x86";
}

// Convert an RVA to a file offset using the section table. The image is mapped
// as a plain data file, not as an image, so section RVAs are not usable
// directly.
DWORD RvaToFileOffset(PIMAGE_NT_HEADERS32 ntHeaders, DWORD rva) {
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);
    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        const DWORD start = section[i].VirtualAddress;
        const DWORD end = start + section[i].SizeOfRawData;
        if (rva >= start && rva < end) {
            return rva - start + section[i].PointerToRawData;
        }
    }
    return 0;
}

// Tri-state so "could not determine" stays distinct from "off".
enum class Flag { Unknown, Off, On };

struct PeFlags {
    Flag aslr = Flag::Unknown;
    Flag cfg = Flag::Unknown;
    Flag safeSeh = Flag::Unknown;
};

// Parse the mapped image. POD-only and no C++ objects requiring unwinding,
// because __try cannot appear in a function that needs one.
//
// The optional header layout differs between PE32 and PE32+. DllCharacteristics
// happens to land at the same offset in both, but DataDirectory does not: it is
// at optional-header offset 96 in PE32 and 112 in PE32+. Reading a PE32 file
// through IMAGE_NT_HEADERS64 therefore lands two directory entries off and
// parses the import address table as if it were the load config directory.
PeFlags ParseMappedImage(const BYTE* bytes, size_t size) noexcept {
    PeFlags flags;

    // A file replaced underneath us turns reads of the mapped view into
    // EXCEPTION_IN_PAGE_ERROR, which is not a C++ exception. The bounds checks
    // below still apply, this only covers the file changing mid-read.
    __try {
        if (size < sizeof(IMAGE_DOS_HEADER)) {
            return flags;
        }

        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
            return flags;
        }

        const LONG ntOffset = dos->e_lfanew;
        if (ntOffset < 0 || static_cast<size_t>(ntOffset) + sizeof(IMAGE_NT_HEADERS32) > size) {
            return flags;
        }

        auto* nt32 = reinterpret_cast<PIMAGE_NT_HEADERS32>(const_cast<BYTE*>(bytes + ntOffset));
        if (nt32->Signature != IMAGE_NT_SIGNATURE) {
            return flags;
        }

        const bool isPe32 = (nt32->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC);
        const bool isPe32Plus = (nt32->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
        if (!isPe32 && !isPe32Plus) {
            return flags;
        }

        // Safe for both variants: DllCharacteristics sits at the same optional
        // header offset in PE32 and PE32+.
        const WORD dllCharacteristics = nt32->OptionalHeader.DllCharacteristics;
        flags.aslr = (dllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE) ? Flag::On : Flag::Off;
        flags.cfg = (dllCharacteristics & IMAGE_DLLCHARACTERISTICS_GUARD_CF) ? Flag::On : Flag::Off;

        // SafeSEH is a 32-bit x86 concept only. 64-bit images use table-based
        // exception handling, which is not opt-in.
        if (!isPe32 || nt32->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
            return flags;
        }

        if (nt32->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG) {
            flags.safeSeh = Flag::Off;
            return flags;
        }

        const IMAGE_DATA_DIRECTORY dir =
            nt32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG];
        if (dir.VirtualAddress == 0 || dir.Size == 0) {
            flags.safeSeh = Flag::Off;
            return flags;
        }

        // The SEH handler table only exists in load config structures large
        // enough to declare it.
        const DWORD needed = static_cast<DWORD>(
            offsetof(IMAGE_LOAD_CONFIG_DIRECTORY32, SEHandlerCount) + sizeof(DWORD));
        if (dir.Size < needed) {
            flags.safeSeh = Flag::Off;
            return flags;
        }

        const DWORD offset = RvaToFileOffset(nt32, dir.VirtualAddress);
        if (offset == 0 || static_cast<size_t>(offset) + needed > size) {
            flags.safeSeh = Flag::Off;
            return flags;
        }

        const auto* loadConfig =
            reinterpret_cast<const IMAGE_LOAD_CONFIG_DIRECTORY32*>(bytes + offset);

        flags.safeSeh = (loadConfig->SEHandlerTable != 0 && loadConfig->SEHandlerCount > 0)
            ? Flag::On : Flag::Off;
        return flags;
    }
    __except (GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR
                  ? EXCEPTION_EXECUTE_HANDLER
                  : EXCEPTION_CONTINUE_SEARCH) {
        return PeFlags{};
    }
}

const char* FlagToString(Flag f) {
    switch (f) {
    case Flag::On:  return kEnabled;
    case Flag::Off: return kDisabled;
    default:        return kNotApplicable;
    }
}

// Map the image from disk and hand the bytes to the guarded parser.
PeFlags ReadImageFlags(const wchar_t* path) {
    util::HandleGuard file(::CreateFileW(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.Valid() || file.Get() == INVALID_HANDLE_VALUE) {
        return PeFlags{};
    }

    LARGE_INTEGER fileSize = {};
    if (!::GetFileSizeEx(file.Get(), &fileSize) || fileSize.QuadPart < sizeof(IMAGE_DOS_HEADER)) {
        return PeFlags{};
    }

    util::HandleGuard mapping(::CreateFileMappingW(file.Get(), nullptr, PAGE_READONLY, 0, 0, nullptr));
    if (!mapping.Valid()) {
        return PeFlags{};
    }

    const LPVOID base = ::MapViewOfFile(mapping.Get(), FILE_MAP_READ, 0, 0, 0);
    if (base == nullptr) {
        return PeFlags{};
    }

    const PeFlags flags = ParseMappedImage(static_cast<const BYTE*>(base),
        static_cast<size_t>(fileSize.QuadPart));

    ::UnmapViewOfFile(base);
    return flags;
}

std::string ResolveDep(HANDLE process, const std::string& architecture) {
    // On 64-bit Windows, DEP is always on for 64-bit processes and cannot be
    // turned off, so there is nothing to query.
    if (architecture == "x64" || architecture == "ARM64") {
        return kEnabled;
    }

    DWORD depFlags = 0;
    BOOL depPermanent = FALSE;
    if (!::GetProcessDEPPolicy(process, &depFlags, &depPermanent)) {
        return kNotApplicable;
    }
    return (depFlags & PROCESS_DEP_ENABLE) ? kEnabled : kDisabled;
}

} // namespace

HIMAGELIST SystemSmallImageList() {
    return g_systemImageList.load(std::memory_order_acquire);
}

void ProcessCache::BeginPass() {
    snapshotNames_.clear();

    // One snapshot gives base names for essentially every process without
    // needing a handle to any of them, which is what makes the Process Name
    // column populate correctly when running unelevated.
    util::HandleGuard snapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (snapshot.Valid() && snapshot.Get() != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry = {};
        entry.dwSize = sizeof(entry);
        if (::Process32FirstW(snapshot.Get(), &entry)) {
            do {
                snapshotNames_.emplace(
                    static_cast<uint32_t>(entry.th32ProcessID),
                    util::StringConversion::WideToNarrow(entry.szExeFile));
            } while (::Process32NextW(snapshot.Get(), &entry));
        }
    }

    for (auto& [pid, cached] : cache_) {
        cached.seenThisPass = false;
    }
}

const ProcessDetails& ProcessCache::Get(uint32_t pid) {
    const auto snapshotIt = snapshotNames_.find(pid);
    const std::string identity = (snapshotIt != snapshotNames_.end()) ? snapshotIt->second : std::string();

    auto it = cache_.find(pid);
    if (it != cache_.end()) {
        // ponytail: PID reuse is detected by the executable name changing under
        // the same PID. Cheap because the snapshot is already in hand. A
        // recycled PID reusing the same image within one refresh tick would
        // slip through. Compare process creation times if that ever matters.
        if (it->second.identity == identity) {
            it->second.seenThisPass = true;
            return it->second.details;
        }
        cache_.erase(it);
    }

    Entry entry;
    entry.details = Resolve(pid);
    entry.identity = identity;
    entry.seenThisPass = true;

    if (entry.details.name.empty()) {
        entry.details.name = identity.empty() ? "Unknown" : identity;
    }

    return cache_.emplace(pid, std::move(entry)).first->second.details;
}

void ProcessCache::EndPass() {
    for (auto it = cache_.begin(); it != cache_.end();) {
        it = it->second.seenThisPass ? std::next(it) : cache_.erase(it);
    }
}

ProcessDetails ProcessCache::Resolve(uint32_t pid) const {
    ProcessDetails details;
    details.integrityLevel = kNotApplicable;
    details.architecture = kNotApplicable;
    details.depStatus = kNotApplicable;
    details.aslrStatus = kNotApplicable;
    details.cfgStatus = kNotApplicable;
    details.safeSehStatus = kNotApplicable;

    if (const auto it = snapshotNames_.find(pid); it != snapshotNames_.end()) {
        details.name = it->second;
    }

    if (pid == kSystemIdlePid) {
        details.name = "System Idle Process";
        return details;
    }
    if (pid == kSystemPid) {
        details.name = "System";
        details.integrityLevel = "System";
        return details;
    }

    // PROCESS_QUERY_LIMITED_INFORMATION is deliberately the only right asked
    // for. It succeeds against nearly every process from a medium-integrity
    // token, where PROCESS_QUERY_INFORMATION | PROCESS_VM_READ does not. The PE
    // headers are read from disk, so no read access to process memory is
    // needed for any of this.
    util::HandleGuard process(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!process.Valid()) {
        details.integrityLevel = "Denied";
        return details;
    }

    details.integrityLevel = IntegrityLevelResolver::Resolve(process.Get());
    details.architecture = ResolveArchitecture(process.Get());
    details.depStatus = ResolveDep(process.Get(), details.architecture);

    WCHAR exePath[MAX_PATH] = {};
    DWORD pathLen = MAX_PATH;
    if (::QueryFullProcessImageNameW(process.Get(), 0, exePath, &pathLen)) {
        details.executablePath = util::StringConversion::WideToNarrow(exePath);

        const PeFlags flags = ReadImageFlags(exePath);
        details.aslrStatus = FlagToString(flags.aslr);
        details.cfgStatus = FlagToString(flags.cfg);
        details.safeSehStatus = FlagToString(flags.safeSeh);

        // Resolved once per process here on the worker thread, so a slow disk
        // or a network path cannot stall painting.
        details.iconIndex = ResolveIconIndex(exePath);
    }

    return details;
}

} // namespace netwatch::system

#include <windows.h>

#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cwchar>

constexpr std::size_t kVersionExportCount = 17;
extern "C" FARPROC g_versionExports[kVersionExportCount];

namespace {

constexpr wchar_t kPayloadName[] = L"DeadSpaceCompleteInputFix.dll";
constexpr wchar_t kLogName[] = L"DeadSpaceCompleteInputFix.bootstrap.log";
constexpr char kInitialiseExport[] = "DCIF_Initialize";
constexpr std::size_t kExportCount = kVersionExportCount;

const char* const kExportNames[kExportCount] = {
    "GetFileVersionInfoA",
    "GetFileVersionInfoByHandle",
    "GetFileVersionInfoExA",
    "GetFileVersionInfoExW",
    "GetFileVersionInfoSizeA",
    "GetFileVersionInfoSizeExA",
    "GetFileVersionInfoSizeExW",
    "GetFileVersionInfoSizeW",
    "GetFileVersionInfoW",
    "VerFindFileA",
    "VerFindFileW",
    "VerInstallFileA",
    "VerInstallFileW",
    "VerLanguageNameA",
    "VerLanguageNameW",
    "VerQueryValueA",
    "VerQueryValueW"
};

HMODULE g_self = nullptr;
HMODULE g_realVersion = nullptr;
DWORD_PTR g_originalAffinityMask = 0;
DWORD_PTR g_selectedAffinityMask = 0;
bool g_affinityRestricted = false;

bool ModuleDirectory(HMODULE module, wchar_t* output, const std::size_t capacity) {
    if (!output || capacity == 0)
        return false;

    const DWORD length = GetModuleFileNameW(module, output, static_cast<DWORD>(capacity));
    if (length == 0 || length >= capacity)
        return false;

    wchar_t* slash = std::wcsrchr(output, L'\\');
    if (!slash)
        return false;
    *slash = L'\0';
    return true;
}

bool AppendPath(wchar_t* path, const std::size_t capacity, const wchar_t* leaf) {
    if (!path || !leaf)
        return false;
    const std::size_t current = std::wcslen(path);
    const std::size_t extra = std::wcslen(leaf);
    if (current + 1 + extra + 1 > capacity)
        return false;
    path[current] = L'\\';
    std::wmemcpy(path + current + 1, leaf, extra + 1);
    return true;
}

void AppendBootstrapLog(const char* status) {
    wchar_t directory[MAX_PATH] = {};
    if (!ModuleDirectory(g_self, directory, _countof(directory)) ||
        !AppendPath(directory, _countof(directory), kLogName)) {
        return;
    }

    HANDLE file = CreateFileW(
        directory, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    wchar_t processPath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, processPath, _countof(processPath));
    const bool textureChild = GetEnvironmentVariableW(L"DSTL_TEXMOD_CHILD", nullptr, 0) != 0;

    char line[1024] = {};
    const int length = std::snprintf(
        line, sizeof(line),
        "pid=%lu texture_child=%d affinity_restricted=%d "
        "affinity_original=%p affinity_selected=%p status=%s process=%ls\r\n",
        static_cast<unsigned long>(GetCurrentProcessId()), textureChild ? 1 : 0,
        g_affinityRestricted ? 1 : 0,
        reinterpret_cast<void*>(g_originalAffinityMask),
        reinterpret_cast<void*>(g_selectedAffinityMask),
        status ? status : "unknown", processPath);
    if (length > 0) {
        DWORD written = 0;
        WriteFile(file, line, static_cast<DWORD>(length), &written, nullptr);
    }
    CloseHandle(file);
}

bool IsDeadSpaceProcess() {
    wchar_t processPath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, processPath, _countof(processPath)) == 0)
        return false;
    const wchar_t* baseName = std::wcsrchr(processPath, L'\\');
    baseName = baseName ? baseName + 1 : processPath;
    return _wcsicmp(baseName, L"Dead Space.exe") == 0;
}

bool IsAllowedHost() {
    return IsDeadSpaceProcess() ||
        GetEnvironmentVariableW(L"DCIF_ALLOW_TEST_HOST", nullptr, 0) != 0;
}

void ApplyStandaloneCpuSafety() {
    if (!IsDeadSpaceProcess())
        return;

    DWORD_PTR processMask = 0;
    DWORD_PTR systemMask = 0;
    if (!GetProcessAffinityMask(
            GetCurrentProcess(), &processMask, &systemMask) || processMask == 0) {
        return;
    }

    g_originalAffinityMask = processMask;
    DWORD_PTR selected = 0;
    unsigned int selectedCount = 0;
    unsigned int allowedCount = 0;
    for (unsigned int bit = 0; bit < sizeof(DWORD_PTR) * 8; ++bit) {
        const DWORD_PTR candidate = static_cast<DWORD_PTR>(1) << bit;
        if ((processMask & candidate) == 0)
            continue;
        ++allowedCount;
        if (selectedCount < 8) {
            selected |= candidate;
            ++selectedCount;
        }
    }

    g_selectedAffinityMask = selected;
    if (allowedCount <= 8 || selected == 0)
        return;

    g_affinityRestricted =
        SetProcessAffinityMask(GetCurrentProcess(), selected) != FALSE;
}

bool LoadSystemVersion() {
    wchar_t path[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(path, _countof(path));
    if (length == 0 || length >= _countof(path) ||
        !AppendPath(path, _countof(path), L"version.dll")) {
        return false;
    }

    g_realVersion = LoadLibraryW(path);
    if (!g_realVersion || g_realVersion == g_self)
        return false;

    for (std::size_t index = 0; index < kExportCount; ++index) {
        g_versionExports[index] = GetProcAddress(g_realVersion, kExportNames[index]);
        if (!g_versionExports[index])
            return false;
    }
    return true;
}

DWORD WINAPI PayloadBootstrapThread(void*) {
    if (!IsAllowedHost())
        return 0;

    wchar_t payloadPath[MAX_PATH] = {};
    if (!ModuleDirectory(g_self, payloadPath, _countof(payloadPath)) ||
        !AppendPath(payloadPath, _countof(payloadPath), kPayloadName)) {
        AppendBootstrapLog("payload_path_failed_safe");
        return 0;
    }

    HMODULE payload = LoadLibraryW(payloadPath);
    if (!payload) {
        AppendBootstrapLog("payload_missing_or_load_failed_safe");
        return 0;
    }

    using InitialiseFn = BOOL (WINAPI*)();
    const auto initialise = reinterpret_cast<InitialiseFn>(
        GetProcAddress(payload, kInitialiseExport));
    if (!initialise || !initialise()) {
        AppendBootstrapLog("payload_rejected_safe");
        FreeLibrary(payload);
        return 0;
    }

    AppendBootstrapLog("payload_initialised");
    return 0;
}

} // namespace

extern "C" FARPROC g_versionExports[kVersionExportCount] = {};

#define VERSION_THUNK(name, offset) \
    extern "C" __declspec(naked) void Forward_##name() { \
        __asm { jmp dword ptr [g_versionExports + offset] } \
    }

VERSION_THUNK(GetFileVersionInfoA, 0)
VERSION_THUNK(GetFileVersionInfoByHandle, 4)
VERSION_THUNK(GetFileVersionInfoExA, 8)
VERSION_THUNK(GetFileVersionInfoExW, 12)
VERSION_THUNK(GetFileVersionInfoSizeA, 16)
VERSION_THUNK(GetFileVersionInfoSizeExA, 20)
VERSION_THUNK(GetFileVersionInfoSizeExW, 24)
VERSION_THUNK(GetFileVersionInfoSizeW, 28)
VERSION_THUNK(GetFileVersionInfoW, 32)
VERSION_THUNK(VerFindFileA, 36)
VERSION_THUNK(VerFindFileW, 40)
VERSION_THUNK(VerInstallFileA, 44)
VERSION_THUNK(VerInstallFileW, 48)
VERSION_THUNK(VerLanguageNameA, 52)
VERSION_THUNK(VerLanguageNameW, 56)
VERSION_THUNK(VerQueryValueA, 60)
VERSION_THUNK(VerQueryValueW, 64)

#undef VERSION_THUNK

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason != DLL_PROCESS_ATTACH)
        return TRUE;

    g_self = instance;
    DisableThreadLibraryCalls(instance);
    // Dead Space corrupts memory while enumerating more than eight processors.
    // Apply the smallest process-local restriction before any game code runs so
    // this standalone proxy does not depend on a separate stability mod. The
    // selected mask is always a subset of the process's existing allowance.
    ApplyStandaloneCpuSafety();
    if (!LoadSystemVersion())
        return FALSE;

    HANDLE thread = CreateThread(nullptr, 0, PayloadBootstrapThread, nullptr, 0, nullptr);
    if (thread)
        CloseHandle(thread);
    return TRUE;
}

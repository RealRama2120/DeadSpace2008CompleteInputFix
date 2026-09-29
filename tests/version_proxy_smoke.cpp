#include <windows.h>
#include <winver.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <vector>

namespace {

const char* const kNames[] = {
    "GetFileVersionInfoA", "GetFileVersionInfoByHandle", "GetFileVersionInfoExA",
    "GetFileVersionInfoExW", "GetFileVersionInfoSizeA", "GetFileVersionInfoSizeExA",
    "GetFileVersionInfoSizeExW", "GetFileVersionInfoSizeW", "GetFileVersionInfoW",
    "VerFindFileA", "VerFindFileW", "VerInstallFileA", "VerInstallFileW",
    "VerLanguageNameA", "VerLanguageNameW", "VerQueryValueA", "VerQueryValueW"
};

bool SiblingPath(const wchar_t* leaf, wchar_t* output, const std::size_t capacity) {
    const DWORD length = GetModuleFileNameW(nullptr, output, static_cast<DWORD>(capacity));
    if (length == 0 || length >= capacity)
        return false;
    wchar_t* slash = std::wcsrchr(output, L'\\');
    if (!slash)
        return false;
    *(slash + 1) = L'\0';
    if (std::wcslen(output) + std::wcslen(leaf) + 1 > capacity)
        return false;
    wcscat_s(output, capacity, leaf);
    return true;
}

} // namespace

unsigned int CountBits(DWORD_PTR value) {
    unsigned int count = 0;
    while (value != 0) {
        count += static_cast<unsigned int>(value & 1);
        value >>= 1;
    }
    return count;
}

int wmain(const int argc, wchar_t** argv) {
    SetEnvironmentVariableW(L"DCIF_ALLOW_TEST_HOST", L"1");
    const bool affinityMode =
        argc > 1 && _wcsicmp(argv[1], L"--affinity") == 0;
    DWORD_PTR affinityBefore = 0;
    DWORD_PTR systemMask = 0;
    if (affinityMode && !GetProcessAffinityMask(
            GetCurrentProcess(), &affinityBefore, &systemMask)) {
        std::printf("FAIL: initial process affinity could not be read (%lu).\n", GetLastError());
        return 1;
    }
    wchar_t proxyPath[MAX_PATH] = {};
    if (!SiblingPath(L"version.dll", proxyPath, _countof(proxyPath)))
        return 1;

    HMODULE proxy = LoadLibraryW(proxyPath);
    if (!proxy) {
        std::printf("FAIL: version.dll did not load (%lu).\n", GetLastError());
        return 2;
    }

    if (affinityMode) {
        DWORD_PTR affinityAfter = 0;
        if (!GetProcessAffinityMask(GetCurrentProcess(), &affinityAfter, &systemMask)) {
            std::printf("FAIL: restricted process affinity could not be read (%lu).\n", GetLastError());
            return 7;
        }
        const unsigned int beforeCount = CountBits(affinityBefore);
        const unsigned int afterCount = CountBits(affinityAfter);
        if (affinityAfter == 0 || (affinityAfter & affinityBefore) != affinityAfter ||
            afterCount > 8 || (beforeCount > 8 && afterCount != 8)) {
            std::printf(
                "FAIL: standalone CPU safety produced invalid affinity "
                "(before=%p/%u after=%p/%u).\n",
                reinterpret_cast<void*>(affinityBefore), beforeCount,
                reinterpret_cast<void*>(affinityAfter), afterCount);
            return 8;
        }
    }

    for (WORD index = 0; index < _countof(kNames); ++index) {
        FARPROC byName = GetProcAddress(proxy, kNames[index]);
        FARPROC byOrdinal = GetProcAddress(proxy, MAKEINTRESOURCEA(index + 1));
        if (!byName || byName != byOrdinal) {
            std::printf("FAIL: export %u (%s) missing or ordinal mismatch.\n", index + 1, kNames[index]);
            return 3;
        }
    }

    using SizeFn = DWORD (WINAPI*)(LPCWSTR, LPDWORD);
    using InfoFn = BOOL (WINAPI*)(LPCWSTR, DWORD, DWORD, LPVOID);
    using QueryFn = BOOL (WINAPI*)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
    const auto sizeFn = reinterpret_cast<SizeFn>(GetProcAddress(proxy, "GetFileVersionInfoSizeW"));
    const auto infoFn = reinterpret_cast<InfoFn>(GetProcAddress(proxy, "GetFileVersionInfoW"));
    const auto queryFn = reinterpret_cast<QueryFn>(GetProcAddress(proxy, "VerQueryValueW"));

    wchar_t ownPath[MAX_PATH] = {};
    const UINT systemLength = GetSystemDirectoryW(ownPath, _countof(ownPath));
    if (systemLength == 0 || systemLength >= _countof(ownPath) ||
        std::wcslen(ownPath) + std::wcslen(L"\\kernel32.dll") + 1 > _countof(ownPath)) {
        return 4;
    }
    wcscat_s(ownPath, _countof(ownPath), L"\\kernel32.dll");
    DWORD ignored = 0;
    const DWORD size = sizeFn(ownPath, &ignored);
    if (size == 0) {
        std::printf("FAIL: forwarded GetFileVersionInfoSizeW failed (%lu).\n", GetLastError());
        return 4;
    }
    std::vector<unsigned char> data(size);
    if (!infoFn(ownPath, 0, size, data.data())) {
        std::printf("FAIL: forwarded GetFileVersionInfoW failed (%lu).\n", GetLastError());
        return 5;
    }
    void* root = nullptr;
    UINT rootSize = 0;
    if (!queryFn(data.data(), L"\\", &root, &rootSize) || !root || rootSize == 0) {
        std::printf("FAIL: forwarded VerQueryValueW failed (%lu).\n", GetLastError());
        return 6;
    }

    Sleep(500);
    if (affinityMode) {
        std::printf(
            "PASS: standalone Dead Space host affinity is safely limited to at most eight processors.\n");
    } else {
        std::printf("PASS: all 17 VERSION exports and ReShade's W-version calls forward correctly.\n");
    }
    return 0;
}

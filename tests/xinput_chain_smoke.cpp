#include <windows.h>
#include <xinput.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

extern "C" volatile FARPROC g_fixtureIat[3] = {};

namespace {

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

int wmain(const int argc, wchar_t** argv) {
    SetEnvironmentVariableW(L"DCIF_ALLOW_TEST_HOST", L"1");

    wchar_t fixturePath[MAX_PATH] = {};
    wchar_t proxyPath[MAX_PATH] = {};
    const bool systemMode = argc > 1 && _wcsicmp(argv[1], L"--system") == 0;
    if (systemMode) {
        const UINT length = GetSystemDirectoryW(fixturePath, _countof(fixturePath));
        if (length == 0 || length >= _countof(fixturePath) ||
            std::wcslen(fixturePath) + std::wcslen(L"\\xinput1_3.dll") + 1 > _countof(fixturePath)) {
            return 1;
        }
        wcscat_s(fixturePath, _countof(fixturePath), L"\\xinput1_3.dll");
    } else if (!SiblingPath(L"xinput1_3.dll", fixturePath, _countof(fixturePath))) {
        return 1;
    }
    if (!SiblingPath(L"version.dll", proxyPath, _countof(proxyPath))) {
        return 1;
    }

    HMODULE fixture = LoadLibraryW(fixturePath);
    if (!fixture) {
        std::printf("FAIL: fixture XInput DLL did not load (%lu).\n", GetLastError());
        return 2;
    }
    g_fixtureIat[0] = GetProcAddress(fixture, "XInputGetState");
    g_fixtureIat[1] = GetProcAddress(fixture, "XInputGetCapabilities");
    g_fixtureIat[2] = GetProcAddress(fixture, "XInputSetState");
    if (!g_fixtureIat[0] || !g_fixtureIat[1] || !g_fixtureIat[2])
        return 3;
    FARPROC original = g_fixtureIat[0];

    HMODULE proxy = LoadLibraryW(proxyPath);
    if (!proxy) {
        std::printf("FAIL: version proxy did not load (%lu).\n", GetLastError());
        return 4;
    }

    for (unsigned attempt = 0; attempt < 80 && g_fixtureIat[0] == original; ++attempt)
        Sleep(100);
    if (g_fixtureIat[0] == original) {
        std::printf("FAIL: payload did not patch the validated fixture IAT slot.\n");
        return 5;
    }

    using GetStateFn = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);
    const auto call = reinterpret_cast<GetStateFn>(const_cast<FARPROC>(g_fixtureIat[0]));
    XINPUT_STATE state = {};
    if (systemMode) {
        const auto originalCall = reinterpret_cast<GetStateFn>(original);
        for (DWORD index = 0; index < 4; ++index) {
            XINPUT_STATE before = {};
            XINPUT_STATE after = {};
            const DWORD beforeResult = originalCall(index, &before);
            const DWORD afterResult = call(index, &after);
            if (beforeResult != afterResult) {
                std::printf("FAIL: system XInput result changed for slot %lu.\n", index);
                return 6;
            }
            if (beforeResult == ERROR_SUCCESS &&
                (before.dwPacketNumber != after.dwPacketNumber ||
                 before.Gamepad.wButtons != after.Gamepad.wButtons ||
                 before.Gamepad.bLeftTrigger != after.Gamepad.bLeftTrigger ||
                 before.Gamepad.bRightTrigger != after.Gamepad.bRightTrigger ||
                 before.Gamepad.sThumbLX != after.Gamepad.sThumbLX ||
                 before.Gamepad.sThumbLY != after.Gamepad.sThumbLY)) {
                std::printf("FAIL: system XInput unrelated state changed for slot %lu.\n", index);
                return 7;
            }
        }
        std::printf("PASS: validated IAT interception chained the system XInput target.\n");
        return 0;
    }

    const DWORD result = call(0, &state);
    if (result != ERROR_SUCCESS ||
        state.dwPacketNumber != 0x12345678 ||
        state.Gamepad.wButtons != (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_RIGHT_SHOULDER) ||
        state.Gamepad.bLeftTrigger != 17 || state.Gamepad.bRightTrigger != 231 ||
        state.Gamepad.sThumbLX != -12000 || state.Gamepad.sThumbLY != 13000 ||
        state.Gamepad.sThumbRX != 4000 || state.Gamepad.sThumbRY != -5000) {
        std::printf("FAIL: transparent XInput chaining changed unrelated state.\n");
        return 6;
    }

    std::memset(&state, 0xA5, sizeof(state));
    if (call(1, &state) != ERROR_DEVICE_NOT_CONNECTED) {
        std::printf("FAIL: player-index error code was not preserved.\n");
        return 7;
    }

    std::printf("PASS: validated IAT interception chained the current XInput target transparently.\n");
    return 0;
}

#include "config.hpp"
#include "diagnostics.hpp"
#include "logging.hpp"
#include "mouse_hook.hpp"
#include "xinput_hook.hpp"

#include <windows.h>

#include <cstdlib>
#include <cwchar>

namespace {

HMODULE g_self = nullptr;
INIT_ONCE g_initialiseOnce = INIT_ONCE_STATIC_INIT;

bool IsAllowedHost() {
    wchar_t processPath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, processPath, _countof(processPath)) == 0)
        return false;
    const wchar_t* baseName = std::wcsrchr(processPath, L'\\');
    baseName = baseName ? baseName + 1 : processPath;
    if (_wcsicmp(baseName, L"Dead Space.exe") == 0)
        return true;
    return GetEnvironmentVariableW(L"DCIF_ALLOW_TEST_HOST", nullptr, 0) != 0;
}

void LogModule(const wchar_t* name) {
    HMODULE module = GetModuleHandleW(name);
    wchar_t path[MAX_PATH] = L"<not loaded>";
    if (module)
        GetModuleFileNameW(module, path, _countof(path));
    Logging::Write(L"Module %s: %s", name, path);
}

DWORD WINAPI WorkerThread(void*) {
    const DevelopmentConfig config = LoadConfiguration(g_self);
    wchar_t processPath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, processPath, _countof(processPath));
    const bool textureChild = GetEnvironmentVariableW(L"DSTL_TEXMOD_CHILD", nullptr, 0) != 0;
    Logging::Write(
        L"DEAD SPACE COMPLETE INPUT FIX starting. process=%s texture_child=%d",
        processPath, textureChild ? 1 : 0);

    LogModule(L"version.dll");
    LogModule(L"dinput8.dll");
    LogModule(L"xinput1_3.dll");
    LogModule(L"dsound.dll");
    LogModule(L"d3d9.dll");

    // The Texture Compatibility bootstrap exits its initial parent quickly. A short
    // delay avoids installing a transient hook there while leaving vanilla startup seamless.
    Sleep(textureChild ? 250 : 1500);

    bool controllerInstalled = false;
    bool mouseInstalled = !config.mouse.enabled;
    for (unsigned attempt = 0;
         attempt < 30 && (!controllerInstalled || !mouseInstalled);
         ++attempt) {
        if (!controllerInstalled)
            controllerInstalled = XInputHook::Install(config);
        if (!mouseInstalled)
            mouseInstalled = MouseHook::Install(config.mouse);
        if (!controllerInstalled || !mouseInstalled)
            Sleep(1000);
    }
    if (!controllerInstalled) {
        Logging::Write(L"XInput path was not uniquely validated; controller behavior left vanilla.");
    }
    if (!mouseInstalled)
        Logging::Write(L"Mouse signatures were not uniquely validated; mouse behavior left vanilla.");

    if (!config.diagnosticsEnabled) {
        Logging::Write(L"Diagnostics disabled; validated input hooks remain active.");
        return 0;
    }

    LONG lastControllerCalls = -1;
    LONG lastMouseCaptures = -1;
    for (;;) {
        Sleep(config.summaryIntervalMs);
        const LONG controllerCalls = Diagnostics::TotalCalls();
        if (controllerCalls != lastControllerCalls) {
            Diagnostics::WriteSummary();
            lastControllerCalls = controllerCalls;
        }
        const LONG mouseCaptures = MouseHook::TotalCaptures();
        if (mouseCaptures != lastMouseCaptures) {
            MouseHook::WriteSummary();
            lastMouseCaptures = mouseCaptures;
        }
    }
}

BOOL CALLBACK InitialiseOnce(PINIT_ONCE, PVOID, PVOID*) {
    if (!IsAllowedHost() || !Logging::Initialise(g_self))
        return FALSE;
    HANDLE thread = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
    if (!thread)
        return FALSE;
    CloseHandle(thread);
    return TRUE;
}

} // namespace

extern "C" BOOL WINAPI DCIF_Initialize() {
    if (!InitOnceExecuteOnce(&g_initialiseOnce, InitialiseOnce, nullptr, nullptr))
        return FALSE;
    return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

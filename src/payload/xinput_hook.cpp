#include "xinput_hook.hpp"

#include "controller_transform.hpp"
#include "diagnostics.hpp"
#include "logging.hpp"

#include <xinput.h>

#include <cstddef>
#include <cstdlib>
#include <cstdint>

namespace XInputHook {
namespace {

using GetStateFn = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);

GetStateFn g_originalGetState = nullptr;
void** g_iatSlot = nullptr;
ControllerTransformConfig g_transform;

DWORD WINAPI HookedXInputGetState(const DWORD playerIndex, XINPUT_STATE* state) {
    const GetStateFn original = g_originalGetState;
    if (!original)
        return ERROR_DEVICE_NOT_CONNECTED;

    const DWORD result = original(playerIndex, state);
    Diagnostics::Observe(playerIndex, result, state);
    if (result == ERROR_SUCCESS && state) {
        ApplyRightStickTransform(
            &state->Gamepad.sThumbRX,
            &state->Gamepad.sThumbRY,
            g_transform);
    }
    return result;
}

bool IsReadableSection(const IMAGE_SECTION_HEADER& section) {
    return (section.Characteristics & IMAGE_SCN_MEM_READ) != 0 &&
           (section.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0;
}

void** FindUniqueSlot(FARPROC getState, FARPROC getCapabilities, FARPROC setState) {
    auto* base = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (!base)
        return nullptr;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        return nullptr;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        return nullptr;
    }

    void** match = nullptr;
    std::size_t matches = 0;
    const IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    for (WORD sectionIndex = 0; sectionIndex < nt->FileHeader.NumberOfSections; ++sectionIndex) {
        const IMAGE_SECTION_HEADER& section = sections[sectionIndex];
        if (!IsReadableSection(section))
            continue;
        const DWORD size = section.Misc.VirtualSize;
        if (size < sizeof(void*) * 3 ||
            section.VirtualAddress >= nt->OptionalHeader.SizeOfImage ||
            size > nt->OptionalHeader.SizeOfImage - section.VirtualAddress) {
            continue;
        }

        std::uint8_t* start = base + section.VirtualAddress;
        const std::size_t limit = size - sizeof(void*) * 3;
        for (std::size_t offset = 0; offset <= limit; offset += sizeof(void*)) {
            auto** candidate = reinterpret_cast<void**>(start + offset);
            if (candidate[0] == getState &&
                candidate[1] == getCapabilities &&
                candidate[2] == setState) {
                match = candidate;
                ++matches;
            }
        }
    }
    return matches == 1 ? match : nullptr;
}

void LogAddressModule(const wchar_t* label, const void* address) {
    HMODULE owner = nullptr;
    wchar_t path[MAX_PATH] = L"<unknown>";
    if (address && GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address), &owner)) {
        GetModuleFileNameW(owner, path, _countof(path));
    }
    Logging::Write(L"%s address=%p module=%s", label, address, path);
}

} // namespace

bool Install(const DevelopmentConfig& config) {
    HMODULE xinput = GetModuleHandleW(L"xinput1_3.dll");
    if (!xinput)
        return false;

    FARPROC getState = GetProcAddress(xinput, "XInputGetState");
    FARPROC getCapabilities = GetProcAddress(xinput, "XInputGetCapabilities");
    FARPROC setState = GetProcAddress(xinput, "XInputSetState");
    if (!getState || !getCapabilities || !setState)
        return false;

    void** slot = FindUniqueSlot(getState, getCapabilities, setState);
    if (!slot)
        return false;

    // Publish the chain target and immutable transform before exposing the hook.
    // This prevents a racing game thread from observing a temporarily null target.
    g_originalGetState = reinterpret_cast<GetStateFn>(getState);
    g_transform = config.controller;
    Diagnostics::Configure(config.idleCalibrationSeconds);

    DWORD oldProtection = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &oldProtection))
        return false;

    void* const previous = InterlockedCompareExchangePointer(
        reinterpret_cast<void* volatile*>(slot),
        reinterpret_cast<void*>(&HookedXInputGetState),
        reinterpret_cast<void*>(getState));
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void*), oldProtection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
    if (previous != getState)
        return false;

    g_iatSlot = slot;

    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto rva = reinterpret_cast<std::uintptr_t>(slot) - base;
    Logging::Write(
        L"Installed validated XInputGetState IAT hook at RVA 0x%08lX. "
        L"transform=%d inner=%.4f game_comp=%d game_inner=%.6f "
        L"legacy_anti=%.4f square_outer=%d",
        static_cast<unsigned long>(rva), g_transform.enabled ? 1 : 0,
        g_transform.physicalInnerDeadzone,
        g_transform.compensateGameDeadzone ? 1 : 0,
        g_transform.gameInnerDeadzone, g_transform.antiDeadzone,
        g_transform.preserveSquareOuterRange ? 1 : 0);
    LogAddressModule(L"Chained XInputGetState target", getState);
    return true;
}

} // namespace XInputHook

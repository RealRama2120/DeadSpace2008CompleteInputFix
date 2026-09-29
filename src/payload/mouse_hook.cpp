#include "mouse_hook.hpp"

#include "logging.hpp"
#include "mouse_transform.hpp"

#include <windows.h>
#include <tlhelp32.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace MouseHook {
namespace {

constexpr std::int16_t kWildcard = -1;

// Dead Space EA App 1.0.0.222 signatures. Every signature is validated as a
// unique executable-section match before any game byte is changed.
constexpr std::int16_t kCapturePattern[] = {
    0x8B, 0x4E, 0x24, 0x8B, 0x54, 0x24, 0x18, 0x89,
    0x11, 0x8B, 0x46, 0x24, 0x8B, 0x4C, 0x24, 0x1C,
    0x89, 0x48, 0x04, 0x8B, 0x44, 0x24, 0x20, 0x8B,
    0x56, 0x24, 0x89, 0x42, 0x08, 0x33, 0xC0, 0x39,
};

constexpr std::int16_t kStandardCallPattern[] = {
    0xE8, kWildcard, kWildcard, kWildcard, kWildcard,
    0x8B, 0x97, 0x80, 0x05, 0x00, 0x00, 0x33, 0xC0,
    0x8D, 0x74, 0x24, 0x38, 0xC6, 0x44, 0x24, 0x38,
    0x00, 0x89, 0x44, 0x24, 0x40, 0x89, 0x44, 0x24,
    0x44, 0x89, 0x44,
};

constexpr std::int16_t kZeroGPrimaryCallPattern[] = {
    0xE8, kWildcard, kWildcard, kWildcard, kWildcard,
    0x8B, 0x7C, 0x24, 0x30, 0x0F, 0x57, 0xC9, 0xB9,
    0x24, 0x00, 0x00, 0x00, 0x8D, 0x74, 0x24, 0x40,
    0xF3, 0xA5, 0xF3, 0x0F, 0x10, 0x44, 0x24, 0x40,
    0xF3, 0x0F, 0x5C,
};

constexpr std::int16_t kZeroGVerticalCallPattern[] = {
    0xE8, kWildcard, kWildcard, kWildcard, kWildcard,
    0xD9, 0x05, kWildcard, kWildcard, kWildcard, kWildcard,
    0x0F, 0x57, 0xD2, 0x83, 0xEC, 0x08, 0xD9, 0x54,
    0x24, 0x04, 0x8D, 0x84, 0x24, 0xD8, 0x00, 0x00,
    0x00, 0xD9, 0x1C, 0x24, 0xE8,
    kWildcard, kWildcard, kWildcard, kWildcard,
};

constexpr std::int16_t kCameraFunctionPattern[] = {
    0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0,
    kWildcard, kWildcard, kWildcard, kWildcard, kWildcard, kWildcard,
    0x83, 0xE8, 0x00, 0x53, 0x8B, 0x5D, 0x08, 0x56,
    0x57, kWildcard, kWildcard, kWildcard, kWildcard, kWildcard,
    kWildcard, 0x83, 0xE8, 0x01, kWildcard, kWildcard,
};

constexpr std::int16_t kSensitivityPattern[] = {
    0xA1, kWildcard, kWildcard, kWildcard, kWildcard,
    0x0F, 0xB6, 0x88, 0x60, 0x05, 0x00, 0x00,
    0xF3, 0x0F, 0x10, 0x05, kWildcard, kWildcard, kWildcard, kWildcard,
    0xF3, 0x0F, 0x59, 0x05, kWildcard, kWildcard, kWildcard, kWildcard,
    0x8B, 0x94, 0x88, 0xE8, 0x04, 0x00, 0x00,
    0xF3, 0x0F, 0x58, 0x05, kWildcard, kWildcard, kWildcard, kWildcard,
    0xF3, 0x0F, 0x11, 0x82, 0x1C, 0x01, 0x00, 0x00, 0xC3,
};

constexpr std::int16_t kInvertPattern[] = {
    0xA2, kWildcard, kWildcard, kWildcard, kWildcard,
    0x6A, 0x00, 0xB8, kWildcard, kWildcard, kWildcard, kWildcard,
    0xE8, kWildcard, kWildcard, kWildcard, kWildcard, 0x83, 0xC4, 0x04,
    0xA2, kWildcard, kWildcard, kWildcard, kWildcard,
    0x6A, 0x00, 0xB8, kWildcard, kWildcard, kWildcard, kWildcard,
    0xE8, kWildcard, kWildcard, kWildcard, kWildcard, 0x83, 0xC4, 0x04,
    0xA2, kWildcard, kWildcard, kWildcard, kWildcard,
};

struct ExecutableImage {
    std::uint8_t* base = nullptr;
    const IMAGE_NT_HEADERS32* nt = nullptr;
};

struct CameraCallArguments {
    void* target;
    float vertical;
    float horizontal;
    float third;
};

struct PatchRecord {
    std::uint8_t* address = nullptr;
    std::array<std::uint8_t, 7> original{};
    std::array<std::uint8_t, 7> replacement{};
    std::size_t size = 0;
};

struct SuspendedThread {
    HANDLE handle = nullptr;
    DWORD id = 0;
};

MouseConfig g_config;
volatile LONG g_rawX = 0;
volatile LONG g_rawY = 0;
volatile LONG g_captureCalls = 0;
volatile LONG g_nonzeroCaptures = 0;
volatile LONG g_standardCalls = 0;
volatile LONG g_standardOverrides = 0;
volatile LONG g_zeroGPrimaryCalls = 0;
volatile LONG g_zeroGPrimaryOverrides = 0;
volatile LONG g_zeroGVerticalCalls = 0;
volatile LONG g_zeroGVerticalOverrides = 0;
volatile LONG g_installState = 0;

std::uint8_t* g_captureReturn = nullptr;
std::uint8_t* g_originalCameraFunction = nullptr;
volatile float* g_sensitivity = nullptr;
volatile BYTE* g_invertX = nullptr;
volatile BYTE* g_invertY = nullptr;

bool GetExecutableImage(ExecutableImage& image) {
    image.base = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (!image.base)
        return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image.base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        return false;
    image.nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(image.base + dos->e_lfanew);
    return image.nt->Signature == IMAGE_NT_SIGNATURE &&
           image.nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
           image.nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
}

template <std::size_t Length>
std::uint8_t* FindUniqueExecutableMatch(
    const ExecutableImage& image,
    const std::int16_t (&pattern)[Length]) {
    std::uint8_t* match = nullptr;
    std::size_t matches = 0;
    const IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(image.nt);
    for (WORD sectionIndex = 0;
         sectionIndex < image.nt->FileHeader.NumberOfSections;
         ++sectionIndex) {
        const IMAGE_SECTION_HEADER& section = sections[sectionIndex];
        if ((section.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0 ||
            section.Misc.VirtualSize < Length ||
            section.VirtualAddress >= image.nt->OptionalHeader.SizeOfImage ||
            section.Misc.VirtualSize >
                image.nt->OptionalHeader.SizeOfImage - section.VirtualAddress) {
            continue;
        }

        std::uint8_t* const start = image.base + section.VirtualAddress;
        const std::size_t limit = section.Misc.VirtualSize - Length;
        for (std::size_t offset = 0; offset <= limit; ++offset) {
            bool equal = true;
            for (std::size_t index = 0; index < Length; ++index) {
                if (pattern[index] != kWildcard &&
                    start[offset + index] != static_cast<std::uint8_t>(pattern[index])) {
                    equal = false;
                    break;
                }
            }
            if (equal) {
                match = start + offset;
                ++matches;
            }
        }
    }
    return matches == 1 ? match : nullptr;
}

bool IsWritableImageAddress(const ExecutableImage& image, const void* address) {
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    const auto base = reinterpret_cast<std::uintptr_t>(image.base);
    if (value < base || value >= base + image.nt->OptionalHeader.SizeOfImage)
        return false;
    const DWORD rva = static_cast<DWORD>(value - base);
    const IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(image.nt);
    for (WORD index = 0; index < image.nt->FileHeader.NumberOfSections; ++index) {
        const IMAGE_SECTION_HEADER& section = sections[index];
        const DWORD size = section.Misc.VirtualSize;
        if (rva >= section.VirtualAddress && rva - section.VirtualAddress < size) {
            return (section.Characteristics & IMAGE_SCN_MEM_WRITE) != 0 &&
                   (section.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0;
        }
    }
    return false;
}

std::uint8_t* RelativeCallTarget(std::uint8_t* call) {
    if (!call || call[0] != 0xE8)
        return nullptr;
    std::int32_t displacement = 0;
    std::memcpy(&displacement, call + 1, sizeof(displacement));
    return call + 5 + displacement;
}

void BuildRelativePatch(
    PatchRecord& patch,
    std::uint8_t* address,
    const std::uint8_t opcode,
    const void* target,
    const std::size_t size) {
    patch.address = address;
    patch.size = size;
    std::memcpy(patch.original.data(), address, size);
    patch.replacement = patch.original;
    patch.replacement[0] = opcode;
    const auto destination = reinterpret_cast<std::uintptr_t>(target);
    const auto next = reinterpret_cast<std::uintptr_t>(address + 5);
    const auto displacement = static_cast<std::uint32_t>(destination - next);
    std::memcpy(patch.replacement.data() + 1, &displacement, sizeof(displacement));
    for (std::size_t index = 5; index < size; ++index)
        patch.replacement[index] = 0x90;
}

bool WritePatch(const PatchRecord& patch, const bool restore) {
    DWORD oldProtection = 0;
    if (!VirtualProtect(patch.address, patch.size, PAGE_EXECUTE_READWRITE, &oldProtection))
        return false;
    const auto& bytes = restore ? patch.original : patch.replacement;
    std::memcpy(patch.address, bytes.data(), patch.size);
    DWORD ignored = 0;
    const bool protectionRestored =
        VirtualProtect(patch.address, patch.size, oldProtection, &ignored) != FALSE;
    FlushInstructionCache(GetCurrentProcess(), patch.address, patch.size);
    return protectionRestored;
}

void ResumeThreads(std::vector<SuspendedThread>& threads) {
    for (auto iterator = threads.rbegin(); iterator != threads.rend(); ++iterator) {
        ResumeThread(iterator->handle);
        CloseHandle(iterator->handle);
    }
    threads.clear();
}

bool SuspendOtherThreads(
    const std::array<PatchRecord, 4>& patches,
    std::vector<SuspendedThread>& threads) {
    const DWORD processId = GetCurrentProcessId();
    const DWORD currentThreadId = GetCurrentThreadId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    THREADENTRY32 entry = {};
    entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID != processId || entry.th32ThreadID == currentThreadId)
                continue;
            HANDLE thread = OpenThread(
                THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                FALSE, entry.th32ThreadID);
            if (!thread) {
                CloseHandle(snapshot);
                for (auto& opened : threads)
                    CloseHandle(opened.handle);
                threads.clear();
                return false;
            }
            threads.push_back({thread, entry.th32ThreadID});
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);

    std::size_t suspended = 0;
    for (; suspended < threads.size(); ++suspended) {
        if (SuspendThread(threads[suspended].handle) == static_cast<DWORD>(-1))
            break;
    }
    if (suspended != threads.size()) {
        for (std::size_t index = suspended; index < threads.size(); ++index)
            CloseHandle(threads[index].handle);
        threads.resize(suspended);
        ResumeThreads(threads);
        return false;
    }

    for (const auto& thread : threads) {
        CONTEXT context = {};
        context.ContextFlags = CONTEXT_CONTROL;
        if (!GetThreadContext(thread.handle, &context)) {
            ResumeThreads(threads);
            return false;
        }
        const auto instruction = static_cast<std::uintptr_t>(context.Eip);
        for (const auto& patch : patches) {
            const auto start = reinterpret_cast<std::uintptr_t>(patch.address);
            if (instruction >= start && instruction < start + patch.size) {
                ResumeThreads(threads);
                return false;
            }
        }
    }
    return true;
}

float ClampedVerticalDelta(void* target, const float delta) {
    if (!target)
        return delta;
    const float current = *static_cast<const float*>(target);
    return ClampMouseVerticalDelta(current, delta);
}

bool GetGameSettings(float& sensitivity, bool& invertX, bool& invertY) {
    if (!g_sensitivity || !g_invertX || !g_invertY)
        return false;
    sensitivity = *g_sensitivity;
    invertX = *g_invertX != 0;
    invertY = *g_invertY != 0;
    return true;
}

bool ApplyMouseDelta(
    CameraCallArguments* arguments,
    const MouseCameraMode mode,
    const LONG rawX,
    const LONG rawY) {
    float sensitivity = 0.0f;
    bool invertX = false;
    bool invertY = false;
    MouseCameraDelta delta;
    if (!arguments || !GetGameSettings(sensitivity, invertX, invertY) ||
        !CalculateMouseCameraDelta(
            g_config, mode, rawX, rawY, sensitivity, invertX, invertY, delta)) {
        return false;
    }
    if (mode != MouseCameraMode::ZeroGVertical)
        arguments->horizontal = delta.horizontal;
    arguments->vertical = ClampedVerticalDelta(arguments->target, delta.vertical);
    return true;
}

extern "C" void __stdcall CaptureMouseDelta(const LONG x, const LONG y) {
    InterlockedExchange(&g_rawX, x);
    InterlockedExchange(&g_rawY, y);
    InterlockedIncrement(&g_captureCalls);
    if (x != 0 || y != 0)
        InterlockedIncrement(&g_nonzeroCaptures);
}

extern "C" void __stdcall AdjustStandardCamera(CameraCallArguments* arguments) {
    InterlockedIncrement(&g_standardCalls);
    const LONG rawX = InterlockedExchange(&g_rawX, 0);
    const LONG rawY = InterlockedExchange(&g_rawY, 0);
    if (rawX == 0 && rawY == 0)
        return;
    if (ApplyMouseDelta(arguments, MouseCameraMode::Standard, rawX, rawY))
        InterlockedIncrement(&g_standardOverrides);
}

extern "C" void __stdcall AdjustZeroGPrimary(CameraCallArguments* arguments) {
    InterlockedIncrement(&g_zeroGPrimaryCalls);
    const LONG rawX = InterlockedExchange(&g_rawX, 0);
    const LONG rawY = InterlockedCompareExchange(&g_rawY, 0, 0);
    if (rawX == 0 && rawY == 0)
        return;
    if (ApplyMouseDelta(arguments, MouseCameraMode::ZeroGPrimary, rawX, rawY))
        InterlockedIncrement(&g_zeroGPrimaryOverrides);
}

extern "C" void __stdcall AdjustZeroGVertical(CameraCallArguments* arguments) {
    InterlockedIncrement(&g_zeroGVerticalCalls);
    const LONG rawY = InterlockedExchange(&g_rawY, 0);
    if (rawY == 0)
        return;

    if (ApplyMouseDelta(arguments, MouseCameraMode::ZeroGVertical, 0, rawY))
        InterlockedIncrement(&g_zeroGVerticalOverrides);
}

__declspec(naked) void CaptureDetour() {
    __asm {
        pushfd
        pushad
        mov ecx, dword ptr [esp + 0Ch]
        mov eax, dword ptr [ecx + 1Ch]
        mov edx, dword ptr [ecx + 20h]
        push edx
        push eax
        call CaptureMouseDelta
        popad
        popfd
        mov ecx, dword ptr [esi + 24h]
        mov edx, dword ptr [esp + 18h]
        jmp dword ptr [g_captureReturn]
    }
}

__declspec(naked) void StandardCameraDetour() {
    __asm {
        lea eax, dword ptr [esp + 4]
        pushad
        push eax
        call AdjustStandardCamera
        popad
        xor eax, eax
        jmp dword ptr [g_originalCameraFunction]
    }
}

__declspec(naked) void ZeroGPrimaryCameraDetour() {
    __asm {
        lea eax, dword ptr [esp + 4]
        pushad
        push eax
        call AdjustZeroGPrimary
        popad
        xor eax, eax
        jmp dword ptr [g_originalCameraFunction]
    }
}

__declspec(naked) void ZeroGVerticalCameraDetour() {
    __asm {
        lea eax, dword ptr [esp + 4]
        pushad
        push eax
        call AdjustZeroGVertical
        popad
        xor eax, eax
        jmp dword ptr [g_originalCameraFunction]
    }
}

} // namespace

bool Install(const MouseConfig& config) {
    if (!config.enabled)
        return true;
    if (InterlockedCompareExchange(&g_installState, 1, 0) != 0)
        return InterlockedCompareExchange(&g_installState, 0, 0) == 2;

    ExecutableImage image;
    std::uint8_t* const capture =
        GetExecutableImage(image) ? FindUniqueExecutableMatch(image, kCapturePattern) : nullptr;
    std::uint8_t* const standardCall =
        capture ? FindUniqueExecutableMatch(image, kStandardCallPattern) : nullptr;
    std::uint8_t* const zeroGPrimaryCall =
        capture ? FindUniqueExecutableMatch(image, kZeroGPrimaryCallPattern) : nullptr;
    std::uint8_t* const zeroGVerticalCall =
        capture ? FindUniqueExecutableMatch(image, kZeroGVerticalCallPattern) : nullptr;
    std::uint8_t* const cameraFunction =
        capture ? FindUniqueExecutableMatch(image, kCameraFunctionPattern) : nullptr;
    std::uint8_t* const sensitivityMatch =
        capture ? FindUniqueExecutableMatch(image, kSensitivityPattern) : nullptr;
    std::uint8_t* const invertMatch =
        capture ? FindUniqueExecutableMatch(image, kInvertPattern) : nullptr;

    if (!capture || !standardCall || !zeroGPrimaryCall || !zeroGVerticalCall ||
        !cameraFunction || !sensitivityMatch || !invertMatch ||
        RelativeCallTarget(standardCall) != cameraFunction ||
        RelativeCallTarget(zeroGPrimaryCall) != cameraFunction ||
        RelativeCallTarget(zeroGVerticalCall) != cameraFunction) {
        InterlockedExchange(&g_installState, 0);
        return false;
    }

    volatile float* sensitivity = nullptr;
    volatile BYTE* invertX = nullptr;
    volatile BYTE* invertY = nullptr;
    std::memcpy(&sensitivity, sensitivityMatch + 16, sizeof(sensitivity));
    std::memcpy(&invertX, invertMatch + 21, sizeof(invertX));
    std::memcpy(&invertY, invertMatch + 41, sizeof(invertY));
    if (!IsWritableImageAddress(image, const_cast<float*>(sensitivity)) ||
        !IsWritableImageAddress(image, const_cast<BYTE*>(invertX)) ||
        !IsWritableImageAddress(image, const_cast<BYTE*>(invertY)) ||
        reinterpret_cast<std::uintptr_t>(invertY) !=
            reinterpret_cast<std::uintptr_t>(invertX) + 1 ||
        reinterpret_cast<std::uintptr_t>(sensitivity) !=
            reinterpret_cast<std::uintptr_t>(invertX) + 0x17) {
        InterlockedExchange(&g_installState, 0);
        return false;
    }

    g_config = config;
    g_captureReturn = capture + 7;
    g_originalCameraFunction = cameraFunction;
    g_sensitivity = sensitivity;
    g_invertX = invertX;
    g_invertY = invertY;

    std::array<PatchRecord, 4> patches;
    BuildRelativePatch(patches[0], capture, 0xE9, &CaptureDetour, 7);
    BuildRelativePatch(patches[1], standardCall, 0xE8, &StandardCameraDetour, 5);
    BuildRelativePatch(patches[2], zeroGPrimaryCall, 0xE8, &ZeroGPrimaryCameraDetour, 5);
    BuildRelativePatch(patches[3], zeroGVerticalCall, 0xE8, &ZeroGVerticalCameraDetour, 5);

    std::vector<SuspendedThread> threads;
    threads.reserve(64);
    if (!SuspendOtherThreads(patches, threads)) {
        InterlockedExchange(&g_installState, 0);
        return false;
    }

    std::size_t applied = 0;
    for (; applied < patches.size(); ++applied) {
        if (!WritePatch(patches[applied], false))
            break;
    }
    if (applied != patches.size()) {
        while (applied > 0) {
            --applied;
            WritePatch(patches[applied], true);
        }
        ResumeThreads(threads);
        InterlockedExchange(&g_installState, 0);
        return false;
    }
    ResumeThreads(threads);
    InterlockedExchange(&g_installState, 2);

    const auto base = reinterpret_cast<std::uintptr_t>(image.base);
    Logging::Write(
        L"Installed fail-closed mouse hooks. capture_rva=0x%08lX "
        L"standard_rva=0x%08lX zero_primary_rva=0x%08lX "
        L"zero_vertical_rva=0x%08lX camera_rva=0x%08lX "
        L"sensitivity_rva=0x%08lX invert_x_rva=0x%08lX invert_y_rva=0x%08lX",
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(capture) - base),
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(standardCall) - base),
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(zeroGPrimaryCall) - base),
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(zeroGVerticalCall) - base),
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(cameraFunction) - base),
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(sensitivity) - base),
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(invertX) - base),
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(invertY) - base));
    Logging::Write(
        L"Mouse scales sensitivity_offset=%.4f standard=(%.8f,%.8f) "
        L"zero_g=(%.8f,%.8f); zero delta preserves vanilla camera arguments.",
        g_config.sensitivityOffset,
        g_config.standardHorizontalScale, g_config.standardVerticalScale,
        g_config.zeroGHorizontalScale, g_config.zeroGVerticalScale);
    return true;
}

long TotalCaptures() {
    return InterlockedCompareExchange(&g_captureCalls, 0, 0);
}

void WriteSummary() {
    Logging::Write(
        L"Mouse summary captures=%ld nonzero=%ld standard=%ld/%ld "
        L"zero_primary=%ld/%ld zero_vertical=%ld/%ld pending_raw=(%ld,%ld)",
        InterlockedCompareExchange(&g_captureCalls, 0, 0),
        InterlockedCompareExchange(&g_nonzeroCaptures, 0, 0),
        InterlockedCompareExchange(&g_standardOverrides, 0, 0),
        InterlockedCompareExchange(&g_standardCalls, 0, 0),
        InterlockedCompareExchange(&g_zeroGPrimaryOverrides, 0, 0),
        InterlockedCompareExchange(&g_zeroGPrimaryCalls, 0, 0),
        InterlockedCompareExchange(&g_zeroGVerticalOverrides, 0, 0),
        InterlockedCompareExchange(&g_zeroGVerticalCalls, 0, 0),
        InterlockedCompareExchange(&g_rawX, 0, 0),
        InterlockedCompareExchange(&g_rawY, 0, 0));
}

} // namespace MouseHook

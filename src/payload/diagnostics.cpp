#include "diagnostics.hpp"

#include "logging.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>

namespace Diagnostics {
namespace {

volatile LONG g_calls = 0;
volatile LONG g_successes = 0;
volatile LONG g_windowCalls = 0;
volatile LONG g_windowSuccesses = 0;
volatile LONG g_connectedMask = 0;
volatile LONG g_firstSuccessTick = 0;
volatile LONG g_maxIdleMagnitude = 0;
volatile LONG g_maxIdleAbsX = 0;
volatile LONG g_maxIdleAbsY = 0;
volatile LONG g_minNonZeroMagnitude = LONG_MAX;
volatile LONG g_maxMagnitude = 0;
volatile LONG g_windowMinNonZeroMagnitude = LONG_MAX;
volatile LONG g_windowMaxMagnitude = 0;
volatile LONG g_lastRightX = 0;
volatile LONG g_lastRightY = 0;
volatile LONG g_magnitudeBuckets[16] = {};
DWORD g_idleCalibrationMs = 5000;

void UpdateMaximum(volatile LONG* destination, const LONG value) {
    LONG observed = *destination;
    while (value > observed) {
        const LONG previous = InterlockedCompareExchange(destination, value, observed);
        if (previous == observed)
            return;
        observed = previous;
    }
}

void UpdateMinimum(volatile LONG* destination, const LONG value) {
    LONG observed = *destination;
    while (value < observed) {
        const LONG previous = InterlockedCompareExchange(destination, value, observed);
        if (previous == observed)
            return;
        observed = previous;
    }
}

LONG AbsoluteAxis(const SHORT value) {
    return value == INT16_MIN ? 32768L : std::abs(static_cast<LONG>(value));
}

} // namespace

void Configure(const DWORD idleCalibrationSeconds) {
    g_idleCalibrationMs = std::clamp<DWORD>(idleCalibrationSeconds, 1, 30) * 1000;
}

void Observe(const DWORD playerIndex, const DWORD result, const XINPUT_STATE* state) {
    InterlockedIncrement(&g_calls);
    InterlockedIncrement(&g_windowCalls);
    if (result != ERROR_SUCCESS || !state)
        return;

    InterlockedIncrement(&g_successes);
    InterlockedIncrement(&g_windowSuccesses);
    if (playerIndex < 4)
        InterlockedOr(&g_connectedMask, 1L << playerIndex);

    const LONG absoluteX = AbsoluteAxis(state->Gamepad.sThumbRX);
    const LONG absoluteY = AbsoluteAxis(state->Gamepad.sThumbRY);
    const LONG magnitude = static_cast<LONG>(std::lround(std::sqrt(
        static_cast<double>(absoluteX) * absoluteX +
        static_cast<double>(absoluteY) * absoluteY)));

    UpdateMaximum(&g_maxMagnitude, magnitude);
    UpdateMaximum(&g_windowMaxMagnitude, magnitude);
    if (magnitude > 0) {
        UpdateMinimum(&g_minNonZeroMagnitude, magnitude);
        UpdateMinimum(&g_windowMinNonZeroMagnitude, magnitude);
    }
    InterlockedExchange(&g_lastRightX, state->Gamepad.sThumbRX);
    InterlockedExchange(&g_lastRightY, state->Gamepad.sThumbRY);
    const LONG bucket = std::min<LONG>(15, magnitude / 2897);
    InterlockedIncrement(&g_magnitudeBuckets[bucket]);

    LONG start = g_firstSuccessTick;
    if (start == 0) {
        const LONG now = static_cast<LONG>(GetTickCount());
        InterlockedCompareExchange(&g_firstSuccessTick, now, 0);
        start = g_firstSuccessTick;
    }
    const DWORD elapsed = GetTickCount() - static_cast<DWORD>(start);
    if (elapsed <= g_idleCalibrationMs) {
        UpdateMaximum(&g_maxIdleMagnitude, magnitude);
        UpdateMaximum(&g_maxIdleAbsX, absoluteX);
        UpdateMaximum(&g_maxIdleAbsY, absoluteY);
    }
}

void WriteSummary() {
    const LONG calls = g_calls;
    const LONG successes = g_successes;
    const LONG minimum = g_minNonZeroMagnitude == LONG_MAX ? 0 : g_minNonZeroMagnitude;
    const LONG windowCalls = InterlockedExchange(&g_windowCalls, 0);
    const LONG windowSuccesses = InterlockedExchange(&g_windowSuccesses, 0);
    const LONG windowMinimumRaw = InterlockedExchange(&g_windowMinNonZeroMagnitude, LONG_MAX);
    const LONG windowMinimum = windowMinimumRaw == LONG_MAX ? 0 : windowMinimumRaw;
    const LONG windowMaximum = InterlockedExchange(&g_windowMaxMagnitude, 0);
    wchar_t buckets[512] = {};
    std::size_t used = 0;
    for (std::size_t index = 0; index < _countof(g_magnitudeBuckets); ++index) {
        const int written = _snwprintf_s(
            buckets + used, _countof(buckets) - used, _TRUNCATE,
            index == 0 ? L"%ld" : L",%ld", g_magnitudeBuckets[index]);
        if (written <= 0)
            break;
        used += static_cast<std::size_t>(written);
    }

    Logging::Write(
        L"XInput summary: total=(%ld/%ld) window=(%ld/%ld) slots=0x%lX "
        L"last_right=(%ld,%ld) window_magnitude=(%ld..%ld) idle_max=(%ld,%ld) "
        L"idle_magnitude=%ld total_nonzero_min=%ld total_max=%ld buckets=[%s]",
        calls, successes, windowCalls, windowSuccesses, g_connectedMask,
        g_lastRightX, g_lastRightY, windowMinimum, windowMaximum,
        g_maxIdleAbsX, g_maxIdleAbsY, g_maxIdleMagnitude, minimum,
        g_maxMagnitude, buckets);
}

LONG TotalCalls() {
    return g_calls;
}

} // namespace Diagnostics

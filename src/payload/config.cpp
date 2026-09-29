#include "config.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cwchar>

namespace {

bool ModuleSiblingPath(HMODULE module, const wchar_t* leaf, wchar_t* output, const std::size_t capacity) {
    const DWORD length = GetModuleFileNameW(module, output, static_cast<DWORD>(capacity));
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

bool ReadBool(const wchar_t* path, const wchar_t* section, const wchar_t* key, const bool fallback) {
    return GetPrivateProfileIntW(section, key, fallback ? 1 : 0, path) != 0;
}

DWORD ReadBoundedDword(
    const wchar_t* path,
    const wchar_t* section,
    const wchar_t* key,
    const DWORD fallback,
    const DWORD minimum,
    const DWORD maximum) {
    const UINT value = GetPrivateProfileIntW(section, key, fallback, path);
    return std::clamp<DWORD>(value, minimum, maximum);
}

float ReadBoundedFloat(
    const wchar_t* path,
    const wchar_t* section,
    const wchar_t* key,
    const float fallback,
    const float minimum,
    const float maximum) {
    wchar_t fallbackText[32] = {};
    // Preserve small camera scales and measured deadzone constants when the INI
    // or an individual key is absent. Four decimal places materially rounded
    // the 1/1200 zero-G scale and the verified 8689/32767 game deadzone.
    std::swprintf(fallbackText, _countof(fallbackText), L"%.9g", fallback);
    wchar_t text[64] = {};
    GetPrivateProfileStringW(section, key, fallbackText, text, _countof(text), path);
    wchar_t* end = nullptr;
    const double parsed = std::wcstod(text, &end);
    if (end == text || *end != L'\0' || !std::isfinite(parsed))
        return fallback;
    return std::clamp(static_cast<float>(parsed), minimum, maximum);
}

} // namespace

DevelopmentConfig LoadConfiguration(HMODULE module) {
    DevelopmentConfig config;
    wchar_t path[MAX_PATH] = {};
    if (!ModuleSiblingPath(module, L"DeadSpaceCompleteInputFix.ini", path, _countof(path)))
        return config;

    config.diagnosticsEnabled = ReadBool(path, L"Development", L"DiagnosticsEnabled", false);
    config.summaryIntervalMs = ReadBoundedDword(
        path, L"Development", L"SummaryIntervalMs", 10000, 1000, 60000);
    config.idleCalibrationSeconds = ReadBoundedDword(
        path, L"Development", L"IdleCalibrationSeconds", 5, 1, 30);
    config.controller.enabled = ReadBool(
        path, L"ControllerExperiment", L"Enabled", true);
    config.controller.physicalInnerDeadzone = ReadBoundedFloat(
        path, L"ControllerExperiment", L"PhysicalInnerDeadzone", 0.11f, 0.0f, 0.50f);
    config.controller.compensateGameDeadzone = ReadBool(
        path, L"ControllerExperiment", L"CompensateGameDeadzone", true);
    config.controller.gameInnerDeadzone = ReadBoundedFloat(
        path, L"ControllerExperiment", L"GameInnerDeadzone",
        8689.0f / 32767.0f, 0.0f, 0.75f);
    config.controller.antiDeadzone = ReadBoundedFloat(
        path, L"ControllerExperiment", L"AntiDeadzone", 0.14f, 0.0f, 0.75f);
    config.controller.preserveSquareOuterRange = ReadBool(
        path, L"ControllerExperiment", L"PreserveSquareOuterRange", true);
    config.mouse.enabled = ReadBool(path, L"MouseExperiment", L"Enabled", true);
    config.mouse.sensitivityOffset = ReadBoundedFloat(
        path, L"MouseExperiment", L"SensitivityOffset", 0.01f, 0.0f, 0.25f);
    config.mouse.standardHorizontalScale = ReadBoundedFloat(
        path, L"MouseExperiment", L"StandardHorizontalScale",
        -1.0f / 1000.0f, -0.01f, -0.00001f);
    config.mouse.standardVerticalScale = ReadBoundedFloat(
        path, L"MouseExperiment", L"StandardVerticalScale",
        1.0f / 1000.0f, 0.00001f, 0.01f);
    config.mouse.zeroGHorizontalScale = ReadBoundedFloat(
        path, L"MouseExperiment", L"ZeroGHorizontalScale",
        -1.0f / 1200.0f, -0.01f, -0.00001f);
    config.mouse.zeroGVerticalScale = ReadBoundedFloat(
        path, L"MouseExperiment", L"ZeroGVerticalScale",
        1.0f / 1200.0f, 0.00001f, 0.01f);
    return config;
}

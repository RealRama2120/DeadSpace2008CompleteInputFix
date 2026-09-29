#pragma once

#include <windows.h>
#include <xinput.h>

namespace Diagnostics {

void Configure(DWORD idleCalibrationSeconds);
void Observe(DWORD playerIndex, DWORD result, const XINPUT_STATE* state);
void WriteSummary();
LONG TotalCalls();

} // namespace Diagnostics


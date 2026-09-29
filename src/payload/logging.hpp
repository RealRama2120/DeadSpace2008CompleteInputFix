#pragma once

#include <windows.h>

namespace Logging {

bool Initialise(HMODULE module);
void Write(const wchar_t* format, ...);

} // namespace Logging


#pragma once

#include "config.hpp"

namespace MouseHook {

bool Install(const MouseConfig& config);
long TotalCaptures();
void WriteSummary();

} // namespace MouseHook

#pragma once

#include <cstdint>

struct ControllerTransformConfig {
    bool enabled = true;
    float physicalInnerDeadzone = 0.11f;
    bool compensateGameDeadzone = true;
    float gameInnerDeadzone = 8689.0f / 32767.0f;
    // Retained only for reproducing the earlier development profiles.
    float antiDeadzone = 0.14f;
    bool preserveSquareOuterRange = true;
};

void ApplyRightStickTransform(
    std::int16_t* x,
    std::int16_t* y,
    const ControllerTransformConfig& config);

#pragma once

struct MouseConfig {
    bool enabled = true;
    float sensitivityOffset = 0.01f;
    float standardHorizontalScale = -1.0f / 1000.0f;
    float standardVerticalScale = 1.0f / 1000.0f;
    float zeroGHorizontalScale = -1.0f / 1200.0f;
    float zeroGVerticalScale = 1.0f / 1200.0f;
};

enum class MouseCameraMode {
    Standard,
    ZeroGPrimary,
    ZeroGVertical,
};

struct MouseCameraDelta {
    float horizontal = 0.0f;
    float vertical = 0.0f;
};

bool CalculateMouseCameraDelta(
    const MouseConfig& config,
    MouseCameraMode mode,
    long rawX,
    long rawY,
    float gameSensitivity,
    bool invertX,
    bool invertY,
    MouseCameraDelta& output);

float ClampMouseVerticalDelta(float current, float delta);

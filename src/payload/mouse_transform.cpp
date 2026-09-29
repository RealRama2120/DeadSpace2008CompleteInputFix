#include "mouse_transform.hpp"

#include <cmath>

bool CalculateMouseCameraDelta(
    const MouseConfig& config,
    const MouseCameraMode mode,
    const long rawX,
    const long rawY,
    const float gameSensitivity,
    const bool invertX,
    const bool invertY,
    MouseCameraDelta& output) {
    output = {};
    if (rawX == 0 && rawY == 0)
        return false;

    const float sensitivity = gameSensitivity + config.sensitivityOffset;
    if (!std::isfinite(sensitivity) || sensitivity <= 0.0f || sensitivity > 4.0f)
        return false;

    const bool standard = mode == MouseCameraMode::Standard;
    const bool verticalOnly = mode == MouseCameraMode::ZeroGVertical;
    const float horizontalScale =
        standard ? config.standardHorizontalScale : config.zeroGHorizontalScale;
    const float verticalScale =
        standard ? config.standardVerticalScale : config.zeroGVerticalScale;
    if (!std::isfinite(horizontalScale) || !std::isfinite(verticalScale))
        return false;

    if (!verticalOnly)
        output.horizontal = static_cast<float>(rawX) * horizontalScale * sensitivity;
    output.vertical = static_cast<float>(rawY) * verticalScale * sensitivity;
    if (invertX)
        output.horizontal = -output.horizontal;
    if (invertY)
        output.vertical = -output.vertical;
    return true;
}

float ClampMouseVerticalDelta(const float current, const float delta) {
    if (!std::isfinite(current) || !std::isfinite(delta))
        return delta;
    const float candidate = current + delta;
    if (candidate > 1.0f)
        return 1.0f - current;
    if (candidate < -1.0f)
        return -1.0f - current;
    return delta;
}

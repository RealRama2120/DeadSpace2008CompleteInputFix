#include "controller_transform.hpp"

#include <algorithm>
#include <cmath>

namespace {

float NormaliseAxis(const std::int16_t value) {
    if (value == INT16_MIN)
        return -1.0f;
    return static_cast<float>(value) / 32767.0f;
}

std::int16_t EncodeAxis(const float value) {
    const float clamped = std::clamp(value, -1.0f, 1.0f);
    const long rounded = std::lround(clamped * 32767.0f);
    return static_cast<std::int16_t>(std::clamp(rounded, -32767L, 32767L));
}

float InvertDeadSpaceMagnitude(const float targetMagnitude, const float gameDeadzone) {
    // The unpacked EA executable applies this response after XInputGetState:
    //   output = input * (magnitude - deadzone) / (1 - deadzone)
    // for magnitudes between its deadzone and the unit-radius boundary. Solve
    // that quadratic for the input magnitude which produces targetMagnitude.
    // Above unit radius the game clamps only the scale calculation and leaves
    // the square XInput axes intact, so no inverse is needed there.
    if (targetMagnitude >= 1.0f)
        return targetMagnitude;
    const float deadzone = std::clamp(gameDeadzone, 0.0f, 0.95f);
    const float discriminant = deadzone * deadzone +
        4.0f * (1.0f - deadzone) * std::max(targetMagnitude, 0.0f);
    return 0.5f * (deadzone + std::sqrt(discriminant));
}

} // namespace

void ApplyRightStickTransform(
    std::int16_t* x,
    std::int16_t* y,
    const ControllerTransformConfig& config) {
    if (!x || !y || !config.enabled)
        return;

    const float inputX = NormaliseAxis(*x);
    const float inputY = NormaliseAxis(*y);
    const float magnitude = std::sqrt(inputX * inputX + inputY * inputY);
    const float inner = std::clamp(config.physicalInnerDeadzone, 0.0f, 0.95f);

    if (magnitude <= inner || magnitude <= 0.0f) {
        *x = 0;
        *y = 0;
        return;
    }

    const float directionX = inputX / magnitude;
    const float directionY = inputY / magnitude;
    // XInput exposes a square axis range. Scale to the edge of that square in
    // the current direction so a fully deflected diagonal remains fully
    // deflected on both axes instead of being collapsed onto a unit circle.
    const float maximumDirectionComponent = std::max(std::abs(directionX), std::abs(directionY));
    const float outerMagnitude = config.preserveSquareOuterRange
        ? 1.0f / maximumDirectionComponent
        : 1.0f;
    const float usableMagnitude = std::clamp(
        (magnitude - inner) / (outerMagnitude - inner), 0.0f, 1.0f);
    float outputMagnitude = 0.0f;
    if (config.compensateGameDeadzone) {
        const float targetMagnitude = usableMagnitude * outerMagnitude;
        outputMagnitude = InvertDeadSpaceMagnitude(
            targetMagnitude, config.gameInnerDeadzone);
    }
    else {
        const float anti = std::clamp(config.antiDeadzone, 0.0f, 0.95f);
        outputMagnitude = anti + usableMagnitude * (outerMagnitude - anti);
    }
    *x = EncodeAxis(directionX * outputMagnitude);
    *y = EncodeAxis(directionY * outputMagnitude);
}

#include "../src/payload/controller_transform.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

bool Expect(const bool condition, const char* message) {
    if (!condition)
        std::printf("FAIL: %s\n", message);
    return condition;
}

float NormaliseTestAxis(const std::int16_t value) {
    return value == INT16_MIN ? -1.0f : static_cast<float>(value) / 32767.0f;
}

void ApplyDeadSpaceRightStickResponse(std::int16_t* x, std::int16_t* y) {
    constexpr float kGameDeadzone = 8689.0f / 32767.0f;
    const float inputX = NormaliseTestAxis(*x);
    const float inputY = NormaliseTestAxis(*y);
    const float magnitude = std::min(
        std::sqrt(inputX * inputX + inputY * inputY), 1.0f);
    if (magnitude <= kGameDeadzone) {
        *x = 0;
        *y = 0;
        return;
    }
    const float scale = (magnitude - kGameDeadzone) / (1.0f - kGameDeadzone);
    *x = static_cast<std::int16_t>(std::lround(inputX * scale * 32767.0f));
    *y = static_cast<std::int16_t>(std::lround(inputY * scale * 32767.0f));
}

} // namespace

int main() {
    ControllerTransformConfig config;
    config.enabled = false;
    std::int16_t x = 4000;
    std::int16_t y = -5000;
    ApplyRightStickTransform(&x, &y, config);
    if (!Expect(x == 4000 && y == -5000, "disabled transform must be bit-transparent"))
        return 1;

    config.enabled = true;
    config.compensateGameDeadzone = false;
    config.physicalInnerDeadzone = 0.10f;
    config.antiDeadzone = 0.20f;
    x = 1000;
    y = 1000;
    ApplyRightStickTransform(&x, &y, config);
    if (!Expect(x == 0 && y == 0, "values inside the radial deadzone must be zero"))
        return 2;

    x = 32767;
    y = 0;
    ApplyRightStickTransform(&x, &y, config);
    if (!Expect(x == 32767 && y == 0, "full cardinal range must be preserved"))
        return 3;

    x = 32767;
    y = 32767;
    ApplyRightStickTransform(&x, &y, config);
    if (!Expect(x == 32767 && y == 32767,
                "full diagonal axis range must preserve the XInput square boundary"))
        return 4;

    config.preserveSquareOuterRange = false;
    x = 32767;
    y = 32767;
    ApplyRightStickTransform(&x, &y, config);
    if (!Expect(x >= 23169 && x <= 23171 && y == x,
                "comparison mode must reproduce the measured experiment's circular outer range"))
        return 5;
    config.preserveSquareOuterRange = true;

    x = 32767;
    y = 16000;
    ApplyRightStickTransform(&x, &y, config);
    if (!Expect(x == 32767 && std::abs(y - 16000) <= 1,
                "mixed full-stick directions must preserve the square boundary"))
        return 6;

    x = 12000;
    y = 12000;
    ApplyRightStickTransform(&x, &y, config);
    if (!Expect(x > 0 && y > 0 && x == y,
                "diagonal direction must remain radial and symmetric"))
        return 7;

    config.physicalInnerDeadzone = 0.11f;
    config.antiDeadzone = 0.14f;
    config.compensateGameDeadzone = true;
    x = 2809;
    y = 254;
    ApplyRightStickTransform(&x, &y, config);
    if (!Expect(x == 0 && y == 0, "worst measured idle range must remain suppressed"))
        return 8;

    x = 3800;
    y = 0;
    ApplyRightStickTransform(&x, &y, config);
    if (!Expect(x > 8689 && y == 0,
                "first surviving input must clear the verified game deadzone"))
        return 9;
    ApplyDeadSpaceRightStickResponse(&x, &y);
    if (!Expect(x > 0 && x < 400 && y == 0,
                "composed game response must begin with a small nonzero value"))
        return 10;

    x = 3606;
    y = 0;
    ApplyRightStickTransform(&x, &y, config);
    ApplyDeadSpaceRightStickResponse(&x, &y);
    if (!Expect(x >= 0 && x <= 4 && y == 0,
                "the first raw step outside the hardware cutoff must not jump"))
        return 11;

    x = 18186;
    y = 0;
    ApplyRightStickTransform(&x, &y, config);
    ApplyDeadSpaceRightStickResponse(&x, &y);
    if (!Expect(x >= 16380 && x <= 16390 && y == 0,
                "the verified game transform must compose to a linear midpoint"))
        return 12;

    x = 0;
    y = 18186;
    ApplyRightStickTransform(&x, &y, config);
    ApplyDeadSpaceRightStickResponse(&x, &y);
    if (!Expect(y >= 16380 && y <= 16390 && x == 0,
                "horizontal and vertical composed response must be identical"))
        return 13;

    constexpr std::int16_t samples[] = {
        -32767, -24576, -16384, -8192, -4096, 0, 4096, 8192, 16384, 24576, 32767
    };
    for (const std::int16_t rawX : samples) {
        for (const std::int16_t rawY : samples) {
            const float inputX = NormaliseTestAxis(rawX);
            const float inputY = NormaliseTestAxis(rawY);
            const float magnitude = std::sqrt(inputX * inputX + inputY * inputY);
            int expectedX = 0;
            int expectedY = 0;
            if (magnitude > config.physicalInnerDeadzone) {
                const float directionX = inputX / magnitude;
                const float directionY = inputY / magnitude;
                const float outerMagnitude = 1.0f /
                    std::max(std::abs(directionX), std::abs(directionY));
                const float usable = std::clamp(
                    (magnitude - config.physicalInnerDeadzone) /
                        (outerMagnitude - config.physicalInnerDeadzone),
                    0.0f, 1.0f);
                expectedX = static_cast<int>(std::lround(
                    directionX * usable * outerMagnitude * 32767.0f));
                expectedY = static_cast<int>(std::lround(
                    directionY * usable * outerMagnitude * 32767.0f));
            }

            x = rawX;
            y = rawY;
            ApplyRightStickTransform(&x, &y, config);
            ApplyDeadSpaceRightStickResponse(&x, &y);
            if (!Expect(
                    std::abs(static_cast<int>(x) - expectedX) <= 5 &&
                    std::abs(static_cast<int>(y) - expectedY) <= 5,
                    "inverse must compose with the game response across the XInput square"))
                return 14;
        }
    }

    std::int16_t previous = 0;
    for (int raw = 0; raw <= 32767; raw += 127) {
        x = static_cast<std::int16_t>(raw);
        y = 0;
        ApplyRightStickTransform(&x, &y, config);
        if (!Expect(x >= previous, "cardinal response must be monotonic"))
            return 15;
        previous = x;
    }

    x = 14500;
    y = -9300;
    ApplyRightStickTransform(&x, &y, config);
    const std::int16_t positiveX = x;
    const std::int16_t positiveY = y;
    x = -14500;
    y = 9300;
    ApplyRightStickTransform(&x, &y, config);
    if (!Expect(std::abs(static_cast<int>(x) + positiveX) <= 1 &&
                std::abs(static_cast<int>(y) + positiveY) <= 1,
                "response must remain odd-symmetric across stick direction"))
        return 16;

    std::printf("PASS: controller transform invariants hold.\n");
    return 0;
}

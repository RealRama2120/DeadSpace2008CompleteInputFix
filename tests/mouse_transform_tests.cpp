#include "../src/payload/mouse_transform.hpp"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

bool NearlyEqual(const float left, const float right) {
    return std::fabs(left - right) <= 0.000001f;
}

bool Expect(const bool condition, const char* message) {
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

} // namespace

int main() {
    MouseConfig config;
    MouseCameraDelta delta;
    bool passed = true;

    passed &= Expect(
        !CalculateMouseCameraDelta(
            config, MouseCameraMode::Standard, 0, 0, 0.49f, false, false, delta),
        "zero input must preserve vanilla camera arguments");
    passed &= Expect(
        CalculateMouseCameraDelta(
            config, MouseCameraMode::Standard, 1000, 1000, 0.49f, false, false, delta),
        "standard input must produce a delta");
    passed &= Expect(
        NearlyEqual(delta.horizontal, -0.5f) && NearlyEqual(delta.vertical, 0.5f),
        "standard scaling must match the recovered reference behavior");

    CalculateMouseCameraDelta(
        config, MouseCameraMode::Standard, 1000, 1000, 0.49f, true, true, delta);
    passed &= Expect(
        NearlyEqual(delta.horizontal, 0.5f) && NearlyEqual(delta.vertical, -0.5f),
        "game inversion settings must remain effective");

    CalculateMouseCameraDelta(
        config, MouseCameraMode::ZeroGPrimary, 1200, 1200, 0.49f, false, false, delta);
    passed &= Expect(
        NearlyEqual(delta.horizontal, -0.5f) && NearlyEqual(delta.vertical, 0.5f),
        "zero-G scaling must match the recovered reference behavior");

    CalculateMouseCameraDelta(
        config, MouseCameraMode::ZeroGVertical, 1200, 1200, 0.49f, false, false, delta);
    passed &= Expect(
        NearlyEqual(delta.horizontal, 0.0f) && NearlyEqual(delta.vertical, 0.5f),
        "zero-G vertical pass must leave the horizontal argument alone");

    passed &= Expect(
        NearlyEqual(ClampMouseVerticalDelta(0.9f, 0.4f), 0.1f) &&
        NearlyEqual(ClampMouseVerticalDelta(-0.9f, -0.4f), -0.1f) &&
        NearlyEqual(ClampMouseVerticalDelta(0.25f, 0.4f), 0.4f),
        "vertical camera bounds must be preserved");

    passed &= Expect(
        !CalculateMouseCameraDelta(
            config, MouseCameraMode::Standard, 1, 1,
            std::numeric_limits<float>::quiet_NaN(), false, false, delta),
        "invalid sensitivity must fail safely");

    MouseConfig invalidScale = config;
    invalidScale.standardHorizontalScale = std::numeric_limits<float>::quiet_NaN();
    passed &= Expect(
        !CalculateMouseCameraDelta(
            invalidScale, MouseCameraMode::Standard, 1, 1,
            1.0f, false, false, delta),
        "invalid transform scale must fail safely");

    if (!passed)
        return 1;
    std::cout << "PASS: mouse transform invariants hold.\n";
    return 0;
}

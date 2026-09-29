#include "../src/payload/config.hpp"

#include <cmath>
#include <cstdio>

namespace {

bool Near(const float left, const float right) {
    return std::fabs(left - right) < 0.000001f;
}

bool Check(const bool condition, const char* message) {
    if (condition)
        return true;
    std::fprintf(stderr, "FAIL: %s\n", message);
    return false;
}

} // namespace

int wmain() {
    const DevelopmentConfig config = LoadConfiguration(GetModuleHandleW(nullptr));
    bool ok = true;
    ok &= Check(!config.diagnosticsEnabled, "missing INI must keep diagnostics disabled");
    ok &= Check(config.controller.enabled, "missing INI must enable the controller fix");
    ok &= Check(Near(config.controller.physicalInnerDeadzone, 0.11f),
        "missing INI must use the accepted physical deadzone");
    ok &= Check(config.controller.compensateGameDeadzone,
        "missing INI must compensate the verified game deadzone");
    ok &= Check(Near(config.controller.gameInnerDeadzone, 8689.0f / 32767.0f),
        "missing INI must use the verified game deadzone");
    ok &= Check(config.controller.preserveSquareOuterRange,
        "missing INI must preserve full square outer range");
    ok &= Check(config.mouse.enabled, "missing INI must enable the mouse fix");
    ok &= Check(Near(config.mouse.sensitivityOffset, 0.01f),
        "missing INI must use the accepted mouse sensitivity offset");
    ok &= Check(Near(config.mouse.standardHorizontalScale, -1.0f / 1000.0f) &&
        Near(config.mouse.standardVerticalScale, 1.0f / 1000.0f),
        "missing INI must use the accepted standard-camera scales");
    ok &= Check(Near(config.mouse.zeroGHorizontalScale, -1.0f / 1200.0f) &&
        Near(config.mouse.zeroGVerticalScale, 1.0f / 1200.0f),
        "missing INI must use the conservative zero-G reference scales");
    if (!ok)
        return 1;
    std::puts("Configuration defaults tests passed.");
    return 0;
}

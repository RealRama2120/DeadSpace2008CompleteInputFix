#pragma once

#include "controller_transform.hpp"
#include "mouse_transform.hpp"

#include <windows.h>

struct DevelopmentConfig {
    bool diagnosticsEnabled = false;
    DWORD summaryIntervalMs = 10000;
    DWORD idleCalibrationSeconds = 5;
    ControllerTransformConfig controller;
    MouseConfig mouse;
};

DevelopmentConfig LoadConfiguration(HMODULE module);

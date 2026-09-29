#include <windows.h>
#include <xinput.h>

#include <cstring>

extern "C" {

DWORD WINAPI XInputGetState(const DWORD index, XINPUT_STATE* state) {
    if (index != 0 || !state)
        return ERROR_DEVICE_NOT_CONNECTED;
    std::memset(state, 0, sizeof(*state));
    state->dwPacketNumber = 0x12345678;
    state->Gamepad.wButtons = XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_RIGHT_SHOULDER;
    state->Gamepad.bLeftTrigger = 17;
    state->Gamepad.bRightTrigger = 231;
    state->Gamepad.sThumbLX = -12000;
    state->Gamepad.sThumbLY = 13000;
    state->Gamepad.sThumbRX = 4000;
    state->Gamepad.sThumbRY = -5000;
    return ERROR_SUCCESS;
}

DWORD WINAPI XInputSetState(const DWORD index, XINPUT_VIBRATION*) {
    return index == 0 ? ERROR_SUCCESS : ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputGetCapabilities(const DWORD index, DWORD, XINPUT_CAPABILITIES* capabilities) {
    if (index != 0 || !capabilities)
        return ERROR_DEVICE_NOT_CONNECTED;
    std::memset(capabilities, 0, sizeof(*capabilities));
    capabilities->Type = XINPUT_DEVTYPE_GAMEPAD;
    capabilities->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
    return ERROR_SUCCESS;
}

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE, DWORD, void*) {
    return TRUE;
}


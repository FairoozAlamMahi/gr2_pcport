#pragma once

#include "common/types.h"
#include <windows.h>
#include <cstddef>

namespace Pad {

#pragma pack(push, 1)
struct SceFVector3 {
    float x, y, z;
};

struct SceFQuaternion {
    float x, y, z, w;
};

struct ScePadTouch {
    uint16_t x;
    uint16_t y;
    uint8_t  id;
    uint8_t  reserve[3];
};

struct ScePadTouchData {
    uint8_t     touchNum;
    uint8_t     reserve[7];
    ScePadTouch touch[2];
};

struct ScePadData {
    uint32_t        buttons;          // 0x00
    uint8_t         leftStickX;       // 0x04
    uint8_t         leftStickY;       // 0x05
    uint8_t         rightStickX;      // 0x06
    uint8_t         rightStickY;      // 0x07
    uint8_t         analogButtons[2]; // 0x08 (0: L2, 1: R2)
    uint8_t         padding[2];       // 0x0A
    SceFQuaternion  orientation;      // 0x0C (16 bytes)
    SceFVector3     acceleration;     // 0x1C (12 bytes)
    SceFVector3     angularVelocity;  // 0x28 (12 bytes)
    ScePadTouchData touchData;        // 0x34 (24 bytes)
    uint8_t         connected;        // 0x4C (1 byte)
    uint8_t         padding2[3];      // 0x4D (3 bytes)
    uint64_t        timestamp;        // 0x50 (8 bytes)
    uint8_t         extensionData[16];// 0x58 (16 bytes)
    uint8_t         connectedCount;   // 0x68 (1 byte)
    uint8_t         reserve[2];       // 0x69 (2 bytes)
    uint8_t         deviceUniqueData[12]; // 0x6B (12 bytes)
};

static_assert(offsetof(ScePadData, buttons) == 0x00, "buttons offset mismatch");
static_assert(offsetof(ScePadData, leftStickX) == 0x04, "leftStickX offset mismatch");
static_assert(offsetof(ScePadData, rightStickX) == 0x06, "rightStickX offset mismatch");
static_assert(offsetof(ScePadData, analogButtons) == 0x08, "analogButtons offset mismatch");
static_assert(offsetof(ScePadData, orientation) == 0x0C, "orientation offset mismatch");
static_assert(offsetof(ScePadData, acceleration) == 0x1C, "acceleration offset mismatch");
static_assert(offsetof(ScePadData, angularVelocity) == 0x28, "angularVelocity offset mismatch");
static_assert(offsetof(ScePadData, touchData) == 0x34, "touchData offset mismatch");
static_assert(offsetof(ScePadData, connected) == 0x4C, "connected offset mismatch");
static_assert(offsetof(ScePadData, timestamp) == 0x50, "timestamp offset mismatch");

struct ScePadControllerInformation {
    float    touchPixelDensity;
    uint16_t touchResolutionX;
    uint16_t touchResolutionY;
    uint8_t  stickInfo[4];
    uint8_t  connected;
    uint8_t  connectionType;
    uint8_t  connectedCount;
    uint8_t  deviceClass;
    uint8_t  reserve[8];
};

struct ScePadVibrationParam {
    uint8_t largeMotor;
    uint8_t smallMotor;
};
#pragma pack(pop)

void Initialize();
void UpdateInput();

// Exported libScePad functions
extern "C" {
PS4_SYSV_ABI s32 scePadInit();
PS4_SYSV_ABI s32 scePadOpen(s32 user_id, s32 type, s32 index, const void* param);
PS4_SYSV_ABI s32 scePadClose(s32 handle);
PS4_SYSV_ABI s32 scePadReadState(s32 handle, ScePadData* data);
PS4_SYSV_ABI s32 scePadRead(s32 handle, ScePadData* data, s32 num);
PS4_SYSV_ABI s32 scePadGetControllerInformation(s32 handle, ScePadControllerInformation* info);
PS4_SYSV_ABI s32 scePadResetOrientation(s32 handle);
PS4_SYSV_ABI s32 scePadSetTiltCorrectionState(s32 handle, s32 state);
PS4_SYSV_ABI s32 scePadSetLightBar(s32 handle, const void* color);
PS4_SYSV_ABI s32 scePadSetVibration(s32 handle, const void* param);
}

} // namespace Pad

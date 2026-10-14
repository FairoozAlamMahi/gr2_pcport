#pragma once

#include "common/types.h"
#include <windows.h>

namespace Pad {

#pragma pack(push, 1)
struct SceFVector3 {
    float x, y, z;
};

struct SceFQuaternion {
    float x, y, z, w;
};

struct ScePadTouch {
    u16 x;
    u16 y;
    u8 id;
    u8 reserve[3];
};

struct ScePadTouchData {
    u8 touchNum;
    u8 reserve[7];
    ScePadTouch touch[2];
};

struct ScePadData {
    u32 buttons;
    u8 leftStickX;
    u8 leftStickY;
    u8 rightStickX;
    u8 rightStickY;
    u8 padding[12];
    SceFVector3 angularVelocity;
    SceFVector3 acceleration;
    SceFQuaternion orientation;
    ScePadTouchData touchData;
    u8 connected;
    u64 timestamp;
    u8 extensionData[16];
    u8 connectedCount;
    u8 reserve[2];
};

struct ScePadControllerInformation {
    float touchPixelDensity;
    uint16_t touchResolutionX;
    uint16_t touchResolutionY;
    uint8_t connected;
    uint8_t connectionType;
    uint8_t connectedCount;
    uint8_t deviceClass;
    uint8_t reserve[8];
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

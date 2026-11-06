/* The Dolphin SDK's controller functions the game calls, on sonarlib's PAD kit (sonar/pad/pad.h): reads and the
   motor on input's GameCube profile. A virtual controller needs no reset or calibration, and the kit reads the
   SDK's analog mode 3 only. Padclamp.c is the SDK's, compiled as the decompilation has it. */
#include "dolphin/pad.h"

#include <sonar/pad/pad.h>

#include <stddef.h>

_Static_assert(sizeof(PADStatus) == sizeof(sl_pad_status), "PADStatus is the kit's sl_pad_status");
_Static_assert(offsetof(PADStatus, err) == offsetof(sl_pad_status, err), "PADStatus.err is where the kit has it");

BOOL PADInit(void) {
    sl_pad_init(NULL);
    return TRUE;
}

u32 PADRead(PADStatus* status) {
    return sl_pad_read((sl_pad_status*)status);
}

BOOL PADReset(u32 mask) {
    (void)mask;
    return TRUE;
}

BOOL PADRecalibrate(u32 mask) {
    (void)mask;
    return TRUE;
}

void PADSetAnalogMode(u32 mode) {
    (void)mode;
}

void PADSetSpec(u32 spec) {
    sl_pad_set_spec(spec);
}

void PADControlMotor(s32 chan, u32 command) {
    sl_pad_control_motor(chan, command);
}

void PADControlAllMotors(const u32* commandArray) {
    const uint32_t commands[SL_PAD_MAX_CHANNELS] = { commandArray[0], commandArray[1], commandArray[2],
                                                     commandArray[3] };
    sl_pad_control_all_motors(commands);
}

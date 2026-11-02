/* The Dolphin SDK's Game Boy Advance functions the game calls. No Game Boy Advance is ever connected to the port
   (sonarlib docs/roadmap.md, M6), so every port answers as an empty one did and the game takes its own path for a
   missing one. */
#include "GBA/gba.h"

void GBAInit(void) {}

s32 GBAGetStatus(s32 chan, u8* status) {
    (void)chan, (void)status;
    return GBA_NOT_READY;
}

s32 GBAReset(s32 chan, u8* status) {
    (void)chan, (void)status;
    return GBA_NOT_READY;
}

/* Nothing is ever in progress. */
s32 GBAGetProcessStatus(s32 chan, u8* percentp) {
    (void)chan, (void)percentp;
    return GBA_READY;
}

s32 GBARead(s32 chan, u8* dst, u8* status) {
    (void)chan, (void)dst, (void)status;
    return GBA_NOT_READY;
}

s32 GBAWrite(s32 chan, u8* src, u8* status) {
    (void)chan, (void)src, (void)status;
    return GBA_NOT_READY;
}

s32 GBAJoyBootAsync(s32 chan, s32 palette_color, s32 palette_speed, u8* programp, s32 length, u8* status,
                    GBACallback callback) {
    (void)chan, (void)palette_color, (void)palette_speed, (void)programp, (void)length, (void)status, (void)callback;
    return GBA_NOT_READY;
}

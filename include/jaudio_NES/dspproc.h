#ifndef DSPPROC_H
#define DSPPROC_H

#include "types.h"

extern s32 DSPSendCommands(u32* commands, u32 count);
extern u32 DSPReleaseHalt();
extern void DSPWaitFinish();
extern void DsetupTable(u32 arg0, uaddr arg1, uaddr arg2, uaddr arg3, uaddr arg4);
extern void DsyncFrame(u32 subframes, uaddr dspbuf_start, uaddr dspbuf_end);
extern void DwaitFrame();
extern void DiplSec(u32 arg0);
extern void DagbSec(u32 arg0);

#endif

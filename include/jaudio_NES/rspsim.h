#ifndef RSPSIM_H
#define RSPSIM_H

#include "types.h"

#define RSPSIM_MODE_INIT 0
#define RSPSIM_MODE_MOVE 1

#ifdef SONAR_PORT
#include "PR/abi.h"

/* sonarlib port: the commands are walked as Acmd, whose w1 holds a whole host pointer */
extern s32 RspStart(Acmd* pTaskCmds, s32 allTasks);
extern void RspStart2(Acmd* pTaskCmds, s32 allTasks, s32 mode);
#else
extern s32 RspStart(u32* pTaskCmds, s32 allTasks);
extern void RspStart2(u32* pTaskCmds, s32 allTasks, s32 mode);
#endif

#endif

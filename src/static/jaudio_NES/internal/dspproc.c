#include "jaudio_NES/dspproc.h"
#include "dolphin/dsp.h"

#ifdef SONAR_PORT
#include "port/dsp_mail.h"
/* sonarlib port: a host address does not fit a mail, so the port's DSP gives each one a 32-bit handle. */
#define DSP_MAIL_ADDRESS(address) ac_dsp_mail_address((const void*)(address))
#else
#define DSP_MAIL_ADDRESS(address) (address)
#endif

static u16 DSP_MIXERLEVEL = 0x4000;

extern s32 DSPSendCommands(u32* commands, u32 count) {
    if (DSPCheckMailToDSP() != 0) {
        return -1;
    }
    if (DSPCheckMailFromDSP() != 0) {
        return -1;
    }

    int i;

    for (i = 0; i < count; i++) {
        DSPSendMailToDSP(commands[i]);

        while (DSPCheckMailToDSP() != 0);
    }

    return 0;
}

extern u32 DSPReleaseHalt() {
    while (DSPCheckMailToDSP() != 0);
    DSPSendMailToDSP(0);
    if (DSPCheckMailFromDSP() != 0) {
        DSPReadMailFromDSP();
    }

    return 0x88881357;
}

extern void DSPWaitFinish() {
    do {
        while (DSPCheckMailFromDSP() == 0);
    } while ((DSPReadMailFromDSP() + 0x77780000) == 0x1357);
}

extern void DsetupTable(u32 arg0, uaddr arg1, uaddr arg2, uaddr arg3, uaddr arg4) {
    u32 commands[5];

    commands[0] = (arg0 & 0xFFFF) | 0x81000000;
    commands[1] = DSP_MAIL_ADDRESS(arg1);
    commands[2] = DSP_MAIL_ADDRESS(arg2);
    commands[3] = DSP_MAIL_ADDRESS(arg3);
    commands[4] = DSP_MAIL_ADDRESS(arg4);

    DSPSendCommands(commands, ARRAY_COUNT(commands));
    DSPWaitFinish();
}

extern void DsyncFrame(u32 subframes, uaddr dspbuf_start, uaddr dspbuf_end){
    u32 commands[3];

    commands[0] = (subframes << 16 & 0xFF0000) | 0x82000000 | DSP_MIXERLEVEL;
    commands[1] = DSP_MAIL_ADDRESS(dspbuf_start);
    commands[2] = DSP_MAIL_ADDRESS(dspbuf_end);

    DSPSendCommands(commands, ARRAY_COUNT(commands));
    DSPWaitFinish();    
}

extern void DwaitFrame(){
    u32 commands[1];

    commands[0] = 0x80000000;
    DSPSendCommands(commands, ARRAY_COUNT(commands));
    DSPWaitFinish();    
}

extern void DiplSec(u32 arg0){
    u32 commands[2];

    commands[0] = 0x8B000008;
    commands[1] = arg0;
    DSPSendCommands(commands, ARRAY_COUNT(commands));
    DSPWaitFinish();    
}

extern void DagbSec(u32 arg0){
    u32 commands[2];

    commands[0] = 0x8C000008;
    commands[1] = arg0;
    DSPSendCommands(commands, ARRAY_COUNT(commands));
    DSPWaitFinish();    
}




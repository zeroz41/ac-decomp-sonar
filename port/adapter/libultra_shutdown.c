/* libultra's shutdown, osShutdownStart, the game's shutdown.c substituted (sonarlib docs/steps/m6-10e.md): the game's
   text, but for its GX. It cancels the GX thread when another thread calls it and makes the caller the GX thread, as
   the game's file did through GX, by the glue's (port/gx_sink.h). The flush, the frame's abort and the draw done only
   made the GPU wait, and are the reference's (port/reference.h, ac_reference_sync); GX's draw done called JUTVideo's
   callback, which the seam calls after them (port/jsys_display.h). It includes no GX header. */
#include "libultra/libultra.h"
#include "dolphin/os.h"
#include "dolphin/dvd.h"
#include "dolphin/pad.h"
#include "jaudio_NES/game64.h"
#include "libultra/initialize.h"
#include "libjsys/jsyswrapper.h"
#include "libultra/os_timer.h"
#include "boot.h"
#include "m_nmibuf.h"

#include <port/gx_sink.h>
#include <port/jsys_display.h>
#include <port/reference.h>

void osShutdownStart(int val) {
    s32 dstat;
    OSThread* gthread;
    OSThread* cthread;
    u32 enable;

    OSReport("***osShutdownStart***\n");
    dstat = DVDGetDriveStatus();

    if (dstat == -1) {
        while (TRUE) {}
    }
    osIsEnableShutdown();
    PADRecalibrate(0xF0000000);
    Na_Reset();

    gthread = ac_gx_thread();
    enable = OSDisableInterrupts();
    cthread = OSGetCurrentThread();

    if (gthread != cthread) {
        OSCancelThread(gthread);
        ac_gx_set_thread();
    }
    ac_reference_sync(AC_REFERENCE_FLUSH);
    ac_reference_sync(AC_REFERENCE_ABORT_FRAME);
    ac_reference_sync(AC_REFERENCE_DRAW_DONE);
    ac_jsys_video_draw_done();

    OSRestoreInterrupts(enable);
    JW_JUTXfb_clearIndex();
    VIWaitForRetrace();
    VISetBlack(TRUE);
    VIFlush();
    VIWaitForRetrace();
    osStopTimerAll();
    LCDisable();
    OSReport("リセットしてもこれだけは忘れない。 osAppNMIBuffer[15] = %08x\n", osAppNMIBuffer[15]);

    if ((val == 2) && (DVDCheckDisk() == 0)) {
        OSReport("蓋空き。ホットリセットします。\n");
        val = 0;
    }

    if (APPNMI_HOTRESET_GET()) {
        OSResetSystem(OS_RESET_RESTART, 0, FALSE);
        return;
    }
    switch (val) {
        case OS_RESET_RESTART:
            OSResetSystem(OS_RESET_HOTRESET, osAppNMIBuffer[15], FALSE);
            break;
        case OS_RESET_HOTRESET:
            HotResetIplMenu();
            break;
        case OS_RESET_SHUTDOWN:
            bcopy(osAppNMIBuffer, NMISaveArea, sizeof(osAppNMIBuffer));
            OSResetSystem(OS_RESET_RESTART, 0x80000000, FALSE);
            break;
    }
}

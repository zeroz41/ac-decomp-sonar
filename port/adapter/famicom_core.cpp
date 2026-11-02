/* The NES emulator's entry points, whose core is assembly not yet converted (sonarlib docs/roadmap.md, M6). The
   game's famicom_init declines to start on the port, so nothing calls these. */
#include "Famicom/famicomPriv.h"
#include "Famicom/ks_nes_core.h"
#include "Famicom/ks_nes_draw.h"
#include "dolphin/os.h"

static const char not_converted[] = "the NES emulator is not converted";

void ksNesPushResetButton(ksNesStateObj* sp) {
    (void)sp;
    OSHalt(not_converted);
}

int ksNesReset(ksNesCommonWorkObj* wp, ksNesStateObj* sp, u32 flags, u8* chrramp, u8* bbramp) {
    (void)wp, (void)sp, (void)flags, (void)chrramp, (void)bbramp;
    OSHalt(not_converted);
    return -1;
}

void ksNesEmuFrame(ksNesCommonWorkObj* wp, ksNesStateObj* sp, u32 flags) {
    (void)wp, (void)sp, (void)flags;
    OSHalt(not_converted);
}

void ksNesDrawInit(ksNesCommonWorkObj* wp) {
    (void)wp;
    OSHalt(not_converted);
}

void ksNesDraw(ksNesCommonWorkObj* wp, ksNesStateObj* sp) {
    (void)wp, (void)sp;
    OSHalt(not_converted);
}

void ksNesDrawEnd() { OSHalt(not_converted); }

/* Checks the port's pad display (src/game/m_debug_hayakawa.c, pad_disp): debug_hayakawa_draw sets a pressed button's
   primitive color as the console's word, r in its high byte, as gDPSetColor's emulation reads it. */
#include "dolphin/os.h"
#include "dolphin/vi.h"
#include "gfxalloc.h"
#include "graph.h"
#include "jsyswrap.h"
#include "libc64/__osMalloc.h"
#include "libforest/emu64.h"
#include "libforest/osreport.h"
#include "libu64/gfxprint.h"
#include "m_debug.h"
#include "m_debug_hayakawa.h"
#include "m_rcp.h"

#include <stdio.h>
#include <string.h>

/* m_debug_hayakawa.c's other references, which the pad display does not reach but its other functions name.
   main.h's are defined without it, since it declares main as the console's game does. */
static Debug_mode debug_registers;
Debug_mode* debug_mode = &debug_registers;
int ScreenWidth = 320;
int ScreenHeight = 240;
int __osMalloc_FreeBlockTest_Enable;
int emu64_get_aflags(int idx) {
    (void)idx;
    return 0;
}
void emu64_set_aflags(int idx, int value) {
    (void)idx;
    (void)value;
}
Gfx* gfx_gDPFillRectangleF(Gfx* gfx, u32 ulx, u32 uly, u32 lrx, u32 lry) {
    (void)ulx, (void)uly, (void)lrx, (void)lry;
    return gfx;
}
void* JC_JFWSystem_getRootHeap() { return NULL; }
int JC_JKRHeap_dump(void* heap) {
    (void)heap;
    return 0;
}
s32 JC_JKRHeap_getTotalFreeSize(void* heap) {
    (void)heap;
    return 0;
}
void* JC_JUTDbPrint_getManager() { return NULL; }
void JC_JUTDbPrint_setVisible(void* dbprint, BOOL visible) {
    (void)dbprint;
    (void)visible;
}
void* JC_JUTProcBar_getManager() { return NULL; }
void JC_JUTProcBar_setVisible(void* procbar, BOOL visible) {
    (void)procbar;
    (void)visible;
}
void JC_JUTProcBar_setVisibleHeapBar(void* procbar, BOOL visible) {
    (void)procbar;
    (void)visible;
}
void JW_SetLowResoMode(int enabled) { (void)enabled; }
void JW_SetProgressiveMode(int enabled) { (void)enabled; }
s32 OSCheckActiveThreads(void) { return 0; }
u32 OSGetConsoleType() { return 0; }
void OSReport(const char* fmt, ...) { (void)fmt; }
void OSReportDisable() {}
void OSReportEnable() {}
void VIConfigurePan(u16 x_origin, u16 y_origin, u16 width, u16 height) {
    (void)x_origin, (void)y_origin, (void)width, (void)height;
}

/* The display list helpers the pad display calls: gfxalloc.c's opening and closing, a rectangle that writes nothing,
   and a text printer that prints nothing. */
Gfx* gfxopen(Gfx* gfxp) { return gfxp + 1; }
Gfx* gfxclose(Gfx* gfxp, Gfx* gfxp_new) {
    (void)gfxp;
    return gfxp_new;
}
Gfx* gfx_gDPFillRectangle1(Gfx* gfx, u32 ulx, u32 uly, u32 lrx, u32 lry) {
    (void)ulx, (void)uly, (void)lrx, (void)lry;
    return gfx;
}
static Gfx* printer_gfx;
void gfxprint_init(gfxprint_t* this) { (void)this; }
void gfxprint_open(gfxprint_t* this, Gfx* glistp) {
    (void)this;
    printer_gfx = glistp;
}
Gfx* gfxprint_close(gfxprint_t* this) {
    (void)this;
    return printer_gfx;
}
void gfxprint_cleanup(gfxprint_t* this) { (void)this; }

static int failures;

#define CHECK(cond)                                                                                                   \
    if (!(cond)) {                                                                                                    \
        printf("%s:%d: %s\n", __FILE__, __LINE__, #cond);                                                             \
        failures++;                                                                                                   \
    }

int main(void) {
    static GRAPH graph;
    static Gfx poly_opa[64];
    static Gfx overlay[8];
    u32 colors[8];
    int count = 0;
    int i;

    memset(poly_opa, 0, sizeof poly_opa);
    graph.polygon_opaque_thaga.thaGfx.head_p = poly_opa;
    graph.overlay_thaga.thaGfx.head_p = overlay;

    /* The pad display on, and pad 0 holding the C stick right (bit 0, yellow), B (bit 14, red) and A (bit 15,
       green). */
    SETREG(HREG, 47, 1);
    SETREG(HREG, 40, (s16)(1 << 0 | 1 << 14 | 1 << 15));
    debug_hayakawa_draw(&graph);

    for (i = 0; i < 64 && count < 8; i++) {
        if ((poly_opa[i].words.w0 >> 24) == G_SETPRIMCOLOR) {
            colors[count++] = (u32)poly_opa[i].words.w1;
        }
    }

    /* The black backing, then each pressed button's color in button order. */
    CHECK(count == 4);
    CHECK(count < 1 || colors[0] == 0x00000000);
    CHECK(count < 2 || colors[1] == 0xFFFF00FF);
    CHECK(count < 3 || colors[2] == 0xFF0000FF);
    CHECK(count < 4 || colors[3] == 0x00FF00FF);

    if (failures)
        printf("%d failures\n", failures);
    return failures != 0;
}

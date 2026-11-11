/* The reference's GX of the frame's seams (sonarlib docs/steps/m6-10e.md), the hooks that name GX's types: what the
   game's JFWDisplay.cpp gave GX for the display copy, the pixel format and clearEfb's quad. In ac_port_reference
   (port/adapter/jsys_display_reference.c) each makes the game's GX calls under --diff or --reference, as the console's
   emu64 draws under the state they leave, and does nothing otherwise; in ac_port (port/adapter/reference_none.c) each
   does nothing. The GX-free hooks, the waits, the flushes and the copy, are in port/reference.h. Game threads, one at a
   time. */
#ifndef AC_PORT_DISPLAY_REFERENCE_H
#define AC_PORT_DISPLAY_REFERENCE_H

#include "dolphin/gx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* prepareCopyDisp's set-up of the display copy, from JFWDisplay's members and JUTVideo's render mode. */
typedef struct ac_reference_copy {
    GXColor clear;               /* mClearColor */
    u32 z;                       /* mZClear */
    const GXRenderModeObj* mode; /* JUTVideo's */
    u16 width;                   /* the mode's frame buffer width */
    u16 efb_height;              /* its EFB height */
    u16 xfb_height;              /* its XFB height */
    u32 clamp;                   /* mClamp */
    u32 gamma;                   /* mGamma */
    BOOL enable_alpha;           /* mEnableAlpha */
} ac_reference_copy;

/* prepareCopyDisp's GX: the copy's clear, source, destination, scale, filter, clamp and gamma, the z mode and, with
   alpha, the alpha update. */
void ac_reference_prepare_copy(const ac_reference_copy* copy);
/* preGX's: the texture and vertex caches invalidated, and the pixel format and dither by the mode and alpha. */
void ac_reference_pre_gx(const GXRenderModeObj* mode, BOOL enable_alpha);
/* clearEfb's quad over the mode's whole EFB, to color and z 0xFFFFFF, with alpha 0 when alpha is enabled. */
void ac_reference_clear_efb(const GXRenderModeObj* mode, GXColor color, BOOL enable_alpha);

#ifdef __cplusplus
}
#endif

#endif

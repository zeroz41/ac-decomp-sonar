/* The reference's GX front ends, in ac_port_reference only (port/reference.h; sonarlib docs/steps/m6-10a.md and
   m6-10e.md). Under --diff or --reference the main front end takes the GX written outside display lists, which the
   console's emu64 and the reference's GX of the frame's seams (port/adapter/jsys_display_reference.c) write, and the
   glue's records record it (ac_reference_record). It adds no clear: the frame's clear is the jsys kit's, in both
   programs (port/jsys_display.h). Under --diff the console's emu64 draws each task through the reference's own front
   end, between ac_gx_reference_begin and ac_gx_reference_end, into a frame of its own, which that front end clears,
   while the main front end keeps the game's (M6 step 6j). */
#include "dolphin/gx.h"
#include "gx/__gx.h"

#include "emu64_reference.h"

#include <port/gx_sink.h>
#include <port/reference.h>

#include <sonar/gx_frontend.h>
#include <sonar/render.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static sl_gxf* main_gxf;  /* made at the first hand-over under either flag */
static BOOL main_failed;  /* main_gxf could not be made */
static sl_gxf* ref_gxf;   /* made at the first ac_gx_reference_begin */
static BOOL to_reference; /* hand-overs go to ref_gxf */

sl_gxf* ac_gx_create_front_end(uint32_t flags) {
    sl_gxf_desc desc;
    memset(&desc, 0, sizeof desc);
    desc.memory = ac_gx_memory;
    desc.max_vertices = 0;
    desc.flags = flags;
    return sl_gxf_create(&desc);
}

sl_gxf* ac_gx_front_end(void) {
    return main_gxf;
}

static void commands(sl_gxf* gxf, const u8* bytes, size_t size, u64 offset) {
    size_t error_offset = 0;
    sl_gx_dl_result result;
    if (size == 0)
        return;
    result = sl_gxf_commands(gxf, bytes, size, &error_offset);
    if (result != SL_GX_DL_OK)
        ac_gx_error(offset + error_offset, "sl_gxf_commands failed", (int)result);
}

BOOL ac_reference_gx(const u8* bytes, size_t size, u64 offset) {
    if (to_reference) {
        if (ref_gxf == NULL) {
            if (size != 0)
                ac_gx_error(offset, "bytes dropped: no reference GX front end, out of memory", (int)size);
            return TRUE;
        }
        commands(ref_gxf, bytes, size, offset);
        return TRUE;
    }
    if (!ac_diff_enabled() && !ac_reference_enabled())
        return FALSE;
    if (main_gxf == NULL && !main_failed) {
        main_gxf = ac_gx_create_front_end(SL_GXF_NO_CLEAR);
        main_failed = main_gxf == NULL;
    }
    if (main_gxf == NULL) {
        if (size != 0)
            ac_gx_error(offset, "bytes dropped: no GX front end, out of memory", (int)size);
        return TRUE;
    }
    commands(main_gxf, bytes, size, offset);
    return TRUE;
}

void ac_reference_record(struct sl_frame* frame, BOOL first) {
    sl_result result;
    if (frame == NULL || main_gxf == NULL)
        return;
    result = first ? sl_gxf_record(main_gxf, frame) : sl_gxf_record_more(main_gxf, frame);
    if (result != SL_OK)
        ac_gx_record_refused(result);
}

/* The SDK's GX state when the reference began, and the main front end's state on its way to the reference's.
   Static: the register and vertex states are large. */
static struct __GXData_struct saved_gx;
static sl_gx_register_state state_registers;
static sl_gx_vertex_state state_vertex;
static uint32_t state_xf[SL_GXF_XF_MEMORY_WORDS];

void ac_gx_reference_begin(void) {
    ac_gx_flush();
    if (ref_gxf == NULL)
        ref_gxf = ac_gx_create_front_end(0);
    if (main_gxf != NULL && ref_gxf != NULL) {
        sl_gxf_get_state(main_gxf, &state_registers, &state_vertex, state_xf);
        sl_gxf_set_state(ref_gxf, &state_registers, &state_vertex, state_xf);
    }
    saved_gx = *gx;
    to_reference = TRUE;
}

void ac_gx_reference_end(struct sl_frame* reference_frame) {
    ac_gx_flush();
    if (reference_frame != NULL && ref_gxf != NULL) {
        const sl_result result = sl_gxf_record(ref_gxf, reference_frame);
        if (result != SL_OK)
            ac_gx_record_refused(result);
    }
    *gx = saved_gx;
    to_reference = FALSE;
}

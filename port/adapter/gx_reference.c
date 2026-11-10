/* The reference's own GX front end, in ac_port_reference only (port/reference.h; sonarlib docs/steps/m6-10a.md): under
   --diff the console's emu64 draws each task through it, between ac_gx_reference_begin and ac_gx_reference_end, into
   a frame of its own, while the main front end (port/adapter/dolphin_gx.c) keeps the game's (M6 step 6j). */
#include "dolphin/gx.h"
#include "gx/__gx.h"

#include "emu64_reference.h"

#include <port/gx_sink.h>
#include <port/reference.h>

#include <sonar/gx_frontend.h>
#include <sonar/render.h>

#include <stddef.h>
#include <stdint.h>

static sl_gxf* ref_gxf;   /* made at the first ac_gx_reference_begin */
static BOOL to_reference; /* hand-overs go to ref_gxf */

BOOL ac_reference_gx(const u8* bytes, size_t size, u64 offset) {
    size_t error_offset = 0;
    sl_gx_dl_result result;
    if (!to_reference)
        return FALSE;
    if (ref_gxf == NULL) {
        if (size != 0)
            ac_gx_error(offset, "bytes dropped: no reference GX front end, out of memory", (int)size);
        return TRUE;
    }
    if (size == 0)
        return TRUE;
    result = sl_gxf_commands(ref_gxf, bytes, size, &error_offset);
    if (result != SL_GX_DL_OK)
        ac_gx_error(offset + error_offset, "sl_gxf_commands failed", (int)result);
    return TRUE;
}

/* The SDK's GX state when the reference began, and the main front end's state on its way to the reference's.
   Static: the register and vertex states are large. */
static struct __GXData_struct saved_gx;
static sl_gx_register_state state_registers;
static sl_gx_vertex_state state_vertex;
static uint32_t state_xf[SL_GXF_XF_MEMORY_WORDS];

void ac_gx_reference_begin(void) {
    sl_gxf* main_gxf;
    ac_gx_flush();
    if (ref_gxf == NULL)
        ref_gxf = ac_gx_create_front_end();
    main_gxf = ac_gx_front_end();
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

/* The reference's half of the emu64 glue, in ac_port_reference only (port/reference.h; sonarlib docs/steps/m6-10a.md):
   the console's emu64, built into the program under emu64_ref_ names (port/CMakeLists.txt), drawn through sonarlib's
   GX front end. Under --diff each task is drawn by the kit and by the reference, and the kit's and the reference's
   frames are compared (M6 step 6j). Under --reference the kit is not made, and the reference draws every task into
   the game's own frame instead (M6 step 7b). OSInit takes the reference's buffers under either. */
#include "game.h"
#include "graph.h"
#include "libforest/emu64.h"
#include "sys_ucode.h"

#include "ac_port.h"
#include "emu64_reference.h"

#include <port/emu64_glue.h>
#include <port/gx_sink.h>
#include <port/reference.h>

#include <sonar/dolphin_os/os.h>
#include <sonar/emu64/emu64.h>
#include <sonar/render.h>
#include <sonar/render_capture.h>
#include <sonar/runtime.h>

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

const char ac_program_name[] = "ac_port_reference";
const char ac_reference_options[] = " [--diff] [--reference]";

/* ---- The reference (--diff, --reference): the calls it runs inside the switch, kept until the next task */

static BOOL diff;
static BOOL reference; /* --reference: no kit; the reference draws each task into the game's frame */
static BOOL ref_init;        /* emu64_init was called since the last task */
static BOOL ref_ucode_info;  /* emu64_set_ucode_info was, with these arguments */
static int ref_ucode_count;
static ucode_info* ref_ucode_infos;
static BOOL ref_first_ucode; /* emu64_set_first_ucode was, with this argument */
static void* ref_first_ucode_arg;

BOOL ac_diff_enable(void) {
    diff = TRUE;
    return TRUE;
}

BOOL ac_diff_enabled(void) {
    return diff;
}

BOOL ac_reference_enable(void) {
    reference = TRUE;
    return TRUE;
}

BOOL ac_reference_enabled(void) {
    return reference;
}

/* The reference, emu64.c, hands GX the textures and palettes it converts into its buffers, which must lie in GX's
   address space, palettes below 0x02000000: under --diff and --reference they come from the bottom of the arena
   before anything else allocates, each 32-byte aligned, so the heaps start 0x14400 bytes later. */
void ac_reference_boot(void) {
    if (diff || reference) {
        u8* const lo = (u8*)(((uintptr_t)sl_dolphin_os_arena_lo() + 31u) & ~(uintptr_t)31u);
        u8* const data = lo;
        u8* const bss = data + AC_DIFF_TEXTURE_DATA_SIZE;
        u8* const palettes = bss + AC_DIFF_TEXTURE_BSS_SIZE;
        sl_dolphin_os_set_arena_lo(palettes + AC_DIFF_PALETTES_SIZE);
        emu64_ref_set_buffers(data, bss, palettes, AC_DIFF_PALETTES_SIZE);
    }
}

void ac_reference_init(void) {
    if (diff || reference)
        ref_init = TRUE;
}

void ac_reference_set_ucode_info(int count, struct ucode_info_s* info) {
    if (!diff && !reference)
        return;
    ref_ucode_info = TRUE;
    ref_ucode_count = count;
    ref_ucode_infos = info;
}

void ac_reference_set_first_ucode(void* ucode) {
    if (!diff && !reference)
        return;
    ref_first_ucode = TRUE;
    ref_first_ucode_arg = ucode;
}

void ac_reference_refresh(void) {
    if (diff || reference)
        emu64_ref_refresh();
}

void ac_reference_static_range(void* begin, void* end) {
    if (diff || reference)
        emu64_ref_texture_cache_data_entry_set(begin, end);
}

void ac_reference_set_aflags(int idx, int value) {
    if (diff || reference)
        emu64_ref_set_aflags(idx, value);
}

void ac_reference_report(void) {
    if (reference)
        printf(" palette-overflows %" PRIu32, (uint32_t)emu64_ref_palette_overflows());
}

/* ---- The differential test's comparison and report */

#define AC_DIFF_LINES 16 /* the most difference lines a run prints */

static u64 diff_tasks;
static u64 diff_equal;
static u64 diff_different;
static u64 diff_refused;
static u64 diff_triangles;  /* over both captures of the equal tasks */
static u64 diff_skipped[2]; /* the kit's and the reference's, over the tasks compared */
static u32 diff_lines;

/* GRAPH's lists' field names less _thaga, in ac_emu64_list's order. */
static const char* const list_names[AC_EMU64_LISTS] = {
    "work",  "line_opaque", "line_translucent", "overlay",   "polygon_opaque", "polygon_translucent",
    "font",  "shadow",      "light",            "bg_opaque", "bg_translucent",
};

/* sl_capture_field's names, in lower case, by value. */
static const char* const field_names[] = {
    "same",    "count",   "primitive", "view",   "material", "params",  "draw_color",
    "texture", "sampler", "position",  "normal", "color",    "texcoord",
};

/* A kit draw's label, the host address of the command that drew it, named: its list and index within one of the
   game's lists, static and its GX address in main memory or a mapped image range, else unknown. */
static void label_name(uint64_t label, char* name, size_t size) {
    const u8* const p = (const u8*)(uintptr_t)label;
    u32 address;
    int i;
    if (game_class_p != NULL && game_class_p->graph != NULL) {
        for (i = 0; i < AC_EMU64_LISTS; i++) {
            const THA_GA* const list = ac_emu64_list(game_class_p->graph, i);
            const u8* const buf = (const u8*)list->thaGfx.buf_p;
            const u8* const head = (const u8*)list->thaGfx.head_p;
            if (buf != NULL && p >= buf && p < head) {
                snprintf(name, size, "%s %" PRIu64, list_names[i], (uint64_t)((size_t)(p - buf) / sizeof(Gfx)));
                return;
            }
        }
    }
    if (ac_gx_find_address(p, &address)) {
        snprintf(name, size, "static 0x%08" PRIx32, (uint32_t)address);
        return;
    }
    snprintf(name, size, "unknown");
}

static void diff_report(const sl_capture* kit, const sl_capture_difference* d) {
    char name[64];
    const char* const field =
        (u32)d->field < sizeof field_names / sizeof field_names[0] ? field_names[d->field] : "unknown";
    if (diff_lines >= AC_DIFF_LINES)
        return;
    diff_lines++;
    if (d->draw[0] < kit->draw_count)
        label_name(kit->draws[d->draw[0]].label, name, sizeof name);
    else
        snprintf(name, sizeof name, "none");
    printf("ac_port: diff tick %" PRIu64 " triangle %" PRIu32 " vertex %" PRIu32 " field %s index %" PRIu32
           " kit %" PRIu32 " %s reference %" PRIu32 "\n",
           sl_tick_count(), d->triangle, d->vertex, field, d->index, d->draw[0], name, d->draw[1]);
    fflush(stdout);
}

/* A frame of its own, submitted, and its capture; NULL when the frame was refused or gave no capture. */
static sl_capture* frame_capture(sl_frame* frame) {
    if (frame == NULL || sl_frame_submit(frame) != SL_OK)
        return NULL;
    return sl_capture_take();
}

/* The reference's calls since the last task, in the console's order, then its task and cleanup. */
static void reference_task(Gfx* gfx) {
    if (ref_init)
        emu64_ref_init();
    if (ref_ucode_info)
        emu64_ref_set_ucode_info(ref_ucode_count, ref_ucode_infos);
    if (ref_first_ucode)
        emu64_ref_set_first_ucode(ref_first_ucode_arg);
    ref_init = ref_ucode_info = ref_first_ucode = FALSE;
    emu64_ref_taskstart(gfx);
    emu64_ref_cleanup();
}

/* The task drawn by the kit and by the reference, each into a frame of its own between the two halves of the
   game's, and the two captures compared. */
static void diff_task(sl_emu64* emu, Gfx* gfx) {
    ac_gx_counts before, after;
    sl_frame* frame;
    sl_frame* kit_frame;
    sl_frame* ref_frame;
    sl_capture* kit;
    sl_capture* ref;
    sl_capture_difference d;
    u32 overflows;
    BOOL refused;
    ac_gx_get_counts(&before);
    if (!ac_frame_task(&frame)) {
        ac_gx_get_counts(&after);
        if (after.outside == before.outside) { /* the game's frame was refused: neither draws, as without --diff */
            diff_tasks++;
            diff_refused++;
        }
        return;
    }
    ac_frame_split(FALSE);

    kit_frame = sl_frame_begin();
    if (kit_frame != NULL)
        sl_emu64_task(emu, kit_frame, (const sl_emu64_gfx*)gfx);
    kit = frame_capture(kit_frame);

    overflows = emu64_ref_palette_overflows();
    ref_frame = sl_frame_begin();
    ac_gx_reference_begin();
    reference_task(gfx);
    ac_gx_reference_end(ref_frame);
    ref = frame_capture(ref_frame);
    refused = kit == NULL || ref == NULL || emu64_ref_palette_overflows() != overflows;

    diff_tasks++;
    if (refused) {
        diff_refused++;
    } else if (sl_capture_compare(kit, ref, NULL, &d)) {
        diff_equal++;
        diff_triangles += (u64)kit->triangle_count + ref->triangle_count;
    } else {
        diff_different++;
        diff_report(kit, &d);
    }
    if (kit != NULL && ref != NULL) {
        diff_skipped[0] += kit->skipped;
        diff_skipped[1] += ref->skipped;
    }
    sl_capture_free(kit);
    sl_capture_free(ref);

    ac_frame_split(TRUE);
}

/* Under --diff, the task compared with the reference's. Under --reference the reference draws it through the GX
   front end into the open frame, and the GX state it leaves stays, as on the console. */
BOOL ac_reference_task(sl_emu64* kit, Gfx* gfx) {
    sl_frame* frame;
    if (diff) {
        diff_task(kit, gfx);
        return TRUE;
    }
    if (!reference)
        return FALSE;
    if (ac_frame_task(&frame))
        reference_task(gfx);
    return TRUE;
}

void ac_reference_submitted(void) {
    if (diff)
        printf("ac_port: diff tasks %" PRIu64 " equal %" PRIu64 " different %" PRIu64 " refused %" PRIu64
               " triangles %" PRIu64 " skipped %" PRIu64 " %" PRIu64 "\n",
               (uint64_t)diff_tasks, (uint64_t)diff_equal, (uint64_t)diff_different, (uint64_t)diff_refused,
               (uint64_t)diff_triangles, (uint64_t)diff_skipped[0], (uint64_t)diff_skipped[1]);
}

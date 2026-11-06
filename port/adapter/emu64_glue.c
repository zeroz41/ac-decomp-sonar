/* What the game calls of emu64 (src/static/libforest/emu64/emu64.c, which the port does not compile), on sonarlib's
   kits/emu64 (sonarlib docs/roadmap.md, M6 step 6g): each task is recorded into a frame on the kit, the game's edits
   on the kit's path come here (port/emu64_glue.h), and emu64_taskstart is where the program's proof stops the run:
   at play's first frame, after its task's submit (port/README.md). */
#include "game.h"
#include "graph.h"
#include "m_actor.h"
#include "m_play.h"
#include "libforest/emu64.h"
#include "libforest/emu64/emu64_wrapper.h"

#include "ac_port.h"

#include <port/emu64_glue.h>

#include <sonar/dolphin_os/os.h>
#include <sonar/emu64/emu64.h>
#include <sonar/render.h>
#include <sonar/runtime.h>

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* The kit reads the port's Gfx as its own command, and an N64 matrix as 0x40 bytes. */
_Static_assert(sizeof(Gfx) == sizeof(sl_emu64_gfx), "the port's Gfx is the kit's command");
_Static_assert(offsetof(Gfx, words.w0) == offsetof(sl_emu64_gfx, w0), "a command's first word");
_Static_assert(offsetof(Gfx, words.w1) == offsetof(sl_emu64_gfx, w1), "a command's second word");
_Static_assert(sizeof(Mtx) == 0x40, "an N64 matrix is 16 words of s15.16");

/* emu64.hpp's AFLAGS_MAX: emu64 keeps its debug flags only in its debug build. */
#ifdef EMU64_DEBUG
#define AC_AFLAGS_MAX 100
#else
#define AC_AFLAGS_MAX 0
#endif

static u8 aflags[AC_AFLAGS_MAX > 0 ? AC_AFLAGS_MAX : 1];

u8 FrameCansel = FALSE;

static sl_emu64* emu;
static u64 null_frames; /* tasks skipped because sl_frame_begin gave no frame */
static u64 unowned;     /* actors drawn outside main memory, without owners */

/* GRAPH's eleven display lists, in its field order (graph.h). */
static const size_t list_fields[AC_EMU64_LISTS] = {
    offsetof(GRAPH, work_thaga),      offsetof(GRAPH, line_opaque_thaga),    offsetof(GRAPH, line_translucent_thaga),
    offsetof(GRAPH, overlay_thaga),   offsetof(GRAPH, polygon_opaque_thaga), offsetof(GRAPH, polygon_translucent_thaga),
    offsetof(GRAPH, font_thaga),      offsetof(GRAPH, shadow_thaga),         offsetof(GRAPH, light_thaga),
    offsetof(GRAPH, bg_opaque_thaga), offsetof(GRAPH, bg_translucent_thaga),
};

void ac_emu64_start(sl_emu64* made) {
    emu = made;
}

void emu64_init(void) {
}

void emu64_set_ucode_info(int count, ucode_info* info) {
    (void)count;
    (void)info;
}

void emu64_set_first_ucode(void* ucode) {
    (void)ucode;
}

void emu64_cleanup(void) {
}

void emu64_refresh(void) {
    sl_emu64_refresh(emu);
}

void emu64_texture_cache_data_entry_set(void* begin, void* end) {
    sl_emu64_static_range(emu, begin, end);
}

void emu64_set_aflags(int idx, int value) {
    if (idx > 0 && idx < AC_AFLAGS_MAX)
        aflags[idx] = (u8)value;
}

int emu64_get_aflags(int idx) {
    if (idx > 0 && idx < AC_AFLAGS_MAX)
        return aflags[idx];
    return 0;
}

void ac_emu64_written(const void* begin, const void* end) {
    sl_emu64_written(emu, begin, end);
}

/* GXColor's bytes, R in bits 24 to 31. */
void ac_emu64_set_clear(GXColor color, u32 z) {
    sl_emu64_set_clear(emu, (u32)color.r << 24 | (u32)color.g << 16 | (u32)color.b << 8 | color.a, z);
}

/* The actor's offset from main memory's base, FALSE for an actor outside it. */
static BOOL actor_offset(const ACTOR* actor, uint64_t* offset) {
    const u8* const base = (const u8*)sl_dolphin_os_memory_base();
    const u8* const p = (const u8*)actor;
    if (base == NULL || p < base || p >= base + sl_dolphin_os_memory_size())
        return FALSE;
    *offset = (uint64_t)(p - base);
    return TRUE;
}

static uint64_t owner_of(uint64_t offset, int list) {
    return offset << 4 | (uint64_t)(list + 1);
}

static THA_GA* list_of(GRAPH* graph, int list) {
    return (THA_GA*)((u8*)graph + list_fields[list]);
}

/* One G_NOOP of tag SL_EMU64_TAG_OWNER, opening owner, or closing with 0, at the list's head. */
static Gfx* write_marker(THA_GA* list, uint64_t owner) {
    Gfx* const at = list->thaGfx.head_p;
    at->words.w0 = (u32)0x00 << 24 | (u32)SL_EMU64_TAG_OWNER << 16;
    at->words.w1 = (u32)owner; /* below 2^32: main memory is far below 256 MiB */
    list->thaGfx.head_p = at + 1;
    return at;
}

void ac_emu64_owner_open(GRAPH* graph, ACTOR* actor, ac_emu64_marks* marks) {
    uint64_t offset;
    int i;
    if (!actor_offset(actor, &offset)) {
        for (i = 0; i < AC_EMU64_LISTS; i++)
            marks->at[i] = NULL;
        unowned++;
        return;
    }
    /* A list graph_ct gives no buffer (the line lists) gets none. */
    for (i = 0; i < AC_EMU64_LISTS; i++) {
        THA_GA* const list = list_of(graph, i);
        marks->at[i] = list->thaGfx.buf_p != NULL ? write_marker(list, owner_of(offset, i)) : NULL;
    }
}

void ac_emu64_owner_close(GRAPH* graph, const ac_emu64_marks* marks) {
    int i;
    for (i = 0; i < AC_EMU64_LISTS; i++) {
        THA_GA* const list = list_of(graph, i);
        Gfx* const mark = (Gfx*)marks->at[i];
        if (mark == NULL)
            continue;
        if (list->thaGfx.head_p == mark + 1)
            list->thaGfx.head_p = mark; /* the actor drew nothing here: the marker is taken back */
        else
            write_marker(list, 0);
    }
}

void ac_emu64_forget_actor(const ACTOR* actor) {
    uint64_t offset;
    int i;
    if (!actor_offset(actor, &offset))
        return;
    for (i = 0; i < AC_EMU64_LISTS; i++)
        sl_emu64_forget(emu, owner_of(offset, i));
}

void ac_port_report(const char* what) {
    sl_emu64_stats kit;
    sl_render_stats render;
    sl_emu64_get_stats(emu, &kit);
    sl_render_get_stats(&render);
    printf("ac_port: %s tick %" PRIu64 " retrace %" PRIu64 " gx %" PRIu64 " dvd %" PRIu64 " %" PRIu64
           " hash %016" PRIx64 " instances %" PRIu32 " compiles %" PRIu64 " holes %" PRIu64 "/%" PRIu64 " cpu %" PRIu64
           " refused %" PRIu64 " views %" PRIu64 " invalid %" PRIu64 " problems %" PRIu64 " overflows %" PRIu64
           " unowned %" PRIu64 "\n",
           what, sl_tick_count(), sl_retrace_count(), (uint64_t)ac_gx_bytes(), (uint64_t)ac_dvd_reads(),
           (uint64_t)ac_dvd_read_bytes(), sl_render_frame_hash(), kit.instances, kit.compiles, kit.vertex_holes,
           kit.list_holes, kit.cpu_draws, kit.refused, kit.view_overflows, render.frames_invalid, render.problems,
           render.overflows, (uint64_t)unowned);
    if (null_frames != 0)
        fprintf(stderr, "ac_port: %" PRIu64 " tasks without a frame\n", (uint64_t)null_frames);
    fflush(stdout);
    fflush(stderr);
}

/* One frame a task: its lists recorded on the kit, then submitted. The proof's check: the first frame play's exec
   draws ends the run, after its submit. */
void emu64_taskstart(Gfx* gfx) {
    sl_frame* const frame = sl_frame_begin();
    if (frame == NULL) {
        null_frames++;
    } else {
        sl_emu64_task(emu, frame, (const sl_emu64_gfx*)gfx);
        sl_frame_submit(frame);
    }
    if (game_class_p != NULL && game_class_p->exec == play_main) {
        ac_port_report("play");
        _Exit(0);
    }
}

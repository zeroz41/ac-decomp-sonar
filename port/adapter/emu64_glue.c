/* What the game calls of emu64 (src/static/libforest/emu64/emu64.c, whose entry points the port renames), on
   sonarlib's kits/emu64 (sonarlib docs/roadmap.md, M6 steps 6g and 6h): each task is recorded on the kit into the frame the GX
   glue holds open from JW_BeginFrame to JW_EndFrame (port/gx_sink.h), the game's edits on the kit's path come here
   (port/emu64_glue.h), and the program's proof stops the run at play's first frame, after the submit of the frame
   that holds play's first task (port/NOTES.md). Under --diff each task is drawn also by the console's emu64, the
   reference, built into the program under emu64_ref_ names, and the kit's and the reference's frames are compared
   (M6 step 6j). Under --goldens each frame goes to sonar/goldens.h with the keys Dolphin reads (M6 step 6n). Under
   --reference the kit is not made, and the reference draws every task into the game's own frame instead (M6 step
   7b). */
#include "game.h"
#include "graph.h"
#include "m_actor.h"
#include "m_common_data.h"
#include "m_play.h"
#include "libforest/emu64.h"
#include "libforest/emu64/emu64_wrapper.h"

#include "ac_port.h"

#include <port/emu64_glue.h>
#include <port/goldens.h>

#include <sonar/base/log.h>
#include <sonar/dolphin_os/os.h>
#include <sonar/emu64/emu64.h>
#include <sonar/goldens.h>
#include <sonar/render.h>
#include <sonar/render_capture.h>
#include <sonar/runtime.h>
#include <sonar/vi/vi.h>

#include <inttypes.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static u64 unowned;    /* actors drawn with no key, outside main memory and every table, without owners */
static BOOL play_task; /* play's first task was recorded */
static BOOL window;    /* --window: the run goes on after play's first frame */
static BOOL goldens;   /* --goldens: so does it, and each frame goes to the goldens */
static BOOL measure;   /* --measure: so does it */
static BOOL replay;    /* --replay: so does it, and each line names the scene */
static atomic_uint_fast64_t goldens_frames;
static atomic_int play_reached; /* play's first frame was submitted; the presenter reads it */

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

/* ---- The reference (--diff, --reference): the calls it runs inside the switch, kept until the next task */

static BOOL diff;
static BOOL reference; /* --reference: no kit; the reference draws each task into the game's frame */
static BOOL ref_init;        /* emu64_init was called since the last task */
static BOOL ref_ucode_info;  /* emu64_set_ucode_info was, with these arguments */
static int ref_ucode_count;
static ucode_info* ref_ucode_infos;
static BOOL ref_first_ucode; /* emu64_set_first_ucode was, with this argument */
static void* ref_first_ucode_arg;

void ac_diff_enable(void) {
    diff = TRUE;
}

BOOL ac_diff_enabled(void) {
    return diff;
}

void ac_reference_enable(void) {
    reference = TRUE;
}

BOOL ac_reference_enabled(void) {
    return reference;
}

void emu64_init(void) {
    if (diff || reference)
        ref_init = TRUE;
}

void emu64_set_ucode_info(int count, ucode_info* info) {
    if (!diff && !reference)
        return;
    ref_ucode_info = TRUE;
    ref_ucode_count = count;
    ref_ucode_infos = info;
}

void emu64_set_first_ucode(void* ucode) {
    if (!diff && !reference)
        return;
    ref_first_ucode = TRUE;
    ref_first_ucode_arg = ucode;
}

/* The reference's cleanup runs inside the switch, after its task. */
void emu64_cleanup(void) {
}

void emu64_refresh(void) {
    if (emu != NULL)
        sl_emu64_refresh(emu);
    if (diff || reference)
        emu64_ref_refresh();
}

void emu64_texture_cache_data_entry_set(void* begin, void* end) {
    if (emu != NULL)
        sl_emu64_static_range(emu, begin, end);
    if (diff || reference)
        emu64_ref_texture_cache_data_entry_set(begin, end);
}

void emu64_set_aflags(int idx, int value) {
    if (idx > 0 && idx < AC_AFLAGS_MAX)
        aflags[idx] = (u8)value;
    if (diff || reference)
        emu64_ref_set_aflags(idx, value);
}

int emu64_get_aflags(int idx) {
    if (idx > 0 && idx < AC_AFLAGS_MAX)
        return aflags[idx];
    return 0;
}

void ac_emu64_written(const void* begin, const void* end) {
    if (emu != NULL)
        sl_emu64_written(emu, begin, end);
}

/* ---- Owners and parts (port/emu64_glue.h): an actor's eleven owners, one a list, by its key */

#define AC_EMU64_TABLES 2              /* the NPCs' and the structures' */
#define AC_EMU64_TABLE_SLOTS 256u      /* a table's most slots */
#define AC_EMU64_TABLE_KEYS (1u << 27) /* table 0's slot 0, above every offset in main memory (dolphin_os.c) */
#define AC_EMU64_MADE 256              /* the most actors made between two ac_emu64_snap_made */

static struct {
    uintptr_t base;
    u32 stride;
    u32 count;
} tables[AC_EMU64_TABLES];

static uint64_t made[AC_EMU64_MADE]; /* the keys of the actors made since the last ac_emu64_snap_made */
static u32 made_count;
static BOOL made_past; /* more than AC_EMU64_MADE were */

void ac_emu64_actor_table(u32 table, const void* base, u32 stride, u32 count) {
    if (table >= AC_EMU64_TABLES || stride == 0 || count > AC_EMU64_TABLE_SLOTS) {
        SL_ERROR("emu64", "ac_emu64_actor_table(%u, %u, %u): past %d tables or %u slots", (unsigned)table,
                 (unsigned)stride, (unsigned)count, AC_EMU64_TABLES, (unsigned)AC_EMU64_TABLE_SLOTS);
        abort();
    }
    tables[table].base = (uintptr_t)base;
    tables[table].stride = stride;
    tables[table].count = count;
}

/* The actor's key: its offset from main memory's base, or for an actor in a table, the table's key for its slot;
   FALSE for an actor outside both. */
static BOOL actor_key(const ACTOR* actor, uint64_t* key) {
    const u8* const base = (const u8*)sl_dolphin_os_memory_base();
    const u8* const p = (const u8*)actor;
    const uintptr_t at = (uintptr_t)actor;
    u32 table;
    if (base != NULL && p >= base && p < base + sl_dolphin_os_memory_size()) {
        *key = (uint64_t)(p - base);
        return TRUE;
    }
    for (table = 0; table < AC_EMU64_TABLES; table++) {
        const uintptr_t first = tables[table].base;
        if (first != 0 && at >= first && at - first < (uintptr_t)tables[table].stride * tables[table].count) {
            *key = AC_EMU64_TABLE_KEYS + (uint64_t)table * AC_EMU64_TABLE_SLOTS +
                   (uint64_t)((at - first) / tables[table].stride);
            return TRUE;
        }
    }
    return FALSE;
}

static uint64_t owner_of(uint64_t key, int list) {
    return key << 4 | (uint64_t)(list + 1);
}

static THA_GA* list_of(GRAPH* graph, int list) {
    return (THA_GA*)((u8*)graph + list_fields[list]);
}

/* One G_NOOP of tag SL_EMU64_TAG_OWNER, opening owner, or closing with 0, at at. */
static void owner_marker(Gfx* at, uint64_t owner) {
    at->words.w0 = (u32)0x00 << 24 | (u32)SL_EMU64_TAG_OWNER << 16;
    at->words.w1 = (u32)owner; /* below 2^32: keys are below 2^28 */
}

/* One G_NOOP of tag SL_EMU64_TAG_PART, opening part, at at. */
static void part_marker(Gfx* at, u32 part) {
    at->words.w0 = (u32)0x00 << 24 | (u32)SL_EMU64_TAG_PART << 16;
    at->words.w1 = part;
}

/* The owner marker at the list's head. */
static Gfx* write_marker(THA_GA* list, uint64_t owner) {
    Gfx* const at = list->thaGfx.head_p;
    owner_marker(at, owner);
    list->thaGfx.head_p = at + 1;
    return at;
}

void ac_emu64_owner_open(GRAPH* graph, ACTOR* actor, ac_emu64_marks* marks) {
    uint64_t key;
    int i;
    if (emu == NULL || !actor_key(actor, &key)) {
        for (i = 0; i < AC_EMU64_LISTS; i++)
            marks->at[i] = NULL;
        if (emu != NULL)
            unowned++; /* with no kit, the lists are the console's */
        return;
    }
    /* A list graph_ct gives no buffer (the line lists) gets none. */
    for (i = 0; i < AC_EMU64_LISTS; i++) {
        THA_GA* const list = list_of(graph, i);
        marks->at[i] = list->thaGfx.buf_p != NULL ? write_marker(list, owner_of(key, i)) : NULL;
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
    uint64_t key;
    int i;
    if (emu == NULL || !actor_key(actor, &key))
        return;
    for (i = 0; i < AC_EMU64_LISTS; i++)
        sl_emu64_forget(emu, owner_of(key, i));
}

void ac_emu64_part_open(GRAPH* graph, u32 part, ac_emu64_marks* marks) {
    int i;
    for (i = 0; i < AC_EMU64_LISTS; i++) {
        THA_GA* const list = list_of(graph, i);
        marks->at[i] = NULL;
        if (emu != NULL && list->thaGfx.buf_p != NULL) {
            Gfx* const at = list->thaGfx.head_p;
            part_marker(at, part);
            list->thaGfx.head_p = at + 1;
            marks->at[i] = at;
        }
    }
}

/* An owner marker of w1 0 closes a part as it closes an owner. */
void ac_emu64_part_close(GRAPH* graph, const ac_emu64_marks* marks) {
    ac_emu64_owner_close(graph, marks);
}

Gfx* ac_emu64_part_open_at(Gfx** gfx_pp, u32 part) {
    Gfx* const at = *gfx_pp;
    if (emu == NULL)
        return NULL;
    part_marker(at, part);
    *gfx_pp = at + 1;
    return at;
}

void ac_emu64_part_close_at(Gfx** gfx_pp, Gfx* mark) {
    if (mark == NULL)
        return;
    if (*gfx_pp == mark + 1) {
        *gfx_pp = mark; /* the part drew nothing: the marker is taken back */
    } else {
        owner_marker(*gfx_pp, 0);
        (*gfx_pp)++;
    }
}

void ac_emu64_forget_part(const ACTOR* actor, u32 part) {
    uint64_t key;
    int i;
    if (emu == NULL || !actor_key(actor, &key))
        return;
    for (i = 0; i < AC_EMU64_LISTS; i++)
        sl_emu64_forget(emu, sl_emu64_part(owner_of(key, i), part));
}

void ac_emu64_snap_part(const ACTOR* actor, u32 part) {
    uint64_t key;
    int i;
    if (emu == NULL || !actor_key(actor, &key))
        return;
    for (i = 0; i < AC_EMU64_LISTS; i++)
        sl_emu64_snap(emu, sl_emu64_part(owner_of(key, i), part));
}

static void snap_key(uint64_t key) {
    int i;
    for (i = 0; i < AC_EMU64_LISTS; i++)
        sl_emu64_snap(emu, owner_of(key, i));
}

void ac_emu64_snap_actor(const ACTOR* actor) {
    uint64_t key;
    if (emu != NULL && actor_key(actor, &key))
        snap_key(key);
}

void ac_emu64_snap_all(void) {
    if (emu != NULL)
        sl_emu64_snap_all(emu);
}

void ac_emu64_actor_made(const ACTOR* actor) {
    uint64_t key;
    if (emu == NULL || !actor_key(actor, &key))
        return;
    if (made_count < AC_EMU64_MADE)
        made[made_count++] = key;
    else
        made_past = TRUE;
}

void ac_emu64_snap_made(void) {
    u32 i;
    if (emu == NULL)
        return;
    if (made_past) {
        sl_emu64_snap_all(emu);
    } else {
        for (i = 0; i < made_count; i++)
            snap_key(made[i]);
    }
    made_count = 0;
    made_past = FALSE;
}

void ac_port_report(const char* what) {
    sl_emu64_stats kit;
    sl_render_stats render;
    ac_gx_counts gx;
    sl_audio_mix_stats audio;
    memset(&kit, 0, sizeof kit); /* no kit under --reference */
    if (emu != NULL)
        sl_emu64_get_stats(emu, &kit);
    sl_render_get_stats(&render);
    ac_gx_get_counts(&gx);
    sl_audio_get_mix_stats(&audio);
    printf("ac_port: %s tick %" PRIu64 " retrace %" PRIu64 " gx %" PRIu64 " dvd %" PRIu64 " %" PRIu64
           " hash %016" PRIx64 " instances %" PRIu32 " compiles %" PRIu64 " holes %" PRIu64 "/%" PRIu64 " cpu %" PRIu64
           " refused %" PRIu64 " views %" PRIu64 " invalid %" PRIu64 " problems %" PRIu64 " overflows %" PRIu64
           " unowned %" PRIu64 " strays %" PRIu64 " gx-errors %" PRIu64 " unmapped %" PRIu64 " outside %" PRIu64
           " nested %" PRIu64 " refused-frames %" PRIu64 " audio %" PRIu64 " loud %" PRIu64 " dsp %" PRIu64
           " sound %016" PRIx64,
           what, sl_tick_count(), sl_retrace_count(), (uint64_t)ac_gx_bytes(), (uint64_t)ac_dvd_reads(),
           (uint64_t)ac_dvd_read_bytes(), sl_render_frame_hash(), kit.instances, kit.compiles, kit.vertex_holes,
           kit.list_holes, kit.cpu_draws, kit.refused, kit.view_overflows, render.frames_invalid, render.problems,
           render.overflows, (uint64_t)unowned, kit.strays, (uint64_t)gx.errors, (uint64_t)gx.unmapped,
           (uint64_t)gx.outside, (uint64_t)gx.nested, (uint64_t)gx.refused_frames, audio.frames, audio.loud_frames,
           (uint64_t)ac_dsp_sounding_frames(), audio.hash);
    if (reference)
        printf(" palette-overflows %" PRIu32, (uint32_t)emu64_ref_palette_overflows());
    if (replay)
        printf(" scene %d", Save_Get(scene_no));
    printf("\n");
    fflush(stdout);
    fflush(stderr);
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

/* GRAPH's lists' field names less _thaga, in list_fields' order. */
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
            const THA_GA* const list = list_of(game_class_p->graph, i);
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
static void diff_task(Gfx* gfx) {
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

/* The task's lists, recorded on the kit into the open frame after the front end's draws so far, or under --diff
   compared with the reference's. Under --reference the reference draws them through the GX front end into the open
   frame, and the GX state it leaves stays, as on the console. The proof's check marks play's first task: the frame
   that holds it ends the run at its submit (ac_emu64_frame_submitted). */
void emu64_taskstart(Gfx* gfx) {
    sl_frame* frame;
    if (diff) {
        diff_task(gfx);
    } else if (ac_frame_task(&frame)) {
        if (reference)
            reference_task(gfx);
        else
            sl_emu64_task(emu, frame, (const sl_emu64_gfx*)gfx);
    }
    if (game_class_p != NULL && game_class_p->exec == play_main)
        play_task = TRUE;
}

void ac_window_enable(void) {
    window = TRUE;
}

void ac_goldens_enable(void) {
    goldens = TRUE;
}

void ac_measure_enable(void) {
    measure = TRUE;
}

void ac_replay_enable(void) {
    replay = TRUE;
}

/* The keys Dolphin reads at the decomp's symbols (port/NOTES.md, Goldens), and the part of the frame buffer the VI
   shows: from VIConfigure's, the whole of it, the EFB's 640 x 480 before. */
void ac_goldens_frame(void) {
    sl_golden_key keys[5];
    sl_golden_rect shown = { 0, 0, 640, 480 };
    sl_vi_pan pan;
    u32 fader, fader_time;
    sl_result result;
    if (!goldens)
        return;
    ac_fader_state(&fader, &fader_time);
    keys[0].name = "menu_frame";
    keys[0].value = ac_initial_menu_frame();
    keys[1].name = "fader";
    keys[1].value = fader;
    keys[2].name = "fader_time";
    keys[2].value = fader_time;
    keys[3].name = "game_frame";
    keys[3].value = game_class_p != NULL ? (u32)game_class_p->frame_counter : 0;
    keys[4].name = "play";
    keys[4].value = game_class_p != NULL && game_class_p->exec == play_main;
    if (sl_vi_get_pan(&pan) == SL_OK) {
        shown.x = pan.x;
        shown.y = pan.y;
        shown.width = pan.width;
        shown.height = pan.height;
    }
    atomic_fetch_add(&goldens_frames, 1);
    result = sl_goldens_frame(keys, 5, &shown);
    if (result != SL_OK) {
        fprintf(stderr, "ac_port: sl_goldens_frame: %s\n", sl_result_string(result));
        _Exit(1);
    }
}

u64 ac_goldens_frames(void) {
    return (u64)atomic_load(&goldens_frames);
}

BOOL ac_play_reached(void) {
    return atomic_load(&play_reached) != 0;
}

void ac_emu64_frame_submitted(void) {
    if (play_task && !atomic_load(&play_reached)) {
        if (diff)
            printf("ac_port: diff tasks %" PRIu64 " equal %" PRIu64 " different %" PRIu64 " refused %" PRIu64
                   " triangles %" PRIu64 " skipped %" PRIu64 " %" PRIu64 "\n",
                   (uint64_t)diff_tasks, (uint64_t)diff_equal, (uint64_t)diff_different, (uint64_t)diff_refused,
                   (uint64_t)diff_triangles, (uint64_t)diff_skipped[0], (uint64_t)diff_skipped[1]);
        ac_port_report(reference ? "reference" : "play");
        if (!window && !goldens && !measure && !replay)
            _Exit(0);
        atomic_store(&play_reached, 1);
    }
}

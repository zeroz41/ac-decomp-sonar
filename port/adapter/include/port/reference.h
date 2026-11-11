/* The hooks the shared glue and the frame's seams call of the reference, the console's emu64 drawn through sonarlib's
   GX front end (sonarlib docs/steps/m6-10a.md, m6-10e.md). Two programs of one build define them: ac_port_reference
   by the reference's own files, port/adapter/emu64_reference.c, gx_reference.c and jsys_display_reference.c, with
   emu64's unit under the emu64_ref_ names, for the differential test (--diff, M6 step 6j) and the measured reference
   (--reference, M6 step 7b); ac_port, the player's program, by port/adapter/reference_none.c, where each does nothing
   and reports no reference. This header names no GX; the hooks that do are in port/display_reference.h. Each is for
   the game threads, one at a time, unless it says otherwise. */
#ifndef AC_PORT_REFERENCE_H
#define AC_PORT_REFERENCE_H

#include "dolphin/types.h"
#ifdef _LANGUAGE_C
#include "PR/mbi.h" /* Gfx: the game's sources and the glue build with _LANGUAGE_C, the SDK's adapters without */
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sl_emu64;
struct sl_frame;
struct sl_gxf;
struct ucode_info_s;

/* The program's name, ac_port or ac_port_reference, and the options its usage adds after --validate: none, or
   " [--diff [--diff-from <tick>]] [--reference]". */
extern const char ac_program_name[];
extern const char ac_reference_options[];

/* --diff and --reference, ac_port.c's, before boot: TRUE when the program has the reference, and the run takes it;
   FALSE in ac_port, which gives the usage. */
BOOL ac_diff_enable(void);
BOOL ac_reference_enable(void);
/* Whether the run took --diff, or --reference; FALSE in ac_port. Any game thread. */
BOOL ac_diff_enabled(void);
BOOL ac_reference_enabled(void);
/* --diff-from's tick, ac_port.c's, before boot, which needs --diff: before it each task is still drawn by both and
   compared, so the reference's state follows the console's, but nothing is counted or printed. TRUE in
   ac_port_reference; FALSE in ac_port, which gives the usage. */
BOOL ac_diff_from(uint64_t tick);

/* OSInit's, once main memory exists: under either flag, the reference's buffers from the bottom of the arena. */
void ac_reference_boot(void);

/* The game's emu64 entry points (emu64_glue.c) call these each time: under either flag the reference keeps the calls
   it runs inside the switch until the next task, and runs the others, as the console's emu64 did; otherwise they do
   nothing. */
void ac_reference_init(void);
void ac_reference_set_ucode_info(int count, struct ucode_info_s* info);
void ac_reference_set_first_ucode(void* ucode);
void ac_reference_refresh(void);
void ac_reference_static_range(void* begin, void* end);
void ac_reference_set_aflags(int idx, int value);

#ifdef _LANGUAGE_C
/* emu64_taskstart's: under --diff, the task drawn by the kit and by the reference into frames of their own and the
   captures compared; under --reference, the task drawn by the reference into the game's open frame. TRUE under
   either, and FALSE otherwise, when the glue records the task on the kit. */
BOOL ac_reference_task(struct sl_emu64* kit, Gfx* gfx);
#endif

/* ac_port_report's, before its scene: " palette-overflows <n>" under --reference. */
void ac_reference_report(void);
/* ac_emu64_frame_submitted's, before the proof's line, and at a replay's end, before the replay's: the differential
   test's line under --diff. A task counts as equal only when its captures compare and hold as many copies; the line's
   copies are the kit's in the equal tasks. */
void ac_reference_submitted(void);

/* The GX glue's hand-over (dolphin_gx.c), with the bytes written outside display lists since the last, offset bytes
   into that stream. Under --diff or --reference, TRUE: between ac_gx_reference_begin and ac_gx_reference_end the bytes
   went to the reference's front end, and otherwise to the main front end, made at the first with SL_GXF_NO_CLEAR, or
   they were dropped as an error when it could not be made; the glue empties its buffer. Otherwise FALSE, and the glue
   judges the bytes itself. */
BOOL ac_reference_gx(const u8* bytes, size_t size, u64 offset);
/* The GX glue's record (dolphin_gx.c), after its hand-over, into the open frame: under either flag, the main front
   end's draws gathered since its last record, with sl_gxf_record when first, its first record of the frame, and
   sl_gxf_record_more otherwise; a refusal is counted by ac_gx_record_refused. Nothing for a NULL frame, or before the
   main front end exists. */
void ac_reference_record(struct sl_frame* frame, BOOL first);
/* The main front end, gx_reference.c's: NULL before the first hand-over under either flag, when it could not be made,
   and always in ac_port, which defines neither. */
struct sl_gxf* ac_gx_front_end(void);
/* A front end on GX's address space (port/gx_sink.h, ac_gx_memory), with flags (SL_GXF_*); NULL when out of memory.
   ac_port_reference's only. */
struct sl_gxf* ac_gx_create_front_end(uint32_t flags);

/* The GX calls of the frame's seams that only make the GPU wait, flush or sync (port/adapter/jsys_display.cpp,
   jsys_video.cpp, libultra_shutdown.c). Under --diff or --reference each is the GX call it names, as the reference's
   front end reads the stream the console's GX did; otherwise nothing. GX's draw-done callback stays NULL in both
   programs: a seam calls the game's (port/jsys_display.h, ac_jsys_video_draw_done) after a draw-done call. */
typedef enum ac_reference_sync_call {
    AC_REFERENCE_FLUSH,         /* GXFlush */
    AC_REFERENCE_DRAW_DONE,     /* GXDrawDone */
    AC_REFERENCE_SET_DRAW_DONE, /* GXSetDrawDone */
    AC_REFERENCE_ABORT_FRAME,   /* GXAbortFrame */
    AC_REFERENCE_PIX_MODE_SYNC  /* GXPixModeSync */
} ac_reference_sync_call;
void ac_reference_sync(ac_reference_sync_call call);
/* GXCopyDisp(xfb, GX_TRUE), the display copy that clears the EFB, under either flag; otherwise nothing. The copy's
   set-up is ac_reference_prepare_copy's (port/display_reference.h). */
void ac_reference_copy_display(void* xfb);

#ifdef __cplusplus
}
#endif

#endif

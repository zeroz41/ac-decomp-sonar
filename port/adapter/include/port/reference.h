/* The hooks the shared glue calls of the reference, the console's emu64 drawn through sonarlib's GX front end (sonarlib
   docs/steps/m6-10a.md). Two programs of one build define them: ac_port_reference by the reference's own files,
   port/adapter/emu64_reference.c and gx_reference.c, with emu64's unit under the emu64_ref_ names, for the
   differential test (--diff, M6 step 6j) and the measured reference (--reference, M6 step 7b); ac_port, the player's
   program, by port/adapter/reference_none.c, where each does nothing and reports no reference. Each is for the game
   threads, one at a time, unless it says otherwise. */
#ifndef AC_PORT_REFERENCE_H
#define AC_PORT_REFERENCE_H

#include "dolphin/types.h"
#ifdef _LANGUAGE_C
#include "PR/mbi.h" /* Gfx: the game's sources and the glue build with _LANGUAGE_C, the SDK's adapters without */
#endif

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sl_emu64;
struct ucode_info_s;

/* The program's name, ac_port or ac_port_reference, and the options its usage adds after --validate: none, or
   " [--diff] [--reference]". */
extern const char ac_program_name[];
extern const char ac_reference_options[];

/* --diff and --reference, ac_port.c's, before boot: TRUE when the program has the reference, and the run takes it;
   FALSE in ac_port, which gives the usage. */
BOOL ac_diff_enable(void);
BOOL ac_reference_enable(void);
/* Whether the run took --diff, or --reference; FALSE in ac_port. Any game thread. */
BOOL ac_diff_enabled(void);
BOOL ac_reference_enabled(void);

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
/* ac_emu64_frame_submitted's, before the proof's line: the differential test's line under --diff. */
void ac_reference_submitted(void);

/* The GX glue's hand-over (dolphin_gx.c), once the main front end exists, with the bytes written outside display
   lists since the last, offset bytes into that stream. Between ac_gx_reference_begin and ac_gx_reference_end, TRUE:
   the bytes went to the reference's front end, or were dropped as an error when it could not be made, and the glue
   empties its buffer. Otherwise FALSE. */
BOOL ac_reference_gx(const u8* bytes, size_t size, u64 offset);

#ifdef __cplusplus
}
#endif

#endif

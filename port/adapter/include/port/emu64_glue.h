/* What the game's Portability edits and the GX glue call of the port's emu64 glue (port/adapter/emu64_glue.c), which
   records each of emu64's tasks on sonarlib's kits/emu64 into the frame the GX glue holds open (port/gx_sink.h;
   sonarlib docs/roadmap.md, M6 steps 6g and 6h). Game thread. */
#ifndef AC_PORT_EMU64_GLUE_H
#define AC_PORT_EMU64_GLUE_H

#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct graph_s;
struct actor_s;
struct sl_emu64;

/* GRAPH's eleven display lists, its THA_GA fields in their order (graph.h): work, line opaque, line translucent,
   overlay, polygon opaque, polygon translucent, font, shadow, light, bg opaque, bg translucent. */
#define AC_EMU64_LISTS 11

/* Where ac_emu64_owner_open wrote each list's opening marker; NULL for none. On the caller's stack. */
typedef struct ac_emu64_marks {
    void* at[AC_EMU64_LISTS];
} ac_emu64_marks;

/* The emu64 the glue records each of emu64's tasks with, made by the program (main.c). Main thread, before the game
   runs. */
void ac_emu64_start(struct sl_emu64* emu);

/* The game wrote static data in [begin, end): the programs that read it compile again at their next use. */
void ac_emu64_written(const void* begin, const void* end);

/* The actor's draws are its owner's, one owner a list: an opening marker at each list's head, kept in marks. An actor
   outside main memory gets none, and is counted. */
void ac_emu64_owner_open(struct graph_s* graph, struct actor_s* actor, ac_emu64_marks* marks);
/* Takes back the marker of each list the actor did not draw into, whose head is still just past it, and writes a
   closing marker at the head of each other list. */
void ac_emu64_owner_close(struct graph_s* graph, const ac_emu64_marks* marks);
/* The actor is gone: its eleven owners are forgotten, so a later actor at its address starts unmatched. */
void ac_emu64_forget_actor(const struct actor_s* actor);

/* The GX glue's ac_frame_end, after its submit: once the frame holds play's first task, prints the proof's line and
   ends the run, exit 0, but under --window (ac_window_enable), where the run goes on. */
void ac_emu64_frame_submitted(void);

/* The differential test (port/NOTES.md, --diff; sonarlib docs/roadmap.md, M6 step 6j): each task is drawn by the
   kit and by the console's emu64, the reference, into frames of their own, and the two captures are compared.
   ac_diff_enable is main.c's, before boot; ac_diff_enabled is for any game thread. */
void ac_diff_enable(void);
BOOL ac_diff_enabled(void);

/* The reference's buffers, which OSInit takes from the bottom of the arena under --diff: emu64.c's two texture
   caches and its palette buffer, each 32-byte aligned. */
#define AC_DIFF_TEXTURE_DATA_SIZE 0xC000 /* emu64.c's TEX_BUFFER_DATA_SIZE */
#define AC_DIFF_TEXTURE_BSS_SIZE 0x400   /* emu64.c's TEX_BUFFER_BSS_SIZE */
#define AC_DIFF_PALETTES_SIZE 0x8000     /* GX's largest palette */

/* emu64.c, the reference, under the names port/CMakeLists.txt gives it. Only the glue calls it, under --diff, but
   for emu64_ref_set_buffers, which is OSInit's. */
struct ucode_info_s;
void emu64_ref_set_buffers(void* data, void* bss, void* palettes, u32 palettes_size);
u32 emu64_ref_palette_overflows(void);
void emu64_ref_init(void);
void emu64_ref_cleanup(void);
void emu64_ref_taskstart(void* gfx);
void emu64_ref_set_ucode_info(int count, struct ucode_info_s* info);
void emu64_ref_set_first_ucode(void* ucode);
void emu64_ref_refresh(void);
void emu64_ref_texture_cache_data_entry_set(void* begin, void* end);
void emu64_ref_set_aflags(int idx, int value);

#ifdef __cplusplus
}
#endif

#endif

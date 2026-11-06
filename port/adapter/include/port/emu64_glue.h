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
   ends the run, exit 0. */
void ac_emu64_frame_submitted(void);

#ifdef __cplusplus
}
#endif

#endif

/* What the game's Portability edits and the GX glue call of the port's emu64 glue (port/adapter/emu64_glue.c), which
   records each of emu64's tasks on sonarlib's kits/emu64 into the frame the GX glue holds open (port/gx_sink.h;
   sonarlib docs/roadmap.md, M6 steps 6g and 6h). Game thread. */
#ifndef AC_PORT_EMU64_GLUE_H
#define AC_PORT_EMU64_GLUE_H

#include "dolphin/types.h"
#ifdef _LANGUAGE_C
/* Gfx and THA_GA, a typedef of an unnamed union, so not declared ahead: the game's sources and the glue build with
   _LANGUAGE_C, the SDK's adapters without. */
#include "PR/mbi.h"
#include "THA_GA.h"
#endif

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

/* The emu64 the glue records each of emu64's tasks with, made by the program (ac_port.c). Main thread, before the game
   runs. */
void ac_emu64_start(struct sl_emu64* emu);

/* The game wrote static data in [begin, end): the programs that read it compile again at their next use. */
void ac_emu64_written(const void* begin, const void* end);

/* An actor's owners are keyed, one owner a list, as key << 4 | (list + 1): the key of an actor in main memory is its
   offset there, and of one in a table ac_emu64_actor_table registered 2^27 + table * 2^8 + slot, so keys stay below
   2^28 and the owners below 2^32, a marker's w1. */

/* The actor's draws are its owner's, one owner a list: an opening marker at each list's head, kept in marks. An actor
   with no key, outside main memory and every table, gets none, and is counted. */
void ac_emu64_owner_open(struct graph_s* graph, struct actor_s* actor, ac_emu64_marks* marks);
/* Takes back the marker of each list the actor did not draw into, whose head is still just past it, and writes a
   closing marker at the head of each other list. */
void ac_emu64_owner_close(struct graph_s* graph, const ac_emu64_marks* marks);
/* The actor is gone: its eleven owners are forgotten, so a later actor at its key starts unmatched. */
void ac_emu64_forget_actor(const struct actor_s* actor);

/* A table of count actors outside main memory, from base, an element each stride bytes: an actor inside element k is
   slot k of the table. Table 0 is the NPCs' (ac_npc_ctrl.c_inc), table 1 the structures' (ac_structure_clip.c_inc);
   at most 256 slots. Registering a table again replaces it. */
void ac_emu64_actor_table(u32 table, const void* base, u32 stride, u32 count);

/* The draws from here to ac_emu64_part_close are part `part` (below 2^24) of the innermost owner, in each list: a
   part marker at each list's head, kept in marks (sonarlib kits/emu64 gfx.h, SL_EMU64_TAG_PART). */
void ac_emu64_part_open(struct graph_s* graph, u32 part, ac_emu64_marks* marks);
/* Takes back the part marker of each list the part did not draw into, and closes it in each other list. */
void ac_emu64_part_close(struct graph_s* graph, const ac_emu64_marks* marks);
/* The same in the one list a draw holds open at *gfx_pp, as part_open's and part_close's: the marker, NULL for none. */
#ifdef _LANGUAGE_C
Gfx* ac_emu64_part_open_at(Gfx** gfx_pp, u32 part);
void ac_emu64_part_close_at(Gfx** gfx_pp, Gfx* mark);
#endif

/* The task's mark from *gfx_pp on: an anchor marker at *gfx_pp, stepped past (sonarlib kits/emu64 gfx.h,
   SL_EMU64_TAG_ANCHOR). The word is an sl_anchor (sonar/render.h), a column and at most one row, with or without
   AC_EMU64_ANCHOR_FRAME. It shows on a display wider than the game's and at a UI scale, and says where an element
   belongs that only the game knows:
   - In the scene's view, a perspective of the whole frame, which the kit stretches, the draws after it take the
     anchor. A centered draw, as every task starts, is part of the widened view's world, and a stretched one
     stretches across the display as the game drew it across the frame, which is what a wipe's model needs to cover
     the display.
   - The game's 2D after it, every other view, is laid out against the anchor's edge of the display's safe area and
     scaled about it by the UI scale: the clock's bottom corner, the dialogue's bottom. With no mark it is centered.
   The caller sets the mark back to SL_ANCHOR_CENTER after its element. Under --reference it writes nothing. */
#ifdef _LANGUAGE_C
void ac_emu64_anchor_at(Gfx** gfx_pp, u32 anchor);
#endif
/* Beside the anchor in ac_emu64_anchor_at's word: the 2D after it keeps the frame's size at its anchor's place
   whatever the UI scale, for an element the game places from a point of its world, as the item-name bubble. It is
   the kit's SL_EMU64_ANCHOR_FRAME under the port's name, as game code includes no kit header. */
#define AC_EMU64_ANCHOR_FRAME 0x100u

/* The part of the actor's eleven owners is gone: forgotten, so what next takes its slot starts unmatched. */
void ac_emu64_forget_part(const struct actor_s* actor, u32 part);
/* The part of the actor's eleven owners jumped: it shows unblended in the next task's frame. */
void ac_emu64_snap_part(const struct actor_s* actor, u32 part);
/* The actor's eleven owners jumped. */
void ac_emu64_snap_actor(const struct actor_s* actor);
/* Nothing blends in the next task's frame: a scene change (game_ct). */
void ac_emu64_snap_all(void);

/* The actor was made, its ct_proc called (m_actor.c): its first move may jump, so ac_emu64_snap_made snaps it. Up to
   256 actors are kept, by their keys; past them the next ac_emu64_snap_made snaps the whole frame. */
void ac_emu64_actor_made(const struct actor_s* actor);
/* Snaps the owners of each actor made since the last call, and clears the record: Actor_info_call_actor's, before
   its moves. */
void ac_emu64_snap_made(void);

/* With no kit (--reference), none of the calls above writes or marks anything. */

/* The GX glue's ac_frame_end, after its submit: once the frame holds play's first task, prints the proof's line,
   named reference under --reference, and ends the run, exit 0, but under --window, --goldens and --measure, where the
   run goes on. */
void ac_emu64_frame_submitted(void);

/* The graph's display list of that index, in GRAPH's field order (AC_EMU64_LISTS), for the reference's names of
   the kit's draws (emu64_reference.c). */
#ifdef _LANGUAGE_C
THA_GA* ac_emu64_list(struct graph_s* graph, int list);
#endif

#ifdef __cplusplus
}
#endif

#endif

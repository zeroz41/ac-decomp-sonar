/* What the game's culls call of the port's culling hook (port/adapter/ac_cull.c): on a display wider than the game's
   frame, what the game culls at the frame's sides is drawn out to the display's, and the game's own cull results, which
   its simulation reads, stay as they were (sonarlib docs/steps/m6-9c3.md; docs/rendering.md, Widescreen). The edits
   that call it are of the Enhancement kind: with sonar.video.widescreen off, at 4:3 and in every headless run the
   widening is 1, each check is the game's and no actor is drawn for the display alone. Game thread. */
#ifndef AC_PORT_CULL_H
#define AC_PORT_CULL_H

#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct actor_s;
struct xyz_s;

/* The game's Actor_draw_actor_no_culling_check2 with the display's widening (sl_render_widening) where its test of
   the clip x over w has 1: TRUE for what stands in the widened view. A result for drawing only. */
int ac_cull_wide_check(struct actor_s* actor, struct xyz_s* camera_pos, f32 camera_w);

/* TRUE for an actor to draw for the display alone, after the game's own draw or delete check of it
   (Actor_info_draw_actor), whose do_not_draw it takes: the game left it undrawn and alive, with neither NO_CULL nor
   NO_DRAW_WHILE_CULLED, the game's other conditions to draw hold, and ac_cull_wide_check passes. The actor's instance,
   class_size bytes of its profile, and qrand's two words are then kept for ac_cull_wide_end, which the caller calls
   after Actor_draw. FALSE, too, for an actor with no profile and when the copy cannot be allocated. */
BOOL ac_cull_wide_begin(struct actor_s* actor, int do_not_draw);
/* Puts back the instance and qrand's words ac_cull_wide_begin kept, so the draw leaves the actor as the game's cull
   left it, drawn false among the rest, and the random stream where it was. */
void ac_cull_wide_end(struct actor_s* actor);

#ifdef __cplusplus
}
#endif

#endif

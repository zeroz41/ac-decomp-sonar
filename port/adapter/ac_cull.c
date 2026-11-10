/* The port's culling hook (port/cull.h; sonarlib docs/steps/m6-9c3.md, M6 step 9c3; docs/rendering.md, Widescreen;
   docs/decisions.md D40). The game culls against its 4:3 frame, and one cull result both draws and simulates: the
   actors' sets ACTOR_STATE_NO_CULL, which the update pass reads and the draw pass deletes by. So on a wider display
   the hook gives a second result, for drawing alone, and the game's stays the game's:
   - an actor the game culls is drawn when it stands in the widened view, and what its draw wrote, its instance and
     qrand's two words, is put back, so the simulation meets what a 4:3 run left it;
   - a bg item's cull flag, which only its draw loops read, is set by the widened view.
   The display's widening is sonarlib's sl_render_widening, 1 with sonar.video.widescreen off, at 4:3 and headless,
   where each check is the game's and nothing is drawn for the display alone.

   A draw that writes outside its actor's instance and qrand would leave that write behind. None is known: a replay in
   a wide window must write the headless run's state hashes (port/tests/state_test.cmake). An actor found to do so
   keeps the game's result by a test of its id in ac_cull_wide_begin. Fish and insects cull in their move pass and do
   not come through here (port/NOTES.md, The window). Game thread. */
#include "libc64/qrand.h"
#include "m_actor.h"

#include <port/cull.h>

#include <sonar/render.h>

#include <stdlib.h>
#include <string.h>

/* The instance and the random words ac_cull_wide_begin kept, until ac_cull_wide_end: one actor at a time. The buffer
   is the host's and only grows, so the game's heaps are as a 4:3 run's. */
static u8* kept;
static size_t kept_room;
static size_t kept_size; /* 0: nothing is kept */
static u32 kept_idum;
static u32 kept_itemp;

/* Actor_draw_actor_no_culling_check2 (m_actor.c), its test of x against side in place of 1. */
static int in_view(const ACTOR* actor, const xyz_t* camera_pos, f32 camera_w, f32 side) {
    int res = FALSE;

    if (-actor->cull_radius < camera_pos->z && camera_pos->z < actor->cull_distance + actor->cull_radius) {
        f32 m = camera_w < 1.0f ? 1.0f : 1.0f / camera_w;
        int width_OK = (m * (fabsf(camera_pos->x) - actor->cull_width)) < side;

        if (width_OK && (m * (camera_pos->y + actor->cull_height)) > -1.0f &&
            (m * (camera_pos->y - actor->cull_radius) < 1.0f)) {
            res = TRUE;
        }
    }

    return res;
}

int ac_cull_wide_check(ACTOR* actor, xyz_t* camera_pos, f32 camera_w) {
    return in_view(actor, camera_pos, camera_w, sl_render_widening());
}

BOOL ac_cull_wide_begin(ACTOR* actor, int do_not_draw) {
    const ACTOR_PROFILE* profile;
    const u32* idum;
    const u32* itemp;
    f32 widening;
    size_t size;

    /* The game drew it, deleted it, or draws it culled. */
    if (actor->drawn != FALSE || actor->dw_proc == NULL ||
        (actor->state_bitfield & (ACTOR_STATE_NO_DRAW_WHILE_CULLED | ACTOR_STATE_NO_CULL)) != 0) {
        return FALSE;
    }
    /* The game's other conditions to draw (Actor_info_draw_actor). */
    if (do_not_draw != FALSE || actor->ct_proc != NULL || (actor->state_bitfield & ACTOR_STATE_INVISIBLE) != 0 ||
        actor->cull_while_talking != FALSE || actor->skip_drawing != FALSE) {
        return FALSE;
    }
    /* At a widening of 1 the check below is the game's, which this actor failed. */
    widening = sl_render_widening();
    if (!(widening > 1.0f) || in_view(actor, &actor->camera_position, actor->camera_w, widening) == FALSE) {
        return FALSE;
    }

    profile = actor->dlftbl != NULL ? actor->dlftbl->profile : NULL;
    size = profile != NULL ? profile->class_size : 0;
    if (size < sizeof(ACTOR)) {
        return FALSE;
    }
    if (size > kept_room) {
        u8* grown = (u8*)realloc(kept, size);

        if (grown == NULL) {
            return FALSE;
        }
        kept = grown;
        kept_room = size;
    }
    memcpy(kept, actor, size);
    kept_size = size;
    qrand_state(&idum, &itemp);
    kept_idum = *idum;
    kept_itemp = *itemp;
    return TRUE;
}

void ac_cull_wide_end(ACTOR* actor) {
    if (kept_size == 0) {
        return;
    }
    memcpy(actor, kept, kept_size);
    kept_size = 0;
    qrand_set_state(kept_idum, kept_itemp);
}

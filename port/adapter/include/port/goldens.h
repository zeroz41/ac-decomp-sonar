/* What the game's Portability edits give the port's goldens (port/adapter/emu64_glue.c, port/NOTES.md, Goldens), and
   what they call of it: the keys Dolphin reads at the decomp's symbols, read natively, and qrand's seed. Game
   thread. */
#ifndef AC_PORT_GOLDENS_H
#define AC_PORT_GOLDENS_H

#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* initial_menu's frame_count (initial_menu.c). */
u32 ac_initial_menu_frame(void);
/* jc_fader's mStatus, and mTicksTarget << 16 | mTicksRun, into *status and *time; 0 and 0 while there is none
   (jsyswrap.cpp). */
void ac_fader_state(u32* status, u32* time);
/* The seed init_rnd gives sqrand, for the tick it would give: --qrand-seed's value when it is given, else tick
   (main.c). */
u32 ac_port_rnd_seed(u32 tick);

#ifdef __cplusplus
}
#endif

#endif

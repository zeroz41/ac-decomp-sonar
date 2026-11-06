/* What the game calls of emu64 (src/static/libforest/emu64/emu64.c), which the port does not compile yet: emu64's
   entry points on sonarlib's kit come with step 6g. Until then a frame's display list is not run, and
   emu64_taskstart is where the program's proof stops the run: at play's first frame (port/README.md). */
#include "game.h"
#include "m_play.h"
#include "libforest/emu64.h"
#include "libforest/emu64/emu64_wrapper.h"

#include "ac_port.h"

#include <sonar/runtime.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

/* emu64.hpp's AFLAGS_MAX: emu64 keeps its debug flags only in its debug build. */
#ifdef EMU64_DEBUG
#define AC_AFLAGS_MAX 100
#else
#define AC_AFLAGS_MAX 0
#endif

static u8 aflags[AC_AFLAGS_MAX > 0 ? AC_AFLAGS_MAX : 1];

u8 FrameCansel = FALSE;

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
}

void emu64_texture_cache_data_entry_set(void* begin, void* end) {
    (void)begin;
    (void)end;
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

void ac_port_report(const char* what) {
    printf("ac_port: %s tick %" PRIu64 " retrace %" PRIu64 " gx %" PRIu64 " dvd %" PRIu64 " %" PRIu64 "\n", what,
           sl_tick_count(), sl_retrace_count(), (uint64_t)ac_gx_bytes(), (uint64_t)ac_dvd_reads(),
           (uint64_t)ac_dvd_read_bytes());
    fflush(stdout);
}

/* The proof's check: the first frame play's exec draws ends the run. */
void emu64_taskstart(Gfx* gfx) {
    (void)gfx;
    if (game_class_p != NULL && game_class_p->exec == play_main) {
        ac_port_report("play");
        _Exit(0);
    }
}

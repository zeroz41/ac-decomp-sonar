/* The adapter's entry points for the port's start-up. */
#ifndef AC_PORT_H
#define AC_PORT_H

#include "dolphin/types.h"

#include <sonar/audio.h>
#include <sonar/fs.h>

#include <port/gx_sink.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The save store the memory cards keep their files in, for CARDInit to take; none keeps them in memory only. Main
   thread, before CARDInit. */
void ac_card_set_store(sl_save_store store);
/* Inserts the card the store holds under a name of 1 to 59 characters of a-z, 0-9, '_' and '-', or a new one, in
   slot chan; before CARDInit it waits for it. FALSE when the slot holds a card or the store cannot be read. Main
   thread. */
BOOL ac_card_insert(s32 chan, const char* name);
/* Ejects slot chan's card, as pulling it out of the console does. Main thread. */
void ac_card_eject(s32 chan);

/* The console's audio RAM as host memory, and its size in *size, for the DSP to read its samples from. Any
   thread. */
const u8* ac_aram_memory(u32* size);

/* Starts sonarlib audio for the console's audio interface: the caller's sink, tier, output rate, latency, driver,
   coefficient ROM and capture callback, with the interface's rate and block for the console rate and frame, on an
   os alarm (sl_audio_init's errors). Main thread, after os starts and before the game's AIInit. */
sl_result ac_audio_init(const sl_audio_desc* desc);
/* Stops it, as sl_audio_shutdown. Main thread. */
void ac_audio_shutdown(void);

/* The DVD glue's reads since start-up and the bytes they read (dolphin_dvd.c). Game thread. */
u64 ac_dvd_reads(void);
u64 ac_dvd_read_bytes(void);

/* Prints the proof's line, "ac_port: <what> tick <t> retrace <r> gx <bytes> dvd <reads> <bytes> hash <h> instances
   <n> compiles <n> holes <vertex>/<list> cpu <n> refused <n> views <overflows> invalid <frames> problems <n>
   overflows <n> unowned <n>", to stdout and flushes it (emu64_glue.c): the run's counts, the hash of the frame
   submitted last, the kit's counts with its last task's instances, and the render module's. */
void ac_port_report(const char* what);

#ifdef __cplusplus
}
#endif

#endif

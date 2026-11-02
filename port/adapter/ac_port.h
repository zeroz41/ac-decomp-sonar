/* The adapter's entry points for the port's start-up. */
#ifndef AC_PORT_H
#define AC_PORT_H

#include "dolphin/types.h"

#include <sonar/audio.h>
#include <sonar/fs.h>

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

#ifdef __cplusplus
}
#endif

#endif

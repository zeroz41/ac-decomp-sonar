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

/* Writes the host's audiorom.img layout, from the package's audiorom.tbl, into the game's bank and data tables
   (AudiobankHeaderStart, AudiodataHeaderStart): each entry's addr and size (dolphin_dvd.c, port/NOTES.md, Sound).
   Stops the run with a message for a table that is missing or not this build's. Nas_InitAudio's, before it reads
   them. */
void ac_audio_rom_tables(void);

/* The frames command 2 gave whose rendered channels hold a sample at 1/256 of full scale or above (dolphin_dsp.c).
   Any thread. */
u64 ac_dsp_sounding_frames(void);

/* The DVD glue's reads since start-up and the bytes they read (dolphin_dvd.c). Game thread. */
u64 ac_dvd_reads(void);
u64 ac_dvd_read_bytes(void);

/* --window: play's first frame prints the play line and ends no run (emu64_glue.c). main.c's, before boot. */
void ac_window_enable(void);
/* Whether play's first frame was submitted. Any thread. */
BOOL ac_play_reached(void);

/* --goldens: play's first frame prints the play line and ends no run, and each frame's begin hands the frame submitted
   before it to the goldens with its keys (emu64_glue.c). main.c's, before boot. */
void ac_goldens_enable(void);
/* The GX glue's frame begin, before it begins the frame, when a frame was submitted since the last: with the goldens
   on, calls sl_goldens_frame for that frame with the keys as they stand, after its tick, and the VI's pan; a failure
   stops the run with a message. Game thread. */
void ac_goldens_frame(void);
/* The frames handed to the goldens. Any thread. */
u64 ac_goldens_frames(void);

/* --measure: play's first frame prints the play line and ends no run (emu64_glue.c). main.c's, before boot. */
void ac_measure_enable(void);
/* The GX glue's frame end, after its submit and ac_emu64_frame_submitted, for a frame submitted: from play's first
   frame on, with the measured run on, calls sl_measure_frame, which waits until the main thread has presented it, and
   keeps the tick; a failure stops the run with a message (main.c). Game thread. */
void ac_measure_frame(void);

/* Prints the proof's line, "ac_port: <what> tick <t> retrace <r> gx <bytes> dvd <reads> <bytes> hash <h> instances
   <n> compiles <n> holes <vertex>/<list> cpu <n> refused <n> views <overflows> invalid <frames> problems <n>
   overflows <n> unowned <n> gx-errors <n> unmapped <n> outside <n> nested <n> refused-frames <n> audio <n> loud <n>
   dsp <n> sound <h>", to stdout and flushes it (emu64_glue.c): the run's counts, the hash of the frame submitted last,
   the kit's counts with its last task's instances, the render module's, the GX glue's (ac_gx_get_counts), the audio
   mix's (sl_audio_get_mix_stats) and the DSP's sounding frames (ac_dsp_sounding_frames). Under --reference the kit's
   counts are 0 and the line ends with " palette-overflows <n>", emu64's palettes refused past its buffer. */
void ac_port_report(const char* what);

#ifdef __cplusplus
}
#endif

#endif

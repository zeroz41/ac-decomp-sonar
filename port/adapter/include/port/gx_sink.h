/* The sink the SDK's GX units write their command bytes into, in place of the console's write gather pipe, GX's
   address space, and the frame sonarlib's GX front end and emu64's kit record into (port/adapter/dolphin_gx.c;
   sonarlib docs/roadmap.md, M6 step 6h). Outside a display list the bytes go to the GX front end at each GXFlush and
   before each record; between GXBeginDisplayList and GXEndDisplayList they go into the list's buffer.

   AC begins frames on several game threads: the main thread, initial_menu's, the graph thread and whichever runs
   dvderr. Core os runs one game thread at a time, as the console's one CPU did, so they share the sink, the front end
   and the frame: each function is for the game threads, one at a time, unless it says otherwise. */
#ifndef AC_PORT_GX_SINK_H
#define AC_PORT_GX_SINK_H

#include "dolphin/types.h"

#include <sonar/base/result.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sl_frame;
struct sl_gxf;

/* Each appends its value's bytes, big-endian, as the write gather pipe did. */
void ac_gx_write_u8(u8 value);
void ac_gx_write_u16(u16 value);
void ac_gx_write_u32(u32 value);
void ac_gx_write_f32(f32 value);

/* GX's address space, of 32 bits, of which the registers that take an address as 32-byte blocks hold 24, so 512 MiB:
   main memory at 0 to its size, then the image's data ranges (sl_mem_image_data) in address order, then its
   zero-initialized ranges (sl_mem_image_zero_data) in address order, each at the end of the one before rounded up to
   32, plus its begin's offset within 32 bytes. The two share a table of 16 ranges. A range past the table, or that
   would end past 0x20000000, is not mapped, and is logged. OSInit builds it once main memory exists, before any thread but the main one runs;
   it is read-only after. Main thread. */
void ac_gx_map_memory(void);
/* A pointer the GPU read, as the 32-bit value a unit writes: its GX address when it lies in main memory or a mapped
   range, and otherwise 0, counted as unmapped. NULL is no address: 0, not counted. */
u32 ac_gx_address(const void* pointer);
/* ac_gx_address without the count, for names in reports: TRUE with the address when the pointer lies in main memory
   or a mapped range, FALSE for NULL and any other pointer. */
BOOL ac_gx_find_address(const void* pointer, u32* address);
/* ac_gx_address for a TLUT load, whose address the front end masks to 25 bits: an address at or past 0x02000000 is
   0, counted as unmapped too. */
u32 ac_gx_tlut_address(const void* pointer);
/* A GX address back to its pointer; NULL for 0 or an address in no region. Any thread. */
void* ac_gx_pointer(u32 address);
/* GX's memory for the front end and the emu64 kit's lists (sl_emu64_set_memory): the host bytes at [address,
   address + size) when they lie in one region, and NULL otherwise. user is unused. Any thread. */
const void* ac_gx_memory(void* user, uint32_t address, size_t size);

/* The bytes written outside display lists since start-up. */
u64 ac_gx_bytes(void);
/* The pointers ac_gx_address and ac_gx_tlut_address could not map since start-up. */
u64 ac_gx_unmapped(void);

/* GXFlush's, in place of the console's sync of the write gather pipe: hands the bytes written outside a display
   list since the last hand-over to the GX front end, created at the first. */
void ac_gx_flush(void);

/* JW_BeginFrame's, after FrameDrawing is set: begins the frame (sl_frame_begin). A NULL frame is counted in
   refused_frames, and its records are skipped while its GX commands are still decoded. A begin while a frame is
   open is counted in nested and ignored. */
void ac_frame_begin(void);
/* JW_EndFrame's, after endRender: hands the sink's bytes to the front end, records them into the frame after what
   it holds, and submits it, then calls ac_emu64_frame_submitted and, for a frame submitted, ac_measure_frame. An end
   with no frame open is counted in nested and does nothing. */
void ac_frame_end(void);
/* emu64_taskstart's, and J2D's seam's before each draw (port/jsys_j2d.h): hands the sink's bytes to the front end
   and records them into the open frame, then gives it in *frame for the task or the draw to record into. FALSE,
   with *frame NULL, when the frame was refused, or when no frame is open, counted in outside. */
BOOL ac_frame_task(struct sl_frame** frame);

/* The differential test's (port/NOTES.md, --diff; sonarlib docs/roadmap.md, M6 step 6j), around each emu64 task,
   after ac_frame_task. With begin FALSE, submits the open frame early and leaves the glue's frame open with no frame,
   so records go nowhere; with TRUE, begins a new one (sl_frame_begin), whose next record is its first. A refusal
   counts in refused_frames. Does nothing when no frame is open. Neither calls ac_emu64_frame_submitted. */
void ac_frame_split(BOOL begin);
/* What the reference's front end (port/adapter/gx_reference.c, in ac_port_reference) takes of the glue's. Its
   begin and end hand over through ac_gx_flush, and the hand-over gives it the bytes (port/reference.h,
   ac_reference_gx). */
/* The front end the sink's bytes go to; NULL before the first hand-over, or when it could not be made. */
struct sl_gxf* ac_gx_front_end(void);
/* A front end on GX's address space; NULL when out of memory. */
struct sl_gxf* ac_gx_create_front_end(void);
/* Counts an error in errors, and logs the first, at offset bytes into the stream written outside display lists. */
void ac_gx_error(u64 offset, const char* what, int detail);
/* Counts a record the front end refused, and logs the first. */
void ac_gx_record_refused(sl_result result);

/* The glue's counts since start-up. */
typedef struct ac_gx_counts {
    u64 errors;         /* hand-overs the front end failed, and bytes dropped; the first is logged */
    u64 unmapped;       /* as ac_gx_unmapped */
    u64 outside;        /* emu64 tasks with no frame open */
    u64 nested;         /* frame begins inside a frame, and ends outside one */
    u64 refused_frames; /* frame begins sl_frame_begin gave no frame */
} ac_gx_counts;
void ac_gx_get_counts(ac_gx_counts* counts);

/* The FIFO's glue (dolphin_gx.c) for GXDisplayList.c:GXEndDisplayList's overflow, in place of the console's
   __piReg[5] bit 26, set when a write ran past the list's end. Reading it clears it. */
BOOL ac_gx_take_overflow(void);

#ifdef __cplusplus
}
#endif

#endif

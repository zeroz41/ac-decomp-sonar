/* The sink the SDK's GX units write their command bytes into, in place of the console's write gather pipe
   (port/adapter/dolphin_gx.c). Outside a display list the bytes are counted and discarded at each GXFlush; between
   GXBeginDisplayList and GXEndDisplayList they go into the list's buffer. Game thread. */
#ifndef AC_PORT_GX_SINK_H
#define AC_PORT_GX_SINK_H

#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Each appends its value's bytes, big-endian, as the write gather pipe did. */
void ac_gx_write_u8(u8 value);
void ac_gx_write_u16(u16 value);
void ac_gx_write_u32(u32 value);
void ac_gx_write_f32(f32 value);

/* A pointer the GPU read, as the 32-bit value a unit writes: its offset from main memory's base when it lies in
   main memory, as OSCachedToPhysical gives, and otherwise 0, counted as unmapped. */
u32 ac_gx_address(const void* pointer);
/* An offset in main memory back to its pointer; NULL for 0. */
void* ac_gx_pointer(u32 address);

/* The bytes written outside display lists since start-up. */
u64 ac_gx_bytes(void);
/* The pointers ac_gx_address could not map since start-up. */
u64 ac_gx_unmapped(void);

/* GXFlush's, in place of the console's sync of the write gather pipe: discards the bytes written outside a display
   list since the last flush. */
void ac_gx_flush(void);

/* The FIFO's glue (dolphin_gx.c) for GXDisplayList.c:GXEndDisplayList's overflow, in place of the console's
   __piReg[5] bit 26, set when a write ran past the list's end. Reading it clears it. */
BOOL ac_gx_take_overflow(void);

#ifdef __cplusplus
}
#endif

#endif

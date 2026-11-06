/* The sink the SDK's GX units write their command bytes into (port/gx_sink.h), and the FIFO functions of GXFifo.c,
   which the port does not compile: the console's FIFO was hardware, the write gather pipe pouring into a ring the
   GPU read. Here the CPU FIFO is either linked to the GPU's, as GXInit leaves it, and its bytes are counted and
   discarded at each GXFlush, or it is a display list's, set by GXBeginDisplayList, and its bytes go into the list.
   Step 6h hands the linked FIFO's bytes to the GX front end. */
#include "dolphin/gx.h"
#include "dolphin/os.h"
#include "gx/__gx.h"

#include <port/gx_sink.h>

#include <sonar/dolphin_os/os.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static struct __GXFifoObj* cpu_fifo;
static struct __GXFifoObj* gp_fifo;
static OSThread* gx_thread;
static u64 bytes_total;
static u64 bytes_pending;
static u64 unmapped;
static BOOL overflow;

/* A CPU FIFO that is not the GPU's is a display list: the console's FIFO was then not linked, and the write gather
   pipe wrote into the list's memory at its write pointer. */
static void put(const u8* bytes, u32 size) {
    if (cpu_fifo != NULL && cpu_fifo != gp_fifo) {
        u8* const write = (u8*)cpu_fifo->wrPtr;
        u8* const end = cpu_fifo->base + cpu_fifo->size;
        if (write < cpu_fifo->base || write > end || (size_t)(end - write) < size) {
            overflow = TRUE;
            return;
        }
        memcpy(write, bytes, size);
        cpu_fifo->wrPtr = write + size;
        return;
    }
    bytes_total += size;
    bytes_pending += size;
}

void ac_gx_write_u8(u8 value) {
    put(&value, 1);
}

void ac_gx_write_u16(u16 value) {
    const u8 bytes[2] = { (u8)(value >> 8), (u8)value };
    put(bytes, sizeof bytes);
}

void ac_gx_write_u32(u32 value) {
    const u8 bytes[4] = { (u8)(value >> 24), (u8)(value >> 16), (u8)(value >> 8), (u8)value };
    put(bytes, sizeof bytes);
}

void ac_gx_write_f32(f32 value) {
    u32 bits;
    memcpy(&bits, &value, sizeof bits);
    ac_gx_write_u32(bits);
}

u32 ac_gx_address(const void* pointer) {
    const u8* const base = (const u8*)sl_dolphin_os_memory_base();
    const u8* const p = (const u8*)pointer;
    if (base != NULL && p >= base && p < base + sl_dolphin_os_memory_size())
        return (u32)(p - base);
    unmapped++;
    return 0;
}

void* ac_gx_pointer(u32 address) {
    u8* const base = (u8*)sl_dolphin_os_memory_base();
    if (address == 0 || base == NULL)
        return NULL;
    return base + address;
}

u64 ac_gx_bytes(void) {
    return bytes_total;
}

u64 ac_gx_unmapped(void) {
    return unmapped;
}

void ac_gx_flush(void) {
    bytes_pending = 0;
}

BOOL ac_gx_take_overflow(void) {
    const BOOL taken = overflow;
    overflow = FALSE;
    return taken;
}

/* ---- GXFifo.c ---- */

void GXInitFifoBase(GXFifoObj* fifo, void* base, u32 size) {
    struct __GXFifoObj* realFifo = (struct __GXFifoObj*)fifo;
    realFifo->base = (u8*)base;
    realFifo->top = (u8*)base + size - 4;
    realFifo->size = size;
    realFifo->count = 0;
    GXInitFifoLimits(fifo, size - 0x4000, (size >> 1) & ~0x1F);
    GXInitFifoPtrs(fifo, base, base);
}

void GXInitFifoPtrs(GXFifoObj* fifo, void* readPtr, void* writePtr) {
    struct __GXFifoObj* realFifo = (struct __GXFifoObj*)fifo;
    realFifo->rdPtr = readPtr;
    realFifo->wrPtr = writePtr;
    realFifo->count = (s32)((u8*)writePtr - (u8*)readPtr);
    if (realFifo->count < 0)
        realFifo->count += realFifo->size;
}

void GXInitFifoLimits(GXFifoObj* fifo, u32 hiWatermark, u32 loWatermark) {
    struct __GXFifoObj* realFifo = (struct __GXFifoObj*)fifo;
    realFifo->hiWatermark = hiWatermark;
    realFifo->loWatermark = loWatermark;
}

void GXSetCPUFifo(GXFifoObj* fifo) {
    cpu_fifo = (struct __GXFifoObj*)fifo;
}

void GXSetGPFifo(GXFifoObj* fifo) {
    gp_fifo = (struct __GXFifoObj*)fifo;
}

void GXSaveCPUFifo(GXFifoObj* fifo) {
    __GXSaveCPUFifoAux((struct __GXFifoObj*)fifo);
}

/* The console flushed the write gather pipe and read the write pointer back from the PI. The sink writes through
   at once, so the object already holds it: a display list's count is the bytes written into it, and the linked
   FIFO, whose bytes the sink discards, holds none. */
void __GXSaveCPUFifoAux(struct __GXFifoObj* realFifo) {
    if (realFifo != gp_fifo) {
        realFifo->count = (s32)((u8*)realFifo->wrPtr - (u8*)realFifo->rdPtr);
        if (realFifo->count < 0)
            realFifo->count += realFifo->size;
    } else {
        realFifo->count = 0;
    }
}

void GXGetGPStatus(GXBool* overhi, GXBool* underlow, GXBool* readIdle, GXBool* cmdIdle, GXBool* brkpt) {
    *overhi = GX_FALSE;
    *underlow = GX_FALSE;
    *readIdle = GX_TRUE;
    *cmdIdle = GX_TRUE;
    *brkpt = GX_FALSE;
}

OSThread* GXSetCurrentGXThread(void) {
    OSThread* const previous = gx_thread;
    gx_thread = OSGetCurrentThread();
    return previous;
}

OSThread* GXGetCurrentGXThread(void) {
    return gx_thread;
}

GXFifoObj* GXGetCPUFifo(void) {
    return (GXFifoObj*)cpu_fifo;
}

GXFifoObj* GXGetGPFifo(void) {
    return (GXFifoObj*)gp_fifo;
}

void __GXFifoInit(void) {
    gx_thread = OSGetCurrentThread();
    cpu_fifo = NULL;
    gp_fifo = NULL;
}

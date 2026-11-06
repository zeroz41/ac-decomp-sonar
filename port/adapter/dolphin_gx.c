/* The sink the SDK's GX units write their command bytes into (port/gx_sink.h), GX's address space, the frame the GX
   front end and emu64's kit record into, and the FIFO functions of GXFifo.c, which the port does not compile: the
   console's FIFO was hardware, the write gather pipe pouring into a ring the GPU read. Here the CPU FIFO is either
   linked to the GPU's, as GXInit leaves it, and its bytes go to sonarlib's GX front end at each GXFlush and before
   each record, or it is a display list's, set by GXBeginDisplayList, and its bytes go into the list (sonarlib
   docs/roadmap.md, M6 step 6h). */
#include "dolphin/gx.h"
#include "dolphin/os.h"
#include "gx/__gx.h"

#include <port/emu64_glue.h>
#include <port/gx_sink.h>

#include <sonar/base/log.h>
#include <sonar/dolphin_os/os.h>
#include <sonar/gx_frontend.h>
#include <sonar/mem.h>
#include <sonar/os.h>
#include <sonar/render.h>
#include <sonar/sonar.h>

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static struct __GXFifoObj* cpu_fifo;
static struct __GXFifoObj* gp_fifo;
static OSThread* gx_thread;
static u64 bytes_total;
static BOOL overflow;

/* ---- The sink: the bytes written outside display lists since the last hand-over */

static u8* pending;
static size_t pending_size;
static size_t pending_capacity;

static u64 gx_errors;
static u64 unmapped;

/* Counts an error and logs the first, at offset bytes into the stream written outside display lists. */
static void gx_error(u64 offset, const char* what, int detail) {
    if (gx_errors++ == 0)
        SL_ERROR("gx", "the GX front end's first error, at stream byte %" PRIu64 ": %s (%d)", (uint64_t)offset, what,
                 detail);
}

static BOOL pending_grow(size_t size) {
    size_t capacity = pending_capacity != 0 ? pending_capacity : 0x10000;
    u8* grown;
    while (capacity - pending_size < size) {
        if (capacity > SIZE_MAX / 2)
            return FALSE;
        capacity *= 2;
    }
    grown = (u8*)realloc(pending, capacity);
    if (grown == NULL)
        return FALSE;
    pending = grown;
    pending_capacity = capacity;
    return TRUE;
}

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
    if (pending_capacity - pending_size < size && !pending_grow(size)) {
        gx_error(bytes_total - size, "bytes dropped: the sink's buffer could not grow", (int)size);
        return;
    }
    memcpy(pending + pending_size, bytes, size);
    pending_size += size;
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

u64 ac_gx_bytes(void) {
    return bytes_total;
}

/* ---- GX's address space: main memory at 0, then the image's data ranges, then its zero-initialized ranges */

/* The registers that take a memory address as 32-byte blocks hold 24 bits of them. */
#define AC_GX_SPACE_END 0x20000000u
/* The front end masks a TLUT load's address to 25 bits. */
#define AC_GX_TLUT_END 0x02000000u
/* The most image ranges the table maps, data and zero-initialized together. */
#define AC_GX_RANGES 16

typedef struct gx_region {
    const u8* host;
    u32 address;
    u32 size;
} gx_region;

static gx_region regions[1 + AC_GX_RANGES];
static u32 region_count;

/* Maps count of the image's ranges, of the kind named, after the regions mapped so far; next is where the last ends. */
static void map_ranges(const sl_mem_range* ranges, u32 count, const char* kind, u64* next) {
    u32 i;
    for (i = 0; i < count; i++) {
        uintptr_t begin, end;
        u64 size, address;
        if (region_count >= 1 + AC_GX_RANGES) {
            SL_WARN("gx", "GX address space: image %s range %" PRIu32 " not mapped: past the table's %d", kind,
                    (uint32_t)i, AC_GX_RANGES);
            continue;
        }
        begin = (uintptr_t)ranges[i].begin;
        end = (uintptr_t)ranges[i].end;
        size = (u64)(end - begin);
        /* At the end of the region before, rounded up to 32, plus the range's offset within 32 bytes. */
        address = ((*next + 31u) & ~(u64)31u) + (begin & 31u);
        if (address + size > AC_GX_SPACE_END) {
            SL_WARN("gx",
                    "GX address space: image %s range %" PRIu32 ", 0x%" PRIx64 " bytes, not mapped: it would end "
                    "past 0x%08x",
                    kind, (uint32_t)i, (uint64_t)size, AC_GX_SPACE_END);
            continue;
        }
        regions[region_count].host = (const u8*)ranges[i].begin;
        regions[region_count].address = (u32)address;
        regions[region_count].size = (u32)size;
        region_count++;
        *next = address + size;
        SL_INFO("gx", "GX address space: image %s range %" PRIu32 ", 0x%08" PRIx64 " bytes at 0x%08" PRIx64, kind,
                (uint32_t)i, (uint64_t)size, (uint64_t)address);
    }
}

void ac_gx_map_memory(void) {
    sl_mem_range ranges[AC_GX_RANGES];
    u32 count;
    u64 next;
    if (region_count != 0 || sl_dolphin_os_memory_base() == NULL)
        return;
    regions[0].host = (const u8*)sl_dolphin_os_memory_base();
    regions[0].address = 0;
    regions[0].size = (u32)sl_dolphin_os_memory_size();
    region_count = 1;
    SL_INFO("gx", "GX address space: main memory, 0x%08" PRIx32 " bytes at 0x00000000", (uint32_t)regions[0].size);
    next = regions[0].size;
    /* The ranges past the table's are counted and not written; map_ranges warns for each. */
    count = sl_mem_image_data(ranges, AC_GX_RANGES);
    map_ranges(ranges, count < AC_GX_RANGES ? count : AC_GX_RANGES, "data", &next);
    if (count > AC_GX_RANGES)
        SL_WARN("gx", "GX address space: image data ranges %d to %" PRIu32 " not mapped: past the table's %d",
                AC_GX_RANGES, (uint32_t)count - 1, AC_GX_RANGES);
    /* Zero-initialized statics, such as buffers the game fills at run time and gives GX. */
    count = sl_mem_image_zero_data(ranges, AC_GX_RANGES);
    map_ranges(ranges, count < AC_GX_RANGES ? count : AC_GX_RANGES, "zero-initialized", &next);
    if (count > AC_GX_RANGES)
        SL_WARN("gx",
                "GX address space: image zero-initialized ranges %d to %" PRIu32 " not mapped: past the table's %d",
                AC_GX_RANGES, (uint32_t)count - 1, AC_GX_RANGES);
}

u32 ac_gx_address(const void* pointer) {
    const u8* const p = (const u8*)pointer;
    u32 i;
    if (p == NULL)
        return 0; /* no address, so not counted */
    for (i = 0; i < region_count; i++) {
        if (p >= regions[i].host && p < regions[i].host + regions[i].size)
            return regions[i].address + (u32)(p - regions[i].host);
    }
    unmapped++;
    return 0;
}

BOOL ac_gx_find_address(const void* pointer, u32* address) {
    const u8* const p = (const u8*)pointer;
    u32 i;
    if (p == NULL)
        return FALSE;
    for (i = 0; i < region_count; i++) {
        if (p >= regions[i].host && p < regions[i].host + regions[i].size) {
            *address = regions[i].address + (u32)(p - regions[i].host);
            return TRUE;
        }
    }
    return FALSE;
}

u32 ac_gx_tlut_address(const void* pointer) {
    const u32 address = ac_gx_address(pointer);
    if (address >= AC_GX_TLUT_END) {
        unmapped++;
        return 0;
    }
    return address;
}

/* The region holding [address, address + size), or NULL. */
static const gx_region* region_of(u32 address, size_t size) {
    u32 i;
    for (i = 0; i < region_count; i++) {
        const gx_region* const r = &regions[i];
        if (address >= r->address && address - r->address < r->size && size <= r->size - (address - r->address))
            return r;
    }
    return NULL;
}

void* ac_gx_pointer(u32 address) {
    const gx_region* r;
    if (address == 0)
        return NULL;
    r = region_of(address, 1);
    return r != NULL ? (void*)(r->host + (address - r->address)) : NULL;
}

u64 ac_gx_unmapped(void) {
    return unmapped;
}

/* The front end's memory: the host bytes at [address, address + size) when they lie in one region. */
static const void* gx_memory(void* user, uint32_t address, size_t size) {
    const gx_region* const r = region_of(address, size);
    (void)user;
    return r != NULL ? r->host + (address - r->address) : NULL;
}

/* ---- The front end, and the frame from the game's frame begin to its end */

static sl_gxf* gxf;
static sl_gxf* ref_gxf;   /* the differential test's reference's (ac_gx_reference_begin), made at the first */
static BOOL to_reference; /* hand-overs go to ref_gxf */
static sl_frame* frame;
static BOOL frame_open;
static BOOL frame_recorded; /* the open frame had its sl_gxf_record */
static u64 outside;
static u64 nested;
static u64 refused_frames;
static u64 refused_records;

/* A front end on GX's address space; NULL when out of memory. */
static sl_gxf* create_gxf(void) {
    sl_gxf_desc desc;
    memset(&desc, 0, sizeof desc);
    desc.memory = gx_memory;
    desc.max_vertices = 0;
    return sl_gxf_create(&desc);
}

/* The bytes written since the last hand-over go to the front end, created at the first, or to the reference's from
   ac_gx_reference_begin to ac_gx_reference_end. On a thread that is not a game thread they are dropped. */
static void hand_over(void) {
    const u64 offset = bytes_total - pending_size;
    size_t error_offset = 0;
    sl_gx_dl_result result;
    sl_gxf* target;
    if (sl_os_thread_current().id == 0) {
        if (pending_size != 0)
            gx_error(offset, "bytes dropped: handed over on a thread that is not a game thread", (int)pending_size);
        pending_size = 0;
        return;
    }
    if (gxf == NULL) {
        gxf = create_gxf();
        if (gxf == NULL) {
            gx_error(offset, "bytes dropped: no GX front end, out of memory", (int)pending_size);
            pending_size = 0;
            return;
        }
    }
    target = to_reference ? ref_gxf : gxf;
    if (target == NULL) {
        if (pending_size != 0)
            gx_error(offset, "bytes dropped: no reference GX front end, out of memory", (int)pending_size);
        pending_size = 0;
        return;
    }
    if (pending_size == 0)
        return;
    result = sl_gxf_commands(target, pending, pending_size, &error_offset);
    if (result != SL_GX_DL_OK)
        gx_error(offset + error_offset, "sl_gxf_commands failed", (int)result);
    pending_size = 0;
}

/* The bytes since the last hand-over, recorded into the open frame after what it holds: its first record clears. */
static void record(void) {
    sl_result result;
    hand_over();
    if (frame == NULL || gxf == NULL)
        return;
    result = frame_recorded ? sl_gxf_record_more(gxf, frame) : sl_gxf_record(gxf, frame);
    frame_recorded = TRUE;
    if (result != SL_OK && refused_records++ == 0)
        SL_WARN("gx", "the GX front end's first refused record: %s", sl_result_string(result));
}

void ac_gx_flush(void) {
    hand_over();
}

void ac_frame_begin(void) {
    if (frame_open) {
        nested++;
        return;
    }
    frame_open = TRUE;
    frame_recorded = FALSE;
    frame = sl_frame_begin();
    if (frame == NULL)
        refused_frames++;
}

void ac_frame_end(void) {
    if (!frame_open) {
        nested++;
        return;
    }
    record();
    if (frame != NULL)
        sl_frame_submit(frame);
    frame = NULL;
    frame_open = FALSE;
    ac_emu64_frame_submitted();
}

BOOL ac_frame_task(struct sl_frame** task_frame) {
    *task_frame = NULL;
    if (!frame_open) {
        outside++;
        return FALSE;
    }
    record();
    *task_frame = frame;
    return frame != NULL;
}

void ac_frame_split(BOOL begin) {
    if (!frame_open)
        return;
    if (!begin) {
        if (frame != NULL)
            sl_frame_submit(frame);
        frame = NULL;
        return;
    }
    frame_recorded = FALSE;
    frame = sl_frame_begin();
    if (frame == NULL)
        refused_frames++;
}

/* The SDK's GX state when the reference began, and the main front end's state on its way to the reference's.
   Static: the register and vertex states are large. */
static struct __GXData_struct saved_gx;
static sl_gx_register_state state_registers;
static sl_gx_vertex_state state_vertex;
static uint32_t state_xf[SL_GXF_XF_MEMORY_WORDS];

void ac_gx_reference_begin(void) {
    hand_over();
    if (ref_gxf == NULL)
        ref_gxf = create_gxf();
    if (gxf != NULL && ref_gxf != NULL) {
        sl_gxf_get_state(gxf, &state_registers, &state_vertex, state_xf);
        sl_gxf_set_state(ref_gxf, &state_registers, &state_vertex, state_xf);
    }
    saved_gx = *gx;
    to_reference = TRUE;
}

void ac_gx_reference_end(struct sl_frame* reference_frame) {
    hand_over();
    if (reference_frame != NULL && ref_gxf != NULL) {
        const sl_result result = sl_gxf_record(ref_gxf, reference_frame);
        if (result != SL_OK && refused_records++ == 0)
            SL_WARN("gx", "the GX front end's first refused record: %s", sl_result_string(result));
    }
    *gx = saved_gx;
    to_reference = FALSE;
}

void ac_gx_get_counts(ac_gx_counts* counts) {
    counts->errors = gx_errors;
    counts->unmapped = unmapped;
    counts->outside = outside;
    counts->nested = nested;
    counts->refused_frames = refused_frames;
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
   FIFO, whose bytes the sink hands to the GX front end, holds none. */
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

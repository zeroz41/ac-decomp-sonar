/* The Dolphin SDK's OS functions the game calls, on sonarlib's kits/dolphin_os (sonarlib docs/decisions.md D41): the
   game's objects pass straight through, so their layout here is the kit's. Threads are the exception: the kit knows
   an OSThread by its address only, and the glue keeps the fields game code reads. */
#include "dolphin/os.h"
#include "dolphin/os/OSAlarm.h"
#include "dolphin/base/PPCArch.h"
#include "dolphin/os/OSAlloc.h"
#include "dolphin/os/OSArena.h"
#include "dolphin/os/OSCache.h"
#include "dolphin/os/OSContext.h"
#include "dolphin/os/OSError.h"
#include "dolphin/os/OSInterrupt.h"
#include "dolphin/os/OSLink.h"
#include "dolphin/os/OSMemory.h"
#include "dolphin/os/OSMessage.h"
#include "dolphin/os/OSMutex.h"
#include "dolphin/os/OSReset.h"
#include "dolphin/os/OSRtc.h"
#include "dolphin/os/OSThread.h"
#include "dolphin/os/OSTime.h"
#include "boot.h"

#include <port/gx_sink.h>
#include <port/reference.h>

#include <sonar/base/log.h>
#include <sonar/dolphin_os/os.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SAME_FIELD(game, kit, game_field, kit_field)                                                                  \
    _Static_assert(offsetof(game, game_field) == offsetof(kit, kit_field) &&                                          \
                       sizeof(((game*)0)->game_field) == sizeof(((kit*)0)->kit_field),                                \
                   #game "." #game_field " is not where the kit has it")

_Static_assert(sizeof(OSThreadQueue) == sizeof(sl_dolphin_os_thread_queue), "OSThreadQueue's size");
_Static_assert(sizeof(OSMessageQueue) == sizeof(sl_dolphin_os_message_queue), "OSMessageQueue's size");
SAME_FIELD(OSMessageQueue, sl_dolphin_os_message_queue, queueSend, queue_send);
SAME_FIELD(OSMessageQueue, sl_dolphin_os_message_queue, queueReceive, queue_receive);
SAME_FIELD(OSMessageQueue, sl_dolphin_os_message_queue, msgArray, msg_array);
SAME_FIELD(OSMessageQueue, sl_dolphin_os_message_queue, msgCount, msg_count);
SAME_FIELD(OSMessageQueue, sl_dolphin_os_message_queue, firstIndex, first_index);
SAME_FIELD(OSMessageQueue, sl_dolphin_os_message_queue, usedCount, used_count);
_Static_assert(OS_MESSAGE_BLOCK == SL_DOLPHIN_OS_MESSAGE_BLOCK && OS_MESSAGE_NOBLOCK == SL_DOLPHIN_OS_MESSAGE_NOBLOCK,
               "the message flags");

_Static_assert(sizeof(OSMutex) == sizeof(sl_dolphin_os_mutex), "OSMutex's size");
SAME_FIELD(OSMutex, sl_dolphin_os_mutex, queue, queue);
SAME_FIELD(OSMutex, sl_dolphin_os_mutex, thread, thread);
SAME_FIELD(OSMutex, sl_dolphin_os_mutex, count, count);
SAME_FIELD(OSMutex, sl_dolphin_os_mutex, link, link);
_Static_assert(sizeof(OSCond) == sizeof(sl_dolphin_os_cond), "OSCond's size");
SAME_FIELD(OSCond, sl_dolphin_os_cond, queue, queue);

_Static_assert(sizeof(OSAlarm) == sizeof(sl_dolphin_os_alarm), "OSAlarm's size");
SAME_FIELD(OSAlarm, sl_dolphin_os_alarm, handler, handler);
SAME_FIELD(OSAlarm, sl_dolphin_os_alarm, fire, fire);
SAME_FIELD(OSAlarm, sl_dolphin_os_alarm, prev, prev);
SAME_FIELD(OSAlarm, sl_dolphin_os_alarm, next, next);
SAME_FIELD(OSAlarm, sl_dolphin_os_alarm, period, period);
SAME_FIELD(OSAlarm, sl_dolphin_os_alarm, start, start);

_Static_assert(OS_THREAD_STATE_READY == SL_DOLPHIN_OS_THREAD_READY &&
                   OS_THREAD_STATE_RUNNING == SL_DOLPHIN_OS_THREAD_RUNNING &&
                   OS_THREAD_STATE_WAITING == SL_DOLPHIN_OS_THREAD_WAITING &&
                   OS_THREAD_STATE_MORIBUND == SL_DOLPHIN_OS_THREAD_MORIBUND,
               "the thread states");
_Static_assert(OS_PRIORITY_MIN == SL_DOLPHIN_OS_PRIORITY_MIN && OS_PRIORITY_MAX == SL_DOLPHIN_OS_PRIORITY_MAX,
               "the thread priorities");

#define KIT_QUEUE(queue) ((sl_dolphin_os_message_queue*)(queue))
#define KIT_MUTEX(mutex) ((sl_dolphin_os_mutex*)(mutex))
#define KIT_COND(cond) ((sl_dolphin_os_cond*)(cond))
#define KIT_ALARM(alarm) ((sl_dolphin_os_alarm*)(alarm))
/* The kit calls the game's handler from the alarm's own handler field, as the SDK did; both take two pointers. */
#define KIT_HANDLER(handler) ((sl_dolphin_os_alarm_handler)(void*)(handler))

void OSInitMessageQueue(OSMessageQueue* queue, OSMessage* msgArray, int msgCount) {
    sl_dolphin_os_queue_init(KIT_QUEUE(queue), msgArray, msgCount);
}

BOOL OSSendMessage(OSMessageQueue* queue, OSMessage msg, int flags) {
    return sl_dolphin_os_queue_send(KIT_QUEUE(queue), msg, flags);
}

BOOL OSJamMessage(OSMessageQueue* queue, OSMessage msg, int flags) {
    return sl_dolphin_os_queue_jam(KIT_QUEUE(queue), msg, flags);
}

BOOL OSReceiveMessage(OSMessageQueue* queue, OSMessage* msgPtr, int flags) {
    return sl_dolphin_os_queue_receive(KIT_QUEUE(queue), msgPtr, flags);
}

BOOL OSDisableInterrupts(void) { return sl_dolphin_os_disable_interrupts(); }
BOOL OSEnableInterrupts(void) { return sl_dolphin_os_enable_interrupts(); }
BOOL OSRestoreInterrupts(BOOL level) { return sl_dolphin_os_restore_interrupts(level); }

OSTime OSGetTime(void) { return sl_dolphin_os_get_time(); }
OSTick OSGetTick(void) { return sl_dolphin_os_get_tick(); }

/* ---- Threads: the kit keeps each thread's state, and these keep the fields game code reads, such as
   osreport.c's check of the current thread's state and JKRThread's stack bounds. */

static void sync_thread(OSThread* thread) {
    const s32 priority = sl_dolphin_os_thread_priority(thread);
    thread->state = (u16)sl_dolphin_os_thread_state(thread);
    if (priority >= 0)
        thread->priority = thread->base = priority;
}

void OSInitThreadQueue(OSThreadQueue* queue) { queue->head = queue->tail = NULL; }

OSThread* OSGetCurrentThread(void) {
    OSThread* thread = sl_dolphin_os_thread_current();
    if (thread)
        sync_thread(thread);
    return thread;
}

BOOL OSIsThreadSuspended(OSThread* thread) { return thread->suspend > 0; }
BOOL OSIsThreadTerminated(OSThread* thread) { return sl_dolphin_os_thread_is_terminated(thread); }

/* The thread runs on a stack core os sizes for native code; the game's stack keeps the SDK's bounds and magic word
   for the code that reads them. */
BOOL OSCreateThread(OSThread* thread, void* (*func)(void*), void* param, void* stack, u32 stackSize,
                    OSPriority priority, u16 attr) {
    if (!sl_dolphin_os_thread_create(thread, func, param, priority))
        return FALSE;
    memset(thread, 0, sizeof *thread);
    thread->state = OS_THREAD_STATE_READY;
    thread->attr = attr & OS_THREAD_ATTR_DETACH;
    thread->suspend = 1;
    thread->priority = thread->base = priority;
    thread->val = (void*)-1;
    thread->stackBase = (u8*)stack;
    thread->stackEnd = (u32*)((u8*)stack - stackSize);
    *thread->stackEnd = OS_THREAD_STACK_MAGIC;
    if (thread->attr & OS_THREAD_ATTR_DETACH)
        sl_dolphin_os_thread_detach(thread);
    return TRUE;
}

void OSExitThread(void* val) {
    OSThread* thread = sl_dolphin_os_thread_current();
    if (thread) {
        thread->val = val;
        thread->state = (thread->attr & OS_THREAD_ATTR_DETACH) ? 0 : OS_THREAD_STATE_MORIBUND;
    }
    sl_dolphin_os_thread_exit(val);
}

void OSCancelThread(OSThread* thread) {
    sl_dolphin_os_thread_cancel(thread);
    sync_thread(thread);
}

BOOL OSJoinThread(OSThread* thread, void** val) {
    const BOOL joined = sl_dolphin_os_thread_join(thread, val);
    if (joined)
        thread->state = 0;
    return joined;
}

void OSDetachThread(OSThread* thread) {
    thread->attr |= OS_THREAD_ATTR_DETACH;
    sl_dolphin_os_thread_detach(thread);
    sync_thread(thread);
}

s32 OSResumeThread(OSThread* thread) {
    const s32 previous = sl_dolphin_os_thread_resume(thread);
    if (previous >= 0)
        thread->suspend = previous > 0 ? previous - 1 : 0;
    sync_thread(thread);
    return previous;
}

s32 OSSuspendThread(OSThread* thread) {
    const s32 previous = sl_dolphin_os_thread_suspend(thread);
    if (previous >= 0)
        thread->suspend = previous + 1;
    sync_thread(thread);
    return previous;
}

BOOL OSSetThreadPriority(OSThread* thread, OSPriority priority) {
    const BOOL set = sl_dolphin_os_thread_set_priority(thread, priority);
    sync_thread(thread);
    return set;
}

OSPriority OSGetThreadPriority(OSThread* thread) { return thread->base; }

void OSYieldThread(void) { sl_dolphin_os_thread_yield(); }

/* The SDK's scheduler lock, which only JUTException's error handler releases on its way out (below). Core os
   schedules the threads, so the glue keeps only the count the SDK returned. */
static s32 scheduler_lock;

s32 OSDisableScheduler(void) { return scheduler_lock++; }
s32 OSEnableScheduler(void) { return scheduler_lock--; }
s32 OSCheckActiveThreads(void) { return sl_dolphin_os_thread_count(); }

void OSSleepThread(OSThreadQueue* queue) { sl_dolphin_os_thread_sleep((sl_dolphin_os_thread_queue*)queue); }
void OSWakeupThread(OSThreadQueue* queue) { sl_dolphin_os_thread_wakeup((sl_dolphin_os_thread_queue*)queue); }

/* ---- Mutexes and conditions */

void OSInitMutex(OSMutex* mutex) { sl_dolphin_os_mutex_init(KIT_MUTEX(mutex)); }
void OSLockMutex(OSMutex* mutex) { sl_dolphin_os_mutex_lock(KIT_MUTEX(mutex)); }
void OSUnlockMutex(OSMutex* mutex) { sl_dolphin_os_mutex_unlock(KIT_MUTEX(mutex)); }
BOOL OSTryLockMutex(OSMutex* mutex) { return sl_dolphin_os_mutex_try_lock(KIT_MUTEX(mutex)); }
void OSInitCond(OSCond* cond) { sl_dolphin_os_cond_init(KIT_COND(cond)); }
void OSWaitCond(OSCond* cond, OSMutex* mutex) { sl_dolphin_os_cond_wait(KIT_COND(cond), KIT_MUTEX(mutex)); }
void OSSignalCond(OSCond* cond) { sl_dolphin_os_cond_signal(KIT_COND(cond)); }

/* ---- Alarms */

/* The kit's alarms need no set-up. */
void OSInitAlarm(void) {}

void OSCreateAlarm(OSAlarm* alarm) { sl_dolphin_os_alarm_create(KIT_ALARM(alarm)); }

void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler) {
    sl_dolphin_os_alarm_set(KIT_ALARM(alarm), tick, KIT_HANDLER(handler));
}

void OSSetAbsAlarm(OSAlarm* alarm, OSTime time, OSAlarmHandler handler) {
    sl_dolphin_os_alarm_set_abs(KIT_ALARM(alarm), time, KIT_HANDLER(handler));
}

void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler) {
    sl_dolphin_os_alarm_set_periodic(KIT_ALARM(alarm), start, period, KIT_HANDLER(handler));
}

void OSCancelAlarm(OSAlarm* alarm) { sl_dolphin_os_alarm_cancel(KIT_ALARM(alarm)); }

/* ---- Settings */

_Static_assert(OS_SOUND_MODE_MONO == SL_DOLPHIN_OS_SOUND_MONO && OS_SOUND_MODE_STEREO == SL_DOLPHIN_OS_SOUND_STEREO,
               "the SDK's sound modes");
_Static_assert(OS_FONT_ENCODE_ANSI == SL_DOLPHIN_OS_FONT_ANSI && OS_FONT_ENCODE_SJIS == SL_DOLPHIN_OS_FONT_SJIS,
               "the SDK's font encodings");

u32 OSGetSoundMode(void) { return sl_dolphin_os_sound_mode(); }
void OSSetSoundMode(u32 mode) { sl_dolphin_os_set_sound_mode(mode); }
u32 OSGetProgressiveMode(void) { return sl_dolphin_os_progressive_mode(); }
void OSSetProgressiveMode(u32 on) { sl_dolphin_os_set_progressive_mode(on); }
u16 OSGetFontEncode(void) { return sl_dolphin_os_font_encode(); }

/* A retail console: the game checks only the development bit, which turns on its debug modes. */
u32 OSGetConsoleType(void) { return OS_CONSOLE_RETAIL1; }

/* ---- Reset */

_Static_assert(OS_RESET_RESTART == SL_DOLPHIN_OS_RESET_RESTART && OS_RESET_HOTRESET == SL_DOLPHIN_OS_RESET_HOTRESET &&
                   OS_RESET_SHUTDOWN == SL_DOLPHIN_OS_RESET_SHUTDOWN &&
                   OS_RESETCODE_RESTART == SL_DOLPHIN_OS_RESETCODE_RESTART,
               "the SDK's resets");

u8 ac_nmi_save_area[0x40];

/* The reset button stays released until the port's input reaches it. */
BOOL OSGetResetSwitchState(void) { return FALSE; }

u32 OSGetResetCode(void) { return sl_dolphin_os_reset_code(); }

/* The memory kept across a restart, which the game reports at boot: the port's NMISaveArea. */
void OSGetSaveRegion(void** start, void** end) {
    *start = ac_nmi_save_area;
    *end = ac_nmi_save_area + sizeof ac_nmi_save_area;
}

/* The console kept NMISaveArea across a restart, and the game copies it back at boot. The console never returned
   from a restart or hot reset, so neither does the port: one that cannot reset stops. The menu is the console's and
   is not forced. */
void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu) {
    sl_result result;
    (void)forceMenu;
    result = sl_dolphin_os_reset_system(reset, resetCode, ac_nmi_save_area, sizeof ac_nmi_save_area);
    SL_ERROR("dolphin_os", "OSResetSystem(%d, 0x%08x) failed: %d", reset, (unsigned)resetCode, (int)result);
    abort();
}

/* ---- Main memory: the kit's, whose first byte stands for physical address 0 */

/* The console's 24 MB, scaled for 64-bit structures (sonarlib docs/platform.md, Memory). */
#define AC_MAIN_MEMORY_SIZE (4u * 24u * 1024u * 1024u)
/* The emu64 glue keys an actor by its offset here, below its static actors' keys from 2^27 (port/emu64_glue.h). */
_Static_assert(AC_MAIN_MEMORY_SIZE <= 1u << 27, "main memory's offsets stay below the emu64 glue's table keys");

/* The host has no uncached mirror, so both views are the same memory. */
void* OSPhysicalToCached(u32 paddr) { return (u8*)sl_dolphin_os_memory_base() + paddr; }
void* OSPhysicalToUncached(u32 paddr) { return OSPhysicalToCached(paddr); }
u32 OSCachedToPhysical(void* caddr) { return (u32)((u8*)caddr - (u8*)sl_dolphin_os_memory_base()); }
u32 OSUncachedToPhysical(void* ucaddr) { return OSCachedToPhysical(ucaddr); }
void* OSCachedToUncached(void* caddr) { return caddr; }
void* OSUncachedToCached(void* ucaddr) { return ucaddr; }

void* OSGetArenaLo(void) { return sl_dolphin_os_arena_lo(); }
void* OSGetArenaHi(void) { return sl_dolphin_os_arena_hi(); }
void OSSetArenaLo(void* lo) { sl_dolphin_os_set_arena_lo(lo); }
void OSSetArenaHi(void* hi) { sl_dolphin_os_set_arena_hi(hi); }

/* The SDK's allocator kept its heap descriptors at the start of the arena it was given. The game creates no OS heap:
   JKRHeap only reserves the descriptors, and jaudio's OSAlloc2 allocates from its own heap once it has one. So the
   glue reserves the console's 12 bytes per descriptor, and the arena after it starts where the console's did, but
   keeps no heaps. */
volatile OSHeapHandle __OSCurrHeap = -1;

void* OSInitAlloc(void* arenaStart, void* arenaEnd, int maxHeaps) {
    uaddr start = (uaddr)arenaStart + (uaddr)maxHeaps * 12u;
    (void)arenaEnd;
    __OSCurrHeap = -1;
    return (void*)((start + 31u) & ~(uaddr)31u);
}

void* OSAllocFromHeap(int heap, u32 size) {
    SL_ERROR("dolphin_os", "OSAllocFromHeap(%d, %u): the port keeps no OS heaps", heap, (unsigned)size);
    return NULL;
}

/* The console's OSInit set up the arena and the OS. The port's OS is sonarlib's, so this creates main memory, with
   the arena past low memory, and restores the memory the console kept across a restart. Calling it again does
   nothing, as the SDK's guarded. */
void OSInit(void) {
    static BOOL initialized;
    sl_result result;
    if (initialized)
        return;
    initialized = TRUE;
    result = sl_dolphin_os_memory_create(AC_MAIN_MEMORY_SIZE, NULL);
    if (result != SL_OK && result != SL_ERROR_ALREADY_INITIALIZED) {
        SL_ERROR("dolphin_os", "OSInit: no main memory: %d", (int)result);
        abort();
    }
    /* The reference's buffers, under --diff and --reference, from the bottom of the arena before anything else
       allocates (port/reference.h). */
    ac_reference_boot();
    /* GX's address space, main memory's and the image's data ranges, before any other thread runs. */
    ac_gx_map_memory();
    /* Boot's main thread, the game thread "main" the program started on core os, becomes the SDK's default thread
       at its main priority. Its stack is the host's, so stackBase and stackEnd stay 0. */
    {
        static OSThread boot_thread;
        if (!sl_dolphin_os_thread_bind_current(&boot_thread, 16)) {
            SL_ERROR("dolphin_os", "OSInit: the calling thread cannot be bound");
            abort();
        }
        boot_thread.priority = boot_thread.base = 16;
        boot_thread.val = (void*)-1;
        sync_thread(&boot_thread);
    }
    sl_os_boot_reset(NULL, ac_nmi_save_area, sizeof ac_nmi_save_area, NULL);
}

/* ---- CPU errors: the console's exceptions called these handlers, and JUTException installs one to show its crash
   screen. A fault on the host goes to sonarlib's crash report instead, so the glue keeps the table and never calls
   it, and the calls the handler makes are left with nothing to do. */

static OSErrorHandler error_handlers[OS_ERROR_MAX];

OSErrorHandler OSSetErrorHandler(OSError error, OSErrorHandler handler) {
    OSErrorHandler previous;
    if (error >= OS_ERROR_MAX)
        return NULL;
    previous = error_handlers[error];
    error_handlers[error] = handler;
    return previous;
}

void OSFillFPUContext(OSContext* context) { (void)context; }

/* The console's memory protection unit, which the host does not have. */
void OSProtectRange(u32 chan, void* addr, u32 nBytes, u32 control) {
    (void)chan;
    (void)addr;
    (void)nBytes;
    (void)control;
}

/* ---- Interrupts: the console's devices raised them and the SDK dispatched them to these handlers. The glue keeps
   the table and the mask for the devices' kits; jaudio's DSP handler is the one the game installs. */

static __OSInterruptHandler interrupt_handlers[__OS_INTERRUPT_MAX];
static u32 interrupt_mask;

__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler) {
    __OSInterruptHandler previous;
    if (interrupt < 0 || interrupt >= __OS_INTERRUPT_MAX)
        return NULL;
    previous = interrupt_handlers[interrupt];
    interrupt_handlers[interrupt] = handler;
    return previous;
}

__OSInterruptHandler __OSGetInterruptHandler(__OSInterrupt interrupt) {
    if (interrupt < 0 || interrupt >= __OS_INTERRUPT_MAX)
        return NULL;
    return interrupt_handlers[interrupt];
}

/* The SDK's masks set a bit to block an interrupt and return the mask before. */
u32 __OSMaskInterrupts(u32 mask) {
    const u32 previous = interrupt_mask;
    interrupt_mask |= mask;
    return previous;
}

u32 __OSUnmaskInterrupts(u32 mask) {
    const u32 previous = interrupt_mask;
    interrupt_mask &= ~mask;
    return previous;
}

/* An interrupt handler ran on a context of its own, which the host's threads have no counterpart for. */
void OSClearContext(OSContext* context) { (void)context; }
void OSSetCurrentContext(OSContext* context) { (void)context; }

/* ---- Caches: the host's memory is coherent, so storing, flushing, invalidating and touching a range have nothing
   to do, and the locked cache is never enabled. DCZeroRange is dcbz on each 32-byte block the range touches, which
   the game relies on to clear memory. */

void DCInvalidateRange(void* addr, u32 nBytes) {
    (void)addr;
    (void)nBytes;
}
void DCFlushRange(void* addr, u32 nBytes) {
    (void)addr;
    (void)nBytes;
}
void DCStoreRange(void* addr, u32 nBytes) {
    (void)addr;
    (void)nBytes;
}
void DCFlushRangeNoSync(void* addr, u32 nBytes) {
    (void)addr;
    (void)nBytes;
}
void DCStoreRangeNoSync(void* addr, u32 nBytes) {
    (void)addr;
    (void)nBytes;
}
void DCTouchRange(void* addr, u32 nBytes) {
    (void)addr;
    (void)nBytes;
}
void ICInvalidateRange(void* addr, u32 nBytes) {
    (void)addr;
    (void)nBytes;
}
void LCDisable(void) {}

void DCZeroRange(void* addr, u32 nBytes) {
    uintptr_t first;
    uintptr_t end;
    if (nBytes == 0)
        return;
    first = (uintptr_t)addr & ~(uintptr_t)31;
    end = ((uintptr_t)addr + nBytes + 31) & ~(uintptr_t)31;
    memset((void*)first, 0, end - first);
}

/* ---- The processor's state: JUTException reads and writes the machine state register to mask the floating-point
   exceptions, which the host's floating point does not raise, so the glue keeps the value it was given. */

static u32 machine_state;

u32 PPCMfmsr(void) { return machine_state; }
void PPCMtmsr(u32 value) { machine_state = value; }
void PPCSync(void) {}

/* ---- Modules: the console linked foresta.rel at run time, and the port links it in (boot.c's LoadLink). The
   string table named a module's unresolved symbols for the linker's errors, which the port has none of. */

void OSSetStringTable(const void* string_table) { (void)string_table; }

/* ---- The boot mode, which the console's SRAM kept and its boot read: osreport's OSChangeBootMode sets it for the
   next boot, and the port boots one way. */

void __OSSetBootMode(u8 mode) { (void)mode; }
BOOL __OSSyncSram(void) { return TRUE; }

/* The Dolphin SDK's OS functions the game calls, on sonarlib's kits/dolphin_os (sonarlib docs/decisions.md D41): the
   game's objects pass straight through, so their layout here is the kit's. Threads are the exception: the kit knows
   an OSThread by its address only, and the glue keeps the fields game code reads. */
#include "dolphin/os.h"
#include "dolphin/os/OSAlarm.h"
#include "dolphin/os/OSMessage.h"
#include "dolphin/os/OSMutex.h"
#include "dolphin/os/OSReset.h"
#include "dolphin/os/OSRtc.h"
#include "dolphin/os/OSThread.h"
#include "dolphin/os/OSTime.h"
#include "boot.h"

#include <sonar/base/log.h>
#include <sonar/dolphin_os/os.h>

#include <stddef.h>
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

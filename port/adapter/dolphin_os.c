/* The Dolphin SDK's OS functions the game calls, on sonarlib's kits/dolphin_os (sonarlib docs/decisions.md D41): the
   game's objects pass straight through, so their layout here is the kit's. */
#include "dolphin/os.h"
#include "dolphin/os/OSMessage.h"
#include "dolphin/os/OSTime.h"

#include <sonar/dolphin_os/os.h>

#include <stddef.h>

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

#define KIT_QUEUE(queue) ((sl_dolphin_os_message_queue*)(queue))

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

/* The Dolphin SDK's ARAM functions the game calls: the console's 16 MB of auxiliary memory as host memory, and its
   DMA queue (ARQ) on an interrupt source of sonarlib's core os. A request's data arrives, and its callback runs, in
   interrupt context after ARQPostRequest returns, as the console's DMA interrupt delivered them. Addresses in ARAM
   stay the console's offsets; main memory is the host's pointers. */
#include "dolphin/ar.h"
#include "dolphin/os.h"

#include <sonar/os.h>

#include <string.h>

#define ARAM_SIZE 0x1000000u

static u8 aram[ARAM_SIZE];

/* ---- Allocation: a stack from the user area, as ARInit and ARAlloc kept it ---- */

static BOOL ar_initialized;
static u32 stack_pointer;
static u32* block_length;
static u32 free_blocks;

u32 ARInit(u32* stack_index_addr, u32 num_entries) {
    if (!ar_initialized) {
        stack_pointer = __AR_ARAM_USR_BASE_ADDR;
        block_length = stack_index_addr;
        free_blocks = num_entries;
        ar_initialized = TRUE;
    }
    return __AR_ARAM_USR_BASE_ADDR;
}

u32 ARAlloc(u32 length) {
    const int disabled = sl_os_irq_disable();
    const u32 address = stack_pointer;
    if (length > ARAM_SIZE - stack_pointer || free_blocks == 0)
        OSReport("ARAlloc(): out of ARAM for 0x%x bytes\n", (unsigned)length);
    stack_pointer += length;
    if (free_blocks != 0) {
        *block_length++ = length;
        free_blocks--;
    }
    sl_os_irq_restore(disabled);
    return address;
}

u32 ARGetBaseAddress(void) { return __AR_ARAM_USR_BASE_ADDR; }

u32 ARGetSize(void) { return ARAM_SIZE; }

/* ---- The DMA queue ---- */

static sl_os_irq dma_irq;
/* Requests waiting, by priority: the high queue goes first, as the console's did between its low chunks. */
static ARQRequest* queue_head[2];
static ARQRequest* queue_tail[2];

static ARQRequest* pop_request(void) {
    ARQRequest* request = NULL;
    const int disabled = sl_os_irq_disable();
    for (int priority = ARQ_PRIORITY_HIGH; priority >= ARQ_PRIORITY_LOW && request == NULL; --priority) {
        request = queue_head[priority];
        if (request != NULL) {
            queue_head[priority] = request->next;
            if (queue_head[priority] == NULL)
                queue_tail[priority] = NULL;
        }
    }
    sl_os_irq_restore(disabled);
    return request;
}

static void transfer(const ARQRequest* request) {
    const u32 aram_address = (u32)(request->type == ARQ_TYPE_MRAM_TO_ARAM ? request->dest : request->source);
    if (aram_address > ARAM_SIZE || request->length > ARAM_SIZE - aram_address) {
        OSReport("ARQ: 0x%x bytes at ARAM 0x%x are outside it\n", (unsigned)request->length, (unsigned)aram_address);
        return;
    }
    if (request->type == ARQ_TYPE_MRAM_TO_ARAM)
        memcpy(aram + aram_address, (const void*)request->source, request->length);
    else
        memcpy((void*)request->dest, aram + aram_address, request->length);
}

/* The DMA's interrupt: every request waiting, in order. */
static void dma_done(void* user) {
    ARQRequest* request;
    (void)user;
    while ((request = pop_request()) != NULL) {
        transfer(request);
        if (request->callback != NULL)
            request->callback((uaddr)request);
    }
}

void ARQInit(void) {
    sl_os_irq_desc desc;
    if (dma_irq.id != 0)
        return;
    desc.fn = dma_done;
    desc.user = NULL;
    desc.name = "ARAM DMA";
    dma_irq = sl_os_irq_create(&desc);
}

void ARQPostRequest(ARQRequest* task, uaddr owner, u32 type, u32 priority, uaddr source, uaddr dest, u32 length,
                    ARQCallback callback) {
    int disabled;
    const u32 queue = priority == ARQ_PRIORITY_LOW ? ARQ_PRIORITY_LOW : ARQ_PRIORITY_HIGH;
    task->next = NULL;
    task->owner = owner;
    task->type = type;
    task->priority = priority;
    task->source = source;
    task->dest = dest;
    task->length = length;
    task->callback = callback;
    disabled = sl_os_irq_disable();
    if (queue_tail[queue] != NULL)
        queue_tail[queue]->next = task;
    else
        queue_head[queue] = task;
    queue_tail[queue] = task;
    sl_os_irq_restore(disabled);
    sl_os_irq_raise(dma_irq);
}

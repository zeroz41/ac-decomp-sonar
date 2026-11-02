/* Checks the port's ARAM functions (adapter/dolphin_aram.c): the allocation stack, and that the DMA queue moves the
   data and runs each callback in interrupt context after ARQPostRequest returns, high priority first. */
#include "dolphin/ar.h"

#include <sonar/os.h>

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                                                                   \
    if (!(cond)) {                                                                                                    \
        printf("%s:%d: %s\n", __FILE__, __LINE__, #cond);                                                             \
        failures++;                                                                                                   \
    }

void OSReport(const char* fmt, ...) { (void)fmt; }

static uint64_t now_ns(void* user) {
    (void)user;
    return 0;
}

static ARQRequest* done[8];
static int done_count;
static int done_in_interrupt;

static void request_done(uaddr request) {
    if (done_count < 8)
        done[done_count] = (ARQRequest*)request;
    done_count++;
    done_in_interrupt += sl_os_irq_active() != 0;
}

int main(void) {
    sl_os_desc os = {0};
    sl_os_thread_desc thread = {0};
    sl_os_thread self;
    u32 stack[4] = {0};
    ARQRequest low, high, back, quiet;
    static u8 source[0x40], source2[0x20], readback[0x60];

    os.deterministic = 1;
    os.time_fn = now_ns;
    thread.name = "test-main";
    if (sl_os_start(&os) != SL_OK || sl_os_thread_attach(&thread, &self) != SL_OK) {
        printf("cannot start core os\n");
        return 2;
    }

    CHECK(ARInit(stack, 4) == 0x4000);
    CHECK(ARInit(stack, 4) == 0x4000);
    CHECK(ARGetBaseAddress() == 0x4000);
    CHECK(ARGetSize() == 0x1000000);
    CHECK(ARAlloc(0x40) == 0x4000);
    CHECK(ARAlloc(0x20) == 0x4040);
    CHECK(stack[0] == 0x40 && stack[1] == 0x20 && stack[2] == 0);
    ARQInit();
    ARQInit();

    for (int i = 0; i < 0x40; ++i)
        source[i] = (u8)i;
    for (int i = 0; i < 0x20; ++i)
        source2[i] = (u8)(0x80 + i);

    /* Nothing completes before the interrupt, and the high-priority request goes first. */
    ARQPostRequest(&low, 7, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_LOW, (uaddr)source, 0x4000, 0x40, request_done);
    ARQPostRequest(&high, 9, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH, (uaddr)source2, 0x4040, 0x20, request_done);
    CHECK(done_count == 0);
    CHECK(low.owner == 7 && low.length == 0x40 && high.priority == ARQ_PRIORITY_HIGH);
    CHECK(sl_os_irq_dispatch() == SL_OK);
    CHECK(done_count == 2);
    CHECK(done[0] == &high && done[1] == &low);
    CHECK(done_in_interrupt == 2);

    /* Back to main memory, and a request with no callback. */
    memset(readback, 0, sizeof readback);
    ARQPostRequest(&quiet, 0, ARQ_TYPE_ARAM_TO_MRAM, ARQ_PRIORITY_LOW, 0x4040, (uaddr)(readback + 0x40), 0x20, NULL);
    ARQPostRequest(&back, 0, ARQ_TYPE_ARAM_TO_MRAM, ARQ_PRIORITY_LOW, 0x4000, (uaddr)readback, 0x40, request_done);
    CHECK(readback[1] == 0);
    CHECK(sl_os_irq_dispatch() == SL_OK);
    CHECK(done_count == 3 && done[2] == &back);
    CHECK(memcmp(readback, source, 0x40) == 0);
    CHECK(memcmp(readback + 0x40, source2, 0x20) == 0);

    /* A request outside ARAM moves nothing but still completes. */
    memset(readback, 0xEE, sizeof readback);
    ARQPostRequest(&back, 0, ARQ_TYPE_ARAM_TO_MRAM, ARQ_PRIORITY_HIGH, 0xFFFFF0, (uaddr)readback, 0x20, request_done);
    CHECK(sl_os_irq_dispatch() == SL_OK);
    CHECK(done_count == 4 && readback[0] == 0xEE);

    sl_os_thread_exit(NULL);
    sl_os_stop();
    if (failures == 0)
        printf("ok\n");
    return failures != 0;
}

/* Checks the port's audio interface and DSP (adapter/dolphin_ai.c, dolphin_dsp.c). The DSP: jaudio's own boot and
   commands (dspboot.c, dspproc.c) get their answers, and each subframe raises the DSP interrupt once, in interrupt
   context, before DSPReleaseHalt returns. The interface: on a deterministic os, each audio frame takes the block
   AIInitDMA set last and runs the game's callback in interrupt context, and the mix plays its right and left
   samples, the block over again while no other is set, and silence before AIStartDMA. */
#include "ac_port.h"

#include "dolphin/ai.h"
#include "dolphin/dsp.h"
#include "dolphin/os.h"
#include "jaudio_NES/dspboot.h"
#include "jaudio_NES/dspproc.h"
#include "port/dsp_mail.h"

#include <sonar/audio.h>
#include <sonar/os.h>

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                                                                   \
    if (!(cond)) {                                                                                                    \
        printf("%s:%d: %s\n", __FILE__, __LINE__, #cond);                                                             \
        failures++;                                                                                                   \
    }

/* ---- What the SDK's OS gave dspboot.c and the adapters ---- */

static int reports;

void OSReport(const char* fmt, ...) {
    (void)fmt;
    reports++;
}

void OSPanic(const char* file, int line, const char* fmt, ...) {
    printf("OSPanic at %s:%d: %s\n", file, line, fmt);
    failures++;
}

BOOL OSDisableInterrupts(void) { return sl_os_irq_disable(); }
BOOL OSRestoreInterrupts(BOOL level) {
    sl_os_irq_restore(level);
    return level;
}
u32 __OSUnmaskInterrupts(u32 mask) {
    (void)mask;
    return 0;
}

static int dsp_interrupts;
static int dsp_interrupts_outside;

static void dsp_handler(__OSInterrupt interrupt, OSContext* context) {
    (void)context;
    dsp_interrupts++;
    dsp_interrupts_outside += interrupt != OS_INTR_DSP_DSP || !sl_os_irq_active();
}

static __OSInterruptHandler installed;

__OSInterruptHandler __OSGetInterruptHandler(__OSInterrupt interrupt) {
    return interrupt == OS_INTR_DSP_DSP ? installed : NULL;
}

/* ---- Time ---- */

static uint64_t now;

static uint64_t now_ns(void* user) {
    (void)user;
    return now;
}

/* One block of the interface: 560 samples at 32029 Hz, a nanosecond over. */
#define FRAME_NS 17484157u
#define FRAMES 560

static void step_frame(void) {
    now += FRAME_NS;
    CHECK(sl_os_irq_dispatch() == SL_OK);
}

/* ---- The mix ---- */

static float captured[8][FRAMES * 2];
static int captures;
static int bad_captures;

static void capture(void* user, const float* samples, uint32_t frames, uint32_t channels, uint32_t rate) {
    (void)user;
    if (frames != FRAMES || channels != 2 || rate != 32029) {
        bad_captures++;
        return;
    }
    if (captures < 8)
        memcpy(captured[captures], samples, sizeof captured[0]);
    captures++;
}

static int dma_callbacks;
static int dma_callbacks_outside;
static s16* set_from_callback;

static void dma_done(void) {
    dma_callbacks++;
    dma_callbacks_outside += !sl_os_irq_active();
    if (set_from_callback != NULL) {
        AIInitDMA((uaddr)set_from_callback, FRAMES * 4);
        set_from_callback = NULL;
    }
}

/* Whether a captured frame is the block's samples, right then left as the block has them. */
static int plays(const float* frame, const s16* block) {
    for (int i = 0; i < FRAMES; ++i) {
        if (frame[2 * i] != (float)block[2 * i + 1] / 32768.0f || frame[2 * i + 1] != (float)block[2 * i] / 32768.0f)
            return 0;
    }
    return 1;
}

static int silent(const float* frame) {
    for (int i = 0; i < FRAMES * 2; ++i) {
        if (frame[i] != 0.0f)
            return 0;
    }
    return 1;
}

static void check_dsp(void) {
    static s16 channels[64 * 0xC0];
    static s16 filters[2][0x100];
    static s16 effects[4][0x40];
    static s16 output[FRAMES * 2];

    /* The boot ROM's greeting, the boot, the microcode's hello and the first wait (DspBoot). */
    CHECK(DSPCheckMailFromDSP() != 0);
    DspBoot();
    CHECK(DSPCheckMailFromDSP() == 0);
    CHECK(DSPCheckMailToDSP() == 0);
    CHECK(DSPReadMailFromDSP() == 0x80000062);

    /* The setup command's addresses travel as handles, and a command gets one answer, which the game reads. */
    DsetupTable(64, (uaddr)channels, (uaddr)filters[0], (uaddr)filters[1], (uaddr)effects);
    CHECK(DSPCheckMailFromDSP() == 0);
    CHECK(ac_dsp_mail_address(channels) == 1);
    CHECK(ac_dsp_mail_address(effects) == 4);
    CHECK(ac_dsp_mail_address(NULL) == 0);

    /* A frame of seven subframes: each mail of 0 renders 80 samples to each buffer and raises the interrupt once. */
    installed = dsp_handler;
    for (int i = 0; i < FRAMES * 2; ++i)
        output[i] = 0x1234;
    DsyncFrame(7, (uaddr)output, (uaddr)&output[FRAMES]);
    CHECK(dsp_interrupts == 0);
    for (int subframe = 0; subframe < 7; ++subframe) {
        CHECK(DSPReleaseHalt() == 0x88881357);
        CHECK(dsp_interrupts == subframe + 1);
        CHECK(output[80 * subframe] == 0 && output[80 * subframe + 79] == 0);
        CHECK(output[FRAMES + 80 * subframe] == 0 && output[FRAMES + 80 * subframe + 79] == 0);
        CHECK(subframe == 6 || output[80 * (subframe + 1)] == 0x1234);
    }
    CHECK(dsp_interrupts_outside == 0);

    /* Back to waiting for commands: the next frame starts over at the buffers' start. */
    output[0] = 0x1234;
    DwaitFrame();
    CHECK(dsp_interrupts == 7);
    DsyncFrame(7, (uaddr)output, (uaddr)&output[FRAMES]);
    CHECK(DSPReleaseHalt() == 0x88881357);
    CHECK(output[0] == 0 && dsp_interrupts == 8);
    for (int subframe = 1; subframe < 7; ++subframe)
        DSPReleaseHalt();
    CHECK(dsp_interrupts == 14);
    CHECK(reports == 0);
}

static void check_ai(void) {
    static s16 first[FRAMES * 2], second[FRAMES * 2];
    sl_audio_desc audio;

    for (int i = 0; i < FRAMES * 2; ++i) {
        first[i] = (s16)(i * 37 - 20000);
        second[i] = (s16)(15000 - i * 23);
    }
    memset(&audio, 0, sizeof audio);
    audio.sink = SL_AUDIO_SINK_NULL;
    audio.pacing = SL_AUDIO_PACE_MANUAL; /* the port's own */
    audio.capture_fn = capture;
    CHECK(ac_audio_init(&audio) == SL_OK);

    /* As jaudio starts it (Jac_Init, audioproc): nothing plays or calls back before AIStartDMA. */
    AIInit(NULL);
    AIInitDMA((uaddr)first, FRAMES * 4);
    AISetDSPSampleRate(AI_SAMPLERATE_32KHZ);
    CHECK(AIRegisterDMACallback(dma_done) == NULL);
    step_frame();
    CHECK(captures == 1 && silent(captured[0]));
    CHECK(dma_callbacks == 0);
    AIStartDMA();

    /* Each frame takes the block set last, then calls back. */
    set_from_callback = second;
    step_frame();
    CHECK(captures == 2 && plays(captured[1], first));
    CHECK(dma_callbacks == 1);
    step_frame();
    CHECK(captures == 3 && plays(captured[2], second));
    CHECK(dma_callbacks == 2);
    /* None set since: the same block again. */
    step_frame();
    CHECK(captures == 4 && plays(captured[3], second));
    CHECK(dma_callbacks == 3);
    CHECK(dma_callbacks_outside == 0);
    CHECK(bad_captures == 0);

    /* The disc's stream stays still. */
    AISetStreamPlayState(AI_STREAM_START);
    CHECK(AIGetStreamSampleCount() == 0);
    CHECK(AIGetStreamTrigger() == 0);
    ac_audio_shutdown();
    CHECK(reports == 0);
}

int main(void) {
    sl_init_desc init = {0};
    sl_os_desc os = {0};
    sl_os_thread_desc thread = {0};
    sl_os_thread self;

    init.api_version = SL_API_VERSION;
    init.app_name = "ac-ai-dsp-port-test";
    init.log_level = SL_LOG_WARN;
    init.flags = SL_INIT_NO_LOG_FILE;
    os.deterministic = 1;
    os.time_fn = now_ns;
    thread.name = "test-main";
    if (sl_init(&init) != SL_OK || sl_os_start(&os) != SL_OK || sl_os_thread_attach(&thread, &self) != SL_OK) {
        printf("cannot start sonarlib\n");
        return 2;
    }

    check_dsp();
    check_ai();

    sl_os_thread_exit(NULL);
    sl_os_stop();
    sl_shutdown();
    if (failures)
        printf("%d failures\n", failures);
    return failures != 0;
}

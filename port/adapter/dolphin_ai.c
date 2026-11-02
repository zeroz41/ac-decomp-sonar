/* The Dolphin SDK's audio interface functions the game calls, on sonarlib audio's frame (audio.h). The console's
   audio interface played a block of interleaved 16-bit samples from main memory, right then left, and at the end
   of each it took the block AIInitDMA had set last and raised its interrupt, whose callback set the next. The
   game's blocks are 560 samples, so here each audio frame is one block: the frame callback takes the block, in
   interrupt context, and runs the game's callback; two fill voices play the block's right and left samples to
   the main bus. The interface's rate is 32028.5 Hz, which audio takes as 32029; the frames come 16 ppm early.

   The disc's audio stream, which the game never starts, has functions that do nothing and counts that stay 0. */
#include "ac_port.h"

#include "dolphin/ai.h"
#include "dolphin/os.h"

#include <sonar/os.h>

#include <stdint.h>
#include <string.h>

#define AI_RATE 32029
#define AI_BLOCK_SAMPLES 560

static AIDCallback dma_callback;
static BOOL dma_running;
static u32 dsp_sample_rate = AI_SAMPLERATE_32KHZ;

/* The block AIInitDMA set, and the one playing: interleaved right and left samples. */
typedef struct ai_block {
    const s16* samples;
    u32 frames;
} ai_block;

static ai_block next_block;
static ai_block playing;
/* Where each channel's voice reads in the block playing: right, then left. */
static u32 position[2];
static sl_voice voices[2];

static void frame(void* user, uint64_t index) {
    (void)user;
    (void)index;
    if (!dma_running)
        return;
    playing = next_block;
    position[0] = position[1] = 0;
    if (dma_callback != NULL)
        dma_callback();
}

/* Plays the block over again while no other is set, as the interface did; silence before the first. */
static uint32_t fill(void* user, float* samples, uint32_t count) {
    const u32 channel = (u32)(uintptr_t)user;
    u32 i;
    if (playing.samples == NULL || playing.frames == 0) {
        memset(samples, 0, count * sizeof *samples);
        return count;
    }
    for (i = 0; i < count; ++i) {
        samples[i] = (float)playing.samples[2 * position[channel] + channel] / 32768.0f;
        if (++position[channel] == playing.frames)
            position[channel] = 0;
    }
    return count;
}

static void destroy_voices(void) {
    for (int channel = 0; channel < 2; ++channel) {
        sl_voice_destroy(voices[channel]);
        voices[channel].id = 0;
    }
}

sl_result ac_audio_init(const sl_audio_desc* desc) {
    sl_audio_desc audio;
    sl_result result;
    if (desc != NULL)
        audio = *desc;
    else
        memset(&audio, 0, sizeof audio);
    audio.pacing = SL_AUDIO_PACE_ALARM;
    audio.console_rate = AI_RATE;
    audio.frame_samples = AI_BLOCK_SAMPLES;
    audio.frame_fn = frame;
    audio.frame_user = NULL;
    result = sl_audio_init(&audio);
    if (result != SL_OK)
        return result;
    for (int channel = 0; channel < 2; ++channel) {
        sl_voice_source source;
        sl_voice_params params;
        memset(&source, 0, sizeof source);
        source.format = SL_SAMPLE_FILL;
        source.fill = fill;
        source.fill_user = (void*)(uintptr_t)channel;
        memset(&params, 0, sizeof params);
        params.pitch = 1.0f;
        params.interpolation = SL_VOICE_INTERPOLATION_NONE;
        params.volume.value = 1.0f;
        params.sends[channel == 0 ? SL_VOICE_SEND_MAIN_R : SL_VOICE_SEND_MAIN_L].value = 1.0f;
        voices[channel] = sl_voice_create();
        result = voices[channel].id == 0 ? SL_ERROR_OUT_OF_MEMORY : sl_voice_set_source(voices[channel], &source);
        if (result == SL_OK)
            result = sl_voice_set_params(voices[channel], &params);
        if (result == SL_OK)
            result = sl_voice_start(voices[channel]);
        if (result != SL_OK) {
            destroy_voices();
            sl_audio_shutdown();
            return result;
        }
    }
    return SL_OK;
}

void ac_audio_shutdown(void) {
    destroy_voices();
    sl_audio_shutdown();
}

void AIInit(u8* stack) {
    (void)stack;
    if (!sl_audio_is_initialized())
        OSReport("AIInit(): audio is not running, so the game will not be heard\n");
}

AIDCallback AIRegisterDMACallback(AIDCallback callback) {
    const int disabled = sl_os_irq_disable();
    const AIDCallback previous = dma_callback;
    dma_callback = callback;
    sl_os_irq_restore(disabled);
    return previous;
}

void AIInitDMA(uaddr start_addr, u32 length) {
    const int disabled = sl_os_irq_disable();
    next_block.samples = (const s16*)start_addr;
    next_block.frames = length / (2 * sizeof(s16));
    sl_os_irq_restore(disabled);
}

void AIStartDMA(void) { dma_running = TRUE; }

void AISetDSPSampleRate(u32 rate) {
    if (rate != AI_SAMPLERATE_32KHZ && rate != dsp_sample_rate)
        OSReport("AISetDSPSampleRate(): the port plays 32 kHz only\n");
    dsp_sample_rate = rate;
}

/* ---- The disc's audio stream ---- */

u32 AIGetStreamSampleCount(void) { return 0; }

void AIResetStreamSampleCount(void) {}

u32 AIGetStreamTrigger(void) { return 0; }

void AISetStreamPlayState(u32 state) { (void)state; }

void AISetStreamSampleRate(u32 rate) { (void)rate; }

void AISetStreamVolLeft(u8 vol) { (void)vol; }

void AISetStreamVolRight(u8 vol) { (void)vol; }

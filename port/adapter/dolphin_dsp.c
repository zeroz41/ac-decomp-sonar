/* The Dolphin SDK's DSP mailbox functions the game calls, and the DSP behind them: its boot ROM, then the audio
   microcode jaudio boots, as their mails show it (jaudio_NES/internal/dspboot.c and dspproc.c). The microcode
   takes a command word, then the words it says follow. Command 1 gives the channel table and the filters,
   command 2 a frame of subframes to render into a left and a right buffer, and from then on each mail of 0
   renders one 80-sample subframe and raises the DSP interrupt, whose handler jaudio installs. Every command but 3
   is acknowledged with a mail, which the game only checks is not 0x88881357.

   The DSP takes each mail as it is sent and answers at once, as a DSP fast enough never to keep the CPU waiting:
   the game spins on the mailboxes, which the host would never see change otherwise. In deterministic mode the
   interrupt is delivered where the mail was sent, as normal mode delivers it at that handoff, rather than at the
   runtime's next fixed point, which would hold the game's audio back a frame for every subframe.

   The microcode itself is sonarlib's jaudio kit, which renders the channel blocks command 1 gives from ARAM into
   each subframe. The resampling filter's words hold the 256 resampling coefficients, then the 256 samples of the
   constant patterns, and the ADPCM filter's the 32 AFC coefficients, two 16-bit values to a word, the first in
   its high half, as the console's big-endian DMA gave them to the DSP. The effect buffers are not rendered, as
   the kit does not render them. */
#include "ac_port.h"

#include "dolphin/dsp.h"
#include "dolphin/os.h"
#include "port/dsp_mail.h"

#include <sonar/jaudio/dsp.h>
#include <sonar/os.h>

#include <stdatomic.h>
#include <stdint.h>

/* The control register the game sets and clears interrupt bits in, and the rest of the block it never reads. */
volatile u16 __DSPRegs[32];

/* ---- Handles for host addresses ---- */

#define DSP_MAIL_ADDRESSES 32

static const void* mail_addresses[DSP_MAIL_ADDRESSES];
static u32 mail_address_count;

u32 ac_dsp_mail_address(const void* address) {
    u32 i;
    if (address == NULL)
        return 0;
    for (i = 0; i < mail_address_count; ++i) {
        if (mail_addresses[i] == address)
            return i + 1;
    }
    if (mail_address_count == DSP_MAIL_ADDRESSES) {
        OSReport("DSP: no handle left for the address %p\n", address);
        return 0;
    }
    mail_addresses[mail_address_count] = address;
    return ++mail_address_count;
}

static void* mail_address(u32 handle) {
    if (handle == 0 || handle > mail_address_count) {
        OSReport("DSP: 0x%08x is not an address's handle\n", (unsigned)handle);
        return NULL;
    }
    return (void*)mail_addresses[handle - 1];
}

/* ---- The mailbox from the DSP ---- */

#define DSP_OUTBOX_SIZE 4

/* The boot ROM's greeting is waiting from reset. */
static u32 outbox[DSP_OUTBOX_SIZE] = {0x8071FEED};
static u32 outbox_head;
static u32 outbox_count = 1;
static u32 last_mail = 0x8071FEED;

static void post(u32 mail) {
    if (outbox_count == DSP_OUTBOX_SIZE) {
        OSReport("DSP: the CPU left mail 0x%08x unread\n", (unsigned)outbox[outbox_head]);
        outbox_head = (outbox_head + 1) % DSP_OUTBOX_SIZE;
        outbox_count--;
    }
    outbox[(outbox_head + outbox_count) % DSP_OUTBOX_SIZE] = mail;
    outbox_count++;
}

/* ---- The DSP ---- */

#define DSP_SUBFRAME_SAMPLES 80
/* The command words that follow each command: 1's four, 2's two, and the crypto commands' one. */
#define DSP_MAX_ARGS 4

typedef enum dsp_state {
    DSP_STATE_ROM,     /* the boot ROM, waiting for the boot */
    DSP_STATE_BOOT,    /* taking the boot's words */
    DSP_STATE_WAITING, /* the microcode, waiting for a command */
    DSP_STATE_ARGS,    /* taking a command's words */
    DSP_STATE_RENDERING
} dsp_state;

static dsp_state state = DSP_STATE_ROM;
static u32 boot_words;
static u32 command;
static u32 args[1 + DSP_MAX_ARGS]; /* the command word first */
static u32 arg_count;
static u32 args_expected;

/* The tables command 1 gives, as 16-bit values. */
#define DSP_RESAMPLE_COEFFS 256
#define DSP_PATTERN_SAMPLES 256
#define DSP_AFC_COEFFS 32

static s16 resample_coeffs[DSP_RESAMPLE_COEFFS];
static s16 patterns[DSP_PATTERN_SAMPLES];
static s16 afc_coeffs[DSP_AFC_COEFFS];

/* What command 2 gave. */
static u32 subframes;
static u32 subframe;
static u16 output_volume;
static s16* left;
static s16* right;

static sl_os_irq interrupt;

/* The frames with a rendered sample at 1/256 of full scale or above, for the proof's lines; the presenter reads it. */
#define DSP_SOUNDING 128
static _Atomic uint64_t sounding_frames;
static BOOL frame_sounding;

static void dsp_interrupt(void* user) {
    static OSContext context;
    const __OSInterruptHandler handler = __OSGetInterruptHandler(OS_INTR_DSP_DSP);
    (void)user;
    if (handler != NULL)
        handler(OS_INTR_DSP_DSP, &context);
}

static void raise_interrupt(void) {
    if (interrupt.id == 0) {
        sl_os_irq_desc desc;
        desc.fn = dsp_interrupt;
        desc.user = NULL;
        desc.name = "DSP";
        interrupt = sl_os_irq_create(&desc);
    }
    sl_os_irq_raise(interrupt);
    if (sl_os_is_deterministic() && !sl_os_irq_active() && sl_os_thread_current().id != 0)
        (void)sl_os_irq_dispatch();
}

static void boot_microcode(void) {
    sl_jaudio_dsp_desc desc;
    sl_result result;
    desc.aram = ac_aram_memory(&desc.aram_size);
    sl_jaudio_dsp_shutdown();
    result = sl_jaudio_dsp_init(&desc);
    if (result != SL_OK)
        OSReport("DSP: the microcode does not start (%d)\n", (int)result);
}

/* Each word's two values, the high half first. */
static void unpack(s16* values, const u32* words, u32 count) {
    u32 i;
    for (i = 0; i < count / 2; ++i) {
        values[2 * i] = (s16)(u16)(words[i] >> 16);
        values[2 * i + 1] = (s16)(u16)(words[i] & 0xFFFF);
    }
}

static void set_up(u32 channel_count, void* channels, const u32* resample_filter, const u32* adpcm_filter) {
    sl_jaudio_dsp_setup setup;
    sl_result result;
    if (channels == NULL || resample_filter == NULL || adpcm_filter == NULL) {
        OSReport("DSP: the setup command is missing a table\n");
        return;
    }
    unpack(resample_coeffs, resample_filter, DSP_RESAMPLE_COEFFS);
    unpack(patterns, resample_filter + DSP_RESAMPLE_COEFFS / 2, DSP_PATTERN_SAMPLES);
    unpack(afc_coeffs, adpcm_filter, DSP_AFC_COEFFS);
    /* The game's DSPchannel_, which has the kit's layout (jaudio_NES/dspinterface.h). */
    setup.voices = (sl_jaudio_dsp_voice*)channels;
    setup.voice_count = channel_count;
    setup.resample_coeffs = resample_coeffs;
    setup.patterns = patterns;
    setup.afc_coeffs = afc_coeffs;
    result = sl_jaudio_dsp_set_up(&setup);
    if (result != SL_OK)
        OSReport("DSP: the microcode refuses its setup (%d)\n", (int)result);
}

static BOOL sounding(const s16* samples) {
    size_t i;
    for (i = 0; i < DSP_SUBFRAME_SAMPLES; ++i)
        if (samples[i] >= DSP_SOUNDING || samples[i] <= -DSP_SOUNDING)
            return TRUE;
    return FALSE;
}

static void render_subframe(void) {
    const size_t offset = (size_t)subframe * DSP_SUBFRAME_SAMPLES;
    if (left != NULL && right != NULL) {
        sl_jaudio_dsp_render(left + offset, right + offset, output_volume);
        if (sounding(left + offset) || sounding(right + offset))
            frame_sounding = TRUE;
    } else {
        OSReport("DSP: a frame without its buffers\n");
    }
    if (++subframe == subframes) {
        state = DSP_STATE_WAITING;
        if (frame_sounding)
            atomic_fetch_add(&sounding_frames, 1);
    }
    raise_interrupt();
}

u64 ac_dsp_sounding_frames(void) { return atomic_load(&sounding_frames); }

/* The microcode acknowledges a command with its handler's address in its program. */
static void acknowledge(void) { post(0x80000000u | (0x62u + 2u * command)); }

static void run_command(void) {
    state = DSP_STATE_WAITING;
    switch (command) {
        case 0x01:
            set_up(args[0] & 0xFFFF, mail_address(args[1]), (const u32*)mail_address(args[2]),
                   (const u32*)mail_address(args[3]));
            break;
        case 0x02:
            subframes = (args[0] >> 16) & 0xFF;
            output_volume = (u16)(args[0] & 0xFFFF);
            left = (s16*)mail_address(args[1]);
            right = (s16*)mail_address(args[2]);
            subframe = 0;
            frame_sounding = FALSE;
            if (subframes != 0)
                state = DSP_STATE_RENDERING;
            break;
        case 0x0B:
        case 0x0C:
            OSReport("DSP: command 0x%02x, the IPL's or the Game Boy Advance's crypto, is not supported\n",
                     (unsigned)command);
            break;
        default: break;
    }
    acknowledge();
}

static void take_command(u32 mail) {
    if ((mail & 0x80000000u) == 0) {
        OSReport("DSP: 0x%08x is not a command\n", (unsigned)mail);
        return;
    }
    command = (mail >> 24) & 0x7F;
    args[0] = mail;
    arg_count = 1;
    switch (command) {
        case 0x00: args_expected = 0; break;
        case 0x01: args_expected = 4; break;
        case 0x02: args_expected = 2; break;
        case 0x03: return; /* back to the microcode's dispatcher, with no answer */
        case 0x0B:
        case 0x0C: args_expected = 1; break;
        default:
            OSReport("DSP: unknown command 0x%08x\n", (unsigned)mail);
            args_expected = 0;
            break;
    }
    if (args_expected == 0)
        run_command();
    else
        state = DSP_STATE_ARGS;
}

void DSPSendMailToDSP(u32 mail) {
    switch (state) {
        case DSP_STATE_ROM:
            if (mail == 0x80F3A001) {
                state = DSP_STATE_BOOT;
                boot_words = 9;
            } else {
                OSReport("DSP: the boot ROM ignores 0x%08x\n", (unsigned)mail);
            }
            break;
        case DSP_STATE_BOOT:
            /* The microcode's address, length and entry, which the HLE has no use for. */
            if (--boot_words == 0) {
                state = DSP_STATE_WAITING;
                boot_microcode();
                post(0x88881111);
            }
            break;
        case DSP_STATE_WAITING: take_command(mail); break;
        case DSP_STATE_ARGS:
            args[arg_count++] = mail;
            if (arg_count > args_expected)
                run_command();
            break;
        case DSP_STATE_RENDERING:
            if (mail != 0)
                OSReport("DSP: 0x%08x while rendering, where a subframe's 0 goes\n", (unsigned)mail);
            render_subframe();
            break;
    }
}

/* The DSP takes every mail as it is sent. */
u32 DSPCheckMailToDSP(void) { return 0; }

/* The mailbox's full bit. */
u32 DSPCheckMailFromDSP(void) { return outbox_count != 0 ? 0x8000 : 0; }

/* An empty mailbox still holds the last mail. */
u32 DSPReadMailFromDSP(void) {
    if (outbox_count != 0) {
        last_mail = outbox[outbox_head];
        outbox_head = (outbox_head + 1) % DSP_OUTBOX_SIZE;
        outbox_count--;
    }
    return last_mail;
}

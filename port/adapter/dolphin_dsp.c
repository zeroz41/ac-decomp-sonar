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

   The subframes are silent for now: the microcode's channels are rendered by sonarlib's jaudio kit once it has
   them. They play only the console's start-up jingle; the game's sound comes through the CPU's mix. */
#include "dolphin/dsp.h"
#include "dolphin/os.h"
#include "port/dsp_mail.h"

#include <sonar/os.h>

#include <stdint.h>
#include <string.h>

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

/* What command 1 gave, for the channels' renderer. */
static u32 channel_count;
static void* channels;
static void* resample_filter;
static void* adpcm_filter;
static void* effect_buffers;

/* What command 2 gave. */
static u32 subframes;
static u32 subframe;
static u16 output_volume;
static s16* left;
static s16* right;

static sl_os_irq interrupt;

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

static void render_subframe(void) {
    const size_t offset = (size_t)subframe * DSP_SUBFRAME_SAMPLES;
    if (left != NULL)
        memset(left + offset, 0, DSP_SUBFRAME_SAMPLES * sizeof *left);
    if (right != NULL)
        memset(right + offset, 0, DSP_SUBFRAME_SAMPLES * sizeof *right);
    if (++subframe == subframes)
        state = DSP_STATE_WAITING;
    raise_interrupt();
}

/* The microcode acknowledges a command with its handler's address in its program. */
static void acknowledge(void) { post(0x80000000u | (0x62u + 2u * command)); }

static void run_command(void) {
    state = DSP_STATE_WAITING;
    switch (command) {
        case 0x01:
            channel_count = args[0] & 0xFFFF;
            channels = mail_address(args[1]);
            resample_filter = mail_address(args[2]);
            adpcm_filter = mail_address(args[3]);
            effect_buffers = mail_address(args[4]);
            break;
        case 0x02:
            subframes = (args[0] >> 16) & 0xFF;
            output_volume = (u16)(args[0] & 0xFFFF);
            left = (s16*)mail_address(args[1]);
            right = (s16*)mail_address(args[2]);
            subframe = 0;
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

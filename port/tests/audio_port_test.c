/* Checks the port's audio command (jaudio_NES/audiostruct.h): each byte of a command word and each narrow parameter
   reads as the console read it from the big-endian word its sender built (NA_MAKE_COMMAND, Nap_SetS8, Nap_SetU16),
   and an address keeps the whole host pointer. */
#include "jaudio_NES/audiocommon.h"
#include "jaudio_NES/audiostruct.h"

#include <stdio.h>
#include <string.h>

/* The decompilation's math headers define functions that read MSL's constants, which the port does not link. */
unsigned long __float_nan[1] = {0x7fc00000};
unsigned long __float_huge[1] = {0x7f800000};

static int failures;

#define CHECK(cond)                                                                                                   \
    if (!(cond)) {                                                                                                    \
        printf("%s:%d: %s\n", __FILE__, __LINE__, #cond);                                                             \
        failures++;                                                                                                   \
    }

/* What Nap_PortSet stores in the port: the s32 across the whole parameter. */
static void set_param(AudioPort* port, s32 value) { port->param.asAddr = (uaddr)(saddr)value; }

int main(void) {
    AudioPort port;
    static int target;

    memset(&port, 0xAA, sizeof port);
    port.raw_cmd = NA_MAKE_COMMAND(0x12, 0x34, 0x56, 0x78);
    CHECK(port.command.opcode == 0x12);
    CHECK(port.command.arg0 == 0x34);
    CHECK(port.command.arg1 == 0x56);
    CHECK(port.command.arg2 == 0x78);

    /* Nap_SetS8 and Nap_SetU16 shift into the top of the word. */
    set_param(&port, (s32)((u32)(u8)-5 << 24));
    CHECK(port.param.asS8 == -5);
    CHECK(port.param.asU8 == 0xFB);
    set_param(&port, (s32)((u32)0xBEEF << 16));
    CHECK(port.param.asU16 == 0xBEEF);
    CHECK(port.param.asS16 == (s16)0xBEEF);

    /* An s32 reads back whole, and as an address keeps its sign. */
    set_param(&port, -1234);
    CHECK(port.param.asS32 == -1234);
    CHECK((saddr)port.param.asAddr == -1234);
    {
        f32 f = 0.75f;
        s32 bits;
        memcpy(&bits, &f, sizeof bits);
        set_param(&port, bits);
        CHECK(port.param.asF32 == 0.75f);
    }

    /* Nap_SetPtr */
    port.param.asAddr = (uaddr)&target;
    CHECK(port.param.asVoidPtr == &target);

    if (failures)
        printf("%d failures\n", failures);
    return failures != 0;
}

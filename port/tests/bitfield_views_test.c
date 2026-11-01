/* Checks the port's bit-field views of a whole value: each field reads the bits the console's compiler gave it, which
   allocates a value's fields from its most significant bit. */
#include "jaudio_NES/audiostruct.h"
#include "libu64/u64types.h"
#include "m_event.h"

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

int main(void) {
    /* gfxprint sets the channels and sends the word as 0xRRGGBBAA. */
    {
        rgba8888 color;
        color.c.r = 0x12;
        color.c.g = 0x34;
        color.c.b = 0x56;
        color.c.a = 0x78;
        CHECK(color.rgba8888 == 0x12345678);
    }

    /* The driver sets a bus route as a whole and reads its parts (driverinterface.c). */
    {
        MixConfig config;
        config.whole = 0x352;
        CHECK(config.parts.upper == 3);
        CHECK(config.parts.lower0 == 5);
        CHECK(config.parts.lower1 == 2);
    }

    /* The event manager sets bit 7 of the raw byte for used_all_locations (ac_event_manager.c). */
    {
        mEv_save_common_data_c common;
        memset(&common, 0, sizeof common);
        common.bridge_flags.raw = (1 << 7) | 5;
        CHECK(common.bridge_flags.used_all_locations == 1);
        CHECK(common.bridge_flags.locations_used == 5);
        common.bridge_flags.raw = 0;
        common.bridge_flags.used_all_locations = 1;
        CHECK(common.bridge_flags.raw == 0x80);
    }

    if (failures)
        printf("%d failures\n", failures);
    return failures != 0;
}

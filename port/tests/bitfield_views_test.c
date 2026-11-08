/* Checks the port's bit-field views of a whole value: each field reads the bits the console's compiler gave it, which
   allocates a value's fields from its most significant bit. */
#include "jaudio_NES/audiostruct.h"
#include "libu64/u64types.h"
#include "m_event.h"
#include "lb_rtc.h"
#include "m_collision_bg.h"

#include <stdio.h>
#include <string.h>

/* The event schedule as m_event.c takes it, with its copy into mEv_schedule_c. */
#include "src/game/m_event_schedule.c_inc"

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

    /* The event dates compare as words, month << 8 | day (m_event.c, check_date_range) and month << 24 | day << 16 |
       hour (delete_too_short_event), and m_shop.c takes the day as md & 0xFF. */
    {
        mEv_MonthDay_u jan31, feb1;
        mEv_schedule_date_u date;

        jan31.month = lbRTC_JANUARY;
        jan31.day = 31;
        feb1.month = lbRTC_FEBRUARY;
        feb1.day = 1;
        CHECK(feb1.raw == 0x0201);
        CHECK(jan31.raw < feb1.raw);

        date.d.month = lbRTC_DECEMBER;
        date.d.day = 25;
        date.d._2 = 0;
        date.d.hour = 6;
        CHECK(date.raw == 0x0C190006);
        CHECK(date.md == 0x0C19);
        CHECK(date.md == date._h);
        date.md = feb1.raw;
        CHECK(date.d.month == lbRTC_FEBRUARY && date.d.day == 1 && date.d.hour == 6);
    }

    /* Every schedule entry, copied as update_schedule_today copies it, reads the console's words. */
    {
        size_t i;
        int j;

        CHECK(sizeof(event_schedule_data) / sizeof(event_schedule_data[0]) == 134);
        for (i = 0; i < sizeof(event_schedule_data) / sizeof(event_schedule_data[0]); i++) {
            const mEv_schedule_src_c* src = &event_schedule_data[i];
            mEv_schedule_c sched;

            memset(&sched, 0xA5, sizeof sched);
            mEv_copy_schedule_data(&sched, src);
            for (j = 0; j < 2; j++) {
                CHECK(sched.date[j].raw == ((u32)src->date[j].month << 24 | (u32)src->date[j].day << 16 |
                                            (u32)src->date[j]._2 << 8 | src->date[j].hour));
                CHECK(sched.date[j].md == ((u32)src->date[j].month << 8 | src->date[j].day));
            }
            CHECK(sched._8 == src->_8);
            CHECK(sched.type == src->type);
        }

        /* The last entry: {lbRTC_MAY, 1, 0x00, 0}, {lbRTC_MAY, 5, 0x00, 23}, mEv_EVENT_KOINOBORI. */
        {
            mEv_schedule_c sched;

            mEv_copy_schedule_data(&sched, &event_schedule_data[133]);
            CHECK(sched.date[0].raw == 0x05010000);
            CHECK(sched.date[1].raw == 0x05050017);
            CHECK(sched.type == mEv_EVENT_KOINOBORI);
        }
    }

    /* The event manager masks the year off and bounds the rest, 0x00010100 to 0x000C1F17 (mail_event_check). */
    {
        mEv_ymdh_u first, last, later;

        first.year = 3;
        first.month = lbRTC_JANUARY;
        first.day = 1;
        first.hour = 0;
        CHECK(first.raw == 0x03010100);
        CHECK((first.raw & 0x00FFFFFF) == 0x00010100);
        last.year = 3;
        last.month = lbRTC_DECEMBER;
        last.day = 31;
        last.hour = 23;
        CHECK((last.raw & 0x00FFFFFF) == 0x000C1F17);
        later = first;
        later.year = 4;
        CHECK(last.raw < later.raw);
    }

    /* mCoBG_GetHoleNumber_ClData takes a unit as a hole when its console word less the hole's is its attribute. */
    {
        mCoBG_Collision_u hole = { { 0, 1, 2, 0, 0, 2, 0 } };
        mCoBG_Collision_u unit = { { 0, 1, 2, 0, 0, 2, mCoBG_ATTRIBUTE_SAND } };
        mCoBG_Collision_u other = { { 0, 2, 2, 0, 0, 2, mCoBG_ATTRIBUTE_SAND } };

        CHECK(mCoBG_CONSOLE_RAW(hole) == (1u << 26 | 2u << 21 | 2u << 6));
        CHECK(mCoBG_CONSOLE_RAW(unit) - mCoBG_CONSOLE_RAW(hole) == mCoBG_ATTRIBUTE_SAND);
        CHECK(mCoBG_CONSOLE_RAW(other) - mCoBG_CONSOLE_RAW(hole) != mCoBG_ATTRIBUTE_SAND);
        unit.data.slate_flag = 1;
        unit.data.unit_attribute = 63;
        CHECK(mCoBG_CONSOLE_RAW(unit) == (1u << 31 | 1u << 26 | 2u << 21 | 2u << 6 | 63));
    }

    /* The fade and wipe transitions set the color as 0xRRGGBBAA and read its channels (m_fbdemo_fade.c). */
    {
        Color_RGBA8_u32 color;
        color.rgba = 0xA0B0C0FF;
        CHECK(color.r == 0xA0);
        CHECK(color.g == 0xB0);
        CHECK(color.b == 0xC0);
        CHECK(color.a == 0xFF);
    }

    /* The sequence sets the stereo phase byte, type in bits 5-4 and strong_right in bit 3 (track.c). */
    {
        phase stereo;
        *(u8*)&stereo = 0x2A;
        CHECK(stereo._unused == 0);
        CHECK(stereo.type == 2);
        CHECK(stereo.strong_right == 1);
        CHECK(stereo.strong_left == 0);
        CHECK(stereo.strong_reverb_right == 1);
        CHECK(stereo.strong_reverb_left == 0);
        *(u8*)&stereo = 0xC1;
        CHECK(stereo._unused == 3);
        CHECK(stereo.type == 0);
        CHECK(stereo.strong_reverb_left == 1);
    }

    /* The sequence clears a note's flags with a big-endian word, bit 15 for skip_volume_update (track.c). */
    {
        note n;
        n._0A.asU16 = 0xFFFF;
        n._0A.asU16 &= 0x8000 ^ 0xFFFF;
        CHECK(n._0A.flags.skip_volume_update == 0);
        CHECK(n._0A.flags.skip_freq_scale_update == 1);
        CHECK(n._0A.flags.bitF == 1);
        n._0A.asU16 = 0;
        n._0A.flags.use_vibrato = 1;
        n._0A.flags.skip_pan_update = 1;
        CHECK(n._0A.asU16 == (1 << 12 | 1 << 7));
    }

    if (failures)
        printf("%d failures\n", failures);
    return failures != 0;
}

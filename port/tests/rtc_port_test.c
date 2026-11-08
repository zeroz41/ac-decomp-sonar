/* Checks the port's lbRTC_IsEqualDate (src/lb_rtc.c): two dates compare as the console's year << 16 | month << 8 |
   day word, so an earlier month is less whatever the days, and lbRTC_GetIntervalDays2 counts days forward and
   backward on that order. */
#include "lb_rtc.h"
#include "m_common_data.h"
#include "m_time.h"

#include <stdio.h>
#include <string.h>

/* lb_rtc.c's other references, which these checks do not reach but its other functions name. */
common_data_t common_data;
OSTime OSGetTime(void) { return 0; }
OSTime OSCalendarTimeToTicks(OSCalendarTime* td) {
    (void)td;
    return 0;
}
void OSTicksToCalendarTime(OSTime ticks, OSCalendarTime* td) {
    (void)ticks;
    memset(td, 0, sizeof *td);
}

/* m_time.c's, less the weekday, which lbRTC_GetIntervalDays does not read. */
void mTM_ymd_2_time(lbRTC_time_c* time, lbRTC_ymd_c* ymd) {
    memset(time, 0, sizeof *time);
    time->year = ymd->year;
    time->month = ymd->month;
    time->day = ymd->day;
}

static int failures;

#define CHECK(cond)                                                                                                   \
    if (!(cond)) {                                                                                                    \
        printf("%s:%d: %s\n", __FILE__, __LINE__, #cond);                                                             \
        failures++;                                                                                                   \
    }

int main(void) {
    CHECK(lbRTC_IsEqualDate(2003, lbRTC_JANUARY, 31, 2003, lbRTC_FEBRUARY, 1) == lbRTC_LESS);
    CHECK(lbRTC_IsEqualDate(2003, lbRTC_FEBRUARY, 1, 2003, lbRTC_JANUARY, 31) == lbRTC_OVER);
    CHECK(lbRTC_IsEqualDate(2003, lbRTC_DECEMBER, 31, 2004, lbRTC_JANUARY, 1) == lbRTC_LESS);
    CHECK(lbRTC_IsEqualDate(2004, lbRTC_JANUARY, 1, 2003, lbRTC_DECEMBER, 31) == lbRTC_OVER);
    CHECK(lbRTC_IsEqualDate(2003, lbRTC_MAY, 5, 2003, lbRTC_MAY, 6) == lbRTC_LESS);
    CHECK(lbRTC_IsEqualDate(2003, lbRTC_MAY, 5, 2003, lbRTC_MAY, 5) == lbRTC_EQUAL);

    {
        lbRTC_ymd_c jan31 = { 2003, lbRTC_JANUARY, 31 };
        lbRTC_ymd_c feb1 = { 2003, lbRTC_FEBRUARY, 1 };
        lbRTC_ymd_c next_year = { 2004, lbRTC_JANUARY, 31 };

        CHECK(lbRTC_GetIntervalDays2(&jan31, &feb1) == 1);
        CHECK(lbRTC_GetIntervalDays2(&feb1, &jan31) == -1);
        CHECK(lbRTC_GetIntervalDays2(&jan31, &next_year) == 365);
        CHECK(lbRTC_GetIntervalDays2(&jan31, &jan31) == 0);
    }

    if (failures)
        printf("%d failures\n", failures);
    return failures != 0;
}

#include "sys_math.h"
#include "libultra/libultra.h"
#include "libc64/qrand.h"
#include "libc64/math64.h"
#ifdef SONAR_PORT
#include <port/goldens.h>
#endif

extern void init_rnd() {
#ifdef SONAR_PORT /* sonarlib port: the goldens give Dolphin's seed (--qrand-seed) */
    sqrand(ac_port_rnd_seed(osGetCount()));
#else
    sqrand(osGetCount());
#endif
}

#ifdef MUST_MATCH
FORCESTRIP static f32 _rodata_order(int in) {
    return (f32)in;
}
#endif

extern f32 sinf_table(f32 x) {
    s16 v = (SHT_MAX / M_PI) * x;
    s16 sin = sins(v);

    return sin * SHT_MINV;
}

extern f32 cosf_table(f32 x) {
    s16 v = (SHT_MAX / M_PI) * x;
    s16 cos = coss(v);

    return cos * SHT_MINV;
}

/* The original compiler's intrinsics that the game's code calls, for the port build, with the console's results
   (sonarlib docs/porting-model.md, Floating-point determinism). port/CMakeLists.txt includes it in every game unit,
   so the matching build keeps the intrinsics. */
#ifndef AC_PORT_INTRINSICS_H
#define AC_PORT_INTRINSICS_H

#include <sonar/legacy_fp.h>

#include <stdlib.h>
#if defined(_MSC_VER)
#include <malloc.h>
#define __alloca(n) _alloca(n)
#else
#include <alloca.h>
#define __alloca(n) alloca(n)
#define __declspec(x)
#endif

#define __abs(x) abs(x)

#define __fabs(x) sl_fp_fabs(x)
#define __frsqrte(x) sl_fp_frsqrte(x)
#define __fres(x) ((float)sl_fp_fres(x))

/* cntlzw: the leading zeros of a 32-bit word, 32 for 0. */
#if defined(_MSC_VER)
#include <intrin.h>
static inline int ac_cntlzw(unsigned int x) {
    unsigned long index;
    return _BitScanReverse(&index, x) ? 31 - (int)index : 32;
}
#else
static inline int ac_cntlzw(unsigned int x) {
    return x == 0 ? 32 : __builtin_clz(x);
}
#endif
#define __cntlzw(x) ac_cntlzw((unsigned int)(x))

/* sync: a sequentially consistent fence. */
#if defined(_MSC_VER) && (defined(_M_ARM64) || defined(_M_ARM))
#define __sync() __dmb(_ARM64_BARRIER_ISH)
#elif defined(_MSC_VER)
#define __sync() _mm_mfence()
#else
#define __sync() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#endif

#endif

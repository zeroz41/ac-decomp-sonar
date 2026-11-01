#ifndef W_MATH_H
#define W_MATH_H

#include "MSL_C/MSL_Common/float.h"

#ifdef SONAR_PORT
/* sonarlib port: inlined where they are called, and defined once, in port/adapter/msl_inlines.c, where the original
   compiler left a copy in each unit */
#ifdef AC_PORT_MSL_INLINES
#define MSL_EXTERN_INLINE
#define MSL_INLINE
#else
#define MSL_EXTERN_INLINE extern inline __attribute__((gnu_inline))
#define MSL_INLINE extern inline __attribute__((gnu_inline))
#endif
#else
#define MSL_EXTERN_INLINE extern inline
#define MSL_INLINE inline
#endif

#if defined(SONAR_PORT) && defined(__cplusplus)
extern "C" { /* sonarlib port: the one definition is C's */
#endif

#if !defined(BUGFIXES) && !defined(FIX_SQRT_LINKAGE) && !defined(SONAR_PORT)
#define SQRTF_LINKAGE extern
#else
// making the function static instead of extern resolves the bug
#define SQRTF_LINKAGE static
#endif

/**
 * Float square root implementation.
 *
 * NOTE: this function causes a bug in the Metrowerks C Compiler from GC MW 1.3.X
 * to be exhibited. Weak extern inlined functions that contain an initialized
 * static variable turn off data pooling for the translation unit which they are
 * included in. As w_math.h seems to be included in a top-level header that
 * the AC devs used, every translation unit in foresta.rel includes sqrtf.
 * The effect is that the .rodata section has pooling entirely disabled.
 */
SQRTF_LINKAGE inline float sqrtf(float x) {
    static const double _half = .5;
    static const double _three = 3.0;
    volatile float y;

    if (x > 0.0f) {
        double guess = __frsqrte((double)x);                  // returns an approximation to
        guess = _half * guess * (_three - guess * guess * x); // now have 12 sig bits
        guess = _half * guess * (_three - guess * guess * x); // now have 24 sig bits
        guess = _half * guess * (_three - guess * guess * x); // now have 32 sig bits
        y = (float)(x * guess);
        return y;
    }

    return x;
}

#ifdef SQRTF_LINKAGE
#undef SQRTF_LINKAGE
#endif

// Dirty hack to avoid the current header mess
#ifndef FABS_DECLARED
#define FABS_DECLARED
MSL_EXTERN_INLINE double fabs(double x) {
    return __fabs(x);
}
#endif

MSL_INLINE float fabsf(float x) {
    return (float)fabs((double)x);
}

#define INFINITY (*(float*)__float_huge)
#define NAN (*(float*)__float_nan)
#define HUGE_VALF (*(float*)__float_huge)

MSL_EXTERN_INLINE double sqrt(double x) {
    if (x > 0.0) {
        double guess = __frsqrte(x);
        guess = .5 * guess * (3.0 - guess * guess * x);
        guess = .5 * guess * (3.0 - guess * guess * x);
        guess = .5 * guess * (3.0 - guess * guess * x);
        guess = .5 * guess * (3.0 - guess * guess * x);
        return x * guess;
    } else if (x == 0.0f) {
        return 0.0;
    } else if (x) {
        return NAN;
    }
    return INFINITY;
}

#if defined(SONAR_PORT) && defined(__cplusplus)
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

// double atan2(double, double);
// double acos(float);

#ifdef __cplusplus
}
#endif

#endif

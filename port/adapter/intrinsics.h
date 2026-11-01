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

#endif

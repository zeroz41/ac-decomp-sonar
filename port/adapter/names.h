/* The C library as the game's code sees it, forced into every game unit after intrinsics.h. The game defines
   functions whose names the host's C library already declares, so those names are renamed here, after the host
   headers that declare them, and the game's definitions and calls agree on the new names. The decompilation's
   libc headers add a few macros of their own, defined here because the port takes the host's headers. */
#ifndef AC_PORT_NAMES_H
#define AC_PORT_NAMES_H

#ifndef __cplusplus
#include <math.h>
#else
#include <cstdio>
#include <cstdlib>
#endif
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if __has_include(<strings.h>)
#include <strings.h>
#endif

#define bcmp ac_bcmp
#define bcopy ac_bcopy
#define bzero ac_bzero

/* libc64's allocator and formatter (src/static/libc64/), which the game keeps; the host's C library, which sonarlib
   and the host's own libraries call, keeps its own. */
#define malloc ac_malloc
#define free ac_free
#define sprintf ac_sprintf

/* MSL's math, which the game keeps for the console's results; the host's library keeps its own. */
#define acos ac_acos
#define atan ac_atan
#define atan2 ac_atan2
#define copysign ac_copysign
#define cos ac_cos
#define cosf ac_cosf
#define fabs ac_fabs
#define fabsf ac_fabsf
#define floor ac_floor
#define frexp ac_frexp
#define fsqrt ac_fsqrt /* libc64's; glibc declares a narrowing one */
#define ldexp ac_ldexp
#define modf ac_modf
#define sin ac_sin
#define sinf ac_sinf
#define sqrt ac_sqrt
#define sqrtf ac_sqrtf
/* MSL_Common/float.h's, inline; glibc declares a function of its own by the name. */
#define __fpclassifyf ac___fpclassifyf

#define membersize(type, member) (sizeof(((type*)0)->member))

#endif

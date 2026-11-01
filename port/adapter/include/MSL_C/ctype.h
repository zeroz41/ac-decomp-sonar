/* The decompilation's MSL_C/ctype.h declares MSL's library; the port takes the host's, with MSL's unchecked case
   mappings. */
#ifndef AC_PORT_MSL_CTYPE_H
#define AC_PORT_MSL_CTYPE_H

#include <ctype.h>

static inline int ac_msl_tolower(int c) { return c == -1 ? -1 : tolower(c); }
static inline int ac_msl_toupper(int c) { return c == -1 ? -1 : toupper(c); }

#undef _tolower
#undef _toupper
#define _tolower ac_msl_tolower
#define _toupper ac_msl_toupper

#endif

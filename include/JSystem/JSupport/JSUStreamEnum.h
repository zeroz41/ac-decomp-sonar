#ifndef JSUSTREAMENUM_H
#define JSUSTREAMENUM_H

#ifdef SONAR_PORT
/* sonarlib port: the host's stdio defines these names as macros, and EOF as -1 */
#include <stdio.h>
#undef SEEK_SET
#undef SEEK_CUR
#undef SEEK_END
#undef EOF
#endif

enum JSUStreamSeekFrom { SEEK_SET = 0, SEEK_CUR = 1, SEEK_END = 2 };

enum EIoState { GOOD = 0, EOF = 1 };

#endif

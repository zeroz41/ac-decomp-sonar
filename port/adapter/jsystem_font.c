/* JSystem's system font, JFWSystem's default, which the game keeps in its executable (JUTFontData_Ascfont_fix12.s).
   The bytes are the console's, big-endian, as JUTResFont reads them. */

#include <stdalign.h>

alignas(32) const unsigned char JUTResFONT_Ascfont_fix12[] = {
#include "Ascfont_fix12.inc"
};

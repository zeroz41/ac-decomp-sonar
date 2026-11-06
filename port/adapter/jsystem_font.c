/* JSystem's system font, JFWSystem's default, which the game keeps in its executable (JUTFontData_Ascfont_fix12.s).
   ac-font converts its integers to the host's byte order, as JUTResFont reads them in place (importer/font.hpp). */

#include <stdalign.h>

alignas(32) const unsigned char JUTResFONT_Ascfont_fix12[] = {
#include "Ascfont_fix12.inc"
};

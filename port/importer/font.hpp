/* JSystem's font (ResFONT, JUTFont.h) converted to the host's byte order (sonarlib docs/roadmap.md, M6 step 6f):
   JUTResFont reads its integers in place, so the build converts the system font the executable keeps, as the importer
   converts the package's data. */
#ifndef AC_IMPORTER_FONT_HPP
#define AC_IMPORTER_FONT_HPP

#include <sonar/base/result.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ac {

/* A console font as the host's, every other byte kept:
     the header: mFileSize and mNumBlocks (0x08, 0x0C); the 8-byte magic and the padding are kept;
     each block from 0x20, walked by the console's sizes to the file's end: mMagic and mSize;
     INF1: the six u16 at 0x08 to 0x12;
     WID1: the two u16 at 0x08 and 0x0A; the widths are bytes, kept;
     GLY1: the u16 at 0x08 to 0x0E and 0x14 to 0x1E, the u32 at 0x10; the texels from 0x20 are kept;
     MAP1: every u16 from 0x08 to the block's end.

   SL_ERROR_IO for a file shorter than its header, a magic other than FONTbfn1, a block that runs past the file or is
   shorter than its own fields, a block count other than mNumBlocks, a block of another kind, and a MAP1 of odd
   length. */
sl_result convert_font(const uint8_t* console, size_t size, std::vector<uint8_t>& out);

} // namespace ac

#endif

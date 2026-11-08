/* A JKR archive (RARC) laid out again for the host (sonarlib docs/roadmap.md, M6 step 6c). The game mounts archives
   whole and reads their files through its own JKR code, so the importer keeps each archive one file: its directory at
   the host's sizes and in the host's byte order, through the game's own structure types, and its file data with every
   offset kept, the files a rule names converted in place. */
#ifndef AC_IMPORTER_ARCHIVE_HPP
#define AC_IMPORTER_ARCHIVE_HPP

#include <sonar/base/result.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ac {

// A file's conversion, below.
enum class Content : uint8_t { fg_data, u32_table, palettes, face, timg, room_wall, room_floor };

struct ContentRule {
    const char* name; // a file in the archive's first node of type 'DATA'
    Content content;
};

/* A console archive as the host's: the layout below, each rule's file converted in place.

   The host's archive, every integer in the host's order: SArcHeader at 0, with file_data_offset the host info block's
   size D and file_length the host file's size; the info block at 0x20, SArcDataInfo at 0, the nodes at A (0x20), the
   file entries at B and the string table at C, each start rounded up to 32, D the end of the string table rounded up
   to 32; then the console's file data at 0x20 + D, zero bytes following to the next multiple of 32.

   The conversions run on a file's mSize bytes: fg_data converts each whole 0x206-byte mFM_fg_data_c; u32_table swaps
   each whole word; palettes swaps each 2-byte entry; face keeps each record's 0xE00 bytes of texels and swaps the
   palettes around them as 2-byte entries; timg converts the CResTIMG header at the file's start and keeps the texels
   and palette after it, which the card files hold in the console's order; room_wall and room_floor, for files of whole
   0x1020- and 0x2020-byte records, swap each record's 0x20-byte palette at its start as 2-byte entries and keep the
   texels after it.

   SL_ERROR_UNSUPPORTED for a whole compressed archive (Yaz0 or Yay0). SL_ERROR_IO for a signature other than RARC, a
   header_length other than 0x20, an info block, table or file data past the file, a node's entries past the entry
   table, an entry's name past the string table, a rule's file missing, a directory, compressed or with data past the
   file data, and a file a conversion finds malformed, a room_wall or room_floor file that is not whole records among
   them. */
sl_result convert_archive(const uint8_t* console, size_t size, const ContentRule* rules, size_t rule_count,
                          std::vector<uint8_t>& out);

} // namespace ac

#endif

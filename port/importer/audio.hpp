/* jaudio_NES's audiorom.img laid out again for the host (sonarlib docs/roadmap.md, M6 step 6l). The sequenced
   engine copies a bank from ARAM into its cache and turns the bank's offsets into pointers in place
   (Nas_BankOfsToAddr_Inner, system.c:811-912; __WaveTouch, system.c:1692), so the banks need the host's structures;
   sequences, envelopes and sample data stay the console's bytes, read in the console's byte order where they are
   read (NOTES.md, Sound). */
#ifndef AC_IMPORTER_AUDIO_HPP
#define AC_IMPORTER_AUDIO_HPP

#include <sonar/base/result.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ac {

// audiorom.tbl's version word, which ac_audio_rom_tables (dolphin_dvd.c) checks.
inline constexpr uint32_t audiorom_table_version = 1;

/* The disc's audiorom.img as the host's, by the tables compiled into the game (game/audioheaders.c), laid out by
   sl_jaudio_nas_rom_layout (sonar/jaudio/nas_rom.h) with this build's limits.

   image: the host ROM, as the kit lays it out.

   table (audiorom.tbl), host-order u32s: audiorom_table_version, the bank table's entry count N, the data table's
   count M, then N pairs of the bank entries' addr and size, then M pairs of the data entries', as the host image has
   them. Entries of size 0 keep their addr, which names another entry.

   SL_ERROR_IO, with *error saying which bank and offset, for a ROM the kit can't lay out. */
struct AudioRom {
    std::vector<uint8_t> image;
    std::vector<uint8_t> table;
    uint32_t console_banks = 0; // the bank region's console size
    uint32_t host_banks = 0;    // and its host size
};

sl_result convert_audiorom(const uint8_t* console, size_t size, AudioRom& out, std::string& error);

} // namespace ac

#endif

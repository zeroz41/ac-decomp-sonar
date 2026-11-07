/* The boot sound's bank and wave set (NIN_BANK and NIN_WAVE, game64_dummy.c_inc) laid out for the host (sonarlib
   docs/roadmap.md, M6 step 6m). JAudio1 reads both in place, as structures of pointers and the console's integers, so
   the build lays out the bytes the executable keeps with sonarlib's jaudio kit (sonar/jaudio/jaudio1.h), whose
   structures boot_sound.cpp checks against the game's own (bx.h, heapctrl.h). The layout is the building machine's. */
#ifndef AC_IMPORTER_BOOT_SOUND_HPP
#define AC_IMPORTER_BOOT_SOUND_HPP

#include <sonar/base/result.h>

#include <cstdint>
#include <string>
#include <vector>

namespace ac {

/* The bank (`bank`) or the wave set laid out into `host`. SL_ERROR_IO, with the kit's text in `error`, for a file the
   kit refuses. */
sl_result lay_out_boot_sound(bool bank, const std::vector<uint8_t>& console, std::vector<uint8_t>& host,
                             std::string& error);

/* The definitions game64_dummy.c_inc includes: NIN_BANK and NIN_WAVE, 32-byte aligned, holding the two images. */
std::string boot_sound_include(const std::vector<uint8_t>& bank, const std::vector<uint8_t>& wave);

} // namespace ac

#endif

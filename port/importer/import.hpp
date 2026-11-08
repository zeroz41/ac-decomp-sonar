/* The package ac-import writes (sonarlib docs/roadmap.md, M6 steps 6c and 11c): each file the game opens from boot to
   Town's first frame, at the game's own name for it. */
#ifndef AC_IMPORTER_IMPORT_HPP
#define AC_IMPORTER_IMPORT_HPP

#include <sonar/import.h>

#include <cstdint>

namespace ac {

inline constexpr const char* importer_id = "ac";
inline constexpr uint32_t importer_version = 4;

// Adds the package's entries, the table below, to an import. On a failure, *failed is the path of the entry whose
// sl_import_add failed.
//
//   COPYDATE, static.str   copy 1            the disc's bytes
//   forest_1st.arc         archive 2         the directory; fgdata.bin, the ten message tables (*_data_table.bin),
//                                            pallet_boy.bin, face_boy.bin
//   forest_2nd.arc         archive 3         the directory; fgnpcdata.bin, message_data_table.bin, the house walls'
//                                            and floors' palettes, and the 41 banners' and icons' CResTIMG headers
//                                            (.bti)
//   famicom.arc            archive 1         the directory only
//   audiorom.img           audiorom 1        the banks laid out for the host (audio.hpp)
//   audiorom.tbl           audiorom-table 1  the host image's bank and data tables, from audiorom.img
sl_result add_entries(sl_import* imp, const char** failed);

} // namespace ac

#endif

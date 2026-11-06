/* The package ac-import writes (sonarlib docs/roadmap.md, M6 step 6c): each file the game opens from boot to Town's
   first frame, at the game's own name for it. */
#ifndef AC_IMPORTER_IMPORT_HPP
#define AC_IMPORTER_IMPORT_HPP

#include <sonar/import.h>

#include <cstdint>

namespace ac {

inline constexpr const char* importer_id = "ac";
inline constexpr uint32_t importer_version = 1;

// Adds the package's entries, the table below, to an import. On a failure, *failed is the path of the entry whose
// sl_import_add failed.
//
//   COPYDATE, static.str   copy 1     the disc's bytes
//   forest_1st.arc         archive 1  the directory; fgdata.bin, string_data_table.bin, pallet_boy.bin, face_boy.bin
//   forest_2nd.arc         archive 1  the directory; message_data_table.bin
//   famicom.arc            archive 1  the directory only
sl_result add_entries(sl_import* imp, const char** failed);

} // namespace ac

#endif

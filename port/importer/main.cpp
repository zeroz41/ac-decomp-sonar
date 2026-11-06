/* ac-import <source> <package> <cache>: writes the package the game reads from boot to Town's first frame (import.hpp)
   from the player's disc, an image or a directory in sl-disc's extracted layout, reusing what an earlier run left in
   the cache. */
#include "import.hpp"

#include <sonar/sonar.h>

#include <cinttypes>
#include <cstdio>

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: ac-import <source> <package> <cache>\n");
        return 2;
    }
    const sl_import_desc desc = { argv[1], argv[2], argv[3], ac::importer_id, ac::importer_version };
    sl_import* imp = nullptr;
    sl_result result = sl_import_begin(&desc, &imp);
    if (result != SL_OK) {
        std::fprintf(stderr, "ac-import: begin: %s\n", sl_result_string(result));
        return 1;
    }
    const char* failed = nullptr;
    result = ac::add_entries(imp, &failed);
    if (result != SL_OK) {
        std::fprintf(stderr, "ac-import: %s: %s\n", failed, sl_result_string(result));
        sl_import_abort(imp);
        return 1;
    }
    sl_import_stats stats = {};
    // finish frees the import whether or not it succeeds.
    result = sl_import_finish(imp, &stats);
    if (result != SL_OK) {
        std::fprintf(stderr, "ac-import: finish: %s\n", sl_result_string(result));
        return 1;
    }
    std::printf("entries %" PRIu32 " converted %" PRIu32 " reused %" PRIu32 " size %" PRIu64 "\n", stats.entries,
                stats.converted, stats.reused, stats.package_size);
    return 0;
}

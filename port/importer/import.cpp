/* The package ac-import writes (import.hpp). Each entry reads its whole disc file. */
#include "import.hpp"

#include "archive.hpp"
#include "audio.hpp"

#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

namespace ac {
namespace {

// archive's user: the entry's rules.
struct ArchiveRules {
    const ContentRule* rules;
    size_t count;
};

constexpr ContentRule forest_1st_rules[] = {
    { "fgdata.bin", Content::fg_data },
    { "string_data_table.bin", Content::u32_table },
    { "pallet_boy.bin", Content::palettes },
    { "face_boy.bin", Content::face },
};
constexpr ContentRule forest_2nd_rules[] = {
    { "message_data_table.bin", Content::u32_table },
};

ArchiveRules forest_1st = { forest_1st_rules, std::size(forest_1st_rules) };
ArchiveRules forest_2nd = { forest_2nd_rules, std::size(forest_2nd_rules) };
ArchiveRules famicom = { nullptr, 0 };

// The disc's bytes as they are.
sl_result copy(void* user, const void* const* inputs, const size_t* sizes, sl_import_output* out) {
    (void)user;
    return sl_import_write(out, inputs[0], sizes[0]);
}

// The archive laid out again for the host, the user's rules' files converted.
sl_result archive(void* user, const void* const* inputs, const size_t* sizes, sl_import_output* out) {
    const ArchiveRules* rules = static_cast<const ArchiveRules*>(user);
    std::vector<uint8_t> host;
    const sl_result result =
        convert_archive(static_cast<const uint8_t*>(inputs[0]), sizes[0], rules->rules, rules->count, host);
    if (result != SL_OK) {
        return result;
    }
    return sl_import_write(out, host.data(), host.size());
}

// audiorom.img laid out for the host, and its tables (audio.hpp); the user says which.
enum class AudioPart { image, table };
AudioPart audio_image = AudioPart::image;
AudioPart audio_table = AudioPart::table;

sl_result audiorom(void* user, const void* const* inputs, const size_t* sizes, sl_import_output* out) {
    AudioRom rom;
    std::string error;
    const sl_result result = convert_audiorom(static_cast<const uint8_t*>(inputs[0]), sizes[0], rom, error);
    if (result != SL_OK) {
        std::fprintf(stderr, "ac-import: audiorom.img: %s\n", error.c_str());
        return result;
    }
    const std::vector<uint8_t>& part = *static_cast<AudioPart*>(user) == AudioPart::image ? rom.image : rom.table;
    return sl_import_write(out, part.data(), part.size());
}

struct Entry {
    const char* path;  // the entry's path
    const char* input; // its disc file's; null for the entry's path
    const char* recipe;
    uint32_t recipe_version;
    sl_import_convert_fn convert;
    void* user;
};

const Entry entries[] = {
    { "COPYDATE", nullptr, "copy", 1, copy, nullptr },
    { "static.str", nullptr, "copy", 1, copy, nullptr },
    { "forest_1st.arc", nullptr, "archive", 1, archive, &forest_1st },
    { "forest_2nd.arc", nullptr, "archive", 1, archive, &forest_2nd },
    { "famicom.arc", nullptr, "archive", 1, archive, &famicom },
    { "audiorom.img", nullptr, "audiorom", 1, audiorom, &audio_image },
    { "audiorom.tbl", "audiorom.img", "audiorom-table", 1, audiorom, &audio_table },
};

} // namespace

sl_result add_entries(sl_import* imp, const char** failed) {
    for (const Entry& entry : entries) {
        const sl_import_input input = { entry.input != nullptr ? entry.input : entry.path, 0, 0 };
        sl_import_entry add = {};
        add.path = entry.path;
        add.inputs = &input;
        add.input_count = 1;
        add.recipe = entry.recipe;
        add.recipe_version = entry.recipe_version;
        add.convert = entry.convert;
        add.user = entry.user;
        const sl_result result = sl_import_add(imp, &add);
        if (result != SL_OK) {
            *failed = entry.path;
            return result;
        }
    }
    return SL_OK;
}

} // namespace ac

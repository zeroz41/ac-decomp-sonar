/* A JKR archive laid out again for the host (archive.hpp). The console's archive is read as JKR reads it
   (JKRAramArchive::open, JKRAramArchive.cpp:122-175; JKRCompArchive::open, JKRCompArchive.cpp:54-180), big-endian. */
#include "archive.hpp"

#include "ac.schema.h"

#include <sonar/base/endian.h>

#include <algorithm>
#include <cstring>
#include <new>
#include <stdexcept>

// JKR reads the info block at 0x20 (JKRAramArchive.cpp:150, JKRCompArchive.cpp:97).
static_assert(sizeof(JKRArchive::SArcHeader) == 0x20, "the host's archive header is the console's size");
// The game counts Town's item layers by the host's size (m_field_make.c:509).
static_assert(sizeof(mFM_fg_data_c) == AC_FGDATA_CONSOLE_SIZE, "the host's item layer is the console's size");

namespace ac {
namespace {

constexpr uint32_t signature_rarc = 0x52415243; // 'RARC'
constexpr uint32_t signature_yaz0 = 0x59617A30; // 'Yaz0'
constexpr uint32_t signature_yay0 = 0x59617930; // 'Yay0'
constexpr uint32_t type_data = 0x44415441;      // 'DATA'
constexpr uint32_t header_size = AC_ARCHEADER_CONSOLE_SIZE;
constexpr uint32_t face_texels = 0xE00; // a face record's texels
constexpr uint32_t face_record = 0xE20; // its texels and its palette (m_player_lib.c:981-1010)

uint64_t round32(uint64_t value) {
    return (value + 31) & ~uint64_t{ 31 };
}

// Whether `length` bytes at `offset` lie within `limit` bytes.
bool within(uint64_t offset, uint64_t length, uint64_t limit) {
    return offset <= limit && length <= limit - offset;
}

// A name as CArcName::store keeps it: lower case, with its hash (JKRArchivePri.cpp:158).
struct ArcName {
    char string[0x101];
    uint16_t hash;
};

void store_name(const char* name, ArcName& out) {
    out.hash = 0;
    size_t count = 0;
    for (; *name != '\0'; name++) {
        int lower = static_cast<unsigned char>(*name);
        if (lower >= 'A' && lower <= 'Z') {
            lower += 'a' - 'A';
        }
        out.hash = static_cast<uint16_t>(lower + out.hash * 3);
        if (count < 0x100) {
            out.string[count++] = static_cast<char>(lower);
        }
    }
    out.string[count] = '\0';
}

sl_result swap_palette_run(uint8_t* data, size_t size) {
    if (size % 2 != 0) {
        return SL_ERROR_IO;
    }
    sl_bswap16_array(data, size / 2);
    return SL_OK;
}

sl_result convert_file(uint8_t* data, size_t size, Content content) {
    switch (content) {
        case Content::fg_data:
            for (size_t at = 0; size - at >= AC_FGDATA_CONSOLE_SIZE; at += AC_FGDATA_CONSOLE_SIZE) {
                mFM_fg_data_c element;
                ac_FgData_from_console(&element, data + at);
                std::memcpy(data + at, &element, sizeof(element));
            }
            return SL_OK;
        case Content::u32_table:
            sl_bswap32_array(data, size / 4);
            return SL_OK;
        case Content::palettes:
            return swap_palette_run(data, size);
        case Content::face: {
            if (size < face_texels) {
                return SL_ERROR_IO;
            }
            // Record k's texels are kept; the bytes between them and after the last are palettes.
            size_t palette = face_texels;
            for (size_t k = 1; k * face_record + face_texels <= size; k++) {
                if (swap_palette_run(data + palette, k * face_record - palette) != SL_OK) {
                    return SL_ERROR_IO;
                }
                palette = k * face_record + face_texels;
            }
            return swap_palette_run(data + palette, size - palette);
        }
    }
    return SL_ERROR_IO;
}

sl_result convert(const uint8_t* console, size_t size, const ContentRule* rules, size_t rule_count,
                  std::vector<uint8_t>& out) {
    if (size >= 4) {
        const uint32_t magic = sl_load_be32(console);
        if (magic == signature_yaz0 || magic == signature_yay0) {
            return SL_ERROR_UNSUPPORTED;
        }
    }
    if (size < header_size) {
        return SL_ERROR_IO;
    }
    JKRArchive::SArcHeader header;
    ac_ArcHeader_from_console(&header, console);
    if (header.signature != signature_rarc || header.header_length != header_size) {
        return SL_ERROR_IO;
    }
    if (!within(header_size, std::max<uint64_t>(header.file_data_offset, AC_ARCDATAINFO_CONSOLE_SIZE), size) ||
        !within(uint64_t{ header_size } + header.file_data_offset, header.file_data_length, size)) {
        return SL_ERROR_IO;
    }
    const uint8_t* block = console + header_size;
    JKRArchive::SArcDataInfo info;
    ac_ArcDataInfo_from_console(&info, block);
    if (!within(uint64_t{ header_size } + info.node_offset, uint64_t{ info.num_nodes } * AC_DIRENTRY_CONSOLE_SIZE,
                size) ||
        !within(uint64_t{ header_size } + info.file_entry_offset,
                uint64_t{ info.num_file_entries } * AC_FILEENTRY_CONSOLE_SIZE, size) ||
        !within(uint64_t{ header_size } + info.string_table_offset, info.string_table_length, size)) {
        return SL_ERROR_IO;
    }

    std::vector<JKRArchive::SDIDirEntry> nodes(info.num_nodes);
    for (uint32_t i = 0; i < info.num_nodes; i++) {
        ac_DirEntry_from_console(&nodes[i], block + info.node_offset + uint64_t{ i } * AC_DIRENTRY_CONSOLE_SIZE);
        if (uint64_t{ nodes[i].mFirstIdx } + nodes[i].mNum > info.num_file_entries) {
            return SL_ERROR_IO;
        }
    }
    const char* strings = reinterpret_cast<const char*>(block + info.string_table_offset);
    std::vector<JKRArchive::SDIFileEntry> entries(info.num_file_entries);
    for (uint32_t i = 0; i < info.num_file_entries; i++) {
        ac_FileEntry_from_console(&entries[i],
                                  block + info.file_entry_offset + uint64_t{ i } * AC_FILEENTRY_CONSOLE_SIZE);
        const uint32_t name = entries[i].getNameOffset();
        if (name >= info.string_table_length ||
            std::memchr(strings + name, '\0', info.string_table_length - name) == nullptr) {
            return SL_ERROR_IO;
        }
    }

    // The host's layout.
    const uint64_t a = round32(sizeof(JKRArchive::SArcDataInfo));
    const uint64_t b = round32(a + nodes.size() * sizeof(JKRArchive::SDIDirEntry));
    const uint64_t c = round32(b + entries.size() * sizeof(JKRArchive::SDIFileEntry));
    const uint64_t d = round32(c + info.string_table_length);
    const uint64_t data_at = header_size + d;
    const uint64_t total = data_at + round32(header.file_data_length);

    const uint8_t* console_data = block + header.file_data_offset;
    const uint32_t data_length = header.file_data_length;

    out.assign(static_cast<size_t>(total), 0);
    uint8_t* host = out.data();
    uint8_t* host_block = host + header_size;
    header.file_data_offset = static_cast<uint32_t>(d);
    header.file_length = static_cast<uint32_t>(total);
    std::memcpy(host, &header, sizeof(header));
    info.node_offset = static_cast<uint32_t>(a);
    info.file_entry_offset = static_cast<uint32_t>(b);
    info.string_table_offset = static_cast<uint32_t>(c);
    std::memcpy(host_block, &info, sizeof(info));
    if (!nodes.empty()) {
        std::memcpy(host_block + a, nodes.data(), nodes.size() * sizeof(nodes[0]));
    }
    if (!entries.empty()) {
        std::memcpy(host_block + b, entries.data(), entries.size() * sizeof(entries[0]));
    }
    std::memcpy(host_block + c, strings, info.string_table_length);
    uint8_t* host_data = host + data_at;
    std::memcpy(host_data, console_data, data_length);

    // Each rule's file, as the game's lookup finds it (findTypeResource, JKRArchivePri.cpp:73).
    const auto data_node = std::find_if(nodes.begin(), nodes.end(),
                                        [](const JKRArchive::SDIDirEntry& node) { return node.mType == type_data; });
    for (size_t r = 0; r < rule_count; r++) {
        ArcName name;
        store_name(rules[r].name, name);
        const JKRArchive::SDIFileEntry* file = nullptr;
        if (data_node != nodes.end()) {
            for (uint32_t i = 0; i < data_node->mNum; i++) {
                const JKRArchive::SDIFileEntry& entry = entries[data_node->mFirstIdx + i];
                if (entry.mHash == name.hash && std::strcmp(strings + entry.getNameOffset(), name.string) == 0) {
                    file = &entry;
                    break;
                }
            }
        }
        if (file == nullptr || file->isDirectory() || file->isCompressed() ||
            !within(file->mDataOffset, file->mSize, data_length)) {
            return SL_ERROR_IO;
        }
        if (convert_file(host_data + file->mDataOffset, file->mSize, rules[r].content) != SL_OK) {
            return SL_ERROR_IO;
        }
    }
    return SL_OK;
}

} // namespace

sl_result convert_archive(const uint8_t* console, size_t size, const ContentRule* rules, size_t rule_count,
                          std::vector<uint8_t>& out) {
    try {
        return convert(console, size, rules, rule_count, out);
    } catch (const std::bad_alloc&) {
        // No exception crosses sonar::import's callback.
        return SL_ERROR_OUT_OF_MEMORY;
    } catch (const std::length_error&) {
        // A size past the vector's largest.
        return SL_ERROR_OUT_OF_MEMORY;
    }
}

} // namespace ac

/* The checks both importer tests run on an archive ac-import converted (importer/archive.hpp): the host's archive
   against the console's, read here big-endian rather than through the importer's code. Every node, entry, string and
   data byte is checked. */
#ifndef AC_TESTS_IMPORT_CHECKS_HPP
#define AC_TESTS_IMPORT_CHECKS_HPP

#include "../importer/archive.hpp"

#include "JSystem/JKernel/JKRArchive.h"
#include "m_field_make.h"

#include <sonar/base/endian.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace ac_test {

inline int failures = 0;

// Reports a failed check: at most 50, so a broken archive does not flood the log.
inline void check(bool ok, const char* what, const char* where, size_t at) {
    if (!ok) {
        if (failures < 50) {
            std::printf("%s @0x%zx: %s\n", where, at, what);
        }
        failures++;
    }
}

#define AC_CHECK(cond, where, at) ac_test::check((cond), #cond, (where), (at))

inline uint64_t round32(uint64_t value) {
    return (value + 31) & ~uint64_t{ 31 };
}

template <typename T> T load(const uint8_t* p) {
    T value;
    std::memcpy(&value, p, sizeof(value));
    return value;
}

// A converted file's 16-bit values in the host's order, from `begin` to `end`.
inline void check_swapped16(const uint8_t* console, const uint8_t* host, size_t begin, size_t end, const char* name) {
    for (size_t at = begin; at + 2 <= end; at += 2) {
        AC_CHECK(load<uint16_t>(host + at) == sl_load_be16(console + at), name, at);
    }
}

inline void check_same(const uint8_t* console, const uint8_t* host, size_t begin, size_t end, const char* name) {
    for (size_t at = begin; at < end; at++) {
        AC_CHECK(host[at] == console[at], name, at);
    }
}

// A rule's file, its content converted: each value equal to the console's read big-endian, the rest kept.
inline void check_file(const uint8_t* console, const uint8_t* host, size_t size, ac::Content content,
                       const char* name) {
    switch (content) {
        case ac::Content::fg_data: {
            size_t at = 0;
            for (; size - at >= 0x206; at += 0x206) {
                const mFM_fg_data_c element = load<mFM_fg_data_c>(host + at);
                AC_CHECK(element.fg_id == sl_load_be16(console + at), name, at);
                for (int z = 0; z < 16; z++) {
                    for (int x = 0; x < 16; x++) {
                        AC_CHECK(element.items[z][x] == sl_load_be16(console + at + 2 + (z * 16 + x) * 2), name, at);
                    }
                }
                AC_CHECK(std::memcmp(element.haniwa_step, console + at + 0x202, 4) == 0, name, at);
            }
            check_same(console, host, at, size, name);
            break;
        }
        case ac::Content::u32_table: {
            size_t at = 0;
            for (; at + 4 <= size; at += 4) {
                AC_CHECK(load<uint32_t>(host + at) == sl_load_be32(console + at), name, at);
            }
            check_same(console, host, at, size, name);
            break;
        }
        case ac::Content::palettes:
            AC_CHECK(size % 2 == 0, name, 0);
            check_swapped16(console, host, 0, size, name);
            break;
        case ac::Content::face: {
            // Records of 0xE00 bytes of texels and a 0x20-byte palette; the sunburn palettes follow the last.
            size_t at = 0;
            for (; at + 0xE00 <= size; at += 0xE20) {
                check_same(console, host, at, at + 0xE00, name);
                const size_t palette_end = at + 0xE20 + 0xE00 <= size ? at + 0xE20 : size;
                check_swapped16(console, host, at + 0xE00, palette_end, name);
            }
            break;
        }
    }
}

// The host's archive `host` made from the console's `console` with `rules`: the layout, every node, entry and string,
// each rule's file converted and every other data byte kept.
inline void check_archive(const char* name, const std::vector<uint8_t>& console, const std::vector<uint8_t>& host,
                          const ac::ContentRule* rules, size_t rule_count) {
    const uint8_t* c = console.data();
    if (console.size() < 0x40) {
        AC_CHECK(console.size() >= 0x40, name, 0);
        return;
    }
    const uint32_t file_data_offset = sl_load_be32(c + 0x0C);
    const uint32_t file_data_length = sl_load_be32(c + 0x10);
    const uint8_t* block = c + 0x20;
    const uint32_t num_nodes = sl_load_be32(block + 0x00);
    const uint32_t node_offset = sl_load_be32(block + 0x04);
    const uint32_t num_entries = sl_load_be32(block + 0x08);
    const uint32_t entry_offset = sl_load_be32(block + 0x0C);
    const uint32_t strings_length = sl_load_be32(block + 0x10);
    const uint32_t strings_offset = sl_load_be32(block + 0x14);
    const uint64_t console_end = uint64_t{ 0x20 } + file_data_offset + file_data_length;
    if (console_end > console.size() || 0x20 + uint64_t{ node_offset } + num_nodes * 0x10ull > console.size() ||
        0x20 + uint64_t{ entry_offset } + num_entries * 0x14ull > console.size() ||
        0x20 + uint64_t{ strings_offset } + strings_length > console.size()) {
        AC_CHECK(console_end <= console.size(), name, 0);
        AC_CHECK(!"the console's tables lie within it", name, 0);
        return;
    }

    // The host's layout.
    const uint64_t a = round32(sizeof(JKRArchive::SArcDataInfo));
    const uint64_t b = round32(a + num_nodes * sizeof(JKRArchive::SDIDirEntry));
    const uint64_t e = round32(b + num_entries * sizeof(JKRArchive::SDIFileEntry));
    const uint64_t d = round32(e + strings_length);
    const uint64_t data_at = 0x20 + d;
    const uint64_t total = data_at + round32(file_data_length);
    AC_CHECK(host.size() == total, name, 0);
    if (host.size() != total) {
        return;
    }
    const uint8_t* h = host.data();
    const uint8_t* host_block = h + 0x20;

    const JKRArchive::SArcHeader header = load<JKRArchive::SArcHeader>(h);
    AC_CHECK(header.signature == sl_load_be32(c + 0x00), name, 0x00);
    AC_CHECK(header.file_length == total, name, 0x04);
    AC_CHECK(header.header_length == 0x20, name, 0x08);
    AC_CHECK(header.file_data_offset == d, name, 0x0C);
    AC_CHECK(header.file_data_length == file_data_length, name, 0x10);
    AC_CHECK(header._14 == sl_load_be32(c + 0x14), name, 0x14);
    AC_CHECK(header._18 == sl_load_be32(c + 0x18), name, 0x18);
    AC_CHECK(header._1C == sl_load_be32(c + 0x1C), name, 0x1C);

    const JKRArchive::SArcDataInfo info = load<JKRArchive::SArcDataInfo>(host_block);
    AC_CHECK(info.num_nodes == num_nodes, name, 0x20);
    AC_CHECK(info.node_offset == a, name, 0x24);
    AC_CHECK(info.num_file_entries == num_entries, name, 0x28);
    AC_CHECK(info.file_entry_offset == b, name, 0x2C);
    AC_CHECK(info.string_table_length == strings_length, name, 0x30);
    AC_CHECK(info.string_table_offset == e, name, 0x34);
    AC_CHECK(info.nextFreeFileID == sl_load_be16(block + 0x18), name, 0x38);
    AC_CHECK(info.isSyncIDs == (block[0x1A] != 0), name, 0x3A);
    AC_CHECK(std::memcmp(info._1B, block + 0x1B, 5) == 0, name, 0x3B);

    // Every byte of the info block outside its structures is zero.
    std::vector<bool> used(d, false);
    const auto use = [&used](uint64_t begin, uint64_t size) {
        for (uint64_t i = begin; i < begin + size; i++) {
            used[i] = true;
        }
    };
    use(0, sizeof(JKRArchive::SArcDataInfo));
    use(a, num_nodes * sizeof(JKRArchive::SDIDirEntry));
    use(b, num_entries * sizeof(JKRArchive::SDIFileEntry));
    use(e, strings_length);
    for (uint64_t i = 0; i < d; i++) {
        if (!used[i]) {
            AC_CHECK(host_block[i] == 0, name, 0x20 + i);
        }
    }

    std::vector<JKRArchive::SDIDirEntry> nodes(num_nodes);
    for (uint32_t i = 0; i < num_nodes; i++) {
        const uint8_t* node = block + node_offset + i * 0x10;
        nodes[i] = load<JKRArchive::SDIDirEntry>(host_block + a + i * sizeof(JKRArchive::SDIDirEntry));
        const size_t at = 0x20 + a + i * sizeof(JKRArchive::SDIDirEntry);
        AC_CHECK(nodes[i].mType == sl_load_be32(node + 0x00), name, at);
        AC_CHECK(nodes[i].mOffset == sl_load_be32(node + 0x04), name, at);
        AC_CHECK(nodes[i]._08 == sl_load_be16(node + 0x08), name, at);
        AC_CHECK(nodes[i].mNum == sl_load_be16(node + 0x0A), name, at);
        AC_CHECK(nodes[i].mFirstIdx == sl_load_be32(node + 0x0C), name, at);
    }

    std::vector<JKRArchive::SDIFileEntry> entries(num_entries);
    for (uint32_t i = 0; i < num_entries; i++) {
        const uint8_t* entry = block + entry_offset + i * 0x14;
        entries[i] = load<JKRArchive::SDIFileEntry>(host_block + b + i * sizeof(JKRArchive::SDIFileEntry));
        const JKRArchive::SDIFileEntry& host_entry = entries[i];
        const size_t at = 0x20 + b + i * sizeof(JKRArchive::SDIFileEntry);
        AC_CHECK(host_entry.mFileID == sl_load_be16(entry + 0x00), name, at);
        AC_CHECK(host_entry.mHash == sl_load_be16(entry + 0x02), name, at);
        AC_CHECK(host_entry.mFlag == sl_load_be32(entry + 0x04), name, at);
        AC_CHECK(host_entry.mDataOffset == sl_load_be32(entry + 0x08), name, at);
        AC_CHECK(host_entry.mSize == sl_load_be32(entry + 0x0C), name, at);
        AC_CHECK(host_entry.mData == nullptr, name, at);
        AC_CHECK(host_entry.getNameOffset() == (sl_load_be32(entry + 0x04) & 0xFFFFFF), name, at);
        AC_CHECK(host_entry.getFlags() == entry[0x04], name, at);
        AC_CHECK(host_entry.isDirectory() == ((entry[0x04] & 0x02) != 0), name, at);
    }

    check_same(block + strings_offset, host_block + e, 0, strings_length, name);

    // The data: each rule's file converted, every other byte kept, zero bytes after.
    const uint8_t* console_data = block + file_data_offset;
    const uint8_t* host_data = h + data_at;
    const char* strings = reinterpret_cast<const char*>(block + strings_offset);
    std::vector<bool> converted(file_data_length, false);
    for (size_t r = 0; r < rule_count; r++) {
        const JKRArchive::SDIFileEntry* file = nullptr;
        for (const JKRArchive::SDIDirEntry& node : nodes) {
            if (node.mType != 0x44415441) { // 'DATA'
                continue;
            }
            for (uint32_t i = node.mFirstIdx; i < node.mFirstIdx + node.mNum && i < num_entries; i++) {
                const uint32_t offset = entries[i].getNameOffset();
                if (offset < strings_length && std::strcmp(strings + offset, rules[r].name) == 0) {
                    file = &entries[i];
                    break;
                }
            }
            break;
        }
        AC_CHECK(file != nullptr, rules[r].name, 0);
        if (file == nullptr || uint64_t{ file->mDataOffset } + file->mSize > file_data_length) {
            AC_CHECK(file == nullptr || uint64_t{ file->mDataOffset } + file->mSize <= file_data_length, rules[r].name,
                     0);
            continue;
        }
        check_file(console_data + file->mDataOffset, host_data + file->mDataOffset, file->mSize, rules[r].content,
                   rules[r].name);
        for (uint32_t i = 0; i < file->mSize; i++) {
            converted[file->mDataOffset + i] = true;
        }
    }
    for (uint32_t i = 0; i < file_data_length; i++) {
        if (!converted[i]) {
            AC_CHECK(host_data[i] == console_data[i], name, data_at + i);
        }
    }
    for (uint64_t i = data_at + file_data_length; i < total; i++) {
        AC_CHECK(h[i] == 0, name, i);
    }
}

} // namespace ac_test

#endif

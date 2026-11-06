/* JSystem's font in the host's byte order (font.hpp). The console's font is read as JUTResFont reads it
   (JUTResFont::countBlock and setBlock, JUTResFont.cpp:97-170), big-endian. */
#include "font.hpp"

#include <sonar/base/endian.h>

#include <cstring>
#include <new>

namespace ac {
namespace {

constexpr char font_magic[8] = { 'F', 'O', 'N', 'T', 'b', 'f', 'n', '1' };
constexpr size_t header_size = 0x20;        // ResFONT's mData
constexpr size_t block_header = 0x08;       // BlockHeader
constexpr uint32_t magic_inf1 = 0x494E4631; // 'INF1'
constexpr uint32_t magic_wid1 = 0x57494431; // 'WID1'
constexpr uint32_t magic_gly1 = 0x474C5931; // 'GLY1'
constexpr uint32_t magic_map1 = 0x4D415031; // 'MAP1'

void to_host16(uint8_t* p) {
    const uint16_t value = sl_load_be16(p);
    std::memcpy(p, &value, sizeof value);
}

void to_host32(uint8_t* p) {
    const uint32_t value = sl_load_be32(p);
    std::memcpy(p, &value, sizeof value);
}

// The u16 fields from `begin` to `end` of a block.
void to_host16_run(uint8_t* block, size_t begin, size_t end) {
    for (size_t at = begin; at + 2 <= end; at += 2) {
        to_host16(block + at);
    }
}

// Converts one block's fields, `size` its console size; false for a block too short for its fields.
bool convert_block(uint8_t* block, uint32_t magic, uint32_t size) {
    switch (magic) {
        case magic_inf1:
            if (size < 0x14) {
                return false;
            }
            to_host16_run(block, 0x08, 0x14);
            return true;
        case magic_wid1:
            if (size < 0x0C) {
                return false;
            }
            to_host16_run(block, 0x08, 0x0C);
            return true;
        case magic_gly1:
            if (size < 0x20) {
                return false;
            }
            to_host16_run(block, 0x08, 0x10);
            to_host32(block + 0x10);
            to_host16_run(block, 0x14, 0x20);
            return true;
        case magic_map1:
            if ((size - block_header) % 2 != 0) {
                return false;
            }
            to_host16_run(block, 0x08, size);
            return true;
        default:
            return false;
    }
}

} // namespace

sl_result convert_font(const uint8_t* console, size_t size, std::vector<uint8_t>& out) {
    if (console == nullptr || size < header_size || std::memcmp(console, font_magic, sizeof font_magic) != 0) {
        return SL_ERROR_IO;
    }
    const uint32_t block_count = sl_load_be32(console + 0x0C);
    try {
        out.assign(console, console + size);
    } catch (const std::bad_alloc&) { return SL_ERROR_OUT_OF_MEMORY; }
    uint8_t* const host = out.data();
    to_host32(host + 0x08);
    to_host32(host + 0x0C);
    uint32_t blocks = 0;
    size_t at = header_size;
    while (at < size) {
        if (size - at < block_header) {
            return SL_ERROR_IO;
        }
        const uint32_t magic = sl_load_be32(console + at);
        const uint32_t block_size = sl_load_be32(console + at + 4);
        if (block_size < block_header || block_size > size - at) {
            return SL_ERROR_IO;
        }
        to_host32(host + at);
        to_host32(host + at + 4);
        if (!convert_block(host + at, magic, block_size)) {
            return SL_ERROR_IO;
        }
        blocks++;
        at += block_size;
    }
    if (blocks != block_count) {
        return SL_ERROR_IO;
    }
    return SL_OK;
}

} // namespace ac

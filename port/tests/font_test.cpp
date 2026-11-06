/* Checks ac-font's conversion (importer/font.hpp) on a font built byte by byte, worked out by hand: each field in the
   host's order, the magic, padding, widths and texels kept, and each refusal. */
#include "../importer/font.hpp"

#include <sonar/base/endian.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what, size_t at) {
    if (!ok) {
        std::printf("@0x%zx: %s\n", at, what);
        failures++;
    }
}

#define FONT_CHECK(cond, at) check((cond), #cond, (at))

// The console's font:
//   0x00  the header: FONTbfn1, mFileSize 0xA6, mNumBlocks 4, 0x10 bytes of padding
//   0x20  INF1, 0x20 bytes: six u16, then padding
//   0x40  WID1, 0x10 bytes: start and end codes, four widths
//   0x50  GLY1, 0x40 bytes: its fields, then 0x20 bytes of texels
//   0x90  MAP1, 0x16 bytes: five u16, then two entries
constexpr size_t inf1_at = 0x20, wid1_at = 0x40, gly1_at = 0x50, map1_at = 0x90, font_size = 0xA6;

void put16(std::vector<uint8_t>& bytes, size_t at, uint16_t value) {
    sl_store_be16(bytes.data() + at, value);
}

void put32(std::vector<uint8_t>& bytes, size_t at, uint32_t value) {
    sl_store_be32(bytes.data() + at, value);
}

std::vector<uint8_t> console_font() {
    std::vector<uint8_t> bytes(font_size, 0);
    std::memcpy(bytes.data(), "FONTbfn1", 8);
    put32(bytes, 0x08, font_size);
    put32(bytes, 0x0C, 4);
    for (size_t at = 0x10; at < 0x20; at++) {
        bytes[at] = static_cast<uint8_t>(0xE0 + at); // padding, kept
    }
    put32(bytes, inf1_at + 0x00, 0x494E4631); // 'INF1'
    put32(bytes, inf1_at + 0x04, 0x20);
    put16(bytes, inf1_at + 0x08, 0x0001);     // mFontType
    put16(bytes, inf1_at + 0x0A, 0x0A0B);     // mAscent
    put16(bytes, inf1_at + 0x0C, 0x0C0D);     // mDescent
    put16(bytes, inf1_at + 0x0E, 0x0E0F);     // mWidth
    put16(bytes, inf1_at + 0x10, 0x1011);     // mLeading
    put16(bytes, inf1_at + 0x12, 0x1213);     // mDefaultCode
    put32(bytes, wid1_at + 0x00, 0x57494431); // 'WID1'
    put32(bytes, wid1_at + 0x04, 0x10);
    put16(bytes, wid1_at + 0x08, 0x2021); // mStartCode
    put16(bytes, wid1_at + 0x0A, 0x2223); // mEndCode
    bytes[wid1_at + 0x0C] = 0x31;         // the widths, bytes
    bytes[wid1_at + 0x0D] = 0x32;
    bytes[wid1_at + 0x0E] = 0x33;
    bytes[wid1_at + 0x0F] = 0x34;
    put32(bytes, gly1_at + 0x00, 0x474C5931); // 'GLY1'
    put32(bytes, gly1_at + 0x04, 0x40);
    put16(bytes, gly1_at + 0x08, 0x4041);     // mStartCode
    put16(bytes, gly1_at + 0x0A, 0x4243);     // mEndCode
    put16(bytes, gly1_at + 0x0C, 0x4445);     // mCellWidth
    put16(bytes, gly1_at + 0x0E, 0x4647);     // mCellHeight
    put32(bytes, gly1_at + 0x10, 0x48494A4B); // mTextureSize
    put16(bytes, gly1_at + 0x14, 0x4C4D);     // mTextureFormat
    put16(bytes, gly1_at + 0x16, 0x4E4F);     // mNumRows
    put16(bytes, gly1_at + 0x18, 0x5051);     // mNumColumns
    put16(bytes, gly1_at + 0x1A, 0x5253);     // mTextureWidth
    put16(bytes, gly1_at + 0x1C, 0x5455);     // mTextureHeight
    put16(bytes, gly1_at + 0x1E, 0x5657);     // mPadding
    for (size_t at = 0x20; at < 0x40; at++) {
        bytes[gly1_at + at] = static_cast<uint8_t>(0x80 + at); // texels, kept
    }
    put32(bytes, map1_at + 0x00, 0x4D415031); // 'MAP1'
    put32(bytes, map1_at + 0x04, 0x16);
    put16(bytes, map1_at + 0x08, 0x6061); // mMappingMethod
    put16(bytes, map1_at + 0x0A, 0x6263); // mStartCode
    put16(bytes, map1_at + 0x0C, 0x6465); // mEndCode
    put16(bytes, map1_at + 0x0E, 0x0002); // mNumEntries
    put16(bytes, map1_at + 0x10, 0x6869); // mLeading
    put16(bytes, map1_at + 0x12, 0x6A6B); // the two entries
    put16(bytes, map1_at + 0x14, 0x6C6D);
    return bytes;
}

template <typename T> T load(const std::vector<uint8_t>& bytes, size_t at) {
    T value;
    std::memcpy(&value, bytes.data() + at, sizeof value);
    return value;
}

// Each u16 the hand-built font holds at `at` in the host's order.
void check16(const std::vector<uint8_t>& host, size_t at, uint16_t value) {
    FONT_CHECK(load<uint16_t>(host, at) == value, at);
}

void check32(const std::vector<uint8_t>& host, size_t at, uint32_t value) {
    FONT_CHECK(load<uint32_t>(host, at) == value, at);
}

void check_kept(const std::vector<uint8_t>& console, const std::vector<uint8_t>& host, size_t begin, size_t end) {
    for (size_t at = begin; at < end; at++) {
        FONT_CHECK(host[at] == console[at], at);
    }
}

void check_converted() {
    const std::vector<uint8_t> console = console_font();
    std::vector<uint8_t> host;
    FONT_CHECK(ac::convert_font(console.data(), console.size(), host) == SL_OK, 0);
    FONT_CHECK(host.size() == font_size, 0);
    if (host.size() != font_size) {
        return;
    }
    check_kept(console, host, 0x00, 0x08); // the magic
    check32(host, 0x08, font_size);
    check32(host, 0x0C, 4);
    check_kept(console, host, 0x10, 0x20); // the padding

    check32(host, inf1_at + 0x00, 0x494E4631);
    check32(host, inf1_at + 0x04, 0x20);
    check16(host, inf1_at + 0x08, 0x0001);
    check16(host, inf1_at + 0x0A, 0x0A0B);
    check16(host, inf1_at + 0x0C, 0x0C0D);
    check16(host, inf1_at + 0x0E, 0x0E0F);
    check16(host, inf1_at + 0x10, 0x1011);
    check16(host, inf1_at + 0x12, 0x1213);
    check_kept(console, host, inf1_at + 0x14, wid1_at);

    check32(host, wid1_at + 0x00, 0x57494431);
    check32(host, wid1_at + 0x04, 0x10);
    check16(host, wid1_at + 0x08, 0x2021);
    check16(host, wid1_at + 0x0A, 0x2223);
    check_kept(console, host, wid1_at + 0x0C, gly1_at); // the widths

    check32(host, gly1_at + 0x00, 0x474C5931);
    check32(host, gly1_at + 0x04, 0x40);
    check16(host, gly1_at + 0x08, 0x4041);
    check16(host, gly1_at + 0x0A, 0x4243);
    check16(host, gly1_at + 0x0C, 0x4445);
    check16(host, gly1_at + 0x0E, 0x4647);
    check32(host, gly1_at + 0x10, 0x48494A4B);
    check16(host, gly1_at + 0x14, 0x4C4D);
    check16(host, gly1_at + 0x16, 0x4E4F);
    check16(host, gly1_at + 0x18, 0x5051);
    check16(host, gly1_at + 0x1A, 0x5253);
    check16(host, gly1_at + 0x1C, 0x5455);
    check16(host, gly1_at + 0x1E, 0x5657);
    check_kept(console, host, gly1_at + 0x20, map1_at); // the texels

    check32(host, map1_at + 0x00, 0x4D415031);
    check32(host, map1_at + 0x04, 0x16);
    check16(host, map1_at + 0x08, 0x6061);
    check16(host, map1_at + 0x0A, 0x6263);
    check16(host, map1_at + 0x0C, 0x6465);
    check16(host, map1_at + 0x0E, 0x0002);
    check16(host, map1_at + 0x10, 0x6869);
    check16(host, map1_at + 0x12, 0x6A6B);
    check16(host, map1_at + 0x14, 0x6C6D);
}

void check_refused(const std::vector<uint8_t>& console, const char* what) {
    std::vector<uint8_t> host;
    const sl_result result = ac::convert_font(console.data(), console.size(), host);
    if (result != SL_ERROR_IO) {
        std::printf("%s: %d, not SL_ERROR_IO\n", what, static_cast<int>(result));
        failures++;
    }
}

void check_refusals() {
    std::vector<uint8_t> bytes = console_font();
    bytes[7] = '2';
    check_refused(bytes, "a magic other than FONTbfn1");

    bytes = console_font();
    put32(bytes, map1_at + 0x04, 0x18);
    check_refused(bytes, "a block past the file");

    bytes = console_font();
    put32(bytes, 0x0C, 3);
    check_refused(bytes, "a block count other than the blocks'");

    bytes = console_font();
    put32(bytes, wid1_at + 0x00, 0x57494432); // 'WID2'
    check_refused(bytes, "a block of another kind");

    bytes = console_font();
    bytes.pop_back();
    put32(bytes, 0x08, font_size - 1);
    put32(bytes, map1_at + 0x04, 0x15);
    check_refused(bytes, "a MAP1 of odd length");
}

} // namespace

int main() {
    check_converted();
    check_refusals();
    if (failures != 0) {
        std::printf("ac_font: %d failures\n", failures);
        return 1;
    }
    std::printf("ac_font: ok\n");
    return 0;
}

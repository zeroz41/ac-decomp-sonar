/* Checks ac-import's archive conversion (importer/archive.hpp) on a console archive built byte by byte, worked out by
   hand: the host's layout, each node and entry, the string table, each rule's file converted and every other byte
   kept, the directory alone with no rules, and each refusal. */
#include "import_checks.hpp"

#include <cstdio>
#include <iterator>
#include <vector>

namespace {

// The console's archive:
//   0x000  SArcHeader
//   0x020  the info block, 0x1C0 bytes: SArcDataInfo; 2 nodes at 0x20; 12 entries at 0x40; strings at 0x140, 0x80 long
//   0x1E0  the file data, 0x81C0 bytes:
//          0x0000 fgdata.bin             0x40F   two elements and 3 bytes
//          0x0420 string_data_table.bin  0x16    five words and 2 bytes
//          0x0440 pallet_boy.bin         0x40    two palettes
//          0x0480 face_boy.bin           0x1CA0  two records and three sunburn palettes
//          0x2120 wall.bin               0x2040  two wall records
//          0x4160 floor.bin              0x4040  two floor records
//          0x81A0 other.bin              0x10    no rule names it
constexpr size_t block = 0x20;
constexpr size_t nodes_at = block + 0x20;
constexpr size_t entries_at = block + 0x40;
constexpr size_t strings_at = block + 0x140;
constexpr size_t data_at = 0x1E0;
constexpr size_t data_size = 0x81C0;
constexpr size_t console_size = data_at + data_size;

// The string table's names and their offsets.
constexpr char strings[] =
    ".\0..\0root\0data\0fgdata.bin\0string_data_table.bin\0pallet_boy.bin\0face_boy.bin\0other.bin\0wall.bin\0"
    "floor.bin";
constexpr uint32_t name_dot = 0, name_dotdot = 2, name_root = 5, name_data = 10, name_fgdata = 15,
                   name_string_table = 26, name_pallet = 48, name_face = 63, name_other = 76, name_wall = 86,
                   name_floor = 95;

constexpr uint32_t file_attr = 0x11; // a file, loaded to main RAM
constexpr uint32_t dir_attr = 0x02;

void put16(std::vector<uint8_t>& bytes, size_t at, uint16_t value) {
    sl_store_be16(bytes.data() + at, value);
}

void put32(std::vector<uint8_t>& bytes, size_t at, uint32_t value) {
    sl_store_be32(bytes.data() + at, value);
}

// CArcName::store's hash of a lower-case name (JKRArchivePri.cpp:158).
uint16_t name_hash(uint32_t offset) {
    uint16_t hash = 0;
    for (const char* c = strings + offset; *c != '\0'; c++) {
        hash = static_cast<uint16_t>(*c + hash * 3);
    }
    return hash;
}

void put_entry(std::vector<uint8_t>& bytes, int index, uint16_t id, uint32_t attr, uint32_t name, uint32_t offset,
               uint32_t size) {
    const size_t at = entries_at + index * 0x14;
    put16(bytes, at + 0x00, id);
    put16(bytes, at + 0x02, name_hash(name));
    put32(bytes, at + 0x04, attr << 24 | name);
    put32(bytes, at + 0x08, offset);
    put32(bytes, at + 0x0C, size);
    put32(bytes, at + 0x10, 0);
}

std::vector<uint8_t> console_archive() {
    std::vector<uint8_t> bytes(console_size, 0);
    put32(bytes, 0x00, 0x52415243); // 'RARC'
    put32(bytes, 0x04, console_size);
    put32(bytes, 0x08, 0x20);
    put32(bytes, 0x0C, 0x1C0);
    put32(bytes, 0x10, data_size);
    put32(bytes, 0x14, 0x8180); // JKRCompArchive's main-RAM part
    put32(bytes, 0x18, 0x40);   // and its ARAM part

    put32(bytes, block + 0x00, 2);
    put32(bytes, block + 0x04, 0x20);
    put32(bytes, block + 0x08, 12);
    put32(bytes, block + 0x0C, 0x40);
    put32(bytes, block + 0x10, 0x80);
    put32(bytes, block + 0x14, 0x140);
    put16(bytes, block + 0x18, 12);
    bytes[block + 0x1A] = 1;
    bytes[block + 0x1B] = 0x5A;

    put32(bytes, nodes_at + 0x00, 0x524F4F54); // 'ROOT'
    put32(bytes, nodes_at + 0x04, name_root);
    put16(bytes, nodes_at + 0x08, name_hash(name_root));
    put16(bytes, nodes_at + 0x0A, 3);
    put32(bytes, nodes_at + 0x0C, 0);
    put32(bytes, nodes_at + 0x10, 0x44415441); // 'DATA'
    put32(bytes, nodes_at + 0x14, name_data);
    put16(bytes, nodes_at + 0x18, name_hash(name_data));
    put16(bytes, nodes_at + 0x1A, 9);
    put32(bytes, nodes_at + 0x1C, 3);

    put_entry(bytes, 0, 0xFFFF, dir_attr, name_data, 1, 0x10);
    put_entry(bytes, 1, 0xFFFF, dir_attr, name_dot, 0, 0x10);
    put_entry(bytes, 2, 0xFFFF, dir_attr, name_dotdot, 0xFFFFFFFF, 0x10);
    put_entry(bytes, 3, 3, file_attr, name_fgdata, 0x0000, 0x40F);
    put_entry(bytes, 4, 4, file_attr, name_string_table, 0x0420, 0x16);
    put_entry(bytes, 5, 5, file_attr, name_pallet, 0x0440, 0x40);
    put_entry(bytes, 6, 6, file_attr, name_face, 0x0480, 0x1CA0);
    put_entry(bytes, 7, 7, file_attr, name_other, 0x81A0, 0x10);
    put_entry(bytes, 8, 8, file_attr, name_wall, 0x2120, 0x2040);
    put_entry(bytes, 9, 9, file_attr, name_floor, 0x4160, 0x4040);
    put_entry(bytes, 10, 0xFFFF, dir_attr, name_dot, 1, 0x10);
    put_entry(bytes, 11, 0xFFFF, dir_attr, name_dotdot, 0, 0x10);

    std::copy(std::begin(strings), std::end(strings), bytes.begin() + strings_at);

    // The file data: byte i is i * 7 + 3, so every swap shows.
    for (size_t i = 0; i < data_size; i++) {
        bytes[data_at + i] = static_cast<uint8_t>(i * 7 + 3);
    }
    return bytes;
}

const ac::ContentRule rules[] = {
    { "fgdata.bin", ac::Content::fg_data },      { "string_data_table.bin", ac::Content::u32_table },
    { "pallet_boy.bin", ac::Content::palettes }, { "face_boy.bin", ac::Content::face },
    { "wall.bin", ac::Content::room_wall },      { "floor.bin", ac::Content::room_floor },
};

sl_result convert(const std::vector<uint8_t>& console, const ac::ContentRule* with, size_t count) {
    std::vector<uint8_t> host;
    return ac::convert_archive(console.data(), console.size(), with, count, host);
}

sl_result convert(const std::vector<uint8_t>& console) {
    return convert(console, rules, std::size(rules));
}

void check_converted() {
    const std::vector<uint8_t> console = console_archive();
    std::vector<uint8_t> host;
    AC_CHECK(ac::convert_archive(console.data(), console.size(), rules, std::size(rules), host) == SL_OK, "convert", 0);

    // The layout, by hand: A 0x20, B 0x40, C 0x160 (0x40 + 12 * 0x18), D 0x1E0, the data at 0x200.
    AC_CHECK(host.size() == 0x83C0, "layout", 0);
    if (host.size() != 0x83C0) {
        return;
    }
    const auto header = ac_test::load<JKRArchive::SArcHeader>(host.data());
    AC_CHECK(header.file_length == 0x83C0 && header.file_data_offset == 0x1E0, "layout", 0);
    AC_CHECK(header._14 == 0x8180 && header._18 == 0x40, "layout", 0);
    const auto info = ac_test::load<JKRArchive::SArcDataInfo>(host.data() + 0x20);
    AC_CHECK(info.node_offset == 0x20 && info.file_entry_offset == 0x40 && info.string_table_offset == 0x160, "layout",
             0x20);
    const auto face = ac_test::load<JKRArchive::SDIFileEntry>(host.data() + 0x20 + 0x40 + 6 * 0x18);
    AC_CHECK(face.mDataOffset == 0x480 && face.mSize == 0x1CA0 && face.getNameOffset() == name_face, "face_boy.bin", 0);
    AC_CHECK(face.getFlags() == file_attr && !face.isDirectory() && face.mData == nullptr, "face_boy.bin", 0);
    // fgdata.bin's first fg_id: data bytes 0x03 0x0A, read big-endian.
    AC_CHECK(ac_test::load<uint16_t>(host.data() + 0x200) == 0x030A, "fgdata.bin", 0);
    // string_data_table.bin's first word: data bytes at 0x420, 0x420 * 7 + 3 = 0x1CE3, so 0xE3 0xEA 0xF1 0xF8.
    AC_CHECK(ac_test::load<uint32_t>(host.data() + 0x200 + 0x420) == 0xE3EAF1F8, "string_data_table.bin", 0);
    // face_boy.bin's first texel byte is kept: 0x480 * 7 + 3 = 0x1F83.
    AC_CHECK(host[0x200 + 0x480] == 0x83, "face_boy.bin", 0);
    // wall.bin's second record at 0x3140: its first palette entry 0xC3 0xCA read big-endian, its first texel at 0x3160
    // kept, 0x3160 * 7 + 3 = 0x15AA3.
    AC_CHECK(ac_test::load<uint16_t>(host.data() + 0x200 + 0x3140) == 0xC3CA, "wall.bin", 0x1020);
    AC_CHECK(host[0x200 + 0x3160] == 0xA3, "wall.bin", 0x1040);
    // floor.bin's second record at 0x6180: 0x83 0x8A, and its first texel at 0x61A0 kept, 0x61A0 * 7 + 3 = 0x2AB63.
    AC_CHECK(ac_test::load<uint16_t>(host.data() + 0x200 + 0x6180) == 0x838A, "floor.bin", 0x2020);
    AC_CHECK(host[0x200 + 0x61A0] == 0x63, "floor.bin", 0x2040);

    ac_test::check_archive("converted", console, host, rules, std::size(rules));
    AC_CHECK(ac_test::wall_records == 2 && ac_test::floor_records == 2, "the records checked", 0);
}

void check_directory_alone() {
    const std::vector<uint8_t> console = console_archive();
    std::vector<uint8_t> host;
    AC_CHECK(ac::convert_archive(console.data(), console.size(), nullptr, 0, host) == SL_OK, "no rules", 0);
    ac_test::check_archive("no rules", console, host, nullptr, 0);
}

void check_refusals() {
    std::vector<uint8_t> bytes = console_archive();
    AC_CHECK(convert(bytes) == SL_OK, "refusals", 0);

    bytes = console_archive();
    put32(bytes, 0, 0x59617A30); // 'Yaz0'
    AC_CHECK(convert(bytes) == SL_ERROR_UNSUPPORTED, "Yaz0", 0);
    put32(bytes, 0, 0x59617930); // 'Yay0'
    AC_CHECK(convert(bytes) == SL_ERROR_UNSUPPORTED, "Yay0", 0);
    put32(bytes, 0, 0x52415244); // 'RARD'
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "signature", 0);

    bytes = console_archive();
    bytes.resize(0x10);
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "a file shorter than its header", 0);

    bytes = console_archive();
    put32(bytes, 0x08, 0x40);
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "header_length", 0x08);

    bytes = console_archive();
    put32(bytes, 0x0C, 0x10000);
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "info block past the file", 0x0C);

    bytes = console_archive();
    put32(bytes, block + 0x04, 0x10000);
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "nodes past the file", block + 0x04);
    bytes = console_archive();
    put32(bytes, block + 0x08, 0x1000);
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "entries past the file", block + 0x08);
    bytes = console_archive();
    put32(bytes, block + 0x10, 0x10000);
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "strings past the file", block + 0x10);

    bytes = console_archive();
    put32(bytes, 0x10, 0x9000);
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "file data past the file", 0x10);

    bytes = console_archive();
    put16(bytes, nodes_at + 0x1A, 10); // entries 3 to 12, of 12
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "a node's entries past the table", nodes_at + 0x10);

    bytes = console_archive();
    put32(bytes, entries_at + 7 * 0x14 + 0x04, file_attr << 24 | 0x80);
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "a name past the strings", entries_at + 7 * 0x14);

    bytes = console_archive();
    const ac::ContentRule missing[] = { { "missing.bin", ac::Content::u32_table } };
    AC_CHECK(convert(bytes, missing, 1) == SL_ERROR_IO, "a missing file", 0);
    const ac::ContentRule directory[] = { { ".", ac::Content::u32_table } };
    AC_CHECK(convert(bytes, directory, 1) == SL_ERROR_IO, "a directory", 0);
    const ac::ContentRule other[] = { { "other.bin", ac::Content::u32_table } };
    AC_CHECK(convert(bytes, other, 1) == SL_OK, "other.bin", 0);
    put32(bytes, entries_at + 7 * 0x14 + 0x0C, 0x30); // 0x81A0 + 0x30, past 0x81C0
    AC_CHECK(convert(bytes, other, 1) == SL_ERROR_IO, "data past the file data", entries_at + 7 * 0x14);

    bytes = console_archive();
    put32(bytes, entries_at + 3 * 0x14 + 0x04, (file_attr | 0x04) << 24 | name_fgdata);
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "a compressed file", entries_at + 3 * 0x14);

    bytes = console_archive();
    put32(bytes, entries_at + 5 * 0x14 + 0x0C, 0x3F);
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "palettes of odd size", entries_at + 5 * 0x14);

    bytes = console_archive();
    put32(bytes, entries_at + 6 * 0x14 + 0x0C, 0xDFF);
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "a face shorter than its texels", entries_at + 6 * 0x14);
    put32(bytes, entries_at + 6 * 0x14 + 0x0C, 0xE00 + 0x1F);
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "a face palette of odd length", entries_at + 6 * 0x14);

    bytes = console_archive();
    put32(bytes, entries_at + 8 * 0x14 + 0x0C, 0x1021);
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "a wall of one record and a byte", entries_at + 8 * 0x14);
    bytes = console_archive();
    put32(bytes, entries_at + 9 * 0x14 + 0x0C, 0x1020);
    AC_CHECK(convert(bytes) == SL_ERROR_IO, "a floor of one wall record", entries_at + 9 * 0x14);
}

} // namespace

int main() {
    check_converted();
    check_directory_alone();
    check_refusals();
    if (ac_test::failures != 0) {
        std::printf("%d failed\n", ac_test::failures);
        return 1;
    }
    std::printf("ac_import_archive: passed\n");
    return 0;
}

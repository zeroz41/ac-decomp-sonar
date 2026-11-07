/* Checks ac-import on the player's disc (sonarlib docs/roadmap.md, M6 step 6c):
     ac_import_disc_test <ac-import> <disc> <version>
   The disc is an image or a directory in sl-disc's extracted layout; with none the test is skipped (77). The disc's ID
   and revision must be the version's. ac-import runs three times, into new packages: the second with the first's
   cache, which converts nothing, and the third with a new cache; the three packages are the same bytes. The package
   mounts through fs, and each entry is checked against the disc's file, read here rather than through the importer:
   the archives with import_checks.hpp, the copies byte for byte, and audiorom.img's host layout, with audiorom.tbl,
   field by field against the disc's through the game's own structures and tables (check_audiorom). The packages and
   caches go to a temporary directory that is removed at the end. */
// The game's headers before the host's: libultra's pad structures name a member errno, which <cerrno> defines.
#include "jaudio_NES/audioheaders.h"
#include "jaudio_NES/audiostruct.h"

#include "import_checks.hpp"

#include <sonar/disc/disc.h>
#include <sonar/fs.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

// The package's entries, from the spec's table rather than the importer's code.
struct Expected {
    const char* path; // the entry's path and its disc file's
    bool archive;
    const ac::ContentRule* rules;
    size_t rule_count;
};

const ac::ContentRule forest_1st_rules[] = {
    { "fgdata.bin", ac::Content::fg_data },
    { "string_data_table.bin", ac::Content::u32_table },
    { "pallet_boy.bin", ac::Content::palettes },
    { "face_boy.bin", ac::Content::face },
};
const ac::ContentRule forest_2nd_rules[] = {
    { "message_data_table.bin", ac::Content::u32_table },
};

const Expected expected[] = {
    { "COPYDATE", false, nullptr, 0 },
    { "static.str", false, nullptr, 0 },
    { "forest_1st.arc", true, forest_1st_rules, std::size(forest_1st_rules) },
    { "forest_2nd.arc", true, forest_2nd_rules, std::size(forest_2nd_rules) },
    { "famicom.arc", true, nullptr, 0 },
};

// audiorom.img and audiorom.tbl, which check_audiorom reads together.
constexpr size_t audio_entries = 2;

bool read_host_file(const fs::path& path, std::vector<uint8_t>& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return !file.bad();
}

// The disc's files, read by the test: an image through formats/disc, an extracted directory as host files.
class Disc {
  public:
    ~Disc() {
        sl_disc_close(disc_);
    }

    bool open(const fs::path& path) {
        if (fs::is_directory(path)) {
            files_ = path / "files";
            std::vector<uint8_t> boot;
            if (!read_host_file(path / "sys" / "boot.bin", boot) || boot.size() < 8) {
                return false;
            }
            id_.assign(reinterpret_cast<const char*>(boot.data()), 6);
            revision_ = boot[7];
            return true;
        }
        image_.open(path, std::ios::binary);
        std::error_code error;
        const uintmax_t size = fs::file_size(path, error);
        if (!image_ || error) {
            return false;
        }
        const sl_disc_source source = { read_image, &image_, size };
        if (sl_disc_open(&source, &disc_) != SL_DISC_OK) {
            return false;
        }
        const sl_disc_info* info = sl_disc_get_info(disc_);
        id_ = info->id;
        revision_ = info->revision;
        return true;
    }

    const std::string& id() const {
        return id_;
    }
    int revision() const {
        return revision_;
    }

    bool read(const char* name, std::vector<uint8_t>& out) {
        if (disc_ == nullptr) {
            return read_host_file(files_ / name, out);
        }
        uint32_t index = 0;
        sl_disc_entry entry;
        if (sl_disc_find(disc_, name, &index) != SL_DISC_OK || sl_disc_entry_get(disc_, index, &entry) != SL_DISC_OK ||
            entry.is_directory) {
            return false;
        }
        out.resize(static_cast<size_t>(entry.size));
        size_t read = 0;
        return sl_disc_read_file(disc_, index, 0, out.data(), out.size(), &read) == SL_DISC_OK && read == out.size();
    }

  private:
    static int read_image(void* user, uint64_t offset, void* buffer, size_t size) {
        auto* image = static_cast<std::ifstream*>(user);
        image->clear();
        image->seekg(static_cast<std::streamoff>(offset));
        image->read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
        return image->gcount() == static_cast<std::streamsize>(size);
    }

    std::ifstream image_;
    sl_disc* disc_ = nullptr;
    fs::path files_;
    std::string id_;
    int revision_ = -1;
};

// audiorom.img's host layout (importer/audio.hpp) against the disc's, read here big-endian through the game's own
// tables and with the game's own host structures: each table entry, the regions around the banks byte for byte, and
// in each bank every structure the engine relocates, followed from the host head by its pointers.
class AudioCheck {
  public:
    AudioCheck(const std::vector<uint8_t>& console, const std::vector<uint8_t>& image,
               const std::vector<uint8_t>& table)
        : console_(console), image_(image), table_(table) {}

    // The number of structures checked.
    size_t run() {
        const ArcHeader& banks = AudiobankHeaderStart;
        const ArcHeader& data = AudiodataHeaderStart;
        const size_t n = static_cast<size_t>(banks.numEntries), m = static_cast<size_t>(data.numEntries);
        AC_CHECK(table_.size() == 4 * (3 + 2 * (n + m)), "audiorom.tbl", 0);
        if (table_.size() != 4 * (3 + 2 * (n + m)))
            return 0;
        AC_CHECK(word(0) == 1 && word(1) == n && word(2) == m, "audiorom.tbl", 0);
        const uint32_t region = static_cast<uint32_t>(data.entries[BANK_TABLE].addr);
        const uint32_t region_size = static_cast<uint32_t>(data.entries[BANK_TABLE].size);
        const uint32_t host_region = word(3 + 2 * (n + BANK_TABLE) + 1);
        AC_CHECK(word(3 + 2 * (n + BANK_TABLE)) == region && host_region >= region_size && host_region % 32 == 0,
                 "audiorom.tbl", BANK_TABLE);
        const uint32_t growth = host_region - region_size;
        AC_CHECK(image_.size() == console_.size() + growth, "audiorom.img", 0);
        if (image_.size() != console_.size() + growth)
            return 0;
        for (size_t i = 0; i < m; i++) {
            if (i == BANK_TABLE)
                continue;
            const uint32_t addr = static_cast<uint32_t>(data.entries[i].addr);
            const uint32_t size = static_cast<uint32_t>(data.entries[i].size);
            const uint32_t host = addr >= region + region_size ? addr + growth : addr;
            AC_CHECK(word(3 + 2 * (n + i)) == host && word(3 + 2 * (n + i) + 1) == size, "audiorom.tbl", i);
            AC_CHECK(std::memcmp(image_.data() + host, console_.data() + addr, size) == 0, "audiorom.img", addr);
        }
        size_t checked = 0;
        for (size_t i = 0; i < n; i++) {
            const ArcEntry& entry = banks.entries[i];
            const uint32_t addr = word(3 + 2 * i), size = word(3 + 2 * i + 1);
            if (entry.size == 0) {
                AC_CHECK(addr == static_cast<uint32_t>(entry.addr) && size == 0, "audiorom.tbl", i);
                continue;
            }
            AC_CHECK(addr % 32 == 0 && size % 32 == 0 && uint64_t{ addr } + size <= host_region, "audiorom.tbl", i);
            c_ = console_.data() + region + entry.addr;
            h_ = image_.data() + region + addr;
            host_size_ = size;
            checked += bank(entry, i);
        }
        return checked;
    }

  private:
    uint32_t word(size_t index) const {
        uint32_t value;
        std::memcpy(&value, table_.data() + 4 * index, 4);
        return value;
    }
    uint32_t be32(uint32_t offset) const {
        return sl_load_be32(c_ + offset);
    }
    // A structure, at a multiple of 16; or with `entry`, one entry of the head or the drum list.
    template <typename T> T host(uaddr offset, size_t where, bool entry = false) {
        T value{};
        const bool inside = (entry || offset % 16 == 0) && offset + sizeof value <= host_size_;
        AC_CHECK(inside, "audiorom.img bank", where);
        if (inside)
            std::memcpy(&value, h_ + offset, sizeof value);
        return value;
    }
    float console_float(uint32_t offset) const {
        const uint32_t bits = be32(offset);
        float value;
        std::memcpy(&value, &bits, sizeof value);
        return value;
    }

    size_t bank(const ArcEntry& entry, size_t index) {
        const uint32_t instruments = (static_cast<uint16_t>(entry.param1) >> 8) & 0xFF;
        const uint32_t drums = static_cast<uint8_t>(entry.param1), effects = static_cast<uint16_t>(entry.param2);
        size_t checked = 0;
        const uaddr drum_list = host<uaddr>(0, index, true);
        if (be32(0) != 0 && drums != 0) {
            for (uint32_t i = 0; i < drums; i++) {
                const uint32_t at = be32(be32(0) + 4 * i);
                const uaddr drum_at = host<uaddr>(drum_list + sizeof(uaddr) * i, index, true);
                AC_CHECK((at == 0) == (drum_at == 0), "audiorom.img drum", at);
                if (at == 0 || drum_at == 0)
                    continue;
                const perctable drum = host<perctable>(drum_at, at);
                AC_CHECK(drum.adsr_decay_idx == c_[at] && drum.pan == c_[at + 1] && drum.is_relocated == c_[at + 2],
                         "audiorom.img drum", at);
                checked += 1 + tuned(drum.tuned_sample, at + 4) +
                           envelope(reinterpret_cast<uaddr>(drum.envelope), be32(at + 0xC));
            }
        }
        const uaddr effect_list = host<uaddr>(sizeof(uaddr), index, true);
        if (be32(4) != 0 && effects != 0) {
            for (uint32_t i = 0; i < effects; i++) {
                const uint32_t at = be32(4) + 8 * i;
                const percvoicetable effect = host<percvoicetable>(effect_list + sizeof(percvoicetable) * i, at);
                AC_CHECK((be32(at) == 0) == (effect.tuned_sample.wavetable == nullptr), "audiorom.img sound effect",
                         at);
                if (be32(at) != 0)
                    checked += 1 + tuned(effect.tuned_sample, at);
            }
        }
        for (uint32_t i = 0; i < instruments; i++) {
            const uint32_t at = be32(8 + 4 * i);
            const uaddr inst_at = host<uaddr>(sizeof(uaddr) * (2 + i), index, true);
            AC_CHECK((at == 0) == (inst_at == 0), "audiorom.img instrument", at);
            if (at == 0 || inst_at == 0)
                continue;
            const voicetable inst = host<voicetable>(inst_at, at);
            AC_CHECK(inst.is_relocated == c_[at] && inst.normal_range_low == c_[at + 1] &&
                         inst.normal_range_high == c_[at + 2] && inst.adsr_decay_idx == c_[at + 3],
                     "audiorom.img instrument", at);
            checked += 1 + tuned(inst.normal_pitch_tuned_sample, at + 0x10) +
                       envelope(reinterpret_cast<uaddr>(inst.envelope), be32(at + 4));
            if (inst.normal_range_low != 0)
                checked += tuned(inst.low_pitch_tuned_sample, at + 0x08);
            else
                AC_CHECK(inst.low_pitch_tuned_sample.wavetable == nullptr, "audiorom.img instrument", at);
            if (inst.normal_range_high != 0x7F)
                checked += tuned(inst.high_pitch_tuned_sample, at + 0x18);
            else
                AC_CHECK(inst.high_pitch_tuned_sample.wavetable == nullptr, "audiorom.img instrument", at);
        }
        return checked;
    }

    // A tuned sample, its wave table, and the wave table's loop and book.
    size_t tuned(const wtstr& sample, uint32_t at) {
        AC_CHECK(sample.tuning == console_float(at + 4), "audiorom.img tuning", at);
        const uint32_t w = be32(at);
        const smzwavetable wave = host<smzwavetable>(reinterpret_cast<uaddr>(sample.wavetable), w);
        const uint32_t bits = be32(w);
        AC_CHECK(wave.bit31 == (bits >> 31) && wave.codec == ((bits >> 28) & 7) && wave.medium == ((bits >> 26) & 3) &&
                     wave.bit26 == ((bits >> 25) & 1) && wave.is_relocated == 0 && wave.size == (bits & 0xFFFFFF) &&
                     reinterpret_cast<uaddr>(wave.sample) == be32(w + 4),
                 "audiorom.img wave table", w);
        if (wave.size == 0) {
            AC_CHECK(wave.loop == nullptr && wave.book == nullptr, "audiorom.img wave table", w);
            return 1;
        }
        const uint32_t l = be32(w + 8), b = be32(w + 0xC);
        const adpcmloop loop = host<adpcmloop>(reinterpret_cast<uaddr>(wave.loop), l);
        AC_CHECK(loop.loop_start == be32(l) && loop.loop_end == be32(l + 4) && loop.count == be32(l + 8) &&
                     loop.sample_end == be32(l + 0xC),
                 "audiorom.img loop", l);
        for (uint32_t i = 0; loop.count != 0 && i < 16; i++)
            AC_CHECK(loop.predictor_state[i] == static_cast<s16>(sl_load_be16(c_ + l + 0x10 + 2 * i)),
                     "audiorom.img loop", l);
        const uaddr book_at = reinterpret_cast<uaddr>(wave.book);
        const adpcmbook book = host<adpcmbook>(book_at, b);
        AC_CHECK(book.order == static_cast<s32>(be32(b)) && book.n_predictors == static_cast<s32>(be32(b + 4)),
                 "audiorom.img book", b);
        const uint64_t values =
            uint64_t{ static_cast<uint32_t>(book.order) } * static_cast<uint32_t>(book.n_predictors) * 8;
        AC_CHECK(book_at + sizeof book + 2 * values <= host_size_, "audiorom.img book", b);
        for (uint64_t i = 0; i < values && book_at + sizeof book + 2 * values <= host_size_; i++) {
            s16 value;
            std::memcpy(&value, h_ + book_at + sizeof book + 2 * i, 2);
            AC_CHECK(value == static_cast<s16>(sl_load_be16(c_ + b + 8 + 2 * i)), "audiorom.img book", b);
        }
        return 3;
    }

    // An envelope: the console's bytes, to the entry that ends it.
    size_t envelope(uaddr host_at, uint32_t at) {
        AC_CHECK(host_at % 16 == 0 && host_at != 0, "audiorom.img envelope", at);
        for (uint32_t i = 0;; i += 4) {
            const bool inside = host_at + i + 4 <= host_size_;
            AC_CHECK(inside, "audiorom.img envelope", at);
            if (!inside)
                return 0;
            AC_CHECK(std::memcmp(h_ + host_at + i, c_ + at + i, 4) == 0, "audiorom.img envelope", at);
            const s16 delay = static_cast<s16>(sl_load_be16(c_ + at + i));
            if (delay == ADSR_DISABLE || delay == ADSR_HANG || delay == ADSR_GOTO || delay == ADSR_RESTART)
                return 1;
        }
    }

    const std::vector<uint8_t>& console_;
    const std::vector<uint8_t>& image_;
    const std::vector<uint8_t>& table_;
    const uint8_t* c_ = nullptr;
    const uint8_t* h_ = nullptr;
    uint32_t host_size_ = 0;
};

struct Stats {
    unsigned long entries = 0, converted = 0, reused = 0;
    unsigned long long size = 0;
};

// Runs ac-import, its line of statistics to `log`.
bool run_import(const std::string& tool, const std::string& disc, const fs::path& package, const fs::path& cache,
                const fs::path& log, Stats& stats) {
    const std::string command = "\"" + tool + "\" \"" + disc + "\" \"" + package.string() + "\" \"" + cache.string() +
                                "\" > \"" + log.string() + "\"";
    if (std::system(command.c_str()) != 0) {
        std::printf("ac-import failed: %s\n", command.c_str());
        return false;
    }
    std::ifstream file(log);
    std::string line;
    std::getline(file, line);
    std::printf("%s\n", line.c_str());
    return std::sscanf(line.c_str(), "entries %lu converted %lu reused %lu size %llu", &stats.entries, &stats.converted,
                       &stats.reused, &stats.size) == 4;
}

bool read_entry(const char* path, std::vector<uint8_t>& out) {
    const std::string virtual_path = std::string("game:") + path;
    sl_file file;
    if (sl_fs_open(virtual_path.c_str(), 0, &file) != SL_OK) {
        return false;
    }
    uint64_t size = 0;
    size_t read = 0;
    bool ok = sl_fs_size(file, &size) == SL_OK;
    if (ok) {
        out.resize(static_cast<size_t>(size));
        ok = sl_fs_read(file, 0, out.data(), out.size(), &read) == SL_OK && read == out.size();
    }
    return sl_fs_close(file) == SL_OK && ok;
}

int run(const std::string& tool, const std::string& disc_path, const std::string& version, const fs::path& tmp) {
    Disc disc;
    AC_CHECK(disc.open(disc_path), "open the disc", 0);
    if (ac_test::failures != 0) {
        return 1;
    }
    // AC_VERSION is the ID and the revision: GAFE01_00.
    AC_CHECK(disc.id() == version.substr(0, 6), "the disc's ID", 0);
    AC_CHECK(version.size() > 7 && disc.revision() == std::atoi(version.c_str() + 7), "the disc's revision", 0);

    Stats first, second, third;
    const bool ran = run_import(tool, disc_path, tmp / "first.slpack", tmp / "cache", tmp / "first.txt", first) &&
                     run_import(tool, disc_path, tmp / "second.slpack", tmp / "cache", tmp / "second.txt", second) &&
                     run_import(tool, disc_path, tmp / "third.slpack", tmp / "cache3", tmp / "third.txt", third);
    AC_CHECK(ran, "ac-import ran three times", 0);
    if (!ran) {
        return 1;
    }
    const size_t entries = std::size(expected) + audio_entries;
    AC_CHECK(first.entries == entries, "the first run's entries", 0);
    AC_CHECK(second.converted == 0 && second.reused == entries, "the second run reused all", 0);
    AC_CHECK(third.converted == entries, "the third run converted all", 0);
    std::vector<uint8_t> first_bytes, second_bytes, third_bytes;
    AC_CHECK(read_host_file(tmp / "first.slpack", first_bytes) && read_host_file(tmp / "second.slpack", second_bytes) &&
                 read_host_file(tmp / "third.slpack", third_bytes),
             "read the packages", 0);
    AC_CHECK(first_bytes == second_bytes, "the second package is the first's bytes", 0);
    AC_CHECK(first_bytes == third_bytes, "the third package is the first's bytes", 0);

    sl_fs_desc fs_desc = {};
    fs_desc.flags = SL_FS_INIT_NO_USER_MOUNT;
    if (sl_fs_init(&fs_desc) != SL_OK) {
        AC_CHECK(!"sl_fs_init", "fs", 0);
        return 1;
    }
    const std::string package = (tmp / "first.slpack").string();
    const sl_mount_desc mount_desc = { "game", package.c_str(), SL_MOUNT_PACKAGE, "ac", 2 };
    sl_mount mount;
    AC_CHECK(sl_fs_mount(&mount_desc, &mount) == SL_OK, "mount the package", 0);
    for (const Expected& entry : expected) {
        std::vector<uint8_t> host, console;
        AC_CHECK(read_entry(entry.path, host), entry.path, 0);
        AC_CHECK(disc.read(entry.path, console), entry.path, 0);
        if (entry.archive) {
            ac_test::check_archive(entry.path, console, host, entry.rules, entry.rule_count);
        } else {
            AC_CHECK(host == console, entry.path, 0);
        }
    }
    std::vector<uint8_t> image, table, console;
    AC_CHECK(read_entry("audiorom.img", image) && read_entry("audiorom.tbl", table), "audiorom", 0);
    AC_CHECK(disc.read("audiorom.img", console), "audiorom.img", 0);
    const size_t structures = AudioCheck(console, image, table).run();
    std::printf("audiorom: %zu structures checked\n", structures);
    AC_CHECK(structures != 0, "audiorom.img", 0);
    sl_fs_shutdown();
    return ac_test::failures != 0 ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::printf("usage: ac_import_disc_test <ac-import> <disc> <version>\n");
        return 2;
    }
    if (argv[2][0] == '\0') {
        std::printf("no disc: set AC_DISC to the player's disc image or extracted directory\n");
        return 77;
    }
    // A new directory, named by the clock (<random> pulls in the host's math, which names.h renames).
    std::error_code error;
    fs::path tmp;
    for (auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();; stamp++) {
        tmp = fs::temp_directory_path(error) / ("ac_import_disc_" + std::to_string(stamp));
        if (error || fs::create_directory(tmp, error) || error) {
            break;
        }
    }
    if (error) {
        std::printf("no temporary directory: %s\n", error.message().c_str());
        return 1;
    }
    const int result = run(argv[1], argv[2], argv[3], tmp);
    fs::remove_all(tmp, error);
    if (result != 0) {
        std::printf("%d failed\n", ac_test::failures);
        return result;
    }
    std::printf("ac_import_disc: passed\n");
    return 0;
}

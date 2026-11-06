/* Checks ac-import on the player's disc (sonarlib docs/roadmap.md, M6 step 6c):
     ac_import_disc_test <ac-import> <disc> <version>
   The disc is an image or a directory in sl-disc's extracted layout; with none the test is skipped (77). The disc's ID
   and revision must be the version's. ac-import runs three times, into new packages: the second with the first's
   cache, which converts nothing, and the third with a new cache; the three packages are the same bytes. The package
   mounts through fs, and each entry is checked against the disc's file, read here rather than through the importer:
   the archives with import_checks.hpp, the copies byte for byte. The packages and caches go to a temporary directory
   that is removed at the end. */
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
    AC_CHECK(first.entries == std::size(expected), "the first run's entries", 0);
    AC_CHECK(second.converted == 0 && second.reused == std::size(expected), "the second run reused all", 0);
    AC_CHECK(third.converted == std::size(expected), "the third run converted all", 0);
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
    const sl_mount_desc mount_desc = { "game", package.c_str(), SL_MOUNT_PACKAGE, "ac", 1 };
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

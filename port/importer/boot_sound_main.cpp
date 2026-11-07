/* ac-boot-sound <bank.bin> <wave.bin> <out.inc>: lays out the boot sound's bank and wave set for the host
   (boot_sound.hpp) and writes the definitions game64_dummy.c_inc includes. */
#include "boot_sound.hpp"

#include <sonar/sonar.h>

#include <cstdio>
#include <vector>

namespace {

bool read_file(const char* path, std::vector<uint8_t>& bytes) {
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return false;
    }
    uint8_t buffer[4096];
    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof buffer, file)) != 0) {
        bytes.insert(bytes.end(), buffer, buffer + read);
    }
    const bool ok = std::ferror(file) == 0;
    std::fclose(file);
    return ok;
}

bool lay_out(bool bank, const char* path, std::vector<uint8_t>& host) {
    std::vector<uint8_t> console;
    if (!read_file(path, console)) {
        std::fprintf(stderr, "ac-boot-sound: cannot read %s\n", path);
        return false;
    }
    std::string error;
    const sl_result result = ac::lay_out_boot_sound(bank, console, host, error);
    if (result != SL_OK) {
        std::fprintf(stderr, "ac-boot-sound: %s: %s%s%s\n", path, sl_result_string(result), error.empty() ? "" : ": ",
                     error.c_str());
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: ac-boot-sound <bank.bin> <wave.bin> <out.inc>\n");
        return 2;
    }
    std::vector<uint8_t> bank, wave;
    if (!lay_out(true, argv[1], bank) || !lay_out(false, argv[2], wave)) {
        return 1;
    }
    const std::string text = ac::boot_sound_include(bank, wave);
    std::FILE* file = std::fopen(argv[3], "w");
    bool ok = file != nullptr && std::fwrite(text.data(), 1, text.size(), file) == text.size();
    ok = file != nullptr && std::fclose(file) == 0 && ok;
    if (!ok) {
        std::remove(argv[3]);
        std::fprintf(stderr, "ac-boot-sound: cannot write %s\n", argv[3]);
        return 1;
    }
    return 0;
}

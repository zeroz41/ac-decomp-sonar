/* ac-font <in.bfn> <out.inc>: converts a JSystem font to the host's byte order (font.hpp) and writes its bytes as the
   initializer jsystem_font.c includes. */
#include "font.hpp"

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

bool write_include(const char* path, const std::vector<uint8_t>& bytes) {
    std::FILE* file = std::fopen(path, "w");
    if (file == nullptr) {
        return false;
    }
    bool ok = true;
    for (size_t at = 0; at < bytes.size() && ok; at++) {
        ok = std::fprintf(file, (at % 16 == 15 || at + 1 == bytes.size()) ? "0x%02x,\n" : "0x%02x,", bytes[at]) > 0;
    }
    ok = std::fclose(file) == 0 && ok;
    if (!ok) {
        std::remove(path);
    }
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: ac-font <in.bfn> <out.inc>\n");
        return 2;
    }
    std::vector<uint8_t> console;
    if (!read_file(argv[1], console)) {
        std::fprintf(stderr, "ac-font: cannot read %s\n", argv[1]);
        return 1;
    }
    std::vector<uint8_t> host;
    const sl_result result = ac::convert_font(console.data(), console.size(), host);
    if (result != SL_OK) {
        std::fprintf(stderr, "ac-font: %s: %s\n", argv[1], sl_result_string(result));
        return 1;
    }
    if (!write_include(argv[2], host)) {
        std::fprintf(stderr, "ac-font: cannot write %s\n", argv[2]);
        return 1;
    }
    return 0;
}

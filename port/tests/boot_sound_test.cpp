/* Checks ac-boot-sound (importer/boot_sound.hpp) on the decompilation's extracts of NIN_BANK and NIN_WAVE: each image
   walked through the game's own structures (bx.h, heapctrl.h) as Bank_Test and Wave_Test reach them, every field
   against the console's bytes, every relocated pointer the offset of the structure its console offset names (one
   place for each, at a multiple of 16, in the console's order) and every other pointer the console's value; and the
   build's boot_sound.inc written again. The boot sound has percussion only, so an instrument or a voice fails.

   ac_boot_sound_test <NIN_BANK.bin> <NIN_WAVE.bin> <boot_sound.inc> */
// The game's headers before the host's, as in audio.cpp.
#include "jaudio_NES/bx.h"
#include "jaudio_NES/heapctrl.h"

#include "../importer/boot_sound.hpp"

#include <sonar/base/endian.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void fail(const char* file, size_t at, const std::string& what) {
    std::printf("%s @0x%zx: %s\n", file, at, what.c_str());
    failures++;
}

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

// One image against its console file. Console offsets are the file's; host offsets are the image's.
class Walk {
  public:
    Walk(const char* file, const std::vector<uint8_t>& console, const std::vector<uint8_t>& host)
        : file_(file), console_(console), host_(host) {
    }

    // Whether the console has `size` bytes at `con` and the host `host_size` at `at`; a failure if not.
    bool fits(uint32_t con, size_t console_size, size_t at, size_t host_size, const char* kind) {
        if (con > console_.size() || console_.size() - con < console_size) {
            fail(file_, con, std::string(kind) + " past the console file's end");
            return false;
        }
        if (at > host_.size() || host_.size() - at < host_size) {
            fail(file_, con, std::string(kind) + " past the image's end");
            return false;
        }
        return true;
    }

    uint32_t c32(uint32_t con) const {
        return sl_load_be32(console_.data() + con);
    }
    uint16_t c16(uint32_t con) const {
        return sl_load_be16(console_.data() + con);
    }

    template <typename T> T h(size_t at) const {
        T value;
        std::memcpy(&value, host_.data() + at, sizeof value);
        return value;
    }

    // A 32-bit integer or float field in the host's order.
    void word(uint32_t con, size_t at, const char* what) {
        if (h<uint32_t>(at) != c32(con)) {
            fail(file_, con, std::string(what) + " is not the console's");
        }
    }
    void half(uint32_t con, size_t at, const char* what) {
        if (h<uint16_t>(at) != c16(con)) {
            fail(file_, con, std::string(what) + " is not the console's");
        }
    }
    void bytes(uint32_t con, size_t at, size_t size, const char* what) {
        if (std::memcmp(console_.data() + con, host_.data() + at, size) != 0) {
            fail(file_, con, std::string(what) + " is not the console's bytes");
        }
    }

    // A pointer the engine does not relocate: the console's value, widened without its sign.
    void kept(uint32_t con, size_t at, const char* what) {
        if (h<uintptr_t>(at) != uintptr_t{ c32(con) }) {
            fail(file_, con, std::string(what) + " is not the console's value");
        }
    }

    /* A pointer the engine relocates, at console offset `con` and host offset `at`: true, with the target's host
       offset in `target` and its console offset in `con_target`, for a structure not walked yet. */
    bool follow(uint32_t con, size_t at, const char* kind, uint32_t& con_target, size_t& target) {
        con_target = c32(con);
        target = h<uintptr_t>(at);
        if (con_target == 0) {
            if (target != 0) {
                fail(file_, con, std::string("a pointer of 0 to ") + kind + " that is not 0");
            }
            return false;
        }
        if (target % 16 != 0) {
            fail(file_, con, std::string(kind) + " not at a multiple of 16");
        }
        const auto found = seen_.find(con_target);
        if (found != seen_.end()) {
            if (found->second.first != target || found->second.second != kind) {
                fail(file_, con, std::string(kind) + " named twice in two places or as two kinds");
            }
            return false;
        }
        seen_.emplace(con_target, std::make_pair(target, kind));
        return true;
    }

    // The structures in the console's order, each after the one before it.
    void ordered() {
        size_t last = 0;
        bool first = true;
        for (const auto& [con, place] : seen_) {
            if (!first && place.first <= last) {
                fail(file_, con, std::string(place.second) + " not after the structure before it");
            }
            last = place.first;
            first = false;
        }
        if (host_.size() % 32 != 0) {
            fail(file_, 0, "an image whose size is not a multiple of 32");
        }
    }

    size_t structures() const {
        return seen_.size();
    }

    // A jaheap_, none of whose pointers the engine relocates.
    void heap(uint32_t con, size_t at) {
        bytes(con + 0x00, at + offsetof(jaheap_, isRootHeap), 2, "a heap's flags");
        half(con + 0x02, at + offsetof(jaheap_, childCount), "a heap's child count");
        word(con + 0x04, at + offsetof(jaheap_, heapId), "a heap's id");
        kept(con + 0x08, at + offsetof(jaheap_, startAddress), "a heap's start");
        word(con + 0x0C, at + offsetof(jaheap_, usedSize), "a heap's used size");
        word(con + 0x10, at + offsetof(jaheap_, size), "a heap's size");
        kept(con + 0x14, at + offsetof(jaheap_, firstChild), "a heap's first child");
        kept(con + 0x18, at + offsetof(jaheap_, parent), "a heap's parent");
        kept(con + 0x1C, at + offsetof(jaheap_, nextSibling), "a heap's next sibling");
        kept(con + 0x20, at + offsetof(jaheap_, groupOwner), "a heap's group owner");
        kept(con + 0x24, at + offsetof(jaheap_, firstGroupedHeap), "a heap's first grouped heap");
        kept(con + 0x28, at + offsetof(jaheap_, nextGroupedHeap), "a heap's next grouped heap");
    }

    const char* file_;
    const std::vector<uint8_t>& console_;
    const std::vector<uint8_t>& host_;

  private:
    std::map<uint32_t, std::pair<size_t, const char*>> seen_;
};

constexpr uint32_t magic(const char (&text)[5]) {
    return uint32_t(uint8_t(text[0])) << 24 | uint32_t(uint8_t(text[1])) << 16 | uint32_t(uint8_t(text[2])) << 8 |
           uint32_t(uint8_t(text[3]));
}

constexpr size_t console_heap = 0x2C;

// ---- the bank ----

void rand_(Walk& w, uint32_t con, size_t at) {
    if (!w.fits(con, 0x10, at, sizeof(Rand_), "a random effect")) {
        return;
    }
    w.bytes(con + 0x00, at + offsetof(Rand_, id), 1, "a random effect's id");
    w.word(con + 0x04, at + offsetof(Rand_, value), "a random effect's value");
    w.word(con + 0x08, at + offsetof(Rand_, range), "a random effect's range");
    w.bytes(con + 0x0C, at + offsetof(Rand_, _0C), 4, "a random effect's last bytes");
}

void vmap(Walk& w, uint32_t con, size_t at) {
    if (!w.fits(con, 0x10, at, sizeof(Vmap_), "a velocity map")) {
        return;
    }
    w.bytes(con + 0x00, at + offsetof(Vmap_, mBaseVelocity), 4,
            "a velocity map's base velocity and the bytes after it");
    // The code reads the word before the volume as one u32 (map[1], oneshot.c:823), not as bx.h's two s16.
    w.word(con + 0x04, at + offsetof(Vmap_, mVolume) - 4, "a velocity map's wave");
    w.word(con + 0x08, at + offsetof(Vmap_, mVolume), "a velocity map's volume");
    w.word(con + 0x0C, at + offsetof(Vmap_, mPitch), "a velocity map's pitch");
}

void perc_keymap(Walk& w, uint32_t con, size_t at) {
    if (!w.fits(con, 0x14, at, offsetof(PercKeymap_, mVelocities), "a percussion key map")) {
        return;
    }
    w.word(con + 0x00, at + offsetof(PercKeymap_, mPitch), "a percussion key map's pitch");
    w.word(con + 0x04, at + offsetof(PercKeymap_, mVolume), "a percussion key map's volume");
    uint32_t con_target = 0;
    size_t target = 0;
    if (w.follow(con + 0x08, at + offsetof(PercKeymap_, _08), "a random effect", con_target, target)) {
        rand_(w, con_target, target);
    }
    if (w.follow(con + 0x0C, at + offsetof(PercKeymap_, _0C), "a random effect", con_target, target)) {
        rand_(w, con_target, target);
    }
    w.word(con + 0x10, at + offsetof(PercKeymap_, mVelocityCount), "a percussion key map's velocity count");
    const int32_t count = int32_t(w.c32(con + 0x10));
    if (count < 0 ||
        !w.fits(con, 0x14 + 4 * size_t(count), at, offsetof(PercKeymap_, mVelocities) + sizeof(Vmap_*) * size_t(count),
                "a percussion key map's velocities")) {
        return;
    }
    for (int32_t i = 0; i < count; i++) {
        if (w.follow(con + 0x14 + 4 * uint32_t(i), at + offsetof(PercKeymap_, mVelocities) + sizeof(Vmap_*) * size_t(i),
                     "a velocity map", con_target, target)) {
            vmap(w, con_target, target);
        }
    }
}

void perc(Walk& w, uint32_t con, size_t at) {
    if (!w.fits(con, 0x408, at, sizeof(Perc_), "a percussion set")) {
        return;
    }
    if (w.c32(con) != magic("PER2")) {
        fail(w.file_, con, "a percussion set whose magic is not 'PER2'");
    }
    w.word(con + 0x00, at + offsetof(Perc_, mMagic), "a percussion set's magic");
    w.bytes(con + 0x04, at + offsetof(Perc_, _04), 0x84, "a percussion set's unknown bytes");
    for (uint32_t key = 0; key < 128; key++) {
        uint32_t con_target = 0;
        size_t target = 0;
        if (w.follow(con + 0x88 + 4 * key, at + offsetof(Perc_, mKeyRegions) + sizeof(PercKeymap_*) * key,
                     "a percussion key map", con_target, target)) {
            perc_keymap(w, con_target, target);
        }
        w.bytes(con + 0x288 + key, at + offsetof(Perc_, _288) + key, 1, "a percussion key's pan");
        w.half(con + 0x308 + 2 * key, at + offsetof(Perc_, _308) + 2 * key, "a percussion key's release");
    }
}

void bank(Walk& w) {
    if (!w.fits(0, 0x20 + 4 + 4 * BANK_INST_COUNT, 0, sizeof(Ibnk_), "the bank's head")) {
        return;
    }
    if (w.c32(0) != magic("IBNK") || w.c32(0x20) != magic("BANK")) {
        fail(w.file_, 0, "a bank whose magics are not 'IBNK' and 'BANK'");
    }
    w.word(0x00, offsetof(Ibnk_, magic), "the bank's magic");
    w.word(0x04, offsetof(Ibnk_, _04), "the bank's size");
    w.word(0x08, offsetof(Ibnk_, _08), "the bank's virtual number");
    w.word(0x0C, offsetof(Ibnk_, _0C), "the bank's word at 0xC");
    w.kept(0x10, offsetof(Ibnk_, waveArcBank), "the bank's pointer at 0x10");
    w.bytes(0x14, offsetof(Ibnk_, _14), 0x0C, "the bank's bytes at 0x14");
    const size_t bank_at = offsetof(Ibnk_, bank);
    w.word(0x20, bank_at + offsetof(Bank_, mMagic), "the bank's 'BANK'");
    int percs = 0;
    for (uint32_t slot = 0; slot < BANK_INST_COUNT; slot++) {
        const uint32_t con = 0x24 + 4 * slot;
        uint32_t con_target = 0;
        size_t target = 0;
        const char* kind = slot < BANK_TEST_VOICE_OFFSET  ? "an instrument"
                           : slot < BANK_TEST_PERC_OFFSET ? "a voice"
                                                          : "a percussion set";
        if (!w.follow(con, bank_at + offsetof(Bank_, mInstruments) + sizeof(Inst_*) * slot, kind, con_target, target)) {
            continue;
        }
        if (slot < BANK_TEST_PERC_OFFSET) {
            fail(w.file_, con, std::string(kind) + ", which the boot sound has none of");
            continue;
        }
        perc(w, con_target, target);
        percs++;
    }
    if (percs == 0) {
        fail(w.file_, 0x24, "a bank with no percussion set");
    }
}

// ---- the wave set ----

void wave(Walk& w, uint32_t con, size_t at) {
    if (!w.fits(con, 0x2C, at, sizeof(Wave_) + 4, "a wave")) {
        return;
    }
    w.bytes(con + 0x00, at + offsetof(Wave_, _00), 4, "a wave's bytes");
    w.word(con + 0x04, at + offsetof(Wave_, _04), "a wave's rate");
    w.word(con + 0x08, at + offsetof(Wave_, srcAddress), "a wave's address");
    w.word(con + 0x0C, at + offsetof(Wave_, length), "a wave's length");
    w.word(con + 0x10, at + offsetof(Wave_, isLooping), "a wave's loop flag");
    w.word(con + 0x14, at + offsetof(Wave_, loopAddress), "a wave's loop address");
    w.word(con + 0x18, at + offsetof(Wave_, loopStartPosition), "a wave's loop start");
    w.word(con + 0x1C, at + offsetof(Wave_, _1C), "a wave's word at 0x1C");
    w.half(con + 0x20, at + offsetof(Wave_, loopYN1), "a wave's loop yn1");
    w.half(con + 0x22, at + offsetof(Wave_, loopYN2), "a wave's loop yn2");
    w.kept(con + 0x24, at + offsetof(Wave_, fileLoadStatus), "a wave's load status pointer");
    // The word at 0x28, after the declared fields, which the engine never reads.
    w.word(con + 0x28, at + sizeof(Wave_), "a wave's word at 0x28");
}

void archive(Walk& w, uint32_t con, size_t at) {
    if (!w.fits(con, 0x74, at, offsetof(WaveArchive_, waves), "a wave archive")) {
        return;
    }
    w.bytes(con + 0x00, at + offsetof(WaveArchive_, filePath), 0x40, "a wave archive's file name");
    w.heap(con + 0x40, at + offsetof(WaveArchive_, heap));
    w.word(con + 0x40 + console_heap, at + offsetof(WaveArchive_, fileLoadStatus), "a wave archive's load status");
    w.word(con + 0x44 + console_heap, at + offsetof(WaveArchive_, waveCount), "a wave archive's wave count");
    const int32_t count = int32_t(w.c32(con + 0x44 + console_heap));
    if (count < 0 ||
        !w.fits(con, 0x74 + 4 * size_t(count), at, offsetof(WaveArchive_, waves) + sizeof(Wave_*) * size_t(count),
                "a wave archive's waves")) {
        return;
    }
    for (int32_t i = 0; i < count; i++) {
        uint32_t con_target = 0;
        size_t target = 0;
        if (w.follow(con + 0x74 + 4 * uint32_t(i), at + offsetof(WaveArchive_, waves) + sizeof(Wave_*) * size_t(i),
                     "a wave", con_target, target)) {
            wave(w, con_target, target);
        }
    }
}

void wave_id(Walk& w, uint32_t con, size_t at) {
    if (!w.fits(con, 0x38, at, sizeof(WaveID_), "a wave id")) {
        return;
    }
    w.word(con + 0x00, at + offsetof(WaveID_, id), "a wave id's id");
    w.heap(con + 0x04, at + offsetof(WaveID_, heap));
    w.word(con + 0x04 + console_heap, at + offsetof(WaveID_, loadStatus), "a wave id's load status");
    w.kept(con + 0x08 + console_heap, at + offsetof(WaveID_, data), "a wave id's wave");
}

void ctrl(Walk& w, uint32_t con, size_t at, uint32_t expected) {
    if (!w.fits(con, 8, at, offsetof(Ctrl_, waveIDs), "a control list")) {
        return;
    }
    if (w.c32(con) != expected) {
        fail(w.file_, con, "a control list whose magic is not its slot's");
    }
    w.word(con + 0x00, at + offsetof(Ctrl_, magic), "a control list's magic");
    w.word(con + 0x04, at + offsetof(Ctrl_, count), "a control list's count");
    const int32_t count = int32_t(w.c32(con + 0x04));
    if (count < 0 || !w.fits(con, 8 + 4 * size_t(count), at,
                             offsetof(Ctrl_, waveIDs) + sizeof(WaveID_*) * size_t(count), "a control list's ids")) {
        return;
    }
    for (int32_t i = 0; i < count; i++) {
        uint32_t con_target = 0;
        size_t target = 0;
        if (w.follow(con + 8 + 4 * uint32_t(i), at + offsetof(Ctrl_, waveIDs) + sizeof(WaveID_*) * size_t(i),
                     "a wave id", con_target, target)) {
            wave_id(w, con_target, target);
        }
    }
}

void scene(Walk& w, uint32_t con, size_t at) {
    if (!w.fits(con, 0x18, at, offsetof(SCNE_, _18), "a scene")) {
        return;
    }
    if (w.c32(con) != magic("SCNE")) {
        fail(w.file_, con, "a scene whose magic is not 'SCNE'");
    }
    w.word(con + 0x00, at + offsetof(SCNE_, magic), "a scene's magic");
    w.word(con + 0x04, at + offsetof(SCNE_, _04), "a scene's word at 4");
    w.word(con + 0x08, at + offsetof(SCNE_, _08), "a scene's id count");
    const uint32_t ids = w.c32(con + 0x08);
    if (ids > w.console_.size() ||
        !w.fits(con, 0x18 + 4 * size_t(ids), at, offsetof(SCNE_, _18) + 4 * size_t(ids), "a scene's ids")) {
        return;
    }
    for (uint32_t i = 0; i < ids; i++) {
        w.word(con + 0x18 + 4 * i, at + offsetof(SCNE_, _18) + 4 * i, "a scene's id");
    }
    const struct {
        uint32_t con;
        size_t at;
        uint32_t magic;
    } lists[] = { { 0x0C, offsetof(SCNE_, cdf), magic("C-DF") },
                  { 0x10, offsetof(SCNE_, cex), magic("C-EX") },
                  { 0x14, offsetof(SCNE_, cst), magic("C-ST") } };
    for (const auto& list : lists) {
        uint32_t con_target = 0;
        size_t target = 0;
        if (w.follow(con + list.con, at + list.at, "a control list", con_target, target)) {
            ctrl(w, con_target, target, list.magic);
        }
    }
}

void wave_set(Walk& w) {
    if (!w.fits(0, 0x18, 0, sizeof(Wsys_), "the wave set's head")) {
        return;
    }
    if (w.c32(0) != magic("WSYS")) {
        fail(w.file_, 0, "a wave set whose magic is not 'WSYS'");
    }
    w.word(0x00, offsetof(Wsys_, magic), "the wave set's magic");
    w.word(0x04, offsetof(Wsys_, size), "the wave set's size");
    w.word(0x08, offsetof(Wsys_, globalID), "the wave set's number");
    w.word(0x0C, offsetof(Wsys_, _0C), "the wave set's word at 0xC");
    uint32_t con = 0;
    size_t at = 0;
    if (w.follow(0x10, offsetof(Wsys_, waveArcBank), "the wave archives", con, at) &&
        w.fits(con, 8, at, offsetof(WaveArchiveBank_, waveGroups), "the wave archives")) {
        if (w.c32(con) != magic("WINF")) {
            fail(w.file_, con, "wave archives whose magic is not 'WINF'");
        }
        w.word(con + 0x00, at + offsetof(WaveArchiveBank_, magic), "the wave archives' magic");
        w.word(con + 0x04, at + offsetof(WaveArchiveBank_, count), "the wave archives' count");
        const int32_t count = int32_t(w.c32(con + 0x04));
        if (count >= 0 && w.fits(con, 8 + 4 * size_t(count), at,
                                 offsetof(WaveArchiveBank_, waveGroups) + sizeof(WaveArchive_*) * size_t(count),
                                 "the wave archives")) {
            for (int32_t i = 0; i < count; i++) {
                uint32_t con_target = 0;
                size_t target = 0;
                if (w.follow(con + 8 + 4 * uint32_t(i),
                             at + offsetof(WaveArchiveBank_, waveGroups) + sizeof(WaveArchive_*) * size_t(i),
                             "a wave archive", con_target, target)) {
                    archive(w, con_target, target);
                }
            }
        }
    }
    if (w.follow(0x14, offsetof(Wsys_, ctrlGroup), "the scenes", con, at) &&
        w.fits(con, 0x0C, at, offsetof(CtrlGroup_, scenes), "the scenes")) {
        if (w.c32(con) != magic("WBCT")) {
            fail(w.file_, con, "scenes whose magic is not 'WBCT'");
        }
        w.word(con + 0x00, at + offsetof(CtrlGroup_, magic), "the scenes' magic");
        w.word(con + 0x04, at + offsetof(CtrlGroup_, _04), "the scenes' word at 4");
        w.word(con + 0x08, at + offsetof(CtrlGroup_, count), "the scenes' count");
        const int32_t count = int32_t(w.c32(con + 0x08));
        if (count >= 0 && w.fits(con, 0x0C + 4 * size_t(count), at,
                                 offsetof(CtrlGroup_, scenes) + sizeof(SCNE_*) * size_t(count), "the scenes")) {
            for (int32_t i = 0; i < count; i++) {
                uint32_t con_target = 0;
                size_t target = 0;
                if (w.follow(con + 0x0C + 4 * uint32_t(i),
                             at + offsetof(CtrlGroup_, scenes) + sizeof(SCNE_*) * size_t(i), "a scene", con_target,
                             target)) {
                    scene(w, con_target, target);
                }
            }
        }
    }
}

bool lay_out(bool is_bank, const char* path, std::vector<uint8_t>& console, std::vector<uint8_t>& host) {
    if (!read_file(path, console)) {
        std::printf("cannot read %s\n", path);
        return false;
    }
    std::string error;
    if (ac::lay_out_boot_sound(is_bank, console, host, error) != SL_OK) {
        std::printf("%s: the kit refuses it: %s\n", path, error.c_str());
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::printf("usage: ac_boot_sound_test <NIN_BANK.bin> <NIN_WAVE.bin> <boot_sound.inc>\n");
        return 2;
    }
    std::vector<uint8_t> bank_console, bank_host, wave_console, wave_host, include;
    if (!lay_out(true, argv[1], bank_console, bank_host) || !lay_out(false, argv[2], wave_console, wave_host)) {
        return 1;
    }
    Walk bank_walk("NIN_BANK", bank_console, bank_host);
    bank(bank_walk);
    bank_walk.ordered();
    Walk wave_walk("NIN_WAVE", wave_console, wave_host);
    wave_set(wave_walk);
    wave_walk.ordered();

    // The same files lay out the same images, and the build's include holds them.
    std::vector<uint8_t> again;
    std::string error;
    if (ac::lay_out_boot_sound(true, bank_console, again, error) != SL_OK || again != bank_host ||
        ac::lay_out_boot_sound(false, wave_console, again, error) != SL_OK || again != wave_host) {
        fail("both", 0, "a second layout that is not the first");
    }
    if (!read_file(argv[3], include)) {
        std::printf("cannot read %s\n", argv[3]);
        return 1;
    }
    const std::string text = ac::boot_sound_include(bank_host, wave_host);
    if (std::string(include.begin(), include.end()) != text) {
        fail(argv[3], 0, "not what ac-boot-sound writes from the extracts");
    }

    std::printf("ac_boot_sound: bank 0x%zx bytes to 0x%zx, %zu structures; wave set 0x%zx bytes to 0x%zx, %zu "
                "structures; %d failures\n",
                bank_console.size(), bank_host.size(), bank_walk.structures(), wave_console.size(), wave_host.size(),
                wave_walk.structures(), failures);
    return failures == 0 ? 0 : 1;
}

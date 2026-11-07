/* The boot sound laid out for the host (boot_sound.hpp), on sonarlib's jaudio kit. */
// The game's headers before the host's, as in audio.cpp.
#include "jaudio_NES/bx.h"
#include "jaudio_NES/heapctrl.h"

#include "boot_sound.hpp"

#include <sonar/jaudio/jaudio1.h>

#include <cstddef>
#include <cstdio>
#include <new>

namespace ac {
namespace {

// A game field and the kit's: at the same offset, of the same size.
#define AC_SAME_FIELD(game, game_field, kit, kit_field)                     \
    static_assert(offsetof(game, game_field) == offsetof(kit, kit_field) && \
                      sizeof(game::game_field) == sizeof(kit::kit_field),   \
                  #game "::" #game_field " is not where the kit's " #kit_field " is")
#define AC_SAME_SIZE(game, kit) static_assert(sizeof(game) == sizeof(kit), #game " is not the kit's size")

AC_SAME_FIELD(jaheap_, isRootHeap, sl_jaudio_jaheap, is_root_heap);
AC_SAME_FIELD(jaheap_, memoryType, sl_jaudio_jaheap, memory_type);
AC_SAME_FIELD(jaheap_, childCount, sl_jaudio_jaheap, child_count);
AC_SAME_FIELD(jaheap_, heapId, sl_jaudio_jaheap, heap_id);
AC_SAME_FIELD(jaheap_, startAddress, sl_jaudio_jaheap, start_address);
AC_SAME_FIELD(jaheap_, usedSize, sl_jaudio_jaheap, used_size);
AC_SAME_FIELD(jaheap_, size, sl_jaudio_jaheap, size);
AC_SAME_FIELD(jaheap_, firstChild, sl_jaudio_jaheap, first_child);
AC_SAME_FIELD(jaheap_, parent, sl_jaudio_jaheap, parent);
AC_SAME_FIELD(jaheap_, nextSibling, sl_jaudio_jaheap, next_sibling);
AC_SAME_FIELD(jaheap_, groupOwner, sl_jaudio_jaheap, group_owner);
AC_SAME_FIELD(jaheap_, firstGroupedHeap, sl_jaudio_jaheap, first_grouped_heap);
AC_SAME_FIELD(jaheap_, nextGroupedHeap, sl_jaudio_jaheap, next_grouped_heap);
AC_SAME_SIZE(jaheap_, sl_jaudio_jaheap);

AC_SAME_FIELD(Bank_, mMagic, sl_jaudio_ibnk_bank, magic);
AC_SAME_FIELD(Bank_, mInstruments, sl_jaudio_ibnk_bank, slots);
AC_SAME_SIZE(Bank_, sl_jaudio_ibnk_bank);
AC_SAME_FIELD(Ibnk_, magic, sl_jaudio_ibnk, magic);
AC_SAME_FIELD(Ibnk_, _04, sl_jaudio_ibnk, size);
AC_SAME_FIELD(Ibnk_, _08, sl_jaudio_ibnk, virtual_bank);
AC_SAME_FIELD(Ibnk_, _0C, sl_jaudio_ibnk, unknown_0c);
AC_SAME_FIELD(Ibnk_, waveArcBank, sl_jaudio_ibnk, unknown_10);
AC_SAME_FIELD(Ibnk_, _14, sl_jaudio_ibnk, unknown_14);
AC_SAME_FIELD(Ibnk_, bank, sl_jaudio_ibnk, bank);
AC_SAME_SIZE(Ibnk_, sl_jaudio_ibnk);

/* Vmap_ as the code reads it: its second word a u32 (map[1], oneshot.c:823, 875 and 964; testPercMap's _04,
   oneshot.c:853), where bx.h declares two s16 its compiler places at 2 and 4. */
AC_SAME_FIELD(Vmap_, mBaseVelocity, sl_jaudio_ibnk_vmap, base_velocity);
static_assert(offsetof(sl_jaudio_ibnk_vmap, wave) == 4 && offsetof(Vmap_, mVolume) == 8,
              "Vmap_'s second word is not where the kit's wave is");
AC_SAME_FIELD(Vmap_, mVolume, sl_jaudio_ibnk_vmap, volume);
AC_SAME_FIELD(Vmap_, mPitch, sl_jaudio_ibnk_vmap, pitch);
AC_SAME_SIZE(Vmap_, sl_jaudio_ibnk_vmap);

AC_SAME_FIELD(InstKeymap_, mBaseKey, sl_jaudio_ibnk_inst_keymap, base_key);
AC_SAME_FIELD(InstKeymap_, mVelocityCount, sl_jaudio_ibnk_inst_keymap, velocity_count);
static_assert(offsetof(InstKeymap_, mVelocities) == offsetof(sl_jaudio_ibnk_inst_keymap, velocities));
AC_SAME_FIELD(PercKeymap_, mPitch, sl_jaudio_ibnk_perc_keymap, pitch);
AC_SAME_FIELD(PercKeymap_, mVolume, sl_jaudio_ibnk_perc_keymap, volume);
AC_SAME_FIELD(PercKeymap_, _08, sl_jaudio_ibnk_perc_keymap, rand_08);
AC_SAME_FIELD(PercKeymap_, _0C, sl_jaudio_ibnk_perc_keymap, rand_0c);
AC_SAME_FIELD(PercKeymap_, mVelocityCount, sl_jaudio_ibnk_perc_keymap, velocity_count);
static_assert(offsetof(PercKeymap_, mVelocities) == offsetof(sl_jaudio_ibnk_perc_keymap, velocities));

AC_SAME_FIELD(Sense_, id, sl_jaudio_ibnk_sense, id);
AC_SAME_FIELD(Sense_, type, sl_jaudio_ibnk_sense, type);
AC_SAME_FIELD(Sense_, threshold, sl_jaudio_ibnk_sense, threshold);
AC_SAME_FIELD(Sense_, min, sl_jaudio_ibnk_sense, min);
AC_SAME_FIELD(Sense_, max, sl_jaudio_ibnk_sense, max);
AC_SAME_SIZE(Sense_, sl_jaudio_ibnk_sense);
AC_SAME_FIELD(Rand_, id, sl_jaudio_ibnk_rand, id);
AC_SAME_FIELD(Rand_, value, sl_jaudio_ibnk_rand, value);
AC_SAME_FIELD(Rand_, range, sl_jaudio_ibnk_rand, range);
AC_SAME_FIELD(Rand_, _0C, sl_jaudio_ibnk_rand, unknown_0c);
AC_SAME_SIZE(Rand_, sl_jaudio_ibnk_rand);
AC_SAME_FIELD(Osc_, mode, sl_jaudio_ibnk_osc, mode);
AC_SAME_FIELD(Osc_, rate, sl_jaudio_ibnk_osc, rate);
AC_SAME_FIELD(Osc_, attackVecOffset, sl_jaudio_ibnk_osc, attack_vector);
AC_SAME_FIELD(Osc_, releaseVecOffset, sl_jaudio_ibnk_osc, release_vector);
AC_SAME_FIELD(Osc_, width, sl_jaudio_ibnk_osc, width);
AC_SAME_FIELD(Osc_, vertex, sl_jaudio_ibnk_osc, vertex);
AC_SAME_SIZE(Osc_, sl_jaudio_ibnk_osc);

AC_SAME_FIELD(Inst_, mMagic, sl_jaudio_ibnk_inst, magic);
AC_SAME_FIELD(Inst_, mFlag, sl_jaudio_ibnk_inst, flag);
AC_SAME_FIELD(Inst_, mFreqMultiplier, sl_jaudio_ibnk_inst, freq_multiplier);
AC_SAME_FIELD(Inst_, mGainMultiplier, sl_jaudio_ibnk_inst, gain_multiplier);
AC_SAME_FIELD(Inst_, mOscillators, sl_jaudio_ibnk_inst, oscillators);
AC_SAME_FIELD(Inst_, mEffects, sl_jaudio_ibnk_inst, effects);
AC_SAME_FIELD(Inst_, mSensors, sl_jaudio_ibnk_inst, sensors);
AC_SAME_FIELD(Inst_, mKeyRegionCount, sl_jaudio_ibnk_inst, key_region_count);
static_assert(offsetof(Inst_, mKeyRegions) == offsetof(sl_jaudio_ibnk_inst, key_regions));
AC_SAME_FIELD(Perc_, mMagic, sl_jaudio_ibnk_perc, magic);
AC_SAME_FIELD(Perc_, _04, sl_jaudio_ibnk_perc, unknown_04);
AC_SAME_FIELD(Perc_, mKeyRegions, sl_jaudio_ibnk_perc, key_regions);
AC_SAME_FIELD(Perc_, _288, sl_jaudio_ibnk_perc, pan);
AC_SAME_FIELD(Perc_, _308, sl_jaudio_ibnk_perc, release);
AC_SAME_SIZE(Perc_, sl_jaudio_ibnk_perc);

AC_SAME_FIELD(Wsys_, magic, sl_jaudio_wsys, magic);
AC_SAME_FIELD(Wsys_, size, sl_jaudio_wsys, size);
AC_SAME_FIELD(Wsys_, globalID, sl_jaudio_wsys, global_id);
AC_SAME_FIELD(Wsys_, _0C, sl_jaudio_wsys, unknown_0c);
AC_SAME_FIELD(Wsys_, waveArcBank, sl_jaudio_wsys, wave_archive_bank);
AC_SAME_FIELD(Wsys_, ctrlGroup, sl_jaudio_wsys, ctrl_group);
AC_SAME_SIZE(Wsys_, sl_jaudio_wsys);
AC_SAME_FIELD(WaveArchiveBank_, magic, sl_jaudio_wsys_archive_bank, magic);
AC_SAME_FIELD(WaveArchiveBank_, count, sl_jaudio_wsys_archive_bank, count);
AC_SAME_FIELD(WaveArchiveBank_, waveGroups, sl_jaudio_wsys_archive_bank, archives);
AC_SAME_FIELD(WaveArchive_, filePath, sl_jaudio_wsys_archive, file_path);
AC_SAME_FIELD(WaveArchive_, heap, sl_jaudio_wsys_archive, heap);
AC_SAME_FIELD(WaveArchive_, fileLoadStatus, sl_jaudio_wsys_archive, file_load_status);
AC_SAME_FIELD(WaveArchive_, waveCount, sl_jaudio_wsys_archive, wave_count);
AC_SAME_FIELD(WaveArchive_, waves, sl_jaudio_wsys_archive, waves);
// Wave_'s declared fields; the kit keeps the word the console has after them.
AC_SAME_FIELD(Wave_, _00, sl_jaudio_wsys_wave, unknown_00);
AC_SAME_FIELD(Wave_, compBlockIdx, sl_jaudio_wsys_wave, comp_block_idx);
AC_SAME_FIELD(Wave_, key, sl_jaudio_wsys_wave, key);
AC_SAME_FIELD(Wave_, _04, sl_jaudio_wsys_wave, rate);
AC_SAME_FIELD(Wave_, srcAddress, sl_jaudio_wsys_wave, src_address);
AC_SAME_FIELD(Wave_, length, sl_jaudio_wsys_wave, length);
AC_SAME_FIELD(Wave_, isLooping, sl_jaudio_wsys_wave, is_looping);
AC_SAME_FIELD(Wave_, loopAddress, sl_jaudio_wsys_wave, loop_address);
AC_SAME_FIELD(Wave_, loopStartPosition, sl_jaudio_wsys_wave, loop_start_position);
AC_SAME_FIELD(Wave_, _1C, sl_jaudio_wsys_wave, unknown_1c);
AC_SAME_FIELD(Wave_, loopYN1, sl_jaudio_wsys_wave, loop_yn1);
AC_SAME_FIELD(Wave_, loopYN2, sl_jaudio_wsys_wave, loop_yn2);
AC_SAME_FIELD(Wave_, fileLoadStatus, sl_jaudio_wsys_wave, file_load_status);
static_assert(sizeof(Wave_) <= offsetof(sl_jaudio_wsys_wave, unknown_28));
AC_SAME_FIELD(CtrlGroup_, magic, sl_jaudio_wsys_ctrl_group, magic);
AC_SAME_FIELD(CtrlGroup_, _04, sl_jaudio_wsys_ctrl_group, scene);
AC_SAME_FIELD(CtrlGroup_, count, sl_jaudio_wsys_ctrl_group, count);
AC_SAME_FIELD(CtrlGroup_, scenes, sl_jaudio_wsys_ctrl_group, scenes);
AC_SAME_FIELD(SCNE_, magic, sl_jaudio_wsys_scene, magic);
AC_SAME_FIELD(SCNE_, _04, sl_jaudio_wsys_scene, unknown_04);
AC_SAME_FIELD(SCNE_, _08, sl_jaudio_wsys_scene, count);
AC_SAME_FIELD(SCNE_, cdf, sl_jaudio_wsys_scene, cdf);
AC_SAME_FIELD(SCNE_, cex, sl_jaudio_wsys_scene, cex);
AC_SAME_FIELD(SCNE_, cst, sl_jaudio_wsys_scene, cst);
AC_SAME_FIELD(SCNE_, _18, sl_jaudio_wsys_scene, ids);
AC_SAME_FIELD(Ctrl_, magic, sl_jaudio_wsys_ctrl, magic);
AC_SAME_FIELD(Ctrl_, count, sl_jaudio_wsys_ctrl, count);
AC_SAME_FIELD(Ctrl_, waveIDs, sl_jaudio_wsys_ctrl, wave_ids);
AC_SAME_FIELD(WaveID_, id, sl_jaudio_wsys_wave_id, id);
AC_SAME_FIELD(WaveID_, heap, sl_jaudio_wsys_wave_id, heap);
AC_SAME_FIELD(WaveID_, loadStatus, sl_jaudio_wsys_wave_id, load_status);
AC_SAME_FIELD(WaveID_, data, sl_jaudio_wsys_wave_id, data);
AC_SAME_SIZE(WaveID_, sl_jaudio_wsys_wave_id);

#undef AC_SAME_FIELD
#undef AC_SAME_SIZE

void append_bytes(std::string& out, const char* name, const std::vector<uint8_t>& bytes) {
    out += "u8 ";
    out += name;
    out += "[] ATTRIBUTE_ALIGN(32) = {\n";
    char text[8];
    for (size_t at = 0; at < bytes.size(); at++) {
        std::snprintf(text, sizeof text, "0x%02x,", bytes[at]);
        out += text;
        out += (at % 16 == 15 || at + 1 == bytes.size()) ? "\n" : "";
    }
    out += "};\n";
}

} // namespace

sl_result lay_out_boot_sound(bool bank, const std::vector<uint8_t>& console, std::vector<uint8_t>& host,
                             std::string& error) {
    char text[256] = {};
    sl_jaudio_file_desc desc = {};
    desc.data = console.data();
    desc.size = console.size();
    desc.error = text;
    desc.error_size = sizeof text;
    size_t size = 0;
    sl_result result = bank ? sl_jaudio_ibnk_layout(&desc, &size) : sl_jaudio_wsys_layout(&desc, &size);
    if (result == SL_OK) {
        try {
            host.assign(size, 0);
        } catch (const std::bad_alloc&) { return SL_ERROR_OUT_OF_MEMORY; }
        desc.image = host.data();
        desc.image_capacity = host.size();
        result = bank ? sl_jaudio_ibnk_layout(&desc, &size) : sl_jaudio_wsys_layout(&desc, &size);
    }
    error = text;
    return result;
}

std::string boot_sound_include(const std::vector<uint8_t>& bank, const std::vector<uint8_t>& wave) {
    std::string out =
        "/* NIN_BANK and NIN_WAVE laid out for the host by ac-boot-sound (port/importer/boot_sound.hpp). */\n";
    append_bytes(out, "NIN_BANK", bank);
    append_bytes(out, "NIN_WAVE", wave);
    return out;
}

} // namespace ac

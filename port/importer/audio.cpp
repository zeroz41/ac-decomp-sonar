/* audiorom.img laid out again for the host (audio.hpp), by sonarlib's jaudio kit (sonar/jaudio/nas_rom.h) from the
   game's own tables (game/audioheaders.c) and this build's limits. */
// The game's headers before the host's: libultra's pad structures name a member errno, which <cerrno> defines.
#include "jaudio_NES/audioheaders.h"
#include "jaudio_NES/system.h"

#include "audio.hpp"

#include <sonar/jaudio/nas_rom.h>

#include <cstddef>
#include <cstring>

// The kit's host structures are the game's (audiostruct.h), as this build's compiler lays them out.
static_assert(sizeof(uaddr) == sizeof(uintptr_t), "a bank's head entries are pointer-wide");
static_assert(sizeof(smzwavetable) == sizeof(sl_jaudio_nas_wavetable) &&
              offsetof(smzwavetable, sample) == offsetof(sl_jaudio_nas_wavetable, sample) &&
              offsetof(smzwavetable, loop) == offsetof(sl_jaudio_nas_wavetable, loop) &&
              offsetof(smzwavetable, book) == offsetof(sl_jaudio_nas_wavetable, book));
static_assert(sizeof(wtstr) == sizeof(sl_jaudio_nas_tuned_sample) &&
              offsetof(wtstr, wavetable) == offsetof(sl_jaudio_nas_tuned_sample, wavetable) &&
              offsetof(wtstr, tuning) == offsetof(sl_jaudio_nas_tuned_sample, tuning));
static_assert(sizeof(voicetable) == sizeof(sl_jaudio_nas_instrument) &&
              offsetof(voicetable, adsr_decay_idx) == offsetof(sl_jaudio_nas_instrument, adsr_decay_idx) &&
              offsetof(voicetable, envelope) == offsetof(sl_jaudio_nas_instrument, envelope) &&
              offsetof(voicetable, low_pitch_tuned_sample) ==
                  offsetof(sl_jaudio_nas_instrument, low_pitch_tuned_sample) &&
              offsetof(voicetable, normal_pitch_tuned_sample) ==
                  offsetof(sl_jaudio_nas_instrument, normal_pitch_tuned_sample) &&
              offsetof(voicetable, high_pitch_tuned_sample) ==
                  offsetof(sl_jaudio_nas_instrument, high_pitch_tuned_sample));
static_assert(sizeof(perctable) == sizeof(sl_jaudio_nas_drum) &&
              offsetof(perctable, pan) == offsetof(sl_jaudio_nas_drum, pan) &&
              offsetof(perctable, is_relocated) == offsetof(sl_jaudio_nas_drum, is_relocated) &&
              offsetof(perctable, tuned_sample) == offsetof(sl_jaudio_nas_drum, tuned_sample) &&
              offsetof(perctable, envelope) == offsetof(sl_jaudio_nas_drum, envelope));
static_assert(sizeof(percvoicetable) == sizeof(sl_jaudio_nas_sound_effect));
static_assert(sizeof(adpcmloop) == sizeof(sl_jaudio_nas_loop) &&
              offsetof(adpcmloop, predictor_state) == offsetof(sl_jaudio_nas_loop, predictor_state));
static_assert(sizeof(adpcmbook) == sizeof(sl_jaudio_nas_book) &&
              offsetof(adpcmbook, codebook) == sizeof(sl_jaudio_nas_book));
static_assert(MEDIUM_CART == SL_JAUDIO_NAS_MEDIUM_CART && CACHE_LOAD_PERSISTENT == SL_JAUDIO_NAS_CACHE_PERSISTENT &&
              CACHE_LOAD_TEMPORARY == SL_JAUDIO_NAS_CACHE_TEMPORARY);

namespace ac {
namespace {

// The engine's limits under SONAR_PORT: NA_RELOCATED_BASE (jaudio_NES/system.h), and the bank caches NA_SPEC_CONFIG
// gives (audioconst.c): one persistent bank's room, and a temporary slot, half the temporary cache. The banks' growth
// is what the larger heap and ARAM leave room for (neosthread.c, game64.c_inc).
constexpr uint32_t persistent_room = 0x6000;
constexpr uint32_t temporary_room = 0x3400 / 2;
constexpr uint32_t max_growth = 0x40000;

std::vector<sl_jaudio_nas_entry> entries_of(const ArcHeader& table) {
    std::vector<sl_jaudio_nas_entry> out(static_cast<size_t>(table.numEntries));
    for (size_t i = 0; i < out.size(); i++) {
        const ArcEntry& entry = table.entries[i];
        out[i].addr = static_cast<uint32_t>(entry.addr);
        out[i].size = static_cast<uint32_t>(entry.size);
        out[i].medium = static_cast<uint8_t>(entry.medium);
        out[i].cache_type = static_cast<uint8_t>(entry.cacheType);
        out[i].instruments = static_cast<uint8_t>(static_cast<uint16_t>(entry.param1) >> 8);
        out[i].drums = static_cast<uint8_t>(entry.param1);
        out[i].sound_effects = static_cast<uint16_t>(entry.param2);
    }
    return out;
}

void put_u32(std::vector<uint8_t>& out, uint32_t value) {
    const size_t at = out.size();
    out.resize(at + 4);
    std::memcpy(out.data() + at, &value, 4);
}

} // namespace

sl_result convert_audiorom(const uint8_t* console, size_t size, AudioRom& out, std::string& error) {
    std::vector<sl_jaudio_nas_entry> banks = entries_of(AudiobankHeaderStart);
    std::vector<sl_jaudio_nas_entry> data = entries_of(AudiodataHeaderStart);
    error.clear();
    if (data.size() <= static_cast<size_t>(BANK_TABLE)) {
        error = "the data table has no bank region";
        return SL_ERROR_IO;
    }
    out.image.assign(size + max_growth, 0);
    char message[256] = {};
    sl_jaudio_nas_rom_desc desc{};
    desc.rom = console;
    desc.rom_size = size;
    desc.data = data.data();
    desc.data_count = static_cast<uint32_t>(data.size());
    desc.bank_region = BANK_TABLE;
    desc.banks = banks.data();
    desc.bank_count = static_cast<uint32_t>(banks.size());
    desc.relocated_base = NA_RELOCATED_BASE;
    desc.persistent_room = persistent_room;
    desc.temporary_room = temporary_room;
    desc.max_growth = max_growth;
    desc.image = out.image.data();
    desc.image_capacity = out.image.size();
    desc.error = message;
    desc.error_size = sizeof message;
    size_t image_size = 0;
    const sl_result result = sl_jaudio_nas_rom_layout(&desc, &image_size);
    if (result != SL_OK) {
        error = message[0] != '\0' ? message : "the layout's arguments";
        return result;
    }
    out.image.resize(image_size);
    out.console_banks = static_cast<uint32_t>(AudiodataHeaderStart.entries[BANK_TABLE].size);
    out.host_banks = data[BANK_TABLE].size;

    out.table.clear();
    put_u32(out.table, audiorom_table_version);
    put_u32(out.table, static_cast<uint32_t>(banks.size()));
    put_u32(out.table, static_cast<uint32_t>(data.size()));
    for (const sl_jaudio_nas_entry& entry : banks) {
        put_u32(out.table, entry.addr);
        put_u32(out.table, entry.size);
    }
    for (const sl_jaudio_nas_entry& entry : data) {
        put_u32(out.table, entry.addr);
        put_u32(out.table, entry.size);
    }
    return SL_OK;
}

} // namespace ac

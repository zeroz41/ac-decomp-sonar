/* The views of the game's three kinds of card file (sonarlib docs/roadmap.md, M6 step 11c; NOTES.md, Saves): the town,
   the passports and the gift letters keep the console's bytes on the card and in the store, and the game reads its
   structures off the kit's host copy, in the host's layout. ac_saves.schema's converters convert each file's
   structures. This file converts what lies around them: the bytes between, the others' protect code, the event save
   areas' data and the checksums. A view runs under the kit's lock, so it calls neither the kit nor core os, and logs
   through sonarlib's log rather than OSReport. */
#include "ac_saves.schema.h"

#include "ac_port.h"

#include <sonar/base/endian.h>
#include <sonar/base/log.h>
#include <sonar/card/memcard.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ---- The event save areas ---- */

/* The structure an event save area's data holds, as (info.type, info.id) picks it (NOTES.md, Saves, the event areas). */
typedef struct event_layout {
    u8 type;
    u8 id;
    u32 size;
    void (*to_host)(void* host, const void* console);
    void (*to_console)(void* console, const void* host);
} event_layout;

#define AREA_DATA_SIZE sizeof(((mEv_area_c*)0)->data)

/* The converters of an area structure, which sits at the start of an int array. */
#define AREA_FUNCTIONS(T)                                                                                              \
    _Static_assert(sizeof(T) <= AREA_DATA_SIZE, #T " fits in an event area's data");                                   \
    _Static_assert(_Alignof(T) <= _Alignof(int), #T " is aligned as an event area's data");                            \
    static void T##_area_to_host(void* host, const void* console) { ac_saves_##T##_from_console((T*)host, console); } \
    static void T##_area_to_console(void* console, const void* host) {                                                 \
        ac_saves_##T##_update_console(console, (const T*)host);                                                        \
    }
#define AREA(type, id, T) { (type), (id), sizeof(T), T##_area_to_host, T##_area_to_console }

AREA_FUNCTIONS(aAPC_event_save_data_c)
AREA_FUNCTIONS(aEv_hatumode_save_c)
AREA_FUNCTIONS(aNTT_event_save_c)
AREA_FUNCTIONS(mEv_dozaemon_c)
AREA_FUNCTIONS(aEANG_event_data_c)
AREA_FUNCTIONS(aEv_tokyoso_c)
AREA_FUNCTIONS(aEv_tunahiki_c)
AREA_FUNCTIONS(mEv_gst_c)
AREA_FUNCTIONS(aESC_event_save_c)
AREA_FUNCTIONS(aEv_turkey_save_c)
AREA_FUNCTIONS(mEv_santa_event_c)
AREA_FUNCTIONS(aEv_yomise_save_c)
AREA_FUNCTIONS(mEv_place_data_c)
AREA_FUNCTIONS(aSNMgr_event_save_c)

/* The summer camper's area holds one mActor_name_t, read through a pointer to it: no structure of the game lays it
   out (ac_event_manager.c:4042-4044, m_npc.c:3592). */
static void camper_to_host(void* host, const void* console) { *(mActor_name_t*)host = sl_load_be16(console); }
static void camper_to_console(void* console, const void* host) { sl_store_be16(console, *(const mActor_name_t*)host); }

/* The pairs the game's mEv_get_save_area and mEv_reserve_save_area calls name, each with the structure it casts the
   area to. */
static const event_layout event_layouts[] = {
    AREA(mEv_EVENT_APRILFOOLS_DAY, 0, aAPC_event_save_data_c),        /* ac_aprilfool_control.c:37-39 */
    AREA(mEv_EVENT_NEW_YEARS_DAY, 7, aEv_hatumode_save_c),            /* ac_hatumode_npc0_talk.c_inc:2-5 */
    AREA(mEv_EVENT_KK_SLIDER, 10, aNTT_event_save_c),                 /* ac_npc_totakeke.c:51-81 */
    AREA(mEv_EVENT_DOZAEMON, 13, mEv_dozaemon_c),                     /* ac_ev_dozaemon.c:58-89 */
    AREA(mEv_EVENT_FISHING_TOURNEY_1, 0, aEANG_event_data_c),         /* ac_ev_angler_move.c_inc:17-63 */
    AREA(mEv_EVENT_FISHING_TOURNEY_2, 0, aEANG_event_data_c),         /* ac_ev_angler_move.c_inc:17-63 */
    AREA(mEv_EVENT_SPORTS_FAIR_FOOT_RACE, 8, aEv_tokyoso_c),          /* ac_tokyoso_control.c:61-64 */
    AREA(mEv_EVENT_SPORTS_FAIR_TUG_OF_WAR, 9, aEv_tunahiki_c),        /* ac_tunahiki_control.c:39-43 */
    AREA(mEv_EVENT_GHOST, 54, mEv_gst_c),                             /* ac_ev_ghost.c:118-132 */
    AREA(mEv_EVENT_SONCHO_BRIDGE_MAKE, 34, aESC_event_save_c),        /* ac_ev_soncho.c:47-55 */
    AREA(mEv_EVENT_HARVEST_FESTIVAL_FRANKLIN, 0, aEv_turkey_save_c),  /* ac_ev_turkey.c:255-258 */
    AREA(mEv_EVENT_TOY_DAY_JINGLE, 0, mEv_santa_event_c),             /* ac_ev_santa.c:114-117 */
    AREA(mEv_EVENT_FIREWORKS_SHOW, 0, aEv_yomise_save_c),             /* ac_ev_yomise_move.c_inc:2-49 */
    { mEv_EVENT_SUMMER_CAMPER, 0, sizeof(mActor_name_t), camper_to_host, camper_to_console },
};

/* Id mEv_EVENT_NUM is an event's lot for any type (ac_event_manager.c:1160-1180, 1772-1774). */
static const event_layout place_layout = AREA(0, mEv_EVENT_NUM, mEv_place_data_c);

/* Id 15 is the set NPC manager's villagers for the types that have an event map (ac_set_npc_manager.c:590-592,
   m_event_map_npc.c:540), the types of l_event_map_type_table (m_event_map_npc.c:12-30). */
static const event_layout set_npc_layout = AREA(0, 15, aSNMgr_event_save_c);
static const u8 event_map_types[] = {
    mEv_EVENT_FIREWORKS_SHOW,      mEv_EVENT_CHERRY_BLOSSOM_FESTIVAL, mEv_EVENT_SPORTS_FAIR_FOOT_RACE,
    mEv_EVENT_SPORTS_FAIR_AEROBICS, mEv_EVENT_SPORTS_FAIR_BALL_TOSS,  mEv_EVENT_SPORTS_FAIR_TUG_OF_WAR,
    mEv_EVENT_HALLOWEEN,           mEv_EVENT_NEW_YEARS_DAY,           mEv_EVENT_FISHING_TOURNEY_1,
    mEv_EVENT_FISHING_TOURNEY_2,   mEv_EVENT_MORNING_AEROBICS,        mEv_EVENT_HARVEST_MOON_FESTIVAL,
    mEv_EVENT_HARVEST_FESTIVAL,    mEv_EVENT_NEW_YEARS_EVE_COUNTDOWN, mEv_EVENT_KAMAKURA,
    mEv_EVENT_GROUNDHOG_DAY,       mEv_EVENT_METEOR_SHOWER,
};

static const event_layout* find_layout(u8 type, u8 id) {
    u32 i;
    if (id == mEv_EVENT_NUM)
        return &place_layout;
    if (id == 15) {
        for (i = 0; i < sizeof event_map_types; ++i) {
            if (event_map_types[i] == type)
                return &set_npc_layout;
        }
    }
    for (i = 0; i < sizeof event_layouts / sizeof event_layouts[0]; ++i) {
        if (event_layouts[i].type == type && event_layouts[i].id == id)
            return &event_layouts[i];
    }
    return NULL;
}

/* The pairs no layout names, each logged once per run. */
static u8 unknown_logged[256 * 256 / 8];

static void log_unknown(u8 type, u8 id) {
    const u32 bit = (u32)type << 8 | id;
    /* A new file's 0xFF bytes show each area in use as (255, 255), a pair no area of the game's has: the game reserves
       areas only of the types it is given (m_event.c:2267-2301), each below mEv_EVENT_NUM (m_event.h:407). */
    if (type == 0xFF && id == 0xFF)
        return;
    if (unknown_logged[bit >> 3] & (1u << (bit & 7)))
        return;
    unknown_logged[bit >> 3] |= (u8)(1u << (bit & 7));
    SL_WARN("saves", "event save area (%u, %u): no layout is known; its data converts as int[11]", type, id);
}

/* The console's data of event save area i in a Save_t at console. */
static size_t area_data_offset(int i) {
    return offsetof(Save_t, event_save_common) + offsetof(mEv_save_common_data_c, area) + (size_t)i * sizeof(mEv_area_c)
           + offsetof(mEv_area_c, data);
}

/* After the Save_t's converter, which leaves each area's data zero: the data of an area in use by its layout, the
   bytes past the layout as they are, and an area not in use as its bytes. */
static void areas_to_host(Save_t* save, const u8* console) {
    int i;
    for (i = 0; i < mEv_AREA_NUM; ++i) {
        mEv_area_c* const area = &save->event_save_common.area[i];
        const u8* const data = console + area_data_offset(i);
        const event_layout* layout;
        memcpy(area->data, data, AREA_DATA_SIZE);
        if ((save->event_save_common.area_use_bitfield & (1 << i)) == 0)
            continue;
        layout = find_layout(area->info.type, area->info.id);
        if (layout != NULL) {
            layout->to_host(area->data, data);
        } else {
            u32 k;
            log_unknown(area->info.type, area->info.id);
            for (k = 0; k < AREA_DATA_SIZE / sizeof area->data[0]; ++k)
                area->data[k] = (int)sl_load_be32(data + k * 4);
        }
    }
}

/* After the Save_t's converter, which keeps each area's data in the console's bytes: the reverse, by the bit and the
   pair in the host's structure. */
static void areas_to_console(const Save_t* save, u8* console) {
    int i;
    for (i = 0; i < mEv_AREA_NUM; ++i) {
        const mEv_area_c* const area = &save->event_save_common.area[i];
        u8* const data = console + area_data_offset(i);
        const event_layout* layout;
        if ((save->event_save_common.area_use_bitfield & (1 << i)) == 0) {
            memcpy(data, area->data, AREA_DATA_SIZE);
            continue;
        }
        layout = find_layout(area->info.type, area->info.id);
        if (layout != NULL) {
            memcpy(data + layout->size, (const u8*)area->data + layout->size, AREA_DATA_SIZE - layout->size);
            layout->to_console(data, area->data);
        } else {
            u32 k;
            log_unknown(area->info.type, area->info.id);
            for (k = 0; k < AREA_DATA_SIZE / sizeof area->data[0]; ++k)
                sl_store_be32(data + k * 4, (u32)area->data[k]);
        }
    }
}

/* ---- The files ---- */

enum { PROTECT_CODE, SAVE, KEEP_MAIL, KEEP_ORIGINAL, KEEP_DIARY, FOREIGNER, PRESENT };

#define NO_CHECKSUM 0xFFFFFFFFu

/* A structure of a file, at offset, and the checksum over [sum_offset, sum_offset + sum_size) at checksum. */
typedef struct region {
    int kind;
    u32 offset;
    u32 size;
    u32 sum_offset;
    u32 sum_size;
    u32 checksum;
} region;

/* Where the structures lie in their files (NOTES.md, Saves). */
enum {
    KEEP_MAIL_AT = 0x1460,
    KEEP_ORIGINAL_AT = 0xCF20,
    KEEP_DIARY_AT = 0x19BC0,
    TOWN_AT = 0x26000,
    TOWN_COPY_AT = 0x4C000,
    FOREIGNER_AT = 0x3040,
};

/* Each structure converts in place in the kit's host copy, which starts on a multiple of SL_MEMCARD_HOST_ALIGN. */
#define IN_PLACE(type, at)                                                                                             \
    _Static_assert(_Alignof(type) <= SL_MEMCARD_HOST_ALIGN, #type " is aligned within the host copy's alignment");     \
    _Static_assert((at) % _Alignof(type) == 0, #type " at " #at " is aligned in the host copy")
IN_PLACE(u16, mCD_SAVE_DATA_OFS);
IN_PLACE(mCD_keep_mail_c, KEEP_MAIL_AT);
IN_PLACE(mCD_keep_original_c, KEEP_ORIGINAL_AT);
IN_PLACE(mCD_keep_diary_c, KEEP_DIARY_AT);
IN_PLACE(Save_t, TOWN_AT);
IN_PLACE(Save_t, TOWN_COPY_AT);
IN_PLACE(mCD_foreigner_c, FOREIGNER_AT);
IN_PLACE(PresentSave_c, mCD_SAVE_DATA_OFS);

/* The file maps, each in the order of its offsets. */
#define SAVE_REGION(at)                                                                                                \
    { SAVE, (at), sizeof(Save_t), (at), sizeof(Save), (at) + offsetof(Save_t, save_check.checksum) }
#define KEEP_REGION(kind, type, at, aligned) { (kind), (at), sizeof(type), (at), (aligned), (at) + offsetof(type, checksum) }
static const region town_regions[] = {
    { PROTECT_CODE, mCD_SAVE_DATA_OFS, 4 * sizeof(u16), 0, 0, NO_CHECKSUM },
    KEEP_REGION(KEEP_MAIL, mCD_keep_mail_c, KEEP_MAIL_AT, mCD_KEEP_MAIL_SIZE),
    KEEP_REGION(KEEP_ORIGINAL, mCD_keep_original_c, KEEP_ORIGINAL_AT, mCD_KEEP_ORIGINAL_SIZE),
    KEEP_REGION(KEEP_DIARY, mCD_keep_diary_c, KEEP_DIARY_AT, mCD_KEEP_DIARY_SIZE),
    SAVE_REGION(TOWN_AT),
    SAVE_REGION(TOWN_COPY_AT),
};
static const region passport_regions[] = {
    KEEP_REGION(FOREIGNER, mCD_foreigner_c, FOREIGNER_AT, sizeof(mCD_foreigner_c)),
};
static const region letters_regions[] = {
    { PRESENT, mCD_SAVE_DATA_OFS, sizeof(PresentSave_c), 0, mCD_PRESENT_SAVE_SIZE,
      mCD_SAVE_DATA_OFS + offsetof(PresentSave_c, checksum) },
};

/* A region converts when it lies in the file; one past its end stays its bytes. */
static BOOL fits(const region* r, u32 length) {
    return r->offset + r->size <= length && r->sum_offset + r->sum_size <= length;
}

static u16 sum_console(const u8* p, u32 size) {
    u16 sum = 0;
    u32 i;
    for (i = 0; i + 1 < size; i += 2)
        sum = (u16)(sum + sl_load_be16(p + i));
    return sum;
}

static u16 sum_host(const u8* p, u32 size) {
    u16 sum = 0;
    u32 i;
    for (i = 0; i + 1 < size; i += 2) {
        u16 word;
        memcpy(&word, p + i, sizeof word);
        sum = (u16)(sum + word);
    }
    return sum;
}

/* The region's structure from the console's bytes at console into the host copy at host, its region's place. */
static void structure_to_host(int kind, const u8* console, u8* host) {
    switch (kind) {
    case SAVE:
        ac_saves_Save_t_from_console((Save_t*)host, console);
        areas_to_host((Save_t*)host, console);
        break;
    case KEEP_MAIL:
        ac_saves_mCD_keep_mail_c_from_console((mCD_keep_mail_c*)host, console);
        break;
    case KEEP_ORIGINAL:
        ac_saves_mCD_keep_original_c_from_console((mCD_keep_original_c*)host, console);
        break;
    case KEEP_DIARY:
        ac_saves_mCD_keep_diary_c_from_console((mCD_keep_diary_c*)host, console);
        break;
    case FOREIGNER:
        ac_saves_mCD_foreigner_c_from_console((mCD_foreigner_c*)host, console);
        break;
    case PRESENT:
        ac_saves_PresentSave_c_from_console((PresentSave_c*)host, console);
        break;
    default: {
        u32 i;
        for (i = 0; i < 4; ++i) {
            const u16 word = sl_load_be16(console + i * 2);
            memcpy(host + i * 2, &word, sizeof word);
        }
        break;
    }
    }
}

/* The host copy's structure at host over the console's bytes at console, which keep the bytes no field names. */
static void structure_to_console(int kind, const u8* host, u8* console) {
    switch (kind) {
    case SAVE:
        ac_saves_Save_t_update_console(console, (const Save_t*)host);
        areas_to_console((const Save_t*)host, console);
        break;
    case KEEP_MAIL:
        ac_saves_mCD_keep_mail_c_update_console(console, (const mCD_keep_mail_c*)host);
        break;
    case KEEP_ORIGINAL:
        ac_saves_mCD_keep_original_c_update_console(console, (const mCD_keep_original_c*)host);
        break;
    case KEEP_DIARY:
        ac_saves_mCD_keep_diary_c_update_console(console, (const mCD_keep_diary_c*)host);
        break;
    case FOREIGNER:
        ac_saves_mCD_foreigner_c_update_console(console, (const mCD_foreigner_c*)host);
        break;
    case PRESENT:
        ac_saves_PresentSave_c_update_console(console, (const PresentSave_c*)host);
        break;
    default: {
        u32 i;
        for (i = 0; i < 4; ++i) {
            u16 word;
            memcpy(&word, host + i * 2, sizeof word);
            sl_store_be16(console + i * 2, word);
        }
        break;
    }
    }
}

/* The file's bytes, then each region's structure, and its checksum set so the host's sum is the console's. */
static void file_to_host(const region* regions, u32 count, const u8* console, u8* host, u32 length) {
    u32 i;
    memcpy(host, console, length);
    for (i = 0; i < count; ++i) {
        const region* const r = &regions[i];
        u16 sum = 0;
        if (!fits(r, length))
            continue;
        if (r->checksum != NO_CHECKSUM)
            sum = sum_console(console + r->sum_offset, r->sum_size);
        structure_to_host(r->kind, console + r->offset, host + r->offset);
        if (r->checksum != NO_CHECKSUM) {
            u16 word;
            memcpy(&word, host + r->checksum, sizeof word);
            word = (u16)(word + sum - sum_host(host + r->sum_offset, r->sum_size));
            memcpy(host + r->checksum, &word, sizeof word);
        }
    }
}

/* The host's bytes outside the regions' structures, then each structure over the console's bytes, and its checksum
   set so the console's sum is the host's. */
static void file_to_console(const region* regions, u32 count, const u8* host, u8* console, u32 length) {
    u32 at = 0;
    u32 i;
    for (i = 0; i < count; ++i) {
        if (!fits(&regions[i], length))
            continue;
        memcpy(console + at, host + at, regions[i].offset - at);
        at = regions[i].offset + regions[i].size;
    }
    memcpy(console + at, host + at, length - at);
    for (i = 0; i < count; ++i) {
        const region* const r = &regions[i];
        u16 sum = 0;
        if (!fits(r, length))
            continue;
        if (r->checksum != NO_CHECKSUM)
            sum = sum_host(host + r->sum_offset, r->sum_size);
        structure_to_console(r->kind, host + r->offset, console + r->offset);
        if (r->checksum != NO_CHECKSUM) {
            const u16 word = sl_load_be16(console + r->checksum);
            sl_store_be16(console + r->checksum, (u16)(word + sum - sum_console(console + r->sum_offset, r->sum_size)));
        }
    }
}

#define COUNT(array) (u32)(sizeof(array) / sizeof((array)[0]))

void ac_saves_town_to_host(void* user, const struct sl_card_dir_entry* entry, const void* console, void* host,
                           uint32_t length) {
    (void)user;
    (void)entry;
    file_to_host(town_regions, COUNT(town_regions), (const u8*)console, (u8*)host, length);
}

void ac_saves_town_to_console(void* user, const struct sl_card_dir_entry* entry, const void* host, void* console,
                              uint32_t length) {
    (void)user;
    (void)entry;
    file_to_console(town_regions, COUNT(town_regions), (const u8*)host, (u8*)console, length);
}

void ac_saves_passport_to_host(void* user, const struct sl_card_dir_entry* entry, const void* console, void* host,
                               uint32_t length) {
    (void)user;
    (void)entry;
    file_to_host(passport_regions, COUNT(passport_regions), (const u8*)console, (u8*)host, length);
}

void ac_saves_passport_to_console(void* user, const struct sl_card_dir_entry* entry, const void* host, void* console,
                                  uint32_t length) {
    (void)user;
    (void)entry;
    file_to_console(passport_regions, COUNT(passport_regions), (const u8*)host, (u8*)console, length);
}

void ac_saves_letters_to_host(void* user, const struct sl_card_dir_entry* entry, const void* console, void* host,
                              uint32_t length) {
    (void)user;
    (void)entry;
    file_to_host(letters_regions, COUNT(letters_regions), (const u8*)console, (u8*)host, length);
}

void ac_saves_letters_to_console(void* user, const struct sl_card_dir_entry* entry, const void* host, void* console,
                                 uint32_t length) {
    (void)user;
    (void)entry;
    file_to_console(letters_regions, COUNT(letters_regions), (const u8*)host, (u8*)console, length);
}

/* The town file under both its names (m_card.c:1526-1527), the passports and the gift letters. */
static const sl_memcard_view views[] = {
    { "DobutsunomoriP_MURA", ac_saves_town_to_host, ac_saves_town_to_console, NULL },
    { "DobutsunomoriP_PL_", ac_saves_passport_to_host, ac_saves_passport_to_console, NULL },
    { "DobutsunomoriP_Omake_", ac_saves_letters_to_host, ac_saves_letters_to_console, NULL },
};

void ac_saves_views(const struct sl_memcard_view** out, uint32_t* count) {
    *out = views;
    *count = COUNT(views);
}

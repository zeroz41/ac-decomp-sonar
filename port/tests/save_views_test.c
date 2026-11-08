/* Checks ac_saves.c's views of the game's card files (NOTES.md, Saves), each case worked by hand in the console's
   bytes: one field of each kind, each container kind of bit-field, each union rule, the event save areas, the dates,
   the others' header, the keep regions' padding, the checksums of each region kind, and random files of each kind to
   the host and back. Offsets are the console's, from the file's start. */
#include "ac_port.h"
#include "m_event.h"
#include "ac_set_npc_manager.h"
#include "ac_tokyoso_control.h"
#include "ac_tunahiki_control.h"

#include <sonar/base/log.h>
#include <sonar/card/memcard.h>

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                       \
    if (!(cond)) {                                        \
        printf("%s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++;                                       \
    }

typedef void (*view_fn)(void* user, const struct sl_card_dir_entry* entry, const void* from, void* to, uint32_t length);

typedef struct file_kind {
    u32 size;
    view_fn to_host;
    view_fn to_console;
} file_kind;

static const file_kind town = { 0x72000, ac_saves_town_to_host, ac_saves_town_to_console };
static const file_kind passport = { 0x6000, ac_saves_passport_to_host, ac_saves_passport_to_console };
static const file_kind letters = { 0x2000, ac_saves_letters_to_host, ac_saves_letters_to_console };

/* The main Save_t's offset in the town file. */
#define SAVE 0x26000

/* The console's file, the host copy the view makes of it, and the console bytes the view writes over a copy of the
   console's, as the kit does. */
static _Alignas(32) u8 console[0x72000];
static _Alignas(32) u8 host[0x72000];
static _Alignas(32) u8 back[0x72000];

static Save_t* const save = (Save_t*)(host + SAVE);

static void to_host(const file_kind* kind) {
    kind->to_host(NULL, NULL, console, host, kind->size);
}

static void to_console(const file_kind* kind) {
    memcpy(back, console, kind->size);
    kind->to_console(NULL, NULL, host, back, kind->size);
}

static void put16(u8* p, u32 v) {
    p[0] = (u8)(v >> 8);
    p[1] = (u8)v;
}

static void put32(u8* p, u32 v) {
    put16(p, v >> 16);
    put16(p + 2, v);
}

static u32 get16(const u8* p) {
    return (u32)p[0] << 8 | p[1];
}

static u32 get32(const u8* p) {
    return get16(p) << 16 | get16(p + 2);
}

/* The game's checksum: the 16-bit sum of a region's 16-bit words, in the console's order or the host's. */
static u16 console_sum(const u8* p, u32 size) {
    u32 sum = 0;
    for (u32 i = 0; i < size; i += 2)
        sum += get16(p + i);
    return (u16)sum;
}

static u16 host_sum(const u8* p, u32 size) {
    u32 sum = 0;
    for (u32 i = 0; i < size; i += 2) {
        u16 word;
        memcpy(&word, p + i, 2);
        sum += word;
    }
    return (u16)sum;
}

static void set_console_sum(u8* region, u32 size, u32 checksum, u16 sum) {
    put16(region + checksum, get16(region + checksum) + sum - console_sum(region, size));
}

static void set_host_sum(u8* region, u32 size, u32 checksum, u16 sum) {
    u16 word;
    memcpy(&word, region + checksum, 2);
    word = (u16)(word + sum - host_sum(region, size));
    memcpy(region + checksum, &word, 2);
}

static u32 rng;

static u32 next_random(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static void fill_random(u8* p, u32 size, u32 seed) {
    rng = seed * 2654435761u + 1;
    for (u32 i = 0; i < size; ++i)
        p[i] = (u8)(next_random() >> 24);
}

/* The records logged under "saves". */
static int saves_logged;

static void count_saves(void* user, const sl_log_record* record) {
    (void)user;
    if (strcmp(record->module, "saves") == 0)
        saves_logged++;
}

/* An event area of the main Save_t: its info at area(i), its data 8 bytes on. */
static u32 area(int i) {
    return SAVE + 0x2056C + (u32)i * 0x34;
}

static void set_area(int i, u8 type, u8 id) {
    console[area(i)] = type;
    console[area(i) + 1] = id;
}

/* A u16, a u32, an s16, a u64, an f32, an lbRTC_time_c's year, a name and a hole. */
static void field_kinds(void) {
    memset(console, 0, town.size);
    put16(console + SAVE + 0x20688, 0x1234);     /* fruit */
    put32(console + SAVE + 0x20468, 0x89ABCDEF); /* shop.sales_sum */
    put16(console + SAVE + 0x21398, 0xFFFE);     /* event_year */
    memcpy(console + SAVE + 0x9F20, "\x01\x23\x45\x67\x89\xAB\xCD\xEF",
           8);                                    /* homes[0].floors[0].layer_main.ftr_switch */
    put16(console + SAVE + 0x20692, 2002);        /* all_grow_renew_time.year */
    memcpy(console + SAVE + 0x20, "TOWNSMAN", 8); /* private_data[0].player_ID.player_name */
    console[SAVE + 0x20345] = 0x5A;               /* shop: the hole after priority_lists */
    put32(console + SAVE + 0x20568, 1);           /* area 0 in use */
    set_area(0, mEv_EVENT_SPORTS_FAIR_TUG_OF_WAR, 9);
    memcpy(console + area(0) + 8, "\x3F\xC0\x00\x00", 4); /* rope, 1.5 */
    to_host(&town);
    CHECK(save->fruit == 0x1234);
    CHECK(save->shop.sales_sum == 0x89ABCDEF);
    CHECK(save->event_year == -2);
    CHECK(save->homes[0].floors[0].layer_main.ftr_switch == 0x0123456789ABCDEFull);
    CHECK(save->all_grow_renew_time.year == 2002);
    CHECK(memcmp(save->private_data[0].player_ID.player_name, "TOWNSMAN", 8) == 0);
    CHECK(host[SAVE + 0x20345] == 0);
    CHECK(((aEv_tunahiki_c*)save->event_save_common.area[0].data)->rope == 1.5f);

    save->fruit = 0x4321;
    save->shop.sales_sum = 0x01020304;
    save->event_year = 300;
    save->homes[0].floors[0].layer_main.ftr_switch = 0xFEDCBA9876543210ull;
    save->all_grow_renew_time.year = 2003;
    memcpy(save->private_data[0].player_ID.player_name, "VILLAGER", 8);
    host[SAVE + 0x20345] = 0x77;
    ((aEv_tunahiki_c*)save->event_save_common.area[0].data)->rope = -2.25f;
    to_console(&town);
    CHECK(memcmp(back + SAVE + 0x20688, "\x43\x21", 2) == 0);
    CHECK(memcmp(back + SAVE + 0x20468, "\x01\x02\x03\x04", 4) == 0);
    CHECK(memcmp(back + SAVE + 0x21398, "\x01\x2C", 2) == 0);
    CHECK(memcmp(back + SAVE + 0x9F20, "\xFE\xDC\xBA\x98\x76\x54\x32\x10", 8) == 0);
    CHECK(memcmp(back + SAVE + 0x20692, "\x07\xD3", 2) == 0);
    CHECK(memcmp(back + SAVE + 0x20, "VILLAGER", 8) == 0);
    CHECK(back[SAVE + 0x20345] == 0x5A);
    CHECK(memcmp(back + area(0) + 8, "\xC0\x10\x00\x00", 4) == 0);
}

/* mQst_base_c's 16 bits, an errand's two signed fields, shop_info's paint_color across a byte, and the mushroom time's
   16-bit and 8-bit containers. */
static void bit_fields(void) {
    mQst_base_c* const base = &save->private_data[0].deliveries[0].base;
    mQst_errand_c* const errand = &save->private_data[0].errands[0];
    mMsr_time_c* const mushroom = &save->mushroom_time;

    memset(console, 0, town.size);
    put16(console + SAVE + 0xB4, 0xADD5);                        /* 10 101101 1 1010 1 01 */
    console[SAVE + 0x336] = 0xFA;                                /* 11111 010 */
    put16(console + SAVE + 0x20466, 0x0580);                     /* 00 0 0 0 1011 0000000 */
    memcpy(console + SAVE + 0x2131C, "\x7D\x2B\xF0\xB8\x38", 5); /* 2002 11, 30, 23, 3 and active */
    to_host(&town);
    CHECK(base->quest_type == 2 && base->quest_kind == 0x2D && base->time_limit_enabled == 1);
    CHECK(base->progress == 10 && base->give_reward == 1 && base->unused == 1);
    CHECK(errand->pockets_idx == -1 && errand->errand_type == mQst_ERRAND_TYPE_FIRST_JOB);
    CHECK(save->shop.shop_info.paint_color == 11 && save->shop.shop_info.shop_level == 0);
    CHECK(save->shop.shop_info.not_loaded_before == 0 && save->shop.shop_info.unused == 0);
    CHECK(mushroom->year == 2002 && mushroom->month == 11 && mushroom->day == 30 && mushroom->hour == 23);
    CHECK(mushroom->hour_quarter == 3 && mushroom->active == 1);

    base->quest_type = 1;
    base->quest_kind = 0x3F;
    base->time_limit_enabled = 0;
    base->progress = 5;
    base->give_reward = 0;
    base->unused = 2;
    errand->pockets_idx = 14;
    errand->errand_type = mQst_ERRAND_TYPE_CHAIN;
    save->shop.shop_info.shop_level = 3;
    save->shop.shop_info.paint_color = 6;
    mushroom->year = 2003;
    mushroom->month = 1;
    mushroom->day = 2;
    mushroom->hour = 8;
    mushroom->hour_quarter = 2;
    mushroom->active = 0;
    to_console(&town);
    CHECK(get16(back + SAVE + 0xB4) == 0x7F2A);    /* 01 111111 0 0101 0 10 */
    CHECK(back[SAVE + 0x336] == 0x71);             /* 01110 001 */
    CHECK(get16(back + SAVE + 0x20466) == 0xC300); /* 11 0 0 0 0110 0000000 */
    CHECK(memcmp(back + SAVE + 0x2131C, "\x7D\x31\x10\x40\x20", 5) == 0);
}

/* The errands under each type, the special event under SHOP, DESIGNER and none, a town villager's and the islander's
   memories, and the contest letter. */
static void unions(void) {
    mQst_errand_c* const errands = save->private_data[0].errands;
    mEv_special_c* const special = &save->event_save_data.special;
    Animal_c* const villager = &save->animals[0];
    Animal_c* const islander = &save->island.animal;

    /* errands[0] at 0x30C is a first job, [1] at 0x364 a chain and [2] at 0x3BC none; each type byte at +0x2A, its
       union at +0x2C. At +0x1C of the union the chain has used_ids[2].npc_id and the first job its bits. */
    memset(console, 0, town.size);
    console[SAVE + 0x336] = 0x02;
    put16(console + SAVE + 0x338 + 0xE, 0xE002);
    console[SAVE + 0x338 + 0x1C] = 0x05; /* used_num 2, wrong_cloth */
    console[SAVE + 0x38E] = 0x01;
    put16(console + SAVE + 0x390 + 0x1C, 0xE003);
    console[SAVE + 0x390 + 0x2A] = 3;
    console[SAVE + 0x3E6] = 0x00;
    put16(console + SAVE + 0x3E8 + 0x1C, 0xE004);
    console[SAVE + 0x3E8 + 0x2A] = 2;
    /* The villager's memory as land at 0x17464, its previous town's name at 0x17D10 and its contest letter at
       0x17D04; the islander's memory as island at 0x2346C and its furniture at 0x23D18. */
    memcpy(console + SAVE + 0x17464, "ABCDEFGH\x30\x05", 10);
    memcpy(console + SAVE + 0x17D10, "IJKLMNOP", 8);
    console[SAVE + 0x17D04] = 0x50;
    put16(console + SAVE + 0x17D06, 0x2A03);
    memcpy(console + SAVE + 0x2346C, "\x11\x22\x33\x44\x55\x66", 6);
    put16(console + SAVE + 0x23D18, 0x1234);
    /* The special event, its kind at 0x204A0 and its union at 0x204A4: SHOP's items[0] at +0x10. */
    put32(console + SAVE + 0x204A0, mEv_SPNPC_SHOP);
    put16(console + SAVE + 0x204A4 + 0x10, 0x2001);
    to_host(&town);
    CHECK(errands[0].errand_type == mQst_ERRAND_TYPE_FIRST_JOB);
    CHECK(errands[0].info.first_job.used_ids[1].npc_id == 0xE002);
    CHECK(errands[0].info.first_job.used_num == 2 && errands[0].info.first_job.wrong_cloth == 1);
    CHECK(errands[1].errand_type == mQst_ERRAND_TYPE_CHAIN);
    CHECK(errands[1].info.chain.used_ids[2].npc_id == 0xE003 && errands[1].info.chain.used_num == 3);
    CHECK(errands[2].errand_type == mQst_ERRAND_TYPE_NONE);
    CHECK(errands[2].info.chain.used_ids[2].npc_id == 0xE004 && errands[2].info.chain.used_num == 2);
    CHECK(memcmp(villager->memories[0].memuni.land.name, "ABCDEFGH", 8) == 0);
    CHECK(villager->memories[0].memuni.land.id == 0x3005);
    CHECK(memcmp(villager->anmuni.previous_land_name, "IJKLMNOP", 8) == 0);
    CHECK(villager->contest_quest.info.letter_data.score == 0x50);
    CHECK(villager->contest_quest.info.letter_data.present == 0x2A03);
    CHECK(islander->memories[0].memuni.island.check == 0x11223344);
    CHECK(islander->memories[0].memuni.island.have_bitfield == 0x5566);
    CHECK(islander->anmuni.island_ftr[0] == 0x1234);
    CHECK(special->kind == mEv_SPNPC_SHOP && special->event.bargin.items[0] == 0x2001);

    errands[0].info.first_job.used_num = 5;
    errands[1].info.chain.used_num = 1;
    errands[2].info.chain.used_ids[2].npc_id = 0xE005;
    villager->memories[0].memuni.land.id = 0x3006;
    villager->contest_quest.info.letter_data.present = 0x2A04;
    islander->memories[0].memuni.island.check = 0x55667788;
    islander->anmuni.island_ftr[0] = 0x1235;
    special->event.bargin.items[0] = 0x2002;
    to_console(&town);
    CHECK(back[SAVE + 0x338 + 0x1C] == 0x0B);
    CHECK(back[SAVE + 0x390 + 0x2A] == 1);
    CHECK(get16(back + SAVE + 0x3E8 + 0x1C) == 0xE005);
    CHECK(memcmp(back + SAVE + 0x17464, "ABCDEFGH\x30\x06", 10) == 0);
    CHECK(memcmp(back + SAVE + 0x17D10, "IJKLMNOP", 8) == 0);
    CHECK(get16(back + SAVE + 0x17D06) == 0x2A04);
    CHECK(memcmp(back + SAVE + 0x2346C, "\x55\x66\x77\x88\x55\x66", 6) == 0);
    CHECK(get16(back + SAVE + 0x23D18) == 0x1235);
    CHECK(get16(back + SAVE + 0x204A4 + 0x10) == 0x2002);

    /* DESIGNER: used at +0x3C, gifted_cloths[0] at +0x40. */
    memset(console, 0, town.size);
    put32(console + SAVE + 0x204A0, mEv_SPNPC_DESIGNER);
    put32(console + SAVE + 0x204A4 + 0x3C, 1);
    put16(console + SAVE + 0x204A4 + 0x40, 0x2403);
    to_host(&town);
    CHECK(special->kind == mEv_SPNPC_DESIGNER);
    CHECK(special->event.designer.used == 1 && special->event.designer.gifted_cloths[0] == 0x2403);
    special->event.designer.gifted_cloths[0] = 0x2404;
    to_console(&town);
    CHECK(get32(back + SAVE + 0x204A4 + 0x3C) == 1 && get16(back + SAVE + 0x204A4 + 0x40) == 0x2404);

    /* None: the union's bytes as they are. */
    memset(console, 0, town.size);
    put32(console + SAVE + 0x204A0, 0xFFFFFFFF);
    put16(console + SAVE + 0x204A4 + 0x10, 0x2001);
    to_host(&town);
    CHECK(special->kind == 0xFFFFFFFF);
    CHECK(memcmp((u8*)&special->event + 0x10, "\x20\x01", 2) == 0);
    memcpy((u8*)&special->event + 0x10, "\x12\x34", 2);
    to_console(&town);
    CHECK(memcmp(back + SAVE + 0x204A4 + 0x10, "\x12\x34", 2) == 0);
}

/* A FOOT_RACE area, a place area, a set-npc area, an area no table names and a free area, both ways. The bits of
   area_use_bitfield are at 0x20568. */
static void event_areas(void) {
    mEv_area_c* const areas = save->event_save_common.area;
    const aEv_tokyoso_c* race;
    const mEv_place_data_c* place;
    int logged;

    memset(console, 0, town.size);
    put32(console + SAVE + 0x20568, 0x0F);
    set_area(0, mEv_EVENT_SPORTS_FAIR_FOOT_RACE, 8);
    put16(console + area(0) + 2, 2002);
    memcpy(console + area(0) + 8, "\x11\x22\x33\x44\xFF\x00", 6); /* _00, _01, flags, yasiro_pos[0] */
    memcpy(console + area(0) + 8 + 0x1A, "\x01\x02\x03\x04", 4);  /* lap, goal */
    for (int k = 0x1E; k < 44; ++k)
        console[area(0) + 8 + k] = (u8)(0xA0 + k);
    set_area(1, mEv_EVENT_KK_SLIDER, mEv_EVENT_NUM);
    memcpy(console + area(1) + 8, "\0\0\0\x03\0\0\0\x04\xFF\xFF\xFF\xFF\0\0\0\x07\xA0\x0B\xFF\xFD", 20);
    set_area(2, mEv_EVENT_FIREWORKS_SHOW, 15);
    for (int k = 0; k < 44; ++k)
        console[area(2) + 8 + k] = (u8)k;
    set_area(3, mEv_EVENT_GHOST, 15); /* no map, so not the set NPC manager's */
    for (int k = 0; k < 11; ++k)
        put32(console + area(3) + 8 + k * 4, 0x10000000u + (u32)k);
    set_area(4, mEv_EVENT_SPORTS_FAIR_FOOT_RACE, 8);
    for (int k = 0; k < 44; ++k)
        console[area(4) + 8 + k] = (u8)(0x30 + k);

    logged = saves_logged;
    to_host(&town);
    CHECK(saves_logged == logged + 1);
    race = (const aEv_tokyoso_c*)areas[0].data;
    CHECK(areas[0].info.type == mEv_EVENT_SPORTS_FAIR_FOOT_RACE && areas[0].info.id == 8);
    CHECK(areas[0].info.year == 2002);
    CHECK(race->_00 == 0x11 && race->_01 == 0x22 && race->flags == 0x3344 && race->yasiro_pos[0] == -256);
    CHECK(race->lap[0] == 1 && race->lap[1] == 2 && race->goal[0] == 3 && race->goal[1] == 4);
    CHECK(memcmp((u8*)areas[0].data + 0x1E, console + area(0) + 8 + 0x1E, 44 - 0x1E) == 0);
    place = (const mEv_place_data_c*)areas[1].data;
    CHECK(place->block.z == 3 && place->block.x == 4 && place->unit.z == -1 && place->unit.x == 7);
    CHECK(place->actor_name == 0xA00B && place->flag == -3);
    CHECK(memcmp(areas[2].data, console + area(2) + 8, 44) == 0);
    for (int k = 0; k < 11; ++k)
        CHECK(areas[3].data[k] == 0x10000000 + k);
    CHECK(memcmp(areas[4].data, console + area(4) + 8, 44) == 0);

    ((aEv_tokyoso_c*)areas[0].data)->flags = 0x5566;
    ((aEv_tokyoso_c*)areas[0].data)->yasiro_pos[0] = -2;
    ((u8*)areas[0].data)[0x20] = 0x42; /* past the structure */
    ((mEv_place_data_c*)areas[1].data)->actor_name = 0xA00C;
    ((mEv_place_data_c*)areas[1].data)->unit.x = 8;
    ((aSNMgr_event_save_c*)areas[2].data)->animal_idx[0] = 9;
    areas[3].data[0] = 0x0A0B0C0D;
    ((u8*)areas[4].data)[0] = 0x99;
    to_console(&town);
    CHECK(memcmp(back + area(0) + 8, "\x11\x22\x55\x66\xFF\xFE", 6) == 0);
    CHECK(memcmp(back + area(0) + 8 + 0x1A, console + area(0) + 8 + 0x1A, 0x20 - 0x1A) == 0);
    CHECK(back[area(0) + 8 + 0x20] == 0x42);
    CHECK(memcmp(back + area(0) + 8 + 0x21, console + area(0) + 8 + 0x21, 44 - 0x21) == 0);
    CHECK(memcmp(back + area(1) + 8 + 0xC, "\0\0\0\x08\xA0\x0C\xFF\xFD", 8) == 0);
    CHECK(back[area(2) + 8] == 9 && memcmp(back + area(2) + 9, console + area(2) + 9, 43) == 0);
    CHECK(memcmp(back + area(3) + 8, "\x0A\x0B\x0C\x0D", 4) == 0);
    CHECK(memcmp(back + area(3) + 12, console + area(3) + 12, 40) == 0);
    CHECK(back[area(4) + 8] == 0x99 && memcmp(back + area(4) + 9, console + area(4) + 9, 43) == 0);

    /* The unknown pair was logged once in the run. */
    to_host(&town);
    CHECK(saves_logged == logged + 1);
}

/* An event area's start_date, set by its month and day on the host, and renew_time, at 0x20F14. */
static void dates(void) {
    mEv_info_c* const info = &save->event_save_common.area[0].info;

    memset(console, 0, town.size);
    put32(console + SAVE + 0x20568, 1);
    set_area(0, mEv_EVENT_SPORTS_FAIR_FOOT_RACE, 8);
    memcpy(console + SAVE + 0x20F14, "\x07\xD2\x0B\x1E", 4);
    to_host(&town);
    CHECK(save->renew_time.year == 2002 && save->renew_time.month == 11 && save->renew_time.day == 30);
    info->start_date.month = 12;
    info->start_date.day = 25;
    save->renew_time.year = 2003;
    save->renew_time.month = 1;
    save->renew_time.day = 2;
    to_console(&town);
    CHECK(memcmp(back + area(0) + 4, "\x0C\x19", 2) == 0);
    CHECK(memcmp(back + SAVE + 0x20F14, "\x07\xD3\x01\x02", 4) == 0);

    memcpy(console, back, town.size);
    to_host(&town);
    CHECK(info->start_date.month == 12 && info->start_date.day == 25);
    CHECK(info->start_date.raw == 0x0C19);
}

/* The town file's first 0x1460 bytes: the comment, banner and icon, the protect code at 0x1440, then zero. */
static void others_header(void) {
    u16 words[4];

    memset(console, 0, town.size);
    fill_random(console, 0x1440, 1);
    memcpy(console + 0x1440, "\x3C\x1C\x12\x34\x56\x78\x9A\xBC", 8);
    to_host(&town);
    memcpy(words, host + 0x1440, sizeof words);
    CHECK(words[0] == 0x3C1C && words[1] == 0x1234 && words[2] == 0x5678 && words[3] == 0x9ABC);
    CHECK(memcmp(host, console, 0x1440) == 0);

    host[0] ^= 0xFF;
    words[3] = 0x9ABD;
    memcpy(host + 0x1440, words, sizeof words);
    to_console(&town);
    CHECK(back[0] == (u8)~console[0] && memcmp(back + 1, console + 1, 0x1440 - 1) == 0);
    CHECK(memcmp(back + 0x1440, "\x3C\x1C\x12\x34\x56\x78\x9A\xBD", 8) == 0);
}

/* keep_mail at 0x1460: 0xBAA4 bytes of structure, then padding to 0xBAC0, all in its checksum's range. */
static void keep_padding(void) {
    memset(console, 0, town.size);
    fill_random(console + 0x1460, 0xBAA4, 2);
    memset(console + 0x1460 + 0xBAA4, 0xA5, 0x1C);
    set_console_sum(console + 0x1460, 0xBAC0, 0, 0);
    to_host(&town);
    for (u32 i = 0xBAA4; i < 0xBAC0; ++i)
        CHECK(host[0x1460 + i] == 0xA5);
    CHECK(host_sum(host + 0x1460, 0xBAC0) == 0);
    to_console(&town);
    for (u32 i = 0xBAA4; i < 0xBAC0; ++i)
        CHECK(back[0x1460 + i] == 0xA5);
    CHECK(console_sum(back + 0x1460, 0xBAC0) == 0);
}

/* Each region kind's checksum: the region, its range and its checksum's offset in it. */
typedef struct region_case {
    const char* name;
    const file_kind* kind;
    u32 offset;
    u32 size;
    u32 checksum;
} region_case;

static const region_case regions[] = {
    { "town", &town, 0x26000, 0x26000, 0x12 },       { "backup town", &town, 0x4C000, 0x26000, 0x12 },
    { "keep_mail", &town, 0x1460, 0xBAC0, 0 },       { "keep_original", &town, 0xCF20, 0xCCA0, 0 },
    { "keep_diary", &town, 0x19BC0, 0xBA20, 0 },     { "passport", &passport, 0x3040, 0x2E20, 0 },
    { "gift letters", &letters, 0, 0x2000, 0x1440 },
};

static void checksums(void) {
    for (u32 r = 0; r < sizeof regions / sizeof regions[0]; ++r) {
        const region_case* const c = &regions[r];
        const u32 size = c->kind->size;
        const int before = failures;
        u16 sum;

        /* Valid on the console reads valid on the host, and the host's valid region writes a valid console one. */
        fill_random(console, size, 10 + r);
        set_console_sum(console + c->offset, c->size, c->checksum, 0);
        to_host(c->kind);
        CHECK(host_sum(host + c->offset, c->size) == 0);
        host[c->offset + c->size - 1] ^= 0x5A;
        set_host_sum(host + c->offset, c->size, c->checksum, 0);
        to_console(c->kind);
        CHECK(console_sum(back + c->offset, c->size) == 0);

        /* A word off by one reads off by one, and writes so. */
        fill_random(console, size, 20 + r);
        set_console_sum(console + c->offset, c->size, c->checksum, 0);
        put16(console + c->offset + c->size - 2, get16(console + c->offset + c->size - 2) + 1);
        to_host(c->kind);
        CHECK(host_sum(host + c->offset, c->size) == 1);
        to_console(c->kind);
        CHECK(console_sum(back + c->offset, c->size) == 1);
        CHECK(memcmp(back, console, size) == 0);

        /* An erased town: its checksum 0 and its sum not. */
        if (c->kind == &town && c->checksum == 0x12) {
            fill_random(console, size, 30 + r);
            put16(console + c->offset + c->checksum, 0);
            sum = console_sum(console + c->offset, c->size);
            CHECK(sum != 0);
            to_host(c->kind);
            CHECK(host_sum(host + c->offset, c->size) == sum);
            to_console(c->kind);
            CHECK(get16(back + c->offset + c->checksum) == 0 && console_sum(back + c->offset, c->size) == sum);
        }
        if (failures != before)
            printf("  in the %s region\n", c->name);
    }
}

/* The pairs the event table names, then a place, a set NPC manager's and one no table names. */
static const u8 area_pairs[][2] = {
    { mEv_EVENT_APRILFOOLS_DAY, 0 },
    { mEv_EVENT_NEW_YEARS_DAY, 7 },
    { mEv_EVENT_KK_SLIDER, 10 },
    { mEv_EVENT_DOZAEMON, 13 },
    { mEv_EVENT_FISHING_TOURNEY_1, 0 },
    { mEv_EVENT_FISHING_TOURNEY_2, 0 },
    { mEv_EVENT_SPORTS_FAIR_FOOT_RACE, 8 },
    { mEv_EVENT_SPORTS_FAIR_TUG_OF_WAR, 9 },
    { mEv_EVENT_GHOST, 54 },
    { mEv_EVENT_SONCHO_BRIDGE_MAKE, 34 },
    { mEv_EVENT_HARVEST_FESTIVAL_FRANKLIN, 0 },
    { mEv_EVENT_TOY_DAY_JINGLE, 0 },
    { mEv_EVENT_FIREWORKS_SHOW, 0 },
    { mEv_EVENT_SUMMER_CAMPER, 0 },
    { mEv_EVENT_GHOST, mEv_EVENT_NUM },
    { mEv_EVENT_KAMAKURA, 15 },
    { mEv_EVENT_GHOST, 15 },
};

/* Random console bytes of each kind of file, to the host and back over themselves, are their bytes again. A town's
   areas take each pair in turn, in use or not, and its special event each kind. */
static void round_trips(void) {
    static const u32 kinds[] = { 0, 1, 2, 3, 4, 5, 0xFFFFFFFF };
    const file_kind* const files[] = { &town, &passport, &letters };
    const u32 pair_count = sizeof area_pairs / sizeof area_pairs[0];

    for (u32 f = 0; f < 3; ++f) {
        for (u32 seed = 0; seed < 64; ++seed) {
            fill_random(console, files[f]->size, 1000 * (f + 1) + seed);
            if (files[f] == &town) {
                for (u32 copy = 0; copy < 2; ++copy) {
                    const u32 at = SAVE + copy * 0x26000;
                    if (seed % 2 == 0)
                        console[at + 0x2056B] |= 0x1F;
                    for (int i = 0; i < 5; ++i) {
                        const u8* const pair = area_pairs[(seed * 5 + (u32)i) % pair_count];
                        console[at + 0x2056C + (u32)i * 0x34] = pair[0];
                        console[at + 0x2056C + (u32)i * 0x34 + 1] = pair[1];
                    }
                    put32(console + at + 0x204A0, kinds[seed % 7]);
                }
            }
            to_host(files[f]);
            to_console(files[f]);
            if (memcmp(back, console, files[f]->size) != 0) {
                u32 i = 0;
                while (back[i] == console[i])
                    ++i;
                printf("file %u seed %u: byte 0x%X is 0x%02X, not 0x%02X\n", f, seed, i, back[i], console[i]);
                failures++;
            }
        }
    }
}

int main(void) {
    const struct sl_memcard_view* views;
    uint32_t view_count = 0;

    sl_log_console(0);
    sl_log_add_sink(count_saves, NULL);

    ac_saves_views(&views, &view_count);
    CHECK(view_count == 3);
    CHECK(strcmp(views[0].prefix, "DobutsunomoriP_MURA") == 0 && views[0].to_host == ac_saves_town_to_host);
    CHECK(strcmp(views[1].prefix, "DobutsunomoriP_PL_") == 0 && views[1].to_host == ac_saves_passport_to_host);
    CHECK(strcmp(views[2].prefix, "DobutsunomoriP_Omake_") == 0 && views[2].to_host == ac_saves_letters_to_host);

    field_kinds();
    bit_fields();
    unions();
    event_areas();
    dates();
    others_header();
    keep_padding();
    checksums();
    round_trips();

    if (failures == 0)
        printf("ok\n");
    return failures != 0;
}

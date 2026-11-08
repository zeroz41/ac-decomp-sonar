/* Checks the port's card functions (adapter/dolphin_card.c) on a memory store: a card inserted before CARDInit, the
   calls that return their result and those that complete in interrupt context, the status the game reads and
   writes, a file kept across an eject, and the game's files through the views (ac_saves.c) with their time. */
/* card.h takes the OS types from os.h. */
#include "dolphin/os.h"
#include "dolphin/card.h"
#include "card/__card.h"
#include "dolphin/dvd.h"

#include "ac_port.h"

#include <sonar/base/time.h>
#include <sonar/card/format.h>
#include <sonar/card/memcard.h>
#include <sonar/os.h>

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                                                                   \
    if (!(cond)) {                                                                                                    \
        printf("%s:%d: %s\n", __FILE__, __LINE__, #cond);                                                             \
        failures++;                                                                                                   \
    }

void OSReport(const char* fmt, ...) { (void)fmt; }

static DVDDiskID disc_id = {{'G', 'A', 'F', 'E'}, {'0', '1'}};

DVDDiskID* DVDGetCurrentDiskID(void) { return &disc_id; }

u16 OSGetFontEncode(void) { return 0; }

static uint64_t now_ns(void* user) {
    (void)user;
    return 0;
}

static s32 called_chan = -9, called_result = 1;
static int called_count, called_in_interrupt;

static void card_done(s32 chan, s32 result) {
    called_chan = chan;
    called_result = result;
    called_count++;
    called_in_interrupt += sl_os_irq_active() != 0;
}

static const char save_name[] = "DobutsunomoriP_F_SAVE";

/* size console bytes from offset of slot A's file file_no, as the store holds them. */
static int stored_bytes(sl_save_store store, s32 file_no, u32 offset, void* out, u32 size) {
    char name[32];
    sl_save save;
    sl_save_section section;
    int found = 0;
    snprintf(name, sizeof name, "card-a-f%03d", (int)file_no);
    if (sl_save_load(store, name, &save, NULL) != SL_OK)
        return 0;
    if (sl_save_find_section(save, "card.gci", &section) == SL_OK &&
        section.size >= SL_CARD_GCI_HEADER_SIZE + offset + size) {
        memcpy(out, (const u8*)section.data + SL_CARD_GCI_HEADER_SIZE + offset, size);
        found = 1;
    }
    sl_save_release(save);
    return found;
}

static double ms_since(uint64_t start) { return (double)(sl_time_now_ns() - start) / 1e6; }

/* The game's three kinds of file through the views, on slot A's mounted card: each written through CARDWrite from
   the host's bytes, held in the console's by the store, and read back in the host's after an eject. A new town's
   save is timed as mCD_write_comp_bg does it, each of its 57 sectors read and then written, and one whole-town
   conversion each way. */
static void views_through_adapter(sl_save_store store, void* work) {
    static _Alignas(32) u8 town[0x72000], console[0x72000], host[0x72000], file[0x6000], sector[0x2000];
    CARDFileInfo info;
    s32 town_no, passport_no, letters_no;
    uint64_t start;
    double reads = 0, writes = 0, to_host, to_console;
    u32 at;
    int value;
    u16 half;

    memset(town, 0, sizeof town);
    value = 6;
    memcpy(town + 0x26000, &value, 4); /* save_check.version */
    value = 0x47414645;
    memcpy(town + 0x26004, &value, 4); /* save_check.code, 'GAFE' */
    CHECK(CARDCreate(0, "DobutsunomoriP_MURA", sizeof town, &info) == CARD_RESULT_READY);
    town_no = info.fileNo;
    for (at = 0; at < sizeof town; at += sizeof sector) {
        start = sl_time_now_ns();
        CHECK(CARDRead(&info, sector, sizeof sector, (s32)at) == CARD_RESULT_READY);
        reads += ms_since(start);
        start = sl_time_now_ns();
        CHECK(CARDWrite(&info, town + at, sizeof sector, (s32)at) == CARD_RESULT_READY);
        writes += ms_since(start);
    }
    CHECK(CARDClose(&info) == CARD_RESULT_READY);
    CHECK(stored_bytes(store, town_no, 0x26000, sector, 8));
    CHECK(memcmp(sector, "\0\0\0\x06GAFE", 8) == 0);

    memset(file, 0, sizeof file);
    half = 0x1234;
    memcpy(file + 0x3070, &half, 2); /* priv.player_ID.player_id */
    CHECK(CARDCreate(0, "DobutsunomoriP_PL_1", 0x6000, &info) == CARD_RESULT_READY);
    passport_no = info.fileNo;
    CHECK(CARDWrite(&info, file, 0x6000, 0) == CARD_RESULT_READY);
    CHECK(CARDClose(&info) == CARD_RESULT_READY);
    CHECK(stored_bytes(store, passport_no, 0x3070, sector, 2));
    CHECK(memcmp(sector, "\x12\x34", 2) == 0);

    memset(file, 0, sizeof file);
    half = 3;
    memcpy(file + 0x1442, &half, 2); /* present_count */
    CHECK(CARDCreate(0, "DobutsunomoriP_Omake_0", 0x2000, &info) == CARD_RESULT_READY);
    letters_no = info.fileNo;
    CHECK(CARDWrite(&info, file, 0x2000, 0) == CARD_RESULT_READY);
    CHECK(CARDClose(&info) == CARD_RESULT_READY);
    CHECK(stored_bytes(store, letters_no, 0x1442, sector, 2));
    CHECK(memcmp(sector, "\x00\x03", 2) == 0);

    /* Put back, the card's host copies are made again from the console's bytes. */
    ac_card_eject(0);
    CHECK(sl_os_irq_dispatch() == SL_OK);
    CHECK(ac_card_insert(0, "card-a"));
    CHECK(CARDMount(0, work, NULL) == CARD_RESULT_READY);
    CHECK(CARDFastOpen(0, town_no, &info) == CARD_RESULT_READY);
    CHECK(CARDRead(&info, sector, CARD_READ_SIZE, 0x26000) == CARD_RESULT_READY);
    memcpy(&value, sector, 4);
    CHECK(value == 6);
    memcpy(&value, sector + 4, 4);
    CHECK(value == 0x47414645);
    CHECK(CARDClose(&info) == CARD_RESULT_READY);
    CHECK(CARDFastOpen(0, passport_no, &info) == CARD_RESULT_READY);
    CHECK(CARDRead(&info, sector, CARD_READ_SIZE, 0x3000) == CARD_RESULT_READY);
    memcpy(&half, sector + 0x70, 2);
    CHECK(half == 0x1234);
    CHECK(CARDClose(&info) == CARD_RESULT_READY);
    CHECK(CARDFastOpen(0, letters_no, &info) == CARD_RESULT_READY);
    CHECK(CARDRead(&info, sector, CARD_READ_SIZE, 0x1400) == CARD_RESULT_READY);
    memcpy(&half, sector + 0x42, 2);
    CHECK(half == 3);
    CHECK(CARDClose(&info) == CARD_RESULT_READY);

    CHECK(stored_bytes(store, town_no, 0, console, sizeof console));
    start = sl_time_now_ns();
    ac_saves_town_to_host(NULL, NULL, console, host, sizeof host);
    to_host = ms_since(start);
    start = sl_time_now_ns();
    ac_saves_town_to_console(NULL, NULL, host, console, sizeof console);
    to_console = ms_since(start);
    CHECK(memcmp(host, town, sizeof town) == 0);
    printf("town save, 57 sectors: reads %.2f ms, writes %.2f ms; one town to_host %.2f ms, to_console %.2f ms\n",
           reads, writes, to_host, to_console);
}

int main(void) {
    sl_os_desc os = {0};
    sl_os_thread_desc thread = {0};
    sl_os_thread self;
    sl_save_store_desc store_desc = {0};
    sl_save_store store;
    CARDFileInfo info;
    CARDStat stat;
    CARDDir dir;
    s32 mem = 0, sector = 0, bytes = 0, files = 0;
    u8 attr = 0xEE;
    static u8 data[2 * 8192], back[2 * 8192];
    static u8 work[CARD_WORKAREA_SIZE];

    os.deterministic = 1;
    os.time_fn = now_ns;
    thread.name = "test-main";
    store_desc.flags = SL_SAVE_STORE_MEMORY | SL_SAVE_STORE_NO_BACKUPS;
    if (sl_os_start(&os) != SL_OK || sl_os_thread_attach(&thread, &self) != SL_OK ||
        sl_save_store_open(&store_desc, &store) != SL_OK) {
        printf("cannot start core os\n");
        return 2;
    }
    for (int i = 0; i < (int)sizeof data; ++i)
        data[i] = (u8)(i * 7 + 3);

    /* Before CARDInit, a card waits for it; CARDInit puts it in slot A and leaves B empty. */
    ac_card_set_store(store);
    CHECK(ac_card_insert(0, "card-a"));
    CHECK(!ac_card_insert(0, "card-b"));
    CHECK(!ac_card_insert(2, "card-c"));
    CARDInit();
    CARDInit();
    CHECK(CARDProbeEx(0, &mem, &sector) == CARD_RESULT_READY);
    CHECK(mem == 16 && sector == 8192);
    CHECK(CARDProbeEx(0, NULL, NULL) == CARD_RESULT_READY);
    CHECK(CARDProbe(0) && !CARDProbe(1));
    mem = 5;
    CHECK(CARDProbeEx(1, &mem, NULL) == CARD_RESULT_NOCARD);
    CHECK(mem == 5);
    CHECK(CARDGetResultCode(1) == CARD_RESULT_NOCARD);

    /* The calls without Async return their result. */
    CHECK(CARDOpen(0, save_name, &info) == CARD_RESULT_NOCARD);
    CHECK(CARDMount(0, work, card_done) == CARD_RESULT_READY);
    CHECK(CARDCheck(0) == CARD_RESULT_READY);
    CHECK(CARDFreeBlocks(0, &bytes, &files) == CARD_RESULT_READY);
    CHECK(bytes == 251 * 8192 && files == 127);
    CHECK(CARDOpen(0, save_name, &info) == CARD_RESULT_NOFILE);
    memset(&info, 0x55, sizeof info);
    CHECK(CARDCreate(0, save_name, sizeof data, &info) == CARD_RESULT_READY);
    CHECK(info.chan == 0 && info.fileNo == 0 && info.offset == 0 && info.iBlock == 0);
    CHECK(CARDCreate(0, save_name, sizeof data, &info) == CARD_RESULT_EXIST);
    CHECK(CARDGetResultCode(0) == CARD_RESULT_EXIST);
    CHECK(CARDWrite(&info, data, sizeof data, 0) == CARD_RESULT_READY);
    CHECK(CARDRead(&info, back, CARD_READ_SIZE, 8192) == CARD_RESULT_READY);
    CHECK(memcmp(back, data + 8192, CARD_READ_SIZE) == 0);
    CHECK(CARDRead(&info, back, 100, 0) == CARD_RESULT_FATAL_ERROR);
    CHECK(CARDFreeBlocks(0, &bytes, NULL) == CARD_RESULT_READY);
    CHECK(bytes == 249 * 8192);

    /* With Async the result waits for the interrupt, BUSY until then, with or without a callback. */
    memset(back, 0, sizeof back);
    CHECK(CARDReadAsync(&info, back, sizeof back, 0, card_done) == CARD_RESULT_READY);
    CHECK(CARDGetResultCode(0) == CARD_RESULT_BUSY);
    CHECK(CARDRead(&info, back, CARD_READ_SIZE, 0) == CARD_RESULT_BUSY);
    CHECK(called_count == 0);
    CHECK(sl_os_irq_dispatch() == SL_OK);
    CHECK(called_count == 1 && called_in_interrupt == 1);
    CHECK(called_chan == 0 && called_result == CARD_RESULT_READY);
    CHECK(memcmp(back, data, sizeof data) == 0);
    CHECK(CARDGetResultCode(0) == CARD_RESULT_READY);
    data[0] ^= 0xFF;
    CHECK(CARDWriteAsync(&info, data, 8192, 0, NULL) == CARD_RESULT_READY);
    CHECK(CARDGetResultCode(0) == CARD_RESULT_BUSY);
    CHECK(sl_os_irq_dispatch() == SL_OK);
    CHECK(CARDGetResultCode(0) == CARD_RESULT_READY);
    CHECK(called_count == 1);

    /* The status the game reads, and the graphics it sets. */
    memset(&stat, 0, sizeof stat);
    CHECK(CARDGetStatus(0, info.fileNo, &stat) == CARD_RESULT_READY);
    CHECK(strcmp(stat.fileName, save_name) == 0);
    CHECK(stat.length == sizeof data);
    CHECK(memcmp(stat.gameName, "GAFE", 4) == 0 && memcmp(stat.company, "01", 2) == 0);
    stat.bannerFormat = CARD_STAT_BANNER_RGB5A3;
    stat.iconAddr = 0;
    stat.iconFormat = CARD_STAT_ICON_RGB5A3;
    stat.iconSpeed = CARD_STAT_SPEED_SLOW;
    stat.commentAddr = 0x1C00;
    CHECK(CARDSetStatusAsync(0, info.fileNo, &stat, card_done) == CARD_RESULT_READY);
    CHECK(sl_os_irq_dispatch() == SL_OK);
    CHECK(called_count == 2 && called_result == CARD_RESULT_READY);
    memset(&stat, 0, sizeof stat);
    CHECK(CARDGetStatus(0, info.fileNo, &stat) == CARD_RESULT_READY);
    CHECK(stat.bannerFormat == CARD_STAT_BANNER_RGB5A3 && stat.iconAddr == 0 && stat.commentAddr == 0x1C00);
    CHECK(stat.offsetBanner == 0 && stat.offsetIcon[0] == 96 * 32 * 2);
    CHECK(stat.offsetData == 96 * 32 * 2 + 32 * 32 * 2);
    stat.commentAddr = 8192 - 32;
    CHECK(CARDSetStatus(0, info.fileNo, &stat) == CARD_RESULT_FATAL_ERROR);

    /* The raw entry, as m_card.c widens its permissions, and the attributes. */
    memset(&dir, 0, sizeof dir);
    CHECK(__CARDGetStatusEx(0, info.fileNo, &dir) == CARD_RESULT_READY);
    CHECK(memcmp(dir.gameName, "GAFE", 4) == 0 && dir.length == 2 && dir.commentAddr == 0x1C00);
    CHECK(dir._padding0 == 0xFF && dir._padding1[0] == 0xFF && dir._padding1[1] == 0xFF);
    CHECK(dir.permission == CARD_ATTR_PUBLIC);
    dir.permission |= CARD_ATTR_NO_MOVE;
    CHECK(__CARDSetStatusEx(0, info.fileNo, &dir) == CARD_RESULT_READY);
    CHECK(CARDGetAttributes(0, info.fileNo, &attr) == CARD_RESULT_READY);
    CHECK(attr == (CARD_ATTR_PUBLIC | CARD_ATTR_NO_MOVE));
    CHECK(CARDSetAttributes(0, info.fileNo, CARD_ATTR_GLOBAL) == CARD_RESULT_NOPERM);
    CHECK(CARDSetAttributes(0, info.fileNo, CARD_ATTR_NO_COPY) == CARD_RESULT_READY);
    CHECK(__CARDGetStatusEx(0, info.fileNo, &dir) == CARD_RESULT_READY);
    CHECK(dir.permission == CARD_ATTR_NO_COPY);

    CHECK(CARDClose(&info) == CARD_RESULT_READY);
    CHECK(info.chan == -1);

    /* Pulled out while mounted: the detach callback, then no card. Put back, the file is there. */
    ac_card_eject(0);
    CHECK(sl_os_irq_dispatch() == SL_OK);
    CHECK(called_count == 3 && called_chan == 0 && called_result == CARD_RESULT_NOCARD);
    CHECK(CARDGetResultCode(0) == CARD_RESULT_NOCARD);
    CHECK(ac_card_insert(0, "card-a"));
    CHECK(!ac_card_insert(0, "card-a"));
    CHECK(CARDMountAsync(0, work, card_done, card_done) == CARD_RESULT_READY);
    CHECK(sl_os_irq_dispatch() == SL_OK);
    CHECK(called_count == 4 && called_result == CARD_RESULT_READY);
    CHECK(CARDFastOpen(0, 0, &info) == CARD_RESULT_READY);
    CHECK(CARDRead(&info, back, sizeof back, 0) == CARD_RESULT_READY);
    CHECK(memcmp(back, data, sizeof data) == 0);

    /* Rename and delete, each way. */
    CHECK(CARDRename(0, save_name, "renamed") == CARD_RESULT_READY);
    CHECK(CARDOpen(0, save_name, &info) == CARD_RESULT_NOFILE);
    CHECK(CARDOpen(0, "renamed", &info) == CARD_RESULT_READY);
    CHECK(CARDDeleteAsync(0, "renamed", card_done) == CARD_RESULT_READY);
    CHECK(sl_os_irq_dispatch() == SL_OK);
    CHECK(called_count == 5 && called_result == CARD_RESULT_READY);
    CHECK(CARDOpen(0, "renamed", &info) == CARD_RESULT_NOFILE);
    CHECK(CARDCreate(0, save_name, 8192, &info) == CARD_RESULT_READY);
    CHECK(CARDFastDelete(0, info.fileNo) == CARD_RESULT_READY);
    CHECK(CARDDelete(0, save_name) == CARD_RESULT_NOFILE);

    /* Format, polled as the game does. */
    CHECK(CARDCreate(0, save_name, 8192, &info) == CARD_RESULT_READY);
    CHECK(CARDFormatAsync(0, NULL) == CARD_RESULT_READY);
    CHECK(CARDGetResultCode(0) == CARD_RESULT_BUSY);
    CHECK(sl_os_irq_dispatch() == SL_OK);
    CHECK(CARDGetResultCode(0) == CARD_RESULT_READY);
    CHECK(CARDOpen(0, save_name, &info) == CARD_RESULT_NOFILE);

    views_through_adapter(store, work);
    CHECK(CARDUnmount(0) == CARD_RESULT_READY);
    CHECK(CARDCheck(0) == CARD_RESULT_NOCARD);

    ac_card_eject(0);
    sl_memcard_shutdown();
    sl_save_store_close(store);
    sl_os_thread_exit(NULL);
    sl_os_stop();
    if (failures == 0)
        printf("ok\n");
    return failures != 0;
}

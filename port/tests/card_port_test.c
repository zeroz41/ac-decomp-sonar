/* Checks the port's card functions (adapter/dolphin_card.c) on a memory store: a card inserted before CARDInit, the
   calls that return their result and those that complete in interrupt context, the status the game reads and
   writes, and a file kept across an eject. */
/* card.h takes the OS types from os.h. */
#include "dolphin/os.h"
#include "dolphin/card.h"
#include "card/__card.h"
#include "dolphin/dvd.h"

#include "ac_port.h"

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

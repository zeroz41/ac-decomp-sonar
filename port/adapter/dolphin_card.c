/* The Dolphin SDK's card functions the game calls, on sonarlib's virtual memory card (sonar/card/memcard.h): two
   slots whose cards keep each file in the save store. The kit's results are the SDK's codes. A call ending in Async
   returns once it has started and calls back in interrupt context; until then CARDGetResultCode gives BUSY, as on
   the console, even with no callback. The other calls return their result. The SDK callbacks ride in the kit's user
   pointer. */
/* card.h takes the OS types from os.h. */
#include "dolphin/os.h"
#include "dolphin/card.h"
#include "card/__card.h"
#include "dolphin/dvd.h"

#include "ac_port.h"

#include <sonar/card/memcard.h>

#include <stdint.h>
#include <string.h>

static sl_save_store store;
static BOOL started;
#define AC_CARD_NAME_MAX 59
/* Cards inserted before CARDInit, for it to put in. */
static char waiting[CARD_NUM_CHANS][AC_CARD_NAME_MAX + 1];

static void* as_user(CARDCallback callback) { return (void*)(uintptr_t)callback; }

static void forward(void* user, uint32_t slot, int32_t result) {
    const CARDCallback callback = (CARDCallback)(uintptr_t)user;
    if (callback != NULL)
        callback((s32)slot, result);
}

/* The SDK's internal calls take their callback with long arguments. */
typedef void (*LongCallback)(long chan, long result);

static void forward_long(void* user, uint32_t slot, int32_t result) {
    const LongCallback callback = (LongCallback)(uintptr_t)user;
    if (callback != NULL)
        callback((long)slot, result);
}

static BOOL insert(s32 chan, const char* name) {
    const sl_memcard_card_desc card = {name, 0};
    return sl_memcard_insert((uint32_t)chan, &card) == SL_OK;
}

void ac_card_set_store(sl_save_store save_store) { store = save_store; }

BOOL ac_card_insert(s32 chan, const char* name) {
    if (chan < 0 || chan >= CARD_NUM_CHANS || name == NULL)
        return FALSE;
    if (started)
        return insert(chan, name);
    if (waiting[chan][0] != '\0' || strlen(name) > AC_CARD_NAME_MAX)
        return FALSE;
    strcpy(waiting[chan], name);
    return TRUE;
}

void ac_card_eject(s32 chan) {
    if (chan < 0 || chan >= CARD_NUM_CHANS)
        return;
    waiting[chan][0] = '\0';
    if (started)
        sl_memcard_eject((uint32_t)chan);
}

/* ---- The card ---- */

/* Starts the virtual card library for the disc's game, in the console's encoding, and puts in the cards waiting. */
void CARDInit(void) {
    const DVDDiskID* id = DVDGetCurrentDiskID();
    sl_memcard_desc desc;
    if (started)
        return;
    memset(&desc, 0, sizeof desc);
    desc.store = store;
    memcpy(desc.game_code, id->gameName, sizeof desc.game_code);
    memcpy(desc.maker_code, id->company, sizeof desc.maker_code);
    desc.encoding = OSGetFontEncode();
    if (sl_memcard_init(&desc) != SL_OK) {
        OSReport("CARDInit: the memory card kit did not start\n");
        return;
    }
    started = TRUE;
    for (s32 chan = 0; chan < CARD_NUM_CHANS; ++chan) {
        if (waiting[chan][0] != '\0' && !insert(chan, waiting[chan]))
            OSReport("CARDInit: card \"%s\" did not go in slot %c\n", waiting[chan], 'A' + chan);
        waiting[chan][0] = '\0';
    }
}

s32 CARDGetResultCode(s32 chan) { return sl_memcard_get_result((uint32_t)chan); }

s32 CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize) {
    uint32_t mbits = 0, sector = 0;
    const s32 result = sl_memcard_probe((uint32_t)chan, &mbits, &sector);
    if (result == CARD_RESULT_READY) {
        if (memSize != NULL)
            *memSize = (s32)mbits;
        if (sectorSize != NULL)
            *sectorSize = (s32)sector;
    }
    return result;
}

BOOL CARDProbe(s32 chan) { return sl_memcard_probe((uint32_t)chan, NULL, NULL) == CARD_RESULT_READY; }

/* The work area is the SDK's own; the kit needs none. */
s32 CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback, CARDCallback attachCallback) {
    (void)workArea;
    return sl_memcard_mount((uint32_t)chan, forward, as_user(detachCallback), forward, as_user(attachCallback));
}

s32 CARDMount(s32 chan, void* workArea, CARDCallback detachCallback) {
    (void)workArea;
    return sl_memcard_mount((uint32_t)chan, forward, as_user(detachCallback), NULL, NULL);
}

s32 CARDUnmount(s32 chan) { return sl_memcard_unmount((uint32_t)chan); }

s32 CARDCheckAsync(s32 chan, CARDCallback callback) {
    return sl_memcard_check((uint32_t)chan, forward, as_user(callback));
}

s32 CARDCheck(s32 chan) { return sl_memcard_check((uint32_t)chan, NULL, NULL); }

s32 CARDFormatAsync(s32 chan, CARDCallback callback) {
    return sl_memcard_format((uint32_t)chan, forward, as_user(callback));
}

s32 CARDFormat(s32 chan) { return sl_memcard_format((uint32_t)chan, NULL, NULL); }

s32 CARDFreeBlocks(s32 chan, s32* byteNotUsed, s32* filesNotUsed) {
    uint32_t bytes = 0, files = 0;
    const s32 result = sl_memcard_free_space((uint32_t)chan, &bytes, &files);
    if (result == CARD_RESULT_READY) {
        if (byteNotUsed != NULL)
            *byteNotUsed = (s32)bytes;
        if (filesNotUsed != NULL)
            *filesNotUsed = (s32)files;
    }
    return result;
}

/* ---- Files ---- */

/* An open file as the SDK keeps it: its slot, at offset 0 and the first block its entry names. */
static s32 opened(s32 result, const sl_memcard_file* file, CARDFileInfo* fileInfo) {
    sl_card_dir_entry entry;
    if (result != CARD_RESULT_READY || fileInfo == NULL)
        return result;
    fileInfo->chan = (s32)file->slot;
    fileInfo->fileNo = (s32)file->file_no;
    fileInfo->offset = 0;
    fileInfo->iBlock = sl_memcard_get_entry(file->slot, file->file_no, &entry) == CARD_RESULT_READY
                           ? entry.first_block
                           : 0;
    return result;
}

static sl_memcard_file kit_file(const CARDFileInfo* fileInfo) {
    const sl_memcard_file file = {(uint32_t)fileInfo->chan, (uint32_t)fileInfo->fileNo, 0};
    return file;
}

s32 CARDOpen(s32 chan, const char* fileName, CARDFileInfo* fileInfo) {
    sl_memcard_file file;
    return opened(sl_memcard_open((uint32_t)chan, fileName, &file), &file, fileInfo);
}

s32 CARDFastOpen(s32 chan, s32 fileNo, CARDFileInfo* fileInfo) {
    sl_memcard_file file;
    return opened(sl_memcard_open_slot((uint32_t)chan, (uint32_t)fileNo, &file), &file, fileInfo);
}

/* Marks the file closed, as the SDK does, while its card is in. */
s32 CARDClose(CARDFileInfo* fileInfo) {
    const s32 result = sl_memcard_probe((uint32_t)fileInfo->chan, NULL, NULL);
    if (result != CARD_RESULT_READY)
        return result;
    fileInfo->chan = -1;
    return CARD_RESULT_READY;
}

s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo, CARDCallback callback) {
    sl_memcard_file file;
    const s32 result = sl_memcard_create((uint32_t)chan, fileName, size, &file, forward, as_user(callback));
    return opened(result, &file, fileInfo);
}

s32 CARDCreate(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo) {
    sl_memcard_file file;
    return opened(sl_memcard_create((uint32_t)chan, fileName, size, &file, NULL, NULL), &file, fileInfo);
}

s32 CARDDeleteAsync(s32 chan, const char* fileName, CARDCallback callback) {
    return sl_memcard_delete((uint32_t)chan, fileName, forward, as_user(callback));
}

s32 CARDDelete(s32 chan, const char* fileName) { return sl_memcard_delete((uint32_t)chan, fileName, NULL, NULL); }

s32 CARDFastDeleteAsync(s32 chan, s32 fileNo, CARDCallback callback) {
    return sl_memcard_delete_slot((uint32_t)chan, (uint32_t)fileNo, forward, as_user(callback));
}

s32 CARDFastDelete(s32 chan, s32 fileNo) { return sl_memcard_delete_slot((uint32_t)chan, (uint32_t)fileNo, NULL, NULL); }

s32 CARDRenameAsync(s32 chan, const char* oldName, const char* newName, CARDCallback callback) {
    return sl_memcard_rename((uint32_t)chan, oldName, newName, forward, as_user(callback));
}

s32 CARDRename(s32 chan, const char* oldName, const char* newName) {
    return sl_memcard_rename((uint32_t)chan, oldName, newName, NULL, NULL);
}

s32 CARDReadAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset, CARDCallback callback) {
    const sl_memcard_file file = kit_file(fileInfo);
    return sl_memcard_read(&file, buf, (uint32_t)length, (uint32_t)offset, forward, as_user(callback));
}

s32 CARDRead(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset) {
    const sl_memcard_file file = kit_file(fileInfo);
    return sl_memcard_read(&file, buf, (uint32_t)length, (uint32_t)offset, NULL, NULL);
}

s32 CARDWriteAsync(CARDFileInfo* fileInfo, const void* buf, s32 length, s32 offset, CARDCallback callback) {
    const sl_memcard_file file = kit_file(fileInfo);
    return sl_memcard_write(&file, buf, (uint32_t)length, (uint32_t)offset, forward, as_user(callback));
}

s32 CARDWrite(CARDFileInfo* fileInfo, const void* buf, s32 length, s32 offset) {
    const sl_memcard_file file = kit_file(fileInfo);
    return sl_memcard_write(&file, buf, (uint32_t)length, (uint32_t)offset, NULL, NULL);
}

/* ---- Directory entries ---- */

s32 CARDGetStatus(s32 chan, s32 fileNo, CARDStat* stat) {
    sl_card_dir_entry entry;
    sl_card_graphics graphics;
    const s32 result = sl_memcard_get_entry((uint32_t)chan, (uint32_t)fileNo, &entry);
    if (result != CARD_RESULT_READY)
        return result;
    memcpy(stat->fileName, entry.file_name, CARD_FILENAME_MAX);
    stat->length = (u32)entry.block_count * SL_CARD_BLOCK_SIZE;
    stat->time = entry.time;
    memcpy(stat->gameName, entry.game_code, sizeof stat->gameName);
    memcpy(stat->company, entry.maker_code, sizeof stat->company);
    stat->bannerFormat = entry.banner_format;
    stat->iconAddr = entry.icon_address;
    stat->iconFormat = entry.icon_formats;
    stat->iconSpeed = entry.icon_speeds;
    stat->commentAddr = entry.comment_address;
    sl_card_graphics_layout(&entry, &graphics);
    stat->offsetBanner = graphics.banner;
    stat->offsetBannerTlut = graphics.banner_palette;
    for (int i = 0; i < CARD_ICON_MAX; ++i)
        stat->offsetIcon[i] = graphics.icons[i];
    stat->offsetIconTlut = graphics.icon_palette;
    stat->offsetData = graphics.data;
    return result;
}

static sl_card_dir_entry graphics_of(const CARDStat* stat) {
    sl_card_dir_entry graphics;
    memset(&graphics, 0, sizeof graphics);
    graphics.banner_format = stat->bannerFormat;
    graphics.icon_address = stat->iconAddr;
    graphics.icon_formats = stat->iconFormat;
    graphics.icon_speeds = stat->iconSpeed;
    graphics.comment_address = stat->commentAddr;
    return graphics;
}

s32 CARDSetStatusAsync(s32 chan, s32 fileNo, CARDStat* stat, CARDCallback callback) {
    const sl_card_dir_entry graphics = graphics_of(stat);
    return sl_memcard_set_graphics((uint32_t)chan, (uint32_t)fileNo, &graphics, forward, as_user(callback));
}

s32 CARDSetStatus(s32 chan, s32 fileNo, CARDStat* stat) {
    const sl_card_dir_entry graphics = graphics_of(stat);
    return sl_memcard_set_graphics((uint32_t)chan, (uint32_t)fileNo, &graphics, NULL, NULL);
}

s32 CARDGetAttributes(s32 chan, s32 fileNo, u8* attr) {
    sl_card_dir_entry entry;
    const s32 result = sl_memcard_get_entry((uint32_t)chan, (uint32_t)fileNo, &entry);
    if (result == CARD_RESULT_READY)
        *attr = entry.permissions;
    return result;
}

s32 CARDSetAttributesAsync(s32 chan, s32 fileNo, u8 attr, CARDCallback callback) {
    return sl_memcard_set_permissions((uint32_t)chan, (uint32_t)fileNo, attr, forward, as_user(callback));
}

s32 CARDSetAttributes(s32 chan, s32 fileNo, u8 attr) {
    return sl_memcard_set_permissions((uint32_t)chan, (uint32_t)fileNo, attr, NULL, NULL);
}

/* The raw entry, field by field: the game's CARDDir is in host order. */
static void dir_from_entry(const sl_card_dir_entry* entry, CARDDir* dir) {
    memcpy(dir->gameName, entry->game_code, sizeof dir->gameName);
    memcpy(dir->company, entry->maker_code, sizeof dir->company);
    dir->_padding0 = entry->reserved0;
    dir->bannerFormat = entry->banner_format;
    memcpy(dir->fileName, entry->file_name, CARD_FILENAME_MAX);
    dir->time = entry->time;
    dir->iconAddr = entry->icon_address;
    dir->iconFormat = entry->icon_formats;
    dir->iconSpeed = entry->icon_speeds;
    dir->permission = entry->permissions;
    dir->copyTimes = entry->copy_count;
    dir->startBlock = entry->first_block;
    dir->length = entry->block_count;
    dir->_padding1[0] = (u8)(entry->reserved1 >> 8);
    dir->_padding1[1] = (u8)entry->reserved1;
    dir->commentAddr = entry->comment_address;
}

static void entry_from_dir(const CARDDir* dir, sl_card_dir_entry* entry) {
    memcpy(entry->game_code, dir->gameName, sizeof entry->game_code);
    memcpy(entry->maker_code, dir->company, sizeof entry->maker_code);
    entry->reserved0 = dir->_padding0;
    entry->banner_format = dir->bannerFormat;
    memcpy(entry->file_name, dir->fileName, CARD_FILENAME_MAX);
    entry->time = dir->time;
    entry->icon_address = dir->iconAddr;
    entry->icon_formats = dir->iconFormat;
    entry->icon_speeds = dir->iconSpeed;
    entry->permissions = dir->permission;
    entry->copy_count = dir->copyTimes;
    entry->first_block = dir->startBlock;
    entry->block_count = dir->length;
    entry->reserved1 = (uint16_t)(dir->_padding1[0] << 8 | dir->_padding1[1]);
    entry->comment_address = dir->commentAddr;
}

long __CARDGetStatusEx(long chan, long fileNo, struct CARDDir* dirent) {
    sl_card_dir_entry entry;
    const s32 result = sl_memcard_get_entry((uint32_t)chan, (uint32_t)fileNo, &entry);
    if (result == CARD_RESULT_READY)
        dir_from_entry(&entry, dirent);
    return result;
}

long __CARDSetStatusExAsync(long chan, long fileNo, struct CARDDir* dirent, void (*callback)(long, long)) {
    sl_card_dir_entry entry;
    entry_from_dir(dirent, &entry);
    return sl_memcard_set_entry((uint32_t)chan, (uint32_t)fileNo, &entry, forward_long, (void*)(uintptr_t)callback);
}

long __CARDSetStatusEx(long chan, long fileNo, struct CARDDir* dirent) {
    sl_card_dir_entry entry;
    entry_from_dir(dirent, &entry);
    return sl_memcard_set_entry((uint32_t)chan, (uint32_t)fileNo, &entry, NULL, NULL);
}

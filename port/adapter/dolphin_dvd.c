/* The Dolphin SDK's DVD functions the game calls, on the package the program mounts as game: (port/README.md, The
   package). The disc's file system table becomes a table of every file and directory in the package, sorted by
   path in byte order, and an entry number is an index in it. Reads are synchronous: an asynchronous read calls its
   callback before it returns, as a read that finished at once would. The disc has no directories to list, no
   audio stream and no drive to fail. */
#include "dolphin/os.h"
#include "dolphin/dvd.h"

#include "ac_port.h"

#include <sonar/base/log.h>
#include <sonar/fs.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* The disc revision this build is for, such as "GAFE01_00": game code, maker code, revision. */
#ifndef AC_PORT_DISC_VERSION
#error "AC_PORT_DISC_VERSION names the disc revision, as AC_VERSION does"
#endif

typedef struct Entry {
    char* path; /* relative to the disc's root, without a leading '/' */
    u32 size;
    BOOL is_directory;
} Entry;

static Entry* entries;
static s32 entry_count;
static s32 entry_capacity;
static u64 read_count;
static u64 read_bytes;
static DVDDiskID disk_id;

/* The table lives as long as the program; names.h gives malloc and free to libc64, so it grows with realloc. */
static BOOL append(const char* directory, const char* name, int is_directory) {
    const size_t directory_length = strlen(directory);
    const size_t name_length = strlen(name);
    char* path;
    if (entry_count == entry_capacity) {
        const s32 capacity = entry_capacity == 0 ? 256 : entry_capacity * 2;
        Entry* const grown = (Entry*)realloc(entries, (size_t)capacity * sizeof *entries);
        if (grown == NULL)
            return FALSE;
        entries = grown;
        entry_capacity = capacity;
    }
    path = (char*)realloc(NULL, directory_length + (directory_length != 0) + name_length + 1);
    if (path == NULL)
        return FALSE;
    memcpy(path, directory, directory_length);
    if (directory_length != 0)
        path[directory_length] = '/';
    memcpy(path + directory_length + (directory_length != 0), name, name_length + 1);
    entries[entry_count].path = path;
    entries[entry_count].size = 0;
    entries[entry_count].is_directory = is_directory != 0;
    entry_count++;
    return TRUE;
}

typedef struct Listing {
    const char* directory;
    BOOL failed;
} Listing;

/* sl_fs_list's callback, which may not call into the file system: it only collects the names. */
static int collect(void* user, const char* name, int is_directory) {
    Listing* const listing = (Listing*)user;
    if (!append(listing->directory, name, is_directory)) {
        listing->failed = TRUE;
        return 1;
    }
    return 0;
}

static BOOL virtual_path(char* buffer, const char* path) {
    static const char mount[] = "game:";
    const size_t length = strlen(path);
    if (sizeof mount + length > SL_FS_MAX_PATH)
        return FALSE;
    memcpy(buffer, mount, sizeof mount - 1);
    memcpy(buffer + sizeof mount - 1, path, length + 1);
    return TRUE;
}

static int compare_entries(const void* a, const void* b) {
    return strcmp(((const Entry*)a)->path, ((const Entry*)b)->path);
}

static void fail(const char* what, sl_result result) {
    SL_ERROR("dvd", "DVDInit: %s: %s", what, sl_result_string(result));
    abort();
}

static int fold(int c) {
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

static BOOL equal_folded(const char* a, const char* b) {
    while (*a != '\0' && fold((unsigned char)*a) == fold((unsigned char)*b)) {
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

/* The disc's identity, from AC_VERSION's form: four characters of game code, two of maker code, '_' and the
   revision in decimal. */
static void init_disk_id(void) {
    static const char version[] = AC_PORT_DISC_VERSION;
    _Static_assert(sizeof version == 10, "AC_VERSION is GGGGMM_RR");
    memset(&disk_id, 0, sizeof disk_id);
    memcpy(disk_id.gameName, version, 4);
    memcpy(disk_id.company, version + 4, 2);
    disk_id.diskNumber = 0;
    disk_id.gameVersion = (u8)((version[7] - '0') * 10 + (version[8] - '0'));
}

void DVDInit(void) {
    static BOOL initialized;
    char path[SL_FS_MAX_PATH];
    s32 i;
    if (initialized)
        return;
    initialized = TRUE;
    init_disk_id();
    /* The root's entries, then each directory's after them in turn: the table is its own work list, and no listing
       runs inside another's callback. */
    {
        Listing listing = { "", FALSE };
        const sl_result result = sl_fs_list("game:", collect, &listing);
        if (result != SL_OK)
            fail("sl_fs_list(game:)", result);
        if (listing.failed)
            fail("out of memory", SL_ERROR_OUT_OF_MEMORY);
    }
    for (i = 0; i < entry_count; i++) {
        Listing listing;
        sl_result result;
        if (!entries[i].is_directory)
            continue;
        if (!virtual_path(path, entries[i].path))
            fail("a path is too long", SL_ERROR_INVALID_ARGUMENT);
        listing.directory = entries[i].path;
        listing.failed = FALSE;
        result = sl_fs_list(path, collect, &listing);
        if (result != SL_OK)
            fail("sl_fs_list", result);
        if (listing.failed)
            fail("out of memory", SL_ERROR_OUT_OF_MEMORY);
    }
    for (i = 0; i < entry_count; i++) {
        sl_file_info info;
        sl_result result;
        if (entries[i].is_directory)
            continue;
        if (!virtual_path(path, entries[i].path))
            fail("a path is too long", SL_ERROR_INVALID_ARGUMENT);
        result = sl_fs_stat(path, &info);
        if (result != SL_OK)
            fail("sl_fs_stat", result);
        if (info.size > UINT32_MAX)
            fail("a file is larger than the disc can hold", SL_ERROR_INVALID_ARGUMENT);
        entries[i].size = (u32)info.size;
    }
    if (entry_count > 1)
        qsort(entries, (size_t)entry_count, sizeof *entries, compare_entries);
}

s32 DVDConvertPathToEntrynum(char* path) {
    s32 low = 0;
    s32 high = entry_count;
    s32 i;
    if (path == NULL)
        return -1;
    if (path[0] == '/')
        path++;
    while (low < high) {
        const s32 middle = low + (high - low) / 2;
        const int order = strcmp(entries[middle].path, path);
        if (order == 0)
            return middle;
        if (order < 0)
            low = middle + 1;
        else
            high = middle;
    }
    /* As SL_MOUNT_CASE_INSENSITIVE looks names up: without an exact match, the first in byte order that matches
       under ASCII case folding. */
    for (i = 0; i < entry_count; i++) {
        if (equal_folded(entries[i].path, path))
            return i;
    }
    return -1;
}

BOOL DVDFastOpen(s32 entryNum, DVDFileInfo* fileInfo) {
    if (entryNum < 0 || entryNum >= entry_count || entries[entryNum].is_directory)
        return FALSE;
    fileInfo->startAddr = (u32)entryNum;
    fileInfo->length = entries[entryNum].size;
    fileInfo->cb.state = 0;
    return TRUE;
}

BOOL DVDOpen(char* fileName, DVDFileInfo* fileInfo) {
    return DVDFastOpen(DVDConvertPathToEntrynum(fileName), fileInfo);
}

BOOL DVDClose(DVDFileInfo* fileInfo) {
    (void)fileInfo;
    return TRUE;
}

s32 DVDReadPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, s32 prio) {
    const s32 entry = (s32)fileInfo->startAddr;
    char path[SL_FS_MAX_PATH];
    sl_file file;
    size_t bytes = 0;
    sl_result result;
    (void)prio;
    if (entry < 0 || entry >= entry_count || entries[entry].is_directory || length < 0 || offset < 0)
        return -1;
    if (!virtual_path(path, entries[entry].path))
        return -1;
    result = sl_fs_open(path, 0, &file);
    if (result != SL_OK) {
        SL_ERROR("dvd", "DVDReadPrio: %s: %s", entries[entry].path, sl_result_string(result));
        return -1;
    }
    result = sl_fs_read(file, (uint64_t)offset, addr, (size_t)length, &bytes);
    sl_fs_close(file);
    if (result != SL_OK) {
        SL_ERROR("dvd", "DVDReadPrio: %s: %s", entries[entry].path, sl_result_string(result));
        return -1;
    }
    read_count++;
    read_bytes += bytes;
    return (s32)bytes;
}

BOOL DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, DVDCallback callback, s32 prio) {
    const s32 result = DVDReadPrio(fileInfo, addr, length, offset, prio);
    fileInfo->callback = callback;
    if (callback != NULL)
        callback(result, fileInfo);
    return TRUE;
}

s32 DVDGetCommandBlockStatus(const DVDCommandBlock* block) {
    (void)block;
    return 0;
}

DVDDiskID* DVDGetCurrentDiskID(void) {
    return &disk_id;
}

BOOL DVDCheckDisk(void) {
    return 1;
}

s32 DVDGetDriveStatus(void) {
    return 0;
}

BOOL DVDOpenDir(char* dirName, DVDDir* dir) {
    (void)dirName;
    (void)dir;
    return FALSE;
}

BOOL DVDReadDir(DVDDir* dir, DVDDirEntry* dirEntry) {
    (void)dir;
    (void)dirEntry;
    return FALSE;
}

BOOL DVDCloseDir(DVDDir* dir) {
    (void)dir;
    return TRUE;
}

BOOL DVDPrepareStreamAsync(DVDFileInfo* fileInfo, u32 length, u32 offset, DVDCallback callback) {
    (void)fileInfo;
    (void)length;
    (void)offset;
    (void)callback;
    return FALSE;
}

s32 DVDCancelStream(DVDCommandBlock* block) {
    (void)block;
    return -1;
}

u64 ac_dvd_reads(void) {
    return read_count;
}

u64 ac_dvd_read_bytes(void) {
    return read_bytes;
}

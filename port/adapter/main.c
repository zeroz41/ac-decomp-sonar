/* ac_port --package <file> [--ticks <n>] [--validate]: Animal Crossing on sonarlib, headless and deterministic, from
   boot to play's first frame (port/README.md). The program starts the library and the render module on the null
   sink, makes the emu64 the glue records each task with, mounts the package as game:, starts the CPU model and the
   runtime in deterministic mode, and calls boot's main, which never returns: the run ends at play's first frame
   (emu64_glue.c) or at the tick limit. */
#include "ac_port.h"

#include <port/emu64_glue.h>

#include <sonar/emu64/emu64.h>
#include <sonar/fs.h>
#include <sonar/mem.h>
#include <sonar/os.h>
#include <sonar/render.h>
#include <sonar/runtime.h>
#include <sonar/sonar.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ac_boot_main(int argc, const char** argv); /* src/static/boot.c's main */

static const char usage[] = "usage: ac_port --package <file> [--ticks <n>] [--validate]\n";

static uint64_t tick_limit = 3600;

static int usage_error(void) {
    fputs(usage, stderr);
    return 2;
}

static int failed(const char* call, sl_result result) {
    fprintf(stderr, "ac_port: %s: %s\n", call, sl_result_string(result));
    return 1;
}

/* Ends the run once the runtime has begun the limit's ticks without a frame of play. */
static void stop_at_limit(void* user, uint64_t retrace) {
    (void)user;
    (void)retrace;
    if (sl_tick_count() >= tick_limit) {
        ac_port_report("limit");
        _Exit(1);
    }
}

static int parse_ticks(const char* text, uint64_t* ticks) {
    char* end = NULL;
    unsigned long long value;
    if (text[0] < '0' || text[0] > '9')
        return 0;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || *end != '\0' || value == 0)
        return 0;
    *ticks = (uint64_t)value;
    return 1;
}

int main(int argc, char** argv) {
    const char* package = NULL;
    uint32_t emu64_flags = 0;
    sl_result result;
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--package") == 0 && i + 1 < argc && package == NULL) {
            package = argv[++i];
        } else if (strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) {
            if (!parse_ticks(argv[++i], &tick_limit))
                return usage_error();
        } else if (strcmp(argv[i], "--validate") == 0) {
            emu64_flags |= SL_EMU64_VALIDATE; /* every use of a program hashes its ranges again */
        } else {
            return usage_error();
        }
    }
    if (package == NULL)
        return usage_error();

    {
        sl_init_desc desc;
        memset(&desc, 0, sizeof desc);
        desc.api_version = SL_API_VERSION;
        desc.app_name = "ac_port";
        desc.flags = SL_INIT_NO_LOG_FILE;
        result = sl_init(&desc);
        if (result != SL_OK)
            return failed("sl_init", result);
    }
    {
        /* The null sink: each frame is checked and hashed at submit, and nothing is presented. */
        sl_render_desc desc;
        memset(&desc, 0, sizeof desc);
        desc.sink = SL_RENDER_SINK_NULL;
        desc.flags = SL_RENDER_HASH_FRAMES;
        desc.max_views = 256;
        result = sl_render_init(&desc);
        if (result != SL_OK)
            return failed("sl_render_init", result);
    }
    {
        /* emu64's texture cache takes the executable's read-only and initialized data as static (emu64.c's
           _f_rodata to _e_data), the game's own and foresta's, linked in. */
        sl_mem_range ranges[16];
        uint32_t count, n;
        sl_emu64* emu = sl_emu64_create(emu64_flags);
        if (emu == NULL)
            return failed("sl_emu64_create", SL_ERROR_OUT_OF_MEMORY);
        count = sl_mem_image_data(ranges, 16);
        for (n = 0; n < count && n < 16; n++)
            sl_emu64_static_range(emu, ranges[n].begin, ranges[n].end);
        ac_emu64_start(emu);
    }
    {
        /* No user directories: the run writes nothing, and the cards stay in memory. */
        sl_fs_desc desc;
        memset(&desc, 0, sizeof desc);
        desc.app_name = "ac_port";
        desc.flags = SL_FS_INIT_NO_USER_MOUNT;
        result = sl_fs_init(&desc);
        if (result != SL_OK)
            return failed("sl_fs_init", result);
    }
    {
        sl_mount_desc desc;
        sl_mount mount;
        memset(&desc, 0, sizeof desc);
        desc.name = "game";
        desc.host_path = package;
        desc.flags = SL_MOUNT_PACKAGE | SL_MOUNT_CASE_INSENSITIVE;
        desc.importer = "ac";      /* ac::importer_id */
        desc.importer_version = 1; /* ac::importer_version */
        result = sl_fs_mount(&desc, &mount);
        if (result != SL_OK)
            return failed("sl_fs_mount", result);
    }
    {
        /* 2001-12-14 12:00:00.000, a Friday, the year's 348th day. */
        sl_os_calendar start;
        sl_os_desc desc;
        memset(&start, 0, sizeof start);
        start.year = 2001;
        start.month = 12;
        start.day = 14;
        start.hour = 12;
        start.weekday = 5;
        start.year_day = 347;
        memset(&desc, 0, sizeof desc);
        desc.deterministic = 1;
        desc.start_calendar = &start;
        result = sl_os_start(&desc);
        if (result != SL_OK)
            return failed("sl_os_start", result);
    }
    {
        /* The main thread joins the CPU model before the runtime starts, so it holds the token as boot's main
           thread; OSInit binds it as the SDK's. */
        sl_os_thread_desc desc;
        sl_os_thread self;
        memset(&desc, 0, sizeof desc);
        desc.name = "main";
        result = sl_os_thread_attach(&desc, &self);
        if (result != SL_OK)
            return failed("sl_os_thread_attach", result);
    }
    {
        /* Fixed ticks at the video rate, NTSC: the rates' zeros. */
        sl_runtime_desc desc;
        memset(&desc, 0, sizeof desc);
        desc.tick_mode = SL_TICK_FIXED;
        desc.deterministic = 1;
        result = sl_runtime_start(&desc);
        if (result != SL_OK)
            return failed("sl_runtime_start", result);
    }
    result = sl_retrace_add_callback(stop_at_limit, NULL);
    if (result != SL_OK)
        return failed("sl_retrace_add_callback", result);

    {
        static const char* const boot_argv[] = { "ac_port", NULL };
        ac_boot_main(1, (const char**)boot_argv);
    }
    return 1;
}

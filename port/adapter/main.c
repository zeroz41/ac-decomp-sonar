/* ac_port --package <file> [--ticks <n>] [--validate] [--diff] [--reference]
           [--window | --goldens <scenes> <dir> | --measure] [--qrand-seed <hex>]: Animal Crossing on sonarlib
   (port/NOTES.md, The program). The process's main thread starts the library and the render module, makes the emu64
   the glue records each task with, mounts the package as game:, starts the CPU model, and creates the game thread
   "main", which starts the runtime and calls boot's main, which never returns.

   Headless, the default: deterministic, on the null sinks, the game's sound mixed and counted but not heard. The main
   thread joins "main", and the run ends at the submit of the frame that holds play's first task (emu64_glue.c, from
   the GX glue's ac_frame_end) or at the tick limit. With --diff, each task is drawn also by the console's emu64 and
   the two are compared (emu64_glue.c).

   --window: in real time, on the GPU sink, in a window, with the game's pads on the keyboard and gamepads and its
   sound on the audio device. The main thread presents until the window is closed, or until --ticks's limit when it
   is given.

   --goldens: headless and deterministic, but on the GPU sink with no window, writing the frames the scenes file names
   into the directory (sonar/goldens.h, port/NOTES.md, Goldens). The GX glue's frame begin hands each frame's keys to
   the goldens (emu64_glue.c), play's first frame ends no run, and the main thread serves the captures until every
   scene is written, or until the tick limit stops them. --qrand-seed gives init_rnd's seed (sys_math.c).

   --measure: headless and deterministic, on the GPU sink with no window, timing each frame from play's first to the
   tick limit (sonar/measure.h, port/NOTES.md, Measured runs). The GX glue's frame end hands each frame to the
   measured run, which waits while the main thread presents it; at the limit the game thread prints the measure line
   and the report, named measured, and the run ends, exit 0.

   --reference: the kit is not made, and the console's emu64 draws every task through the GX front end into the
   game's own frame (emu64_glue.c), headless, with --goldens or with --measure. */
#include "ac_port.h"

#include <port/emu64_glue.h>
#include <port/goldens.h>

#include <sonar/audio.h>
#include <sonar/emu64/emu64.h>
#include <sonar/fs.h>
#include <sonar/goldens.h>
#include <sonar/input.h>
#include <sonar/measure.h>
#include <sonar/mem.h>
#include <sonar/os.h>
#include <sonar/render.h>
#include <sonar/runtime.h>
#include <sonar/sonar.h>
#include <sonar/window.h>

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ac_boot_main(int argc, const char** argv); /* src/static/boot.c's main */

static const char usage[] = "usage: ac_port --package <file> [--ticks <n>] [--validate] [--diff] [--reference]\n"
                            "               [--window | --goldens <scenes> <dir> | --measure] [--qrand-seed <hex>]\n";

static uint64_t tick_limit = 3600;
static int ticks_given;          /* --ticks */
static int window;               /* --window */
static const char* goldens;      /* --goldens's scenes file */
static const char* goldens_dir;  /* and its output directory */
static int measure;              /* --measure */
static uint64_t measured;        /* frames handed to the measured run */
static uint64_t measured_first;  /* the tick of the first */
static uint64_t measured_last;   /* and of the latest */
static int seed_given;           /* --qrand-seed */
static u32 seed;

u32 ac_port_rnd_seed(u32 tick) {
    return seed_given ? seed : tick;
}

static int usage_error(void) {
    fputs(usage, stderr);
    return 2;
}

static int failed(const char* call, sl_result result) {
    fprintf(stderr, "ac_port: %s: %s\n", call, sl_result_string(result));
    return 1;
}

void ac_measure_frame(void) {
    sl_result result;
    if (!measure || !ac_play_reached())
        return;
    if (measured++ == 0)
        measured_first = sl_tick_count();
    measured_last = sl_tick_count();
    result = sl_measure_frame();
    if (result != SL_OK)
        _Exit(failed("sl_measure_frame", result));
}

/* The measure line: the frames, the ticks from the first measured frame's to the last's, each time's median, maximum
   and total in nanoseconds, then what render counted at the presents, which are all the measured ones. */
static void measure_report(void) {
    sl_measure_stats stats;
    sl_render_stats render;
    const sl_measure_time* times[3];
    int i;
    memset(&stats, 0, sizeof stats);
    (void)sl_measure_get_stats(&stats);
    sl_render_get_stats(&render);
    times[0] = &stats.game;
    times[1] = &stats.prepare;
    times[2] = &stats.present;
    printf("ac_port: measure %s frames %" PRIu64 " ticks %" PRIu64, ac_reference_enabled() ? "reference" : "kit",
           stats.frames, measured_last - measured_first);
    for (i = 0; i < 3; i++)
        printf(" %s %" PRIu64 " %" PRIu64 " %" PRIu64, i == 0 ? "game" : i == 1 ? "prepare" : "present",
               times[i]->median_ns, times[i]->max_ns, times[i]->total_ns);
    printf(" draws %" PRIu64 " calls %" PRIu64 " pipelines %" PRIu64 " buffers %" PRIu64 " textures %" PRIu64
           " storage %" PRIu64 " pushes %" PRIu64 "\n",
           render.draws, render.draw_calls, render.pipeline_binds, render.buffer_binds, render.texture_binds,
           render.storage_binds, render.uniform_pushes);
}

/* Ends the run once the runtime has begun the limit's ticks without a frame of play; with --goldens, stops the
   captures, and the main thread ends the run. With --measure, once play was reached, stops the measured run and
   prints its line and the report, here on the game thread at a retrace, while no present runs: the game thread
   waits in sl_measure_frame for each. */
static void stop_at_limit(void* user, uint64_t retrace) {
    (void)user;
    (void)retrace;
    if (sl_tick_count() >= tick_limit) {
        if (goldens != NULL) {
            sl_goldens_stop();
            return;
        }
        if (measure && ac_play_reached()) {
            sl_measure_stop();
            measure_report();
            ac_port_report("measured");
            _Exit(0);
        }
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

static int parse_seed(const char* text, u32* value) {
    char* end = NULL;
    unsigned long parsed;
    if (!isxdigit((unsigned char)text[0]))
        return 0;
    errno = 0;
    parsed = strtoul(text, &end, 16);
    if (errno != 0 || *end != '\0' || parsed > 0xFFFFFFFFul)
        return 0;
    *value = (u32)parsed;
    return 1;
}

/* The scenes the goldens wrote: scenes.tsv's lines. */
static uint64_t scenes_written(void) {
    char path[4096];
    uint64_t lines = 0;
    FILE* file;
    int c;
    snprintf(path, sizeof path, "%s/scenes.tsv", goldens_dir);
    file = fopen(path, "rb");
    if (file == NULL)
        return 0;
    while ((c = fgetc(file)) != EOF)
        lines += c == '\n';
    fclose(file);
    return lines;
}

/* --goldens's main loop: serves the captures until every scene is written, then prints the goldens line, or until the
   tick limit stops them. Never returns. */
static void serve(void) {
    const sl_result result = sl_goldens_serve();
    if (result != SL_OK) {
        fprintf(stderr, "ac_port: sl_goldens_serve: %s\n", sl_result_string(result));
        ac_port_report("limit");
        _Exit(1);
    }
    printf("ac_port: goldens scenes %" PRIu64 " frames %" PRIu64 "\n", scenes_written(),
           ac_goldens_frames());
    fflush(stdout);
    _Exit(0);
}

/* The game thread "main": it owns the ticks from the runtime's start, and OSInit binds it as the SDK's default
   thread (dolphin_os.c). */
static void* game_main(void* arg) {
    static const char* const boot_argv[] = { "ac_port", NULL };
    sl_result result;
    (void)arg;
    {
        /* Fixed ticks at the video rate, NTSC: the rates' zeros. Headless runs are deterministic. */
        sl_runtime_desc desc;
        memset(&desc, 0, sizeof desc);
        desc.tick_mode = SL_TICK_FIXED;
        desc.deterministic = window ? 0 : 1;
        result = sl_runtime_start(&desc);
        if (result != SL_OK)
            _Exit(failed("sl_runtime_start", result));
    }
    if (!window) {
        result = sl_retrace_add_callback(stop_at_limit, NULL);
        if (result != SL_OK)
            _Exit(failed("sl_retrace_add_callback", result));
    }
    ac_boot_main(1, (const char**)boot_argv);
    _Exit(1);
}

/* --window's main loop: presents the latest submitted frame until the window is closed or, with --ticks, the runtime
   has begun the limit's ticks. Never returns. */
static void present(sl_window shown) {
    uint64_t frames = 0;
    int closed = 0;
    for (;;) {
        sl_window_info info;
        sl_window_pump();
        sl_input_update();
        memset(&info, 0, sizeof info);
        if (sl_window_quit_requested() || (sl_window_get_info(shown, &info) == SL_OK && info.close_requested)) {
            closed = 1;
            break;
        }
        if (ticks_given && sl_tick_count() >= tick_limit)
            break;
        sl_window_frame_begin(shown);
        sl_render_present(1.0f, NULL); /* SL_ERROR_NOT_FOUND until the game submits its first frame */
        if (sl_window_present(shown, 1.0f) == SL_OK)
            frames++;
    }
    sl_audio_mix_stats audio;
    sl_audio_get_mix_stats(&audio);
    printf("ac_port: window ticks %" PRIu64 " frames %" PRIu64 " audio %" PRIu64 " loud %" PRIu64 " dsp %" PRIu64 "\n",
           sl_tick_count(), frames, audio.frames, audio.loud_frames, (uint64_t)ac_dsp_sounding_frames());
    fflush(stdout);
    _Exit(closed || ac_play_reached() ? 0 : 1);
}

int main(int argc, char** argv) {
    const char* package = NULL;
    sl_window shown = { 0 };
    uint32_t emu64_flags = 0;
    sl_result result;
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--package") == 0 && i + 1 < argc && package == NULL) {
            package = argv[++i];
        } else if (strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) {
            if (!parse_ticks(argv[++i], &tick_limit))
                return usage_error();
            ticks_given = 1;
        } else if (strcmp(argv[i], "--validate") == 0) {
            emu64_flags |= SL_EMU64_VALIDATE; /* every use of a program hashes its ranges again */
        } else if (strcmp(argv[i], "--diff") == 0) {
            ac_diff_enable(); /* the differential test, before boot: OSInit takes the reference's buffers */
        } else if (strcmp(argv[i], "--reference") == 0) {
            ac_reference_enable(); /* before boot, as --diff */
        } else if (strcmp(argv[i], "--measure") == 0) {
            measure = 1;
            ac_measure_enable(); /* play's first frame ends no run */
        } else if (strcmp(argv[i], "--window") == 0 && goldens == NULL) {
            window = 1;
            ac_window_enable(); /* play's first frame ends no run */
        } else if (strcmp(argv[i], "--goldens") == 0 && i + 2 < argc && goldens == NULL && !window) {
            goldens = argv[++i];
            goldens_dir = argv[++i];
            ac_goldens_enable(); /* play's first frame ends no run */
        } else if (strcmp(argv[i], "--qrand-seed") == 0 && i + 1 < argc) {
            if (!parse_seed(argv[++i], &seed))
                return usage_error();
            seed_given = 1;
        } else {
            return usage_error();
        }
    }
    if (package == NULL || (goldens != NULL && ac_diff_enabled()))
        return usage_error(); /* the goldens take the frames the kit draws, which --diff draws twice */
    if (measure && (window || goldens != NULL || ac_diff_enabled()))
        return usage_error(); /* the measured run presents each frame once, headless */
    if (ac_reference_enabled() && (window || ac_diff_enabled() || (emu64_flags & SL_EMU64_VALIDATE) != 0))
        return usage_error(); /* the reference runs headless, without the kit */

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
    if (window) {
        /* The window system before the render module, so the GPU sink's device shares its video driver: the
           environment's choice, else the platform's. */
        result = sl_window_init(NULL);
        if (result != SL_OK)
            return failed("sl_window_init", result);
    }
    {
        /* Headless, the null sink; --window, --goldens and --measure, the GPU sink at 640 x 480. Each frame is
           checked and hashed at submit. --diff captures each frame too. */
        sl_render_desc desc;
        memset(&desc, 0, sizeof desc);
        desc.sink = window || goldens != NULL || measure ? SL_RENDER_SINK_GPU : SL_RENDER_SINK_NULL;
        desc.flags = SL_RENDER_HASH_FRAMES | (ac_diff_enabled() ? SL_RENDER_CAPTURE_FRAMES : 0u);
        desc.max_views = 256;
        result = sl_render_init(&desc);
        if (result != SL_OK)
            return failed("sl_render_init", result);
    }
    if (goldens != NULL) {
        result = sl_goldens_open(goldens, goldens_dir);
        if (result != SL_OK)
            return failed("sl_goldens_open", result);
    }
    if (measure) {
        /* A median from every frame: at most one a tick. */
        result = sl_measure_open(tick_limit < UINT32_MAX ? (uint32_t)tick_limit : UINT32_MAX);
        if (result != SL_OK)
            return failed("sl_measure_open", result);
    }
    if (!ac_reference_enabled()) {
        /* emu64's texture cache takes the executable's read-only and initialized data as static (emu64.c's
           _f_rodata to _e_data), the game's own and foresta's, linked in. --reference makes no kit. */
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
        desc.importer_version = 2; /* ac::importer_version */
        result = sl_fs_mount(&desc, &mount);
        if (result == SL_ERROR_VERSION_MISMATCH) {
            fprintf(stderr, "ac_port: %s is from another version of the port; import the disc again with ac-import\n",
                    package);
            return 1;
        }
        if (result != SL_OK)
            return failed("sl_fs_mount", result);
    }
    {
        /* Headless: 2001-12-14 12:00:00.000, a Friday, the year's 348th day. --window: the host's clock. */
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
        desc.deterministic = window ? 0 : 1;
        desc.start_calendar = window ? NULL : &start;
        result = sl_os_start(&desc);
        if (result != SL_OK)
            return failed("sl_os_start", result);
    }
    {
        /* The audio interface's frames on os alarms. Headless, the null sink; --window, the audio device at its own
           rate, through the environment's driver, else the platform's. */
        sl_audio_desc desc;
        memset(&desc, 0, sizeof desc);
        desc.sink = window ? SL_AUDIO_SINK_DEVICE : SL_AUDIO_SINK_NULL;
        result = ac_audio_init(&desc);
        if (result != SL_OK)
            return failed("ac_audio_init", result);
    }
    if (window) {
        /* Input's GameCube profile with its default bindings: the keyboard on port 0, each gamepad on the first free
           one, as the window's pump reports them. Both before the game's first PADRead. */
        sl_window_desc desc;
        result = sl_input_init(NULL);
        if (result != SL_OK)
            return failed("sl_input_init", result);
        memset(&desc, 0, sizeof desc);
        desc.title = "Animal Crossing";
        desc.width = 1280;
        desc.height = 960;
        desc.pacing = SL_PACING_VSYNC;
        shown = sl_window_create(&desc);
        if (shown.id == 0)
            return failed("sl_window_create", SL_ERROR_UNSUPPORTED);
    }
    {
        /* Boot's main on a game thread of its own, at the attach's default priority. */
        sl_os_thread_desc desc;
        sl_os_thread game;
        memset(&desc, 0, sizeof desc);
        desc.entry = game_main;
        desc.name = "main";
        game = sl_os_thread_create(&desc);
        if (game.id == 0)
            return failed("sl_os_thread_create", SL_ERROR_OUT_OF_MEMORY);
        result = sl_os_thread_start(game);
        if (result != SL_OK)
            return failed("sl_os_thread_start", result);
        if (window)
            present(shown);
        if (goldens != NULL)
            serve();
        if (measure) {
            /* Presents each frame the game thread hands over until the limit stops it; the game thread ends the run. */
            result = sl_measure_serve();
            if (result != SL_OK)
                _Exit(failed("sl_measure_serve", result));
        }
        /* "main" never returns: the run ends with _Exit. */
        sl_os_thread_join(game, NULL);
    }
    return 1;
}

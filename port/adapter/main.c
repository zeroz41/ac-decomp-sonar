/* ac_port --package <file> [--ticks <n>] [--validate] [--diff] [--reference]
           [--window [--user-dir <dir>] [--set <key>=<value>]...
                     [--replay <recording> [--unpaced] | --record <recording> [--input-script <script>] [--unpaced]]
            | --goldens <scenes> <dir> | --measure [--present-hz <n>] | --replay <recording>] [--qrand-seed <hex>]:
   Animal Crossing on sonarlib
   (port/NOTES.md, The program). The process's main thread starts the library and the render module, makes the emu64
   the glue records each task with, mounts the package as game:, starts the CPU model, and creates the game thread
   "main", which starts the runtime and calls boot's main, which never returns.

   Headless, the default: deterministic, on the null sinks, the game's sound mixed and counted but not heard. The main
   thread joins "main", and the run ends at the submit of the frame that holds play's first task (emu64_glue.c, from
   the GX glue's ac_frame_end) or at the tick limit. With --diff, each task is drawn also by the console's emu64 and
   the two are compared (emu64_glue.c).

   --window: in real time, on the GPU sink, in a window, with the game's pads on the keyboard and gamepads and its
   sound on the audio device. The main thread presents at the clock's alpha for each image's time until the window
   is closed, or until --ticks's limit when it is given. It mounts the player's user directories, or every one under
   --user-dir's directory, keeps the settings in config:settings.toml with each --set over them for the run, keeps the
   memory cards in the save store at user:saves, the card in each slot by its setting, card-a in slot A unless the
   settings name another, and shows sonarlib's overlay on F1 (port/NOTES.md, The window; README.md, Saves). Every
   other run mounts no user directory, reads no settings file and puts no card in either slot, and only a replay
   registers a setting, os's. With --replay, the window plays the recording from its start, as headless does,
   deterministic and paced by the host, so frame for frame as headless at the game's speed, or as fast as it can with
   --unpaced, with no save store and no card in either slot. The replay's end prints the replay line and ends the loop
   (port/NOTES.md, Replays).

   --record, with --window and --ticks: the window records its input to the file from tick 0, deterministic and paced
   by the host, or as fast as it can with --unpaced, at the host's calendar, with os's settings bound as play binds
   them, no save store and no card in either slot. --input-script loads a device script (sonar/input.h) for the
   devices, in place of the host's. At the limit the recording ends, the record line is printed, named record, and
   the loop ends: exit 0, or 1 when the recording failed (port/NOTES.md, Recording).

   --goldens: headless and deterministic, but on the GPU sink with no window, writing the frames the scenes file names
   into the directory (sonar/goldens.h, port/NOTES.md, Goldens). The GX glue's frame begin hands each frame's keys to
   the goldens (emu64_glue.c), play's first frame ends no run, and the main thread serves the captures until every
   scene is written, or until the tick limit stops them. --qrand-seed gives init_rnd's seed (sys_math.c).

   --measure: headless and deterministic, on the GPU sink with no window, timing each frame from play's first to the
   tick limit (sonar/measure.h, port/NOTES.md, Measured runs). The GX glue's frame end hands each frame to the
   measured run, which waits while the main thread presents it: once at alpha 1, or with --present-hz at each of the
   display's times at that rate in the frame's tick, at the clock's alpha. At the limit the game thread prints the
   measure line, the presents line and the report, named measured, and the run ends, exit 0.

   --reference: the kit is not made, and the console's emu64 draws every task through the GX front end into the
   game's own frame (emu64_glue.c), headless, with --goldens or with --measure at no present rate.

   --replay: headless and deterministic, the pads read from an input recording (sonar/input.h; --record makes one,
   and sonarlib's sl-replay builds one from a script), which the main thread starts before os, so it plays from tick
   0, with os's settings bound and given the recorded ones, os at the recorded calendar, else at the headless one, and
   the recorded seed. Play's first frame prints the play line and ends no run, and when the replay ends, at the tick
   after its last, the run prints the report, named replay, and ends, exit 0. Each line names the scene. No tick limit
   applies unless --ticks gives one (port/NOTES.md, Replays). With --window, above. */
#include "ac_port.h"

#include <port/emu64_glue.h>
#include <port/goldens.h>

#include <sonar/audio.h>
#include <sonar/card/memcard.h>
#include <sonar/emu64/emu64.h>
#include <sonar/fs.h>
#include <sonar/goldens.h>
#include <sonar/input.h>
#include <sonar/measure.h>
#include <sonar/mem.h>
#include <sonar/os.h>
#include <sonar/render.h>
#include <sonar/runtime.h>
#include <sonar/settings.h>
#include <sonar/sonar.h>
#include <sonar/ui.h>
#include <sonar/window.h>

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ac_boot_main(int argc, const char** argv); /* src/static/boot.c's main */

static const char usage[] = "usage: ac_port --package <file> [--ticks <n>] [--validate] [--diff] [--reference]\n"
                            "               [--window [--user-dir <dir>] [--set <key>=<value>]...\n"
                            "                         [--replay <recording> [--unpaced]\n"
                            "                          | --record <recording> [--input-script <script>] [--unpaced]]\n"
                            "                | --goldens <scenes> <dir> | --measure [--present-hz <n>]\n"
                            "                | --replay <recording>]\n"
                            "               [--qrand-seed <hex>]\n";

static uint64_t tick_limit = 3600;
static int ticks_given;          /* --ticks */
static int window;               /* --window */
static const char* goldens;      /* --goldens's scenes file */
static const char* goldens_dir;  /* and its output directory */
static int measure;              /* --measure */
static uint64_t measured;        /* frames handed to the measured run */
static uint64_t measured_first;  /* the tick of the first */
static uint64_t measured_last;   /* and of the latest */
static uint32_t present_hz;      /* --present-hz: the measured run's display rate; 0, each frame presented once */
static int seed_given;           /* --qrand-seed */
static u32 seed;
static const char* replay;               /* --replay's recording */
static void* replay_data;                /* and its bytes, read whole before the game thread starts */
static size_t replay_size;
static sl_input_replay_info replay_info; /* and what it says about its start */
static const char* record;               /* --record's recording */
static FILE* record_file;                /* opened before the game thread starts; closed at the recording's end */
static int record_stopping;              /* sl_input_record_stop was called, at the retrace before the record line's */
static sl_result record_result;          /* how the recording ended, set before run_ended */
static const char* input_script;         /* --input-script's script */
static int unpaced;                      /* --unpaced: a window replay or recording as fast as it can */
static atomic_int run_ended;             /* a window replay's or recording's end, which ends the present loop */

/* --window's settings. */
static const char* user_dir;        /* --user-dir */
static const char** assignments;    /* each --set's key=value, in order */
static int assignment_count;        /* and how many */
static char settings_path[4096];    /* the settings file's host path */
static char user_path[4096];        /* and user:'s */
static char saves_path[4096];       /* and the save store's, user:saves */
static int settings_broken;         /* the file did not load, so it is never saved */
static atomic_int settings_changed; /* a setting's value in effect changed since the last save */

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
    static const char* const names[4] = {"game", "prepare", "present", "gpu"};
    const sl_measure_time* times[4];
    int i;
    memset(&stats, 0, sizeof stats);
    (void)sl_measure_get_stats(&stats);
    sl_render_get_stats(&render);
    times[0] = &stats.game;
    times[1] = &stats.prepare;
    times[2] = &stats.present;
    times[3] = &stats.gpu;
    printf("ac_port: measure %s frames %" PRIu64 " ticks %" PRIu64, ac_reference_enabled() ? "reference" : "kit",
           stats.frames, measured_last - measured_first);
    for (i = 0; i < 4; i++)
        printf(" %s %" PRIu64 " %" PRIu64 " %" PRIu64, names[i], times[i]->median_ns, times[i]->max_ns,
               times[i]->total_ns);
    printf(" draws %" PRIu64 " calls %" PRIu64 " pipelines %" PRIu64 " buffers %" PRIu64 " textures %" PRIu64
           " storage %" PRIu64 " pushes %" PRIu64 "\n",
           render.draws, render.draw_calls, render.pipeline_binds, render.buffer_binds, render.texture_binds,
           render.storage_binds, render.uniform_pushes);
}

/* The presents line: the display's rate, 0 without one, then the presents, those held at alpha 1, the display times
   late, the frames dropped, the presents going backward, the frames with one before to blend from, the snaps only the
   library's checks found, and the snaps by cause, in sl_snap_cause's order, at each such frame's first present. */
static void presents_report(void) {
    static const char* const causes[SL_SNAP_CAUSE_COUNT] = { "none",     "unmatched", "explicit",  "forget",
                                                             "cut",      "all",       "duplicate", "rotation",
                                                             "distance", "gap",       "auto-cut" };
    sl_measure_stats stats;
    int i;
    memset(&stats, 0, sizeof stats);
    (void)sl_measure_get_stats(&stats);
    printf("ac_port: presents %" PRIu32 " count %" PRIu64 " held %" PRIu64 " late %" PRIu64 " dropped %" PRIu64
           " backward %" PRIu64 " pairs %" PRIu64 " unexplained %" PRIu64,
           present_hz, stats.presents, stats.held, stats.late, stats.dropped, stats.backward, stats.pairs,
           stats.unexplained);
    for (i = 0; i < SL_SNAP_CAUSE_COUNT; i++)
        printf(" %s %" PRIu64, causes[i], stats.snaps[i]);
    printf("\n");
}

/* Ends the run once the runtime has begun the limit's ticks without a frame of play; with --goldens, stops the
   captures, and the main thread ends the run. With --measure, once play was reached, stops the measured run and
   prints its two lines and the report, here on the game thread at a retrace, while no present runs: the game thread
   waits in sl_measure_frame for each frame's presents. With --replay, first ends the run once the replay has ended,
   with the report, named replay. In a window, the only runs it is registered for are a replay's, for which it prints
   the report once and tells the present loop, which ends the run, as it does at --ticks's limit, and a recording's:
   at the retrace where the runtime has begun the limit's ticks it stops the recording, which ends at that tick's
   boundary, after the retrace callbacks, and one retrace later, where a replay of it ends, it closes the file, prints
   the report, named record, and tells the present loop. */
static void stop_at_limit(void* user, uint64_t retrace) {
    (void)user;
    (void)retrace;
    if (replay != NULL && !sl_input_replay_active()) {
        if (window) {
            if (!atomic_load(&run_ended)) {
                ac_port_report("replay");
                atomic_store(&run_ended, 1);
            }
            return;
        }
        ac_port_report("replay");
        _Exit(0);
    }
    if (record != NULL) {
        if (record_stopping && !atomic_load(&run_ended)) {
            sl_result result = sl_input_record_status();
            if (fclose(record_file) != 0 && result == SL_OK)
                result = SL_ERROR_IO;
            record_file = NULL;
            ac_port_report("record");
            if (result != SL_OK)
                fprintf(stderr, "ac_port: %s: the recording failed: %s\n", record, sl_result_string(result));
            record_result = result;
            atomic_store(&run_ended, 1);
        } else if (!record_stopping && sl_tick_count() >= tick_limit) {
            const sl_result result = sl_input_record_stop();
            if (result != SL_OK)
                fprintf(stderr, "ac_port: %s: sl_input_record_stop: %s\n", record, sl_result_string(result));
            record_stopping = 1;
        }
        return;
    }
    if (window)
        return;
    if (sl_tick_count() >= tick_limit) {
        if (goldens != NULL) {
            sl_goldens_stop();
            return;
        }
        if (measure && ac_play_reached()) {
            sl_measure_stop();
            measure_report();
            presents_report();
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

/* --present-hz's rate: a whole number from 1 to 1000. */
static int parse_hz(const char* text, uint32_t* hz) {
    char* end = NULL;
    unsigned long value;
    if (text[0] < '0' || text[0] > '9')
        return 0;
    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || *end != '\0' || value < 1 || value > 1000)
        return 0;
    *hz = (uint32_t)value;
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

/* Reads a file whole, --replay's recording or --input-script's script; on failure prints why. */
static int read_file(const char* path, void** data, size_t* size) {
    FILE* file = fopen(path, "rb");
    long length;
    if (file == NULL) {
        fprintf(stderr, "ac_port: %s: %s\n", path, strerror(errno));
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "ac_port: %s: %s\n", path, strerror(errno));
        fclose(file);
        return 0;
    }
    *size = (size_t)length;
    *data = calloc(*size != 0 ? *size : 1, 1); /* malloc is the game's (names.h) */
    if (*data == NULL || fread(*data, 1, *size, file) != *size) {
        fprintf(stderr, "ac_port: %s: cannot be read\n", path);
        fclose(file);
        return 0;
    }
    fclose(file);
    return 1;
}

/* Reads --replay's recording and what it says about its start: the seed from the port's data, the version byte 1,
   whether a seed was given and the seed, a u32 little-endian (game_main). A recording with the port's data gives the
   seed, or says none was given, so --qrand-seed beside it is a usage error. Returns 0, or main's exit code. */
static int read_replay(void) {
    sl_result result;
    if (!read_file(replay, &replay_data, &replay_size))
        return 1;
    result = sl_input_replay_read_info(replay_data, replay_size, &replay_info);
    if (result != SL_OK) {
        fprintf(stderr, "ac_port: %s: not a recording this library plays: %s\n", replay, sl_result_string(result));
        return 1;
    }
    if (replay_info.port_data != NULL) {
        const unsigned char* data = (const unsigned char*)replay_info.port_data;
        if (replay_info.port_data_size != 6 || data[0] != 1 || data[1] > 1) {
            fprintf(stderr, "ac_port: %s: the port's data is another port's or another version's\n", replay);
            return 1;
        }
        if (seed_given)
            return usage_error(); /* the recording gives the seed */
        seed_given = data[1];
        seed = (u32)data[2] | (u32)data[3] << 8 | (u32)data[4] << 16 | (u32)data[5] << 24;
    }
    return 0;
}

/* The write of the recording's bytes, at the runtime's tick boundary: to the file --record opened. */
static int write_record(void* user, const void* data, size_t size) {
    (void)user;
    return fwrite(data, 1, size, record_file) == size;
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
    if (record != NULL) {
        /* Before the runtime begins tick 0, so the recording holds every partition from it, with the port's data:
           the version byte, 1, whether a seed was given, and the seed, a u32 little-endian (read_replay). */
        unsigned char data[6];
        sl_input_record_desc desc;
        data[0] = 1;
        data[1] = (unsigned char)(seed_given != 0);
        data[2] = (unsigned char)seed;
        data[3] = (unsigned char)(seed >> 8);
        data[4] = (unsigned char)(seed >> 16);
        data[5] = (unsigned char)(seed >> 24);
        memset(&desc, 0, sizeof desc);
        desc.write = write_record;
        desc.port_data = data; /* copied at the start */
        desc.port_data_size = sizeof data;
        result = sl_input_record_start(&desc);
        if (result != SL_OK) {
            fprintf(stderr, "ac_port: %s: sl_input_record_start: %s\n", record, sl_result_string(result));
            _Exit(1);
        }
    }
    {
        /* Fixed ticks at the video rate, NTSC: the rates' zeros. Headless runs, replays and recordings are
           deterministic, a window's replay or recording paced by the host unless --unpaced. */
        sl_runtime_desc desc;
        memset(&desc, 0, sizeof desc);
        desc.tick_mode = SL_TICK_FIXED;
        desc.deterministic = !window || replay != NULL || record != NULL;
        desc.paced = window && (replay != NULL || record != NULL) && !unpaced;
        result = sl_runtime_start(&desc);
        if (result != SL_OK)
            _Exit(failed("sl_runtime_start", result));
    }
    if (!window || replay != NULL || record != NULL) {
        result = sl_retrace_add_callback(stop_at_limit, NULL);
        if (result != SL_OK)
            _Exit(failed("sl_retrace_add_callback", result));
    }
    ac_boot_main(1, (const char**)boot_argv);
    _Exit(1);
}

/* Marks the settings changed, on whichever thread applied the change: the main loop saves them. */
static void settings_changed_watch(void* user, sl_setting setting) {
    (void)user;
    (void)setting;
    atomic_store(&settings_changed, 1);
}

/* Writes the settings file when a setting changed since the last save, unless the file did not load, so a run that
   changes nothing leaves the player's text as it is. A failure is printed and the run goes on. Main thread. */
static void save_settings(void) {
    sl_result result;
    if (settings_broken || !atomic_load(&settings_changed))
        return;
    atomic_store(&settings_changed, 0);
    result = sl_settings_save_file(NULL);
    if (result != SL_OK)
        fprintf(stderr, "ac_port: cannot save settings: %s\n", sl_result_string(result));
}

/* Headless, no user directories: the run reads and writes nothing of the player's. --window mounts user: and config:,
   the player's directories or every one under --user-dir's, for the settings file and the memory cards' save store
   (start_saves), and the GPU sink keeps its pipeline list in their cache. */
static sl_result start_fs(void) {
    sl_fs_desc desc;
    memset(&desc, 0, sizeof desc);
    if (window) {
        desc.org_name = "sonarlib";
        desc.app_name = "animal-crossing";
        desc.user_root = user_dir;
    } else {
        desc.app_name = "ac_port";
        desc.flags = SL_FS_INIT_NO_USER_MOUNT;
    }
    return sl_fs_init(&desc);
}

/* --window's memory cards, after sl_fs_init: a save store on user:saves, keeping two backups of each save, for
   CARDInit to keep the cards in. The settings bind puts them in the slots (main). A replay and a recording have
   neither. Returns 0, or main's exit code. */
static int start_saves(void) {
    sl_save_store_desc desc;
    sl_save_store store;
    sl_result result = sl_fs_user_dir(SL_USER_DIR_SAVES, saves_path, sizeof saves_path, NULL);
    if (result != SL_OK)
        return failed("sl_fs_user_dir", result);
    memset(&desc, 0, sizeof desc);
    desc.path = NULL; /* user:saves */
    desc.backups = 2;
    result = sl_save_store_open(&desc, &store);
    if (result != SL_OK)
        return failed("sl_save_store_open", result);
    ac_card_set_store(store);
    return 0;
}

/* --window's settings, after sl_fs_init: prints the file's host path, loads config:settings.toml, takes each --set
   over it for the run, watches the settings to save them, and binds os's system settings before os starts, so the
   game boots with the stored ones, and a recording holds them; a replay's recorded ones are in effect instead while
   it runs, and the file keeps the player's (start_replay). A file that does not load is left as it is: the run goes
   on at the defaults and never saves it. Returns 0, or main's exit code. */
static int start_settings(void) {
    static const char file[] = "/settings.toml";
    sl_settings_error error;
    size_t length = 0;
    sl_result result;
    int i;
    result = sl_fs_user_dir(SL_USER_DIR_CONFIG, settings_path, sizeof settings_path - (sizeof file - 1), &length);
    if (result != SL_OK)
        return failed("sl_fs_user_dir", result);
    memcpy(settings_path + length, file, sizeof file);
    result = sl_fs_user_dir(SL_USER_DIR_DATA, user_path, sizeof user_path, NULL);
    if (result != SL_OK)
        return failed("sl_fs_user_dir", result);
    printf("ac_port: settings %s\n", settings_path);
    fflush(stdout);
    memset(&error, 0, sizeof error);
    result = sl_settings_load_file(NULL, &error);
    if (result != SL_OK && result != SL_ERROR_NOT_FOUND) { /* no file is a first run */
        const char* message = error.message != NULL ? error.message : sl_result_string(result);
        settings_broken = 1;
        if (error.line != 0)
            fprintf(stderr, "ac_port: %s:%" PRIu32 ":%" PRIu32 ": %s; it is left as it is\n", settings_path, error.line,
                    error.column, message);
        else
            fprintf(stderr, "ac_port: %s: %s; it is left as it is\n", settings_path, message);
    }
    for (i = 0; i < assignment_count; i++) {
        result = sl_settings_override(assignments[i]);
        if (result == SL_ERROR_INVALID_ARGUMENT) {
            /* malformed; a key or value no setting takes is found after the binds (check_assignments) */
            fprintf(stderr, "ac_port: --set %s: malformed\n", assignments[i]);
            return usage_error();
        }
        if (result != SL_OK)
            return failed("sl_settings_override", result);
    }
    result = sl_settings_add_watch(settings_changed_watch, NULL);
    if (result != SL_OK)
        return failed("sl_settings_add_watch", result);
    result = sl_os_settings_bind();
    if (result != SL_OK)
        return failed("sl_os_settings_bind", result);
    return 0;
}

/* After the last bind, each --set must name a registered setting that holds its override: one naming a key nothing
   registers, or with a value its setting refused at registration (logged then), is a usage error naming the
   assignment. A replay and a recording register no card's setting, so one naming them is too, and a replay takes
   every setting that reaches the game, its TICK settings, from the recording, so one naming them is too. Returns 0,
   or main's exit code. */
static int check_assignments(void) {
    int i;
    for (i = 0; i < assignment_count; i++) {
        /* sl_settings_override took it, so it has an '=' and a key of at most SL_SETTINGS_MAX_KEY - 1 bytes, which
           it read with spaces and tabs trimmed */
        const char* text = assignments[i];
        const char* equals = strchr(text, '=');
        char key[SL_SETTINGS_MAX_KEY];
        size_t begin = 0;
        size_t end = equals != NULL ? (size_t)(equals - text) : 0;
        sl_setting setting;
        while (begin < end && (text[begin] == ' ' || text[begin] == '\t'))
            begin++;
        while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t'))
            end--;
        if (end - begin >= sizeof key)
            end = begin; /* none */
        memcpy(key, text + begin, end - begin);
        key[end - begin] = '\0';
        setting = sl_settings_find(key);
        if (setting.id == 0) {
            fprintf(stderr, "ac_port: --set %s: no setting has the key \"%s\"\n", text, key);
            return usage_error();
        }
        if (!sl_settings_is_overridden(setting)) {
            fprintf(stderr, "ac_port: --set %s: the setting refused the value\n", text);
            return usage_error();
        }
        if (replay != NULL) {
            sl_setting_info info;
            memset(&info, 0, sizeof info);
            if (sl_settings_get_info(setting, &info) == SL_OK && info.apply == SL_SETTING_APPLY_TICK) {
                fprintf(stderr, "ac_port: --set %s: a replay takes this setting from its recording\n", text);
                return usage_error();
            }
        }
    }
    return 0;
}

/* --window's overlay, after the window's, audio's, input's and the cards' binds: sonarlib's pages and an Animal
   Crossing page with the controls, as input's GameCube profile binds them by default, the files' host paths, and the
   card in each slot, which the page edits (the kit's settings; sonarlib's Settings page lists only sonar.* keys). A
   page's text is copied when it is added, so it cannot show that a slot was left empty for a name the kit refused.
   A replay and a recording have no save store and no card, so their page says so. Returns 0, or main's exit code. */
static int start_overlay(void) {
    static char settings_line[sizeof settings_path + 32];
    static char user_line[sizeof user_path + 32];
    static char saves_line[sizeof saves_path + 32];
    sl_ui_item items[16];
    sl_ui_page_desc page;
    uint32_t count = 0;
    sl_result result;
    snprintf(settings_line, sizeof settings_line, "Settings file: %s", settings_path);
    snprintf(user_line, sizeof user_line, "User directory: %s", user_path);
    snprintf(saves_line, sizeof saves_line, "Saves directory: %s", saves_path);
    items[count].type = SL_UI_ITEM_SECTION;
    items[count++].text = "Controls";
    items[count].type = SL_UI_ITEM_TEXT;
    items[count++].text = "The keyboard is controller 1. Each gamepad takes the next free controller.";
    items[count].type = SL_UI_ITEM_TEXT;
    items[count++].text = "W A S D: control stick. I J K L: C-stick.";
    items[count].type = SL_UI_ITEM_TEXT;
    items[count++].text = "Space: A. Left Shift: B. E: X. Q: Y. R: Z. Return: Start.";
    items[count].type = SL_UI_ITEM_TEXT;
    items[count++].text = "Arrow keys: D-pad. 1 and 3: L and R.";
    items[count].type = SL_UI_ITEM_TEXT;
    items[count++].text = "F1: shows or hides this overlay.";
    items[count].type = SL_UI_ITEM_TEXT;
    items[count++].text = "F11 or Alt+Enter: switches between a window and fullscreen.";
    items[count].type = SL_UI_ITEM_SECTION;
    items[count++].text = "Files";
    items[count].type = SL_UI_ITEM_TEXT;
    items[count++].text = settings_line;
    items[count].type = SL_UI_ITEM_TEXT;
    items[count++].text = user_line;
    if (replay == NULL && record == NULL) {
        items[count].type = SL_UI_ITEM_TEXT;
        items[count++].text = saves_line;
    }
    items[count].type = SL_UI_ITEM_SECTION;
    items[count++].text = "Memory cards";
    if (replay != NULL) {
        items[count].type = SL_UI_ITEM_TEXT;
        items[count++].text = "A replay runs with no card in either slot, as headless.";
    } else if (record != NULL) {
        items[count].type = SL_UI_ITEM_TEXT;
        items[count++].text = "A recording runs with no card in either slot.";
    } else {
        items[count].type = SL_UI_ITEM_SETTING; /* a change applies at the tick boundary, so its widget marks it */
        items[count++].text = "kit.card.slot_a";
        items[count].type = SL_UI_ITEM_SETTING;
        items[count++].text = "kit.card.slot_b";
    }
    result = sl_ui_init(NULL); /* F1 toggles it */
    if (result != SL_OK)
        return failed("sl_ui_init", result);
    memset(&page, 0, sizeof page);
    page.title = "Animal Crossing";
    page.items = items;
    page.item_count = count;
    result = sl_ui_add_page(&page, NULL);
    if (result != SL_OK)
        return failed("sl_ui_add_page", result);
    return 0;
}

/* Every replay's start, headless and in a window, on the main thread after the mount and sl_input_init and before
   os starts: binds os's settings headless, as --window's start_settings did, then starts the replay, which checks
   the recorded packages against the one mounted and puts the recorded settings in effect at once, so os starts with
   them and the replay holds every partition from tick 0. Returns 0, or main's exit code. */
static int start_replay(void) {
    sl_result result;
    if (!window) {
        result = sl_os_settings_bind();
        if (result != SL_OK)
            return failed("sl_os_settings_bind", result);
    }
    result = sl_input_replay_start(replay_data, replay_size);
    if (result == SL_ERROR_VERSION_MISMATCH) {
        fprintf(stderr, "ac_port: %s was recorded with another package; import the disc it was recorded from\n",
                replay);
        return 1;
    }
    if (result != SL_OK) {
        fprintf(stderr, "ac_port: %s: sl_input_replay_start: %s\n", replay, sl_result_string(result));
        return 1;
    }
    return 0;
}

/* --input-script's device script, on the main thread after the binds, so its set steps' keys are registered, and
   before the game thread starts, where no pump has run: its devices take the ports, and the host's take none. A line
   the library refuses is printed with its number. Returns 0, or main's exit code. */
static int load_script(void) {
    sl_input_script_desc desc;
    sl_input_script_error error;
    void* text = NULL;
    size_t size = 0;
    sl_result result;
    if (!read_file(input_script, &text, &size))
        return 1;
    memset(&desc, 0, sizeof desc);
    desc.text = (const char*)text;
    desc.size = size;
    /* the video rate's zero, NTSC, as the runtime's */
    memset(&error, 0, sizeof error);
    result = sl_input_script_load(&desc, &error); /* copies the text, which is kept: free is the game's (names.h) */
    if (result == SL_OK)
        return 0;
    if (error.line != 0)
        fprintf(stderr, "ac_port: %s:%" PRIu32 ": %s\n", input_script, error.line,
                error.message != NULL ? error.message : sl_result_string(result));
    else
        fprintf(stderr, "ac_port: %s: %s\n", input_script,
                error.message != NULL ? error.message : sl_result_string(result));
    return 1;
}

/* A rate in whole Hz, rounded as the window's binding rounds the limiter's; 0 for none. */
static uint64_t whole_hz(sl_rate rate) {
    return sl_rate_is_valid(rate) ? ((uint64_t)rate.num + rate.den / 2) / rate.den : 0;
}

/* --window's main loop: presents the latest submitted frame, at the clock's alpha for the image's presentation time
   (1 for a stale window), with the overlay over it, until the window is closed, with --ticks the runtime has begun
   the limit's ticks, or a replay or a recording has ended. The pump updates input. The settings are saved when they
   changed, while the overlay is hidden and at the end, before the window line. A recording's run exits 0 only when
   the recording ended with every write done, so not when the window closed first. Never returns. */
static void present(sl_window shown) {
    static const char* const pacings[] = { "vsync", "mailbox", "immediate", "limiter" };
    char cards[2][SL_MEMCARD_CARD_NAME_MAX + 1];
    int slot;
    uint64_t frames = 0;
    uint64_t last = 0;
    int closed = 0;
    for (;;) {
        sl_window_info info;
        sl_resolved_frame resolved;
        sl_ui_frame_desc overlay;
        uint64_t now;
        float alpha;
        sl_window_pump();
        memset(&info, 0, sizeof info);
        if (sl_window_quit_requested() || (sl_window_get_info(shown, &info) == SL_OK && info.close_requested)) {
            closed = 1;
            break;
        }
        if ((ticks_given && record == NULL && sl_tick_count() >= tick_limit) || atomic_load(&run_ended))
            break; /* a recording's run ends at its end, after the limit (stop_at_limit) */
        now = sl_window_frame_begin(shown);
        alpha = now != 0 ? (float)sl_clock_alpha(now) : 1.0f;
        memset(&overlay, 0, sizeof overlay);
        /* SL_ERROR_NOT_FOUND until the game submits its first frame */
        if (sl_render_present(alpha, &resolved) == SL_OK)
            overlay.resolved = &resolved;
        overlay.delta_seconds = last != 0 && now > last ? (float)((double)(now - last) * 1e-9) : 0.0f;
        overlay.width = info.width;
        overlay.height = info.height;
        overlay.framebuffer_scale = info.pixel_density;
        overlay.window = shown;
        (void)sl_ui_frame(&overlay);
        last = now;
        if (sl_window_present(shown, alpha) == SL_OK)
            frames++;
        if (!sl_ui_is_visible())
            save_settings();
    }
    save_settings();
    sl_audio_mix_stats audio;
    sl_window_info info;
    sl_render_stats render;
    sl_audio_get_mix_stats(&audio);
    memset(&info, 0, sizeof info);
    (void)sl_window_get_info(shown, &info);
    sl_render_get_stats(&render);
    for (slot = 0; slot < 2; slot++) {
        if (sl_memcard_get_card((uint32_t)slot, cards[slot]) != SL_MEMCARD_READY)
            strcpy(cards[slot], "-"); /* no card */
    }
    printf("ac_port: window ticks %" PRIu64 " frames %" PRIu64 " audio %" PRIu64 " loud %" PRIu64 " dsp %" PRIu64
           " pacing %s limit %" PRIu64 " gain %.3f pipelines %" PRIu32 " warmed %" PRIu32 " lists %" PRIu32
           " cards %s %s\n",
           sl_tick_count(), frames, audio.frames, audio.loud_frames, (uint64_t)ac_dsp_sounding_frames(),
           (uint32_t)info.pacing < sizeof pacings / sizeof pacings[0] ? pacings[info.pacing] : "unknown",
           whole_hz(info.frame_limit), (double)sl_audio_output_gain(), render.pipelines, render.pipelines_warmed,
           render.pipeline_lists_written, cards[0], cards[1]);
    fflush(stdout);
    if (record != NULL) {
        if (!atomic_load(&run_ended))
            fprintf(stderr, "ac_port: %s: the window closed before the recording's end\n", record);
        _Exit(atomic_load(&run_ended) && record_result == SL_OK ? 0 : 1);
    }
    _Exit(closed || ac_play_reached() || atomic_load(&run_ended) ? 0 : 1);
}

int main(int argc, char** argv) {
    const char* package = NULL;
    sl_window shown = { 0 };
    uint32_t emu64_flags = 0;
    sl_result result;
    int i;
    assignments = calloc((size_t)argc, sizeof *assignments);
    if (assignments == NULL)
        return failed("calloc", SL_ERROR_OUT_OF_MEMORY);
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
        } else if (strcmp(argv[i], "--present-hz") == 0 && i + 1 < argc) {
            if (!parse_hz(argv[++i], &present_hz))
                return usage_error();
        } else if (strcmp(argv[i], "--window") == 0 && goldens == NULL) {
            window = 1;
            ac_window_enable(); /* play's first frame ends no run */
        } else if (strcmp(argv[i], "--user-dir") == 0 && i + 1 < argc && user_dir == NULL) {
            user_dir = argv[++i];
        } else if (strcmp(argv[i], "--set") == 0 && i + 1 < argc) {
            assignments[assignment_count++] = argv[++i]; /* applied after the settings file, which needs fs */
        } else if (strcmp(argv[i], "--goldens") == 0 && i + 2 < argc && goldens == NULL && !window) {
            goldens = argv[++i];
            goldens_dir = argv[++i];
            ac_goldens_enable(); /* play's first frame ends no run */
        } else if (strcmp(argv[i], "--replay") == 0 && i + 1 < argc && replay == NULL) {
            replay = argv[++i];
            ac_replay_enable(); /* play's first frame ends no run */
        } else if (strcmp(argv[i], "--record") == 0 && i + 1 < argc && record == NULL) {
            record = argv[++i];
            ac_replay_enable(); /* each line names the scene, as a replay's, whose line the record line is */
        } else if (strcmp(argv[i], "--input-script") == 0 && i + 1 < argc && input_script == NULL) {
            input_script = argv[++i];
        } else if (strcmp(argv[i], "--unpaced") == 0) {
            unpaced = 1;
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
        return usage_error(); /* the measured run presents each frame itself, headless */
    if (present_hz != 0 && (!measure || ac_reference_enabled()))
        return usage_error(); /* a display's rate is the measured run's, and the reference is measured at none */
    if (ac_reference_enabled() && (window || ac_diff_enabled() || (emu64_flags & SL_EMU64_VALIDATE) != 0))
        return usage_error(); /* the reference runs headless, without the kit */
    if ((user_dir != NULL || assignment_count != 0) && !window)
        return usage_error(); /* only --window keeps the player's directories and settings */
    if (replay != NULL && (goldens != NULL || measure || ac_diff_enabled() || ac_reference_enabled()))
        return usage_error(); /* a replay runs on the kit, headless or in a window */
    if (record != NULL && (!window || !ticks_given || replay != NULL))
        return usage_error(); /* a recording is a window's, to the tick limit */
    if (input_script != NULL && record == NULL)
        return usage_error(); /* a script's devices play only into a recording */
    if (unpaced && ((replay == NULL && record == NULL) || !window))
        return usage_error(); /* only a window's replay or recording is paced */
    if (replay != NULL) {
        int code;
        if (!ticks_given)
            tick_limit = UINT64_MAX; /* the replay's end ends the run */
        code = read_replay();
        if (code != 0)
            return code;
    }

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
        /* The player's files before the render module, which finds the GPU sink's pipeline list in their cache; then
           the settings. */
        int code;
        result = start_fs();
        if (result != SL_OK)
            return failed("sl_fs_init", result);
        code = replay != NULL || record != NULL ? 0 : start_saves(); /* no cards, as headless */
        if (code == 0)
            code = start_settings();
        if (code != 0)
            return code;
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
           checked and hashed at submit. --diff captures each frame too. max_snaps: a frame's first moves can snap 256
           actors' 11 owners, 2,816 calls, beside the weather's forgets and wraps, 11 calls a part; the default 256
           refused forgets. snap_distance: the game's unit (mFI_UNIT_BASE_SIZE), which parts real motion in the
           world's perspective views from the blends between two objects; core applies it to no orthographic view,
           such as the logo's font view (port/NOTES.md, Measured runs, after step 9b). */
        sl_render_desc desc;
        memset(&desc, 0, sizeof desc);
        desc.sink = window || goldens != NULL || measure ? SL_RENDER_SINK_GPU : SL_RENDER_SINK_NULL;
        desc.flags = SL_RENDER_HASH_FRAMES | (ac_diff_enabled() ? SL_RENDER_CAPTURE_FRAMES : 0u);
        desc.max_views = 256;
        desc.max_snaps = 8192;
        desc.snap_distance = 40.0f;
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
        /* A median from every frame: at most one a tick. --present-hz's rate, {n, 1}, else {0, 0}: each frame
           presented once. */
        sl_measure_desc desc;
        memset(&desc, 0, sizeof desc);
        desc.max_frames = tick_limit < UINT32_MAX ? (uint32_t)tick_limit : UINT32_MAX;
        desc.present_rate.num = present_hz;
        desc.present_rate.den = present_hz != 0 ? 1u : 0u;
        result = sl_measure_open(&desc);
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
    if (!window) {
        /* Headless, after the render module, as before: the GPU sink keeps no pipeline list. */
        result = start_fs();
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
        desc.importer_version = 4; /* ac::importer_version */
        result = sl_fs_mount(&desc, &mount);
        if (result == SL_ERROR_VERSION_MISMATCH) {
            fprintf(stderr, "ac_port: %s is from another version of the port; import the disc again with ac-import\n",
                    package);
            return 1;
        }
        if (result != SL_OK)
            return failed("sl_fs_mount", result);
    }
    if (replay != NULL || window) {
        /* Input's four GameCube ports, before os, so a replay starts before it. Headless, each disconnected until the
           recording connects it, with no backend. --window, the GameCube profile with its default bindings: the
           keyboard on port 0, each gamepad on the first free one, as the window's pump reports them, from the
           window's creation below; before the game's first PADRead. A replay holds the ports, and live input waits
           behind it. */
        result = sl_input_init(NULL);
        if (result != SL_OK)
            return failed("sl_input_init", result);
    }
    if (replay != NULL) {
        int code = start_replay();
        if (code != 0)
            return code;
    }
    {
        /* Headless: 2001-12-14 12:00:00.000, a Friday, the year's 348th day, deterministic. A replay: the recorded
           calendar, else that one. --window: the host's clock, and a recording deterministic from the host's
           calendar, which the recording holds. */
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
        desc.deterministic = !window || replay != NULL || record != NULL;
        if (replay != NULL && replay_info.has_calendar)
            desc.start_calendar = &replay_info.calendar;
        else if (desc.deterministic && record == NULL)
            desc.start_calendar = &start;
        result = sl_os_start(&desc);
        if (result != SL_OK)
            return failed("sl_os_start", result);
    }
    {
        /* The audio interface's frames on os alarms. Headless, the null sink; --window, the audio device at its own
           rate, through the environment's driver, else the platform's. A window's replay or recording outputs at the
           console's rate, as the null sink, so its counts are the headless run's; the platform resamples to the
           device. */
        sl_audio_desc desc;
        memset(&desc, 0, sizeof desc);
        desc.sink = window ? SL_AUDIO_SINK_DEVICE : SL_AUDIO_SINK_NULL;
        if (window && (replay != NULL || record != NULL))
            desc.output_rate = 32029;
        result = ac_audio_init(&desc);
        if (result != SL_OK)
            return failed("ac_audio_init", result);
    }
    if (window) {
        /* The window, whose pump adds the host's keyboard and gamepads to input unless a device script is loaded. */
        sl_window_desc desc;
        memset(&desc, 0, sizeof desc);
        desc.title = "Animal Crossing";
        desc.width = 1280;
        desc.height = 960;
        desc.pacing = SL_PACING_VSYNC;
        shown = sl_window_create(&desc);
        if (shown.id == 0)
            return failed("sl_window_create", SL_ERROR_UNSUPPORTED);
        /* The window's mode, pacing and limiter, the volume, the rumble strength and the cards in the slots, as the
           settings hold them, then the device script and the overlay. */
        result = sl_window_settings_bind(shown);
        if (result != SL_OK)
            return failed("sl_window_settings_bind", result);
        result = sl_audio_settings_bind();
        if (result != SL_OK)
            return failed("sl_audio_settings_bind", result);
        result = sl_input_settings_bind();
        if (result != SL_OK)
            return failed("sl_input_settings_bind", result);
        /* The card in each slot, which CARDInit puts in: card-a in slot A and none in B unless the settings or --set
           name others. A replay and a recording bind none, so both slots stay empty. */
        if (replay == NULL && record == NULL) {
            result = sl_memcard_settings_bind("card-a", NULL);
            if (result != SL_OK)
                return failed("sl_memcard_settings_bind", result);
        }
        {
            int code = check_assignments(); /* every setting --set can name is registered now */
            if (code == 0 && input_script != NULL)
                code = load_script();
            if (code == 0)
                code = start_overlay();
            if (code != 0)
                return code;
        }
    }
    if (record != NULL) {
        record_file = fopen(record, "wb");
        if (record_file == NULL) {
            fprintf(stderr, "ac_port: %s: %s\n", record, strerror(errno));
            return 1;
        }
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

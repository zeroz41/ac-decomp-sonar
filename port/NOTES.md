# The port's notes

## The package

`ac-import <source> <package> <cache>` (`importer/`) writes the package the game reads from boot to Town's first frame and its sound, importer `ac`, version 2. Each entry is a whole disc file, at the game's own name for it, or made from one:

| Entry | Recipe | Conversion |
|---|---|---|
| `COPYDATE` | `copy` 1 | none: text |
| `static.str` | `copy` 1 | none: strings |
| `forest_1st.arc` | `archive` 1 | the directory, and the files below |
| `forest_2nd.arc` | `archive` 1 | the directory, and the file below |
| `famicom.arc` | `archive` 1 | the directory only |
| `audiorom.img` | `audiorom` 1 | the banks laid out for the host ([Sound](#sound)) |
| `audiorom.tbl` | `audiorom-table` 1 | made from `audiorom.img`: the host image's bank and data tables |

Each archive stays whole. Its directory is laid out again at the host's sizes and in the host's byte order, and its file data keeps the console's offsets, names and order (sonarlib's `docs/porting-model.md`, Import).

## The archives

Converted files are converted in place, at their offsets and sizes.

- **`forest_1st.arc`:**
  - converted:
    - `fgdata.bin`, Town's item layers, through `ac.schema`'s `FgData`;
    - `string_data_table.bin`, its `u32` words swapped;
    - `pallet_boy.bin`, its palettes swapped;
    - `face_boy.bin`, its palettes swapped and its texels kept;
  - not yet converted: the mail, `ps`, `psz`, `select`, `super` and `superz` tables.
- **`forest_2nd.arc`:**
  - converted: `message_data_table.bin`, its `u32` words swapped;
  - not yet converted: `fgnpcdata.bin`, `my_original.bin`, `needlework.bin`, the room floor and wall, the stock and island schedules, and the `.bti` images.
- **`famicom.arc`:** none of its contents is converted yet.

The other files on the first frame's path stay as the console's bytes, as the game reads them: text and byte strings (`string_data.bin`, `npc_name_str_table.bin`, `message_data.bin`) and texels (`tex_boy.bin`).

## The program

`ac_port` (`adapter/main.c`) runs the game on sonarlib. By default it runs headless, in deterministic mode, from boot to play's first frame (sonarlib's `docs/roadmap.md`, M6 steps 6f, 6g and 6h); with `--window` it shows the game in a window in real time (step 6k) and plays its sound (step 6l). `--goldens` and `--measure` run headless on the GPU sink with no window (steps 6n and 7b). Each frame runs from `JW_BeginFrame` to `JW_EndFrame`: the GX the game writes is recorded into it by sonarlib's GX front end and emu64's tasks by sonarlib's `kits/emu64`, and it is submitted to the render module. Headless that is the null sink, and the run opens no window and reads no device; the game's sound is mixed on sonarlib's null audio sink, counted and not heard.

### The threads

The process's main thread starts sonarlib, the render module, emu64's kit, fs and os. It then creates and starts a game thread named `main` at priority 0, which starts the runtime and calls the game's `main`, so the game's main thread is the runtime's tick thread. `OSInit` binds it as the SDK's default thread. Headless, the process's main thread joins it and the run ends with `_Exit`. Under `--window` the process's main thread stays outside the game's threads and presents (below). In normal mode, while the tick thread waits in os, sonarlib's idle driver fires each retrace on time.

### The run

`ac_port` mounts the package as `game:`, starts sonarlib's os and runtime in deterministic mode at NTSC's video rate, from a fixed calendar (2001-12-14 12:00:00), and calls the game's `main`. When the game has submitted play's first frame it prints the frame line and exits 0:

```
ac_port: play tick <t> retrace <r> gx <bytes> dvd <reads> <bytes> hash <h> instances <n> compiles <n>
    holes <vertex>/<list> cpu <n> refused <n> views <n> invalid <n> problems <n> overflows <n> unowned <n>
    strays <n> gx-errors <n> unmapped <n> outside <n> nested <n> refused-frames <n> audio <n> loud <n> dsp <n>
    sound <h>
```

on one line: the ticks and retraces run, the GX bytes written outside display lists, the DVD reads and their bytes; the hash of the frame submitted last; from emu64's kit, the instances its last task recorded, its compiles, the vertex and list holes it replayed, its draws worked out on the CPU, its refusals and its views past the render module's 256; from the render module, its invalid frames, problems and overflows; the actors the glue drew without an owner, being outside main memory and every table of static actors ([The glue](#the-glue)); the kit's strays, the part markers outside every owner or out of range, the closes with nothing open and the owners and parts left open at a task's end; and from the GX glue, the hand-overs the front end failed, the pointers it could not map to a GX address, emu64's tasks with no frame open, the frame begins and ends out of order, and the frames the render module refused; then, from sonarlib's audio mix statistics (`sl_audio_get_mix_stats`), the output frames mixed, those with a sample at 1/256 of full scale or above, and the hash of every output sample; and the DSP glue's frames whose rendered channels hold a sample at 1/256 of full scale or above ([Boot sound](#boot-sound)). If `n` ticks pass first (3600 by default) it prints the same fields as `ac_port: limit …` and exits 1. Wrong arguments exit 2, and a failure before the game starts exits 1, a package from another importer version saying to import the disc again. `--reference` draws with the console's emu64 instead of the kit ([Measured runs](#measured-runs)), and names the line `reference`. `--validate` creates the kit with `SL_EMU64_VALIDATE`, so every use of a program hashes the ranges it read again, and a static list the game rewrote is compiled again rather than drawn stale; its line must match a plain run's. The test `ac_port_play` imports the disc and runs `ac_port` twice and once with `--validate`, and all three must print the same line, with instances, a hash, and no invalid frame, view past 256 or overflow, the GX glue's five counts all 0, audio frames above 0 and `dsp` above 0: the boot jingle sounded. Headless runs pace the mix by alarms in deterministic mode, so the audio fields match too. The title's music starts after play's first frame, so the loud frames here are the jingle's.

On `GAFE01_00` the three runs print, as the reference line:

```
ac_port: play tick 557 retrace 557 gx 546409 dvd 486 15148254 hash 7a808f02087f4c1c instances 48 compiles 1091 holes 3/1003 cpu 3 refused 0 views 0 invalid 0 problems 0 overflows 0 unowned 0 strays 0 gx-errors 0 unmapped 0 outside 0 nested 0 refused-frames 0 audio 296800 loud 187636 dsp 355 sound 4c7e56f94e94c11e
```

With the game's sound running (step 6l), play's first frame comes at tick 557, not step 6k's 712, so the ticks, GX bytes, DVD reads (`audiorom.img`'s among them), frame hash, compiles and list holes changed; the instances and the other counts did not. With the boot jingle (step 6m), only `loud` (0 before) and the sound hash changed, and `dsp` is new. With step 6n's edits the frame hash changed (`ad41ee3261159765` before), as the fader's quad now has its color in the console's order (`TColor.h`), and `unowned` is 6, not 5, as the NPCs now made, in buffers of the executable's, are outside main memory; nothing else changed. With sonarlib's step 9b, its part markers and the static actors' owners, the frame hash changed (`3331f638d12a9e75` before) and `unowned` is 0, not 6, as the NPCs' buffers and the structures' are now tables of static actors; `strays` is new, and nothing else changed.

### The window

`--window` starts os and the runtime in normal mode on the host's own clock and date (sonarlib's `docs/roadmap.md`, M6 steps 6k and 11a). The main thread sets up in this order:

1. sonarlib, then fs with the player's user directories: `user:` and `config:`, org `sonarlib` and app `animal-crossing`, on Linux `~/.local/share/sonarlib/animal-crossing` and `~/.config/sonarlib/animal-crossing`, or with `--user-dir <dir>` every one under `dir`, `config:` at `dir/config`. fs comes before the render module, so the GPU sink's pipeline list is `pipelines.slpc` in the cache directory, `~/.cache/sonarlib/animal-crossing` on Linux or `dir/cache` (sonarlib's `docs/rendering.md`, Pipeline list). The window run ends with `_Exit` and never reaches the render module's shutdown, so the list it keeps is the one the sink writes while it runs: once a list lacks a pipeline made and 120 frames in a row made none, about once a scene, on a thread of the sink's own. The next run's render module makes every pipeline the list names before the first frame;
2. the settings: it prints `ac_port: settings <host path>`, loads `config:settings.toml`, where no file is a first run, takes each `--set` over it for the session (`sl_settings_override`), adds a watch that marks the settings changed from whichever thread applies a change, and binds os's system settings (`sl_os_settings_bind`), so the game boots with the stored system settings. A file that does not load prints `ac_port: <host path>:<line>:<column>: <message>; it is left as it is` to stderr, or without the place when the error has none, and the run goes on at the defaults and never saves it;
3. the window system, then the render module on the GPU sink at 640x480, so its device shares the window system's video driver, then emu64's kit;
4. `game:`, the package; os; the game's sound on the host's audio device;
5. input, with the GameCube profile's default bindings: the keyboard on port 0, and each gamepad on the first free port as it arrives;
6. the window, `Animal Crossing`, 1280x960 and paced by VSync, then the binds that apply the settings in effect to it at once: its mode, pacing and limiter (`sl_window_settings_bind`), the volume (`sl_audio_settings_bind`) and the rumble strength (`sl_input_settings_bind`). Every setting a `--set` can name is registered then, so each must name one that holds its override (`sl_settings_is_overridden`): one naming a key nothing registers, or with a value its setting refused at registration, prints `ac_port: --set <assignment>: <why>` and the usage to stderr and exits 2;
7. the overlay (`sl_ui_init`), F1 its toggle, with an `Animal Crossing` page: a Controls section with the keys, the gamepads, F1, and F11 or Alt+Enter, and a Files section with the settings file's and the user directory's host paths;
8. the game thread.

Each pass the main thread then pumps the window, which hands each event to the fullscreen keys, then the overlay, then input, and updates input; begins the presented frame, which applies the video settings changed since the last; presents the latest submitted frame at the clock's alpha for the image's presentation time, the time `sl_window_frame_begin` gives (`sl_clock_alpha`), so the image blends the game's last two frames as the display's time falls between them, or at 1 when the window gives no time, as a stale one does; builds the overlay over it (`sl_ui_frame`) with the window's size and pixel density, the time since the last frame, the window and the frame resolved; and shows it, until the window is closed. Since sonarlib's step 9b each moving thing the title demo draws has an identity of its own and its jumps are marked, and the measured runs find no snap unexplained ([Measured runs](#measured-runs)); before it the window presented at 1. Each pass, when a setting changed since the last save and the overlay is hidden, it saves the file (`sl_settings_save_file`), which writes the stored values anew in tables, without the file's comments. The play line prints at play's first frame and the run goes on. `--ticks` applies only when given, and then ends the run once the runtime has begun that many ticks. At the end it saves them once more when a setting changed since the last save, unless the file did not load, a failure printing `ac_port: cannot save settings: <result>`. So a run that changes no setting leaves the file byte for byte as it was, comments too; `--set` changes none, as an override is for the session. Then it prints

```
ac_port: window ticks <t> frames <n> audio <n> loud <n> dsp <n> pacing <vsync|mailbox|immediate|limiter> limit <Hz> gain <gain> pipelines <n> warmed <n> lists <n>
```

with the ticks begun, the frames presented, the mix statistics and the DSP's sounding frames; the window's pacing and its limiter's rate in effect, from `sl_window_info`, in whole Hz, 0 for none; the audio device's gain (`sl_audio_output_gain`), (volume / 100)², to three decimals; and from `sl_render_stats`, the pipelines the GPU sink made, those of them it made from the pipeline list at the start, and the lists it wrote. It exits 0 if play's first frame was reached or the window was closed, else 1. The game's sound plays on the host's audio device, on the driver the environment names, else the platform's. `--validate` and `--diff` work with it as without. Every other run mounts no user directory, reads no settings file and registers no setting, and `--user-dir` or `--set` without `--window` exits 2, as does a malformed `--set` at once, naming it, or a bad one after the binds.

The test `ac_port_window` runs `ac_port --window` twice to play, then with bad assignments, on SDL's `offscreen` video driver through lavapipe's Vulkan, with SDL's `dummy` audio driver, so nothing opens on the desktop, with `--user-dir` under its own directory, which starts empty, so no player's file is read or written:

- **The settings round trip.** It writes `config/settings.toml` with a comment line, `sonar.video.pacing = "limiter"`, `sonar.video.frame_limit = 120`, `sonar.audio.volume = 50` and `port.later.kept = 1`, a key no code registers, then runs `--ticks 1800 --set sonar.video.frame_limit=90`. The run must exit 0 with the settings line naming the file, one play line and a window line with at least 1800 ticks, frames above 0, `loud` above 0 (by then the title's music plays), `dsp` above 0 (the boot jingle) and `pacing limiter limit 90 gain 0.250`.
- **Unchanged settings.** The round trip changes no setting, as an override is never a change, so afterwards the file must be byte for byte as written, its comment too.
- **The pipeline list.** After the round trip `cache/pipelines.slpc` must exist and its window line must have `lists` above 0. The broken file's run starts from it, so its window line must have `warmed` above 0.
- **A broken file.** It replaces the file with `sonar.audio.volume = fifty`, which does not parse, and runs `--ticks 120`. Its stderr must name the file at line 1, its window line must read `gain 1.000`, and the file must be byte for byte as written. Its exit status is not checked: 120 ticks reach no play.
- **Bad assignments.** With no settings file, `--ticks 60 --set sonar.audio.volum=30`, a key nothing registers, and `--ticks 60 --set sonar.audio.volume=500`, a value its setting refuses, must each exit 2 with stderr naming the assignment; `--set` and `--user-dir` without `--window` must each exit 2 with the usage.

It is skipped without a disc or lavapipe. In real time the play line differs between runs.

The GPU sink compiles pipelines on the game thread, and the GPU driver's shader compiler can be C++ (lavapipe's LLVM is): its allocations then reach the game's `operator new`. sonarlib runs every render call inside a host scope (`sl_mem_host_scope`), and `JKRHeap.cpp`'s rule sends those allocations to the host's allocator, never JKR's heaps.

### Sound

Every run starts the game's sound: `main.c` calls `ac_audio_init` after os starts, on sonarlib's device sink under `--window` and its null sink headless. Animal Crossing's music and sounds come from jaudio_NES's sequenced engine, the N64-style `Nas_*` engine, which mixes on the CPU (`rspsim.c`) into the blocks the AI glue (`adapter/dolphin_ai.c`) plays. The boot jingle is JAudio1's ([Boot sound](#boot-sound)), and the NES emulator's sound plays only in the NES games.

The engine reads `audiorom.img`: sequences, then banks, then waves, as the game's compiled tables (`game/audioheaders.c`) give them. It copies a bank into its cache and turns the bank's offsets into pointers in place (`Nas_BankOfsToAddr_Inner`, `__WaveTouch`), so the banks must be the host's structures. The importer (`importer/audio.cpp`) turns the game's tables into entries for sonarlib's `kits/jaudio` (`sonar/jaudio/nas_rom.h`), which lays every bank out again at the host's sizes, 32-byte aligned, with each structure at a multiple of 16 and its pointers as offsets in the bank; the sequence and wave regions stay the disc's bytes, the waves moved by what the banks grew. On `GAFE01_00` the banks grow from 0x67C80 to 0x8F300 bytes. `importer/audio.cpp` checks each kit structure against the game's own (`static_assert`), and gives the kit this build's limits: `NA_RELOCATED_BASE`, the caches' room below and at most 0x40000 of growth. `audiorom.tbl` holds the host image's table entries, host-order `u32`s after a version word and the two counts; `Nas_InitAudio` calls `ac_audio_rom_tables` (`dolphin_dvd.c`) to write them into `AudiobankHeaderStart` and `AudiodataHeaderStart` before it reads them, and a package without it stops the run with a message.

Sequences and envelopes stay the console's bytes: a sequence is a byte stream whose tables are found only by running it, and sequences hold envelopes too. So the engine reads their 16-bit values with `sl_load_be16` where it reads them: the sequence map (`system.c`, `track.c`), a sequence's `u16` operands (`track.c`), `melody.c`'s patch of a sequence's offsets (with `sl_store_be16`), and `Nas_EnvInit` (`effect.c`); `DEFAULT_ENV` (`audiotable.c`) is written in the console's byte order. Sample data is read a byte at a time. Loops and books come only from banks, and the kit converts them.

The pools at host sizes: `Jac_Start`'s ARAM is 0x850000 (`game64.c_inc`), a persistent bank's cache 0x6000 and the temporary cache 0x3400, two slots of 0x1A00 (`audioconst.c`), and jaudio's heap 0x22C00 larger in both places that size it (`neosthread.c`, `game64.c_inc`'s `audiomemory`), so the misc heap keeps its room. Measured at tick 3600 of a window run on `GAFE01_00`: misc 0x42E20 of 0x49400, emem 0x19B60 of 0x28000, the temporary bank cache 0x13E0 (one bank) of 0x3400, the persistent cache empty. The largest host banks: a permanent one 0x6AA0, a temporary one 0x19C0, a persistent one 0x5D20.

The test `ac_import_disc` checks both entries against the disc through the game's own tables and structures: the table's counts, each data entry where the growth puts it and the regions around the banks byte for byte, and in every bank each structure the engine relocates, followed from the host head by its pointers and compared field by field with the disc's, read big-endian.

### Boot sound

The jingle at boot (`BootSound`, `game64_dummy.c_inc`) is JAudio1's, the engine other GameCube games share: a sequence (`NIN_SEQ`), a bank (`NIN_BANK`, an IBNK) and a wave set (`NIN_WAVE`, a WSYS), which the executable keeps. The sequence is a byte stream and stays the console's. JAudio1 reads the bank and the wave set in place, as structures of 32-bit offsets it turns into pointers (`Bank_Test`, `Wave_Test`, with `PTconvert`) and integers in the console's order, so the build lays them out for the host. The decompilation's build extracts both (`config.yml`, `assets/jaudio/NIN_BANK.bin` and `NIN_WAVE.bin`), and `ac-boot-sound` (`importer/boot_sound.cpp`) lays each out with sonarlib's jaudio kit (`sonar/jaudio/jaudio1.h`) and writes `boot_sound.inc`, which `game64_dummy.c_inc` includes in place of the two arrays. Each structure is the host's, at a multiple of 16 in the console's order, with its relocated pointers as offsets in the image, which the engine's `PTconvert` turns into pointers; the others keep the console's value. The tool checks each kit structure against the game's own (`bx.h`, `heapctrl.h`, `static_assert`): `Vmap_` as the code reads it, a `u32` of the wave set's number and the wave's at 4 (`map[1]`, `oneshot.c`), where `bx.h`'s two `s16` fall at 2 and 4, and `Wave_`'s declared fields, after which the kit keeps the console's word at 0x28. The layout is the building machine's, like `ac-font`'s. On `GAFE01_00` the bank grows from 0x880 to 0xE40 bytes and the wave set from 0x240 to 0x2A0. `BootSound` then registers both as on the console, and the jingle plays on the DSP glue's channels (`adapter/dolphin_dsp.c`, on the kit's microcode), which count the frames in which a rendered sample reaches 1/256 of full scale: the proof's `dsp`.

On the jingle's path the engine reads a sound id's bytes as bank, program, key and velocity, so `noteon.c` and `jammain_2.c` store its value with `sl_store_be32` and `oneshot.c` loads it with `sl_load_be32`; `Jam_SeqmainNote` (`jammain_2.c`) reads a track's children and `_D5` by name rather than by console offsets; `EffecterInit_Perc` (`oneshot.c`) reads a percussion key map's two `Rand_` pointers by name; and `FORCE_RELEASE_TABLE` (`bankdrv.c`) is the six `s16` the console reads.

The test `ac_boot_sound` lays out the two extracts again, walks each image through the game's structures as `Bank_Test` and `Wave_Test` reach them, compares every field with the console's bytes, read big-endian, every relocated pointer with the place of the structure the console's offset names and every other pointer with the console's value, and checks that the build's `boot_sound.inc` is what the tool writes. It fails on an instrument or a voice, which the jingle has none of.

### The emu64 differential test

`--diff` draws every emu64 task twice from the same game state, through the kit and through the console's emu64 (`src/static/libforest/emu64/emu64.c`, built into `ac_port` as the reference), and compares the two (sonarlib's validation.md, Differential tests, and roadmap.md, M6 step 6j). The reference draws through the SDK's GX units into a second GX front end. Each of the two goes into a frame of its own with captures on, the kit's first, and the game's frame begins again after them for the task's fader and 2D, so a `--diff` tick submits four frames. Without the flag none of the reference runs and no frame is captured, unless `--reference` draws every frame with it ([Measured runs](#measured-runs)). A task whose frame or capture is refused, or in which the reference's palettes overflowed, counts as refused and is not compared. The kit's draws are labeled by the command that drew them, which the glue names as a field of the task's lists and its index (`polygon_opaque 412`), as `static` and its GX address, or as `unknown`. For each task that differs it prints, at most 16 times a run:

```
ac_port: diff tick <tick> triangle <t> vertex <v> field <field> index <i> kit <draw> <name> reference <draw>
```

and at the end, before the play line, the tasks compared, the triangles of the equal tasks summed over both captures, and the draws each skipped:

```
ac_port: diff tasks <n> equal <n> different <n> refused <n> triangles <n> skipped <n> <n>
```

The test `ac_port_diff` imports the disc and runs `ac_port --diff`, which must reach play's first frame and exit 0 with tasks compared, none different and none refused; it prints the differences, the diff line and the play line, and is skipped without a disc. The reference rewrites the display lists it runs as the console's emu64 does, so the kit recompiles three programs whose hash fails and the play line's compiles and hash differ from a plain run's; neither is compared. Since the game's sound (step 6l), play starts at tick 557 here too, so 357 tasks are compared; since step 6n's NPC buffers `unowned` was 6, and since sonarlib's step 9b it is 0 and `strays` is new and 0, with the hash and every other count unchanged. On `GAFE01_00`:

```
ac_port: diff tasks 357 equal 357 different 0 refused 0 triangles 3976 skipped 0 0
ac_port: play tick 557 retrace 557 gx 1696436 dvd 486 15148254 hash a8b722393cbeb168 instances 48 compiles 1094 holes 3/1003 cpu 3 refused 0 views 0 invalid 0 problems 0 overflows 0 unowned 0 strays 0 gx-errors 0 unmapped 0 outside 0 nested 0 refused-frames 0 audio 296800 loud 187636 dsp 355 sound 4c7e56f94e94c11e
```

### Goldens

`--goldens <scenes> <dir>` writes the frames a scenes file names, for comparison with Dolphin's (sonarlib's validation.md, Port goldens, and roadmap.md, M6 step 6n). The run is headless and deterministic, but on sonarlib's GPU sink with no window, and play's first frame does not end it. At each frame's begin the GX glue hands sonarlib's `sl_goldens_frame` the frame submitted before it, with its keys as they stand then, after that frame's tick, which is where Dolphin's MemoryWatcher reads them, and the part of the frame buffer the VI shows, which the VI glue passes to `kit_vi` from `VIConfigure` and `VIConfigurePan`. The process's main thread serves the captures and, once every scene is written, prints `ac_port: goldens scenes <n> frames <n>` and exits 0; at the tick limit it names the scenes left and exits 1.

The keys (`port/tests/goldens.scenes`), each read natively at the variable Dolphin reads at the decomp's symbol:

- `menu_frame`: initial_menu's `frame_count` (`800E2698`), through an accessor in `initial_menu.c`;
- `fader` and `fader_time`: `jc_fader`'s status and its ticks target `<< 16 |` its ticks run (`800E2608`, offsets 4 and 8), through an accessor in `jsyswrap.cpp`, 0 while there is no fader;
- `game_frame`: `game_class_p->frame_counter`, which Dolphin reaches through the OS module list's first module, foresta.rel, and its `.bss`;
- `play`: 1 while `game_class_p->exec` is `play_main`. Dolphin reads `exec` less foresta's text section's entry, `2bb0c7`, so the scenes give it as `ref:play=2bb0c7 port:play=1`.

The scenes are the Nintendo logo fading in (`logo_in_03` to `_31`), held (`logo_held`) and fading out (`logo_out_02` to `_31`), and play's title demo at its 40th to 80th frames (`play_40` to `play_80`). Dolphin's dumps trail the keys by two frame-buffer changes in the fade-in; from the fade-out on, a dump shows the port's frame two frames less by its keys, so those scenes give the reference's `fader_time` and `game_frame` two less than the port's. Play's earlier frames are black or within the tolerance of it, and its first 2 dumps have no keys.

`--qrand-seed <hex>` seeds `qrand` with that value at `init_rnd` instead of the clock (`sys_math.c`). Dolphin's clock at `init_rnd` includes its disc reads' time, so the title demo's player and falling petals match only with Dolphin's seed, which the test reads from Dolphin's trace (`__qrand_idum`'s first value after its start).

The test `ac_port_goldens` runs sonarlib's `sl-capture dolphin` on the disc with `AC_DOLPHIN`, Dolphin 2609's `dolphin-emu-nogui`, into a cache in the build tree (`goldens/dolphin`, given relative to `goldens/` as Dolphin's MemoryWatcher socket path must be short), then `ac-import`, `ac_port --goldens` with Dolphin's seed through SDL's offscreen driver and lavapipe, and `sl-capture compare`, printing every line; a scene fails when more than 1% of its pixels differ by more than 16, and its difference is written as `goldens/port/<scene>_diff.png`. It is skipped without the disc, Dolphin, lavapipe or sl-capture, or for a disc other than `GAFE01_00`. On `GAFE01_00` the logo's scenes differ in at most 0.35% of their pixels, at its edges and in its gradient, and play's in at most 0.44%, at edges and in the petals' blending. The test then captures the scenes again with `--reference` into `goldens/port-reference` and compares them with the same Dolphin frames: they pass within the same figures, differing from the kit's by at most 0.001% of the pixels.

### Measured runs

`--measure` times the title demo from play's first frame to the tick limit, frame by frame (sonarlib's validation.md, Performance budgets, Measured runs, and roadmap.md, M6 steps 7b, 9a and 9b). The run is headless and deterministic, as a plain run is, but on sonarlib's GPU sink with no window, and play's first frame does not end it. The process's main thread opens sonarlib's `sonar/measure.h` with the tick limit and the present rate, and serves. From the frame that holds play's first task, the GX glue's `ac_frame_end` calls `ac_measure_frame` after its submit, which hands the frame to `sl_measure_frame` and waits while the main thread presents it: once, at alpha 1, or with `--present-hz <n>` at each of a display's times at `n` Hz in the frame's tick, at the clock's alpha. At the limit, on the game thread, it stops the measuring and prints three lines, then exits 0:

```
ac_port: measure <kit|reference> frames <n> ticks <n> game <median> <max> <total> prepare <median> <max> <total>
    present <median> <max> <total> gpu <median> <max> <total> draws <n> calls <n> pipelines <n> buffers <n> textures <n>
    storage <n> pushes <n>
ac_port: presents <hz> count <n> held <n> late <n> dropped <n> backward <n> pairs <n> unexplained <n> none <n>
    unmatched <n> explicit <n> forget <n> cut <n> all <n> duplicate <n> rotation <n> distance <n> gap <n> auto-cut <n>
ac_port: measured tick <t> ...
```

the first two each on one line. The measure line has the frames presented and the ticks from the first measured frame to the last, which the game's total covers; the game's time a frame from one hand-over to the next, the render worker's preparation, the present's own work over the frame's presents and the GPU's work for them, waited for after each present, the last while the game makes the next, in nanoseconds; and what the presents drew, from `sl_render_stats`. The presents line has `--present-hz`'s rate, or 0; the presents; those held, shown at alpha 1 when asked for less; the display times passed late, with no present; the frames dropped, handed over with no display time left in their tick; the presents going backward, of an earlier frame than the present before or of the same one at a lower alpha; the pairs, frames presented with one before to blend from; the snaps only sonarlib's checks found, rotation, distance, duplicate and auto-cut (sonarlib's time-and-interpolation.md, Snapping); and the transforms snapped by cause at each pair's first present, in `sl_snap_cause`'s order. The third has the play line's fields at the limit. When the limit comes before play's first frame it prints the `limit` line and exits 1. `--measure` with `--window`, `--goldens` or `--diff` exits 2, and so does `--present-hz` without `--measure` or with `--reference`.

`--reference` draws every frame with the console's emu64 in place of the kit, as `--diff`'s reference does, but into the game's own frame through the main GX front end, every task, keeping the GX state it leaves as on the console. The kit is not created and the glue's calls on it are skipped; the owner and part markers are not written and nothing is marked, so `unowned`, `strays` and the kit's fields read 0. `OSInit` carves the reference's buffers from the arena as under `--diff`, so the heaps start 0x14400 bytes later and the frame hash is the reference's own. Its lines end with ` palette-overflows <n>`, emu64's palettes refused past its buffer. With `--measure` its measure line reads `reference`, at no present rate only, and with `--goldens` its frames are captured; with `--window`, `--diff` or `--validate` it exits 2. On `GAFE01_00` its run to play's first frame prints:

```
ac_port: reference tick 557 retrace 557 gx 1696436 dvd 486 15148254 hash 356699db24314616 instances 0 compiles 0 holes 0/0 cpu 0 refused 0 views 0 invalid 0 problems 0 overflows 0 unowned 0 strays 0 gx-errors 0 unmapped 0 outside 0 nested 0 refused-frames 0 audio 296800 loud 187636 dsp 355 sound 4c7e56f94e94c11e palette-overflows 0
```

the game's fields the play line's.

The test `ac_port_measure` imports the disc and runs `ac_port` and `ac_port --reference` to play's first frame, whose game fields (tick, retrace, DVD, audio, loud, dsp and sound) must be the same, with no palette overflowed. Then `--measure --ticks 1200` twice and `--measure --reference --ticks 1200` twice, through SDL's offscreen driver and lavapipe: each must exit 0 with its three lines, the same in its two runs once the twelve times are taken out, with frames, ticks, draws and calls, each frame presented once with none held, late, dropped or going backward, and with nothing invalid, overflowed, refused, unowned or stray and the GX glue's five counts 0, the kit's with no unexplained snap. The kit's and the reference's measure the same frames and ticks with the same game fields. `--present-hz 144` without `--measure`, and with `--measure --reference`, must exit 2 with the usage. Then the kit's `--measure --ticks 1200` once with `--present-hz 144` and once with `--present-hz 360`: each must exit 0 with its three lines, with the `measured` line and the ticks of the kit's runs at no rate, its frames and its dropped ones adding up to their frames, more presents than frames, more at 360 Hz than at 144 Hz, none held or going backward and no unexplained snap; the two must drop the same frames and count the same pairs and snaps. It prints every line, and is skipped without the disc or lavapipe. Each measured run at no rate takes about 8 seconds on lavapipe, at 144 Hz about 18 and at 360 Hz about 46. On `GAFE01_00` they measure 643 frames over 642 ticks:

```
ac_port: measure kit frames 643 ticks 642 ... draws 118552 calls 85021 pipelines 13682 buffers 13663 textures 78510 storage 8106 pushes 0
ac_port: presents 0 count 643 held 0 late 0 dropped 0 backward 0 pairs 643 unexplained 0 none 226440 unmatched 348 explicit 1078 forget 14 cut 0 all 156 duplicate 0 rotation 0 distance 0 gap 0 auto-cut 0
ac_port: measure reference frames 643 ticks 642 ... draws 114125 calls 83654 pipelines 13682 buffers 4248 textures 78510 storage 4248 pushes 0
ac_port: presents 0 count 643 held 0 late 0 dropped 0 backward 0 pairs 643 unexplained 0 none 0 unmatched 114040 explicit 0 forget 0 cut 0 all 0 duplicate 0 rotation 0 distance 0 gap 85 auto-cut 0
```

and the kit's at 144 Hz and 360 Hz present 1545 and 3862 times, dropping none, with the same pairs and snaps as at no rate:

```
ac_port: presents 144 count 1545 held 0 late 0 dropped 0 backward 0 pairs 643 unexplained 0 none 226440 unmatched 348 explicit 1078 forget 14 cut 0 all 156 duplicate 0 rotation 0 distance 0 gap 0 auto-cut 0
ac_port: presents 360 count 3862 held 0 late 0 dropped 0 backward 0 pairs 643 unexplained 0 none 226440 unmatched 348 explicit 1078 forget 14 cut 0 all 156 duplicate 0 rotation 0 distance 0 gap 0 auto-cut 0
```

The baseline that sonarlib's merging steps (M6 steps 7c to 7g) are measured against: `--measure` and `--measure --reference` with `--ticks 4100`, the title demo's town to just before its end at tick 4193, three runs of each, each figure the median of the three runs'. They ran on an Intel Core Ultra 7 265K (20 threads, the `powersave` governor) with an NVIDIA GeForce RTX 3090 on NVIDIA's driver 615.78.08, Vulkan 1.4, Linux 7.2, the port's RelWithDebInfo build, through SDL's offscreen driver. Both measure 3539 frames over 3542 ticks.

| | game | prepare | present | CPU a tick (game) |
|---|---|---|---|---|
| the kit, median | 0.38 ms | 0.068 ms | 0.17 ms | 0.48 ms |
| the kit, maximum | 22.1 ms | 5.7 ms | 8.5 ms | |
| the reference, median | 1.94 ms | 0.061 ms | 0.17 ms | 2.06 ms |
| the reference, maximum | 13.2 ms | 0.70 ms | 3.4 ms | |

The CPU a tick is the game's total over the ticks. Each count is the same in every run; a present averages:

| | draws | draw calls | pipelines | buffers | textures | storage | pushes |
|---|---|---|---|---|---|---|---|
| the kit | 161.5 | 161.5 | 17.1 | 163.8 | 108.5 | 5.9 | 487.5 |
| the reference | 150.2 | 150.2 | 16.9 | 7.7 | 108.5 | 0 | 239.0 |

The kit binds a vertex buffer for nearly every draw, the reference about 8 a present, and the kit's game time a frame is a fifth of the reference's, which runs the console's emu64 on the CPU every frame. By tick 4100 the reference has written 883 MB of GX outside display lists, the kit 5.7 MB.

After sonarlib's step 7c, in which retained meshes share device buffers by vertex stride and index size, the same runs on the same host, with a load average of 7 to 11 from other work:

| | game | prepare | present | CPU a tick (game) |
|---|---|---|---|---|
| the kit, median | 0.41 ms | 0.068 ms | 0.14 ms | 0.51 ms |
| the kit, maximum | 23.9 ms | 7.4 ms | 4.6 ms | |
| the reference, median | 2.00 ms | 0.062 ms | 0.18 ms | 2.13 ms |
| the reference, maximum | 13.0 ms | 0.33 ms | 3.2 ms | |

Only the kit's buffer bindings changed, from 163.8 a present to 27.2; the reference's stay 7.7, and every other count is the same. On lavapipe the measured test's kit run binds 13,663 buffers, against 127,981 before. The kit's present fell from 0.17 to 0.14 ms, while the reference's, which binds as before, rose to 0.18 ms. Both games' times rose with the host's load, the kit's by 0.03 ms and the reference's by 0.06 ms; step 7c changed nothing the game thread runs a frame.

After sonarlib's step 7d, in which the GPU sink reads every draw's values from one storage buffer a present instead of uniform pushes, the same runs on the same host, with a load average of 10 to 15 from other work:

| | game | prepare | present | CPU a tick (game) |
|---|---|---|---|---|
| the kit, median | 0.37 ms | 0.090 ms | 0.14 ms | 0.44 ms |
| the kit, maximum | 24.1 ms | 9.2 ms | 10.8 ms | |
| the reference, median | 1.78 ms | 0.049 ms | 0.15 ms | 1.88 ms |
| the reference, maximum | 13.0 ms | 1.7 ms | 2.9 ms | |

Only the storage bindings and pushes changed: the kit binds 13.7 storage buffers a present, was 5.9, and the reference 7.7, was 0, the records to both stages of each view, and neither pushes, against 487.5 and 239.0 before. Every other count is the same. On lavapipe the measured test's kit run pushes nothing, against 357,806 before. The kit's present stayed 0.14 ms and the reference's was 0.15 ms, against 0.18 after step 7c. The kit's prepare median rose from 0.068 to 0.090 ms and the reference's fell from 0.062 to 0.049 ms, as in an earlier run of the step; step 7e found the kit's cause, below.

After sonarlib's step 7e, in which the GPU sink lets at most three presents be unfinished and a measured run waits for the GPU after each present, the same runs on the same host, with a load average of 6 to 7 from other work:

| | game | prepare | present | gpu | CPU a tick (game) |
|---|---|---|---|---|---|
| the kit, median | 0.46 ms | 0.072 ms | 0.14 ms | 0.88 ms | 0.53 ms |
| the kit, maximum | 24.2 ms | 3.2 ms | 4.8 ms | 4.1 ms | |
| the reference, median | 1.83 ms | 0.048 ms | 0.14 ms | 0.67 ms | 1.94 ms |
| the reference, maximum | 11.8 ms | 0.44 ms | 3.7 ms | 3.2 ms | |

Every count is the same as after step 7d, and no present waited in `sl_render_present`. The GPU time is the frame's from its present's submit to the end of the wait, with nothing else on the GPU: the kit's, 0.88 ms, is about a third more than the reference's, 0.67 ms, for 8% more draws. Step 7f, the combiner's cost on the GPU, is measured by it.

The kit's preparation rose after step 7d because nothing bounded the presents the GPU had not finished in a run without a window, and the runs present faster than the GPU draws: the command buffers SDL_GPU had submitted and not yet cleaned up grew to about 300 after step 7d, against 100 to 150 after step 7c, the GPU drawing each present about a tenth slower after 7d. Each submit on SDL_GPU's Vulkan backend checks the fence of every one of them, and the worker submitted twice a frame. With step 7e's bound and the worker's one submit a frame, the kit's preparation median is 0.062 to 0.066 ms in runs without the wait for the GPU, against 0.088 to 0.092 ms after step 7d.

Waiting for the GPU after each present raises the CPU's times. In four runs of the kit with the wait, each followed by one without, the game's median was 0.45 to 0.47 ms against 0.39 to 0.41 ms, the preparation's 0.071 to 0.079 ms against 0.062 to 0.066 ms, and the present's 0.134 to 0.145 ms against 0.119 to 0.124 ms. The main thread sleeps in the wait, using less of the CPU than without it, so the rise is most likely the host's power management, its cores under the `powersave` governor running slower after idling. The runs from step 7e on compare with each other, and with those before only through such runs in turn. The reference's preparation, 0.048 ms with the wait, is as after step 7d; its fall after 7d, from 0.062 ms, stays unexplained.

After sonarlib's step 7f, in which the combiner's fragment shader indexes its arrays only with constants, the same runs on the same host, with a load average of 5 to 7 from other work:

| | game | prepare | present | gpu | CPU a tick (game) |
|---|---|---|---|---|---|
| the kit, median | 0.39 ms | 0.063 ms | 0.11 ms | 0.29 ms | 0.48 ms |
| the kit, maximum | 20.3 ms | 2.9 ms | 3.1 ms | 2.8 ms | |
| the reference, median | 1.81 ms | 0.052 ms | 0.15 ms | 0.29 ms | 1.91 ms |
| the reference, maximum | 12.8 ms | 0.98 ms | 3.4 ms | 3.2 ms | |

Every count is the same as after step 7e. A frame took the GPU a median of 0.29 ms in both runs, against 0.88 ms in the kit's and 0.67 ms in the reference's after step 7e. The kit's CPU times fell with it: the game's median from 0.46 to 0.39 ms, the preparation's from 0.072 to 0.063 ms and the present's from 0.14 to 0.11 ms. The kit's game now takes longer than the GPU's frame, so its threads spend less time waiting for the GPU, which most likely raised those times after step 7e, as above. The reference's game took longer than the GPU's frame before as well, and its times are about as they were.

After sonarlib's step 7g, in which consecutive calls that bind nothing new are drawn by one indirect call, the same runs on the same host, with a load average of 5 to 7 from other work:

| | game | prepare | present | gpu | CPU a tick (game) |
|---|---|---|---|---|---|
| the kit, median | 0.38 ms | 0.061 ms | 0.11 ms | 0.28 ms | 0.46 ms |
| the kit, maximum | 21.5 ms | 3.1 ms | 3.4 ms | 2.8 ms | |
| the reference, median | 1.83 ms | 0.053 ms | 0.15 ms | 0.28 ms | 1.92 ms |
| the reference, maximum | 12.8 ms | 0.95 ms | 3.8 ms | 2.8 ms | |

Only the draw calls changed: the kit's runs encode 119.6 a present, were 161.5, and the reference's 111.9, were 150.2, about a quarter fewer each. Every other count is the same as after step 7f. On lavapipe the measured test's kit run encodes 85,021 calls, against 118,552 before, and the reference's 83,654, against 114,125. These runs were taken in turn with three of the build before the step, which measured the kit's present at 0.113 ms, its preparation at 0.064 ms and a frame on the GPU at 0.278 ms, against 0.112, 0.061 and 0.284 ms after it, and the reference's GPU at 0.270 ms, against 0.280: no change beyond the runs' spread, as expected of the 42 and 38 calls a present saved, which come to about 0.3 µs at the 8 to 9 ns a call `gpu_render_budget` measures (sonarlib's rendering.md, Cost). The reference's present rose from 0.141 to 0.148 ms, the build before the step's runs 0.139 to 0.146 ms and these 0.144 to 0.149 ms, as it did in the step's prototype, from 0.139 to 0.147 ms, while the kit's did not; its cause is not found.

After sonarlib's step 7h, in which new textures and meshes are staged in memory and copied at the start of the next present's command buffer through one kept transfer buffer, the same runs on the same host, with a load average of 4 to 5 from other work:

| | game | prepare | present | gpu | CPU a tick (game) |
|---|---|---|---|---|---|
| the kit, median | 0.41 ms | 0.064 ms | 0.10 ms | 0.18 ms | 0.49 ms |
| the kit, maximum | 3.1 ms | 2.7 ms | 4.7 ms | 2.2 ms | |
| the reference, median | 1.78 ms | 0.056 ms | 0.22 ms | 0.31 ms | 1.90 ms |
| the reference, maximum | 9.8 ms | 0.26 ms | 3.1 ms | 1.6 ms | |

Every count is the same as after step 7g. These runs were taken in turn with three of the build before the step. The kit's longest tick took the game 3.0 to 3.6 ms, against 18.7 to 23.3 ms before: the ticks that load a scene no longer make a transfer buffer and submit a copy for each texture and mesh they make. Its medians stayed within the runs' spread: the game's 0.41 to 0.42 ms, against 0.39 to 0.45 ms, the present's 0.100 to 0.104 ms, against 0.098 to 0.108 ms, and a frame on the GPU 0.177 to 0.181 ms, against 0.171 to 0.182 ms. Its longest present took 3.0 to 5.8 ms, against 2.7 to 3.0 ms; the present after a load now records the load's copies, but which presents those were is not looked into. The reference's front end makes its meshes every frame, so every present now copies them, about 526 KB: its game's median fell from 2.02 to 2.08 ms to 1.76 to 1.80 ms, and its longest tick from 13.1 to 13.3 ms to 9.8 to 10.4 ms, while its present's median rose from 0.170 to 0.184 ms to 0.216 to 0.237 ms, and a frame on the GPU from 0.209 to 0.230 ms to 0.304 to 0.307 ms, timed now with the copies. By their medians, its game, present and GPU together take 0.13 ms less a tick. A frame takes the GPU less time in both builds than in the runs after step 7g: the build before this step, the same library as those runs', measured the kit's at 0.171 to 0.182 ms, against 0.27 to 0.29 ms then. The host's GPU ran the same work faster, and why is not looked into.

After sonarlib's step 9a, in which a frame's time is its submit's and a measured run can present at a display's rate, the kit's runs on the same host at no present rate, at 144 Hz and at 360 Hz, three of each taken in turn, with a load average of about 5 from other work. Each measures 3539 frames over 3542 ticks with the same `measured` line, and takes about 3 seconds at no rate, 4 to 5 at 144 Hz and 10 at 360 Hz:

| | game | prepare | present | gpu | CPU a tick (game) |
|---|---|---|---|---|---|
| no present rate, median | 0.38 ms | 0.056 ms | 0.091 ms | 0.17 ms | 0.44 ms |
| no present rate, maximum | 3.1 ms | 2.8 ms | 3.4 ms | 1.9 ms | |
| 144 Hz, median | 0.45 ms | 0.067 ms | 0.23 ms | 0.44 ms | 0.52 ms |
| 144 Hz, maximum | 2.9 ms | 2.7 ms | 2.9 ms | 3.4 ms | |
| 360 Hz, median | 0.65 ms | 0.079 ms | 0.57 ms | 1.09 ms | 0.72 ms |
| 360 Hz, maximum | 5.7 ms | 2.8 ms | 6.0 ms | 4.2 ms | |

The present and gpu times are a frame's over its presents. Each present's counts are the same at every rate, as after step 7h: 161.5 draws, 119.6 draw calls, 17.1 pipelines, 27.2 buffers, 108.5 textures, 13.7 storage buffers and no pushes. By the totals over the presents, a present's own work averages 0.12 ms and the GPU's 0.22 ms at every rate. The presents lines are the same in each rate's three runs:

```
ac_port: presents 0 count 3539 held 0 late 0 dropped 0 backward 0 pairs 3539 unexplained 13678 none 807889 unmatched 212624 explicit 0 forget 0 cut 0 all 0 duplicate 0 rotation 3672 distance 0 gap 147 auto-cut 10006
ac_port: presents 144 count 8503 held 0 late 9 dropped 0 backward 0 pairs 3539 unexplained 13678 none 807889 unmatched 212624 explicit 0 forget 0 cut 0 all 0 duplicate 0 rotation 3672 distance 0 gap 147 auto-cut 10006
ac_port: presents 360 count 21256 held 0 late 24 dropped 0 backward 0 pairs 3539 unexplained 13678 none 807889 unmatched 212624 explicit 0 forget 0 cut 0 all 0 duplicate 0 rotation 3672 distance 0 gap 147 auto-cut 10006
```

No present was held, against 14,144 of 14,156 in a prototype presenting each frame four times with the time stamped at the frame's begin (sonarlib's roadmap.md, Steps 9a to 9f). A frame is presented 2.40 times at 144 Hz and 6.01 at 360 Hz. The ticks are NTSC's, 2.4024 and 6.006 display times each, and the 3543 from the first measured frame's to the last's end have 8512 and 21280, the presents and the late ones. The late ones are the display times of the 4 ticks that hand over no frame, 3539 frames in 3543 ticks. No tick hands over two frames, so none is dropped. Every frame has one before it to blend from, the first measured included, so the pairs are the frames. The 13,678 unexplained snaps, 3672 by rotation and 10,006 by an automatic cut, are the kit's to explain; zero is not asked for here.

At no present rate every count is the same as after step 7h, and each median is at or below 7h's: the game's 0.34 to 0.43 ms, against 0.41 to 0.42 ms, the preparation's 0.055 to 0.057 ms, against 0.064 ms, the present's 0.088 to 0.094 ms, against 0.100 to 0.104 ms, and a frame on the GPU 0.164 to 0.171 ms, against 0.177 to 0.181 ms. These runs were not taken in turn with the build before the step, so the fall is not put down to it; the step adds the snaps' counting to each present, from the resolved frame, at no cost these runs show. At a display's rate the game's median rose with the rate, to 0.45 ms at 144 Hz and 0.65 ms at 360 Hz, and the preparation's with it, though the game thread does the same work at every rate. It sleeps while its frame is presented, by the medians about 0.5 ms at 144 Hz and 1.5 ms at 360 Hz, the presents and the GPU's work for all but the last, and its time leaves that out, so the rise is most likely the host's power management after the sleep, as after step 7e; it is not looked into.

After sonarlib's step 9b, in which each moving thing the title demo draws has an identity of its own and the game's jumps are marked ([The glue](#the-glue)), the kit's runs on the same host at no present rate, at 144 Hz and at 360 Hz, three of each taken in turn, with a load average of 6 to 10 from other work. Each measures 3539 frames over 3542 ticks with the same `measured` line, and takes about 3 seconds at no rate, 5 to 6 at 144 Hz and 10 at 360 Hz:

| | game | prepare | present | gpu | CPU a tick (game) |
|---|---|---|---|---|---|
| no present rate, median | 0.43 ms | 0.067 ms | 0.090 ms | 0.17 ms | 0.51 ms |
| no present rate, maximum | 3.1 ms | 2.6 ms | 3.8 ms | 1.6 ms | |
| 144 Hz, median | 0.49 ms | 0.067 ms | 0.23 ms | 0.42 ms | 0.57 ms |
| 144 Hz, maximum | 2.8 ms | 2.8 ms | 4.5 ms | 2.7 ms | |
| 360 Hz, median | 0.68 ms | 0.083 ms | 0.57 ms | 1.02 ms | 0.78 ms |
| 360 Hz, maximum | 3.7 ms | 2.9 ms | 6.5 ms | 4.3 ms | |

Each present's counts are the same as after step 9a, at every rate, and by the totals over the presents a present's own work averages 0.12 ms and the GPU's 0.21 to 0.23 ms. The `measured` line's game is 9a's, its ticks, GX bytes, DVD reads, instances, compiles, holes, audio and sound hash; its frame hash is `739de53249a5b859`, was `d5fd1f91b64c64c9`, and `unowned` is 0, was 9417, with `strays`, `refused` and `problems` 0. The presents lines are the same in each rate's three runs:

```
ac_port: presents 0 count 3539 held 0 late 0 dropped 0 backward 0 pairs 3539 unexplained 0 none 1039313 unmatched 1777 explicit 7191 forget 199 cut 0 all 156 duplicate 0 rotation 0 distance 0 gap 0 auto-cut 0
ac_port: presents 144 count 8503 held 0 late 9 dropped 0 backward 0 pairs 3539 unexplained 0 none 1039313 unmatched 1777 explicit 7191 forget 199 cut 0 all 156 duplicate 0 rotation 0 distance 0 gap 0 auto-cut 0
ac_port: presents 360 count 21256 held 0 late 24 dropped 0 backward 0 pairs 3539 unexplained 0 none 1039313 unmatched 1777 explicit 7191 forget 199 cut 0 all 156 duplicate 0 rotation 0 distance 0 gap 0 auto-cut 0
```

No snap is unexplained, against 9a's 13,678, 3672 by rotation and 10,006 by an automatic cut, and the late display times, 9 and 24, are 9a's, those of the 4 ticks that hand over no frame. The transforms that blend are 1,039,313, were 807,889, and those unmatched 1777, were 212,624: most of those had no id, the static actors' among them, which now have owners, and now match. The marks explain the rest: 7191 snap at the game's marks, the actors' first moves and the weather's wraps; 199 are forgotten, the parts whose slots were filled again and the actors gone; and 156 snap all, the first measured frame's, play's first, after `game_ct`'s snap for the scene (a run of its first 3 frames has the 156). 9a counted 147 of them as a gap, more than 250 ms after the frame before; a mark comes before the gap, and more of the frame's transforms have ids now. The times are not compared with 9a's: the host's load was higher, 6 to 10 against about 5, and the runs were not taken in turn with the build before the step.

The step's build also found:

- **The title logo's letters.** The title, part 2 of `ANIMAL_LOGO`'s font list (`aAL_title_draw`), slides its letters in in the logo's orthographic font view, view 5, along an easing curve, one joint moving up to 311 units a tick. With the snap distance in every view, 160 of their joints snapped by distance, and in the 32 frames where those were 10 to 18 of the view's 18 candidates the view cut automatically, snapping 1248 more of its joints, texture matrices and lights: 1408 unexplained in each run. The distance is in the world's units, which an orthographic view does not share, so sonarlib applies it only to transforms placing a draw in a perspective view (its time-and-interpolation.md, Snapping); an orthographic view's transforms snap by their turns, marks and identity, and its cuts count only those. The world's views lost nothing: they had no distance snap.
- **How deep the owners go.** An owner, then a part in it: the deepest any list goes, in the run to play's first frame and over 4100 ticks, is 2, so the kit's 16 levels are ample.

And it read three of the game's other actors in the source, which need no change here:

- **The NPCs of `aNPC_NPC2`.** Their actors come from `zelda_malloc`, the zelda arena in main memory (`ac_npc_ctrl.c_inc`'s clip takes it as `get_actor_area_proc` under `aNPC_NPC2`, and `aNPC_keep_actor_class` is only without it; `m_play.c`'s `zelda_InitArena` makes the arena in the game's memory), so their keys are offsets and no table is registered.
- **The police station's lost and found** (`bg_police_item`) draws its items by type, in the order of their units in a packed table (`bPI_draw_item_loop`), not a pool of slots; it is one of the pools sonarlib's step 9f takes up.
- **The post office's letters** (`bg_post_item`) are drawn at fixed places, the nth at `setX[n]` (`bPTI_actor_draw`), so a letter's ordinal is its place and needs no part.

#### The lists' room

The owner and part markers ([The glue](#the-glue)) are commands in the game's display lists, which the console's lists never had room for. Each of the nine lists `graph_setup_double_buffer` makes from `sys_dynamic` (`include/sys_dynamic.h`) is a two-head arena: the game's commands go from its head, and in POLY_OPA the matrices and vertices `GRAPH_ALLOC` takes go from its tail (`graph.h`). A marker pair is 2 commands around an owner or part in each list it draws in; one that drew nothing is taken back. The bg items' type-1 loops add 2 to each position they draw, and the weather adds 2 to each of up to 100 parts a frame. A head that crosses its tail overwrites the matrices before `graph_draw_finish`'s `THA_GA_isCrash` notices, after the frame.

A probe measured each list's peak, the head's and the tail's use in the same frame, over the run to play's first frame and the 4100-tick measured run. It sat in `graph_draw_finish` after its branches and before its checks, where each head and tail is at its frame's furthest, and is not kept. It ran once with the markers and once under `--reference`, which writes none and plays the same game. The uses are in the host's commands, 16 bytes each against the console's 8, and the percentages are of the console's sizes. Only POLY_OPA has a tail.

With the markers:

| List | Size | Play: head + tail | % | Measured: head + tail | % |
|---|---|---|---|---|---|
| POLY_OPA | 9952 | 306 + 531 | 8.4 | 575 + 876 | 14.6 |
| POLY_XLU | 2048 | 262 | 12.8 | 480 | 23.4 |
| OVERLAY | 1024 | 10 | 1.0 | 10 | 1.0 |
| WORK | 128 | 1 | 0.8 | 1 | 0.8 |
| FONT | 1792 | 52 | 2.9 | 101 | 5.6 |
| SHADOW | 512 | 53 | 10.4 | 94 | 18.4 |
| LIGHT | 256 | 8 | 3.1 | 8 | 3.1 |
| NEW0, the bg's opaque | 512 | 215 | 42.0 | 222 | 43.4 |
| NEW1, the bg's translucent | 256 | 127 | 49.6 | 133 | 52.0 |

Without them, under `--reference`:

| List | Size | Play: head + tail | % | Measured: head + tail | % |
|---|---|---|---|---|---|
| POLY_OPA | 9952 | 248 + 531 | 7.8 | 402 + 941 | 13.5 |
| POLY_XLU | 2048 | 202 | 9.9 | 314 | 15.3 |
| OVERLAY | 1024 | 8 | 0.8 | 8 | 0.8 |
| WORK | 128 | 1 | 0.8 | 1 | 0.8 |
| FONT | 1792 | 48 | 2.7 | 93 | 5.2 |
| SHADOW | 512 | 43 | 8.4 | 82 | 16.0 |
| LIGHT | 256 | 8 | 3.1 | 8 | 3.1 |
| NEW0, the bg's opaque | 512 | 157 | 30.7 | 162 | 31.6 |
| NEW1, the bg's translucent | 256 | 87 | 34.0 | 91 | 35.5 |

The markers make a head up to 1.5 times as long, at the most POLY_XLU's 480 against 314 and NEW1's 133 against 91, and WORK's and LIGHT's no longer; they leave the tail as it was. POLY_OPA's two peaks fall in different frames: its furthest heads are 575 and 437, and its furthest tails both 941. The bg items' positions and the weather's parts each draw at least a matrix and a list, so their markers at most double the head they wrap. Under `SONAR_PORT` each list is therefore twice the console's size, and `#else` keeps the console's. A frame that filled a console list fits it doubled while its markers are no more than its own commands. With the doubled lists, the fullest in these runs, NEW1, is at 26%. The probe read the same uses with them, and every line the runs print is as before. The tail's matrices and vertices keep their console sizes on the host, so POLY_OPA's 941 host commands of tail are 15,056 bytes, 1882 of the console's commands (18.9% of its list).

Nothing else depends on `dynamic_t`'s size:

- The magic words are its first and last fields, which `graph_setup_double_buffer` writes and `graph_draw_finish` checks wherever the struct ends.
- The only `sizeof` is that function's `bzero`, and only `graph.c`'s `CONSTRUCT_THA_GA` reads the size constants.
- `sys_dynamic` is the host's (`src/system/sys_dynamic.c`, in `ac_port`'s data), outside main memory and its arena, so neither moves.
- The debug mode's reads of POLY_OPA's free bytes (`m_debug_mode.c`) see more room, and no run here turns the debug mode on.
- The console's build keeps its `0x20410` bytes (`symbols.txt`).

### The seam audit

`seam-policy.toml` lists where the game may call GX: the SDK's own units (`sdk`), the glue (`glue`), and each other file that calls it today, as a transitional entry with a note. The test `ac_seam_audit` runs sonarlib's `sl-seam-audit` on the build's `compile_commands.json` with the policy and the repository as root; it reads only source files, not the disc, and passes with no unlisted call and no stale entry, printing its summary line, `seam GX transitional <n> calls <n> sdk <n> glue <n> unlisted <n> stale <n> skipped <n>`. Quoted includes of sources, such as the game's `.c_inc` files and the asset includes under `build/<version>`, are scanned as files of their own; files outside the repository, such as sonarlib's, are skipped.

### The glue

- **DVD** (`adapter/dolphin_dvd.c`): the package, listed once into a table sorted by path; an entry number is the index. Paths are relative to the disc's root, without regard to case. Reads complete at once. Directory listing and streaming fail.
- **OS** (`adapter/dolphin_os.c`): `OSInit` binds boot's main thread, the game thread `main`, as the SDK's at priority 16. It is the runtime's tick thread throughout: its retrace waits begin ticks, and while it waits in os the runtime moves time.
- **VI and PAD** (`adapter/dolphin_vi.c`, `dolphin_pad.c`) on sonarlib's `kit_vi` and `kit_pad`. Frames pace on `JUTVideo`'s retrace queue through the VI glue's post-retrace callback, as on the console; `setFrameRate` also sets the tick's length in retraces. `VIConfigure` sets the kit's pan to the whole frame buffer and `VIConfigurePan` to its rectangle, as the SDK's do.
- **GX** (`adapter/dolphin_gx.c`, `include/port/gx_sink.h`): the SDK's own GX units are compiled for the host, writing into a sink in the console's byte order. Outside a display list the bytes go to sonarlib's GX front end at each `GXFlush`, at the frame's end and before each emu64 task; display lists fill their buffers as on the console. `JW_BeginFrame` begins the frame and `JW_EndFrame` records the last bytes and submits it. Pointers written as 32-bit values become GX addresses: main memory at 0, then the executable's read-only and initialized data ranges, each 32-byte aligned after the one before, within the 512 MiB that the registers' 24-bit fields reach; NULL is 0, and a pointer in no region is 0 and counted as unmapped. `OSInit` lays the space out and logs its table. `GXFifo.c` is not compiled: the glue keeps the FIFO objects, and the GPU is always idle, its draw-done and draw-sync complete at once.
- **emu64** (`adapter/emu64_glue.c`, `include/port/emu64_glue.h`) on sonarlib's `kits/emu64`. `emu64_taskstart` records the task's lists with `sl_emu64_task` into the frame `JW_BeginFrame` opened, after the GX written before it, and the run ends when the frame holding play's first task is submitted. The task passes no clear: the front end's first record clears the frame. `emu64_texture_cache_data_entry_set` and `emu64_refresh` go to the kit; at start the program adds the executable's read-only and initialized data as static ranges (`sl_mem_image_data`). Owners are actors: `Actor_draw` writes an owner marker at the head of each of `GRAPH`'s eleven display lists, `(key << 4) | (list + 1)` for the actor's key, takes back each marker the actor drew nothing after and closes the other lists; `Actor_dt` forgets the actor's owners. An actor's key is its offset in main memory, or, for an actor in a table of static actors, 2^27 + table × 2^8 + slot: `ac_emu64_actor_table` registers a table by its base, stride and count, an actor inside element k being slot k, and registering it again replaces it. Table 0 is the NPCs' nine buffers (`aNPC_n_actor_cl_tbl`, `aNPC_keep_actor_class`), table 1 the structures' (`aSTR_actor_cl`, the structure clip's setup). `dolphin_os.c` asserts main memory is at most 2^27 bytes, so offsets and table keys stay apart, the keys below 2^28 and the owners below 2^32, a marker's w1. An actor with no key, outside main memory and every table, gets no owner and counts in `unowned`. Parts are owners inside an actor's owner, for what one actor draws many of: `ac_emu64_part_open` and `ac_emu64_part_close` write and close a part marker in each of the eleven lists, taking back those the part drew nothing after, and `ac_emu64_part_open_at` and `ac_emu64_part_close_at` do it in the one list a draw holds open; a part's ids are `sl_emu64_part(owner, part)`'s, and a part opened in a part is the enclosing part's. The weather's particles, the effects, the field's blocks, the bg items' positions and pools, and the title logo's layers each draw in a part ([The edits on the path](#the-edits-on-the-path)), so a slot keeps its ids when others fill and empty. The game's jumps are marked: `ac_emu64_forget_part` forgets a part of the actor's owners when what fills its slot is new, `ac_emu64_snap_part` and `ac_emu64_snap_actor` snap a part or the actor in the next task's frame, and `ac_emu64_snap_all` the whole frame; `ac_emu64_actor_made` records up to 256 actors made, by key, after each `ct_proc`, and `ac_emu64_snap_made`, before `Actor_info_call_actor`'s moves, snaps their owners, or the whole frame past 256. Forgetting an actor's owners does not forget its parts, which are owners of their own; the sites that reuse a slot forget its part as they fill it. Under `--reference` none of these writes or marks anything.
- **Inlines** (`adapter/game_inlines.c`): out-of-line copies of `guScaleF` and `guTranslateF`.
- **The system font** is converted by `ac-font` (`importer/font.cpp`) at build time into the host's byte order, and `jsystem_font.c` includes the result.

### The edits on the path

Each is a Portability edit, under `SONAR_PORT` with the original kept, unless it compiles the same on the console.

- `JKRHeap.cpp`: the global `operator new` and `delete`, in every form, sized and aligned too. Allocations inside sonarlib's host scope, from a thread the game does not own, or before JKR's heaps exist go to the host's allocator; `delete` picks by whether the address lies in main memory.
- `JKRExpHeap.cpp`: the `0x10`s that are a block header's size become `sizeof(CMemBlock)` (no guard: the same on the console).
- `jsyswrap.cpp`: `SystemHeapSize`'s `0xD0` becomes what the root heap takes at the host's sizes; `JW_BeginFrame` begins the port's frame and `JW_EndFrame` submits it.
- `JFWDisplay.h`: `setFrameRate` sets the runtime tick's length in retraces.
- `ac_animal_logo.c`, `m_player_tools.c_inc`: display lists the game writes into lose `const`.
- jaudio's spin loops (`dvdthread.c`, `os.c`) call `sl_os_thread_sleep(1)` in each pass, which waits for the next fixed point.
- jaudio's heap (`neosthread.c`, `game64.c_inc`'s `audiomemory`) is `0x22C00` larger, since its structures and bank caches are larger on the host and it divides the heap in console bytes.
- jaudio's sound engine (`system.c`, `track.c`, `effect.c`, `audiotable.c`, `audioconst.c`, `melody.c`, `game64.c_inc`): the host's bank tables, the console's byte order where sequences and envelopes are read, and the pools at host sizes ([Sound](#sound)).
- `game64_dummy.c_inc`: `NIN_BANK` and `NIN_WAVE` are the host's layout from `boot_sound.inc` ([Boot sound](#boot-sound)).
- JAudio1 on the jingle's path (`noteon.c`, `jammain_2.c`, `oneshot.c`, `bankdrv.c`): a sound id in the console's byte order, fields read by name, and `FORCE_RELEASE_TABLE`'s six `s16` ([Boot sound](#boot-sound)).
- `game64.c_inc`: `Na_CheckRestartReady` is TRUE while jaudio has run no audio frame.
- `c_keyframe.c`: its four C99 inline table getters are emitted out of line.
- `gbi.h`: `Mtx_t` is 16 `s32`s, 0x40 bytes as on the console, not 16 64-bit `long`s; the glue asserts it.
- `sys_matrix.c`: the five functions that write or read a matrix through 16-bit halves (`_MtxF_to_Mtx`, `Matrix_MtxtoMtxF`, `suMtxMakeTS`, `suMtxMakeSRT`, `suMtxMakeSRT_ZXY`) work on the halves in the console's order in a local array and move them to or from the 32-bit words, so on a little-endian host they agree with the game's word writers and the kit.
- `m_actor.c`: `Actor_draw` opens and closes the actor's owner markers, and `Actor_dt` forgets its owners. `Actor_ct` and `Actor_info_call_actor` record each actor made after its `ct_proc` (`ac_emu64_actor_made`), as its first move may jump, and `Actor_info_call_actor` snaps those made since its last call before its loop (`ac_emu64_snap_made`).
- `game.c`: `game_ct` snaps the whole next frame (`ac_emu64_snap_all`), as every scene change goes through it, so nothing blends with the scene before.
- `sys_dynamic.h`: the nine display lists `graph_setup_double_buffer` makes are twice the console's, for the owner and part markers ([The lists' room](#the-lists-room)).
- The weather (`ac_weather.c`): `aWeather_DrawWeatherPrv` draws each particle in part i of the weather's owners, i its slot (0 to 99), and `aWeather_GetWeatherPrv` forgets the part of the slot it claims. `ac_weather_sakura.c`, `ac_weather_leaf.c` and `ac_weather_snow.c`: their scroll checks snap the part of a particle they wrap from one edge of the view to the other.
- `ef_effect_control.c`: `eEC_actor_draw` draws each effect in part i of the control's owners, i its index, and `eEC_MakeEffect` forgets the part of the index it claims, the control actor being the one that holds `eEC_CLIP`.
- `bg_item_common.c_inc`, `bg_xmas_item_draw.c_inc`: the bg items draw each position in a part of its own. Both type-1 loops and `bXI_draw_loop_type1_xtree` open part `(block × 256 + ut) × 16 + j` around each position they draw, block being the draw table's `bz × 7 + bx`, ut the position's unit and j the draw list's index in `bg_item_common_draw_item_body`, 0 for the shadow, whose parts go into the sub-list the shadow list calls; `bg_item_common_draw_item` and `bg_item_common_draw_item_body` hand the loops the table, its block and j in file statics, and a position whose ut or j is out of range opens no part. The pools draw each element in part 2^23 + kind × 256 + slot, the kinds drop 0, fruit drop 1, pit 2, ten coin 3, fade 4 and hole 5, and `fade_entry`, `ten_coin_entryR`, `drop_entry_main`, `hole_effect_entry` and `pit_entry` forget the part of the slot they fill, through the clip's `bg_item_actorx`.
- `ac_field_draw.c`: `aFD_DrawBlock` draws each block's body in part `bz << 8 | bx` of the field's owners.
- `ac_animal_logo.c`: `aAL_actor_draw` draws the back in part 1, the title in part 2, the copyright and trademark in part 3 and "press start" in part 4.
- `ac_structure_clip.c_inc`: the structure clip's setup registers `aSTR_actor_cl` as table 1 of the glue's static actors ([The glue](#the-glue)).
- `m_msg_main.c_inc`, `ac_npc_post_girl.c_inc`: five buffers `mFont_UnintToString` writes past are as wide as what it writes: the hour's 15 bytes into 2, the minutes' 4 and the seconds' 5 into 2 (`mMsg_CopyHour`, `Min`, `Sec`), and the post office's loan balance, 7 bytes and a comma into 2, and account, 11 into 8. The console's overruns hit dead stack; the host's stack protector aborts, which ended the port at Rover's time prompt on the train.
- `m_address_ovl.c`: after rewriting `lat_atena_v`'s initialized vertices, it marks them written, so the programs that load them compile again.
- The SDK's GX units and headers: the sink's writers, addresses through the sink, declarations the host compile needs, and C for `GXTransform.c`'s paired-single assembly. `GXTexture.c` and `GXFrameBuf.c` write and read the addresses in all 24 bits of the registers' fields, and a TLUT load's below its 32 MiB; `GXInit.c`'s `__GXInitGX` sets its placeholder array bases to NULL.
- `TColor.h`: `toUInt32` and `set(u32)` build and read the color word in the console's order, `r` in its high byte, so the fader's quad, `JUTResFont`'s colors and `J2DGrafContext`'s opacity tests get the console's values. Without it the logo never fades.
- `m_event.h`, `m_event.c`, `m_event_schedule.c_inc`, `ac_event_manager.c`: the event dates' bytes (`mEv_MonthDay_u`, `mEv_schedule_date_c`, and the year, month, day and hour word, `mEv_ymdh_u`, moved into the header) are in reverse order and `mEv_schedule_date_u`'s `md` is `raw`'s high half, so `raw` and `md` are the console's words, month above day, and dates compare and mask as there. The schedule table keeps its source order in its own type, `mEv_copy_schedule_data` copies an entry field by field where the game copies it whole, and the two schedules it fills with month 100 set their dates by name. Without it a later month with an earlier day is the earlier date.
- `lb_rtc.c`: `lbRTC_IsEqualDate` subtracts the console's `year << 16 | month << 8 | day` words, built from the fields, so January 31 is before February 1 and `lbRTC_GetIntervalDays2` counts in the right direction.
- `m_collision_bg.h`, `m_collision_bg_info.c_inc`: `mCoBG_CONSOLE_RAW` builds the word the console's `raw` reads from `mCoBG_Collision_u`'s bit-fields, and the hole test subtracts those, so a unit that differs from a hole only in its attribute matches as on the console.
- `u64types.h`: `Color_RGBA8_u32`'s bytes are in reverse order, so `rgba` reads `r` in its high byte, as on the console.
- `m_debug_hayakawa.c`: the pad display builds each button's primitive color from `rgba_u`'s fields in the console's order, `r` in its high byte, where it passed `raw`.
- `xldtob.c`, `xprintf.c`: `_D0` is 3, the formatter's own case for a little-endian double, so `_Ldtob` reads the exponent from the last `u16`, and `_Printf` tests the sign by `ll < 0`. Without it `%f`, `%e` and `%g` print garbage and overrun their buffer.
- `audiostruct.h`: `phase`'s bit-fields are declared in reverse order, since the host's compilers allocate them from the least significant bit, so a stereo phase byte from a sequence or a port reads its type and strong bits as on the console.
- `audiostruct.h`: `note_`'s `_0A` flags are declared in reverse order for the same reason, so a sequence's word clearing `_0A.asU16`'s bits clears the flags it names on the console.
- `ac_npc_ctrl.c_inc`: the NPCs' nine actor buffers and the size test are `0xC00` bytes, as the NPC classes are `0xB68` to `0xBE8` at the host's sizes against the console's `0x9D0`; without it Porter at the station is never made. `aNPC_keep_actor_class` registers the nine buffers as table 0 of the glue's static actors ([The glue](#the-glue)).
- `initial_menu.c` and `jsyswrap.cpp`: accessors for the goldens' keys, `frame_count` and `jc_fader` ([Goldens](#goldens)).
- `sys_math.c`: `init_rnd` takes its seed through `ac_port_rnd_seed`, which is the clock unless `--qrand-seed` gives one.
- `adapter/names.h` renames libc64's `malloc`, `free` and `sprintf` to `ac_malloc`, `ac_free` and `ac_sprintf`, so the host's C library keeps its own.

### What runs silent or does nothing

- **Sound:** headless the mix goes to the null sink, unheard.
- **Drawing** (headless): emu64's frames go to the render module's null sink, which hashes them and draws nothing, so there is no picture.
- **Input** (headless): no input module runs, so every pad channel reads no controller.
- **Saves:** the memory cards stay in memory.
- **The Famicom emulator** declines to start.

### Open items

- **A made actor's parts.** A part's ids sit under `sl_emu64_part(owner, part)`, so `ac_emu64_forget_actor` and the snap of a made actor (`ac_emu64_snap_made`) reach the owner's ids but not its parts', as sonarlib's step 9b has it. An actor that draws in parts and is made at the main-memory offset of one that just died would blend its part n with the old actor's part n for one frame. None does today: every actor that draws in parts (the weather, the effect control, the bg items, the field and the title logo) is made once a scene, and `game_ct` snaps all. The fix belongs in the kit, which would keep the parts each owner opened and forget or snap them with it, in a later step.

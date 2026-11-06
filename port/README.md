# The port's notes

## The package

`ac-import <source> <package> <cache>` (`importer/`) writes the package the game reads from boot to Town's first frame, importer `ac`, version 1. Each entry is a whole disc file, at the game's own name for it:

| Entry | Recipe | Conversion |
|---|---|---|
| `COPYDATE` | `copy` 1 | none: text |
| `static.str` | `copy` 1 | none: strings |
| `forest_1st.arc` | `archive` 1 | the directory, and the files below |
| `forest_2nd.arc` | `archive` 1 | the directory, and the file below |
| `famicom.arc` | `archive` 1 | the directory only |

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

`ac_port` (`adapter/main.c`) runs the game on sonarlib headless, in deterministic mode, from boot to play's first frame (sonarlib's `docs/roadmap.md`, M6 step 6f). It opens no window and makes no sound.

### Building and running

Build the decompilation for the disc's revision first (`python configure.py`, then `ninja`): the port reads its `objdiff.json` and the asset includes under `build/<version>`. Then:

```sh
cmake -S port -B port/build -G Ninja -DSONARLIB_DIR=<sonarlib> -DAC_DISC=<disc>
cmake --build port/build
ctest --test-dir port/build
```

`AC_VERSION` picks the revision, `GAFE01_00` by default. `AC_DISC` names the player's disc, an image or a directory in `sl-disc`'s extracted layout; without it the tests that need the disc are skipped.

```sh
port/build/ac-import <disc> <package> <cache>
port/build/ac_port --package <package> [--ticks <n>]
```

`ac_port` mounts the package as `game:`, starts sonarlib's os and runtime in deterministic mode at NTSC's video rate, from a fixed calendar (2001-12-14 12:00:00), and calls the game's `main`. When the game reaches play's first frame it prints `ac_port: play tick <t> retrace <r> gx <bytes> dvd <reads> <bytes>` and exits 0. If `n` ticks pass first (3600 by default) it prints the same counts as `ac_port: limit …` and exits 1. Wrong arguments exit 2, and a failure before the game starts exits 1. The test `ac_port_play` imports the disc and runs `ac_port` twice, and both runs must print the same line.

### The glue

- **DVD** (`adapter/dolphin_dvd.c`): the package, listed once into a table sorted by path; an entry number is the index. Paths are relative to the disc's root, without regard to case. Reads complete at once. Directory listing and streaming fail.
- **OS** (`adapter/dolphin_os.c`): `OSInit` binds boot's main thread as a game thread at priority 16. The main thread is the runtime's tick thread throughout: its retrace waits begin ticks, and while it waits in os the runtime moves time.
- **VI and PAD** (`adapter/dolphin_vi.c`, `dolphin_pad.c`) on sonarlib's `kit_vi` and `kit_pad`. Frames pace on `JUTVideo`'s retrace queue through the VI glue's post-retrace callback, as on the console; `setFrameRate` also sets the tick's length in retraces.
- **GX** (`adapter/dolphin_gx.c`, `include/port/gx_sink.h`): the SDK's own GX units are compiled for the host, writing into a sink in the console's byte order. Outside a display list the bytes are counted and discarded at each `GXFlush`; display lists fill their buffers as on the console. Pointers written as 32-bit values become offsets into main memory. `GXFifo.c` is not compiled: the glue keeps the FIFO objects, and the GPU is always idle, its draw-done and draw-sync complete at once.
- **emu64** (`adapter/emu64_glue.c`): its entry points do nothing, and `emu64_taskstart` watches for play's first frame.
- **Inlines** (`adapter/game_inlines.c`): out-of-line copies of `guScaleF` and `guTranslateF`.
- **The system font** is converted by `ac-font` (`importer/font.cpp`) at build time into the host's byte order, and `jsystem_font.c` includes the result.

### The edits on the path

Each is a Portability edit, under `SONAR_PORT` with the original kept, unless it compiles the same on the console.

- `JKRHeap.cpp`: the global `operator new` and `delete`, in every form, sized and aligned too. Allocations from a thread the game does not own, or before JKR's heaps exist, go to the host's allocator; `delete` picks by whether the address lies in main memory.
- `JKRExpHeap.cpp`: the `0x10`s that are a block header's size become `sizeof(CMemBlock)` (no guard: the same on the console).
- `jsyswrap.cpp`: `SystemHeapSize`'s `0xD0` becomes what the root heap takes at the host's sizes.
- `JFWDisplay.h`: `setFrameRate` sets the runtime tick's length in retraces.
- `ac_animal_logo.c`, `m_player_tools.c_inc`: display lists the game writes into lose `const`.
- jaudio's spin loops (`dvdthread.c`, `os.c`) call `sl_os_thread_sleep(1)` in each pass, which waits for the next fixed point.
- jaudio's heap (`neosthread.c`, `game64.c_inc`'s `audiomemory`) is `0x20000` larger, since its structures are larger on the host and it divides the heap in console bytes.
- `game64_dummy.c_inc`: `BootSound` returns after `Wavegroup_Init`, since the boot sound is in the console's layout.
- `game64.c_inc`: `Na_CheckRestartReady` is TRUE while jaudio has run no audio frame.
- `c_keyframe.c`: its four C99 inline table getters are emitted out of line.
- The SDK's GX units and headers: the sink's writers, addresses through the sink, declarations the host compile needs, and C for `GXTransform.c`'s paired-single assembly.
- `adapter/names.h` renames libc64's `malloc`, `free` and `sprintf` to `ac_malloc`, `ac_free` and `ac_sprintf`, so the host's C library keeps its own.

### What runs silent or does nothing

- **Sound:** no audio runs. `AIInit` reports that the game will not be heard. jaudio's threads run with no audio frames, `audiorom.img` is not in the package, and the boot sound is skipped.
- **Drawing:** emu64 draws nothing and the GX sink discards what it gets, so there is no picture.
- **Input:** no input module runs, so every pad channel reads no controller.
- **Saves:** the memory cards stay in memory.
- **The Famicom emulator** declines to start.

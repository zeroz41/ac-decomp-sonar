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

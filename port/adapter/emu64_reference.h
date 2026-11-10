/* What the reference's two files share, and only they include (sonarlib docs/steps/m6-10a.md): emu64.c under the
   names port/CMakeLists.txt gives it, the buffers OSInit takes for it, and the reference's own GX front end
   (port/adapter/gx_reference.c). A file of ac_port that called one would fail ac_port's link. */
#ifndef AC_PORT_EMU64_REFERENCE_H
#define AC_PORT_EMU64_REFERENCE_H

#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sl_frame;
struct ucode_info_s;

/* The reference's buffers, which OSInit takes from the bottom of the arena under --diff and --reference: emu64.c's
   two texture caches and its palette buffer, each 32-byte aligned. */
#define AC_DIFF_TEXTURE_DATA_SIZE 0xC000 /* emu64.c's TEX_BUFFER_DATA_SIZE */
#define AC_DIFF_TEXTURE_BSS_SIZE 0x400   /* emu64.c's TEX_BUFFER_BSS_SIZE */
#define AC_DIFF_PALETTES_SIZE 0x8000     /* GX's largest palette */

/* emu64.c, the reference. */
void emu64_ref_set_buffers(void* data, void* bss, void* palettes, u32 palettes_size);
u32 emu64_ref_palette_overflows(void);
void emu64_ref_init(void);
void emu64_ref_cleanup(void);
void emu64_ref_taskstart(void* gfx);
void emu64_ref_set_ucode_info(int count, struct ucode_info_s* info);
void emu64_ref_set_first_ucode(void* ucode);
void emu64_ref_refresh(void);
void emu64_ref_texture_cache_data_entry_set(void* begin, void* end);
void emu64_ref_set_aflags(int idx, int value);

/* gx_reference.c's: the differential test's reference's GX bytes, kept apart. Begin hands the sink's bytes to the
   front end, copies its state into the reference's front end, created at the first begin, saves the SDK's GX state
   (*gx) and sends every later hand-over to the reference's front end. End hands the sink's bytes to it, records them
   into frame (sl_gxf_record, which clears; a refusal counts as the main front end's do), restores *gx and sends
   hand-overs to the front end again. The reference's front end keeps its own texture memory. */
void ac_gx_reference_begin(void);
void ac_gx_reference_end(struct sl_frame* frame);

#ifdef __cplusplus
}
#endif

#endif

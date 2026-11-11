/* The reference's hooks in ac_port, the player's program, which links no reference and no GX front end
   (port/reference.h, port/display_reference.h; sonarlib docs/steps/m6-10a.md and m6-10e.md): --diff, --diff-from and
   --reference are refused, and each hook does nothing. */
#include <port/display_reference.h>
#include <port/reference.h>

const char ac_program_name[] = "ac_port";
const char ac_reference_options[] = "";

BOOL ac_diff_enable(void) {
    return FALSE;
}

BOOL ac_diff_enabled(void) {
    return FALSE;
}

BOOL ac_reference_enable(void) {
    return FALSE;
}

BOOL ac_reference_enabled(void) {
    return FALSE;
}

BOOL ac_diff_from(uint64_t tick) {
    (void)tick;
    return FALSE;
}

void ac_reference_boot(void) {
}

void ac_reference_init(void) {
}

void ac_reference_set_ucode_info(int count, struct ucode_info_s* info) {
    (void)count;
    (void)info;
}

void ac_reference_set_first_ucode(void* ucode) {
    (void)ucode;
}

void ac_reference_refresh(void) {
}

void ac_reference_static_range(void* begin, void* end) {
    (void)begin;
    (void)end;
}

void ac_reference_set_aflags(int idx, int value) {
    (void)idx;
    (void)value;
}

BOOL ac_reference_task(struct sl_emu64* kit, Gfx* gfx) {
    (void)kit;
    (void)gfx;
    return FALSE;
}

void ac_reference_report(void) {
}

void ac_reference_submitted(void) {
}

BOOL ac_reference_gx(const u8* bytes, size_t size, u64 offset) {
    (void)bytes;
    (void)size;
    (void)offset;
    return FALSE;
}

void ac_reference_record(struct sl_frame* frame, BOOL first) {
    (void)frame;
    (void)first;
}

void ac_reference_sync(ac_reference_sync_call call) {
    (void)call;
}

void ac_reference_copy_display(void* xfb) {
    (void)xfb;
}

void ac_reference_prepare_copy(const ac_reference_copy* copy) {
    (void)copy;
}

void ac_reference_pre_gx(const GXRenderModeObj* mode, BOOL enable_alpha) {
    (void)mode;
    (void)enable_alpha;
}

void ac_reference_clear_efb(const GXRenderModeObj* mode, GXColor color, BOOL enable_alpha) {
    (void)mode;
    (void)color;
    (void)enable_alpha;
}

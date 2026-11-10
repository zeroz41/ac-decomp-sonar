/* The reference's hooks in ac_port, the player's program, which links no reference (port/reference.h; sonarlib
   docs/steps/m6-10a.md): --diff and --reference are refused, and each hook does nothing. */
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

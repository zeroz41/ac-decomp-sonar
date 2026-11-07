/* The Dolphin SDK's video functions the game calls, on sonarlib's VI kit (sonar/vi/vi.h): the retrace count, waits
   and callbacks on the runtime's retrace clock. The display's timing set-up has no counterpart, since frames go
   through render; VIConfigure and VIConfigurePan set only the kit's pan, the part of the frame buffer the VI shows,
   for the port's goldens (sonar/goldens.h), as the SDK's do (dolphin/vi/vi.c). */
/* vi.h names the render mode in VIConfigure's parameters only, and GXStruct.h includes it before defining the mode,
   so the mode is declared first, at file scope. */
struct _GXRenderModeObj;

#include "dolphin/gx/GXStruct.h"
#include "dolphin/vi.h"

#include <sonar/base/log.h>
#include <sonar/vi/vi.h>

#include <stdlib.h>

void VIInit(void) {
    const sl_result result = sl_vi_init(NULL);
    if (result != SL_OK && result != SL_ERROR_ALREADY_INITIALIZED) {
        SL_ERROR("vi", "VIInit: %s", sl_result_string(result));
        abort();
    }
}

void VIConfigure(const struct _GXRenderModeObj* rm) {
    sl_vi_configure_pan(0, 0, rm->fbWidth, rm->xfbHeight);
}

void VIConfigurePan(u16 x_origin, u16 y_origin, u16 width, u16 height) {
    sl_vi_configure_pan(x_origin, y_origin, width, height);
}

void VIFlush(void) {
    sl_vi_flush();
}

void VIWaitForRetrace(void) {
    sl_vi_wait_for_retrace();
}

u32 VIGetRetraceCount(void) {
    return sl_vi_retrace_count();
}

u32 VIGetDTVStatus(void) {
    return sl_vi_dtv_status();
}

u32 VIGetTvFormat(void) {
    return sl_vi_tv_format();
}

void VISetBlack(BOOL black) {
    sl_vi_set_black(black);
}

void VISetNextFrameBuffer(void* fb) {
    sl_vi_set_next_frame_buffer(fb);
}

VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback callback) {
    return sl_vi_set_pre_retrace_callback(callback);
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback callback) {
    return sl_vi_set_post_retrace_callback(callback);
}

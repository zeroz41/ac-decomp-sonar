/* The frame's seams (sonarlib docs/steps/m6-10e.md): JFWDisplay's (port/adapter/jsys_display.cpp) and JUTVideo's
   (port/adapter/jsys_video.cpp), the game's JFWDisplay.cpp and JUTVideo.cpp substituted. They call no GX: the frame's
   clear is recorded on sonarlib's kits/jsys into the frame the GX glue holds open (port/gx_sink.h, ac_frame_task), and
   the GX that only the reference's program needs goes to its hooks (port/reference.h, port/display_reference.h). Game
   threads, one at a time. */
#ifndef AC_PORT_JSYS_DISPLAY_H
#define AC_PORT_JSYS_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

struct sl_frame;

/* GXCopyDisp's, after each display copy, JFWDisplay's and JUTVideo's: the copy cleared the EFB to the copy's clear,
   the color and z prepareCopyDisp gave it last, GXInit's before the first. The clear is pending until the end of
   JFWDisplay::beginRender, which records it into the open frame at the EFB's format. */
void ac_jsys_display_copied(void);
/* The GX glue's (ac_frame_split), after a new frame begins: records the frame's clear, the last JFWDisplay recorded,
   into frame again, as the GX front end's first record cleared each frame. Nothing for a NULL frame, or before the
   first clear. */
void ac_jsys_display_reclear(struct sl_frame* frame);
/* GX's draw-done callback, JUTVideo::drawDoneCallback, which GX's draw done called at once: called once JUTVideo's
   constructor has run, and nothing before, when GX's callback was not yet set. */
void ac_jsys_video_draw_done(void);

#ifdef __cplusplus
}
#endif

#endif

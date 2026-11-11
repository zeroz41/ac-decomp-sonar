/* J2D's seam (port/adapter/jsys_j2d.cpp; sonarlib docs/steps/m6-10c.md): J2DGrafContext and J2DOrthoGraph, the
   game's J2DGrafContext.cpp and J2DOrthoGraph.cpp substituted, drawn on sonarlib's kits/jsys into the frame the GX
   glue holds open (port/gx_sink.h, ac_frame_task). JUTResFont's seam (port/adapter/jsys_font.cpp; m6-10d.md) draws
   its glyphs through the same object, under J2D's port, in runs the glue closes. Game thread. */
#ifndef AC_PORT_JSYS_J2D_H
#define AC_PORT_JSYS_J2D_H

#ifdef __cplusplus
extern "C" {
#endif

struct sl_jsys_j2d;
struct sl_mat34;

/* The J2D kit's object the seam draws with, made by the program (ac_port.c) for the game's 640 x 480 frame. Main
   thread, before the game runs. Until it is handed over J2D draws nothing; a port set before then is set again at
   the next setPort. */
void ac_jsys_j2d_start(struct sl_jsys_j2d* j2d);
/* The kit's object, for the font's seam; NULL until it is handed over, when the font draws nothing. */
struct sl_jsys_j2d* ac_jsys_j2d_kit(void);
/* The position matrix J2D last gave the kit, or would have loaded into GX: mPosMtx at each setPort, where setup2D
   loaded it, and at each draw. The identity until the first. The font draws under it, as it drew under GX's. */
void ac_jsys_j2d_position(struct sl_mat34* out);
/* The GX glue's (port/adapter/dolphin_gx.c): closes the font's open run, recording it into its frame, before the glue
   records anything else into the frame, so the text keeps its place among the frame's draws. Nothing before the
   object is handed over. */
void ac_jsys_j2d_flush(void);
/* The GX glue's, at the frame's end after its last record and before its submit, whether or not the frame was
   refused: closes the open run and starts the next frame of the kit's page cache. Nothing before the hand-over. */
void ac_jsys_j2d_end_frame(void);

#ifdef __cplusplus
}
#endif

#endif

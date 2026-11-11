/* J2D's seam (port/adapter/jsys_j2d.cpp; sonarlib docs/steps/m6-10c.md): J2DGrafContext and J2DOrthoGraph, the
   game's J2DGrafContext.cpp and J2DOrthoGraph.cpp substituted, drawn on sonarlib's kits/jsys into the frame the GX
   glue holds open (port/gx_sink.h, ac_frame_task). Game thread. */
#ifndef AC_PORT_JSYS_J2D_H
#define AC_PORT_JSYS_J2D_H

#ifdef __cplusplus
extern "C" {
#endif

struct sl_jsys_j2d;

/* The J2D kit's object the seam draws with, made by the program (ac_port.c) for the game's 640 x 480 frame. Main
   thread, before the game runs. Until it is handed over J2D draws nothing; a port set before then is set again at
   the next setPort. */
void ac_jsys_j2d_start(struct sl_jsys_j2d* j2d);

#ifdef __cplusplus
}
#endif

#endif

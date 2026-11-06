#ifndef _DOLPHIN_GXPRIV
#define _DOLPHIN_GXPRIV

#include "dolphin/gx/GXVert.h"

typedef struct GXLightObj_ {
  u32 padding[3];
  u32 color;
  float a0;
  float a1;
  float a2;
  float k0;
  float k1;
  float k2;
  float px;
  float py;
  float pz;
  float nx;
  float ny;
  float nz;
} GXLightObj_;

#define XF_LIGHT_BASE 0x0600
#define XF_LIGHT_SIZE 0x10

#define GX_FIFO_ADDR 0xCC008000

#ifdef SONAR_PORT /* sonarlib port: the write gather pipe is the port's sink (port/adapter/dolphin_gx.c) */
#include <port/gx_sink.h>
#define GX_WRITE_U8(v) (ac_gx_write_u8(v))
#define GX_WRITE_U32(v) (ac_gx_write_u32(v))
#else
#define GX_WRITE_U8(v) (GXWGFifo.u8 = v)
#define GX_WRITE_U32(v) (GXWGFifo.u32 = v)
#endif

typedef struct GXData {
  u16 cpSRreg;
  u16 cpCRreg;
} GXData;

extern GXData* __GXData;

#endif // _DOLPHIN_GXPRIV

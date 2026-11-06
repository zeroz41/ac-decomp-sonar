/* The out-of-line copy of each of the game's C99 inlines that a call the host compiler does not inline needs: a
   header's inline definition provides none, as msl_inlines.c says for MSL's math, so this unit declares each
   extern inline. */
#include "libultra/gu.h"

extern inline void guTranslateF(float m[4][4], float x, float y, float z);
extern inline void guScaleF(float mf[4][4], float x, float y, float z);

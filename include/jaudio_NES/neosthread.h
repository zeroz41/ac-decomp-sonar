#ifndef NEOSTHREAD_H
#define NEOSTHREAD_H

#include "types.h"
#include "jaudio_NES/audiostruct.h"

extern u32 Neos_Update(s16* dst);
extern void ImageLoaded(uaddr param);
extern BOOL Neos_CheckBoot(void);
extern void* neosproc(void* param);

#endif

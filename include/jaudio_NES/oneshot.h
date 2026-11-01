#ifndef _JAUDIO_ONESHOT_H
#define _JAUDIO_ONESHOT_H

#include "types.h"
#include "jaudio_NES/heapctrl.h"

typedef struct jc_ jc_;
typedef struct Inst_ Inst_;
typedef struct Perc_ Perc_;
typedef struct Pmap_ Pmap_;
typedef struct Osc_ Osc_;
typedef struct jcs_ jcs_;
typedef struct Vmap_ Vmap_;
typedef struct Wave_ Wave_;

/* A WaveID_ (bx.h), named CtrlWave_ because __Oneshot_GetLogicalChannel's mangled symbol uses that name */
typedef struct CtrlWave_ {
	u32 id;         // _00
	jaheap_ heap;   // _04, the wave's ARAM, from heap.startAddress at _0C
	u32 loadStatus; // _30
	Wave_* data;    // _34
} CtrlWave_;
typedef union SOUNDID_ SOUNDID_;

/**
 * @brief This is a "boxed" integer type to be passed by value.
 *
 * @note Size: 4. Why wasn't this just an enum... This compiler is not smart enough to optimize this.
 */
union SOUNDID_ {
	u32 value; // _00
	u8 bytes[4];
};

void Effecter_Overwrite_1ShotD(jc_*, Osc_*, u32);
Perc_* PercRead(u32, u32);
Inst_* InstRead(u32, u32);
Vmap_* VmapRead(Inst_*, u8, u8);
void Init_1shot(jcs_*, u32);
void Stop_1Shot(jc_*);
void Stop_1Shot_R(jc_*, u16);
void AllStop_1Shot(jcs_*);
void SetPitchTarget_1Shot(jc_*, f32, u32);
void SetKeyTarget_1Shot(jc_*, u8, u32);
void Gate_1Shot(jc_*, u8, u8, s32);
void UpdatePause_1Shot(jc_*, u8 a1);
void UpdatePanPower_1Shot(jc_*, f32, f32, f32, f32);
void FlushRelease_1Shot(jcs_*);
u32 One_CheckInstWave(SOUNDID_); // Return type unsure
jc_* Play_1shot(jcs_*, SOUNDID_, u32);
jc_* Play_1shot_Perc(jcs_*, SOUNDID_, u32);
jc_* Play_1shot_Osc(jcs_*, SOUNDID_, u32);

#endif

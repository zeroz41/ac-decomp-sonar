#include <dolphin.h>
#include <dolphin/mtx.h>

#define qr0 0

#ifdef SONAR_PORT
// The port's C fallbacks for the paired-single assembly, line for line with the same register names (sonarlib
// porting-model.md, Inline assembly and paired singles).
#include <sonar/legacy_math.h>

// The address d(r) of a load or store.
#define PS_EA(r, d) ((f32*)((char*)(r) + (d)))
#endif

#ifdef SONAR_PORT
void PSMTXMultVec(const Mtx44 m, const Vec* src, Vec* dst)
{
	sl_ps f0, f1, f2, f3, f4, f5, f8, f9, f10, f11;
	// Read before they are written; their ps1 lanes only reach ps1 lanes, which are not stored.
	sl_ps f6 = sl_ps_fill(0.0), f12 = sl_ps_fill(0.0);

	f0 = sl_ps_load(PS_EA(src, 0));
	f2 = sl_ps_load(PS_EA(m, 0));
	f1 = sl_ps_load1(PS_EA(src, 8));
	f4 = sl_ps_mul(f2, f0);
	f3 = sl_ps_load(PS_EA(m, 8));
	f5 = sl_ps_madd(f3, f1, f4);
	f8 = sl_ps_load(PS_EA(m, 16));
	f6 = sl_ps_sum0(f5, f6, f5);
	f9 = sl_ps_load(PS_EA(m, 24));
	f10 = sl_ps_mul(f8, f0);
	sl_ps_store1(PS_EA(dst, 0), f6);
	f11 = sl_ps_madd(f9, f1, f10);
	f2 = sl_ps_load(PS_EA(m, 32));
	f12 = sl_ps_sum0(f11, f12, f11);
	f3 = sl_ps_load(PS_EA(m, 40));
	f4 = sl_ps_mul(f2, f0);
	sl_ps_store1(PS_EA(dst, 4), f12);
	f5 = sl_ps_madd(f3, f1, f4);
	f6 = sl_ps_sum0(f5, f6, f5);
	sl_ps_store1(PS_EA(dst, 8), f6);
}
#else
asm void PSMTXMultVec(const register Mtx44 m, const register Vec* src, register Vec* dst)
{
#ifdef __MWERKS__ // clang-format off
	nofralloc
	psq_l f0, Vec.x(src), 0, qr0
	psq_l f2, 0(m), 0, qr0
	psq_l f1, Vec.z(src), 1, qr0
	ps_mul f4, f2, f0
	psq_l f3, 8(m), 0, qr0
	ps_madd f5, f3, f1, f4
	psq_l f8, 16(m), 0, qr0
	ps_sum0 f6, f5, f6, f5
	psq_l f9, 24(m), 0, qr0
	ps_mul f10, f8, f0
	psq_st f6, Vec.x(dst), 1, qr0
	ps_madd f11, f9, f1, f10
	psq_l f2, 32(m), 0, qr0
	ps_sum0 f12, f11, f12, f11
	psq_l f3, 40(m), 0, qr0
	ps_mul f4, f2, f0
	psq_st f12, Vec.y(dst), 1, qr0
	ps_madd f5, f3, f1, f4
	ps_sum0 f6, f5, f6, f5
	psq_st f6, Vec.z(dst), 1, qr0
	blr
#endif // clang-format on
}
#endif

#ifdef SONAR_PORT
void PSMTXMultVecArray(const GC_Mtx m, const Vec* srcBase, Vec* dstBase, u32 count)
{
	sl_ps f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13;
	u32 ctr;

	f13 = sl_ps_load(PS_EA(m, 0x0));
	f12 = sl_ps_load(PS_EA(m, 0x10));
	count = count - 0x1;
	f11 = sl_ps_load(PS_EA(m, 0x8));
	f0 = sl_ps_merge00(f13, f12);
	dstBase = (Vec*)PS_EA(dstBase, -0x4);
	f10 = sl_ps_load(PS_EA(m, 0x18));
	f1 = sl_ps_merge11(f13, f12);

	ctr = count;
	f4 = sl_ps_load(PS_EA(m, 0x20));
	f2 = sl_ps_merge00(f11, f10);
	f5 = sl_ps_load(PS_EA(m, 0x28));
	f3 = sl_ps_merge11(f11, f10);
	f6 = sl_ps_load(PS_EA(srcBase, 0));
	srcBase = (const Vec*)PS_EA(srcBase, 8);
	f7 = sl_ps_load1(PS_EA(srcBase, 0));
	f8 = sl_ps_madds0(f0, f6, f3);
	f9 = sl_ps_mul(f4, f6);
	f8 = sl_ps_madds1(f1, f6, f8);
	f10 = sl_ps_madd(f5, f7, f9);

	do {
		srcBase = (const Vec*)PS_EA(srcBase, 4);
		f6 = sl_ps_load(PS_EA(srcBase, 0));
		f12 = sl_ps_madds0(f2, f7, f8);
		srcBase = (const Vec*)PS_EA(srcBase, 8);
		f7 = sl_ps_load1(PS_EA(srcBase, 0));
		f13 = sl_ps_sum0(f10, f9, f10);
		f8 = sl_ps_madds0(f0, f6, f3);
		f9 = sl_ps_mul(f4, f6);
		dstBase = (Vec*)PS_EA(dstBase, 0x4);
		sl_ps_store(PS_EA(dstBase, 0), f12);
		f8 = sl_ps_madds1(f1, f6, f8);
		dstBase = (Vec*)PS_EA(dstBase, 0x8);
		sl_ps_store1(PS_EA(dstBase, 0), f13);
		f10 = sl_ps_madd(f5, f7, f9);
	} while (--ctr != 0);

	f12 = sl_ps_madds0(f2, f7, f8);
	f13 = sl_ps_sum0(f10, f9, f10);
	dstBase = (Vec*)PS_EA(dstBase, 0x4);
	sl_ps_store(PS_EA(dstBase, 0), f12);
	dstBase = (Vec*)PS_EA(dstBase, 0x8);
	sl_ps_store1(PS_EA(dstBase, 0), f13);
}
#else
asm void PSMTXMultVecArray(const register GC_Mtx m, const register Vec* srcBase,
                           register Vec* dstBase, register u32 count)
{
#ifdef __MWERKS__ // clang-format off
	psq_l      f13, 0x0(m), 0, qr0
	psq_l      f12, 0x10(m), 0, qr0
	subi       count, count, 0x1
	psq_l      f11, 0x8(m), 0, qr0
	ps_merge00 f0, f13, f12
	subi       dstBase, dstBase, 0x4
	psq_l      f10, 0x18(m), 0, qr0
	ps_merge11 f1, f13, f12

	mtctr count
	psq_l      f4, 0x20(m), 0, qr0
	ps_merge00 f2, f11, f10
	psq_l      f5, 0x28(m), 0, qr0
	ps_merge11 f3, f11, f10
	psq_l      f6, Vec.x(srcBase), 0, qr0
	psq_lu     f7, Vec.z(srcBase), 1, qr0
	ps_madds0  f8, f0, f6, f3
	ps_mul     f9, f4, f6
	ps_madds1  f8, f1, f6, f8
	ps_madd    f10, f5, f7, f9

loop:
	psq_lu    f6, Vec.y(srcBase), 0, qr0
	ps_madds0 f12, f2, f7, f8
	psq_lu    f7, Vec.z(srcBase), 1, qr0
	ps_sum0   f13, f10, f9, f10
	ps_madds0 f8, f0, f6, f3
	ps_mul    f9, f4, f6
	psq_stu   f12, 0x4(dstBase), 0, qr0
	ps_madds1 f8, f1, f6, f8
	psq_stu   f13, 0x8(dstBase), 1, qr0
	ps_madd   f10, f5, f7, f9
	bdnz loop

	ps_madds0 f12, f2, f7, f8
	ps_sum0   f13, f10, f9, f10
	psq_stu   f12, 0x4(dstBase), 0, qr0
	psq_stu   f13, 0x8(dstBase), 1, qr0
#endif // clang-format on
}
#endif

#ifdef SONAR_PORT
void PSMTXMultVecSR(const Mtx44 m, const Vec* src, Vec* dst)
{
	sl_ps f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13;

	f0 = sl_ps_load(PS_EA(m, 0x0));
	f6 = sl_ps_load(PS_EA(src, 0));
	f2 = sl_ps_load(PS_EA(m, 0x10));
	f8 = sl_ps_mul(f0, f6);
	f4 = sl_ps_load(PS_EA(m, 0x20));
	f10 = sl_ps_mul(f2, f6);
	f7 = sl_ps_load1(PS_EA(src, 8));
	f12 = sl_ps_mul(f4, f6);
	f3 = sl_ps_load(PS_EA(m, 0x18));
	f8 = sl_ps_sum0(f8, f8, f8);
	f5 = sl_ps_load(PS_EA(m, 0x28));
	f10 = sl_ps_sum0(f10, f10, f10);
	f1 = sl_ps_load(PS_EA(m, 0x8));
	f12 = sl_ps_sum0(f12, f12, f12);
	f9 = sl_ps_madd(f1, f7, f8);
	sl_ps_store1(PS_EA(dst, 0), f9);
	f11 = sl_ps_madd(f3, f7, f10);
	sl_ps_store1(PS_EA(dst, 4), f11);
	f13 = sl_ps_madd(f5, f7, f12);
	sl_ps_store1(PS_EA(dst, 8), f13);
}
#else
asm void PSMTXMultVecSR(const register Mtx44 m, const register Vec* src, register Vec* dst)
{
#ifdef __MWERKS__ // clang-format off
	psq_l   f0, 0x0(m), 0, qr0
	psq_l   f6, Vec.x(src), 0, qr0
	psq_l   f2, 0x10(m), 0, qr0
	ps_mul  f8, f0, f6
	psq_l   f4, 0x20(m), 0, qr0
	ps_mul  f10, f2, f6
	psq_l   f7, Vec.z(src), 1, qr0
	ps_mul  f12, f4, f6
	psq_l   f3, 0x18(m), 0, qr0
	ps_sum0 f8, f8, f8, f8
	psq_l   f5, 0x28(m), 0, qr0
	ps_sum0 f10, f10, f10, f10
	psq_l   f1, 0x8(m), 0, qr0
	ps_sum0 f12, f12, f12, f12
	ps_madd f9, f1, f7, f8
	psq_st  f9, Vec.x(dst), 1, qr0
	ps_madd f11, f3, f7, f10
	psq_st  f11, Vec.y(dst), 1, qr0
	ps_madd f13, f5, f7, f12
	psq_st  f13, Vec.z(dst), 1, qr0
#endif // clang-format on
}
#endif

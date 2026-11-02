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
void PSVECAdd(const Vec* a, const Vec* b, Vec* c)
{
	sl_ps f2, f3, f4, f5, f6, f7;

	f2 = sl_ps_load(PS_EA(a, 0));
	f4 = sl_ps_load(PS_EA(b, 0));
	f6 = sl_ps_add(f2, f4);
	sl_ps_store(PS_EA(c, 0), f6);
	f3 = sl_ps_load1(PS_EA(a, 8));
	f5 = sl_ps_load1(PS_EA(b, 8));
	f7 = sl_ps_add(f3, f5);
	sl_ps_store1(PS_EA(c, 8), f7);
}
#else
asm void PSVECAdd(const register Vec* a, const register Vec* b, register Vec* c)
{
#ifdef __MWERKS__ // clang-format off
	psq_l  f2, Vec.x(a), 0, qr0
	psq_l  f4, Vec.x(b), 0, qr0
	ps_add f6, f2, f4
	psq_st f6, Vec.x(c), 0, qr0
	psq_l  f3, Vec.z(a), 1, qr0
	psq_l  f5, Vec.z(b), 1, qr0
	ps_add f7, f3, f5
	psq_st f7, Vec.z(c), 1, qr0
#endif // clang-format on
}
#endif

#ifdef SONAR_PORT
void PSVECSubtract(const Vec* a, const Vec* b, Vec* c)
{
	sl_ps f2, f3, f4, f5, f6, f7;

	f2 = sl_ps_load(PS_EA(a, 0));
	f4 = sl_ps_load(PS_EA(b, 0));
	f6 = sl_ps_sub(f2, f4);
	sl_ps_store(PS_EA(c, 0), f6);
	f3 = sl_ps_load1(PS_EA(a, 8));
	f5 = sl_ps_load1(PS_EA(b, 8));
	f7 = sl_ps_sub(f3, f5);
	sl_ps_store1(PS_EA(c, 8), f7);
}
#else
asm void PSVECSubtract(const register Vec* a, const register Vec* b, register Vec* c)
{
#ifdef __MWERKS__ // clang-format off
	psq_l  f2, Vec.x(a), 0, qr0
	psq_l  f4, Vec.x(b), 0, qr0
	ps_sub f6, f2, f4
	psq_st f6, Vec.x(c), 0, qr0
	psq_l  f3, Vec.z(a), 1, qr0
	psq_l  f5, Vec.z(b), 1, qr0
	ps_sub f7, f3, f5
	psq_st f7, Vec.z(c), 1, qr0
#endif // clang-format on
}
#endif

#ifdef SONAR_PORT
void PSVECScale(const Vec* src, Vec* dst, f32 mult)
{
	sl_ps f0, f2;
	sl_ps f1 = sl_ps_fill(mult);

	f0 = sl_ps_load(PS_EA(src, 0));
	f2 = sl_ps_load1(PS_EA(src, 8));
	f0 = sl_ps_muls0(f0, f1);
	sl_ps_store(PS_EA(dst, 0), f0);
	f0 = sl_ps_muls0(f2, f1);
	sl_ps_store1(PS_EA(dst, 8), f0);
}
#else
asm void PSVECScale(const register Vec* src, register Vec* dst, register f32 mult)
{
#ifdef __MWERKS__ // clang-format off
	psq_l f0, Vec.x(src), 0, qr0
	psq_l f2, Vec.z(src), 1, qr0
	ps_muls0 f0, f0, f1
	psq_st f0, Vec.x(dst), 0, qr0
	ps_muls0 f0, f2, f1
	psq_st f0, Vec.z(dst), 1, qr0
#endif // clang-format on
}
#endif

#ifdef SONAR_PORT
void PSVECNormalize(const Vec* vec1, Vec* dst)
{
	sl_ps c_half  = sl_ps_fill(0.5f);
	sl_ps c_three = sl_ps_fill(3.0f);
	sl_ps v1_xy;
	sl_ps v1_z;
	sl_ps xx_zz;
	sl_ps xx_yy;
	sl_ps sqsum;
	sl_ps rsqrt = sl_ps_fill(0.0);
	sl_ps nwork0;
	sl_ps nwork1;

	v1_xy     = sl_ps_load(PS_EA(vec1, 0));
	xx_yy     = sl_ps_mul(v1_xy, v1_xy);
	v1_z      = sl_ps_load1(PS_EA(vec1, 8));
	xx_zz     = sl_ps_madd(v1_z, v1_z, xx_yy);
	sqsum     = sl_ps_sum0(xx_zz, v1_z, xx_yy);
	rsqrt.ps0 = sl_fp_frsqrte(sqsum.ps0);
	nwork0    = sl_ps_fill(sl_fp_muls(rsqrt.ps0, rsqrt.ps0));
	nwork1    = sl_ps_fill(sl_fp_muls(rsqrt.ps0, c_half.ps0));
	nwork0    = sl_ps_fill(sl_fp_nmsubs(nwork0.ps0, sqsum.ps0, c_three.ps0));
	rsqrt     = sl_ps_fill(sl_fp_muls(nwork0.ps0, nwork1.ps0));
	v1_xy     = sl_ps_muls0(v1_xy, rsqrt);
	sl_ps_store(PS_EA(dst, 0), v1_xy);
	v1_z = sl_ps_muls0(v1_z, rsqrt);
	sl_ps_store1(PS_EA(dst, 8), v1_z);
}
#else
void PSVECNormalize(const register Vec* vec1, register Vec* dst)
{
	register float c_half  = 0.5f;
	register float c_three = 3.0f;
	register float v1_xy;
	register float v1_z;
	register float xx_zz;
	register float xx_yy;
	register float sqsum;
	register float rsqrt;
	register float nwork0;
	register float nwork1;

#ifdef __MWERKS__ // clang-format off
	asm
	{
		psq_l    v1_xy, Vec.x(vec1), 0, qr0
		ps_mul   xx_yy, v1_xy, v1_xy
		psq_l    v1_z, Vec.z(vec1), 1, qr0
		ps_madd  xx_zz, v1_z, v1_z, xx_yy
		ps_sum0  sqsum, xx_zz, v1_z, xx_yy
		frsqrte  rsqrt, sqsum
		fmuls    nwork0, rsqrt, rsqrt
		fmuls    nwork1, rsqrt, c_half
		fnmsubs  nwork0, nwork0, sqsum, c_three
		fmuls    rsqrt, nwork0, nwork1
		ps_muls0 v1_xy, v1_xy, rsqrt
		psq_st   v1_xy, Vec.x(dst), 0, qr0
		ps_muls0 v1_z, v1_z, rsqrt
		psq_st   v1_z, Vec.z(dst), 1, qr0
	}
#endif // clang-format on
}
#endif

#ifdef SONAR_PORT
float PSVECMag(const Vec* v)
{
	sl_ps f0, f1, f2, f3, f4;

	f0     = sl_ps_load(PS_EA(v, 0));
	f0     = sl_ps_mul(f0, f0);
	f1     = sl_ps_fill(sl_fp_lfs(PS_EA(v, 8)));
	f1     = sl_ps_madd(f1, f1, f0);
	f4     = sl_ps_fill(0.5f);
	f1     = sl_ps_sum0(f1, f0, f0);
	f0.ps0 = sl_fp_frsqrte(f1.ps0);
	f3     = sl_ps_fill(3.0f);
	f2     = sl_ps_fill(sl_fp_muls(f0.ps0, f0.ps0));
	f0     = sl_ps_fill(sl_fp_muls(f0.ps0, f4.ps0));
	f2     = sl_ps_fill(sl_fp_nmsubs(f2.ps0, f1.ps0, f3.ps0));
	f0     = sl_ps_fill(sl_fp_muls(f2.ps0, f0.ps0));
	f0.ps0 = f0.ps0 >= -0.0 ? f0.ps0 : f1.ps0;
	f1     = sl_ps_fill(sl_fp_muls(f1.ps0, f0.ps0));
	return (float)f1.ps0;
}
#else
asm float PSVECMag(const register Vec* v)
{
#ifdef __MWERKS__ // clang-format off
	psq_l   f0, Vec.x(v), 0, qr0
	ps_mul  f0, f0, f0
	lfs     f1, Vec.z(v)
	ps_madd f1, f1, f1, f0
	lfs     f4, 0.5f
	ps_sum0 f1, f1, f0, f0
	frsqrte f0, f1
	lfs     f3, 3.0f
	fmuls   f2, f0, f0
	fmuls   f0, f0, f4
	fnmsubs f2, f2, f1, f3
	fmuls   f0, f2, f0
	fsel    f0, f0, f0, f1
	fmuls   f1, f1, f0
#endif // clang-format on
}
#endif

#ifdef SONAR_PORT
f32 PSVECDotProduct(const Vec* vec1, const Vec* vec2)
{
	sl_ps f1, f2, f3, f4, f5;

	f2 = sl_ps_load(PS_EA(vec1, 4));
	f3 = sl_ps_load(PS_EA(vec2, 4));
	f2 = sl_ps_mul(f2, f3);
	f5 = sl_ps_load(PS_EA(vec1, 0));
	f4 = sl_ps_load(PS_EA(vec2, 0));
	f3 = sl_ps_madd(f5, f4, f2);
	f1 = sl_ps_sum0(f3, f2, f2);
	return (f32)f1.ps0;
}
#else
asm f32 PSVECDotProduct(const register Vec* vec1, const register Vec* vec2)
{
#ifdef __MWERKS__ // clang-format off
	psq_l f2, Vec.y(vec1), 0, qr0
	psq_l f3, Vec.y(vec2), 0, qr0
	ps_mul f2, f2, f3
	psq_l f5, Vec.x(vec1), 0, qr0
	psq_l f4, Vec.x(vec2), 0, qr0
	ps_madd f3, f5, f4, f2
	ps_sum0 f1, f3, f2, f2
#endif // clang-format on
}
#endif

#ifdef SONAR_PORT
void PSVECCrossProduct(const Vec* vec1, const Vec* vec2, Vec* dst)
{
	sl_ps f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;

	f1  = sl_ps_load(PS_EA(vec2, 0));
	f2  = sl_ps_fill(sl_fp_lfs(PS_EA(vec1, 8)));
	f0  = sl_ps_load(PS_EA(vec1, 0));
	f6  = sl_ps_merge10(f1, f1);
	f3  = sl_ps_fill(sl_fp_lfs(PS_EA(vec2, 8)));
	f4  = sl_ps_mul(f1, f2);
	f7  = sl_ps_muls0(f1, f0);
	f5  = sl_ps_msub(f0, f3, f4);
	f8  = sl_ps_msub(f0, f6, f7);
	f9  = sl_ps_merge11(f5, f5);
	f10 = sl_ps_merge01(f5, f8);
	sl_ps_store1(PS_EA(dst, 0), f9);
	f10 = sl_ps_neg(f10);
	sl_ps_store(PS_EA(dst, 4), f10);
}
#else
asm void PSVECCrossProduct(const register Vec* vec1, const register Vec* vec2,
                           register Vec* dst)
{
#ifdef __MWERKS__ // clang-format off
	psq_l f1, Vec.x(vec2), 0, qr0
	lfs f2, Vec.z(vec1)
	psq_l f0, Vec.x(vec1), 0, qr0
	ps_merge10 f6, f1, f1
	lfs f3, Vec.z(vec2)
	ps_mul f4, f1, f2
	ps_muls0 f7, f1, f0
	ps_msub f5, f0, f3, f4
	ps_msub f8, f0, f6, f7
	ps_merge11 f9, f5, f5
	ps_merge01 f10, f5, f8
	psq_st f9, Vec.x(dst), 1, qr0
	ps_neg f10, f10
	psq_st f10, Vec.y(dst), 0, qr0
#endif // clang-format on
}
#endif

#ifdef SONAR_PORT
f32 PSVECSquareDistance(const Vec* vec1, const Vec* vec2)
{
	sl_ps f0, f1, f2;

	f0 = sl_ps_load(PS_EA(vec1, 4));
	f1 = sl_ps_load(PS_EA(vec2, 4));
	f2 = sl_ps_sub(f0, f1);
	f0 = sl_ps_load(PS_EA(vec1, 0));
	f1 = sl_ps_load(PS_EA(vec2, 0));
	f2 = sl_ps_mul(f2, f2);
	f0 = sl_ps_sub(f0, f1);
	f1 = sl_ps_madd(f0, f0, f2);
	f1 = sl_ps_sum0(f1, f2, f2);
	return (f32)f1.ps0;
}
#else
asm f32 PSVECSquareDistance(const register Vec* vec1, const register Vec* vec2)
{
#ifdef __MWERKS__ // clang-format off
	psq_l f0, Vec.y(vec1), 0, qr0
	psq_l f1, Vec.y(vec2), 0, qr0
	ps_sub f2, f0, f1
	psq_l f0, Vec.x(vec1), 0, qr0
	psq_l f1, Vec.x(vec2), 0, qr0
	ps_mul f2, f2, f2
	ps_sub f0, f0, f1
	ps_madd f1, f0, f0, f2
	ps_sum0 f1, f1, f2, f2
#endif // clang-format on
}
#endif

#ifdef SONAR_PORT
f32 PSVECDistance(const Vec* vec1, const Vec* vec2)
{
	sl_ps f0, f1, f2, f3, f4;

	f0     = sl_ps_load(PS_EA(vec1, 4));
	f1     = sl_ps_load(PS_EA(vec2, 4));
	f2     = sl_ps_sub(f0, f1);
	f0     = sl_ps_load(PS_EA(vec1, 0));
	f1     = sl_ps_load(PS_EA(vec2, 0));
	f2     = sl_ps_mul(f2, f2);
	f0     = sl_ps_sub(f0, f1);
	f3     = sl_ps_fill(0.5f);
	f0     = sl_ps_madd(f0, f0, f2);
	f0     = sl_ps_sum0(f0, f2, f2);
	f4     = sl_ps_fill(3.0f);
	f1.ps0 = sl_fp_frsqrte(f0.ps0);
	f2     = sl_ps_fill(sl_fp_muls(f1.ps0, f1.ps0));
	f1     = sl_ps_fill(sl_fp_muls(f1.ps0, f3.ps0));
	f2     = sl_ps_fill(sl_fp_nmsubs(f2.ps0, f0.ps0, f4.ps0));
	f1     = sl_ps_fill(sl_fp_muls(f2.ps0, f1.ps0));
	f1.ps0 = f1.ps0 >= -0.0 ? f1.ps0 : f0.ps0;
	f1     = sl_ps_fill(sl_fp_muls(f0.ps0, f1.ps0));
	return (f32)f1.ps0;
}
#else
asm f32 PSVECDistance(const register Vec* vec1, const register Vec* vec2)
{
#ifdef __MWERKS__ // clang-format off
	psq_l   f0, Vec.y(vec1), 0, qr0
	psq_l   f1, Vec.y(vec2), 0, qr0
	ps_sub  f2, f0, f1
	psq_l   f0, Vec.x(vec1), 0, qr0
	psq_l   f1, Vec.x(vec2), 0, qr0
	ps_mul  f2, f2, f2
	ps_sub  f0, f0, f1
	lfs     f3, 0.5f
	ps_madd f0, f0, f0, f2
	ps_sum0 f0, f0, f2, f2
	lfs     f4, 3.0f
	frsqrte f1, f0
	fmuls   f2, f1, f1
	fmuls   f1, f1, f3
	fnmsubs f2, f2, f0, f4
	fmuls   f1, f2, f1
	fsel    f1, f1, f1, f0
	fmuls   f1, f0, f1
#endif // clang-format on
}
#endif

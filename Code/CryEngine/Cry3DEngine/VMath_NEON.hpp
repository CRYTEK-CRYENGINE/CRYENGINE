// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Version:     v1.00
//  Created:     Michael Kopietz
//  Description: unified vector math lib
// -------------------------------------------------------------------------
//  History:		- created 1999  for Katmai and K3
//							-	...
//							-	integrated into cryengine
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <arm_neon.h>

namespace NVMath
{

	typedef float32x4_t vec4;

	#include "VMath_Prototypes.hpp"

	#define AVOID_COMPILER_BUG

	#ifdef AVOID_COMPILER_BUG

		#define SWIZZLEMASK5(X, Y, Z, W) ((X) | (Y << 2) | (Z << 4) | (W << 6))
		#define SWIZZLEMASK4(N, X, Y, Z) N ## x = SWIZZLEMASK5(X, Y, Z, 0), \
		  N ## y = SWIZZLEMASK5(X, Y, Z, 1),                                \
		  N ## z = SWIZZLEMASK5(X, Y, Z, 2),                                \
		  N ## w = SWIZZLEMASK5(X, Y, Z, 3),
		#define SWIZZLEMASK3(N, X, Y)    SWIZZLEMASK4(N ## x, X, Y, 0) \
		  SWIZZLEMASK4(N ## y, X, Y, 1)                                \
		  SWIZZLEMASK4(N ## z, X, Y, 2)                                \
		  SWIZZLEMASK4(N ## w, X, Y, 3)
		#define SWIZZLEMASK2(N, X)       SWIZZLEMASK3(N ## x, X, 0) \
		  SWIZZLEMASK3(N ## y, X, 1)                                \
		  SWIZZLEMASK3(N ## z, X, 2)                                \
		  SWIZZLEMASK3(N ## w, X, 3)
		#define SWIZZLEMASK1 SWIZZLEMASK2(x, 0) \
		  SWIZZLEMASK2(y, 1)                    \
		  SWIZZLEMASK2(z, 2)                    \
		  SWIZZLEMASK2(w, 3)

enum ESwizzleMask
{
	SWIZZLEMASK1
};

	#else

		#define SWIZZLEMASK5(X, Y, Z, W) { (X + 0), (X + 1), (X + 2), (X + 3), (Y + 0), (Y + 1), (Y + 2), (Y + 3), (Z + 16), (Z + 17), (Z + 18), (Z + 19), (W + 16), (W + 17), (W + 18), (W + 19) }
		#define SWIZZLEMASK4(N, X, Y, Z) SWIZZLEMASK5(X, Y, Z, 0), \
		  SWIZZLEMASK5(X, Y, Z, 4),                                \
		  SWIZZLEMASK5(X, Y, Z, 8),                                \
		  SWIZZLEMASK5(X, Y, Z, 12),
		#define SWIZZLEMASK3(N, X, Y)    SWIZZLEMASK4(N ## x, X, Y, 0) \
		  SWIZZLEMASK4(N ## y, X, Y, 4)                                \
		  SWIZZLEMASK4(N ## z, X, Y, 8)                                \
		  SWIZZLEMASK4(N ## w, X, Y, 12)
		#define SWIZZLEMASK2(N, X)       SWIZZLEMASK3(N ## x, X, 0) \
		  SWIZZLEMASK3(N ## y, X, 4)                                \
		  SWIZZLEMASK3(N ## z, X, 8)                                \
		  SWIZZLEMASK3(N ## w, X, 12)
		#define SWIZZLEMASK1 SWIZZLEMASK2(x, 0) \
		  SWIZZLEMASK2(y, 4)                    \
		  SWIZZLEMASK2(z, 8)                    \
		  SWIZZLEMASK2(w, 12)

		#define SWIZZLEINDEX5(X, Y, Z, W) (X + Y + Z + W)
		#define SWIZZLEINDEX4(N, X, Y, Z) N ## x = SWIZZLEINDEX5(X, Y, Z, 0), \
		  N ## y = SWIZZLEINDEX5(X, Y, Z, 1),                                 \
		  N ## z = SWIZZLEINDEX5(X, Y, Z, 2),                                 \
		  N ## w = SWIZZLEINDEX5(X, Y, Z, 3),
		#define SWIZZLEINDEX3(N, X, Y)    SWIZZLEINDEX4(N ## x, X, Y, 0) \
		  SWIZZLEINDEX4(N ## y, X, Y, 1)                                 \
		  SWIZZLEINDEX4(N ## z, X, Y, 2)                                 \
		  SWIZZLEINDEX4(N ## w, X, Y, 3)
		#define SWIZZLEINDEX2(N, X)       SWIZZLEINDEX3(N ## x, X, 0) \
		  SWIZZLEINDEX3(N ## y, X, 1)                                 \
		  SWIZZLEINDEX3(N ## z, X, 2)                                 \
		  SWIZZLEINDEX3(N ## w, X, 3)
		#define SWIZZLEINDEX1 SWIZZLEINDEX2(x, 0) \
		  SWIZZLEINDEX2(y, 1)                     \
		  SWIZZLEINDEX2(z, 2)                     \
		  SWIZZLEINDEX2(w, 3)

enum ESwizzleMask
{
	SWIZZLEINDEX1
};

namespace cryengine_vmath_neon_internal
{
// Contains pre-calculated lookup indices for Shuffle().  Indexed by ESwizzleMask
static const uint8x16_t g_SwizzleMasks[] = {
	SWIZZLEMASK1
};
} // namespace cryengine_vmath_neon_internal

	#endif // AVOID_COMPILER_BUG

enum ECacheLvl
{
	ECL_LVL1,
	ECL_LVL2,
	ECL_LVL3,
};

/**
 * Bit masks for use with result from SignMask()
 */
static const uint32 BitX = 0x00000080;
static const uint32 BitY = 0x00008000;
static const uint32 BitZ = 0x00800000;
static const uint32 BitW = 0x80000000;

ILINE vec4 Vec4(float x, float y, float z, float w)
{
	const vec4 Ret = { x, y, z, w };
	return Ret;
}

ILINE vec4 Vec4(uint32 x, uint32 y, uint32 z, uint32 w)
{
	const uint32x4_t Ret = { x, y, z, w };
	return (float32x4_t)Ret;
}

ILINE vec4 Vec4(float x)
{
	return Vec4(x, x, x, x);
}

template<int Idx>
ILINE int32 Vec4int32(vec4 V)
{
	return vgetq_lane_s32((int32x4_t)V, Idx);
}

template<int Idx>
ILINE float Vec4float(vec4 V)
{
	return vgetq_lane_f32(V, Idx);
}

ILINE int32 Vec4int32(vec4 V, uint32 Idx)
{
	switch (Idx)
	{
	case 0:
		return vgetq_lane_s32((int32x4_t)V, 0);
	case 1:
		return vgetq_lane_s32((int32x4_t)V, 1);
	case 2:
		return vgetq_lane_s32((int32x4_t)V, 2);
	case 3:
		return vgetq_lane_s32((int32x4_t)V, 3);
	default:
		assert(0);
		return 0;
	}
	//return V.m_s[Idx];
}

ILINE float Vec4float(vec4 V, uint32 Idx)
{
	switch (Idx)
	{
	case 0:
		return vgetq_lane_f32(V, 0);
	case 1:
		return vgetq_lane_f32(V, 1);
	case 2:
		return vgetq_lane_f32(V, 2);
	case 3:
		return vgetq_lane_f32(V, 3);
	default:
		assert(0);
		return 0;
	}
}

ILINE vec4 Vec4Zero()
{
	static const float32x4_t V = { 0.f, 0.f, 0.f, 0.f };
	return V;
}

ILINE vec4 Vec4One()
{
	static const float32x4_t V = { 1.f, 1.f, 1.f, 1.f };
	return V;
}
ILINE vec4 Vec4Four()
{
	static const float32x4_t V = { 4.f, 4.f, 4.f, 4.f };
	return V;
}
ILINE vec4 Vec4ZeroOneTwoThree()
{
	static const float32x4_t V = { 0.f, 1.f, 2.f, 3.f };
	return V;
}
ILINE vec4 Vec4FFFFFFFF()
{
	return Vec4(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);
}
ILINE vec4 Vec4Epsilon()
{
	static const float32x4_t V = { FLT_EPSILON, FLT_EPSILON, FLT_EPSILON, FLT_EPSILON };
	return V;
}

/**
 * Creates vector where each word element is set to value of specified element.
 * INDEX 0 is x, 1 is y, etc.
 * @returns {V[INDEX], V[INDEX], V[INDEX], V[INDEX]}
 */
template<int INDEX>
ILINE vec4 Splat(vec4 V)
{
	switch (INDEX)
	{
	case 0:
	case 1:
		return vdupq_lane_f32(vget_low_f32(V), INDEX);
	case 2:
		// expanded to fix a clang compiler error with the preprocessor
		return vdupq_lane_f32(vget_high_f32(V), 0);
	case 3:
		// expanded to fix a clang compiler error with the preprocessor
		return vdupq_lane_f32(vget_high_f32(V), 1);
	default:
		assert(0);
		return Vec4FFFFFFFF();
	}
}

template<ECacheLvl L>
ILINE void Prefetch(const void* pData)
{
	__builtin_prefetch(pData);
}

/**
 * Return vector containing words from V0 and V1 based on mask M.
 * First two elements of M specify word index in V0 and last two elements
 * specify word index in V1.
 *
 * e.g.
 * Shuffle( M={0,1,2,3} ) => {V0[0], V0[1], V1[2], V1[3]}
 */
template<ESwizzleMask M>
ILINE vec4 Shuffle(vec4 V0, vec4 V1)
{
	#ifdef AVOID_COMPILER_BUG
	return Vec4(Vec4float(V0, M & 3), Vec4float(V0, (M >> 2) & 3), Vec4float(V1, (M >> 4) & 3), Vec4float(V1, (M >> 6) & 3));
	#else
	//return Vec4(V0.m_u[M&3],V0.m_u[(M>>2)&3],V1.m_u[(M>>4)&3],V1.m_u[(M>>6)&3]);
	assert(M >= 0 && M < sizeof(cryengine_vmath_neon_internal::g_SwizzleMasks) / sizeof(cryengine_vmath_neon_internal::g_SwizzleMasks[0]));
	// Get precomputed vector containing lookup indices for swizzle
	const uint8x16_t mask = cryengine_vmath_neon_internal::g_SwizzleMasks[M];
	// Split the lookup indexes into xy and zw pair
	const uint8x8_t low_index = vget_low_u8(mask);
	const uint8x8_t high_index = vget_high_u8(mask);
	// Combine inputs into table; VO=xyzw, V1=XYZW => xyzwXYZW
	const uint8x8x4_t tbl = {
		{
			(uint8x8_t)vget_low_f32(V0),
			(uint8x8_t)vget_high_f32(V0),
			(uint8x8_t)vget_low_f32(V1),
			(uint8x8_t)vget_high_f32(V1),
		}
	};
	// Look up the xy values, then zw values, then combine them
	const uint8x8_t low = vtbl4_u8(tbl, low_index);
	const uint8x8_t high = vtbl4_u8(tbl, high_index);
	const uint8x16_t ret = vcombine_u8(low, high);
	return (float32x4_t)ret;
	#endif // AVOID_COMPILER_BUG
}

template<ESwizzleMask M>
ILINE vec4 Swizzle(vec4 V)
{
	return Shuffle<M>(V, V);
}

/**
 * @returns V0 + V1
 */
ILINE vec4 Add(vec4 V0, vec4 V1)
{
	return vaddq_f32(V0, V1);
}

/**
 * @returns V0 - V1
 */
ILINE vec4 Sub(vec4 V0, vec4 V1)
{
	return vsubq_f32(V0, V1);
}

/**
 * @returns V0 * V1
 */
ILINE vec4 Mul(vec4 V0, vec4 V1)
{
	return vmulq_f32(V0, V1);
}

/**
 * @returns V0 / V1
 */
ILINE vec4 Div(vec4 V0, vec4 V1)
{
	return Mul(V0, Rcp(V1));
}

/**
 * @returns 1 / V
 */
ILINE vec4 RcpFAST(vec4 V)
{
	return vrecpeq_f32(V);
}

/**
 * @returns V0 / V1
 */
ILINE vec4 DivFAST(vec4 V0, vec4 V1)
{
	return Mul(V0, RcpFAST(V1));
}

/**
 * @returns 1 / V
 */
ILINE vec4 Rcp(vec4 V)
{
	// Improve estimate precision with a single Newton-Raphson iteration
	vec4 vEstimate = RcpFAST(V);
	return vmulq_f32(vrecpsq_f32(V, vEstimate), vEstimate);
}

/**
 * @returns V0 * V1 + V2
 */
ILINE vec4 Madd(vec4 V0, vec4 V1, vec4 V2)
{
	// Madd return V0*V1+V2 but vmla(a,b,c) does b*c+a
	return vmlaq_f32(V2, V0, V1);
}

/**
 * @returns V0 * V1 - V2
 */
ILINE vec4 Msub(vec4 V0, vec4 V1, vec4 V2)
{
	// Msub returns V0*V1-V2 but vmls(a,b,c) does a-b*c
	return Sub(Mul(V0, V1), V2);
}

ILINE vec4 Min(vec4 V0, vec4 V1)
{
	return vminq_f32(V0, V1);
}

ILINE vec4 Max(vec4 V0, vec4 V1)
{
	return vmaxq_f32(V0, V1);
}

ILINE vec4 floatToint32(vec4 V)
{
	return (float32x4_t)vcvtq_s32_f32(V);
}

ILINE vec4 int32Tofloat(vec4 V)
{
	return vcvtq_f32_s32((int32x4_t)V);
}

ILINE vec4 CmpLE(vec4 V0, vec4 V1)
{
	return (float32x4_t)vcleq_f32(V0, V1);
}

/**
 * Return sign bit of each word as a single word.  Each is accessible via
 * Bit[X|Y|Z|W] mask.
 */
ILINE uint32 SignMask(vec4 V)
{
	#ifndef __AARCH64_SIMD__
		#if 1
	// Create table from which be select bytes with msb (sign) and combine into
	// single word
	const uint8x8x2_t tbl = {
		{
			(uint8x8_t)vget_low_f32(V),
			(uint8x8_t)vget_high_f32(V),
		}
	};
	const uint8x8_t idx_sign = { 3, 7, 11, 15 }; // MSB -> LSB = W -> X (0xWWZZYYXX)
	const uint8x8_t sign_bytes = vtbl2_u8(tbl, idx_sign);
		#else
	const int32x4_t rev = vshrq_n_s32((int32x4_t)V, 24);
	const int16x4_t shorts = vmovn_s32(rev);
	const uint8x8_t tbl = (uint8x8_t)shorts;
	const uint8x8_t index = { 1, 3, 5, 7 };
	const uint8x8_t sign_bytes = vtbl1_u8(tbl, index);
		#endif
	return vget_lane_u32((uint32x2_t)sign_bytes, 0);
	//return (V.m_Xu>>31)|((V.m_Yu>>31)<<1)|((V.m_Zu>>31)<<2)|((V.m_Wu>>31)<<3);
	#else
	const uint8x16_t tbl = vcombine_u8((uint8x8_t)vget_low_f32(V), (uint8x8_t)vget_high_f32(V));
	const uint8x8_t idx_sign = { 3, 7, 11, 15 }; // MSB -> LSB = W -> X (0xWWZZYYXX)
	const uint8x8_t sign_bytes = vtbl1q_u8(tbl, idx_sign);
	return vget_lane_u32((uint32x2_t)sign_bytes, 0);
	#endif
}

/**
 * @returns V0 & V1
 */
ILINE vec4 And(vec4 V0, vec4 V1)
{
	return (float32x4_t)vandq_u32((uint32x4_t)V0, (uint32x4_t)V1);
}

/**
 * @returns ~V0 & V1
 */
ILINE vec4 AndNot(vec4 V0, vec4 V1)
{
	return (float32x4_t)vandq_u32(vmvnq_u32((uint32x4_t)V0), (uint32x4_t)V1);
}

/**
 * @returns V0 | V1
 */
ILINE vec4 Or(vec4 V0, vec4 V1)
{
	return (float32x4_t)vorrq_u32((uint32x4_t)V0, (uint32x4_t)V1);
}

/**
 * @returns V0 ^ V1
 */
ILINE vec4 Xor(vec4 V0, vec4 V1)
{
	return (float32x4_t)veorq_u32((uint32x4_t)V0, (uint32x4_t)V1);
}

ILINE vec4 Select(vec4 V0, vec4 V1, vec4 M)
{
	return SelectSign(V0, V1, M);
}

/**
 * Returns new vector where each word element comes from V0 if the sign of
 * the corresponding word in M is 0 (+), otherwise the word element comes from
 * V1.
 *
 * e.g.
 * SelectSign( {0.f,1.f,2.f,3.f}, {a.f,b.f,c.f,d.f}, {0.f,0.f,-1.f,-1.f} )
 * => {0.f, 1.f, c.f, d.f}
 */
ILINE vec4 SelectSign(vec4 V0, vec4 V1, vec4 M)
{
	const int32x4_t mask = vshrq_n_s32((int32x4_t)M, 31);
	return vbslq_f32((uint32x4_t)mask, V1, V0);
	//M	=	ShiftAR(M,31);
	//return Or(AndNot(M,V0),And(M,V1));
}

/**
 * Expands each byte of VIn as a float.
 *
 * e.g.
 * ExtractByteToFloat( {0x00010203, 0x04...} ) => {3.f, 2.f, 1.f, 0.f} ...
 */
static ILINE void ExtractByteToFloat(vec4& rVout0, vec4& rVout1, vec4& rVout2, vec4& rVout3, vec4 VIn)
{
	// 16 bytes => 2x 8 shorts
	const int8x8_t XY8 = vget_low_s8((int8x16_t)VIn);
	const int8x8_t ZW8 = vget_high_s8((int8x16_t)VIn);
	const int16x8_t XY16 = vmovl_s8(XY8);
	const int16x8_t ZW16 = vmovl_s8(ZW8);

	// 2x 8 shorts => 4x 4 int
	const int16x4_t X16 = vget_low_s16(XY16);
	const int16x4_t Y16 = vget_high_s16(XY16);
	const int16x4_t Z16 = vget_low_s16(ZW16);
	const int16x4_t W16 = vget_high_s16(ZW16);
	const int32x4_t X32 = vmovl_s16(X16);
	const int32x4_t Y32 = vmovl_s16(Y16);
	const int32x4_t Z32 = vmovl_s16(Z16);
	const int32x4_t W32 = vmovl_s16(W16);

	// 4x 4 int => 4x 4 float
	rVout0 = vcvtq_f32_s32(X32);
	rVout1 = vcvtq_f32_s32(Y32);
	rVout2 = vcvtq_f32_s32(Z32);
	rVout3 = vcvtq_f32_s32(W32);
}

/**
 * Returns vector where each bit is from V0 if the corresponding bit in
 * M is 0, otherwise the bit is from V1 (i.e. vsel instruction from PowerPC, or selb from SPU).
 *
 * e.g.
 * SelectBits(b0001, b1110, b0011) => b0010
 */
ILINE vec4 SelectBits(vec4 V0, vec4 V1, vec4 M)
{
	// Order of args to vbsl is exact opposite of SelectBits()
	return vbslq_f32((uint32x4_t)M, V1, V0);
	//return Or(AndNot(M,V0),And(M,V1));
}

/**
 * Returns vector where, for each word, all bits are set to 1 if the
 * corresponding words in V0 and V1 are equal.
 *
 * e.g.
 * CmpEq( {0.f...}, {0.f, 1.f, 2.f...} ) => {0xffffffff, 0x0, 0x0, 0x0}
 */
ILINE vec4 CmpEq(vec4 V0, vec4 V1)
{
	return (float32x4_t)vceqq_f32(V0, V1);
}

template<int M>
ILINE vec4 SelectStatic(vec4 V0, vec4 V1)
{
	const vec4 mask = Vec4(M & 0x1 ? ~0x0u : 0x0u, M & 0x2 ? ~0x0u : 0x0u, M & 0x4 ? ~0x0u : 0x0u, M & 0x8 ? ~0x0u : 0x0u);
	return Select(V0, V1, mask);
}

}

#undef AVOID_COMPILER_BUG

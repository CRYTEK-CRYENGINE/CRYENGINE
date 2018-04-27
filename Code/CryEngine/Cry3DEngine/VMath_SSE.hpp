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

#define _DO_NOT_DECLARE_INTERLOCKED_INTRINSICS_IN_MEMORY

namespace NVMath
{

typedef __m128 vec4;

#include "VMath_Prototypes.hpp"

#define SWIZZLEMASK4(N, X, Y, Z) N ## x = _MM_SHUFFLE(0, Z, Y, X), \
  N ## y = _MM_SHUFFLE(1, Z, Y, X),                                \
  N ## z = _MM_SHUFFLE(2, Z, Y, X),                                \
  N ## w = _MM_SHUFFLE(3, Z, Y, X),
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
enum ECacheLvl
{
	ECL_LVL1 = _MM_HINT_T0,
	ECL_LVL2 = _MM_HINT_T1,
	ECL_LVL3 = _MM_HINT_T2,
};
#define BitX 1
#define BitY 2
#define BitZ 4
#define BitW 8

ILINE vec4 Vec4(float x)
{
	return _mm_set1_ps(x);
}
ILINE vec4 Vec4(float x, float y, float z, float w)
{
	return _mm_set_ps(w, z, y, x);
}
ILINE vec4 Vec4(uint32 x, uint32 y, uint32 z, uint32 w)
{
	return _mm_set_ps(*reinterpret_cast<float*>(&w),
	                  *reinterpret_cast<float*>(&z),
	                  *reinterpret_cast<float*>(&y),
	                  *reinterpret_cast<float*>(&x));
}
ILINE float Vec4float(vec4 V, uint32 Idx)
{
	union
	{
		vec4  Tmp;
		float V4[4];
	} T;
	T.Tmp = V;
	return T.V4[Idx];
}
template<int Idx>
ILINE float Vec4float(vec4 V)
{
#if defined(VEC4_SSE4)
	float f;
	_MM_EXTRACT_FLOAT(f, V, Idx);
	return f;
#else
	return Vec4float(V, Idx);
#endif
}
template<int Idx>
ILINE int32 Vec4int32(vec4 V)
{
#if defined(VEC4_SSE4)
	return _mm_extract_ps(V, Idx);
#else
	return Vec4int32(V, Idx);
#endif
}
ILINE int32 Vec4int32(vec4 V, uint32 Idx)
{
	union
	{
		vec4  Tmp;
		int32 V4[4];
	} T;
	T.Tmp = V;
	return T.V4[Idx];
}
ILINE vec4 Vec4Zero()
{
	return _mm_setzero_ps();
}

ILINE vec4 Vec4One()
{
	return _mm_set_ps(1.f, 1.f, 1.f, 1.f);
}
ILINE vec4 Vec4Four()
{
	return _mm_set_ps(4.f, 4.f, 4.f, 4.f);
}
ILINE vec4 Vec4ZeroOneTwoThree()
{
	return _mm_set_ps(3.f, 2.f, 1.f, 0.f);
}
ILINE vec4 Vec4FFFFFFFF()
{
	__m128 a = _mm_setzero_ps();
	return _mm_cmpeq_ps(a, a);
}
ILINE vec4 Vec4Epsilon()
{
	return _mm_set_ps(FLT_EPSILON, FLT_EPSILON, FLT_EPSILON, FLT_EPSILON);
}
template<ECacheLvl L>
ILINE void Prefetch(const void* pData)
{
#if (CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID) && !defined(__clang__)
	_mm_prefetch(reinterpret_cast<const char*>(pData), (_mm_hint)L);
#else
	_mm_prefetch(reinterpret_cast<const char*>(pData), (int)L);
#endif
}
template<ESwizzleMask M>
ILINE vec4 Shuffle(vec4 V0, vec4 V1)
{
	return _mm_shuffle_ps(V0, V1, M);
}
template<ESwizzleMask M>
ILINE vec4 Swizzle(vec4 V)
{
	return Shuffle<M>(V, V);
}
ILINE void ExtractByteToFloat(vec4& rVOut0, vec4& rVOut1, vec4& rVOut2, vec4& rVOut3, vec4 VIn)
{
	const vec4 Zf = Vec4Zero();
	const __m128i Z = *reinterpret_cast<const __m128i*>(&Zf);
	__m128i V0 = _mm_unpacklo_epi8(*reinterpret_cast<const __m128i*>(&VIn), Z);
	__m128i V1 = _mm_unpackhi_epi8(*reinterpret_cast<const __m128i*>(&VIn), Z);
	__m128i V00 = _mm_unpacklo_epi8(V0, Z);
	__m128i V01 = _mm_unpackhi_epi8(V0, Z);
	__m128i V10 = _mm_unpacklo_epi8(V1, Z);
	__m128i V11 = _mm_unpackhi_epi8(V1, Z);
	V00 = _mm_srai_epi32(_mm_slli_epi32(V00, 24), 24);
	V01 = _mm_srai_epi32(_mm_slli_epi32(V01, 24), 24);
	V10 = _mm_srai_epi32(_mm_slli_epi32(V10, 24), 24);
	V11 = _mm_srai_epi32(_mm_slli_epi32(V11, 24), 24);
	rVOut0 = int32Tofloat(*reinterpret_cast<const vec4*>(&V00));
	rVOut1 = int32Tofloat(*reinterpret_cast<const vec4*>(&V01));
	rVOut2 = int32Tofloat(*reinterpret_cast<const vec4*>(&V10));
	rVOut3 = int32Tofloat(*reinterpret_cast<const vec4*>(&V11));
}
ILINE vec4 Add(vec4 V0, vec4 V1)
{
	return _mm_add_ps(V0, V1);
}
ILINE vec4 Sub(vec4 V0, vec4 V1)
{
	return _mm_sub_ps(V0, V1);
}
ILINE vec4 Mul(vec4 V0, vec4 V1)
{
	return _mm_mul_ps(V0, V1);
}
ILINE vec4 Div(vec4 V0, vec4 V1)
{
	return _mm_div_ps(V0, V1);
}
ILINE vec4 RcpFAST(vec4 V)
{
	return _mm_rcp_ps(V);
}
ILINE vec4 DivFAST(vec4 V0, vec4 V1)
{
	return Mul(V0, RcpFAST(V1));
}
ILINE vec4 Rcp(vec4 V)
{
	return Div(Vec4One(), V);
}
ILINE vec4 Madd(vec4 V0, vec4 V1, vec4 V2)
{
	return Add(V2, Mul(V0, V1));
}
ILINE vec4 Msub(vec4 V0, vec4 V1, vec4 V2)
{
	return Sub(Mul(V0, V1), V2);
}
ILINE vec4 Min(vec4 V0, vec4 V1)
{
	return _mm_min_ps(V0, V1);
}
ILINE vec4 Max(vec4 V0, vec4 V1)
{
	return _mm_max_ps(V0, V1);
}
ILINE vec4 floatToint32(vec4 V)
{
	const __m128i Tmp = _mm_cvttps_epi32(V);
	return *reinterpret_cast<const vec4*>(&Tmp);
}
ILINE vec4 int32Tofloat(vec4 V)
{
	return _mm_cvtepi32_ps(*reinterpret_cast<__m128i*>(&V));
}
ILINE vec4 CmpLE(vec4 V0, vec4 V1)
{
	return _mm_cmple_ps(V0, V1);
}
ILINE vec4 CmpEq(vec4 V0, vec4 V1)
{
	return _mm_cmpeq_ps(V0, V1);
}
ILINE uint32 SignMask(vec4 V)
{
	return _mm_movemask_ps(V);
}
ILINE vec4 And(vec4 V0, vec4 V1)
{
	const __m128i Tmp = _mm_and_si128(*reinterpret_cast<__m128i*>(&V0), *reinterpret_cast<__m128i*>(&V1));
	return *reinterpret_cast<const vec4*>(&Tmp);
}
ILINE vec4 AndNot(vec4 V0, vec4 V1)
{
	const __m128i Tmp = _mm_andnot_si128(*reinterpret_cast<__m128i*>(&V0), *reinterpret_cast<__m128i*>(&V1));
	return *reinterpret_cast<const vec4*>(&Tmp);
}
ILINE vec4 Or(vec4 V0, vec4 V1)
{
	const __m128i Tmp = _mm_or_si128(*reinterpret_cast<__m128i*>(&V0), *reinterpret_cast<__m128i*>(&V1));
	return *reinterpret_cast<const vec4*>(&Tmp);
}
ILINE vec4 Xor(vec4 V0, vec4 V1)
{
	const __m128i Tmp = _mm_xor_si128(*reinterpret_cast<__m128i*>(&V0), *reinterpret_cast<__m128i*>(&V1));
	return *reinterpret_cast<const vec4*>(&Tmp);
}
ILINE vec4 ShiftAR(vec4 V, uint32 Count)
{
	const __m128i Tmp = _mm_srai_epi32(*reinterpret_cast<__m128i*>(&V), Count);
	return *reinterpret_cast<const vec4*>(&Tmp);
}
template<int INDEX>
ILINE vec4 Splat(vec4 V)
{
	CRY_ASSERT_MESSAGE(0, "Should not be reached!");
	return Vec4FFFFFFFF();
}
template<>
ILINE vec4 Splat<0>(vec4 V)
{
	return Shuffle<xxxx>(V, V);
}
template<>
ILINE vec4 Splat<1>(vec4 V)
{
	return Shuffle<yyyy>(V, V);
}
template<>
ILINE vec4 Splat<2>(vec4 V)
{
	return Shuffle<zzzz>(V, V);
}
template<>
ILINE vec4 Splat<3>(vec4 V)
{
	return Shuffle<wwww>(V, V);
}
ILINE vec4 SelectBits(vec4 V0, vec4 V1, vec4 M)
{
#if defined(VEC4_SSE4)
	return _mm_blendv_ps(V0, V1, M);
#else
	return Or(AndNot(M, V0), And(M, V1));
#endif
}
ILINE vec4 Select(vec4 V0, vec4 V1, vec4 M)
{
#if !defined(VEC4_SSE4)
	M = ShiftAR(M, 31);
#endif
	return SelectBits(V0, V1, M);
}
ILINE vec4 SelectSign(vec4 V0, vec4 V1, vec4 M)
{
#if defined(VEC4_SSE4)
	return Select(V0, V1, M);
#else
	return Select(V0, V1, ShiftAR(M, 31));
#endif
}
template<int M>
ILINE vec4 SelectStatic(vec4 V0, vec4 V1)
{
#if defined(VEC4_SSE4)
	return _mm_blend_ps(V0, V1, M);
#else
	const vec4 mask = Vec4(M & 0x1 ? ~0x0u : 0x0u, M & 0x2 ? ~0x0u : 0x0u, M & 0x4 ? ~0x0u : 0x0u, M & 0x8 ? ~0x0u : 0x0u);
	return Select(V0, V1, mask);
#endif
}

}

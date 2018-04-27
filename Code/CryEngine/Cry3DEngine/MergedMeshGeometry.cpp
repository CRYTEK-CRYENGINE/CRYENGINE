// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryMath/Cry_Geo.h>
#include <CryMath/Cry_GeoIntersect.h>
#include "MergedMeshGeometry.h"
#include "VMath.hpp"

#pragma warning(push)
#pragma warning(disable: 6001)
#pragma warning(disable: 4101)

#if MMRM_ENABLE_PROFILER
	#define MMRM_PROFILE_FUNCTION()  FUNCTION_PROFILER_3DENGINE(PROFILE_3DENGINE)
	#define MMRM_FRAME_PROFILER(x) CRY_PROFILE_REGION(PROFILE_3DENGINE, x)
#else
	#define MMRM_PROFILE_FUNCTION(x)  (void)0
	#define MMRM_FRAME_PROFILER(x) (void)0
#endif

namespace
{
template<class T, class D>
static inline T* AdvancePtr(D*& pData, size_t nCount = 1)
{
	T* Elems = (T*)pData;
	pData = (D*)((T*)pData + nCount);
	return Elems;
}
}

struct SMMRMContact
{
	Plane p;
	int   i;
};

////////////////////////////////////////////////////////////////////////////////
// TODO: cleanup
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
struct CRY_ALIGN(16) Vec3A: public Vec3
{
	Vec3A()
	{
	}
	Vec3A(const Vec3 &v) : Vec3(v)
	{
	}
	Vec3A& operator=(const Vec3& other)
	{
		x = other.x;
		y = other.y;
		z = other.z;
		return *this;
	}
};

struct CRY_ALIGN(16) DualQuatA: public DualQuat
{
	DualQuatA()
	{
	}

	DualQuatA(const DualQuat &v)
		: DualQuat(v)
	{
	}

	DualQuatA& operator=(const DualQuat& other)
	{
		*static_cast<DualQuat*>(this) = other;
		return *this;
	}
};
#else
typedef Vec3     Vec3A;
typedef DualQuat DualQuatA;
#endif

static inline Quat mat33_to_quat(const Matrix33& m)
{
	Quat q;
	float s, p, tr = m.m00 + m.m11 + m.m22;
	q.w = 1, q.v.x = 0, q.v.y = 0, q.v.z = 0;
	if (tr > 0)
		s = sqrt_tpl(tr + 1.0f), p = 0.5f / s, q.w = s * 0.5f, q.v.x = (m.m21 - m.m12) * p, q.v.y = (m.m02 - m.m20) * p, q.v.z = (m.m10 - m.m01) * p;
	else if ((m.m00 >= m.m11) && (m.m00 >= m.m22))
		s = sqrt_tpl(m.m00 - m.m11 - m.m22 + 1.0f), p = 0.5f / s, q.w = (m.m21 - m.m12) * p, q.v.x = s * 0.5f, q.v.y = (m.m10 + m.m01) * p, q.v.z = (m.m20 + m.m02) * p;
	else if ((m.m11 >= m.m00) && (m.m11 >= m.m22))
		s = sqrt_tpl(m.m11 - m.m22 - m.m00 + 1.0f), p = 0.5f / s, q.w = (m.m02 - m.m20) * p, q.v.x = (m.m01 + m.m10) * p, q.v.y = s * 0.5f, q.v.z = (m.m21 + m.m12) * p;
	else if ((m.m22 >= m.m00) && (m.m22 >= m.m11))
		s = sqrt_tpl(m.m22 - m.m00 - m.m11 + 1.0f), p = 0.5f / s, q.w = (m.m10 - m.m01) * p, q.v.x = (m.m02 + m.m20) * p, q.v.y = (m.m12 + m.m21) * p, q.v.z = s * 0.5f;
	return q;
}

#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
	#if CRY_PLATFORM_SSE4
	// _mm_dp_ps
	//
	// tmp0 : = (mask4 == 1) ? (a0 * b0) : +0.0
	// tmp1 : = (mask5 == 1) ? (a1 * b1) : +0.0
	// tmp2 : = (mask6 == 1) ? (a2 * b2) : +0.0
	// tmp3 : = (mask7 == 1) ? (a3 * b3) : +0.0
	// 
	// tmp4 : = tmp0 + tmp1 + tmp2 + tmp3
	// 
	// r0 : = (mask0 == 1) ? tmp4 : +0.0
	// r1 : = (mask1 == 1) ? tmp4 : +0.0
	// r2 : = (mask2 == 1) ? tmp4 : +0.0
	// r3 : = (mask3 == 1) ? tmp4 : +0.0
	static inline __m128 DotProduct4(__m128 a, __m128 b)
	{
		return _mm_dp_ps(a, b, 0xff);
	}
	static inline __m128 DotProduct3(__m128 a, __m128 b)
	{
		return _mm_dp_ps(a, b, 0x77);
	}
	static inline __m128 DotProduct3to4(__m128 a, __m128 b)
	{
		return _mm_dp_ps(a, b, 0x7f);
	}

	// _mm_blendv_ps
	//
	// r0 : = (mask0 & 0x80000000) ? b0 : a0
	// r1 : = (mask1 & 0x80000000) ? b1 : a1
	// r2 : = (mask2 & 0x80000000) ? b2 : a2
	// r3 : = (mask3 & 0x80000000) ? b3 : a3
	static inline __m128 SelectFromVector(__m128 a, __m128 b, __m128 c)
	{
		return _mm_blendv_ps(a, b, c);
	}

	// _mm_blend_ps
	//
	// r0 : = (mask0 == 0) ? b0 : a0
	// r1 : = (mask1 == 0) ? b1 : a1
	// r2 : = (mask2 == 0) ? b2 : a2
	// r3 : = (mask3 == 0) ? b3 : a3
	template<const int c>
	static inline __m128 SelectFromImmediate(__m128 a, __m128 b)
	{
		return _mm_blend_ps(a, b, c);
	}
	#elif CRY_PLATFORM_SSE3
	static inline __m128 DotProduct4(__m128 a, __m128 b)
	{
		__m128 res;

		res = _mm_mul_ps(a, b);
		res = _mm_hadd_ps(res, res);
		res = _mm_hadd_ps(res, res);

		return res;
	}
	static inline __m128 DotProduct3(__m128 a, __m128 b)
	{
		__m128 res;

		res = _mm_mul_ps(a, b);
		res = _mm_and_ps(res, _mm_castsi128_ps(_mm_setr_epi32(~0, ~0, ~0, 0)));
		res = _mm_hadd_ps(res, res);
		res = _mm_hadd_ps(res, res);
		res = _mm_and_ps(res, _mm_castsi128_ps(_mm_setr_epi32(~0, ~0, ~0, 0)));

		return res;
	}
	static inline __m128 DotProduct3to4(__m128 a, __m128 b)
	{
		__m128 res;

		res = _mm_and_ps(_mm_mul_ps(a, b), _mm_castsi128_ps(_mm_setr_epi32(~0, ~0, ~0, 0)));
		res = _mm_hadd_ps(res, res);
		res = _mm_hadd_ps(res, res);

		return res;
	}

	static inline __m128 SelectFromVector(__m128 a, __m128 b, __m128 c)
	{
		__m128 bs = _mm_and_ps(b, c);
		__m128 as = _mm_andnot_ps(c, a);

		return _mm_or_ps(bs, as);
	}
	#else
	static inline __m128 DotProduct4(__m128 a, __m128 b)
	{
		__m128 res;

		res = _mm_mul_ps(a, b);
		res = _mm_add_ps(res, _mm_shuffle_ps(res, res, _MM_SHUFFLE(1, 0, 3, 2)));
		res = _mm_add_ps(res, _mm_shuffle_ps(res, res, _MM_SHUFFLE(0, 1, 2, 3)));

		return res;
	}
	static inline __m128 DotProduct3(__m128 a, __m128 b)
	{
		__m128 res;

		res = _mm_mul_ps(a, b);
		res = _mm_and_ps(res, _mm_castsi128_ps(_mm_setr_epi32(~0, ~0, ~0, 0)));
		res = _mm_add_ps(res, _mm_shuffle_ps(res, res, _MM_SHUFFLE(1, 0, 3, 2)));
		res = _mm_add_ps(res, _mm_shuffle_ps(res, res, _MM_SHUFFLE(0, 1, 2, 3)));
		res = _mm_and_ps(res, _mm_castsi128_ps(_mm_setr_epi32(~0, ~0, ~0, 0)));

		return res;
	}
	static inline __m128 DotProduct3to4(__m128 a, __m128 b)
	{
		__m128 res;

		res = _mm_mul_ps(a, b);
		res = _mm_and_ps(res, _mm_castsi128_ps(_mm_setr_epi32(~0, ~0, ~0, 0)));
		res = _mm_add_ps(res, _mm_shuffle_ps(res, res, _MM_SHUFFLE(1, 0, 3, 2)));
		res = _mm_add_ps(res, _mm_shuffle_ps(res, res, _MM_SHUFFLE(0, 1, 2, 3)));

		return res;
	}

	static inline __m128 SelectFromVector(__m128 a, __m128 b, __m128 c)
	{
		__m128 bs = _mm_and_ps(b, c);
		__m128 as = _mm_andnot_ps(c, a);

		return _mm_or_ps(bs, as);
	}
#endif

	static inline __m128 Absolute(__m128 a)
	{
		return _mm_and_ps(a, _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF)));
	}
#endif

#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
	#if CRY_PLATFORM_F16C
	static inline void CvtToHalf(Vec3f16& v, __m128 value)
	{
		static_assert(sizeof(Vec3f16) == sizeof(uint64_t), "64-bit store intended here for performance");
		const __m128i result = _mm_cvtps_ph(value, 0);
		_mm_storel_epi64(reinterpret_cast<__m128i*>(&v), result); // Store X, Y, Z, W(!)
	}

	static inline void CvtToHalf(Vec3f16& v, const Vec3& value)
	{
		CvtToHalf(v, _mm_set_ps(0.0f, value.z, value.y, value.x));
	}

	static inline void CvtToHalf(Vec2f16& v, const Vec2& value)
	{
		static_assert(sizeof(Vec2f16) == sizeof(uint32_t), "32-bit store intended here for performance");
		const __m128 halfreg = _mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(&value))); // Load X, Y, 0, 0
		const __m128i result = _mm_cvtps_ph(halfreg, 0);
		*reinterpret_cast<int*>(&v) = _mm_cvtsi128_si32(result); // _mm_storeu_si32 not supported on all compilers, this should (hopefully) be MOVD m32, xmm
	}
	#else
	static inline __m128i float_to_half_SSE2(__m128 f)
	{
		#define DECL_CONST4(name, val) static const CRY_ALIGN(16) uint name[4] = { (val), (val), (val), (val) }
		#define GET_CONSTI(name)       *(const __m128i*)&name
		#define GET_CONSTF(name)       *(const __m128*)&name

		DECL_CONST4(mask_sign, 0x80000000u);
		DECL_CONST4(mask_round, ~0xfffu);
		DECL_CONST4(c_f32infty, 255 << 23);
		DECL_CONST4(c_magic, 15 << 23);
		DECL_CONST4(c_nanbit, 0x200);
		DECL_CONST4(c_infty_as_fp16, 0x7c00);
		DECL_CONST4(c_clamp, (31 << 23) - 0x1000);

		__m128 msign = GET_CONSTF(mask_sign);
		__m128 justsign = _mm_and_ps(msign, f);
		__m128i f32infty = GET_CONSTI(c_f32infty);
		__m128 absf = _mm_xor_ps(f, justsign);
		__m128 mround = GET_CONSTF(mask_round);
		__m128i absf_int = _mm_castps_si128(absf); // pseudo-op, but val needs to be copied once so count as mov
		__m128i b_isnan = _mm_cmpgt_epi32(absf_int, f32infty);
		__m128i b_isnormal = _mm_cmpgt_epi32(f32infty, _mm_castps_si128(absf));
		__m128i nanbit = _mm_and_si128(b_isnan, GET_CONSTI(c_nanbit));
		__m128i inf_or_nan = _mm_or_si128(nanbit, GET_CONSTI(c_infty_as_fp16));

		__m128 fnosticky = _mm_and_ps(absf, mround);
		__m128 scaled = _mm_mul_ps(fnosticky, GET_CONSTF(c_magic));
		__m128 clamped = _mm_min_ps(scaled, GET_CONSTF(c_clamp)); // logically, we want PMINSD on "biased", but this should gen better code
		__m128i biased = _mm_sub_epi32(_mm_castps_si128(clamped), _mm_castps_si128(mround));
		__m128i shifted = _mm_srli_epi32(biased, 13);
		__m128i normal = _mm_and_si128(shifted, b_isnormal);
		__m128i not_normal = _mm_andnot_si128(b_isnormal, inf_or_nan);
		__m128i joined = _mm_or_si128(normal, not_normal);

		__m128i sign_shift = _mm_srli_epi32(_mm_castps_si128(justsign), 16);
		__m128i final = _mm_or_si128(joined, sign_shift);

		// ~20 SSE2 ops
		return final;

		#undef DECL_CONST4
		#undef GET_CONSTI
		#undef GET_CONSTF
	}
	static inline __m128i approx_float_to_half_SSE2(__m128 f)
	{

		#if defined(__GNUC__)
			#define DECL_CONST4(name, val) static const uint __attribute__((aligned(16))) name[4] = { (val), (val), (val), (val) }
		#else
			#define DECL_CONST4(name, val) static const __declspec(align(16)) uint name[4] = { (val), (val), (val), (val) }
		#endif
		#define GET_CONSTF(name)         *(const __m128*)&name

		DECL_CONST4(mask_fabs, 0x7fffffffu);
		DECL_CONST4(c_f32infty, (255 << 23));
		DECL_CONST4(c_expinf, (255 ^ 31) << 23);
		DECL_CONST4(c_f16max, (127 + 16) << 23);
		DECL_CONST4(c_magic, 15 << 23);

		__m128 mabs = GET_CONSTF(mask_fabs);
		__m128 fabs = _mm_and_ps(mabs, f);
		__m128 justsign = _mm_xor_ps(f, fabs);

		__m128 f16max = GET_CONSTF(c_f16max);
		__m128 expinf = GET_CONSTF(c_expinf);
		__m128 infnancase = _mm_xor_ps(expinf, fabs);
		__m128 clamped = _mm_min_ps(f16max, fabs);
		__m128 b_notnormal = _mm_cmpnlt_ps(fabs, GET_CONSTF(c_f32infty));
		__m128 scaled = _mm_mul_ps(clamped, GET_CONSTF(c_magic));

		__m128 merge1 = _mm_and_ps(infnancase, b_notnormal);
		__m128 merge2 = _mm_andnot_ps(b_notnormal, scaled);
		__m128 merged = _mm_or_ps(merge1, merge2);

		__m128i shifted = _mm_srli_epi32(_mm_castps_si128(merged), 13);
		__m128i signshifted = _mm_srli_epi32(_mm_castps_si128(justsign), 16);
		__m128i final = _mm_or_si128(shifted, signshifted);

		// ~15 SSE2 ops
		return final;

		#undef DECL_CONST4
		#undef GET_CONSTF
	}

	static inline void CvtToHalf(Vec3f16& v, __m128 value)
	{
		__m128i result = approx_float_to_half_SSE2(value);
		v.x = (reinterpret_cast<int*>(&result))[0];
		v.y = (reinterpret_cast<int*>(&result))[1];
		v.z = (reinterpret_cast<int*>(&result))[2];
	}

	static inline void CvtToHalf(Vec3f16& v, const Vec3& value)
	{
		__m128 val128 = _mm_set_ps(0.f, value.z, value.y, value.x);
		__m128i result = approx_float_to_half_SSE2(val128);
		v.x = (reinterpret_cast<int*>(&result))[0];
		v.y = (reinterpret_cast<int*>(&result))[1];
		v.z = (reinterpret_cast<int*>(&result))[2];
	}

	static inline void CvtToHalf(Vec2f16& v, const Vec2& value)
	{
		__m128 val128 = _mm_set_ps(0.f, 0.f, value.y, value.x);
		__m128i result = approx_float_to_half_SSE2(val128);
		v.x = (reinterpret_cast<int*>(&result))[0];
		v.y = (reinterpret_cast<int*>(&result))[1];
	}
	#endif
#endif

#define MMRM_USE_OPTIMIZED_LINESEG_SPHERE 1
#define MMRM_USE_OPTIMIZED_POINT_LENSEG   1
//----------------------------------------------------------------------------------
//--- 0x00 = no intersection                               --------------------------
//--- 0x01 = one intersection, lineseg has just an ENTRY point but no EXIT point (ls.end is inside the sphere)  --
//--- 0x02 = one intersection, lineseg has just an EXIT point but no ENTRY point (ls.start is inside the sphere)  --
//--- 0x03 = two intersection, lineseg has ENTRY and EXIT point  --
//----------------------------------------------------------------------------------
static ILINE int _Lineseg_Sphere(const Lineseg& ls, const Sphere& s, Vec3& i0, Vec3& i1)
{
#if MMRM_USE_OPTIMIZED_LINESEG_SPHERE
	Vec3 dir = (ls.end - ls.start);
	float a = dir | dir;
	const Vec3 lssc = ls.start - s.center;
	int intersection = 0;
	float b = (dir | (lssc)) * 2.0f;
	float c = ((lssc) | (lssc)) - (s.radius * s.radius);
	float desc = (b * b) - (4 * a * c);

	if (desc >= 0.f)
	{
		float sqrtdesc = sqrt_tpl(desc);
		float r2a = 1.f / (2.0f * a);
		float lamba0 = (-b - sqrtdesc) * r2a;
		float lamba1 = (-b + sqrtdesc) * r2a;

		if (lamba0 > 0.0f)
		{
			i0 = ls.start + ((ls.end - ls.start) * lamba0);
			//skip, if 1st cutting-point is "in front" of ls.end
			if (((i0 - ls.end) | dir) > 0) return 0;
			intersection = 0x01;
		}

		if (lamba1 > 0.0f)
		{
			i1 = ls.start + ((ls.end - ls.start) * lamba1);
			//skip, if 2nd cutting-point is "in front" of ls.end (=ls.end is inside sphere)
			if (((i1 - ls.end) | dir) > 0) return intersection;
			intersection |= 0x02;
		}
	}
	return intersection;
#else

	Vec3 dir = (ls.end - ls.start);
	float a = dir | dir;
	float b = (dir | (ls.start - s.center)) * 2.0f;
	float c = ((ls.start - s.center) | (ls.start - s.center)) - (s.radius * s.radius);
	float desc = (b * b) - (4 * a * c);

	unsigned char intersection = 0;
	if (desc >= 0.0f)
	{
		float lamba0 = (-b - sqrt_tpl(desc)) / (2.0f * a);
		if (lamba0 > 0.0f)
		{
			i0 = ls.start + ((ls.end - ls.start) * lamba0);
			//skip, if 1st cutting-point is "in front" of ls.end
			if (((i0 - ls.end) | dir) > 0) return 0;
			intersection = 0x01;
		}

		float lamba1 = (-b + sqrt_tpl(desc)) / (2.0f * a);
		if (lamba1 > 0.0f)
		{
			i1 = ls.start + ((ls.end - ls.start) * lamba1);
			//skip, if 2nd cutting-point is "in front" of ls.end (=ls.end is inside sphere)
			if (((i1 - ls.end) | dir) > 0) return intersection;
			intersection |= 0x02;
		}
	}
	return intersection;
#endif
}

//----------------------------------------------------------------------------------
/// Returns squared distance from a point to a line segment
//----------------------------------------------------------------------------------
static ILINE float _Point_LinesegSq(const Vec3& p, const Lineseg& lineseg)
{
#if MMRM_USE_OPTIMIZED_POINT_LENSEG
	Vec3 diff = p - lineseg.start;
	Vec3 dir = lineseg.end - lineseg.start;
	float fT = diff.Dot(dir);
	fT = __fsel(fT, fT, 0.f);

	float fSqrLen = dir.len2();
	float mask = fT - fSqrLen;
	float mul0 = __fsel(mask, 1.f, 0.f);
	float mul1 = __fsel(mask, 0.f, 1.f);

	mul1 /= __fsel(mask, 1.f, fSqrLen);
	diff -= dir * (mul0 + mul1);

	return diff.len2();
#else
	Vec3 diff = p - lineseg.start;
	Vec3 dir = lineseg.end - lineseg.start;
	float fT = diff.Dot(dir);

	if (fT <= 0.0f)
	{
		fT = 0.0f;
	}
	else
	{
		float fSqrLen = dir.len2();
		if (fT >= fSqrLen)
		{
			fT = 1.0f;
			diff -= dir;
		}
		else
		{
			fT *= 1.f / (fSqrLen);
			diff -= fT * dir;
		}
	}

	return diff.GetLengthSquared();
#endif
}

#undef MMRM_USE_OPTIMIZED_LINESEG_SPHERE
#undef MMRM_USE_OPTIMIZED_POINT_LENSEG

struct SMMRMInstanceContext
{
	SMMRMInstance*      samples;
	SMMRMSpineVtxBase*  spines;
	SMMRMDeformVertex*  deform;
	SMMRMGeometry*      geom;
	const CCamera*      cam;
	SMMRMUpdateContext* update;
	SMergedRMChunk*     updateChunks;
	primitives::sphere* colliders;
	SMMRMProjectile*    projectiles;
	Vec3*               wind;
	int                 flags;
	int                 ncolliders;
	int                 nprojectiles;
	float               maxViewDistSq;
	float               lodRatioSq;
	float               diameterSq;
	float               dt, dtscale, abstime;
	float               zRotation;
	Vec3                rotationOrigin;
	Vec3                min, max, centre;
	size_t              amount;
	int                 max_iter;
	int                 use_spines;
	int                 frame_count;
};

////////////////////////////////////////////////////////////////////////////////
// Cull a set of instances against the frustum
static inline size_t CullInstanceList(
  SMMRMInstanceContext& context, SMMRMVisibleChunk* visChunks,
  const Vec3& origin, const Vec3& rotationOrigin, float zRotation)
{
	MEMORY_SCOPE_CHECK_HEAP();
	size_t numSamplesVisible = 0u;
	const float maxViewDistSq = context.maxViewDistSq;
	const float lodRatioSq = context.lodRatioSq;
	const float diameterSq = context.diameterSq;
	const float diameter = sqrt_tpl(diameterSq);
	const Vec3 camPos = context.cam->GetPosition();
	const int cullFrustum = context.flags & MMRM_CULL_FRUSTUM;
	const int cullDistance = context.flags & MMRM_CULL_DISTANCE;
	const int cullLod = context.flags & MMRM_CULL_LOD;
	const int forceLod = (context.flags >> MMRM_LOD_SHIFT) & 3;
	const int sampleReduction = context.flags >> MMRM_SAMPLE_REDUCTION_SHIFT;
	const float sampleOffset = 1.0f + (float) sampleReduction;
	const float fExtents = c_MergedMeshesExtent;
	SMMRMChunk* chunks = NULL;
	mmrm_printf("culling %d samples \n", context.amount);
	for (size_t j = 0; j < context.amount; ++j)
	{
		SMMRMInstance& sample = context.samples[j];
		Vec3 sample_pos = ConvertInstanceAbsolute(sample, origin, rotationOrigin, zRotation, fExtents);
		// Note: use the diameter, not the radius for culling as it better describes
		// the maximum movement radius a simulated instance can make.
		const Sphere sampleSphere(sample_pos, diameter);
		const float distanceSq = (camPos - sample_pos).len2() - diameterSq;
		if (j & sampleReduction)
		{
			sample.lastLod = -1;
			continue;
		}
		if (cullDistance && distanceSq > maxViewDistSq * sampleOffset)
		{
			sample.lastLod = -1;
			continue;
		}
		if (cullFrustum && !context.cam->IsSphereVisible_F(sampleSphere))
		{
			sample.lastLod = -1;
			continue;
		}
		int nLodTmp = (int)(distanceSq * lodRatioSq / (max(20.0f * 20.f * diameterSq, 0.001f)));
		size_t nLod = (size_t) (cullLod ? forceLod : nLodTmp);
		PREFAST_SUPPRESS_WARNING(6385)
		for (nLod = std::min(nLod, size_t(MAX_STATOBJ_LODS_NUM - 1)); !(chunks = context.geom->pChunks[nLod]); --nLod)
			;
		if (!chunks)
		{
			sample.lastLod = -1;
			continue;
		}
		sample.lastLod = static_cast<int8>(nLod);
		PREFAST_SUPPRESS_WARNING(6385) for (size_t i = 0; i < context.geom->numChunks[nLod]; ++i)
		{
			mmrm_assert(chunks[i].nvertices);
			mmrm_assert(chunks[i].nindices);
			visChunks[i].vertices += chunks[i].nvertices;
			visChunks[i].indices += chunks[i].nindices;
			visChunks[i].matId = chunks[i].matId;
			mmrm_assert(visChunks[i].vertices);
			mmrm_assert(visChunks[i].indices);
		}
		++numSamplesVisible;
	}
	mmrm_printf("%d samples visible\n", numSamplesVisible);
	return numSamplesVisible;
}

////////////////////////////////////////////////////////////////////////////////
// Update a set of vertices to be worldspace and merge a deformation stream into
// them
static inline void UpdateGeneral(
  SVF_P3S_C4B_T2S* out
  , SVF_P3F_C4B_T2F* in
  , SMMRMDeformVertex* deform
  , uint16* mapping
  , Matrix34 tmat
  , size_t count)
{
	MMRM_PROFILE_FUNCTION();
	for (size_t i = 0; i < count; ++i)
	{
		out[i].xyz = tmat * deform[mapping[i]].pos[0];
		out[i].color = in[i].color;
		out[i].st = in[i].st;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Update a set of vertices to be worldspace
static inline void UpdateGeneral(
  SVF_P3S_C4B_T2S* out
  , SVF_P3F_C4B_T2F* in
  , const Matrix34& wmat)
{
	MMRM_PROFILE_FUNCTION();
	out->xyz = wmat * in->xyz;
	out->color = in->color;
	out->st = in->st;
}
static inline void UpdateGeneral(
  SVF_P3S_C4B_T2S* out
  , SVF_P3F_C4B_T2F* in
  , const Matrix34& wmat
  , size_t count)
{
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		out[i + 0].xyz = wmat * in[i + 0].xyz;
		out[i + 0].color = in[i + 0].color;
		out[i + 0].st = in[i + 0].st;
		out[i + 1].xyz = wmat * in[i + 1].xyz;
		out[i + 1].color = in[i + 1].color;
		out[i + 1].st = in[i + 1].st;
		out[i + 2].xyz = wmat * in[i + 2].xyz;
		out[i + 2].color = in[i + 2].color;
		out[i + 2].st = in[i + 2].st;
		out[i + 3].xyz = wmat * in[i + 3].xyz;
		out[i + 3].color = in[i + 3].color;
		out[i + 3].st = in[i + 3].st;
	}
	for (size_t i = (count & ~3); i < count; ++i)
	{
		out[i].xyz = wmat * in[i].xyz;
		out[i].color = in[i].color;
		out[i].st = in[i].st;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Skin a set of normals against a set of bones
static inline void UpdateNormals(
  Vec3f16* out
  , Vec3* in
  , size_t count)
{
	MMRM_PROFILE_FUNCTION();
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		out[i + 0] = in[i + 0];
		out[i + 1] = in[i + 1];
		out[i + 2] = in[i + 2];
		out[i + 3] = in[i + 3];
	}
	for (size_t i = (count & ~3); i < count; ++i)
	{
		out[i] = in[i];
	}
}

static inline void UpdateNormals(
  Vec3f16* out
  , Vec3* in
  , DualQuatA* bones
  , SMMRMBoneMapping* weights
  , size_t maxSpines
  , size_t count)
{
	MMRM_PROFILE_FUNCTION();
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
	using namespace NVMath;
	DualQuatA wq[4];
	__m128 _wq[4 * 2];
	__m128 nq_len[4];
	__m128 vweights[4];
	__m128 vw[4];
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		_wq[0] = _wq[1] = _wq[2] = _wq[3] = _wq[4] = _wq[5] = _wq[6] = _wq[7] = _mm_xor_ps(_wq[0], _wq[0]);
		vweights[0] = _mm_load_ps(&weights[i + 0].weights[0]);
		vweights[1] = _mm_load_ps(&weights[i + 1].weights[0]);
		vweights[2] = _mm_load_ps(&weights[i + 2].weights[0]);
		vweights[3] = _mm_load_ps(&weights[i + 3].weights[0]);
		switch (maxSpines)
		{
		case 4:
			vw[0] = Swizzle<wwww>(vweights[0]);
			vw[1] = Swizzle<wwww>(vweights[1]);
			vw[2] = Swizzle<wwww>(vweights[2]);
			vw[3] = Swizzle<wwww>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[3]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[3]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[3]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[3]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[3]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[3]].dq), vw[3]));
		case 3:
			vw[0] = Swizzle<zzzz>(vweights[0]);
			vw[1] = Swizzle<zzzz>(vweights[1]);
			vw[2] = Swizzle<zzzz>(vweights[2]);
			vw[3] = Swizzle<zzzz>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[2]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[2]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[2]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[2]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[2]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[2]].dq), vw[3]));
		case 2:
			vw[0] = Swizzle<yyyy>(vweights[0]);
			vw[1] = Swizzle<yyyy>(vweights[1]);
			vw[2] = Swizzle<yyyy>(vweights[2]);
			vw[3] = Swizzle<yyyy>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[1]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[1]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[1]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[1]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[1]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[1]].dq), vw[3]));
		case 1:
			vw[0] = Swizzle<xxxx>(vweights[0]);
			vw[1] = Swizzle<xxxx>(vweights[1]);
			vw[2] = Swizzle<xxxx>(vweights[2]);
			vw[3] = Swizzle<xxxx>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[0]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[0]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[0]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[0]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[0]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[0]].dq), vw[3]));
		}
		nq_len[0] = _mm_rsqrt_ps(DotProduct4(_wq[0 * 2 + 0], _wq[0 * 2 + 0]));
		nq_len[1] = _mm_rsqrt_ps(DotProduct4(_wq[1 * 2 + 0], _wq[1 * 2 + 0]));
		nq_len[2] = _mm_rsqrt_ps(DotProduct4(_wq[2 * 2 + 0], _wq[2 * 2 + 0]));
		nq_len[3] = _mm_rsqrt_ps(DotProduct4(_wq[3 * 2 + 0], _wq[3 * 2 + 0]));

		_mm_store_ps((float*)&wq[0].nq, _mm_mul_ps(_wq[0 * 2 + 0], nq_len[0]));
		_mm_store_ps((float*)&wq[0].dq, _mm_mul_ps(_wq[0 * 2 + 1], nq_len[0]));
		_mm_store_ps((float*)&wq[1].nq, _mm_mul_ps(_wq[1 * 2 + 0], nq_len[1]));
		_mm_store_ps((float*)&wq[1].dq, _mm_mul_ps(_wq[1 * 2 + 1], nq_len[1]));
		_mm_store_ps((float*)&wq[2].nq, _mm_mul_ps(_wq[2 * 2 + 0], nq_len[2]));
		_mm_store_ps((float*)&wq[2].dq, _mm_mul_ps(_wq[2 * 2 + 1], nq_len[2]));
		_mm_store_ps((float*)&wq[3].nq, _mm_mul_ps(_wq[3 * 2 + 0], nq_len[3]));
		_mm_store_ps((float*)&wq[3].dq, _mm_mul_ps(_wq[3 * 2 + 1], nq_len[3]));

		out[i + 0] = wq[0].nq * in[i + 0];
		out[i + 1] = wq[1].nq * in[i + 1];
		out[i + 2] = wq[2].nq * in[i + 2];
		out[i + 3] = wq[3].nq * in[i + 3];
	}
	for (size_t i = (count & ~3); i < count; ++i)
	{
		_wq[0] = _wq[1] = _mm_xor_ps(_wq[0], _wq[0]);
		vweights[0] = _mm_load_ps(&weights[i + 0].weights[0]);
		switch (maxSpines)
		{
		case 4:
			vw[0] = Swizzle<wwww>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].dq), vw[0]));
		case 3:
			vw[0] = Swizzle<zzzz>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].dq), vw[0]));
		case 2:
			vw[0] = Swizzle<yyyy>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].dq), vw[0]));
		case 1:
			vw[0] = Swizzle<xxxx>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].dq), vw[0]));
		}
		nq_len[0] = _mm_rsqrt_ps(DotProduct4(_wq[0 * 2 + 0], _wq[0 * 2 + 0]));
		_mm_store_ps((float*)&wq[0].nq, _mm_mul_ps(_wq[0 * 2 + 0], nq_len[0]));
		_mm_store_ps((float*)&wq[0].dq, _mm_mul_ps(_wq[0 * 2 + 1], nq_len[0]));
		out[i + 0] = wq[0].nq * in[i + 0];
	}
#else
	DualQuatA wq[4];
	float l[4];
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		wq[0] = wq[1] = wq[2] = wq[3] = DualQuatA(ZERO);
		switch (maxSpines)
		{
		case 4:
			wq[0] += bones[weights[i + 0].boneIds[3]] * (weights[i + 0].weights[3] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[3]] * (weights[i + 1].weights[3] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[3]] * (weights[i + 2].weights[3] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[3]] * (weights[i + 3].weights[3] / 255.f);
		case 3:
			wq[0] += bones[weights[i + 0].boneIds[2]] * (weights[i + 0].weights[2] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[2]] * (weights[i + 1].weights[2] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[2]] * (weights[i + 2].weights[2] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[2]] * (weights[i + 3].weights[2] / 255.f);
		case 2:
			wq[0] += bones[weights[i + 0].boneIds[1]] * (weights[i + 0].weights[1] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[1]] * (weights[i + 1].weights[1] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[1]] * (weights[i + 2].weights[1] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[1]] * (weights[i + 3].weights[1] / 255.f);
		case 1:
			wq[0] += bones[weights[i + 0].boneIds[0]] * (weights[i + 0].weights[0] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[0]] * (weights[i + 1].weights[0] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[0]] * (weights[i + 2].weights[0] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[0]] * (weights[i + 3].weights[0] / 255.f);
		}

		l[0] = wq[0].nq.GetLength();
		l[1] = wq[1].nq.GetLength();
		l[2] = wq[2].nq.GetLength();
		l[3] = wq[3].nq.GetLength();

		l[0] = l[0] > 0.f ? 1.f / (l[0]) : l[0];
		l[1] = l[1] > 0.f ? 1.f / (l[1]) : l[1];
		l[2] = l[2] > 0.f ? 1.f / (l[2]) : l[2];
		l[3] = l[3] > 0.f ? 1.f / (l[3]) : l[3];

		wq[0].nq *= l[0];
		wq[1].nq *= l[1];
		wq[2].nq *= l[2];
		wq[3].nq *= l[3];

		out[i + 0] = wq[0].nq * in[i + 0];
		out[i + 1] = wq[1].nq * in[i + 1];
		out[i + 2] = wq[2].nq * in[i + 2];
		out[i + 3] = wq[3].nq * in[i + 3];
	}
	for (size_t i = (count & ~3); i < count; ++i)
	{
		wq[0] = DualQuatA(ZERO);
		switch (maxSpines)
		{
		case 4:
			wq[0] += bones[weights[i + 0].boneIds[3]] * (weights[i + 0].weights[3] / 255.f);
		case 3:
			wq[0] += bones[weights[i + 0].boneIds[2]] * (weights[i + 0].weights[2] / 255.f);
		case 2:
			wq[0] += bones[weights[i + 0].boneIds[1]] * (weights[i + 0].weights[1] / 255.f);
		case 1:
			wq[0] += bones[weights[i + 0].boneIds[0]] * (weights[i + 0].weights[0] / 255.f);
		}
		l[0] = wq[0].nq.GetLength();
		l[0] = l[0] > 0.f ? 1.f / (l[0]) : l[0];
		wq[0].nq *= l[0];
		out[i + 0] = wq[0].nq * in[i + 0];
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////
// Skin a set of vertices against a set of bones
static inline void UpdateGeneral(
  SVF_P3S_C4B_T2S* out
  , SVF_P3F_C4B_T2F* in
  , DualQuatA* bones
  , SMMRMBoneMapping* weights
  , const float fScale
  , size_t maxSpines)
{
	MMRM_PROFILE_FUNCTION();
	Vec3 vpos = Vec3(0, 0, 0);
	const Vec3 xyz = in->xyz * fScale;
	switch (maxSpines)
	{
	case 4:
		vpos += bones[weights->boneIds[3]] * xyz * (weights->weights[3] / 255.f);
	case 3:
		vpos += bones[weights->boneIds[2]] * xyz * (weights->weights[2] / 255.f);
	case 2:
		vpos += bones[weights->boneIds[1]] * xyz * (weights->weights[1] / 255.f);
	case 1:
		vpos += bones[weights->boneIds[0]] * xyz * (weights->weights[0] / 255.f);
	}
	out->xyz = vpos;
	out->color = in->color;
	out->st = in->st;
}
static inline void UpdateGeneral(
  SVF_P3S_C4B_T2S* out
  , SVF_P3F_C4B_T2F* in
  , DualQuatA* bones
  , SMMRMBoneMapping* weights
  , const float fScale
  , size_t maxSpines
  , size_t count)
{
	MMRM_PROFILE_FUNCTION();
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
	using namespace NVMath;
	DualQuatA wq[4];
	__m128 _wq[4 * 2];
	__m128 nq_len[4];
	__m128 vweights[4];
	__m128 vw[4];
	#if MMRM_UNROLL_GEOMETRY_BAKING_LOOPS
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		_wq[0] = _wq[1] = _wq[2] = _wq[3] = _wq[4] = _wq[5] = _wq[6] = _wq[7] = _mm_xor_ps(_wq[0], _wq[0]);
		vweights[0] = _mm_load_ps(&weights[i + 0].weights[0]);
		vweights[1] = _mm_load_ps(&weights[i + 1].weights[0]);
		vweights[2] = _mm_load_ps(&weights[i + 2].weights[0]);
		vweights[3] = _mm_load_ps(&weights[i + 3].weights[0]);
		switch (maxSpines)
		{
		case 4:
			vw[0] = Swizzle<wwww>(vweights[0]);
			vw[1] = Swizzle<wwww>(vweights[1]);
			vw[2] = Swizzle<wwww>(vweights[2]);
			vw[3] = Swizzle<wwww>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[3]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[3]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[3]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[3]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[3]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[3]].dq), vw[3]));
		case 3:
			vw[0] = Swizzle<zzzz>(vweights[0]);
			vw[1] = Swizzle<zzzz>(vweights[1]);
			vw[2] = Swizzle<zzzz>(vweights[2]);
			vw[3] = Swizzle<zzzz>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[2]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[2]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[2]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[2]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[2]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[2]].dq), vw[3]));
		case 2:
			vw[0] = Swizzle<yyyy>(vweights[0]);
			vw[1] = Swizzle<yyyy>(vweights[1]);
			vw[2] = Swizzle<yyyy>(vweights[2]);
			vw[3] = Swizzle<yyyy>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[1]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[1]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[1]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[1]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[1]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[1]].dq), vw[3]));
		case 1:
			vw[0] = Swizzle<xxxx>(vweights[0]);
			vw[1] = Swizzle<xxxx>(vweights[1]);
			vw[2] = Swizzle<xxxx>(vweights[2]);
			vw[3] = Swizzle<xxxx>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[0]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[0]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[0]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[0]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[0]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[0]].dq), vw[3]));
		}
		nq_len[0] = _mm_rsqrt_ps(DotProduct4(_wq[0 * 2 + 0], _wq[0 * 2 + 0]));
		nq_len[1] = _mm_rsqrt_ps(DotProduct4(_wq[1 * 2 + 0], _wq[1 * 2 + 0]));
		nq_len[2] = _mm_rsqrt_ps(DotProduct4(_wq[2 * 2 + 0], _wq[2 * 2 + 0]));
		nq_len[3] = _mm_rsqrt_ps(DotProduct4(_wq[3 * 2 + 0], _wq[3 * 2 + 0]));

		_mm_store_ps((float*)&wq[0].nq, _mm_mul_ps(_wq[0 * 2 + 0], nq_len[0]));
		_mm_store_ps((float*)&wq[0].dq, _mm_mul_ps(_wq[0 * 2 + 1], nq_len[0]));
		_mm_store_ps((float*)&wq[1].nq, _mm_mul_ps(_wq[1 * 2 + 0], nq_len[1]));
		_mm_store_ps((float*)&wq[1].dq, _mm_mul_ps(_wq[1 * 2 + 1], nq_len[1]));
		_mm_store_ps((float*)&wq[2].nq, _mm_mul_ps(_wq[2 * 2 + 0], nq_len[2]));
		_mm_store_ps((float*)&wq[2].dq, _mm_mul_ps(_wq[2 * 2 + 1], nq_len[2]));
		_mm_store_ps((float*)&wq[3].nq, _mm_mul_ps(_wq[3 * 2 + 0], nq_len[3]));
		_mm_store_ps((float*)&wq[3].dq, _mm_mul_ps(_wq[3 * 2 + 1], nq_len[3]));

		out[i + 0].xyz = wq[0] * (in[i + 0].xyz * fScale);
		out[i + 1].xyz = wq[1] * (in[i + 1].xyz * fScale);
		out[i + 2].xyz = wq[2] * (in[i + 2].xyz * fScale);
		out[i + 3].xyz = wq[3] * (in[i + 3].xyz * fScale);

		out[i + 0].color = in[i + 0].color;
		out[i + 0].st = in[i + 0].st;

		out[i + 1].color = in[i + 1].color;
		out[i + 1].st = in[i + 1].st;

		out[i + 2].color = in[i + 2].color;
		out[i + 2].st = in[i + 2].st;

		out[i + 3].color = in[i + 3].color;
		out[i + 3].st = in[i + 3].st;
	}
	for (size_t i = (count & ~3); i < count; ++i)
	#else // MMRM_UNROLL_GEOMETRY_BAKING_LOOPS
	for (size_t i = 0; i < count; ++i)
	#endif
	{
		_wq[0] = _wq[1] = _mm_xor_ps(_wq[0], _wq[0]);
		vweights[0] = _mm_load_ps(&weights[i + 0].weights[0]);
		switch (maxSpines)
		{
		case 4:
			vw[0] = Swizzle<wwww>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].dq), vw[0]));
		case 3:
			vw[0] = Swizzle<zzzz>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].dq), vw[0]));
		case 2:
			vw[0] = Swizzle<yyyy>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].dq), vw[0]));
		case 1:
			vw[0] = Swizzle<xxxx>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].dq), vw[0]));
		}
		nq_len[0] = _mm_rsqrt_ps(DotProduct4(_wq[0 * 2 + 0], _wq[0 * 2 + 0]));
		_mm_store_ps((float*)&wq[0].nq, _mm_mul_ps(_wq[0 * 2 + 0], nq_len[0]));
		_mm_store_ps((float*)&wq[0].dq, _mm_mul_ps(_wq[0 * 2 + 1], nq_len[0]));
		out[i].xyz = wq[0] * (in[i].xyz * fScale);
		out[i].color = in[i].color;
		out[i].st = in[i].st;
	}
#else
	DualQuatA wq[4];
	float l[4];
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		wq[0] = wq[1] = wq[2] = wq[3] = DualQuatA(ZERO);
		switch (maxSpines)
		{
		case 4:
			wq[0] += bones[weights[i + 0].boneIds[3]] * (weights[i + 0].weights[3] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[3]] * (weights[i + 1].weights[3] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[3]] * (weights[i + 2].weights[3] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[3]] * (weights[i + 3].weights[3] / 255.f);
		case 3:
			wq[0] += bones[weights[i + 0].boneIds[2]] * (weights[i + 0].weights[2] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[2]] * (weights[i + 1].weights[2] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[2]] * (weights[i + 2].weights[2] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[2]] * (weights[i + 3].weights[2] / 255.f);
		case 2:
			wq[0] += bones[weights[i + 0].boneIds[1]] * (weights[i + 0].weights[1] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[1]] * (weights[i + 1].weights[1] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[1]] * (weights[i + 2].weights[1] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[1]] * (weights[i + 3].weights[1] / 255.f);
		case 1:
			wq[0] += bones[weights[i + 0].boneIds[0]] * (weights[i + 0].weights[0] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[0]] * (weights[i + 1].weights[0] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[0]] * (weights[i + 2].weights[0] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[0]] * (weights[i + 3].weights[0] / 255.f);
		}

		l[0] = wq[0].nq.GetLength();
		l[1] = wq[1].nq.GetLength();
		l[2] = wq[2].nq.GetLength();
		l[3] = wq[3].nq.GetLength();

		l[0] = l[0] > 0.f ? 1.f / (l[0]) : l[0];
		l[1] = l[1] > 0.f ? 1.f / (l[1]) : l[1];
		l[2] = l[2] > 0.f ? 1.f / (l[2]) : l[2];
		l[3] = l[3] > 0.f ? 1.f / (l[3]) : l[3];

		wq[0].nq *= l[0];
		wq[0].dq *= l[0];
		wq[1].nq *= l[1];
		wq[1].dq *= l[1];
		wq[2].nq *= l[2];
		wq[2].dq *= l[2];
		wq[3].nq *= l[3];
		wq[3].dq *= l[3];

		out[i + 0].xyz = wq[0] * (in[i + 0].xyz * fScale);
		out[i + 1].xyz = wq[1] * (in[i + 1].xyz * fScale);
		out[i + 2].xyz = wq[2] * (in[i + 2].xyz * fScale);
		out[i + 3].xyz = wq[3] * (in[i + 3].xyz * fScale);

		out[i + 0].color = in[i + 0].color;
		out[i + 0].st = in[i + 0].st;

		out[i + 1].color = in[i + 1].color;
		out[i + 1].st = in[i + 1].st;

		out[i + 2].color = in[i + 2].color;
		out[i + 2].st = in[i + 2].st;

		out[i + 3].color = in[i + 3].color;
		out[i + 3].st = in[i + 3].st;

	}
	for (size_t i = (count & ~3); i < count; ++i)
	{
		wq[0] = DualQuatA(ZERO);
		switch (maxSpines)
		{
		case 4:
			wq[0] += bones[weights[i].boneIds[3]] * (weights[i].weights[3] / 255.f);
		case 3:
			wq[0] += bones[weights[i].boneIds[2]] * (weights[i].weights[2] / 255.f);
		case 2:
			wq[0] += bones[weights[i].boneIds[1]] * (weights[i].weights[1] / 255.f);
		case 1:
			wq[0] += bones[weights[i].boneIds[0]] * (weights[i].weights[0] / 255.f);
		}
		l[0] = wq[0].nq.GetLength();
		l[0] = l[0] > 0.f ? 1.f / (l[0]) : l[0];
		wq[0].nq *= l[0];
		wq[0].dq *= l[0];
		out[i].xyz = wq[0] * (in[i].xyz * fScale);
		out[i].color = in[i].color;
		out[i].st = in[i].st;
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////
// Update a set of tangents
static inline void UpdateTangents(
  SPipTangents* out
  , SPipQTangents* in
  , DualQuatA* bones
  , SMMRMBoneMapping* weights
  , size_t maxSpines
  , size_t count)
{
	MMRM_PROFILE_FUNCTION();
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
	using namespace NVMath;
	Quat out_tangents[4];
	DualQuatA wq[4];
	int16 flip[4];
	__m128 _wq[4 * 2];
	__m128 nq_len[4];
	__m128 vweights[4];
	__m128 vw[4];
	Quat in_tangents[4];
	#if MMRM_UNROLL_GEOMETRY_BAKING_LOOPS
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		in_tangents[0] = in[i + 0].GetQ();
		in_tangents[1] = in[i + 1].GetQ();
		in_tangents[2] = in[i + 2].GetQ();
		in_tangents[3] = in[i + 3].GetQ();
		_wq[0] = _wq[1] = _wq[2] = _wq[3] = _wq[4] = _wq[5] = _wq[6] = _wq[7] = _mm_xor_ps(_wq[0], _wq[0]);
		vweights[0] = _mm_load_ps(&weights[i + 0].weights[0]);
		vweights[1] = _mm_load_ps(&weights[i + 1].weights[0]);
		vweights[2] = _mm_load_ps(&weights[i + 2].weights[0]);
		vweights[3] = _mm_load_ps(&weights[i + 3].weights[0]);
		switch (maxSpines)
		{
		case 4:
			vw[0] = Swizzle<wwww>(vweights[0]);
			vw[1] = Swizzle<wwww>(vweights[1]);
			vw[2] = Swizzle<wwww>(vweights[2]);
			vw[3] = Swizzle<wwww>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[3]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[3]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[3]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[3]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[3]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[3]].dq), vw[3]));
		case 3:
			vw[0] = Swizzle<zzzz>(vweights[0]);
			vw[1] = Swizzle<zzzz>(vweights[1]);
			vw[2] = Swizzle<zzzz>(vweights[2]);
			vw[3] = Swizzle<zzzz>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[2]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[2]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[2]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[2]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[2]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[2]].dq), vw[3]));
		case 2:
			vw[0] = Swizzle<yyyy>(vweights[0]);
			vw[1] = Swizzle<yyyy>(vweights[1]);
			vw[2] = Swizzle<yyyy>(vweights[2]);
			vw[3] = Swizzle<yyyy>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[1]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[1]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[1]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[1]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[1]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[1]].dq), vw[3]));
		case 1:
			vw[0] = Swizzle<xxxx>(vweights[0]);
			vw[1] = Swizzle<xxxx>(vweights[1]);
			vw[2] = Swizzle<xxxx>(vweights[2]);
			vw[3] = Swizzle<xxxx>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[0]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[0]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[0]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[0]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[0]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[0]].dq), vw[3]));
		}
		nq_len[0] = _mm_rsqrt_ps(DotProduct4(_wq[0 * 2 + 0], _wq[0 * 2 + 0]));
		nq_len[1] = _mm_rsqrt_ps(DotProduct4(_wq[1 * 2 + 0], _wq[1 * 2 + 0]));
		nq_len[2] = _mm_rsqrt_ps(DotProduct4(_wq[2 * 2 + 0], _wq[2 * 2 + 0]));
		nq_len[3] = _mm_rsqrt_ps(DotProduct4(_wq[3 * 2 + 0], _wq[3 * 2 + 0]));

		_mm_store_ps((float*)&wq[0].nq, _mm_mul_ps(_wq[0 * 2 + 0], nq_len[0]));
		_mm_store_ps((float*)&wq[0].dq, _mm_mul_ps(_wq[0 * 2 + 1], nq_len[0]));
		_mm_store_ps((float*)&wq[1].nq, _mm_mul_ps(_wq[1 * 2 + 0], nq_len[1]));
		_mm_store_ps((float*)&wq[1].dq, _mm_mul_ps(_wq[1 * 2 + 1], nq_len[1]));
		_mm_store_ps((float*)&wq[2].nq, _mm_mul_ps(_wq[2 * 2 + 0], nq_len[2]));
		_mm_store_ps((float*)&wq[2].dq, _mm_mul_ps(_wq[2 * 2 + 1], nq_len[2]));
		_mm_store_ps((float*)&wq[3].nq, _mm_mul_ps(_wq[3 * 2 + 0], nq_len[3]));
		_mm_store_ps((float*)&wq[3].dq, _mm_mul_ps(_wq[3 * 2 + 1], nq_len[3]));

		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		out_tangents[1] = (wq[1].nq * in_tangents[1]);
		out_tangents[2] = (wq[2].nq * in_tangents[2]);
		out_tangents[3] = (wq[3].nq * in_tangents[3]);

		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		if (out_tangents[1].w < 0.f) out_tangents[1] = -out_tangents[1];
		if (out_tangents[2].w < 0.f) out_tangents[2] = -out_tangents[2];
		if (out_tangents[3].w < 0.f) out_tangents[3] = -out_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out[i + 3] = SPipTangents(out_tangents[3], flip[3]);
	}
	for (size_t i = (count & ~3); i < count; ++i)
	#else // MMRM_UNROLL_GEOMETRY_BAKING_LOOPS
	for (size_t i = 0; i < count; ++i)
	#endif
	{
		in_tangents[0] = in[i + 0].GetQ();
		_wq[0] = _wq[1] = _mm_xor_ps(_wq[0], _wq[0]);
		vweights[0] = _mm_load_ps(&weights[i + 0].weights[0]);
		switch (maxSpines)
		{
		case 4:
			vw[0] = Swizzle<wwww>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].dq), vw[0]));
		case 3:
			vw[0] = Swizzle<zzzz>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].dq), vw[0]));
		case 2:
			vw[0] = Swizzle<yyyy>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].dq), vw[0]));
		case 1:
			vw[0] = Swizzle<xxxx>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].dq), vw[0]));
		}
		nq_len[0] = _mm_rsqrt_ps(DotProduct4(_wq[0 * 2 + 0], _wq[0 * 2 + 0]));
		_mm_store_ps((float*)&wq[0].nq, _mm_mul_ps(_wq[0 * 2 + 0], nq_len[0]));
		_mm_store_ps((float*)&wq[0].dq, _mm_mul_ps(_wq[0 * 2 + 1], nq_len[0]));
		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out[i + 0] = SPipTangents(out_tangents[0], flip[0]);
	}
#else
	Quat out_tangents[4];
	Quat in_tangents[4];
	DualQuatA wq[4];
	int16 flip[4];
	f32 l[4];
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		in_tangents[0] = in[i + 0].GetQ();
		in_tangents[1] = in[i + 1].GetQ();
		in_tangents[2] = in[i + 2].GetQ();
		in_tangents[3] = in[i + 3].GetQ();
		wq[0] = wq[1] = wq[2] = wq[3] = DualQuatA(ZERO);
		switch (maxSpines)
		{
		case 4:
			wq[0] += bones[weights[i + 0].boneIds[3]] * (weights[i + 0].weights[3] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[3]] * (weights[i + 1].weights[3] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[3]] * (weights[i + 2].weights[3] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[3]] * (weights[i + 3].weights[3] / 255.f);
		case 3:
			wq[0] += bones[weights[i + 0].boneIds[2]] * (weights[i + 0].weights[2] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[2]] * (weights[i + 1].weights[2] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[2]] * (weights[i + 2].weights[2] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[2]] * (weights[i + 3].weights[2] / 255.f);
		case 2:
			wq[0] += bones[weights[i + 0].boneIds[1]] * (weights[i + 0].weights[1] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[1]] * (weights[i + 1].weights[1] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[1]] * (weights[i + 2].weights[1] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[1]] * (weights[i + 3].weights[1] / 255.f);
		case 1:
			wq[0] += bones[weights[i + 0].boneIds[0]] * (weights[i + 0].weights[0] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[0]] * (weights[i + 1].weights[0] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[0]] * (weights[i + 2].weights[0] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[0]] * (weights[i + 3].weights[0] / 255.f);
		}

		l[0] = wq[0].nq.GetLength();
		l[1] = wq[1].nq.GetLength();
		l[2] = wq[2].nq.GetLength();
		l[3] = wq[3].nq.GetLength();

		l[0] = l[0] > 0.f ? 1.f / (l[0]) : l[0];
		l[1] = l[1] > 0.f ? 1.f / (l[1]) : l[1];
		l[2] = l[2] > 0.f ? 1.f / (l[2]) : l[2];
		l[3] = l[3] > 0.f ? 1.f / (l[3]) : l[3];

		wq[0].nq *= l[0];
		wq[1].nq *= l[1];
		wq[2].nq *= l[2];
		wq[3].nq *= l[3];

		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		out_tangents[1] = (wq[1].nq * in_tangents[1]);
		out_tangents[2] = (wq[2].nq * in_tangents[2]);
		out_tangents[3] = (wq[3].nq * in_tangents[3]);

		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		if (out_tangents[1].w < 0.f) out_tangents[1] = -out_tangents[1];
		if (out_tangents[2].w < 0.f) out_tangents[2] = -out_tangents[2];
		if (out_tangents[3].w < 0.f) out_tangents[3] = -out_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out[i + 3] = SPipTangents(out_tangents[3], flip[3]);
	}
	for (size_t i = (count & ~3); i < count; ++i)
	{
		in_tangents[0] = in[i + 0].GetQ();
		wq[0] = DualQuatA(ZERO);
		switch (maxSpines)
		{
		case 4:
			wq[0] += bones[weights[i + 0].boneIds[3]] * (weights[i + 0].weights[3] / 255.f);
		case 3:
			wq[0] += bones[weights[i + 0].boneIds[2]] * (weights[i + 0].weights[2] / 255.f);
		case 2:
			wq[0] += bones[weights[i + 0].boneIds[1]] * (weights[i + 0].weights[1] / 255.f);
		case 1:
			wq[0] += bones[weights[i + 0].boneIds[0]] * (weights[i + 0].weights[0] / 255.f);
		}
		l[0] = wq[0].nq.GetLength();
		l[0] = l[0] > 0.f ? 1.f / (l[0]) : l[0];
		wq[0].nq *= l[0];
		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out[i + 0] = SPipTangents(out_tangents[0], flip[0]);
	}
#endif
}
static inline void UpdateTangents(SPipTangents* out, SPipQTangents* in, const Matrix34& mat, size_t count)
{
	MMRM_PROFILE_FUNCTION();
	Quat out_tangents[4];
	Quat in_tangents[4];
	int16 flip[4];
	Quat q = mat33_to_quat(Matrix33(mat));
	q.Normalize();

	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		in_tangents[0] = in[i + 0].GetQ();
		in_tangents[1] = in[i + 1].GetQ();
		in_tangents[2] = in[i + 2].GetQ();
		in_tangents[3] = in[i + 3].GetQ();

		out_tangents[0] = q * in_tangents[0];
		out_tangents[1] = q * in_tangents[1];
		out_tangents[2] = q * in_tangents[2];
		out_tangents[3] = q * in_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out[i + 3] = SPipTangents(out_tangents[3], flip[3]);
	}

	for (size_t i = (count & ~3); i < count; ++i)
	{
		in_tangents[0] = in[i + 0].GetQ();
		out_tangents[0] = q * in_tangents[0];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out[i + 0] = SPipTangents(out_tangents[0], flip[0]);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Update a set of general, tangents and normals
static inline void UpdateGeneralTangents(
  SVF_P3S_C4B_T2S* out_general
  , SPipTangents* out_packed_tangents
  , SVF_P3F_C4B_T2F* in_general
  , SPipQTangents* in_packed_tangents
  , DualQuatA* bones
  , SMMRMBoneMapping* weights
  , const float fScale
  , size_t maxSpines
  , size_t count)
{
	MMRM_PROFILE_FUNCTION();
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
	using namespace NVMath;
	DualQuatA wq[4];
	__m128 _wq[4 * 2];
	__m128 nq_len[4];
	__m128 ot[4];
	__m128 vOne = _mm_set1_ps(1.f);
	int16 flip[4];
	__m128 vweights[4];
	__m128 vw[4];
	CRY_ALIGN(16) Quat out_tangents[4];
	CRY_ALIGN(16) Quat in_tangents[4];
	#if MMRM_UNROLL_GEOMETRY_BAKING_LOOPS
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		_wq[0] = _wq[1] = _wq[2] = _wq[3] = _wq[4] = _wq[5] = _wq[6] = _wq[7] = _mm_xor_ps(_wq[0], _wq[0]);
		in_tangents[0] = in_packed_tangents[i + 0].GetQ();
		in_tangents[1] = in_packed_tangents[i + 1].GetQ();
		in_tangents[2] = in_packed_tangents[i + 2].GetQ();
		in_tangents[3] = in_packed_tangents[i + 3].GetQ();
		vweights[0] = _mm_load_ps(&weights[i + 0].weights[0]);
		vweights[1] = _mm_load_ps(&weights[i + 1].weights[0]);
		vweights[2] = _mm_load_ps(&weights[i + 2].weights[0]);
		vweights[3] = _mm_load_ps(&weights[i + 3].weights[0]);
		switch (maxSpines)
		{
		case 4:
			vw[0] = Swizzle<wwww>(vweights[0]);
			vw[1] = Swizzle<wwww>(vweights[1]);
			vw[2] = Swizzle<wwww>(vweights[2]);
			vw[3] = Swizzle<wwww>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[3]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[3]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[3]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[3]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[3]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[3]].dq), vw[3]));
		case 3:
			vw[0] = Swizzle<zzzz>(vweights[0]);
			vw[1] = Swizzle<zzzz>(vweights[1]);
			vw[2] = Swizzle<zzzz>(vweights[2]);
			vw[3] = Swizzle<zzzz>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[2]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[2]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[2]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[2]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[2]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[2]].dq), vw[3]));
		case 2:
			vw[0] = Swizzle<yyyy>(vweights[0]);
			vw[1] = Swizzle<yyyy>(vweights[1]);
			vw[2] = Swizzle<yyyy>(vweights[2]);
			vw[3] = Swizzle<yyyy>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[1]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[1]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[1]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[1]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[1]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[1]].dq), vw[3]));
		case 1:
			vw[0] = Swizzle<xxxx>(vweights[0]);
			vw[1] = Swizzle<xxxx>(vweights[1]);
			vw[2] = Swizzle<xxxx>(vweights[2]);
			vw[3] = Swizzle<xxxx>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[0]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[0]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[0]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[0]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[0]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[0]].dq), vw[3]));
		}
		nq_len[0] = _mm_rsqrt_ps(DotProduct4(_wq[0 * 2 + 0], _wq[0 * 2 + 0]));
		nq_len[1] = _mm_rsqrt_ps(DotProduct4(_wq[1 * 2 + 0], _wq[1 * 2 + 0]));
		nq_len[2] = _mm_rsqrt_ps(DotProduct4(_wq[2 * 2 + 0], _wq[2 * 2 + 0]));
		nq_len[3] = _mm_rsqrt_ps(DotProduct4(_wq[3 * 2 + 0], _wq[3 * 2 + 0]));

		_mm_store_ps((float*)&wq[0].nq, _mm_mul_ps(_wq[0 * 2 + 0], nq_len[0]));
		_mm_store_ps((float*)&wq[0].dq, _mm_mul_ps(_wq[0 * 2 + 1], nq_len[0]));
		_mm_store_ps((float*)&wq[1].nq, _mm_mul_ps(_wq[1 * 2 + 0], nq_len[1]));
		_mm_store_ps((float*)&wq[1].dq, _mm_mul_ps(_wq[1 * 2 + 1], nq_len[1]));
		_mm_store_ps((float*)&wq[2].nq, _mm_mul_ps(_wq[2 * 2 + 0], nq_len[2]));
		_mm_store_ps((float*)&wq[2].dq, _mm_mul_ps(_wq[2 * 2 + 1], nq_len[2]));
		_mm_store_ps((float*)&wq[3].nq, _mm_mul_ps(_wq[3 * 2 + 0], nq_len[3]));
		_mm_store_ps((float*)&wq[3].dq, _mm_mul_ps(_wq[3 * 2 + 1], nq_len[3]));

		CvtToHalf(out_general[i + 0].xyz, wq[0] * (in_general[i + 0].xyz * fScale));
		out_general[i + 0].color = in_general[i + 0].color;
		CvtToHalf(out_general[i + 0].st, in_general[i + 0].st);

		CvtToHalf(out_general[i + 1].xyz, wq[1] * (in_general[i + 1].xyz * fScale));
		out_general[i + 1].color = in_general[i + 1].color;
		CvtToHalf(out_general[i + 1].st, in_general[i + 1].st);

		CvtToHalf(out_general[i + 2].xyz, wq[2] * (in_general[i + 2].xyz * fScale));
		out_general[i + 2].color = in_general[i + 2].color;
		CvtToHalf(out_general[i + 2].st, in_general[i + 2].st);

		CvtToHalf(out_general[i + 3].xyz, wq[3] * (in_general[i + 3].xyz * fScale));
		out_general[i + 3].color = in_general[i + 3].color;
		CvtToHalf(out_general[i + 3].st, in_general[i + 3].st);

		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		out_tangents[1] = (wq[1].nq * in_tangents[1]);
		out_tangents[2] = (wq[2].nq * in_tangents[2]);
		out_tangents[3] = (wq[3].nq * in_tangents[3]);

		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		if (out_tangents[1].w < 0.f) out_tangents[1] = -out_tangents[1];
		if (out_tangents[2].w < 0.f) out_tangents[2] = -out_tangents[2];
		if (out_tangents[3].w < 0.f) out_tangents[3] = -out_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_packed_tangents[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out_packed_tangents[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out_packed_tangents[i + 3] = SPipTangents(out_tangents[3], flip[3]);
	}
	for (size_t i = (count & ~3); i < count; ++i)
	#else // MMRM_UNROLL_GEOMETRY_BAKING_LOOPS
	for (size_t i = 0; i < count; ++i)
	#endif
	{
		_wq[0] = _wq[1] = _mm_xor_ps(_wq[0], _wq[0]);
		in_tangents[0] = in_packed_tangents[i + 0].GetQ();
		vweights[0] = _mm_load_ps(&weights[i + 0].weights[0]);
		switch (maxSpines)
		{
		case 4:
			vw[0] = Swizzle<wwww>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].dq), vw[0]));
		case 3:
			vw[0] = Swizzle<zzzz>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].dq), vw[0]));
		case 2:
			vw[0] = Swizzle<yyyy>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].dq), vw[0]));
		case 1:
			vw[0] = Swizzle<xxxx>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].dq), vw[0]));
		}
		nq_len[0] = _mm_rsqrt_ps(DotProduct4(_wq[0 * 2 + 0], _wq[0 * 2 + 0]));
		_mm_store_ps((float*)&wq[0].nq, _mm_mul_ps(_wq[0 * 2 + 0], nq_len[0]));
		_mm_store_ps((float*)&wq[0].dq, _mm_mul_ps(_wq[0 * 2 + 1], nq_len[0]));
		CvtToHalf(out_general[i + 0].xyz, wq[0] * (in_general[i + 0].xyz * fScale));
		out_general[i + 0].color = in_general[i + 0].color;
		CvtToHalf(out_general[i + 0].st, in_general[i + 0].st);
		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
	}
#else
	DualQuatA wq[4];
	Quat in_tangents[4];
	Quat out_tangents[4];
	float l[4];
	int16 flip[4];
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		wq[0] = wq[1] = wq[2] = wq[3] = DualQuatA(ZERO);
		in_tangents[0] = in_packed_tangents[i + 0].GetQ();
		in_tangents[1] = in_packed_tangents[i + 1].GetQ();
		in_tangents[2] = in_packed_tangents[i + 2].GetQ();
		in_tangents[3] = in_packed_tangents[i + 3].GetQ();
		switch (maxSpines)
		{
		case 4:
			wq[0] += bones[weights[i + 0].boneIds[3]] * (weights[i + 0].weights[3] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[3]] * (weights[i + 1].weights[3] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[3]] * (weights[i + 2].weights[3] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[3]] * (weights[i + 3].weights[3] / 255.f);
		case 3:
			wq[0] += bones[weights[i + 0].boneIds[2]] * (weights[i + 0].weights[2] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[2]] * (weights[i + 1].weights[2] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[2]] * (weights[i + 2].weights[2] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[2]] * (weights[i + 3].weights[2] / 255.f);
		case 2:
			wq[0] += bones[weights[i + 0].boneIds[1]] * (weights[i + 0].weights[1] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[1]] * (weights[i + 1].weights[1] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[1]] * (weights[i + 2].weights[1] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[1]] * (weights[i + 3].weights[1] / 255.f);
		case 1:
			wq[0] += bones[weights[i + 0].boneIds[0]] * (weights[i + 0].weights[0] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[0]] * (weights[i + 1].weights[0] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[0]] * (weights[i + 2].weights[0] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[0]] * (weights[i + 3].weights[0] / 255.f);
		}

		l[0] = wq[0].nq.GetLength();
		l[1] = wq[1].nq.GetLength();
		l[2] = wq[2].nq.GetLength();
		l[3] = wq[3].nq.GetLength();

		l[0] = l[0] > 0.f ? 1.f / (l[0]) : l[0];
		l[1] = l[1] > 0.f ? 1.f / (l[1]) : l[1];
		l[2] = l[2] > 0.f ? 1.f / (l[2]) : l[2];
		l[3] = l[3] > 0.f ? 1.f / (l[3]) : l[3];

		wq[0].nq *= l[0];
		wq[0].dq *= l[0];
		wq[1].nq *= l[1];
		wq[1].dq *= l[1];
		wq[2].nq *= l[2];
		wq[2].dq *= l[2];
		wq[3].nq *= l[3];
		wq[3].dq *= l[3];

		out_general[i + 0].xyz = wq[0] * (in_general[i + 0].xyz * fScale);
		out_general[i + 1].xyz = wq[1] * (in_general[i + 1].xyz * fScale);
		out_general[i + 2].xyz = wq[2] * (in_general[i + 2].xyz * fScale);
		out_general[i + 3].xyz = wq[3] * (in_general[i + 3].xyz * fScale);

		out_general[i + 0].color = in_general[i + 0].color;
		out_general[i + 0].st = in_general[i + 0].st;

		out_general[i + 1].color = in_general[i + 1].color;
		out_general[i + 1].st = in_general[i + 1].st;

		out_general[i + 2].color = in_general[i + 2].color;
		out_general[i + 2].st = in_general[i + 2].st;

		out_general[i + 3].color = in_general[i + 3].color;
		out_general[i + 3].st = in_general[i + 3].st;

		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		out_tangents[1] = (wq[1].nq * in_tangents[1]);
		out_tangents[2] = (wq[2].nq * in_tangents[2]);
		out_tangents[3] = (wq[3].nq * in_tangents[3]);

		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		if (out_tangents[1].w < 0.f) out_tangents[1] = -out_tangents[1];
		if (out_tangents[2].w < 0.f) out_tangents[2] = -out_tangents[2];
		if (out_tangents[3].w < 0.f) out_tangents[3] = -out_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_packed_tangents[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out_packed_tangents[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out_packed_tangents[i + 3] = SPipTangents(out_tangents[3], flip[3]);
	}
	for (size_t i = (count & ~3); i < count; ++i)
	{
		wq[0] = DualQuatA(ZERO);
		in_tangents[0] = in_packed_tangents[i + 0].GetQ();
		switch (maxSpines)
		{
		case 4:
			wq[0] += bones[weights[i].boneIds[3]] * (weights[i].weights[3] / 255.f);
		case 3:
			wq[0] += bones[weights[i].boneIds[2]] * (weights[i].weights[2] / 255.f);
		case 2:
			wq[0] += bones[weights[i].boneIds[1]] * (weights[i].weights[1] / 255.f);
		case 1:
			wq[0] += bones[weights[i].boneIds[0]] * (weights[i].weights[0] / 255.f);
		}
		l[0] = wq[0].nq.GetLength();
		l[0] = l[0] > 0.f ? 1.f / (l[0]) : l[0];
		wq[0].nq *= l[0];
		wq[0].dq *= l[0];

		out_general[i + 0].xyz = wq[0] * (in_general[i + 0].xyz * fScale);
		out_general[i + 0].color = in_general[i + 0].color;
		out_general[i + 0].st = in_general[i + 0].st;
		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
	}
#endif
}

static inline void UpdateGeneralTangentsNormals(
  SVF_P3S_C4B_T2S* out_general
  , SPipTangents* out_packed_tangents
  , Vec3f16* out_normals
  , SVF_P3F_C4B_T2F* in_general
  , SPipQTangents* in_packed_tangents
  , Vec3* in_normals
  , DualQuatA* bones
  , SMMRMBoneMapping* weights
  , const float fScale
  , size_t maxSpines
  , size_t count)
{
	MMRM_PROFILE_FUNCTION();
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
	using namespace NVMath;
	DualQuatA wq[4];
	__m128 _wq[4 * 2];
	__m128 vweights[4], vw[4];
	__m128 nq_len[4];
	__m128 vOne = _mm_set1_ps(1.f);
	int16 flip[4];
	__m128 ot[4];
	CRY_ALIGN(16) Quat out_tangents[4];
	CRY_ALIGN(16) Quat in_tangents[4];
	#if MMRM_UNROLL_GEOMETRY_BAKING_LOOPS
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		_wq[0] = _wq[1] = _wq[2] = _wq[3] = _wq[4] = _wq[5] = _wq[6] = _wq[7] = _mm_xor_ps(_wq[0], _wq[0]);

		in_tangents[0] = in_packed_tangents[i + 0].GetQ();
		in_tangents[1] = in_packed_tangents[i + 1].GetQ();
		in_tangents[2] = in_packed_tangents[i + 2].GetQ();
		in_tangents[3] = in_packed_tangents[i + 3].GetQ();

		vweights[0] = _mm_load_ps(&weights[i + 0].weights[0]);
		vweights[1] = _mm_load_ps(&weights[i + 1].weights[0]);
		vweights[2] = _mm_load_ps(&weights[i + 2].weights[0]);
		vweights[3] = _mm_load_ps(&weights[i + 3].weights[0]);
		switch (maxSpines)
		{
		case 4:
			vw[0] = Swizzle<wwww>(vweights[0]);
			vw[1] = Swizzle<wwww>(vweights[1]);
			vw[2] = Swizzle<wwww>(vweights[2]);
			vw[3] = Swizzle<wwww>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[3]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[3]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[3]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[3]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[3]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[3]].dq), vw[3]));
		case 3:
			vw[0] = Swizzle<zzzz>(vweights[0]);
			vw[1] = Swizzle<zzzz>(vweights[1]);
			vw[2] = Swizzle<zzzz>(vweights[2]);
			vw[3] = Swizzle<zzzz>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[2]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[2]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[2]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[2]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[2]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[2]].dq), vw[3]));
		case 2:
			vw[0] = Swizzle<yyyy>(vweights[0]);
			vw[1] = Swizzle<yyyy>(vweights[1]);
			vw[2] = Swizzle<yyyy>(vweights[2]);
			vw[3] = Swizzle<yyyy>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[1]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[1]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[1]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[1]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[1]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[1]].dq), vw[3]));
		case 1:
			vw[0] = Swizzle<xxxx>(vweights[0]);
			vw[1] = Swizzle<xxxx>(vweights[1]);
			vw[2] = Swizzle<xxxx>(vweights[2]);
			vw[3] = Swizzle<xxxx>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[0]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 1].boneIds[0]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[0]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 2].boneIds[0]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[0]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 3].boneIds[0]].dq), vw[3]));
		}

		nq_len[0] = _mm_rsqrt_ps(DotProduct4(_wq[0 * 2 + 0], _wq[0 * 2 + 0]));
		nq_len[1] = _mm_rsqrt_ps(DotProduct4(_wq[1 * 2 + 0], _wq[1 * 2 + 0]));
		nq_len[2] = _mm_rsqrt_ps(DotProduct4(_wq[2 * 2 + 0], _wq[2 * 2 + 0]));
		nq_len[3] = _mm_rsqrt_ps(DotProduct4(_wq[3 * 2 + 0], _wq[3 * 2 + 0]));

		_mm_store_ps((float*)&wq[0].nq, _mm_mul_ps(_wq[0 * 2 + 0], nq_len[0]));
		_mm_store_ps((float*)&wq[0].dq, _mm_mul_ps(_wq[0 * 2 + 1], nq_len[0]));
		_mm_store_ps((float*)&wq[1].nq, _mm_mul_ps(_wq[1 * 2 + 0], nq_len[1]));
		_mm_store_ps((float*)&wq[1].dq, _mm_mul_ps(_wq[1 * 2 + 1], nq_len[1]));
		_mm_store_ps((float*)&wq[2].nq, _mm_mul_ps(_wq[2 * 2 + 0], nq_len[2]));
		_mm_store_ps((float*)&wq[2].dq, _mm_mul_ps(_wq[2 * 2 + 1], nq_len[2]));
		_mm_store_ps((float*)&wq[3].nq, _mm_mul_ps(_wq[3 * 2 + 0], nq_len[3]));
		_mm_store_ps((float*)&wq[3].dq, _mm_mul_ps(_wq[3 * 2 + 1], nq_len[3]));

		CvtToHalf(out_general[i + 0].xyz, wq[0] * (in_general[i + 0].xyz * fScale));
		out_general[i + 0].color = in_general[i + 0].color;
		CvtToHalf(out_general[i + 0].st, in_general[i + 0].st);

		CvtToHalf(out_general[i + 1].xyz, wq[1] * (in_general[i + 1].xyz * fScale));
		out_general[i + 1].color = in_general[i + 1].color;
		CvtToHalf(out_general[i + 1].st, in_general[i + 1].st);

		CvtToHalf(out_general[i + 2].xyz, wq[2] * (in_general[i + 2].xyz * fScale));
		out_general[i + 2].color = in_general[i + 2].color;
		CvtToHalf(out_general[i + 2].st, in_general[i + 2].st);

		CvtToHalf(out_general[i + 3].xyz, wq[3] * (in_general[i + 3].xyz * fScale));
		out_general[i + 3].color = in_general[i + 3].color;
		CvtToHalf(out_general[i + 3].st, in_general[i + 3].st);

		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		out_tangents[1] = (wq[1].nq * in_tangents[1]);
		out_tangents[2] = (wq[2].nq * in_tangents[2]);
		out_tangents[3] = (wq[3].nq * in_tangents[3]);

		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		if (out_tangents[1].w < 0.f) out_tangents[1] = -out_tangents[1];
		if (out_tangents[2].w < 0.f) out_tangents[2] = -out_tangents[2];
		if (out_tangents[3].w < 0.f) out_tangents[3] = -out_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_packed_tangents[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out_packed_tangents[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out_packed_tangents[i + 3] = SPipTangents(out_tangents[3], flip[3]);

		CvtToHalf(out_normals[i + 0], wq[0].nq * in_normals[i + 0]);
		CvtToHalf(out_normals[i + 1], wq[0].nq * in_normals[i + 1]);
		CvtToHalf(out_normals[i + 2], wq[0].nq * in_normals[i + 2]);
		CvtToHalf(out_normals[i + 3], wq[0].nq * in_normals[i + 3]);
	}
	for (size_t i = (count & ~3); i < count; ++i)
	#else // MMRM_UNROLL_GEOMETRY_BAKING_LOOPS
	for (size_t i = 0; i < count; ++i)
	#endif
	{
		_wq[0] = _wq[1] = _mm_xor_ps(_wq[0], _wq[0]);
		in_tangents[0] = in_packed_tangents[i + 0].GetQ();
		vweights[0] = _mm_load_ps(&weights[i + 0].weights[0]);
		switch (maxSpines)
		{
		case 4:
			vw[0] = Swizzle<wwww>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[3]].dq), vw[0]));
		case 3:
			vw[0] = Swizzle<zzzz>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[2]].dq), vw[0]));
		case 2:
			vw[0] = Swizzle<yyyy>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[1]].dq), vw[0]));
		case 1:
			vw[0] = Swizzle<xxxx>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[weights[i + 0].boneIds[0]].dq), vw[0]));
		}
		nq_len[0] = _mm_rsqrt_ps(DotProduct4(_wq[0 * 2 + 0], _wq[0 * 2 + 0]));
		_mm_store_ps((float*)&wq[0].nq, _mm_mul_ps(_wq[0 * 2 + 0], nq_len[0]));
		_mm_store_ps((float*)&wq[0].dq, _mm_mul_ps(_wq[0 * 2 + 1], nq_len[0]));
		CvtToHalf(out_general[i + 0].xyz, wq[0] * (in_general[i + 0].xyz * fScale));
		out_general[i + 0].color = in_general[i + 0].color;
		CvtToHalf(out_general[i + 0].st, in_general[i + 0].st);
		out_tangents[0] = (wq[0].nq * in_tangents[0]);

		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);

		CvtToHalf(out_normals[i + 0], wq[0].nq * in_normals[i + 0]);
	}
#else
	float l[4];
	Quat out_tangents[4];
	Quat in_tangents[4];
	DualQuatA wq[4];
	int16 flip[4];
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		wq[0] = wq[1] = wq[2] = wq[3] = DualQuatA(ZERO);
		in_tangents[0] = in_packed_tangents[i + 0].GetQ();
		in_tangents[1] = in_packed_tangents[i + 1].GetQ();
		in_tangents[2] = in_packed_tangents[i + 2].GetQ();
		in_tangents[3] = in_packed_tangents[i + 3].GetQ();
		switch (maxSpines)
		{
		case 4:
			wq[0] += bones[weights[i + 0].boneIds[3]] * (weights[i + 0].weights[3] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[3]] * (weights[i + 1].weights[3] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[3]] * (weights[i + 2].weights[3] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[3]] * (weights[i + 3].weights[3] / 255.f);
		case 3:
			wq[0] += bones[weights[i + 0].boneIds[2]] * (weights[i + 0].weights[2] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[2]] * (weights[i + 1].weights[2] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[2]] * (weights[i + 2].weights[2] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[2]] * (weights[i + 3].weights[2] / 255.f);
		case 2:
			wq[0] += bones[weights[i + 0].boneIds[1]] * (weights[i + 0].weights[1] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[1]] * (weights[i + 1].weights[1] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[1]] * (weights[i + 2].weights[1] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[1]] * (weights[i + 3].weights[1] / 255.f);
		case 1:
			wq[0] += bones[weights[i + 0].boneIds[0]] * (weights[i + 0].weights[0] / 255.f);
			wq[1] += bones[weights[i + 1].boneIds[0]] * (weights[i + 1].weights[0] / 255.f);
			wq[2] += bones[weights[i + 2].boneIds[0]] * (weights[i + 2].weights[0] / 255.f);
			wq[3] += bones[weights[i + 3].boneIds[0]] * (weights[i + 3].weights[0] / 255.f);
		}

		l[0] = wq[0].nq.GetLength();
		l[1] = wq[1].nq.GetLength();
		l[2] = wq[2].nq.GetLength();
		l[3] = wq[3].nq.GetLength();

		l[0] = l[0] > 0.f ? 1.f / (l[0]) : l[0];
		l[1] = l[1] > 0.f ? 1.f / (l[1]) : l[1];
		l[2] = l[2] > 0.f ? 1.f / (l[2]) : l[2];
		l[3] = l[3] > 0.f ? 1.f / (l[3]) : l[3];

		wq[0].nq *= l[0];
		wq[0].dq *= l[0];
		wq[1].nq *= l[1];
		wq[1].dq *= l[1];
		wq[2].nq *= l[2];
		wq[2].dq *= l[2];
		wq[3].nq *= l[3];
		wq[3].dq *= l[3];

		out_general[i + 0].xyz = wq[0] * (in_general[i + 0].xyz * fScale);
		out_general[i + 1].xyz = wq[1] * (in_general[i + 1].xyz * fScale);
		out_general[i + 2].xyz = wq[2] * (in_general[i + 2].xyz * fScale);
		out_general[i + 3].xyz = wq[3] * (in_general[i + 3].xyz * fScale);

		out_general[i + 0].color = in_general[i + 0].color;
		out_general[i + 0].st = in_general[i + 0].st;

		out_general[i + 1].color = in_general[i + 1].color;
		out_general[i + 1].st = in_general[i + 1].st;

		out_general[i + 2].color = in_general[i + 2].color;
		out_general[i + 2].st = in_general[i + 2].st;

		out_general[i + 3].color = in_general[i + 3].color;
		out_general[i + 3].st = in_general[i + 3].st;

		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		out_tangents[1] = (wq[1].nq * in_tangents[1]);
		out_tangents[2] = (wq[2].nq * in_tangents[2]);
		out_tangents[3] = (wq[3].nq * in_tangents[3]);

		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		if (out_tangents[1].w < 0.f) out_tangents[1] = -out_tangents[1];
		if (out_tangents[2].w < 0.f) out_tangents[2] = -out_tangents[2];
		if (out_tangents[3].w < 0.f) out_tangents[3] = -out_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_packed_tangents[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out_packed_tangents[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out_packed_tangents[i + 3] = SPipTangents(out_tangents[3], flip[3]);

		out_normals[i + 0] = wq[0].nq * in_normals[i + 0];
		out_normals[i + 0] = wq[0].nq * in_normals[i + 0];
		out_normals[i + 0] = wq[0].nq * in_normals[i + 0];
		out_normals[i + 0] = wq[0].nq * in_normals[i + 0];
	}
	for (size_t i = (count & ~3); i < count; ++i)
	{
		wq[0] = DualQuatA(ZERO);
		in_tangents[0] = in_packed_tangents[i + 0].GetQ();
		switch (maxSpines)
		{
		case 4:
			wq[0] += bones[weights[i].boneIds[3]] * (weights[i].weights[3] / 255.f);
		case 3:
			wq[0] += bones[weights[i].boneIds[2]] * (weights[i].weights[2] / 255.f);
		case 2:
			wq[0] += bones[weights[i].boneIds[1]] * (weights[i].weights[1] / 255.f);
		case 1:
			wq[0] += bones[weights[i].boneIds[0]] * (weights[i].weights[0] / 255.f);
		}
		l[0] = wq[0].nq.GetLength();
		l[0] = l[0] > 0.f ? 1.f / (l[0]) : l[0];
		wq[0].nq *= l[0];
		wq[0].dq *= l[0];

		out_general[i + 0].xyz = wq[0] * (in_general[i + 0].xyz * fScale);
		out_general[i + 0].color = in_general[i + 0].color;
		out_general[i + 0].st = in_general[i + 0].st;
		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_normals[i + 0] = wq[0].nq * in_normals[i + 0];
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////
// Update a set of general, tangents and normals
static inline void UpdateGeneralTangents(
  SVF_P3S_C4B_T2S* out_general
  , SPipTangents* out_packed_tangents
  , SMMRMSkinVertex* in
  , DualQuatA* bones
  , const float fScale
  , size_t maxSpines
  , size_t count)
{
	MMRM_PROFILE_FUNCTION();
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
	using namespace NVMath;
	DualQuatA wq[4];
	__m128 _wq[4 * 2];
	__m128 vweights[4], vw[4];
	__m128 nq_len[4];
	__m128 vOne = _mm_set1_ps(1.f);
	int16 flip[4];
	__m128 ot[4];
	CRY_ALIGN(16) Quat out_tangents[4];
	CRY_ALIGN(16) Quat in_tangents[4];
	#if MMRM_UNROLL_GEOMETRY_BAKING_LOOPS
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		_wq[0] = _wq[1] = _wq[2] = _wq[3] = _wq[4] = _wq[5] = _wq[6] = _wq[7] = _mm_xor_ps(_wq[0], _wq[0]);

		in_tangents[0] = in[i + 0].qt.GetQ();
		in_tangents[1] = in[i + 1].qt.GetQ();
		in_tangents[2] = in[i + 2].qt.GetQ();
		in_tangents[3] = in[i + 3].qt.GetQ();

		vweights[0] = _mm_load_ps(&in[i + 0].weights[0]);
		vweights[1] = _mm_load_ps(&in[i + 1].weights[0]);
		vweights[2] = _mm_load_ps(&in[i + 2].weights[0]);
		vweights[3] = _mm_load_ps(&in[i + 3].weights[0]);
		switch (maxSpines)
		{
		case 4:
			vw[0] = Swizzle<wwww>(vweights[0]);
			vw[1] = Swizzle<wwww>(vweights[1]);
			vw[2] = Swizzle<wwww>(vweights[2]);
			vw[3] = Swizzle<wwww>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[3]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[3]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[3]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[3]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[3]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[3]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[3]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[3]].dq), vw[3]));
		case 3:
			vw[0] = Swizzle<zzzz>(vweights[0]);
			vw[1] = Swizzle<zzzz>(vweights[1]);
			vw[2] = Swizzle<zzzz>(vweights[2]);
			vw[3] = Swizzle<zzzz>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[2]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[2]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[2]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[2]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[2]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[2]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[2]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[2]].dq), vw[3]));
		case 2:
			vw[0] = Swizzle<yyyy>(vweights[0]);
			vw[1] = Swizzle<yyyy>(vweights[1]);
			vw[2] = Swizzle<yyyy>(vweights[2]);
			vw[3] = Swizzle<yyyy>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[1]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[1]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[1]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[1]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[1]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[1]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[1]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[1]].dq), vw[3]));
		case 1:
			vw[0] = Swizzle<xxxx>(vweights[0]);
			vw[1] = Swizzle<xxxx>(vweights[1]);
			vw[2] = Swizzle<xxxx>(vweights[2]);
			vw[3] = Swizzle<xxxx>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[0]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[0]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[0]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[0]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[0]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[0]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[0]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[0]].dq), vw[3]));
		}

		nq_len[0] = _mm_rsqrt_ps(DotProduct4(_wq[0 * 2 + 0], _wq[0 * 2 + 0]));
		nq_len[1] = _mm_rsqrt_ps(DotProduct4(_wq[1 * 2 + 0], _wq[1 * 2 + 0]));
		nq_len[2] = _mm_rsqrt_ps(DotProduct4(_wq[2 * 2 + 0], _wq[2 * 2 + 0]));
		nq_len[3] = _mm_rsqrt_ps(DotProduct4(_wq[3 * 2 + 0], _wq[3 * 2 + 0]));

		_mm_store_ps((float*)&wq[0].nq, _mm_mul_ps(_wq[0 * 2 + 0], nq_len[0]));
		_mm_store_ps((float*)&wq[0].dq, _mm_mul_ps(_wq[0 * 2 + 1], nq_len[0]));
		_mm_store_ps((float*)&wq[1].nq, _mm_mul_ps(_wq[1 * 2 + 0], nq_len[1]));
		_mm_store_ps((float*)&wq[1].dq, _mm_mul_ps(_wq[1 * 2 + 1], nq_len[1]));
		_mm_store_ps((float*)&wq[2].nq, _mm_mul_ps(_wq[2 * 2 + 0], nq_len[2]));
		_mm_store_ps((float*)&wq[2].dq, _mm_mul_ps(_wq[2 * 2 + 1], nq_len[2]));
		_mm_store_ps((float*)&wq[3].nq, _mm_mul_ps(_wq[3 * 2 + 0], nq_len[3]));
		_mm_store_ps((float*)&wq[3].dq, _mm_mul_ps(_wq[3 * 2 + 1], nq_len[3]));

		CvtToHalf(out_general[i + 0].xyz, wq[0] * (in[i + 0].pos * fScale));
		out_general[i + 0].color = in[i + 0].colour;
		CvtToHalf(out_general[i + 0].st, in[i + 0].uv);

		CvtToHalf(out_general[i + 1].xyz, wq[1] * (in[i + 1].pos * fScale));
		out_general[i + 1].color = in[i + 1].colour;
		CvtToHalf(out_general[i + 1].st, in[i + 1].uv);

		CvtToHalf(out_general[i + 2].xyz, wq[2] * (in[i + 2].pos * fScale));
		out_general[i + 2].color = in[i + 2].colour;
		CvtToHalf(out_general[i + 2].st, in[i + 2].uv);

		CvtToHalf(out_general[i + 3].xyz, wq[3] * (in[i + 3].pos * fScale));
		out_general[i + 3].color = in[i + 3].colour;
		CvtToHalf(out_general[i + 3].st, in[i + 3].uv);

		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		out_tangents[1] = (wq[1].nq * in_tangents[1]);
		out_tangents[2] = (wq[2].nq * in_tangents[2]);
		out_tangents[3] = (wq[3].nq * in_tangents[3]);

		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		if (out_tangents[1].w < 0.f) out_tangents[1] = -out_tangents[1];
		if (out_tangents[2].w < 0.f) out_tangents[2] = -out_tangents[2];
		if (out_tangents[3].w < 0.f) out_tangents[3] = -out_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_packed_tangents[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out_packed_tangents[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out_packed_tangents[i + 3] = SPipTangents(out_tangents[3], flip[3]);
	}
	for (size_t i = (count & ~3); i < count; ++i)
	#else // MMRM_UNROLL_GEOMETRY_BAKING_LOOPS
	for (size_t i = 0; i < count; ++i)
	#endif
	{
		//_mm_prefetch((const char*)&in[i+4], _MM_HINT_T0);
		_wq[0] = _wq[1] = _mm_xor_ps(_wq[0], _wq[0]);
		in_tangents[0] = in[i + 0].qt.GetQ();
		vweights[0] = _mm_load_ps(&in[i + 0].weights[0]);
		switch (maxSpines)
		{
		case 4:
			vw[0] = Swizzle<wwww>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[3]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[3]].dq), vw[0]));
		case 3:
			vw[0] = Swizzle<zzzz>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[2]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[2]].dq), vw[0]));
		case 2:
			vw[0] = Swizzle<yyyy>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[1]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[1]].dq), vw[0]));
		case 1:
			vw[0] = Swizzle<xxxx>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[0]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[0]].dq), vw[0]));
		}
		__m128 dp = DotProduct4(_wq[0 * 2 + 0], _wq[0 * 2 + 0]);
		__m128 len = _mm_rsqrt_ps(dp);
		nq_len[0] = _mm_mul_ps(_mm_mul_ps(_mm_set1_ps(0.5f), len), _mm_sub_ps(_mm_set1_ps(3.0f), _mm_mul_ps(_mm_mul_ps(dp, len), len))); // Newton-Rhapson for improved accuracy
		_mm_store_ps((float*)&wq[0].nq, _mm_mul_ps(_wq[0 * 2 + 0], nq_len[0]));
		_mm_store_ps((float*)&wq[0].dq, _mm_mul_ps(_wq[0 * 2 + 1], nq_len[0]));
		CvtToHalf(out_general[i + 0].xyz, wq[0] * (in[i + 0].pos * fScale));
		out_general[i + 0].color = in[i + 0].colour;
		CvtToHalf(out_general[i + 0].st, in[i + 0].uv);
		out_tangents[0] = (wq[0].nq * in_tangents[0]);

		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
	}
#else
	float l[4];
	Quat out_tangents[4];
	Quat in_tangents[4];
	DualQuatA wq[4];
	int16 flip[4];
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		wq[0] = wq[1] = wq[2] = wq[3] = DualQuatA(ZERO);
		in_tangents[0] = in[i + 0].qt.GetQ();
		in_tangents[1] = in[i + 1].qt.GetQ();
		in_tangents[2] = in[i + 2].qt.GetQ();
		in_tangents[3] = in[i + 3].qt.GetQ();
		switch (maxSpines)
		{
		case 4:
			wq[0] += bones[in[i + 0].boneIds[3]] * (in[i + 0].weights[3] / 255.f);
			wq[1] += bones[in[i + 1].boneIds[3]] * (in[i + 1].weights[3] / 255.f);
			wq[2] += bones[in[i + 2].boneIds[3]] * (in[i + 2].weights[3] / 255.f);
			wq[3] += bones[in[i + 3].boneIds[3]] * (in[i + 3].weights[3] / 255.f);
		case 3:
			wq[0] += bones[in[i + 0].boneIds[2]] * (in[i + 0].weights[2] / 255.f);
			wq[1] += bones[in[i + 1].boneIds[2]] * (in[i + 1].weights[2] / 255.f);
			wq[2] += bones[in[i + 2].boneIds[2]] * (in[i + 2].weights[2] / 255.f);
			wq[3] += bones[in[i + 3].boneIds[2]] * (in[i + 3].weights[2] / 255.f);
		case 2:
			wq[0] += bones[in[i + 0].boneIds[1]] * (in[i + 0].weights[1] / 255.f);
			wq[1] += bones[in[i + 1].boneIds[1]] * (in[i + 1].weights[1] / 255.f);
			wq[2] += bones[in[i + 2].boneIds[1]] * (in[i + 2].weights[1] / 255.f);
			wq[3] += bones[in[i + 3].boneIds[1]] * (in[i + 3].weights[1] / 255.f);
		case 1:
			wq[0] += bones[in[i + 0].boneIds[0]] * (in[i + 0].weights[0] / 255.f);
			wq[1] += bones[in[i + 1].boneIds[0]] * (in[i + 1].weights[0] / 255.f);
			wq[2] += bones[in[i + 2].boneIds[0]] * (in[i + 2].weights[0] / 255.f);
			wq[3] += bones[in[i + 3].boneIds[0]] * (in[i + 3].weights[0] / 255.f);
		}

		l[0] = wq[0].nq.GetLength();
		l[1] = wq[1].nq.GetLength();
		l[2] = wq[2].nq.GetLength();
		l[3] = wq[3].nq.GetLength();

		l[0] = l[0] > 0.f ? 1.f / (l[0]) : l[0];
		l[1] = l[1] > 0.f ? 1.f / (l[1]) : l[1];
		l[2] = l[2] > 0.f ? 1.f / (l[2]) : l[2];
		l[3] = l[3] > 0.f ? 1.f / (l[3]) : l[3];

		wq[0].nq *= l[0];
		wq[0].dq *= l[0];
		wq[1].nq *= l[1];
		wq[1].dq *= l[1];
		wq[2].nq *= l[2];
		wq[2].dq *= l[2];
		wq[3].nq *= l[3];
		wq[3].dq *= l[3];

		out_general[i + 0].xyz = wq[0] * (in[i + 0].pos * fScale);
		out_general[i + 1].xyz = wq[1] * (in[i + 1].pos * fScale);
		out_general[i + 2].xyz = wq[2] * (in[i + 2].pos * fScale);
		out_general[i + 3].xyz = wq[3] * (in[i + 3].pos * fScale);

		out_general[i + 0].color = in[i + 0].colour;
		out_general[i + 0].st = in[i + 0].uv;

		out_general[i + 1].color = in[i + 1].colour;
		out_general[i + 1].st = in[i + 1].uv;

		out_general[i + 2].color = in[i + 2].colour;
		out_general[i + 2].st = in[i + 2].uv;

		out_general[i + 3].color = in[i + 3].colour;
		out_general[i + 3].st = in[i + 3].uv;

		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		out_tangents[1] = (wq[1].nq * in_tangents[1]);
		out_tangents[2] = (wq[2].nq * in_tangents[2]);
		out_tangents[3] = (wq[3].nq * in_tangents[3]);

		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		if (out_tangents[1].w < 0.f) out_tangents[1] = -out_tangents[1];
		if (out_tangents[2].w < 0.f) out_tangents[2] = -out_tangents[2];
		if (out_tangents[3].w < 0.f) out_tangents[3] = -out_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_packed_tangents[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out_packed_tangents[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out_packed_tangents[i + 3] = SPipTangents(out_tangents[3], flip[3]);
	}
	for (size_t i = (count & ~3); i < count; ++i)
	{
		wq[0] = DualQuatA(ZERO);
		in_tangents[0] = in[i + 0].qt.GetQ();
		switch (maxSpines)
		{
		case 4:
			wq[0] += bones[in[i].boneIds[3]] * (in[i].weights[3] / 255.f);
		case 3:
			wq[0] += bones[in[i].boneIds[2]] * (in[i].weights[2] / 255.f);
		case 2:
			wq[0] += bones[in[i].boneIds[1]] * (in[i].weights[1] / 255.f);
		case 1:
			wq[0] += bones[in[i].boneIds[0]] * (in[i].weights[0] / 255.f);
		}
		l[0] = wq[0].nq.GetLength();
		l[0] = l[0] > 0.f ? 1.f / (l[0]) : l[0];
		wq[0].nq *= l[0];
		wq[0].dq *= l[0];

		out_general[i + 0].xyz = wq[0] * (in[i + 0].pos * fScale);
		out_general[i + 0].color = in[i + 0].colour;
		out_general[i + 0].st = in[i + 0].uv;
		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
	}
#endif
}

static inline void UpdateGeneralTangentsNormals(
  SVF_P3S_C4B_T2S* out_general
  , SPipTangents* out_packed_tangents
  , Vec3f16* out_normals
  , SMMRMSkinVertex* in
  , DualQuatA* bones
  , const float fScale
  , size_t maxSpines
  , size_t count)
{
	MMRM_PROFILE_FUNCTION();
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
	using namespace NVMath;
	DualQuatA wq[4];
	__m128 _wq[4 * 2];
	__m128 vweights[4], vw[4];
	__m128 nq_len[4];
	__m128 vOne = _mm_set1_ps(1.f);
	int16 flip[4];
	__m128 ot[4];
	CRY_ALIGN(16) Quat out_tangents[4];
	CRY_ALIGN(16) Quat in_tangents[4];
	#if MMRM_UNROLL_GEOMETRY_BAKING_LOOPS
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		//_mm_prefetch((const char*)&in[i+4], _MM_HINT_T0);
		//_mm_prefetch((const char*)&in[i+5], _MM_HINT_T0);
		//_mm_prefetch((const char*)&in[i+6], _MM_HINT_T0);
		//_mm_prefetch((const char*)&in[i+7], _MM_HINT_T0);
		_wq[0] = _wq[1] = _wq[2] = _wq[3] = _wq[4] = _wq[5] = _wq[6] = _wq[7] = _mm_xor_ps(_wq[0], _wq[0]);

		in_tangents[0] = in[i + 0].qt.GetQ();
		in_tangents[1] = in[i + 1].qt.GetQ();
		in_tangents[2] = in[i + 2].qt.GetQ();
		in_tangents[3] = in[i + 3].qt.GetQ();

		vweights[0] = _mm_load_ps(&in[i + 0].weights[0]);
		vweights[1] = _mm_load_ps(&in[i + 1].weights[0]);
		vweights[2] = _mm_load_ps(&in[i + 2].weights[0]);
		vweights[3] = _mm_load_ps(&in[i + 3].weights[0]);
		switch (maxSpines)
		{
		case 4:
			vw[0] = Swizzle<wwww>(vweights[0]);
			vw[1] = Swizzle<wwww>(vweights[1]);
			vw[2] = Swizzle<wwww>(vweights[2]);
			vw[3] = Swizzle<wwww>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[3]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[3]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[3]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[3]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[3]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[3]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[3]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[3]].dq), vw[3]));
		case 3:
			vw[0] = Swizzle<zzzz>(vweights[0]);
			vw[1] = Swizzle<zzzz>(vweights[1]);
			vw[2] = Swizzle<zzzz>(vweights[2]);
			vw[3] = Swizzle<zzzz>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[2]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[2]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[2]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[2]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[2]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[2]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[2]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[2]].dq), vw[3]));
		case 2:
			vw[0] = Swizzle<yyyy>(vweights[0]);
			vw[1] = Swizzle<yyyy>(vweights[1]);
			vw[2] = Swizzle<yyyy>(vweights[2]);
			vw[3] = Swizzle<yyyy>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[1]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[1]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[1]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[1]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[1]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[1]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[1]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[1]].dq), vw[3]));
		case 1:
			vw[0] = Swizzle<xxxx>(vweights[0]);
			vw[1] = Swizzle<xxxx>(vweights[1]);
			vw[2] = Swizzle<xxxx>(vweights[2]);
			vw[3] = Swizzle<xxxx>(vweights[3]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[0]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[0]].dq), vw[0]));
			_wq[1 * 2 + 0] = _mm_add_ps(_wq[1 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[0]].nq), vw[1]));
			_wq[1 * 2 + 1] = _mm_add_ps(_wq[1 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 1].boneIds[0]].dq), vw[1]));
			_wq[2 * 2 + 0] = _mm_add_ps(_wq[2 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[0]].nq), vw[2]));
			_wq[2 * 2 + 1] = _mm_add_ps(_wq[2 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 2].boneIds[0]].dq), vw[2]));
			_wq[3 * 2 + 0] = _mm_add_ps(_wq[3 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[0]].nq), vw[3]));
			_wq[3 * 2 + 1] = _mm_add_ps(_wq[3 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 3].boneIds[0]].dq), vw[3]));
		}

		nq_len[0] = _mm_rsqrt_ps(DotProduct4(_wq[0 * 2 + 0], _wq[0 * 2 + 0]));
		nq_len[1] = _mm_rsqrt_ps(DotProduct4(_wq[1 * 2 + 0], _wq[1 * 2 + 0]));
		nq_len[2] = _mm_rsqrt_ps(DotProduct4(_wq[2 * 2 + 0], _wq[2 * 2 + 0]));
		nq_len[3] = _mm_rsqrt_ps(DotProduct4(_wq[3 * 2 + 0], _wq[3 * 2 + 0]));

		_mm_store_ps((float*)&wq[0].nq, _mm_mul_ps(_wq[0 * 2 + 0], nq_len[0]));
		_mm_store_ps((float*)&wq[0].dq, _mm_mul_ps(_wq[0 * 2 + 1], nq_len[0]));
		_mm_store_ps((float*)&wq[1].nq, _mm_mul_ps(_wq[1 * 2 + 0], nq_len[1]));
		_mm_store_ps((float*)&wq[1].dq, _mm_mul_ps(_wq[1 * 2 + 1], nq_len[1]));
		_mm_store_ps((float*)&wq[2].nq, _mm_mul_ps(_wq[2 * 2 + 0], nq_len[2]));
		_mm_store_ps((float*)&wq[2].dq, _mm_mul_ps(_wq[2 * 2 + 1], nq_len[2]));
		_mm_store_ps((float*)&wq[3].nq, _mm_mul_ps(_wq[3 * 2 + 0], nq_len[3]));
		_mm_store_ps((float*)&wq[3].dq, _mm_mul_ps(_wq[3 * 2 + 1], nq_len[3]));

		CvtToHalf(out_general[i + 0].xyz, wq[0] * (in[i + 0].pos * fScale));
		out_general[i + 0].color = in[i + 0].colour;
		CvtToHalf(out_general[i + 0].st, in[i + 0].uv);

		CvtToHalf(out_general[i + 1].xyz, wq[1] * (in[i + 1].pos * fScale));
		out_general[i + 1].color = in[i + 1].colour;
		CvtToHalf(out_general[i + 1].st, in[i + 1].uv);

		CvtToHalf(out_general[i + 2].xyz, wq[2] * (in[i + 2].pos * fScale));
		out_general[i + 2].color = in[i + 2].colour;
		CvtToHalf(out_general[i + 2].st, in[i + 2].uv);

		CvtToHalf(out_general[i + 3].xyz, wq[3] * (in[i + 3].pos * fScale));
		out_general[i + 3].color = in[i + 3].colour;
		CvtToHalf(out_general[i + 3].st, in[i + 3].uv);

		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		out_tangents[1] = (wq[1].nq * in_tangents[1]);
		out_tangents[2] = (wq[2].nq * in_tangents[2]);
		out_tangents[3] = (wq[3].nq * in_tangents[3]);

		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		if (out_tangents[1].w < 0.f) out_tangents[1] = -out_tangents[1];
		if (out_tangents[2].w < 0.f) out_tangents[2] = -out_tangents[2];
		if (out_tangents[3].w < 0.f) out_tangents[3] = -out_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_packed_tangents[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out_packed_tangents[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out_packed_tangents[i + 3] = SPipTangents(out_tangents[3], flip[3]);

		CvtToHalf(out_normals[i + 0], wq[0].nq * in[i + 0].normal);
		CvtToHalf(out_normals[i + 1], wq[0].nq * in[i + 1].normal);
		CvtToHalf(out_normals[i + 2], wq[0].nq * in[i + 2].normal);
		CvtToHalf(out_normals[i + 3], wq[0].nq * in[i + 3].normal);
	}
	for (size_t i = (count & ~3); i < count; ++i)
	#else // MMRM_UNROLL_GEOMETRY_BAKING_LOOPS
	for (size_t i = 0; i < count; ++i)
	#endif
	{
		//_mm_prefetch((const char*)&in[i+4], _MM_HINT_T0);
		_wq[0] = _wq[1] = _mm_xor_ps(_wq[0], _wq[0]);
		in_tangents[0] = in[i + 0].qt.GetQ();
		vweights[0] = _mm_load_ps(&in[i + 0].weights[0]);
		switch (maxSpines)
		{
		case 4:
			vw[0] = Swizzle<wwww>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[3]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[3]].dq), vw[0]));
		case 3:
			vw[0] = Swizzle<zzzz>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[2]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[2]].dq), vw[0]));
		case 2:
			vw[0] = Swizzle<yyyy>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[1]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[1]].dq), vw[0]));
		case 1:
			vw[0] = Swizzle<xxxx>(vweights[0]);
			_wq[0 * 2 + 0] = _mm_add_ps(_wq[0 * 2 + 0], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[0]].nq), vw[0]));
			_wq[0 * 2 + 1] = _mm_add_ps(_wq[0 * 2 + 1], _mm_mul_ps(_mm_load_ps((float*)&bones[in[i + 0].boneIds[0]].dq), vw[0]));
		}
		nq_len[0] = _mm_rsqrt_ps(DotProduct4(_wq[0 * 2 + 0], _wq[0 * 2 + 0]));
		_mm_store_ps((float*)&wq[0].nq, _mm_mul_ps(_wq[0 * 2 + 0], nq_len[0]));
		_mm_store_ps((float*)&wq[0].dq, _mm_mul_ps(_wq[0 * 2 + 1], nq_len[0]));
		CvtToHalf(out_general[i + 0].xyz, wq[0] * (in[i + 0].pos * fScale));
		out_general[i + 0].color = in[i + 0].colour;
		CvtToHalf(out_general[i + 0].st, in[i + 0].uv);
		out_tangents[0] = (wq[0].nq * in_tangents[0]);

		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);

		CvtToHalf(out_normals[i + 0], wq[0].nq * in[i + 0].normal);
	}
#else
	float l[4];
	Quat out_tangents[4];
	Quat in_tangents[4];
	DualQuatA wq[4];
	int16 flip[4];
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		wq[0] = wq[1] = wq[2] = wq[3] = DualQuatA(ZERO);
		in_tangents[0] = in[i + 0].qt.GetQ();
		in_tangents[1] = in[i + 1].qt.GetQ();
		in_tangents[2] = in[i + 2].qt.GetQ();
		in_tangents[3] = in[i + 3].qt.GetQ();
		switch (maxSpines)
		{
		case 4:
			wq[0] += bones[in[i + 0].boneIds[3]] * (in[i + 0].weights[3] / 255.f);
			wq[1] += bones[in[i + 1].boneIds[3]] * (in[i + 1].weights[3] / 255.f);
			wq[2] += bones[in[i + 2].boneIds[3]] * (in[i + 2].weights[3] / 255.f);
			wq[3] += bones[in[i + 3].boneIds[3]] * (in[i + 3].weights[3] / 255.f);
		case 3:
			wq[0] += bones[in[i + 0].boneIds[2]] * (in[i + 0].weights[2] / 255.f);
			wq[1] += bones[in[i + 1].boneIds[2]] * (in[i + 1].weights[2] / 255.f);
			wq[2] += bones[in[i + 2].boneIds[2]] * (in[i + 2].weights[2] / 255.f);
			wq[3] += bones[in[i + 3].boneIds[2]] * (in[i + 3].weights[2] / 255.f);
		case 2:
			wq[0] += bones[in[i + 0].boneIds[1]] * (in[i + 0].weights[1] / 255.f);
			wq[1] += bones[in[i + 1].boneIds[1]] * (in[i + 1].weights[1] / 255.f);
			wq[2] += bones[in[i + 2].boneIds[1]] * (in[i + 2].weights[1] / 255.f);
			wq[3] += bones[in[i + 3].boneIds[1]] * (in[i + 3].weights[1] / 255.f);
		case 1:
			wq[0] += bones[in[i + 0].boneIds[0]] * (in[i + 0].weights[0] / 255.f);
			wq[1] += bones[in[i + 1].boneIds[0]] * (in[i + 1].weights[0] / 255.f);
			wq[2] += bones[in[i + 2].boneIds[0]] * (in[i + 2].weights[0] / 255.f);
			wq[3] += bones[in[i + 3].boneIds[0]] * (in[i + 3].weights[0] / 255.f);
		}

		l[0] = wq[0].nq.GetLength();
		l[1] = wq[1].nq.GetLength();
		l[2] = wq[2].nq.GetLength();
		l[3] = wq[3].nq.GetLength();

		l[0] = l[0] > 0.f ? 1.f / (l[0]) : l[0];
		l[1] = l[1] > 0.f ? 1.f / (l[1]) : l[1];
		l[2] = l[2] > 0.f ? 1.f / (l[2]) : l[2];
		l[3] = l[3] > 0.f ? 1.f / (l[3]) : l[3];

		wq[0].nq *= l[0];
		wq[0].dq *= l[0];
		wq[1].nq *= l[1];
		wq[1].dq *= l[1];
		wq[2].nq *= l[2];
		wq[2].dq *= l[2];
		wq[3].nq *= l[3];
		wq[3].dq *= l[3];

		out_general[i + 0].xyz = wq[0] * (in[i + 0].pos * fScale);
		out_general[i + 1].xyz = wq[1] * (in[i + 1].pos * fScale);
		out_general[i + 2].xyz = wq[2] * (in[i + 2].pos * fScale);
		out_general[i + 3].xyz = wq[3] * (in[i + 3].pos * fScale);

		out_general[i + 0].color = in[i + 0].colour;
		out_general[i + 0].st = in[i + 0].uv;

		out_general[i + 1].color = in[i + 1].colour;
		out_general[i + 1].st = in[i + 1].uv;

		out_general[i + 2].color = in[i + 2].colour;
		out_general[i + 2].st = in[i + 2].uv;

		out_general[i + 3].color = in[i + 3].colour;
		out_general[i + 3].st = in[i + 3].uv;

		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		out_tangents[1] = (wq[1].nq * in_tangents[1]);
		out_tangents[2] = (wq[2].nq * in_tangents[2]);
		out_tangents[3] = (wq[3].nq * in_tangents[3]);

		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		if (out_tangents[1].w < 0.f) out_tangents[1] = -out_tangents[1];
		if (out_tangents[2].w < 0.f) out_tangents[2] = -out_tangents[2];
		if (out_tangents[3].w < 0.f) out_tangents[3] = -out_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_packed_tangents[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out_packed_tangents[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out_packed_tangents[i + 3] = SPipTangents(out_tangents[3], flip[3]);

		out_normals[i + 0] = wq[0].nq * in[i + 0].normal;
		out_normals[i + 0] = wq[0].nq * in[i + 0].normal;
		out_normals[i + 0] = wq[0].nq * in[i + 0].normal;
		out_normals[i + 0] = wq[0].nq * in[i + 0].normal;
	}
	for (size_t i = (count & ~3); i < count; ++i)
	{
		wq[0] = DualQuatA(ZERO);
		in_tangents[0] = in[i + 0].qt.GetQ();
		switch (maxSpines)
		{
		case 4:
			wq[0] += bones[in[i].boneIds[3]] * (in[i].weights[3] / 255.f);
		case 3:
			wq[0] += bones[in[i].boneIds[2]] * (in[i].weights[2] / 255.f);
		case 2:
			wq[0] += bones[in[i].boneIds[1]] * (in[i].weights[1] / 255.f);
		case 1:
			wq[0] += bones[in[i].boneIds[0]] * (in[i].weights[0] / 255.f);
		}
		l[0] = wq[0].nq.GetLength();
		l[0] = l[0] > 0.f ? 1.f / (l[0]) : l[0];
		wq[0].nq *= l[0];
		wq[0].dq *= l[0];

		out_general[i + 0].xyz = wq[0] * (in[i + 0].pos * fScale);
		out_general[i + 0].color = in[i + 0].colour;
		out_general[i + 0].st = in[i + 0].uv;
		out_tangents[0] = (wq[0].nq * in_tangents[0]);
		if (out_tangents[0].w < 0.f) out_tangents[0] = -out_tangents[0];
		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_normals[i + 0] = wq[0].nq * in[i + 0].normal;
	}
#endif
}

static inline void UpdateGeneralTangentsNormals(
  SVF_P3S_C4B_T2S* out_general
  , SPipTangents* out_packed_tangents
  , Vec3f16* out_normals
  , SVF_P3F_C4B_T2F* in_general
  , SPipQTangents* in_packed_tangents
  , Vec3* in_normals
  , size_t count
  , const Matrix34& wmat)
{
	MMRM_PROFILE_FUNCTION();
	Quat out_tangents[4];
	Quat in_tangents[4];
	int16 flip[4];
	Quat q = mat33_to_quat(Matrix33(wmat));
	q.Normalize();

	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		out_general[i + 0].xyz = wmat * in_general[i + 0].xyz;
		out_general[i + 0].color = in_general[i + 0].color;
		out_general[i + 0].st = in_general[i + 0].st;
		out_general[i + 1].xyz = wmat * in_general[i + 1].xyz;
		out_general[i + 1].color = in_general[i + 1].color;
		out_general[i + 1].st = in_general[i + 1].st;
		out_general[i + 2].xyz = wmat * in_general[i + 2].xyz;
		out_general[i + 2].color = in_general[i + 2].color;
		out_general[i + 2].st = in_general[i + 2].st;
		out_general[i + 3].xyz = wmat * in_general[i + 3].xyz;
		out_general[i + 3].color = in_general[i + 3].color;
		out_general[i + 3].st = in_general[i + 3].st;

		in_tangents[0] = in_packed_tangents[i + 0].GetQ();
		in_tangents[1] = in_packed_tangents[i + 1].GetQ();
		in_tangents[2] = in_packed_tangents[i + 2].GetQ();
		in_tangents[3] = in_packed_tangents[i + 3].GetQ();

		out_tangents[0] = q * in_tangents[0];
		out_tangents[1] = q * in_tangents[1];
		out_tangents[2] = q * in_tangents[2];
		out_tangents[3] = q * in_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_packed_tangents[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out_packed_tangents[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out_packed_tangents[i + 3] = SPipTangents(out_tangents[3], flip[3]);

		out_normals[i + 0] = in_normals[i + 0];
		out_normals[i + 1] = in_normals[i + 1];
		out_normals[i + 2] = in_normals[i + 2];
		out_normals[i + 3] = in_normals[i + 3];
	}
	for (size_t i = (count & ~3); i < count; ++i)
	{
		out_general[i].xyz = wmat * in_general[i].xyz;
		out_general[i].color = in_general[i].color;
		out_general[i].st = in_general[i].st;

		in_tangents[0] = in_packed_tangents[i + 0].GetQ();
		out_tangents[0] = q * in_tangents[0];
		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);

		out_normals[i + 0] = in_normals[i + 0];
	}
}

static inline void UpdateGeneralTangents(
  SVF_P3S_C4B_T2S* out_general
  , SPipTangents* out_packed_tangents
  , SVF_P3F_C4B_T2F* in_general
  , SPipQTangents* in_packed_tangents
  , size_t count
  , const Matrix34& wmat)
{
	MMRM_PROFILE_FUNCTION();
	Quat out_tangents[4];
	Quat in_tangents[4];
	int16 flip[4];
	Quat q = mat33_to_quat(Matrix33(wmat));
	q.Normalize();
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		out_general[i + 0].xyz = wmat * in_general[i + 0].xyz;
		out_general[i + 0].color = in_general[i + 0].color;
		out_general[i + 0].st = in_general[i + 0].st;
		out_general[i + 1].xyz = wmat * in_general[i + 1].xyz;
		out_general[i + 1].color = in_general[i + 1].color;
		out_general[i + 1].st = in_general[i + 1].st;
		out_general[i + 2].xyz = wmat * in_general[i + 2].xyz;
		out_general[i + 2].color = in_general[i + 2].color;
		out_general[i + 2].st = in_general[i + 2].st;
		out_general[i + 3].xyz = wmat * in_general[i + 3].xyz;
		out_general[i + 3].color = in_general[i + 3].color;
		out_general[i + 3].st = in_general[i + 3].st;

		in_tangents[0] = in_packed_tangents[i + 0].GetQ();
		in_tangents[1] = in_packed_tangents[i + 1].GetQ();
		in_tangents[2] = in_packed_tangents[i + 2].GetQ();
		in_tangents[3] = in_packed_tangents[i + 3].GetQ();

		out_tangents[0] = q * in_tangents[0];
		out_tangents[1] = q * in_tangents[1];
		out_tangents[2] = q * in_tangents[2];
		out_tangents[3] = q * in_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_packed_tangents[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out_packed_tangents[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out_packed_tangents[i + 3] = SPipTangents(out_tangents[3], flip[3]);
	}
	for (size_t i = (count & ~3); i < count; ++i)
	{
		out_general[i].xyz = wmat * in_general[i].xyz;
		out_general[i].color = in_general[i].color;
		out_general[i].st = in_general[i].st;

		in_tangents[0] = in_packed_tangents[i + 0].GetQ();
		out_tangents[0] = q * in_tangents[0];
		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
	}
}

static inline void UpdateGeneralTangentsNormals(
  SVF_P3S_C4B_T2S* out_general
  , SPipTangents* out_packed_tangents
  , Vec3f16* out_normals
  , SMMRMSkinVertex* in_general
  , size_t count
  , const Matrix34& wmat)
{
	MMRM_PROFILE_FUNCTION();
	Quat out_tangents[4];
	Quat in_tangents[4];
	int16 flip[4];
	Quat q = mat33_to_quat(Matrix33(wmat));
	q.Normalize();
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		out_general[i + 0].xyz = wmat * in_general[i + 0].pos;
		out_general[i + 0].color = in_general[i + 0].colour;
		out_general[i + 0].st = in_general[i + 0].uv;
		out_general[i + 1].xyz = wmat * in_general[i + 1].pos;
		out_general[i + 1].color = in_general[i + 1].colour;
		out_general[i + 1].st = in_general[i + 1].uv;
		out_general[i + 2].xyz = wmat * in_general[i + 2].pos;
		out_general[i + 2].color = in_general[i + 2].colour;
		out_general[i + 2].st = in_general[i + 2].uv;
		out_general[i + 3].xyz = wmat * in_general[i + 3].pos;
		out_general[i + 3].color = in_general[i + 3].colour;
		out_general[i + 3].st = in_general[i + 3].uv;

		in_tangents[0] = in_general[i + 0].qt.GetQ();
		in_tangents[1] = in_general[i + 1].qt.GetQ();
		in_tangents[2] = in_general[i + 2].qt.GetQ();
		in_tangents[3] = in_general[i + 3].qt.GetQ();

		out_tangents[0] = q * in_tangents[0];
		out_tangents[1] = q * in_tangents[1];
		out_tangents[2] = q * in_tangents[2];
		out_tangents[3] = q * in_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_packed_tangents[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out_packed_tangents[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out_packed_tangents[i + 3] = SPipTangents(out_tangents[3], flip[3]);

		out_normals[i + 0] = in_general[i + 0].normal;
		out_normals[i + 1] = in_general[i + 1].normal;
		out_normals[i + 2] = in_general[i + 2].normal;
		out_normals[i + 3] = in_general[i + 3].normal;
	}
	for (size_t i = (count & ~3); i < count; ++i)
	{
		out_general[i].xyz = wmat * in_general[i].pos;
		out_general[i].color = in_general[i].colour;
		out_general[i].st = in_general[i].uv;
		in_tangents[0] = in_general[i + 0].qt.GetQ();
		out_tangents[0] = q * in_tangents[0];
		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_normals[i + 0] = in_general[i + 0].normal;
	}
}

static inline void UpdateGeneralTangents(
  SVF_P3S_C4B_T2S* out_general
  , SPipTangents* out_packed_tangents
  , SMMRMSkinVertex* in_general
  , size_t count
  , const Matrix34& wmat)
{
	MMRM_PROFILE_FUNCTION();
	Quat out_tangents[4];
	Quat in_tangents[4];
	int16 flip[4];
	Quat q = mat33_to_quat(Matrix33(wmat));
	q.Normalize();
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		out_general[i + 0].xyz = wmat * in_general[i + 0].pos;
		out_general[i + 0].color = in_general[i + 0].colour;
		out_general[i + 0].st = in_general[i + 0].uv;
		out_general[i + 1].xyz = wmat * in_general[i + 1].pos;
		out_general[i + 1].color = in_general[i + 1].colour;
		out_general[i + 1].st = in_general[i + 1].uv;
		out_general[i + 2].xyz = wmat * in_general[i + 2].pos;
		out_general[i + 2].color = in_general[i + 2].colour;
		out_general[i + 2].st = in_general[i + 2].uv;
		out_general[i + 3].xyz = wmat * in_general[i + 3].pos;
		out_general[i + 3].color = in_general[i + 3].colour;
		out_general[i + 3].st = in_general[i + 3].uv;

		in_tangents[0] = in_general[i + 0].qt.GetQ();
		in_tangents[1] = in_general[i + 1].qt.GetQ();
		in_tangents[2] = in_general[i + 2].qt.GetQ();
		in_tangents[3] = in_general[i + 3].qt.GetQ();

		out_tangents[0] = q * in_tangents[0];
		out_tangents[1] = q * in_tangents[1];
		out_tangents[2] = q * in_tangents[2];
		out_tangents[3] = q * in_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_packed_tangents[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out_packed_tangents[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out_packed_tangents[i + 3] = SPipTangents(out_tangents[3], flip[3]);
	}
	for (size_t i = (count & ~3); i < count; ++i)
	{
		out_general[i].xyz = wmat * in_general[i].pos;
		out_general[i].color = in_general[i].colour;
		out_general[i].st = in_general[i].uv;

		in_tangents[0] = in_general[i + 0].qt.GetQ();
		out_tangents[0] = q * in_tangents[0];
		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
	}
}

static inline void UpdateGeneralTangents(
  SVF_P3S_C4B_T2S* out_general
  , SPipTangents* out_packed_tangents
  , SMMRMSkinVertex* in_general
  , SMMRMDeformVertex* deform
  , vtx_idx* mapping
  , const Matrix34& wmat
  , size_t count)
{
	MMRM_PROFILE_FUNCTION();
	Quat out_tangents[4];
	Quat in_tangents[4];
	int16 flip[4];
	Quat q = mat33_to_quat(Matrix33(wmat));
	q.Normalize();
	for (size_t i = 0; i < (count & ~3); i += 4)
	{
		out_general[i + 0].xyz = wmat * deform[mapping[i + 0]].pos[0];
		out_general[i + 0].color = in_general[i + 0].colour;
		out_general[i + 0].st = in_general[i + 0].uv;
		out_general[i + 1].xyz = wmat * deform[mapping[i + 1]].pos[0];
		out_general[i + 1].color = in_general[i + 1].colour;
		out_general[i + 1].st = in_general[i + 1].uv;
		out_general[i + 2].xyz = wmat * deform[mapping[i + 2]].pos[0];
		out_general[i + 2].color = in_general[i + 2].colour;
		out_general[i + 2].st = in_general[i + 2].uv;
		out_general[i + 3].xyz = wmat * deform[mapping[i + 3]].pos[0];
		out_general[i + 3].color = in_general[i + 3].colour;
		out_general[i + 3].st = in_general[i + 3].uv;

		in_tangents[0] = in_general[i + 0].qt.GetQ();
		in_tangents[1] = in_general[i + 1].qt.GetQ();
		in_tangents[2] = in_general[i + 2].qt.GetQ();
		in_tangents[3] = in_general[i + 3].qt.GetQ();

		out_tangents[0] = q * in_tangents[0];
		out_tangents[1] = q * in_tangents[1];
		out_tangents[2] = q * in_tangents[2];
		out_tangents[3] = q * in_tangents[3];

		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		flip[1] = in_tangents[1].w < 0.f ? -1 : 1;
		flip[2] = in_tangents[2].w < 0.f ? -1 : 1;
		flip[3] = in_tangents[3].w < 0.f ? -1 : 1;

		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		if (flip[1] < 0) out_tangents[1] = -out_tangents[1];
		if (flip[2] < 0) out_tangents[2] = -out_tangents[2];
		if (flip[3] < 0) out_tangents[3] = -out_tangents[3];

		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
		out_packed_tangents[i + 1] = SPipTangents(out_tangents[1], flip[1]);
		out_packed_tangents[i + 2] = SPipTangents(out_tangents[2], flip[2]);
		out_packed_tangents[i + 3] = SPipTangents(out_tangents[3], flip[3]);
	}
	for (size_t i = (count & ~3); i < count; ++i)
	{
		out_general[i].xyz = wmat * deform[mapping[i]].pos[0];
		out_general[i].color = in_general[i].colour;
		out_general[i].st = in_general[i].uv;

		in_tangents[0] = in_general[i + 0].qt.GetQ();
		out_tangents[0] = q * in_tangents[0];
		flip[0] = in_tangents[0].w < 0.f ? -1 : 1;
		if (flip[0] < 0) out_tangents[0] = -out_tangents[0];
		out_packed_tangents[i + 0] = SPipTangents(out_tangents[0], flip[0]);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Update a set of indices to be relative to a base vertext
static inline void UpdateIndex(vtx_idx* out, vtx_idx* in, uint32 base)
{
	MMRM_PROFILE_FUNCTION();
	*out = *in + base;
	mmrm_assert(*out <= 0xffff);
}
static inline void UpdateIndices(vtx_idx* out, vtx_idx* in, uint32 base, size_t count)
{
	MMRM_PROFILE_FUNCTION();
	for (size_t i = 0; i < (count & ~7); i += 8)
	{
		out[i + 0] = in[i + 0] + base;
		mmrm_assert(out[i + 0] <= 0xffff);
		out[i + 1] = in[i + 1] + base;
		mmrm_assert(out[i + 1] <= 0xffff);
		out[i + 2] = in[i + 2] + base;
		mmrm_assert(out[i + 2] <= 0xffff);
		out[i + 3] = in[i + 3] + base;
		mmrm_assert(out[i + 3] <= 0xffff);
		out[i + 4] = in[i + 4] + base;
		mmrm_assert(out[i + 4] <= 0xffff);
		out[i + 5] = in[i + 5] + base;
		mmrm_assert(out[i + 5] <= 0xffff);
		out[i + 6] = in[i + 6] + base;
		mmrm_assert(out[i + 6] <= 0xffff);
		out[i + 7] = in[i + 7] + base;
		mmrm_assert(out[i + 7] <= 0xffff);
	}
	for (size_t i = (count & ~7); i < count; i++)
	{
		out[i] = in[i] + base;
		mmrm_assert(out[i] <= 0xffff);
	}
}

// Bilinear Sampling of wind forces
Vec3 SampleWind(const Vec3 pos, const Vec3 (&samples)[(MMRM_WIND_DIM)][(MMRM_WIND_DIM)][(MMRM_WIND_DIM)])
{
	Vec3 result = Vec3(0, 0, 0);
	float px = clamp_tpl(pos.x, 0.f, 1.f - FLT_EPSILON) * (MMRM_WIND_DIM - 1);
	float py = clamp_tpl(pos.y, 0.f, 1.f - FLT_EPSILON) * (MMRM_WIND_DIM - 1);
	float pz = clamp_tpl(pos.z, 0.f, 1.f - FLT_EPSILON) * (MMRM_WIND_DIM - 1);
	float ftix(floorf(px)), ftiy(floorf(py)), ftiz(floorf(pz));
	float fx = px - ftix, fy = py - ftiy, fz = pz - ftiz;
	int d = MMRM_WIND_DIM, ix = static_cast<int>(ftix), iy = static_cast<int>(ftiy), iz = static_cast<int>(ftiz);

	result += samples[ix][iy][iz] * (1.f - fz) * (1.f - fy) * (1.f - fx);
	result += samples[ix + 1][iy][iz] * (1.f - fz) * (1.f - fy) * (fx);
	result += samples[ix][iy + 1][iz] * (1.f - fz) * (fy) * (1.f - fx);
	result += samples[ix + 1][iy + 1][iz] * (1.f - fz) * (fy) * (fx);
	result += samples[ix][iy][iz + 1] * (fz) * (1.f - fy) * (1.f - fx);
	result += samples[ix + 1][iy][iz + 1] * (fz) * (1.f - fy) * (fx);
	result += samples[ix][iy + 1][iz + 1] * (fz) * (fy) * (1.f - fx);
	result += samples[ix + 1][iy + 1][iz + 1] * (fz) * (fy) * (fx);
	return result;
}

// Bilinear Sampling of wind forces
extern inline Vec3 SampleWind(const Vec3& pos, const Vec3* samples)
{
	Vec3 result = Vec3(0, 0, 0);
	float px = clamp_tpl(pos.x, 0.f, 1.f - FLT_EPSILON) * (MMRM_WIND_DIM - 1);
	float py = clamp_tpl(pos.y, 0.f, 1.f - FLT_EPSILON) * (MMRM_WIND_DIM - 1);
	float pz = clamp_tpl(pos.z, 0.f, 1.f - FLT_EPSILON) * (MMRM_WIND_DIM - 1);
	float ftix(floorf(px)), ftiy(floorf(py)), ftiz(floorf(pz));
	float fx = px - ftix, fy = py - ftiy, fz = pz - ftiz;
	int d = MMRM_WIND_DIM, ix = static_cast<int>(ftix), iy = static_cast<int>(ftiy), iz = static_cast<int>(ftiz);

	result += samples[(ix + 0) + (iy + 0) * d + (iz + 0) * sqr(d)] * (1.f - fz) * (1.f - fy) * (1.f - fx);
	result += samples[(ix + 1) + (iy + 0) * d + (iz + 0) * sqr(d)] * (1.f - fz) * (1.f - fy) * (fx);
	result += samples[(ix + 0) + (iy + 1) * d + (iz + 0) * sqr(d)] * (1.f - fz) * (fy) * (1.f - fx);
	result += samples[(ix + 1) + (iy + 1) * d + (iz + 0) * sqr(d)] * (1.f - fz) * (fy) * (fx);
	result += samples[(ix + 0) + (iy + 0) * d + (iz + 1) * sqr(d)] * (fz) * (1.f - fy) * (1.f - fx);
	result += samples[(ix + 1) + (iy + 0) * d + (iz + 1) * sqr(d)] * (fz) * (1.f - fy) * (fx);
	result += samples[(ix + 0) + (iy + 1) * d + (iz + 1) * sqr(d)] * (fz) * (fy) * (1.f - fx);
	result += samples[(ix + 1) + (iy + 1) * d + (iz + 1) * sqr(d)] * (fz) * (fy) * (fx);
	return result;
}

#if MMRM_RENDER_DEBUG
static void TraceWindSample(const Vec3& offs, const Vec3& dw, const SMMRMInstanceContext& context)
{
	Vec3 trace_pos = offs, trace_vel = dw;
	for (size_t ui = 0; ui < 16; ++ui)
	{
		ColorB col0;
		col0.lerpFloat(Col_Blue, Col_Red, ui / 16.f);
		ColorB col1;
		col1.lerpFloat(Col_Blue, Col_Red, (ui + 1) / 16.f);
		const Vec3& pos = trace_pos;
		const Vec3& vel = trace_vel;
		const Vec3& size = context.max - context.min;

		Vec3 realpos;
		realpos.x = context.min.x + pos.x * size.x;
		realpos.y = context.min.y + pos.y * size.y;
		realpos.z = context.min.z + pos.z * size.z;

		const Vec3& nvel = trace_vel + ::SampleWind(pos, context.wind) * (1.f / 8.f);
		Vec3 npos = realpos + nvel * 0.3333f;
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(realpos, col0, npos, col1, 1.f);

		npos.x = (realpos.x - context.min.x) / size.x;
		npos.y = (realpos.y - context.min.y) / size.y;
		npos.z = (realpos.z - context.min.z) / size.z;

		trace_pos = npos;
		trace_vel = nvel;
	}
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Merge instance geometry into a list of buffers
static void MergeInstanceList(SMMRMInstanceContext& context)
{
	using namespace NVMath;
	MEMORY_SCOPE_CHECK_HEAP();
	SMMRMGeometry* geom = context.geom;
	SMMRMUpdateContext* update = context.update;
	SMMRMSpineVtxBase* spines = context.use_spines ? context.spines : NULL;
	SMMRMSpineVtx* geomSpines = (geom->pSpineVtx);
	SMMRMSpineInfo* geomSpineInfo = (geom->pSpineInfo);
	SVF_P3S_C4B_T2S* general = update->general;
	SPipTangents* tangents = update->tangents;
	Vec3f16* normals = update->normals;
	vtx_idx* idxBuf = update->idxBuf;
	PREFAST_SUPPRESS_WARNING(6255)
	DualQuatA * bones = spines && geom->numSpineVtx ? CryStackAllocVector(DualQuatA, (geom->numSpineVtx + 1), CRY_PLATFORM_ALIGNMENT) : NULL;
	if (bones) mmrm_printf("alloced %#x bytes\n", sizeof(DualQuatA) * (geom->numSpineVtx + 1));
	if (bones) { memset(bones, 0x0, sizeof(DualQuatA) * (geom->numSpineVtx + 1)); }
	mmrm_printf("updating %d samples\n", context.amount);
#if MMRM_USE_BOUNDS_CHECK
	mmrm_printf("updating %#x<==>%#x general \n", (unsigned int)general, (unsigned int)TLS_GET(unsigned int, s_general_end));
	mmrm_printf("updating %#x<==>%#x tangents\n", (unsigned int)tangents, (unsigned int)TLS_GET(unsigned int, s_tangents_end));
	mmrm_printf("updating %#x<==>%#x idxBuf\n", (unsigned int)idxBuf, (unsigned int)TLS_GET(unsigned int, s_idx_end));
#endif
	PREFAST_SUPPRESS_WARNING(6255)
	Vec3A * npt = spines && geom->numSpineVtx ? CryStackAllocVector(Vec3A, geom->numSpineVtx, CRY_PLATFORM_ALIGNMENT) : NULL;
	if (npt)
	{
		mmrm_printf("alloced %#x bytes\n", sizeof(Vec3A) * (geom->numSpineVtx));
		memset(npt, 0x0, sizeof(Vec3A) * (geom->numSpineVtx));
	}
	PREFAST_SUPPRESS_WARNING(6255)
	Vec3A * opt = spines && geom->numSpineVtx ? CryStackAllocVector(Vec3A, geom->numSpineVtx, CRY_PLATFORM_ALIGNMENT) : NULL;
	if (opt)
	{
		mmrm_printf("alloced %#x bytes\n", sizeof(Vec3A) * (geom->numSpineVtx));
		memset(opt, 0x0, sizeof(Vec3A) * (geom->numSpineVtx));
	}
	PREFAST_SUPPRESS_WARNING(6255)
	Vec3A * nvel = spines && geom->numSpineVtx ? CryStackAllocVector(Vec3A, geom->numSpineVtx, CRY_PLATFORM_ALIGNMENT) : NULL;
	if (nvel)
	{
		mmrm_printf("alloced %#x bytes\n", sizeof(Vec3A) * (geom->numSpineVtx));
		memset(nvel, 0x0, sizeof(Vec3A) * (geom->numSpineVtx));
	}
	primitives::sphere* colliders = context.colliders;
	SMMRMProjectile* projectiles = context.projectiles;
	PREFAST_SUPPRESS_WARNING(6255)
	SMMRMContact * contacts = geom->numSpineVtx ? CryStackAllocVector(SMMRMContact, geom->numSpineVtx, CRY_PLATFORM_ALIGNMENT) : NULL;
	if (contacts) mmrm_printf("alloced %#x bytes\n", sizeof(SMMRMContact) * (geom->numSpineVtx));
	int i = 0, j = 0, l = 0, off = 0, boneIdx = 1, nLod, nspines = (int)geom->numSpines;
	float i_f = 0.f;
	int base = 0, ncolliders = context.ncolliders, nprojectiles = context.nprojectiles;
	Matrix34A wmat, smat, iwmat, tmat;
	const float fExtents = c_MergedMeshesExtent;
	const aVec3 origin = context.min;
	int iter = 0, max_iter = context.max_iter, frame_id = update->frame_count;
	float c = 0, d = 0, iwsum = 0, dt = 0.f, dtcur = 0.f, dttot = context.dt, dtscale = context.dtscale, abstime = context.abstime
	, airResistance = update->group->physConfig.airResistance
	, damping = update->group->physConfig.fDamping
	, w[3] = { 0 }
	, kH = update->group->physConfig.kH
	, kL = 1.f / (float)max_iter
	, plasticity = 0.f
	, var = 0.f
	, variance = update->group->physConfig.variance
	, fScale
	, rScale
	, rdt;
	aVec3 sample_pos, geom_ctr = geom->aabb.GetCenter();
	Vec3 its[2];
	Vec3A ctr, offs, disp, ndisp, dw, dw0, dir[2], dirO[2], bending[3];
	DualQuatA wq;
	aQuat qrot;
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
	const vec4 vSignMask = NVMath::Vec4(31u, 31u, 31u, 31u), vEpsilon = NVMath::Vec4Epsilon();
	const vec4 vW = NVMath::Vec4(0.f, 0.f, 0.f, 1.f);
	const vec4 vCvt = NVMath::Vec4((float)(1 << 8)), vRcvt = NVMath::Vec4(1.f / (float)(1 << 8));
	const vec4 viwsum = NVMath::Vec4(1.f / 2.f), vOne = NVMath::Vec4One(), vZero = NVMath::Vec4Zero();
	const vec4 vHalf = NVMath::Vec4(0.5f), vNegOne = NVMath::Vec4(-1.f);
	const vec4 vMin = NVMath::Vec4(-127.f), vMax = NVMath::Vec4(128.f);
	const vec4 vAR = NVMath::Vec4(airResistance), vDamping = NVMath::Vec4(damping), vGrav = NVMath::Vec4(0.f, 0.f, -9.81f, 0.f);
#endif
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
	const vec4 vXMask = NVMath::Vec4(~0u, 0u, 0u, 0u);
	const vec4 vYMask = NVMath::Vec4(0u, ~0u, 0u, 0u);
	const vec4 vZMask = NVMath::Vec4(0u, 0u, ~0u, 0u);
	const vec4 vWMask = NVMath::Vec4(0u, 0u, 0u, ~0u);
#endif
	for (size_t k = 0; k < context.amount; ++k)
	{
		SMMRMInstance& sample = context.samples[k];
		{
			// Skip if not visible
			nLod = sample.lastLod;
			if (nLod < 0)
			{
				base += geom->numSpineVtx;
				continue;
			}
			DecompressInstance(sample, origin, context.rotationOrigin, context.zRotation, fExtents, sample_pos, qrot, fScale);

			rScale = 1.f / (max(fScale, FLT_EPSILON));

			wmat = Matrix34(qrot);
			wmat.m03 = sample_pos.x;
			wmat.m13 = sample_pos.y;
			wmat.m23 = sample_pos.z;

			smat = Matrix34::CreateScale(Vec3(fScale, fScale, fScale));
			mmrm_assert(wmat.IsValid() && smat.IsValid());

			iwmat = wmat.GetInvertedFast();
			mmrm_assert(iwmat.IsValid());

			wq = DualQuat(qrot, sample_pos - context.centre);

			tmat = wmat * smat;
			tmat.m03 += -context.centre.x;
			tmat.m13 += -context.centre.y;
			tmat.m23 += -context.centre.z;

			ctr = sample_pos + geom_ctr;
			offs = Vec3((ctr.x - context.min.x) / (context.max.x - context.min.x), (ctr.y - context.min.y) / (context.max.y - context.min.y), (ctr.z - context.min.z) / (context.max.z - context.min.z));
			dw = SampleWind(offs, context.wind);

#if MMRM_RENDER_DEBUG && MMRM_VISUALIZE_FORCES
			if (Cry3DEngineBase::GetCVars()->e_MergedMeshesDebug) TraceWindSample(offs, dw, context);
#endif

			var = dw.len() * variance;
			dw += Vec3(cry_random(0.0f, var), cry_random(0.0f, var), cry_random(0.0f, var));

			iter = 0, max_iter = context.max_iter;
		}
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
		const vec4 vScale = NVMath::Vec4(fScale), rvScale = NVMath::Vec4(rScale);
		const vec4 vDW = NVMath::Vec4(dw.x, dw.y, dw.z, 0.f);
#endif
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
		const vec4 vWX = _mm_load_ps((float*)((char*)&wmat + sizeof(__m128) * 0));
		const vec4 vWY = _mm_load_ps((float*)((char*)&wmat + sizeof(__m128) * 1));
		const vec4 vWZ = _mm_load_ps((float*)((char*)&wmat + sizeof(__m128) * 2));
#endif
		// Spine based simulation performed here
		if (spines)
		{
			MMRM_FRAME_PROFILER("PVRN SPINES");
			bones[0] = wq;
			boneIdx = 1;
			for (j = 0, off = 0; j < nspines; ++j)
			{
				// Convert to full floats
#if MMRM_SIMULATION_USES_FP32
				opt[off + 0] = spines[base + off + 0].pt;
				nvel[off + 0] = Vec3(0, 0, 0); // first spine vertex never moves
#else
				CIntToFloat sp_x0(spines[base + off + 0].pt.x), sp_y0(spines[base + off + 0].pt.y), sp_z0(spines[base + off + 0].pt.z);
				opt[off + 0] = wmat * (Vec3(sp_x0.Convert() / (float)(1 << 8), sp_y0.Convert() / (float)(1 << 8), sp_z0.Convert() / (float)(1 << 8)) * fScale);
				nvel[off + 0] = Vec3(0, 0, 0); // first spine vertex never moves
#endif
				// update plasticity
				plasticity = (MMRM_PLASTICITY_TIME - spines[base + off + 0].vel.x) / (float)MMRM_PLASTICITY_TIME;
				dtcur = dttot + static_cast<float>(spines[base + off + 0].vel.y) * (1.f / 1000.f);
				spines[base + off + 0].vel.y = 0;
				for (i = 1; i < (int)geomSpineInfo[j].nSpineVtx; ++i)
				{
#if MMRM_SIMULATION_USES_FP32
					opt[off + i] = spines[base + off + i].pt;
					nvel[off + i] = spines[base + off + i].vel;
#else
					int sp_x(spines[base + off + i].pt.x), sp_y(spines[base + off + i].pt.y), sp_z(spines[base + off + i].pt.z);
					int sv_x(spines[base + off + i].vel.x), sv_y(spines[base + off + i].vel.y), sv_z(spines[base + off + i].vel.z);
					opt[off + i] = wmat * (Vec3(sp_x / (float)(1 << 8), sp_y / (float)(1 << 8), sp_z / (float)(1 << 8)) * fScale);
					nvel[off + i] = Vec3(sv_x / (float)(1 << 8), sv_y / (float)(1 << 8), sv_z / (float)(1 << 8));
#endif
				}
				for (size_t lc = 0; lc < geom->numSpineVtx; contacts[lc++].i = -1)
					;
				while (dtcur > MMRM_FIXED_STEP)
				{
					dtcur -= (dt = MMRM_FIXED_STEP);
					rdt = 1.f / dt;

					kL = 1.f / (float)(max_iter = context.max_iter);
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
					const vec4 vDT = NVMath::Vec4(dt), vRDT = NVMath::Vec4(rdt), vPlasticity = NVMath::Vec4(plasticity);
					const vec4 vkhkl = NVMath::Vec4(kH * kL);
#endif
					// Advance the spine's vertices depending on simulation state
					for (i = 0, i_f = -1.f; i < (int)geomSpineInfo[j].nSpineVtx; ++i, i_f += 1.f)
					{
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
						const vec4 vopt0 = _mm_load_ps(&opt[off + i].x);
						const vec4 vopt1 = _mm_load_ps(&opt[off + i + (iszero(i) - 1)].x);
						const vec4 vopt2 = _mm_load_ps(&opt[off + i - (iszero(i - ((int)geomSpineInfo[j].nSpineVtx - 1)) - 1)].x);
						const vec4 ovel = _mm_load_ps(&nvel[off + i].x);
						const vec4 vi0 = _mm_set1_ps(i_f);

						const vec4 vdir0 = Sub(vopt1, vopt0);
						const vec4 vlen0 = DotProduct3(vdir0, vdir0);
						const vec4 rvlen0 = _mm_rsqrt_ps(Max(vlen0, vEpsilon));
						const vec4 vndir0 = Mul(rvlen0, vdir0);

						const vec4 vdir1 = Sub(vopt2, vopt0);
						const vec4 vlen1 = DotProduct3(vdir1, vdir1);
						const vec4 rvlen1 = _mm_rsqrt_ps(Max(vlen1, vEpsilon));
						const vec4 vndir1 = Mul(rvlen1, vdir1);

						const vec4 va0 = Sub(vOne, DotProduct3(vDW, vndir0));
						vec4 vdw = Mul(vHalf, Mul(vDW, Absolute(va0)));

						const vec4 va1 = Sub(vOne, DotProduct3(vDW, vndir1));
						vdw = Add(vdw, Mul(vHalf, Mul(vDW, Absolute(va1))));

						vec4 vnvel = Mul(Add(Mul(vdw, vAR), vGrav), vDT);
						vnvel = Add(ovel, vnvel);
						vnvel = Mul(vnvel, Mul(Sub(vOne, Mul(vDamping, vDT)), vPlasticity));
						vnvel = SelectFromVector(vnvel, vZero, _mm_castsi128_ps(_mm_srai_epi32(_mm_castps_si128(vi0), 31)));
						vec4 vnpt = Add(vopt0, Mul(vnvel, vDT));

						_mm_store_ps(&nvel[off + i].x, vnvel);
						_mm_store_ps(&npt[off + i].x, vnpt);
#else
						dir[0] = (opt[off + i + (iszero(i) - 1)] - opt[off + i]);
						dir[1] = (opt[off + i - (iszero(i - ((int)geomSpineInfo[j].nSpineVtx - 1)) - 1)] - opt[off + i]);
						dir[0] *= (float)isqrt_tpl(max(dir[0].len2(), sqr(FLT_EPSILON)));
						dir[1] *= (float)isqrt_tpl(max(dir[1].len2(), sqr(FLT_EPSILON)));
						dw0 = dw * (fabs_tpl(1.f - (dw * dir[0]))) * 0.5f;
						dw0 += dw * (fabs_tpl(1.f - dw * dir[1])) * 0.5f;
						const Vec3& new_vel = (nvel[off + i] * plasticity + (dw0 * airResistance + Vec3(0, 0, -9.81f)) * dt) * __fsel(i_f, 1.f, 0.f) * (1.f - (damping * dt));
						npt[off + i] = opt[off + i] + new_vel * dt;
						nvel[off + i] = new_vel;
#endif
					}
					// Iteratively solve position, bending and contact constraints
					do
					{
						for (l = 0; l < (int)geomSpineInfo[j].nSpineVtx - 1; ++l)
							contacts[l].i = -1;
						// Perform (simple) collision detection. If collision found, record
						// collision plane (normal of sphere at intersection point) as a
						// constraint.
						{
							/*MMRM_FRAME_PROFILER("mmrm colliders"); */
							for (i = 0; i < ncolliders; ++i)
							{
								const primitives::sphere& sph = colliders[i];
								if ((ctr - sph.center).len2() < sqr(sph.r + geomSpineInfo[j].fSpineLen * fScale))
									for (l = 0; l < (int)geomSpineInfo[j].nSpineVtx - 1; ++l)
									{
										if (contacts[l].i >= 0)
											continue;
										bool intersection = false;
										Sphere s(sph.center, sph.r);
										switch (_Lineseg_Sphere(Lineseg(npt[off + l], npt[off + l + 1]), Sphere(sph.center, sph.r), its[0], its[1]))
										{
										case 0x0:
											IF (l > 0 && (contacts[l].i < 0) && (npt[off + l] - sph.center).len2() < sqr(sph.r), 0)
											{
												contacts[l].p.n = (npt[off + l] - sph.center);
												contacts[l].p.n *= (float)isqrt_tpl(max(contacts[l].p.n.len2(), sqr(FLT_EPSILON)));
												contacts[l].p.d = -((sph.center + contacts[l].p.n * sph.r) * contacts[l].p.n);
												contacts[l].i = off + l;
												intersection = true;
											}
											IF (contacts[l + 1].i < 0 && (npt[off + l + 1] - sph.center).len2() < sqr(sph.r), 0)
											{
												contacts[l + 1].p.n = (npt[off + l + 1] - sph.center);
												contacts[l + 1].p.n *= (float)isqrt_tpl(max(contacts[l + 1].p.n.len2(), sqr(FLT_EPSILON)));
												contacts[l + 1].p.d = -((sph.center + contacts[l + 1].p.n * sph.r) * contacts[l + 1].p.n);
												contacts[l + 1].i = off + l + 1;
												intersection = true;
											}
											break;
										case 0x1: /*first_point_in_sphere:*/
											IF (contacts[l + 1].i < 0, 1)
											{
												contacts[l + 1].p.n = (npt[off + l + 1] - sph.center);
												contacts[l + 1].p.n *= (float)isqrt_tpl(max(contacts[l + 1].p.n.len2(), sqr(FLT_EPSILON)));
												contacts[l + 1].p.d = -((sph.center + contacts[l + 1].p.n * sph.r) * contacts[l + 1].p.n);
												contacts[l + 1].i = off + l + 1;
												intersection = true;
											}
											break;
										case 0x2: /*second_point_in_sphere:*/
											IF (contacts[l].i < 0 && l > 0, 1)
											{
												contacts[l].p.n = (npt[off + l] - sph.center);
												contacts[l].p.n *= (float)isqrt_tpl(max(contacts[l].p.n.len2(), sqr(FLT_EPSILON)));
												contacts[l].p.d = -((sph.center + contacts[l].p.n * sph.r) * contacts[l].p.n);
												contacts[l].i = off + l;
												intersection = true;
											}
											break;
										case 0x3: /*both_points_in_sphere:*/
											IF (contacts[l].i < 0 && contacts[l + 1].i < 0, 1)
											{
												Vec3 centre = (its[0] + its[1]) * 0.5f;
												Vec3 normal = (centre - sph.center);
												;
												normal *= (float)isqrt_tpl(max(normal.len2(), sqr(FLT_EPSILON)));
												float dist = -((sph.center + normal * sph.r) * normal);

												IF (l > 0, 1)
												{
													contacts[l].p.n = normal;
													contacts[l].p.d = dist;
													contacts[l].i = off + l;
												}
												contacts[l + 1].p.n = normal;
												contacts[l + 1].p.d = dist;
												contacts[l + 1].i = off + l + 1;
												intersection = true;
											}
											break;
										}
										IF (intersection, 0)
										{
											spines[base + off + 0].vel.x = MMRM_PLASTICITY_TIME;
											max_iter = max(context.max_iter + 2, max_iter);
											kL = 1.f / (float)max_iter;
										}
									}
							}
						}
						// Perform (simple) swept checks for projectiles
						{
							/*MMRM_FRAME_PROFILER("mmrm projectiles ");*/
							for (i = 0; i < nprojectiles; ++i)
							{
								const SMMRMProjectile& projectile = projectiles[i];
								Lineseg pl(projectile.pos[0], projectile.pos[1]);
								float radiusSq = sqr(projectile.r), t0, t1;
								if (_Point_LinesegSq(ctr, pl) < sqr(projectile.r + geomSpineInfo[j].fSpineLen * fScale))
									for (l = 0; l < (int)geomSpineInfo[j].nSpineVtx - 1; ++l)
									{
										bool intersection = false;
										float distanceSq = 0.f;
										if (contacts[l].i >= 0 && contacts[l + 1].i >= 0)
											continue;
										Lineseg spine(npt[off + l], npt[off + l + 1]);
										if ((distanceSq = Distance::Lineseg_LinesegSq(spine, pl, &t0, &t1)) > radiusSq)
											continue;
										Vec3 spt = spine.GetPoint(t0);
										Vec3 plpt = pl.GetPoint(t1) + projectile.dir * projectile.r * (1.0f - (distanceSq / radiusSq));
										IF (contacts[l].i < 0 && l > 0, 1)
										{
											contacts[l].p.n = (npt[off + l] - plpt);
											contacts[l].p.n *= (float)isqrt_tpl(max(contacts[l].p.n.len2(), sqr(FLT_EPSILON)));
											contacts[l].p.d = -((plpt + contacts[l].p.n * projectile.r) * contacts[l].p.n);
											contacts[l].i = off + l;
											intersection = true;
										}
										IF (contacts[l + 1].i < 0, 1)
										{
											contacts[l + 1].p.n = (npt[off + l + 1] - plpt);
											contacts[l + 1].p.n *= (float)isqrt_tpl(max(contacts[l + 1].p.n.len2(), sqr(FLT_EPSILON)));
											contacts[l + 1].p.d = -((plpt + contacts[l + 1].p.n * projectile.r) * contacts[l + 1].p.n);
											contacts[l + 1].i = off + l + 1;
											intersection = true;
										}
										IF (intersection, 1)
										{
											max_iter = max(context.max_iter + 3, max_iter);
											kL = 1.f / (float)max_iter;
										}
									}
							}
						}
						// Enforce contact planes, project back out of contact plane if
						// vertex was moved inside
						{
							/*MMRM_FRAME_PROFILER("mmrm contacts "); */
							for (i = 0; i < (int)geom->numSpineVtx; ++i)
								if (contacts[i].i >= 0 && (c = contacts[i].p | npt[contacts[i].i]) <= 0.f)
									npt[contacts[i].i] += contacts[i].p.n * -c * kL;
						}
#if MMRM_SPINE_SLERP_BENDING
						// Enforce bending - ensure that the spine local-space angle relative
						// to its parent does not differ too far from the resting pose. If
						// so a simple lerp is performed to ensure (not correct, but
						// efficient & sufficient).
						// Problem: not so efficient because the local space computations can become a bottleneck.
						// Further, scaling the counter bending force on the input angle can lead to stability issues
						Matrix34A lmat = Matrix34::CreateIdentity();
	#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
						const vec4 vkL = NVMath::Vec4(kL);
						vec4 vspt0 = _mm_loadu_ps(&geomSpines[off + 0].pt.x);
						vec4 vspt1 = _mm_loadu_ps(&geomSpines[off + 0 + 1].pt.x);
						vec4 vnpt0 = _mm_load_ps(&npt[off + 0].x);
						vec4 vnpt1 = _mm_load_ps(&npt[off + 0 + 1].x);
						vec4 vsplen = NVMath::Vec4(geomSpines[off + 0].len);
						vec4 vndir, vlen2, rvlen, vxxxx, vyyyy, vzzzz, vLX, vLY, vLZ;

						vspt0 = SelectFromVector(Mul(vspt0, vScale), vW, vWMask);
						vxxxx = DotProduct4(vWX, vspt0);
						vyyyy = DotProduct4(vWY, vspt0);
						vzzzz = DotProduct4(vWZ, vspt0);
						vspt0 = SelectFromVector(vspt0, vxxxx, vXMask);
						vspt0 = SelectFromVector(vspt0, vyyyy, vYMask);
						vspt0 = SelectFromVector(vspt0, vzzzz, vZMask);
	#endif
						for (i = 0; i < (int)geomSpineInfo[j].nSpineVtx - 1; ++i)
						{
	#if  MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
							vspt1 = _mm_set_ps(0, geomSpines[off + i + 1].pt.z, geomSpines[off + i + 1].pt.y, geomSpines[off + i + 1].pt.x);
							vnpt1 = _mm_load_ps(&npt[off + i + 1].x);
							vsplen = _mm_set1_ps(geomSpines[off + i].len);
							vLX = _mm_load_ps((float*)((char*)&lmat + sizeof(__m128) * 0));
							vLY = _mm_load_ps((float*)((char*)&lmat + sizeof(__m128) * 1));
							vLZ = _mm_load_ps((float*)((char*)&lmat + sizeof(__m128) * 2));

							vec4 vdir = Sub(vnpt1, vnpt0);
							vlen2 = DotProduct3(vdir, vdir);
							vlen2 = Max(vlen2, vEpsilon);
							rvlen = _mm_rsqrt_ps(vlen2);
							vndir = Mul(vdir, rvlen);

							vspt1 = SelectFromVector(Mul(vspt1, vScale), vW, vWMask);
							vxxxx = DotProduct4(vWX, vspt1);
							vyyyy = DotProduct4(vWY, vspt1);
							vzzzz = DotProduct4(vWZ, vspt1);
							vspt1 = SelectFromVector(vspt1, vxxxx, vXMask);
							vspt1 = SelectFromVector(vspt1, vyyyy, vYMask);
							vspt1 = SelectFromVector(vspt1, vzzzz, vZMask);

							vec4 vodir = Sub(vspt1, vspt0);
							vlen2 = DotProduct3(vodir, vodir);
							vlen2 = Max(vlen2, vEpsilon);
							rvlen = _mm_rsqrt_ps(vlen2);

							vec4 vnodir = Mul(vodir, rvlen);
							vxxxx = DotProduct3(vLX, vnodir);
							vyyyy = DotProduct3(vLY, vnodir);
							vzzzz = DotProduct3(vLZ, vnodir);
							vnodir = SelectFromVector(vnodir, vxxxx, vXMask);
							vnodir = SelectFromVector(vnodir, vyyyy, vYMask);
							vnodir = SelectFromVector(vnodir, vzzzz, vZMask);
							_mm_store_ps(&dirO[1].x, vnodir);

							vec4 vangle = Sub(vOne, DotProduct3(vndir, vnodir));
							vangle = Absolute(vangle);
							vangle = Mul(vkhkl, vangle);
							vangle = Max(Min(vangle, vOne), vZero);

							vec4 ndir1 = Add(vndir, Mul(Sub(vnodir, vndir), vangle));
							vlen2 = DotProduct3(ndir1, ndir1);
							vlen2 = Max(vlen2, vEpsilon);
							rvlen = _mm_rsqrt_ps(vlen2);
							ndir1 = Mul(ndir1, rvlen);
							_mm_store_ps(&dir[0].x, ndir1);

							vnpt1 = Add(vnpt0, Mul(ndir1, Mul(vsplen, vScale)));
							_mm_store_ps(&npt[off + i + 1].x, vnpt1);

							// ToDo : LHS, the below code needs to be ported to vmx :(
							lmat = lmat * Matrix33::CreateRotationV0V1(dirO[1], dir[0]);

							vspt0 = vspt1;
							vnpt0 = vnpt1;
	#else
							dir[1] = (npt[off + i + 1] - npt[off + i]);
							dir[1] *= (float)isqrt_tpl(max(dir[1].len2(), sqr(FLT_EPSILON)));
							dirO[1] = wmat * (geomSpines[off + i + 1].pt * fScale) - wmat * (geomSpines[off + i].pt * fScale);
							dirO[1] = lmat * dirO[1] * (float)isqrt_tpl(max(dirO[1].len2(), sqr(FLT_EPSILON)));
							float angle = dir[1] * dirO[1];
							if (fabs_tpl(1.f - angle) > FLT_EPSILON)
							{
		#if MMRM_USE_LERP
								dir[0] = Vec3::CreateLerp(dir[1], dirO[1], clamp_tpl(fabsf(1.0f - angle) * kH * kL, 0.f, 1.f));
		#else
								dir[0] = Vec3::CreateSlerp(dir[1], dirO[1], clamp_tpl(fabsf(1.0f - angle) * kH * kL, 0.f, 1.f));
		#endif
								dir[0] *= (float)isqrt_tpl(max(dir[0].len2(), sqr(FLT_EPSILON)));
								lmat = lmat * Matrix33::CreateRotationV0V1(dirO[1], dir[0]);
								npt[off + i + 1] = npt[off + i] + dir[0] * geomSpines[off + i].len * fScale;
							}
	#endif
						}
#endif
#if MMRM_SPINE_HEIGHT_BENDING
						// Enforce bending - ensure that the height of the current spine vertex
						// relative to the line segment defined by it's immediate neighbour vertices
						// matches the initial height recorded in it's rest-pose
						for (i = (int)geomSpineInfo[j].nSpineVtx - 2, i_f = (float)i; i > 0; --i, i_f -= 1.f)
						{
							w[0] = (float)__fsel(-(i_f + 1.f), 0.0f, 1.f);
							w[1] = (float)__fsel(-(i_f), 0.0f, 1.f);
							w[2] = (float)__fsel(-(i_f - 1.f), 0.0f, 1.f);
							iwsum = (w[0] + 2.f * w[1] + w[2]);
							iwsum = iwsum > 0.f ? 1.f / (iwsum) : 1.f;
							bending[0] = npt[off + i + 1];
							bending[1] = npt[off + i];
							bending[2] = npt[off + i - 1];
							disp = (bending[1] - (1.f / 3.f) * (bending[0] + bending[1] + bending[2]));
							mmrm_assert(disp.IsValid());
							d = (max(disp.len2(), sqr(FLT_EPSILON)));
							mmrm_assert(NumberValid(d));
							if (fabs(d - sqr(geomSpines[off + i].h)) > FLT_EPSILON)
							{
								d = (float)isqrt_tpl(d);
								npt[off + i + 1] += 2.f * w[0] * iwsum * disp * (1.f - (geomSpines[off + i].h * d)) * kH * kL;
								npt[off + i] -= 4.f * w[1] * iwsum * disp * (1.f - (geomSpines[off + i].h * d)) * kH * kL;
								npt[off + i - 1] += 2.f * w[2] * iwsum * disp * (1.f - (geomSpines[off + i].h * d)) * kH * kL;
								mmrm_assert(npt[off + i + 1].IsValid());
								mmrm_assert(npt[off + i].IsValid());
								mmrm_assert(npt[off + i - 1].IsValid());
							}
							bending[0] = wmat * geomSpines[off + i + 1].pt;
							bending[1] = npt[off + i];
							bending[2] = wmat * geomSpines[off + i - 1].pt;
							disp = (bending[1] - (1.f / 3.f) * (bending[0] + bending[1] + bending[2]));
							mmrm_assert(disp.IsValid());
							d = (max(disp.len2(), sqr(FLT_EPSILON)));
							mmrm_assert(NumberValid(d));
							if (fabs(d - sqr(geomSpines[off + i].h)) > FLT_EPSILON)
							{
								d = (float)isqrt_tpl(d);
								npt[off + i] -= 4.f * w[1] * iwsum * disp * (1.f - (geomSpines[off + i].h * d)) * kH * 0.05f * kL;
								mmrm_assert(npt[off + i].IsValid());
							}
						}
						// Note special handling for the first segment as the bending as defined above has
						// no notion for something to stand "up" and therefore we must manually handle
						// the first spine segment with additional care below
						w[0] = 0.f;
						w[1] = 0.f;
						w[2] = 1.f;
						iwsum = (w[0] + 2.f * w[1] + w[2]);
						iwsum = iwsum > 0.f ? 1.f / (iwsum) : 1.f;
						bending[0] = npt[off + 0] - Vec3(0, 0, 1);
						bending[1] = npt[off + 0];
						bending[2] = npt[off + 1];
						disp = (bending[1] - (1.f / 3.f) * (bending[0] + bending[1] + bending[2]));
						mmrm_assert(disp.IsValid());
						d = max(disp.len2(), sqr(FLT_EPSILON));
						mmrm_assert(NumberValid(d));
						if (fabs(d - sqr(geomSpines[off + 0].h)) > FLT_EPSILON)
						{
							d = (float)isqrt_tpl(d);
							npt[off + 1] += 2.f * w[2] * iwsum * disp * (1.f - (geomSpines[off + 0].h * d)) * kH * kL;
							mmrm_assert(npt[off + 1].IsValid());
						}
#endif
						// Enforce length of current spine to make sure vertices don't drift
						// apart
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
						vnpt0 = _mm_load_ps(&npt[off + 0].x);
#endif
						for (i = 0, i_f = -1.f; i < (int)geomSpineInfo[j].nSpineVtx - 1; ++i, i_f += 1.f)
						{
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
							vnpt1 = _mm_load_ps(&npt[off + i + 1].x);

							const vec4 vi0 = NVMath::Vec4(i_f);
							vsplen = _mm_set1_ps(geomSpines[off + i].len);

							const vec4 vdisp = Sub(vnpt0, vnpt1);
							vlen2 = DotProduct3to4(vdisp, vdisp);
							const vec4 vlen = _mm_sqrt_ps(Max(vlen2, vEpsilon));
							const vec4 vndisp = Mul(vdisp, rvlen);
							const vec4 vdiff = Sub(vlen, Mul(vsplen, vScale));

							const vec4 vKLiwsum = Mul(vdiff, Mul(vkL, viwsum));
							const vec4 vwsum0 = Mul(SelectFromVector(vOne, vZero, _mm_castsi128_ps(_mm_srai_epi32(_mm_castps_si128(vi0), 31))), vKLiwsum);
							const vec4 vwsum1 = vKLiwsum;
							const vec4 valpha0 = Mul(vwsum0, vndisp);
							const vec4 valpha1 = Mul(vwsum1, vndisp);

							vnpt0 = Add(vnpt0, valpha0);
							vnpt1 = Add(vnpt1, valpha1);

							_mm_store_ps(&npt[off + i].x, vnpt0);
							_mm_store_ps(&npt[off + i + 1].x, vnpt1);

							vnpt0 = vnpt1;
#else
							iwsum = 1.f / 2.f;
							disp = npt[off + i] - npt[off + i + 1];
							d = sqrt_tpl(max(disp.len2(), sqr(FLT_EPSILON)));
							const float diff = (d - geomSpines[off + i].len * fScale);
							ndisp = disp / d;
							npt[off + i] += (-(__fsel(i_f, 1.f, 0.f) * iwsum) * (diff) * ndisp) * kL;
							npt[off + i + 1] += (iwsum * (diff) * ndisp) * kL;
#endif
						}
					}
					while (++iter < max_iter);
					// Update final position and calculate actual velocity to be used in
					// next timestep
					opt[off + 0] = npt[off + 0];
					for (i = 1; i < (int)geomSpineInfo[j].nSpineVtx; ++i)
					{
#if MMRM_USE_VECTORIZED_SSE_INSTRUCTIONS
						const vec4 vnpt = _mm_load_ps(&npt[off + i].x);
						const vec4 vopt = _mm_load_ps(&opt[off + i].x);
						const vec4 vnvel = Mul(Sub(vnpt, vopt), vRDT);
						_mm_store_ps(&nvel[off + i].x, vnvel);
						_mm_store_ps(&opt[off + i].x, vnpt);
#else
						nvel[off + i] = (((npt[off + i] - opt[off + i]) * rdt));
						opt[off + i] = npt[off + i];
#endif
					}
				}
				// ... and convert back to fp16
				opt[off + 0] = ((iwmat * opt[off + 0]) * rScale);
#if MMRM_SIMULATION_USES_FP32
				spines[base + off + 0].vel.y += clamp_tpl(dtcur, 0.0f, MMRM_FIXED_STEP + (1.f / 1000.f)) * 1000.f;
				spines[base + off + 0].vel.x = max(spines[base + off + 0].vel.x - spines[base + off + 0].vel.y, 0.f);
#else
				spines[base + off + 0].vel.y += static_cast<int16>(clamp_tpl(dtcur, 0.0f, MMRM_FIXED_STEP + (1.f / 1000.f)) * 1000.f);
				spines[base + off + 0].vel.x = max(spines[base + off + 0].vel.x - spines[base + off + 0].vel.y, 0);
#endif
				for (i = 1; i < (int)geomSpineInfo[j].nSpineVtx; ++i)
				{
#if MMRM_SIMULATION_USES_FP32
					spines[base + off + i].vel = nvel[off + i];
					spines[base + off + i].pt = opt[off + i];
					opt[off + i] = ((iwmat * opt[off + i]) * rScale);
#else
					const Vec3& sv = nvel[off + i];
					float sv_x(clamp_tpl(sv.x, -127.f, 128.f) * (float)(1 << 8)), sv_y(clamp_tpl(sv.y, -127.f, 128.f) * (float)(1 << 8)), sv_z(clamp_tpl(sv.z, -127.f, 128.f) * (float)(1 << 8));
					const Vec3& sp = (opt[off + i] = ((iwmat * opt[off + i]) * rScale));
					float sp_x(clamp_tpl(sp.x, -127.f, 128.f) * (float)(1 << 8)), sp_y(clamp_tpl(sp.y, -127.f, 128.f) * (float)(1 << 8)), sp_z(clamp_tpl(sp.z, -127.f, 128.f) * (float)(1 << 8));
	#if MMRM_RENDER_DEBUG && MMRM_VISUALIZE_FORCES
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(sp, Col_Blue, sp + sv, Col_Green);
	#endif
					spines[base + off + i].vel.x = static_cast<int16>(sv_x);
					spines[base + off + i].vel.y = static_cast<int16>(sv_y);
					spines[base + off + i].vel.z = static_cast<int16>(sv_z);
					spines[base + off + i].pt.x = static_cast<int16>(sp_x);
					spines[base + off + i].pt.y = static_cast<int16>(sp_y);
					spines[base + off + i].pt.z = static_cast<int16>(sp_z);
#endif
				}
				// Convert spines to bones for skinning
				for (i = 0; i < (int)geomSpineInfo[j].nSpineVtx - 1; ++i)
				{
					Vec3 p0 = opt[off + i];
					Vec3 p1 = opt[off + i + 1];
					Vec3 p1p0 = p1 - p0;
					Vec3 opt_normal = (geomSpines[off + i + 1].pt - geomSpines[off + i].pt);
					opt_normal *= (float)isqrt_tpl(max(opt_normal.len2(), sqr(FLT_EPSILON)));
					p1p0 *= (float)isqrt_tpl(max(p1p0.len2(), sqr(FLT_EPSILON)));
					Quat ql = Quat::CreateRotationV0V1(opt_normal, p1p0);
					bones[boneIdx + i] = wq * DualQuat(ql, (p0 - ql * (geomSpines[off + i].pt)));
				}
				off += geomSpineInfo[j].nSpineVtx;
				boneIdx += geomSpineInfo[j].nSpineVtx - 1;
			}
			base += geom->numSpineVtx;
		}

		// Bake the geometry into the buffer
		{
			MMRM_FRAME_PROFILER("PVRN BAKING");
			for (size_t n = 0; n < geom->numChunks[nLod]; ++n)
			{
				const SMMRMChunk& chunk = geom->pChunks[nLod][n];
				uint32 siv = context.updateChunks[n].voff, & iv = context.updateChunks[n].voff;
				uint32& ii = context.updateChunks[n].ioff;
				if (spines && chunk.weights)
				{
					if (chunk.normals)
						UpdateGeneralTangentsNormals(
						  general + iv
						  , tangents + iv
						  , normals + iv
						  , chunk.skin_vertices
						  , bones
						  , fScale
						  , geom->maxSpinesPerVtx
						  , chunk.nvertices);
					else
						UpdateGeneralTangents(
						  general + iv
						  , tangents + iv
						  , chunk.skin_vertices
						  , bones
						  , fScale
						  , geom->maxSpinesPerVtx
						  , chunk.nvertices);
				}
				else
				{
					if (chunk.normals)
						UpdateGeneralTangentsNormals(
						  general + iv
						  , tangents + iv
						  , normals + iv
						  , chunk.skin_vertices
						  , chunk.nvertices
						  , tmat);
					else
						UpdateGeneralTangents(
						  general + iv
						  , tangents + iv
						  , chunk.skin_vertices
						  , chunk.nvertices
						  , tmat);
				}
				UpdateIndices(idxBuf + ii, chunk.indices, siv, chunk.nindices);
				iv += chunk.nvertices;
				ii += chunk.nindices;
			}
		}
	}
}

static inline void MergeInstanceListDeform(SMMRMInstanceContext& context)
{
	MEMORY_SCOPE_CHECK_HEAP();
	SMMRMGeometry* geom = context.geom;
	SMMRMUpdateContext* update = context.update;
	SMMRMDeformVertex* deform_vertices = context.deform;
	SMMRMDeform* deform = (geom->deform);
	SVF_P3S_C4B_T2S* general = update->general;
	SPipTangents* tangents = update->tangents;
	vtx_idx* idxBuf = update->idxBuf;
	mmrm_printf("updating %d samples\n", context.amount);
#if MMRM_USE_BOUNDS_CHECK
	mmrm_printf("updating %#x<==>%#x general \n", (unsigned int)general, (unsigned int)TLS_GET(unsigned int, s_general_end));
	mmrm_printf("updating %#x<==>%#x tangents\n", (unsigned int)tangents, (unsigned int)TLS_GET(unsigned int, s_tangents_end));
	mmrm_printf("updating %#x<==>%#x idxBuf\n", (unsigned int)idxBuf, (unsigned int)TLS_GET(unsigned int, s_idx_end));
#endif
	primitives::sphere* colliders = context.colliders;
	SMMRMProjectile* projectiles = context.projectiles;
	PREFAST_SUPPRESS_WARNING(6255)
	int i = 0, k = 0, off = 0, n = 0;
	int ncolliders = context.ncolliders, nprojectiles = context.nprojectiles;
	Vec3 ls0, ls1;
	Matrix34 wmat, smat, tmat;
	Matrix33 rmat;
	const float fExtents = c_MergedMeshesExtent;
	const Vec3 origin = context.min;
	Quat qr;
	float c = 0, d = 0, w[3] = { 0 }, iwsum = 0, dt = 0.0f, dtcur = 0.f, dttot = context.dt, abstime = context.abstime
	, airResistance = update->group->physConfig.airResistance
	, airModulation = update->group->physConfig.airModulation
	, airFrequency = update->group->physConfig.airFrequency
	, cairFrequency = sqr(cosf(airFrequency * abstime))
	, damping = update->group->physConfig.fDamping
	, kH = update->group->physConfig.kH
	, kL = 1.f
	, var = 0.f
	, variance = update->group->physConfig.variance
	, i_step = 1.f / (float)deform->nvertices, i_f = (deform->nvertices * (context.frame_count & 0xffff)) * i_step
	, rdt;
	int iter = 0, max_iter = update->group->physConfig.max_iter;
	dttot += update->dtscale;
	update->dtscale = 0.f;
	for (k = 0; k < (int)context.amount && context.wind; ++k)
	{
		const SMMRMInstance& sample = context.samples[k];
		const Vec3 sample_pos = ConvertInstanceAbsolute(sample, origin, context.rotationOrigin, context.zRotation, fExtents);
		const float fScale = (1.f / VEGETATION_CONV_FACTOR) * sample.scale;
		DecompressQuat(qr, sample);
		wmat = Matrix34(rmat = Matrix33(qr), sample_pos) * (smat = Matrix34::CreateScale(Vec3(fScale, fScale, fScale)));
		tmat = Matrix34::CreateTranslationMat(-context.centre);
		iter = 0;
		Vec3 ctr, offs, disp, ndisp, dw0, dw, bending[3];
		off = k * deform->nvertices;
		// Simple PBD based deform
		dtcur = dttot;
		while (dtcur > MMRM_FIXED_STEP)
		{
			dtcur -= (dt = min(MMRM_FIXED_STEP, dtcur));
			rdt = 1.f / dt;
			for (i = 0; i < (int)deform->nvertices; ++i, i_f += i_step)
			{
				ctr = deform_vertices[off + i].pos[0];
				offs = Vec3((ctr.x - context.min.x) / (context.max.x - context.min.x), (ctr.y - context.min.y) / (context.max.y - context.min.y), (ctr.z - context.min.z) / (context.max.z - context.min.z));
				dw = SampleWind(offs, context.wind);
#if MMRM_RENDER_DEBUG && MMRM_VISUALIZE_WINDSAMPLES
				TraceWindSample(offs, dw, context);
#endif
				var = sqrt_tpl(dw.len2() * sqr(variance));
				dw += Vec3(cry_random(0.0f, var), cry_random(0.0f, var), cry_random(0.0f, var));
				Vec3 wind = dw * airResistance * (sqr(cosf(airModulation * i_f)) * 0.5f + cairFrequency * 0.5f);
				(deform_vertices[off + i].vel += deform->invmass[i] * (wind + Vec3(0, 0, -9.81f)) * dt) *= (1.f - (damping * dt));
				deform_vertices[off + i].pos[1] = deform_vertices[off + i].pos[0] + deform_vertices[off + i].vel * dt;
#if MMRM_RENDER_DEBUG && MMRM_VISUALIZE_FORCES
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(deform_vertices[off + i].pos[1], Col_Blue, deform_vertices[off + i].pos[1] + deform_vertices[off + i].vel, Col_Red);
#endif
				mmrm_assert(deform_vertices[off + i].pos[1].IsValid());
				Lineseg line(deform_vertices[off + i].pos[0], deform_vertices[off + i].pos[1]);
			}
			do
			{
				for (i = 0; i < (int)deform->nconstraints; ++i)
				{
					const SMMRMDeformConstraint& constraint = deform->constraints[i];
					switch (constraint.type)
					{
					case SMMRMDeformConstraint::DC_EDGE:
						w[0] = deform->invmass[constraint.edge[0]];
						w[1] = deform->invmass[constraint.edge[1]];
						iwsum = (w[0] + w[1]) > 0.f ? 1.f / (w[0] + w[1]) : 1.f;
						disp = deform_vertices[off + constraint.edge[1]].pos[1] - deform_vertices[off + constraint.edge[0]].pos[1];
						mmrm_assert(disp.IsValid());
						d = sqrt_tpl(max(disp.len2(), sqr(FLT_EPSILON)));
						mmrm_assert(NumberValid(d));
						ndisp = disp / d;
						mmrm_assert(ndisp.IsValid());
						deform_vertices[off + constraint.edge[0]].pos[1] += (w[0] * iwsum * (d - constraint.edge_distance * fScale) * ndisp) * constraint.k * kL;
						deform_vertices[off + constraint.edge[1]].pos[1] += (-w[1] * iwsum * (d - constraint.edge_distance * fScale) * ndisp) * constraint.k * kL;
						mmrm_assert(deform_vertices[off + constraint.edge[0]].pos[1].IsValid());
						mmrm_assert(deform_vertices[off + constraint.edge[1]].pos[1].IsValid());
						mmrm_assert((deform_vertices[off + constraint.edge[0]].pos[1] - deform_vertices[off + constraint.edge[0]].pos[0]).len() < 50.f);
						mmrm_assert((deform_vertices[off + constraint.edge[1]].pos[1] - deform_vertices[off + constraint.edge[1]].pos[0]).len() < 50.f);
						break;
					case SMMRMDeformConstraint::DC_BENDING:
						w[0] = deform->invmass[constraint.bending[0]];
						w[1] = deform->invmass[constraint.bending[1]];
						w[2] = deform->invmass[constraint.bending[2]];
						iwsum = (w[0] + 2.f * w[1] + w[2]);
						iwsum = iwsum > 0.f ? 1.f / (iwsum) : 1.f;
						bending[0] = deform_vertices[off + constraint.bending[0]].pos[1];
						bending[1] = deform_vertices[off + constraint.bending[1]].pos[1];
						bending[2] = deform_vertices[off + constraint.bending[2]].pos[1];
						disp = (bending[1] - (1.f / 3.f) * (bending[0] + bending[1] + bending[2]));
						mmrm_assert(disp.IsValid());
						mmrm_assert(NumberValid(d));
						d = max(disp.len2(), sqr(FLT_EPSILON));
						if (fabs(d - sqr(constraint.displacement)) > FLT_EPSILON)
						{
							d = (float)isqrt_tpl(d);
							deform_vertices[off + constraint.bending[0]].pos[1] += 2.f * w[0] * iwsum * disp * (1.f - (constraint.displacement * d)) * constraint.k * kH * kL;
							deform_vertices[off + constraint.bending[1]].pos[1] -= 4.f * w[1] * iwsum * disp * (1.f - (constraint.displacement * d)) * constraint.k * kH * kL;
							deform_vertices[off + constraint.bending[2]].pos[1] += 2.f * w[2] * iwsum * disp * (1.f - (constraint.displacement * d)) * constraint.k * kH * kL;
							mmrm_assert(deform_vertices[off + constraint.bending[0]].pos[1].IsValid());
							mmrm_assert(deform_vertices[off + constraint.bending[1]].pos[1].IsValid());
							mmrm_assert(deform_vertices[off + constraint.bending[2]].pos[1].IsValid());
							mmrm_assert((deform_vertices[off + constraint.bending[0]].pos[1] - deform_vertices[off + constraint.bending[0]].pos[0]).len() < 50.f);
							mmrm_assert((deform_vertices[off + constraint.bending[1]].pos[1] - deform_vertices[off + constraint.bending[1]].pos[0]).len() < 50.f);
							mmrm_assert((deform_vertices[off + constraint.bending[2]].pos[1] - deform_vertices[off + constraint.bending[2]].pos[0]).len() < 50.f);
						}
						break;
					}
					;
				}
				for (i = 0; i < (int)deform->nvertices; ++i)
				{
					if (deform->invmass[i] == 0.f)
						continue;
					Lineseg ls = Lineseg(deform_vertices[off + i].pos[0], deform_vertices[off + i].pos[1]);
					Vec3 lnormal = (deform_vertices[off + i].pos[1] - deform_vertices[off + i].pos[0]);
					const float len2 = lnormal.len2();
					if (len2 < FLT_EPSILON)
						continue;
					lnormal *= sqrt_tpl(max(len2, sqr(FLT_EPSILON)));
					for (int q = 0; q < ncolliders; ++q)
					{
						disp = (deform_vertices[off + i].pos[1] - colliders[q].center);
						d = disp.len2();
						if (d < colliders[q].r * colliders[q].r)
						{
							ndisp = disp * (d = (float)isqrt_tpl(d));
							deform_vertices[off + i].pos[1] = colliders[q].center + ndisp * colliders[q].r;
						}
						else
							switch (_Lineseg_Sphere(ls, Sphere(colliders[q].center, colliders[q].r), ls0, ls1))
							{
							case 0x2:
								disp = (deform_vertices[off + i].pos[0] - colliders[q].center);
								ndisp = disp * (float)isqrt_tpl(max(disp.len2(), FLT_EPSILON));
								ctr = colliders[q].center + ndisp * colliders[q].r;
								if (false)
								{
								case 0x1:
								case 0x3:
									disp = (ls0 - colliders[q].center);
									ndisp = disp * (float)isqrt_tpl(max(disp.len2(), FLT_EPSILON));
									ctr = ls0;
								}
								deform_vertices[off + i].pos[1] = ctr + (ndisp - (ndisp * lnormal) * lnormal) * 1.f * dt;
								mmrm_assert((deform_vertices[off + i].pos[1] - deform_vertices[off + i].pos[0]).len() < 50.f);
							default:
								break;
							}
					}
					// Perform (simple) swept checks for projectiles
					for (int q = 0; q < nprojectiles; ++q)
					{
						const SMMRMProjectile& projectile = projectiles[q];
						Lineseg pl(projectile.pos[0], projectile.pos[1]);
						float radiusSq = sqr(projectile.r), t0, t1, distanceSq;
						if ((distanceSq = Distance::Lineseg_LinesegSq(ls, pl, &t0, &t1)) > radiusSq)
							continue;
						deform_vertices[off + i].pos[1] += projectile.dir * sqr((1.f - (distanceSq / radiusSq))) * 0.25f * dt;
						mmrm_assert((deform_vertices[off + i].pos[1] - deform_vertices[off + i].pos[0]).len() < 50.f);
						mmrm_assert(deform_vertices[off + i].pos[1].IsValid());
					}
				}
			}
			while (++iter < max_iter);
			for (i = 0; i < (int)deform->nvertices; ++i)
			{
				deform_vertices[off + i].vel = ((deform_vertices[off + i].pos[1] - deform_vertices[off + i].pos[0]) * rdt);
				deform_vertices[off + i].pos[0] = deform_vertices[off + i].pos[1];
#if MMRM_RENDER_DEBUG && MMRM_VISUALIZE_FORCES
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(deform_vertices[off + i].pos[0], Col_Blue, deform_vertices[off + i].pos[0] + deform_vertices[off + i].vel, Col_Pink);
#endif
				mmrm_assert(deform_vertices[off + i].pos[0].IsValid());
			}
		}
		update->dtscale += clamp_tpl(dtcur, 0.0f, MMRM_FIXED_STEP + (1.f / 1000.f));

		// Skip if not visible
		int nLod = sample.lastLod;
		if (nLod < 0)
			continue;

		// Bake the geometry into the buffer
		for (n = 0; n < (int)geom->numChunks[nLod]; ++n)
		{
			const SMMRMChunk& chunk = geom->pChunks[nLod][n];
			uint32 siv = context.updateChunks[n].voff, & iv = context.updateChunks[n].voff;
			uint32& ii = context.updateChunks[n].ioff;
			UpdateGeneralTangents(general + iv, tangents + iv, chunk.skin_vertices, deform_vertices + off, deform->mapping, tmat, chunk.nvertices);
			UpdateIndices(idxBuf + ii, chunk.indices, siv, chunk.nindices);
			iv += chunk.nvertices;
			ii += chunk.nindices;
		}
	}
}
//}

////////////////////////////////////////////////////////////////////////////////
void SMMRMGroupHeader::CullInstances(CCamera* cam, Vec3* origin, Vec3* rotationOrigin, float zRotation, int flags)
{
	MEMORY_SCOPE_CHECK_HEAP();
	MMRM_PROFILE_FUNCTION();

	SMMRMGroupHeader* header = this;
	size_t numSamplesVisible = 0, j = 0;
	const size_t nsamples = header->numSamples;
	SMMRMInstance* samples = header->instances;
	const Vec3 _origin(*origin);
	const Vec3 _rotationOrigin(*rotationOrigin);

#if MMRM_USE_BOUNDS_CHECK
	mmrm_assert(CryInterlockedIncrement(const_cast<volatile int*>(&header->debugLock)) == 1);
#endif

	SMMRMInstanceContext ctx;
	ctx.geom = header->procGeom;
	ctx.cam = cam;
	ctx.flags = flags;
	ctx.maxViewDistSq = header->maxViewDistance * header->maxViewDistance;
	ctx.lodRatioSq = header->lodRationNorm * header->lodRationNorm;
	ctx.diameterSq = ((ctx.geom->aabb.max - ctx.geom->aabb.min)).len2();
	ctx.zRotation = zRotation;
	ctx.rotationOrigin = _rotationOrigin;

	const size_t vsb = header->numVisbleChunks * sizeof(SMMRMVisibleChunk);
	PREFAST_SUPPRESS_WARNING(6255)
	SMMRMVisibleChunk * visChunks = (SMMRMVisibleChunk*)alloca(vsb);
	mmrm_printf("alloced %#x bytes\n", vsb);
	memset(visChunks, 0, vsb);

	do
	{
		ctx.amount = nsamples;
		ctx.samples = samples;

		// Perform the actual culling
		numSamplesVisible += CullInstanceList(ctx, visChunks, _origin, _rotationOrigin, zRotation);
		j += ctx.amount;

	}
	while (j < nsamples);

	// Blit the visible chunk data back to main memory
	header->numSamplesVisible = numSamplesVisible;
	memcpy(header->visibleChunks, visChunks, vsb);

#if MMRM_USE_BOUNDS_CHECK
	mmrm_assert(CryInterlockedDecrement(const_cast<volatile int*>(&header->debugLock)) == 0);
#endif
}

////////////////////////////////////////////////////////////////////////////////
void SMMRMUpdateContext::MergeInstanceMeshesSpines(
  CCamera*
  , int flags)
{
	MEMORY_SCOPE_CHECK_HEAP();
	SMMRMUpdateContext* update = this;
	const SMMRMGroupHeader* header = update->group;
	size_t j = 0;
	const size_t nsamples = header->numSamples;
	SMMRMInstance* samples = header->instances;
	SMMRMSpineVtxBase* spines = header->spines;
#if MMRM_USE_BOUNDS_CHECK
	mmrm_assert(CryInterlockedIncrement(const_cast<volatile int*>(&header->debugLock)) == 1);
#endif

	SMMRMInstanceContext ctx;
	ctx.geom = header->procGeom;
	ctx.cam = NULL;
	ctx.update = update;
	ctx.flags = flags;
	ctx.max_iter = max_iter;
	ctx.colliders = colliders;
	ctx.ncolliders = ncolliders;
	ctx.projectiles = projectiles;
	ctx.nprojectiles = nprojectiles;
	ctx.dt = min(max(dt, 0.f), 0.080f);
	ctx.dtscale = 0.f;
	ctx.abstime = abstime;
	ctx.wind = wind;
	ctx.zRotation = zRotation;
	ctx.rotationOrigin = rotationOrigin;
	ctx.min = _min;
	ctx.max = _max;
	ctx.centre = (_max + _min) * 0.5f;
	ctx.use_spines = this->use_spines;
	ctx.frame_count = this->frame_count;

	ctx.updateChunks = &update->chunks[0];

	do
	{
		ctx.amount = nsamples;
		ctx.samples = samples;
		ctx.spines = spines && ctx.use_spines ? spines : NULL;

		// Perform the actual buffering
		MergeInstanceList(ctx);
		j += ctx.amount;

	}
	while (j < nsamples);

	CryInterlockedDecrement(update->updateFlag);
	mmrm_assert(update->updateFlag >= 0);

#if MMRM_USE_BOUNDS_CHECK
	mmrm_assert(CryInterlockedDecrement(const_cast<volatile int*>(&header->debugLock)) == 0);
#endif
}

////////////////////////////////////////////////////////////////////////////////
void SMMRMUpdateContext::MergeInstanceMeshesDeform(
  CCamera*
  , int flags)
{
	mmrm_printf("deform begin\n");

	MEMORY_SCOPE_CHECK_HEAP();
	MMRM_PROFILE_FUNCTION();

	SMMRMUpdateContext* update = this;
	const SMMRMGroupHeader* header = update->group;
	size_t j = 0;
	const size_t nsamples = header->numSamples;
	SMMRMInstance* samples = header->instances;
	SMMRMSpineVtxBase* spines = header->spines;
#if MMRM_USE_BOUNDS_CHECK
	mmrm_assert(CryInterlockedIncrement(const_cast<volatile int*>(&header->debugLock)) == 1);
#endif

	SMMRMInstanceContext ctx;
	ctx.geom = header->procGeom;
	ctx.cam = NULL;
	ctx.update = update;
	ctx.flags = flags;
	ctx.max_iter = max_iter;
	ctx.colliders = colliders;
	ctx.ncolliders = ncolliders;
	ctx.projectiles = projectiles;
	ctx.nprojectiles = nprojectiles;
	ctx.dt = min(max(dt, 0.f), 0.080f);
	ctx.dtscale = 0.f;
	ctx.abstime = abstime;
	ctx.wind = wind;
	ctx.rotationOrigin = Vec3(ZERO);
	ctx.min = _min;
	ctx.max = _max;
	ctx.centre = (_max + _min) * 0.5f;
	ctx.use_spines = this->use_spines;
	ctx.frame_count = this->frame_count;

	ctx.updateChunks = &update->chunks[0];

	do
	{
		ctx.amount = nsamples;
		ctx.samples = samples;
		ctx.deform = header->deform_vertices;

		// Perform the actual deformation simulation and bake the results into
		// the provided buffers.
		MergeInstanceListDeform(ctx);
		j += ctx.amount;

	}
	while (j < nsamples);

	CryInterlockedDecrement(update->updateFlag);
	mmrm_assert(update->updateFlag >= 0);

#if MMRM_USE_BOUNDS_CHECK
	mmrm_assert(CryInterlockedDecrement(const_cast<volatile int*>(&header->debugLock)) == 0);
#endif
}

struct SMergedMeshInstanceSorter
{
	uint32 m_frameId;

	SMergedMeshInstanceSorter(uint32 frameId)
		: m_frameId()
	{}

	bool Visible(const CMergedMeshRenderNode* node) const
	{
		return (m_frameId - node->m_LastDrawFrame) < 4;
	}

	bool operator()(const CMergedMeshRenderNode* a, const CMergedMeshRenderNode* b) const
	{
		const float distSq_A = a->m_DistanceSQ;
		const float distSq_B = b->m_DistanceSQ;
		const bool visible_A = Visible(a);
		const bool visible_B = Visible(b);

		// Prefer closer instances
		if (distSq_A < distSq_B)
			return true;
		if (distSq_A > distSq_B)
			return false;

		// Prefer visible instances
		if (visible_A && !visible_B)
			return true;
		if (!visible_A && visible_B)
			return false;

		// Fix inconsistencies
		return a < b;
	}
};

void CMergedMeshesManager::SortActiveInstances_Async(const SRenderingPassInfo passInfo)
{
	uint32 frameId = passInfo.GetMainFrameID();
	const Vec3& vPos = passInfo.GetCamera().GetPosition();

	if (m_ActiveNodes.size() == 0)
	{
		for (size_t i = 0; i < HashDimXY; ++i)
			for (size_t j = 0; j < HashDimXY; ++j)
				for (size_t k = 0; k < HashDimZ; ++k)
				{
					NodeListT& list = m_Nodes[i & (HashDimXY - 1)][j & (HashDimXY - 1)][k & (HashDimZ - 1)];
					for (NodeListT::iterator it = list.begin(); it != list.end(); ++it)
					{
						CMergedMeshRenderNode* node = *it;
						if (!node->IsActive())
						{
							node->SetActive(1);
							m_ActiveNodes.push_back(node);
						}
					}
				}
	}

	{
		const float fMaxActiveDistExtSQ = pow(GetCVars()->e_MergedMeshesActiveDist + c_MergedMeshesExtent, 2.f);

		size_t nActiveNodes = m_ActiveNodes.size();
		for (size_t i = 0; i < nActiveNodes; ++i)
		{
			CMergedMeshRenderNode* node = m_ActiveNodes[i];
			node->m_DistanceSQ = (vPos - node->m_internalAABB.GetCenter()).len2();
			if (node->m_LastDrawFrame == frameId && node->IsInVisibleSet() == 0)
			{
				node->SetInVisibleSet(1);
				m_VisibleNodes.push_back(node);
			}
			else if (
			  node->m_State == CMergedMeshRenderNode::INITIALIZED ||
			  node->m_State == CMergedMeshRenderNode::DIRTY ||
			  node->m_State == CMergedMeshRenderNode::PREPARING ||
			  node->m_State == CMergedMeshRenderNode::PREPARED)
			{
				// move very far inactive nodes into StreamedOut list
				if (!(gEnv->IsEditor() || gEnv->IsDedicated()) && (node->m_DistanceSQ > fMaxActiveDistExtSQ) && !node->IsInVisibleSet())
				{
					m_StreamedOutNodes.push_back(node);
					m_ActiveNodes.erase(std::remove(m_ActiveNodes.begin(), m_ActiveNodes.end(), node), m_ActiveNodes.end());
					nActiveNodes = m_ActiveNodes.size();
					i--;
				}
			}
		}
	}

	// Sort the active set by distance
	{
		std::sort(m_ActiveNodes.begin(), m_ActiveNodes.end(),
		          SMergedMeshInstanceSorter(frameId));
	}

	// Sort the visible set by distance
	{
		std::sort(m_VisibleNodes.begin(), m_VisibleNodes.end(),
		          SMergedMeshInstanceSorter(frameId));
	}

	{
		// check if some nodes must be moved from StreamedOut into Active nodes list
		const float fMaxActiveDistSQ = pow(GetCVars()->e_MergedMeshesActiveDist, 2.f);
		size_t nStreamedOutSize = m_StreamedOutNodes.size();
		size_t nChecksPerFrame = min((size_t)2000, nStreamedOutSize);
		for (size_t t = 0; t < nChecksPerFrame; ++t)
		{
			static size_t nId = ~0;
			nId++;
			if (nId >= nStreamedOutSize)
				nId = 0;

			CMergedMeshRenderNode* node = m_StreamedOutNodes[nId];
			node->m_DistanceSQ = (vPos - node->m_internalAABB.GetCenter()).len2();
			if (node->m_DistanceSQ < fMaxActiveDistSQ || node->StreamedIn())
			{
				m_ActiveNodes.push_back(node);
				m_StreamedOutNodes.erase(std::remove(m_StreamedOutNodes.begin(), m_StreamedOutNodes.end(), node), m_StreamedOutNodes.end());
				nStreamedOutSize = m_StreamedOutNodes.size();
				nId--;
			}
		}
	}
}

size_t CMergedMeshRenderNode::QueryDensity(const Vec3& pos, IMergedMeshesManager::TFixedSurfacePtrTypeArray& surfaceTypes, IMergedMeshesManager::TFixedDensityArray& density)
{
	size_t i = 0;
	if (!StreamedIn() || m_internalAABB.IsContainPoint(pos) == false)
		return i;
	const float fExtents = c_MergedMeshesExtent;
	const float rExtents = 1.f / fExtents;

	Vec3 rpos = (pos - m_internalAABB.min);
	rpos.x = ((rpos.x) * rExtents) * (MMRM_DENSITY_DIM - 1);
	rpos.y = ((rpos.y) * rExtents) * (MMRM_DENSITY_DIM - 1);
	rpos.z = ((rpos.z) * rExtents) * (MMRM_DENSITY_DIM - 1);
	float px = clamp_tpl(rpos.x, 0.f, (float)(MMRM_DENSITY_DIM)-FLT_EPSILON);
	float py = clamp_tpl(rpos.y, 0.f, (float)(MMRM_DENSITY_DIM)-FLT_EPSILON);
	float pz = clamp_tpl(rpos.z, 0.f, (float)(MMRM_DENSITY_DIM)-FLT_EPSILON);
	float ftix(floorf(px)), ftiy(floorf(py)), ftiz(floorf(pz));
	float fx = px - ftix, fy = py - ftiy, fz = pz - ftiz;
	int d = MMRM_DENSITY_DIM, ix = static_cast<int>(ftix), iy = static_cast<int>(ftiy), iz = static_cast<int>(ftiz);

	for (i = 0; i < MMRM_MAX_SURFACE_TYPES && m_surface_types[i]; ++i)
	{
		float result = 0;
		result += m_density[(ix + 0) + (iy + 0) * d + (iz + 0) * sqr(d)].density_u8[i] * (1.f - fz) * (1.f - fy) * (1.f - fx);
		result += m_density[(ix + 1) + (iy + 0) * d + (iz + 0) * sqr(d)].density_u8[i] * (1.f - fz) * (1.f - fy) * (fx);
		result += m_density[(ix + 0) + (iy + 1) * d + (iz + 0) * sqr(d)].density_u8[i] * (1.f - fz) * (fy) * (1.f - fx);
		result += m_density[(ix + 1) + (iy + 1) * d + (iz + 0) * sqr(d)].density_u8[i] * (1.f - fz) * (fy) * (fx);
		result += m_density[(ix + 0) + (iy + 0) * d + (iz + 1) * sqr(d)].density_u8[i] * (fz) * (1.f - fy) * (1.f - fx);
		result += m_density[(ix + 1) + (iy + 0) * d + (iz + 1) * sqr(d)].density_u8[i] * (fz) * (1.f - fy) * (fx);
		result += m_density[(ix + 0) + (iy + 1) * d + (iz + 1) * sqr(d)].density_u8[i] * (fz) * (fy) * (1.f - fx);
		result += m_density[(ix + 1) + (iy + 1) * d + (iz + 1) * sqr(d)].density_u8[i] * (fz) * (fy) * (fx);

		surfaceTypes[i] = m_surface_types[i];
		density[i] = result;
	}

	return i;
}

void CMergedMeshRenderNode::CalculateDensity()
{
	const float fExtents = c_MergedMeshesExtent;
	const float rExtents = 1.f / fExtents;

	// NULL any previously set surface types
	memset(m_surface_types, 0x0, sizeof(m_surface_types));
	if (m_density == NULL)
		m_density = (SSampleDensity*)CryModuleMemalign(sizeof(SSampleDensity) * cube(MMRM_DENSITY_DIM), 16);
	memset(m_density, 0x0, sizeof(SSampleDensity) * cube(MMRM_DENSITY_DIM));

	for (size_t i = 0; i < m_nGroups; ++i)
	{
		SMMRMGroupHeader* header = &m_groups[i];
		SMMRMGeometry* geom = header->procGeom;
		StatInstGroup& instGroup = GetObjManager()->m_lstStaticTypes[geom->srcGroupId];
		// Hopefully correctly extract the surface type of the material
		IMaterial* material = NULL;
		if (instGroup.pMaterial)
			material = instGroup.pMaterial;
		else
			material = instGroup.pStatObj->GetMaterial();
		if (material == NULL)
			continue;

		// For materials with multiple sub-materials, use the surface type of the first sub-material.
		if (material->GetSubMtlCount() > 0)
		{
			material = material->GetSubMtl(0);
		}

		ISurfaceType* surfaceType = material->GetSurfaceType();
		if (surfaceType == NULL)
			continue;

		// Assign the surfacetype an index into our list of surface types
		size_t si = (size_t)-1, lastSurfaceType = 0;
		for (size_t j = 0; j < MMRM_MAX_SURFACE_TYPES && m_surface_types[j]; ++j, ++lastSurfaceType)
		{
			if (m_surface_types[j] == surfaceType)
			{
				si = j;
				break;
			}
		}
		if (si == (size_t)-1)
		{
			if (lastSurfaceType >= MMRM_MAX_SURFACE_TYPES)
				continue;
			m_surface_types[si = lastSurfaceType] = surfaceType;
		}

		const AABB& aabb = geom->aabb;
		const Vec3& centre = aabb.GetCenter();
		const Vec3& size = aabb.GetSize() * 0.5f;

		const size_t numSamples = (header->instances ? header->numSamples : 0);
		for (size_t j = 0; j < numSamples; ++j)
		{
			Vec3 pos = ConvertInstanceAbsolute(header->instances[j], m_internalAABB.min, m_pos, m_zRotation, fExtents);
			Vec3 vmin = (pos - size) - m_internalAABB.min, vmax = (pos + size) - m_internalAABB.min;

			int iminx = (int)(((vmin.x) * rExtents) * (MMRM_DENSITY_DIM - 1));
			int iminy = (int)(((vmin.y) * rExtents) * (MMRM_DENSITY_DIM - 1));
			int iminz = (int)(((vmin.z) * rExtents) * (MMRM_DENSITY_DIM - 1));

			int imaxx = (int)(((vmax.x) * rExtents) * (MMRM_DENSITY_DIM - 1));
			int imaxy = (int)(((vmax.y) * rExtents) * (MMRM_DENSITY_DIM - 1));
			int imaxz = (int)(((vmax.z) * rExtents) * (MMRM_DENSITY_DIM - 1));

			iminx = clamp_tpl(iminx, 0, MMRM_DENSITY_DIM - 1);
			iminy = clamp_tpl(iminy, 0, MMRM_DENSITY_DIM - 1);
			iminz = clamp_tpl(iminz, 0, MMRM_DENSITY_DIM - 1);

			imaxx = clamp_tpl(imaxx, 0, MMRM_DENSITY_DIM - 1);
			imaxy = clamp_tpl(imaxy, 0, MMRM_DENSITY_DIM - 1);
			imaxz = clamp_tpl(imaxz, 0, MMRM_DENSITY_DIM - 1);

			for (int ik = iminz; ik <= imaxz; ++ik)
				for (int ij = iminy; ij <= imaxy; ++ij)
					for (int ii = iminx; ii <= imaxx; ++ii)
					{
						uint32 density = m_density[ii + ij * MMRM_DENSITY_DIM + ik * sqr(MMRM_DENSITY_DIM)].density_u8[si];
						density = min(density + 0x20u, 0xffu);
						m_density[ii + ij * MMRM_DENSITY_DIM + ik * sqr(MMRM_DENSITY_DIM)].density_u8[si] = static_cast<uint8>(density);
					}
		}
	}
}

void CMergedMeshRenderNode::InitializeSamples(float fExtents, const uint8* pBuffer)
{
	Vec3 vInternalAABBMin = m_internalAABB.min;
	size_t stepcount = 0u;
	for (size_t i = 0; i < m_nGroups; ++i)
	{
		SMMRMGroupHeader* header = &m_groups[i];
		if (!header->splitGroup)
		{
			const SMergedMeshSectorChunk& sectorChunk = *AdvancePtr<SMergedMeshSectorChunk>(pBuffer);
			if (sectorChunk.ver != c_MergedMeshChunkVersion)
				continue;
		}

		for (size_t j = 0; j < header->numSamples; ++j)
		{
			SMergedMeshInstanceCompressed* pSampleChunk = AdvancePtr<SMergedMeshInstanceCompressed>(pBuffer);
			if (m_groups[i].specMismatch)
				continue;

			SwapEndian(pSampleChunk->pos_x, eLittleEndian);
			SwapEndian(pSampleChunk->pos_y, eLittleEndian);
			SwapEndian(pSampleChunk->pos_z, eLittleEndian);
			SwapEndian(pSampleChunk->scale, eLittleEndian);
			SwapEndian(pSampleChunk->rot[0], eLittleEndian);
			SwapEndian(pSampleChunk->rot[1], eLittleEndian);
			SwapEndian(pSampleChunk->rot[2], eLittleEndian);
			SwapEndian(pSampleChunk->rot[3], eLittleEndian);

			header->instances[j].pos_x = pSampleChunk->pos_x;
			header->instances[j].pos_y = pSampleChunk->pos_y;
			header->instances[j].pos_z = pSampleChunk->pos_z;
			header->instances[j].qx = pSampleChunk->rot[0];
			header->instances[j].qy = pSampleChunk->rot[1];
			header->instances[j].qz = pSampleChunk->rot[2];
			header->instances[j].qw = pSampleChunk->rot[3];
			header->instances[j].scale = pSampleChunk->scale;
			header->instances[j].lastLod = -2;
		}
	}

	CalculateDensity();
}

void CMergedMeshRenderNode::InitializeSpines()
{
	const float fExtents = c_MergedMeshesExtent;
	Vec3 vInternalAABBMin = m_internalAABB.min;
	Quat q;
	for (size_t i = 0; i < m_nGroups; ++i)
	{
		SMMRMGroupHeader* header = &m_groups[i];
		if (m_groups[i].specMismatch)
			continue;
		for (size_t j = 0, base = 0; j < header->numSamples; ++j)
		{
			uint16 pos[3] = { uint16(header->instances[j].pos_x), uint16(header->instances[j].pos_y), uint16(header->instances[j].pos_z) };
			const float fScale = (1.f / VEGETATION_CONV_FACTOR) * header->instances[j].scale;
			DecompressQuat(q, header->instances[j]);
			Matrix34 wmat = CreateRotationQ(q, ConvertInstanceAbsolute(pos, vInternalAABBMin, m_pos, m_zRotation, fExtents)) * Matrix34::CreateScale(Vec3(fScale, fScale, fScale));
			for (size_t k = 0; header->procGeom->numSpines && k < header->procGeom->numSpineVtx; ++k, ++base)
			{
#if MMRM_SIMULATION_USES_FP32
				header->spines[base].pt = wmat * header->procGeom->pSpineVtx[k].pt;
				header->spines[base].vel = Vec3(0, 0, 0);
#else
				header->spines[base].pt = header->procGeom->pSpineVtx[k].pt;
				header->spines[base].vel = Vec3(0, 0, 0);
#endif

			}
			for (size_t k = 0; header->procGeom->deform && k < header->procGeom->deform->nvertices; ++k, ++base)
			{
				header->deform_vertices[base].pos[0] = header->deform_vertices[base].pos[1] = wmat * header->procGeom->deform->initial[k];
				header->deform_vertices[base].vel = Vec3(0, 0, 0);
			}
		}
	}
	m_SpinesActive = true;
}

#pragma warning(pop)

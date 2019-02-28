// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:Cry_Math.h
//	Description: Common math class
//
//	History:
//	-Feb 27,2003: Created by Ivo Herzeg
//
//////////////////////////////////////////////////////////////////////

#ifndef CRYMATH_H
#define CRYMATH_H

#if _MSC_VER > 1000
	#pragma once
#endif

//========================================================================================

#include <CryCore/Platform/platform.h>
#include <CryCore/BitFiddling.h>
#include <cstdlib>
#include <cfloat>
#include <cmath>

#if CRY_PLATFORM_SSE2
	#include <xmmintrin.h>
#elif CRY_PLATFORM_NEON
	#include "arm_neon.h"
#endif

//! Only enable math asserts in debug builds
#if defined(_DEBUG)
	#define CRY_MATH_ASSERT(x) CRY_ASSERT_MESSAGE(x, "CRY_MATH_ASSERT")
#else
	#define CRY_MATH_ASSERT(x)
#endif

//! General template for converting to/from types. Specialize as needed
template<typename D> ILINE D             convert()      { return D(0); }
template<typename D, typename S> ILINE D convert(S val) { return D(val); }

// Definitions
constexpr f32 gf_PI = f32(3.14159265358979323846264338327950288419716939937510);
constexpr f64 g_PI = 3.14159265358979323846264338327950288419716939937510;       //!< pi

// Workaround for MSVC 140 bug where constexpr expression did not evaluate
#if !defined(_MSC_VER) || _MSC_VER >= 1910
constexpr f32 gf_PI2 = gf_PI * 2.f;
constexpr f64 g_PI2 = g_PI * 2.0; //!< 2*pi
#else
constexpr f32 gf_PI2 = f32(6.2831853071795864769252867665590057683943387987502); 
constexpr f64 g_PI2 = 6.2831853071795864769252867665590057683943387987502; //!< 2*pi
#endif

constexpr f32 gf_ln2 = 0.69314718055994530941723212145818f;       //!< ln(2)

constexpr f64 sqrt2 = 1.4142135623730950488016887242097;
constexpr f64 sqrt3 = 1.7320508075688772935274463415059;

#if !defined(SWIG)
template<typename T>
constexpr auto DEG2RAD(T&& deg) -> decltype(deg * 1.f)
{
	return deg * (gf_PI / 180.0f);
}

constexpr f64 DEG2RAD(f64 deg)
{
	return deg * (g_PI / 180.0);
}

template<typename T>
constexpr auto RAD2DEG(T&& rad) -> decltype(rad * 1.f)
{
	return rad * (180.0f / gf_PI);
}

constexpr f64 RAD2DEG(f64 rad)
{
	return rad * (180.0 / g_PI);
}
#endif


// Define min and max as proper template
#ifdef min
	#undef min
#endif

#ifdef max
	#undef max
#endif

template<typename T> ILINE T                 min(T a, T b)                { return b < a ? b : a; }
template<typename T> ILINE T                 max(T a, T b)                { return a < b ? b : a; }
template<typename T, class _Compare> ILINE T min(T a, T b, _Compare comp) { return comp(b, a) ? b : a; }
template<typename T, class _Compare> ILINE T max(T a, T b, _Compare comp) { return comp(a, b) ? b : a; }
ILINE int                                    min_branchless(int a, int b) { int diff = a - b; int mask = diff >> 31; return (b & (~mask)) | (a & mask); }

template<typename T> ILINE void              Limit(T& val, T min, T max)
{
	if (val < min)
		val = min;
	else if (val > max)
		val = max;
}
template<class T, class U> ILINE T Lerp(const T& a, const T& b, U t) { return a + T((b - a) * t); }

// Selection functions, could be overloaded or optimized as branchless

template<typename S, typename T> ILINE T if_else(S test, T a, T b) { return test ? a : b; }
template<typename S, typename T> ILINE T if_else_zero(S test, T a) { return test ? a : convert<T>(); }

// Comparison test functions, can be overloaded for SIMD types
ILINE bool All(bool b) { return b; }
ILINE bool Any(bool b) { return b; }

#if CRY_PLATFORM_SSE2

// Optimize functions for scalars
template<> ILINE f32 min(f32 v0, f32 v1)
{
	return _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(v0), _mm_set_ss(v1)));
}
template<> ILINE f32 max(f32 v0, f32 v1)
{
	return _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(v0), _mm_set_ss(v1)));
}

#endif // CRY_PLATFORM_SSE2

#if CRY_PLATFORM_SSE4

template<> ILINE int32 min(int32 v0, int32 v1)
{
	return _mm_cvtsi128_si32(_mm_min_epi32(_mm_set1_epi32(v0), _mm_set1_epi32(v1)));
}
template<> ILINE int32 max(int32 v0, int32 v1)
{
	return _mm_cvtsi128_si32(_mm_max_epi32(_mm_set1_epi32(v0), _mm_set1_epi32(v1)));
}

template<> ILINE uint32 min(uint32 v0, uint32 v1)
{
	return _mm_cvtsi128_si32(_mm_min_epu32(_mm_set1_epi32(v0), _mm_set1_epi32(v1)));
}
template<> ILINE uint32 max(uint32 v0, uint32 v1)
{
	return _mm_cvtsi128_si32(_mm_max_epu32(_mm_set1_epi32(v0), _mm_set1_epi32(v1)));
}

#endif // CRY_PLATFORM_SSE4

//! Create namespace for standard math functions, imported from std, and our extended versions.
//! SSE versions implemented in Cry_Math_SSE.h
namespace crymath
{

//
// Limiting and rounding functions
//

using std::abs;
using std::floor;
using std::ceil;
using std::trunc;

template<typename T> ILINE T clamp(T val, T lo, T hi) { return min(max(val, lo), hi); }
template<typename T> ILINE T saturate(T val)          { return clamp(val, convert<T>(0.0f), convert<T>(1.0f)); }

//
// Mathematical functions
//

using std::sin;
using std::cos;
using std::tan;

template<typename T> ILINE T asin(T op) { return std::asin(clamp(op, T(-1), T(1))); }
template<typename T> ILINE T acos(T op) { return std::acos(clamp(op, T(-1), T(1))); }

using std::atan;
using std::atan2;

template<typename T> ILINE void sincos(T angle, T* pSin, T* pCos) { *pSin = sin(angle); *pCos = cos(angle); }

using std::exp;
using std::exp;
using std::log;
using std::pow;
using std::sqrt;

//
// Define rcp, rsqrt, etc for different platforms.
//

#if CRY_PLATFORM_SSE2

ILINE f32 rcp_fast(f32 op)   { return _mm_cvtss_f32(_mm_rcp_ss(_mm_set_ss(op))); }
ILINE f32 rcp(f32 op)        { float r = rcp_fast(op); return r * (2.0f - op * r); }

ILINE f32 rsqrt_fast(f32 op) { return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(op))); }
ILINE f32 rsqrt(f32 op)      { float r = rsqrt_fast(op); return r * (1.5f - op * r * r * 0.5f); }

ILINE f32 sqrt_fast(f32 op)  { return rsqrt_fast(max(op, FLT_MIN)) * op; }

#else

ILINE f32 rcp(f32 op)      { return 1.0f / op; }
ILINE f32 rcp_fast(f32 op) { return 1.0f / op; }

	#if CRY_PLATFORM_NEON

template<int n>
float rsqrt_helper(float f)
{
	float32x2_t v = vdup_n_f32(f);
	float32x2_t r = vrsqrte_f32(v);
	// n+1 newton iterations because initial approximation is crude
	for (int i = 0; i <= n; ++i)
		r = vrsqrts_f32(v * r, r) * r;
	return vget_lane_f32(r, 0);
}

ILINE f32 sqrt_fast(f32 op)  { return op * rsqrt_helper<0>(op); }
ILINE f32 rsqrt(f32 op)      { return rsqrt_helper<1>(op); }
ILINE f32 rsqrt_fast(f32 op) { return rsqrt_helper<0>(op); }

	#else

ILINE f32 sqrt_fast(f32 op)  { return sqrt(op); }
ILINE f32 rsqrt(f32 op)      { return 1.0f / sqrt(op); }
ILINE f32 rsqrt_fast(f32 op) { return rsqrt(op); }

	#endif

#endif

ILINE f64 rcp(f64 op)        { return 1.0 / op; }
ILINE f64 rsqrt(f64 op)      { return 1.0 / sqrt(op); }

ILINE f32 rcp_safe(f32 op)   { return rcp(max(op, FLT_MIN)); }
ILINE f64 rcp_safe(f64 op)   { return rcp(max(op, DBL_MIN)); }

ILINE f32 rsqrt_safe(f32 op) { return rsqrt(max(op, FLT_MIN)); }
ILINE f64 rsqrt_safe(f64 op) { return rsqrt(max(op, DBL_MIN)); }

// signnz returns -1 if negative, 1 if zero or positive
ILINE int32 signnz(int32 op) { return ((op >> 31) << 1) + 1; }
ILINE int64 signnz(int64 op) { return ((op >> 63) << 1) + 1; }

ILINE f32   signnz(f32 op)
{
#if CRY_PLATFORM_SSE2
	__m128 v = _mm_set_ss(op);
	__m128 s = _mm_or_ps(_mm_and_ps(v, _mm_set_ss(-0.0f)), _mm_set_ss(1.0f));
	return _mm_cvtss_f32(s);
#else
	// copy sign bit from op to 1
	f32 s = 1.0f;
	reinterpret_cast<int32&>(s) |= reinterpret_cast<const int32&>(op) & (1 << 31);
	return s;
#endif
}
ILINE f64 signnz(f64 op)
{
	// copy sign bit from op to 1
	f64 s = 1.0;
	reinterpret_cast<int64&>(s) |= reinterpret_cast<const int64&>(op) & (1LL << 63);
	return s;
}

// sign returns -1 if negative, 0 if zero, 1 if positive
ILINE int32 sign(int32 op) { return (op >> 31) + ((op - 1) >> 31) + 1; }
ILINE int64 sign(int64 op) { return (op >> 63) + ((op - 1) >> 63) + 1; }

ILINE f32   sign(f32 op)
{
#if CRY_PLATFORM_SSE2
	__m128 v = _mm_set_ss(op);
	__m128 s = _mm_or_ps(_mm_and_ps(v, _mm_set_ss(-0.0f)), _mm_set_ss(1.0f));
	__m128 nz = _mm_cmpneq_ps(v, _mm_setzero_ps());
	__m128 s3 = _mm_and_ps(s, nz);
	return _mm_cvtss_f32(s3);
#else
	return if_else_zero(op, signnz(op));
#endif
}
ILINE f64 sign(f64 op) { return if_else_zero(op, signnz(op)); }

// Horizontal operating functions; can be specialized for SIMD types
template<typename T> ILINE T hsum(T v) { return v; }
template<typename T> ILINE T hmin(T v) { return v; }
template<typename T> ILINE T hmax(T v) { return v; }

// Name it so in order to avoid conflicts with cmath.h's "isfinite" function.
template<typename T> ILINE bool valueisfinite(T val)
{
#if CRY_PLATFORM_ANDROID
	return !std::isinf(val) && !std::isnan(val);   // Android NDK r16b/clang does not provide std::isfinite.
#else
	return std::isfinite(val);
#endif
}

} // namespace crymath

template<typename F> ILINE F sqr(const F& op)         { return op * op; }
template<typename F> ILINE F square(const F& op)      { return op * op; }  //!< Deprecated
template<typename F> ILINE F sqr_signed(const F& op)  { return op * crymath::abs(op); }
template<typename F> ILINE F cube(const F& op)        { return op * op * op; }

#include "Cry_ValidNumber.h"
#include "Cry_Math_SSE.h"

namespace crymath
{

// std::fmod is extremely slow
template<typename T> ILINE T mod(T a, T b)
{
	return a - trunc(a / b) * b;
}

template<typename T> ILINE T frac(T val)
{
	return val - floor(val);
}

template<typename T> ILINE T wrap(T f, T lo, T hi)
{
	const T range = hi - lo;
	return f - floor((f - lo) / range) * range;
}

template<typename T>
ILINE int solve_quadratic(T a, T b, T c, T x[2])
{
	if (!a)
	{
		if (b)
		{
			x[0] = -c / b;
			return 1;
		}
		if (!c)
		{
			x[0] = T(0);
			return 1;
		}
	}
	else
	{
		T bh = b * T(-0.5);
		T d = bh * bh - a * c;

		if (d > T(0))
		{
			T s = bh + signnz(bh) * sqrt(d);

			x[0] = c / s;
			x[1] = s / a;
			return 2;
		}
		if (!d)
		{
			x[0] = bh / a;
			return 1;
		}
	}
	return 0;
}

template<typename T>
float solve_quadratic_in_range(T a, T b, T c, T lo, T hi)
{
	T t[2];
	for (int n = solve_quadratic(a, b, c, t); --n >= 0; )
	{
		if (t[n] >= lo && t[n] <= hi)
			hi = t[n];
	}
	return hi;
}



} // namespace crymath

ILINE int32                  int_round(f32 f)             { return f < 0.f ? int32(f - 0.5f) : int32(f + 0.5f); }
ILINE int64                  int_round(f64 f)             { return f < 0.0 ? int64(f - 0.5) : int64(f + 0.5); }

ILINE uint32                 pos_round(f32 f)             { CRY_MATH_ASSERT(f >= 0.0f); return uint32(f + 0.5f); }
ILINE uint64                 pos_round(f64 f)             { CRY_MATH_ASSERT(f >= 0.0); return uint64(f + 0.5); }

ILINE int32                  pos_directed_rounding(f32 f) { return int32(f + 0.5f); }
ILINE uint64                 pos_directed_rounding(f64 f) { return uint64(f + 0.5f); }

ILINE int32                  int_ceil(f32 f)              { int32 i = int32(f); return (f > f32(i)) ? i + 1 : i; }
ILINE int64                  int_ceil(f64 f)              { int64 i = int64(f); return (f > f64(i)) ? i + 1 : i; }

ILINE float                  ufrac8_to_float(float u)     { return u * (1.f / 255.f); }
ILINE float                  ifrac8_to_float(float i)     { return i * (1.f / 127.f); }
ILINE uint8                  float_to_ufrac8(float f)     { uint i = pos_round(f * 255.f);  CRY_MATH_ASSERT(i < 256);       return uint8(i); }
ILINE int8                   float_to_ifrac8(float f)     { int i = int_round(f * 127.f);  CRY_MATH_ASSERT(abs(i) < 128);   return int8(i); }

//! Safely divides 2 numbers, with a specified maximum positive result
ILINE float div_min(float n, float d, float m) { return n * d < m * d * d ? n / d : m; }

//! Deprecated
template<typename T> ILINE T __fsel(T a, T b, T c)
{
	return if_else(a >= convert<T>(), b, c);
}

// Float extraction functions
ILINE int32 sgnnz(f64 x)
{
	union
	{
		f32   f;
		int32 i;
	} u;
	u.f = (f32)x;
	return ((u.i >> 31) << 1) + 1;
}
ILINE int32 sgnnz(f32 x)
{
	union
	{
		f32   f;
		int32 i;
	} u;
	u.f = x;
	return ((u.i >> 31) << 1) + 1;
}

ILINE int32 isneg(f32 x)
{
	union
	{
		f32    f;
		uint32 i;
	} u;
	u.f = x;
	return (int32)(u.i >> 31);
}
ILINE int32 isneg(f64 x)
{
	union
	{
		f32    f;
		uint32 i;
	} u;
	u.f = (f32)x;
	return (int32)(u.i >> 31);
}
ILINE int32 isneg(int32 x) { return (int32)((uint32)x >> 31);  }

ILINE int32 sgn(f64 x)
{
	union
	{
		f32   f;
		int32 i;
	} u;
	u.f = (f32)x;
	return (u.i >> 31) + ((u.i - 1) >> 31) + 1;
}
ILINE int32 sgn(f32 x)
{
	union
	{
		f32   f;
		int32 i;
	} u;
	u.f = x;
	return (u.i >> 31) + ((u.i - 1) >> 31) + 1;
}

ILINE int32 isnonneg(f32 x)
{
	union
	{
		f32    f;
		uint32 i;
	} u;
	u.f = x;
	return (int32)(u.i >> 31 ^ 1);
}
ILINE int32 isnonneg(f64 x)
{
	union
	{
		f32    f;
		uint32 i;
	} u;
	u.f = (f32)x;
	return (int32)(u.i >> 31 ^ 1);
}
ILINE int32 isnonneg(int32 x)          { return (int32)((uint32)x >> 31 ^ 1); }

ILINE int32 getexp(f32 x)              { return (int32)(*(uint32*)&x >> 23 & 0x0FF) - 127;  }
ILINE int32 getexp(f64 x)              { return (int32)(*((uint32*)&x + 1) >> 20 & 0x7FF) - 1023; }
ILINE f32&  setexp(f32& x, int32 iexp) { (*(uint32*)& x &= ~(0x0FF << 23)) |= (iexp + 127) << 23;   return x;  }
ILINE f64&  setexp(f64& x, int32 iexp) { (*((uint32*)&x + 1) &= ~(0x7FF << 20)) |= (iexp + 1023) << 20;  return x; }

ILINE int32 iszero(f32 x)
{
	union
	{
		f32   f;
		int32 i;
	} u;
	u.f = x;
	u.i &= 0x7FFFFFFF;
	return -(u.i >> 31 ^ (u.i - 1) >> 31);
}
ILINE int32 iszero(f64 x)
{
	union
	{
		f32   f;
		int32 i;
	} u;
	u.f = (f32)x;
	u.i &= 0x7FFFFFFF;
	return -((u.i >> 31) ^ (u.i - 1) >> 31);
}
ILINE int32 iszero(int32 x)   { return -(x >> 31 ^ (x - 1) >> 31); }
#if CRY_PLATFORM_64BIT && !defined(__clang__)
ILINE int64 iszero(__int64 x) { return -(x >> 63 ^ (x - 1) >> 63); }
#endif
#if CRY_PLATFORM_64BIT && defined(__clang__) && !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID
ILINE int64 iszero(int64_t x) { return -(x >> 63 ^ (x - 1) >> 63); }
#endif
#if CRY_PLATFORM_64BIT && (CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE)
ILINE int64 iszero(long int x) { return -(x >> 63 ^ (x - 1) >> 63); }
#endif

//! Check if x is within an open interval
template<typename F> ILINE int32 inrange(F x, F end1, F end2) { return isneg(abs(end1 + end2 - x - x) - abs(end1 - end2)); }

template<typename F> ILINE int32 idxmax3(const F* pdata)
{
	int32 imax = isneg(pdata[0] - pdata[1]);
	imax |= isneg(pdata[imax] - pdata[2]) << 1;
	return imax & (2 | (imax >> 1 ^ 1));
}
template<typename F> ILINE int32 idxmin3(const F* pdata)
{
	int32 imin = isneg(pdata[1] - pdata[0]);
	imin |= isneg(pdata[2] - pdata[imin]) << 1;
	return imin & (2 | (imin >> 1 ^ 1));
}
//! Approximation of exp(-x).
ILINE float approxExp(float x) { return crymath::rcp_fast(1.f + x); }

//! Approximation of 1.f - exp(-x).
ILINE float approxOneExp(float x) { return x * crymath::rcp_fast(1.f + x); }

//! \return i if x==1<<i (i=0..63)
ILINE int ilog2(uint64 x)
{
	return (int)IntegerLog2(x);
}

const int32 inc_mod3[] = { 1, 2, 0 }, dec_mod3[] = { 2, 0, 1 };
#ifdef PHYSICS_EXPORTS
ILINE int32 incm3(int32 i) { return inc_mod3[i]; }
ILINE int32 decm3(int32 i) { return dec_mod3[i]; }
#else
ILINE int32 incm3(int32 i) { return i + 1 & (i - 2) >> 31; }
ILINE int32 decm3(int32 i) { return i - 1 + ((i - 1) >> 31 & 3); }
#endif

//////////////////////////////////////////////////////////////////////////

//! This function relaxes a value (val) towards a desired value (to) whilst maintaining continuity
//! of val and its rate of change (valRate). timeDelta is the time between this call and the previous one.
//! The caller would normally keep val and valRate as working variables, and smoothTime is normally
//! a fixed parameter. The to/timeDelta values can change.
//!
//! Implementation details:
//!
//! This is a critically damped spring system. A linear spring is attached between "val" and "to" that
//! drags "val" to "to". At the same time a damper between the two removes oscillations; it's tweaked
//! so it doesn't dampen more than necessary. In combination this gives smooth ease-in and ease-out behavior.
//!
//! smoothTime can be interpreted in a couple of ways:
//! - it's the "expected time to reach the target when at maximum velocity" (the target will, however, not be reached
//!   in that time because the speed will decrease the closer it gets to the target)
//! - it's the 'lag time', how many seconds "val" lags behind "to". If your
//!   target has a certain speed, the lag distance is simply the smoothTime times that speed.
//! - it's 2/omega, where omega is the spring's natural frequency (or less formally a measure of the spring stiffness)
//!
//! The implementation is stable for varying timeDelta, but for performance reasons it uses a polynomial approximation
//! to the exponential function. The approximation works well (within 0.1% of accuracy) when smoothTime > 2*deltaTime,
//! which is usually the case. (but it might be troublesome when you want a stiff spring or have frame hikes!)
//! The implementation handles cases where smoothTime==0 separately and reliably. In that case the target will be
//! reached immediately, and valRate is updated appropriately.
//!
//! Based on "Critically Damped Ease-In/Ease-Out Smoothing", Thomas Lowe, Game Programming Gems IV

// disable divide by zero warning for the divides by smoothTime
// & timeDelta (warning appears even though these cases are guarded)
#pragma warning(push)
#pragma warning(disable:4723)

//! \param[in,out] &val     value to be smoothed
//! \param[in,out]&valRate  rate of change of the value
//! \param[in] timeDelta    time interval
//! \param[in] to           the target value
//! \param[in] smoothTime   timescale for smoothing
template<typename T> ILINE void SmoothCD(
  T& val,
  T& valRate,
  const float timeDelta,
  const T& to,
  const float smoothTime)
{
	if (smoothTime > 0.0f)
	{
		const float omega = 2.0f / smoothTime;
		const float x = omega * timeDelta;
		const float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
		const T change = (val - to);
		const T temp = (T)((valRate + change * omega) * timeDelta);
		valRate = (T)((valRate - temp * omega) * exp);
		val = (T)(to + (change + temp) * exp);
	}
	else if (timeDelta > 0.0f)
	{
		valRate = (T)((to - val) / timeDelta);
		val = to;
	}
	else
	{
		val = to;
		valRate -= valRate; // zero it...
	}
}

//! \param[in,out]	val,       //!< value to be smoothed
//! \param[in,out]	valRate,   //!< rate of change of the value
//! \param[in]	timeDelta,     //!< time interval
//! \param[in]	to,            //!< the target value
//! \param[in]	smoothTime,    //!< timescale for smoothing
//! \param[in]	maxValRate)    //!< maximum allowed rate of change
template<typename T> ILINE void SmoothCDWithMaxRate(
  T& val,
  T& valRate,
  const float timeDelta,
  const T& to,
  const float smoothTime,
  const T& maxValRate)
{
	if (smoothTime > 0.0f)
	{
		const float omega = 2.0f / smoothTime;
		const float x = omega * timeDelta;
		const float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
		const T unclampedChange = val - to;
		const T maxChange = maxValRate * smoothTime;
		const T clampedChange = crymath::clamp<T>(unclampedChange, -maxChange, maxChange);
		const T clampedTo = val - clampedChange;
		const T temp = (T)((valRate + clampedChange * omega) * timeDelta);
		valRate = (T)((valRate - temp * omega) * exp);
		val = (T)(clampedTo + (clampedChange + temp) * exp);
	}
	else if (timeDelta > 0.0f)
	{
		const T unclampedRate = (T)((to - val) / timeDelta);
		valRate = crymath::clamp<T>(unclampedRate, -maxValRate, maxValRate);
		val += valRate * timeDelta;
	}
	else
	{
		val = to;
		valRate -= valRate; // zero it...
	}
}
#pragma warning(pop)

//! Smoothes linear blending into cubic (b-spline) with 0-derivatives near 0 and 1.
inline f32 SmoothBlendValue(const f32 fBlend)
{
	const f32 fBlendAdj = fBlend - 0.5f;
	return __fsel(-fBlend, 0.f, __fsel(fBlend - 1.f, 1.f, 0.5f - 2.f * (fBlendAdj * fBlendAdj * fBlendAdj) + 1.5f * fBlendAdj));
}

// Legacy math function names
#define clamp_tpl      crymath::clamp

#define fabs_tpl       crymath::abs
#define floor_tpl      crymath::floor
#define ceil_tpl       crymath::ceil

#define sin_tpl        crymath::sin
#define cos_tpl        crymath::cos
#define tan_tpl        crymath::tan
#define asin_tpl       crymath::asin
#define acos_tpl       crymath::acos
#define atan_tpl       crymath::atan
#define atan2_tpl      crymath::atan2
#define sincos_tpl     crymath::sincos

#define exp_tpl        crymath::exp
#define log_tpl        crymath::log
#define pow_tpl        crymath::pow
#define fmod_tpl       crymath::mod

#define sqrt_tpl       crymath::sqrt
#define sqrt_fast_tpl  crymath::sqrt_fast

#define isqrt_tpl      crymath::rsqrt
#define isqrt_fast_tpl crymath::rsqrt_fast
#define isqrt_safe_tpl crymath::rsqrt_safe

#define __fres         crymath::rcp

ILINE int32 sgnnz(int32 i) { return crymath::signnz(i); }
ILINE int32 sgn(int32 i)   { return crymath::sign(i); }

ILINE int64 sgnnz(int64 i) { return crymath::signnz(i); }
ILINE int64 sgn(int64 i)   { return crymath::sign(i); }

ILINE f32   fsgnnz(f32 f)  { return crymath::signnz(f); }
ILINE f32   fsgnf(f32 f)   { return crymath::sign(f); }

// Previously in Cry_XOptimise.h
#define FtoI               int
#define fastftol_positive  int
#define fastround_positive pos_round

//////////////////////////////////////////////////////////////////////////
enum type_zero { ZERO };
enum type_min { VMIN };
enum type_max { VMAX };
enum type_identity { IDENTITY };

#include "NumberVector.h"
#include "Cry_Vector2.h"
#include "Cry_Vector3.h"
#include "Cry_Vector4.h"
#include "Cry_MatrixDiag.h"
#include "Cry_Matrix33.h"
#include "Cry_Matrix34.h"
#include "Cry_Matrix44.h"
#include "Cry_Quat.h"
#include "CryHalf.inl"

#endif //math

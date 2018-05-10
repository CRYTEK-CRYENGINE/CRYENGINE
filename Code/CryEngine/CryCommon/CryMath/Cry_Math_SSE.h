// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// SSE operators to shift vector by scalar for all compilers.
// Integer type alias to allow operator overloads. Works for both integer constants and variables.
// Example:
//		i32v4 a;
//		i32v4 b = a << Scalar(4);
//		int x; i32v4 c = a >> Scalar(x);
enum Scalar {};

template<typename T> struct SIMD_traits
{
	using scalar_t  = T;
	using vector4_t = T;
};

template<typename F> using vector4_t = typename SIMD_traits<F>::vector4_t;
template<typename V> using scalar_t  = typename SIMD_traits<V>::scalar_t;

#if CRY_PLATFORM_SSE2

	#define CRY_TYPE_SIMD4

// Support for SSE 32x4 types

	#if CRY_COMPILER_CLANG || CRY_COMPILER_GCC

// __m128i is defined as __v2di instead of __v4si, so we define the correct vector types for correct arithmetic operations.
typedef f32    f32v4 __attribute__((__vector_size__(16)));
typedef int32  i32v4 __attribute__((__vector_size__(16)));
typedef uint32 u32v4 __attribute__((__vector_size__(16)));

//! Define results of built-in comparison operators.
typedef decltype (f32v4() == f32v4()) mask32v4;

	#elif CRY_COMPILER_MSVC

// Declare a new intrinsic type for unsigned ints
union __declspec(intrin_type) __declspec(align(16)) __m128u
{
	__m128i m128i;
	uint32 m128i_u32[4];

	ILINE __m128u() {}
	ILINE __m128u(__m128i in) : m128i(in) {}
	ILINE operator __m128i() const { return m128i; }
};

typedef __m128  f32v4;
typedef __m128i i32v4;
typedef __m128u u32v4;

//! Define results of comparison operators
typedef __m128i mask32v4;

	#else
		#error Unsupported compiler for SSE
	#endif

///////////////////////////////////////////////////////////////////////////
// Association between scalar and vector types

template<> struct SIMD_traits<f32>
{
	using scalar_t  = f32;
	using vector4_t = f32v4;
};
template<> struct SIMD_traits<f32v4>
{
	using scalar_t = f32;
};

template<> struct SIMD_traits<int32>
{
	using scalar_t  = int32;
	using vector4_t = i32v4;
};
template<> struct SIMD_traits<i32v4>
{
	using scalar_t = int32;
};

template<> struct SIMD_traits<uint32>
{
	using scalar_t  = uint32;
	using vector4_t = u32v4;
};
template<> struct SIMD_traits<u32v4>
{
	using scalar_t = uint32;
};

// Template conversions
template<typename S> ILINE vector4_t<S> to_vector4(S s) { return convert<vector4_t<S>>(s); }
template<typename V> ILINE scalar_t<V>  to_scalar(V v)  { return convert<scalar_t<V>>(v); }

///////////////////////////////////////////////////////////////////////////

template<class T, class V> ILINE void check_range(V v, T lo, T hi);

//! Override convert<> for vector intrinsic types
template<> ILINE f32v4  convert<f32v4>()                { return _mm_setzero_ps(); }
template<> ILINE f32v4  convert<f32v4>(f32 v)           { return _mm_set1_ps(v); }
template<> ILINE f32v4  convert<f32v4>(double v)        { return _mm_set1_ps(f32(v)); }
template<> ILINE f32v4  convert<f32v4>(int v)           { return _mm_set1_ps(f32(v)); }
template<> ILINE f32v4  convert<f32v4>(i32v4 v)         { return _mm_cvtepi32_ps(v); }
template<> ILINE f32v4  convert<f32v4>(u32v4 v)         { check_range(v, 0, INT_MAX); return _mm_cvtepi32_ps(v); }
template<> ILINE f32v4  convert<f32v4>(const f32* p)    { return _mm_loadu_ps(p); }

template<> ILINE f32    convert<f32>(f32v4 v)           { return _mm_cvtss_f32(v); }

template<> ILINE i32v4  convert<i32v4>()                { return _mm_setzero_si128(); }
template<> ILINE i32v4  convert<i32v4>(int v)           { return _mm_set1_epi32(v); }
template<> ILINE i32v4  convert<i32v4>(f32v4 v)         { check_range(v, INT_MIN, INT_MAX); return _mm_cvttps_epi32(v); }
template<> ILINE i32v4  convert<i32v4>(const int32* p)  { return _mm_loadu_si128((const __m128i*)p); }

template<> ILINE int32  convert<int32>(i32v4 v)         { return _mm_cvtsi128_si32(v); }

template<> ILINE u32v4  convert<u32v4>()                { return _mm_setzero_si128(); }
template<> ILINE u32v4  convert<u32v4>(uint v)          { return _mm_set1_epi32(v); }
template<> ILINE u32v4  convert<u32v4>(int v)           { CRY_MATH_ASSERT(v >= 0); return _mm_set1_epi32(v); }
template<> ILINE u32v4  convert<u32v4>(f32v4 v)         { check_range(v, 0, INT_MAX); return _mm_cvttps_epi32(v); }
template<> ILINE i32v4  convert<i32v4>(const uint32* p) { return _mm_loadu_si128((const __m128i*)p); }

template<> ILINE uint32 convert<uint32>(u32v4 v)        { return _mm_cvtsi128_si32(v); }

//! convert<V>(s, 0): Load scalar into vector[0] and zero remaining elements
template<typename V, typename S> V convert(S v, void*);

template<> ILINE f32v4             convert<f32v4>(f32 v, void*)  { return _mm_set_ss(v); }
template<> ILINE i32v4             convert<i32v4>(int v, void*)  { return _mm_cvtsi32_si128(v); }
template<> ILINE u32v4             convert<u32v4>(uint v, void*) { return _mm_cvtsi32_si128(v); }

//! Multiple scalar/vector conversion
template<typename V, typename S> V convert(S s0, S s1, S s2, S s3);

template<> ILINE f32v4             convert<f32v4>(f32 s0, f32 s1, f32 s2, f32 s3)     { return _mm_setr_ps(s0, s1, s2, s3); }
template<> ILINE f32v4             convert<f32v4>(int s0, int s1, int s2, int s3)     { return _mm_setr_ps(f32(s0), f32(s1), f32(s2), f32(s3)); }
template<> ILINE i32v4             convert<i32v4>(int s0, int s1, int s2, int s3)     { return _mm_setr_epi32(s0, s1, s2, s3); }
template<> ILINE u32v4             convert<u32v4>(uint s0, uint s1, uint s2, uint s3) { return _mm_setr_epi32(s0, s1, s2, s3); }
template<> ILINE u32v4             convert<u32v4>(int s0, int s1, int s2, int s3)     { return _mm_setr_epi32(s0, s1, s2, s3); }

template<typename V> ILINE V       one_mask(V any)                                    { return _mm_cmpeq_epi32(any, any); }

// Template conversions
template<typename S> ILINE vector4_t<S> to_v4(S s) { return convert<vector4_t<S>>(s); }
template<typename V> ILINE scalar_t<V>  to_s(V v)  { return convert<scalar_t<V>>(v); }

// Shuffle components in a single vector
template<int x, int y, int z, int w> ILINE f32v4 shuffle(f32v4 in) { return _mm_shuffle_ps(in, in, _MM_SHUFFLE(w, z, y, x)); }
template<int x, int y, int z, int w> ILINE i32v4 shuffle(i32v4 in) { return _mm_shuffle_epi32(in, _MM_SHUFFLE(w, z, y, x)); }
template<int x, int y, int z, int w> ILINE u32v4 shuffle(u32v4 in) { return _mm_shuffle_epi32(in, _MM_SHUFFLE(w, z, y, x)); }

// Specializations (faster or shorter instructions)
template<> ILINE f32v4 shuffle<0, 0, 1, 1>(f32v4 in) { return _mm_unpacklo_ps(in, in); }
template<> ILINE f32v4 shuffle<2, 2, 3, 3>(f32v4 in) { return _mm_unpackhi_ps(in, in); }

// Duplicate a single element to entire vector
template<int i, typename V> ILINE V broadcast(V in) { return shuffle<i, i, i, i>(in); }

// Mix components from 2 vectors
template<int x, int y, int z, int w> ILINE f32v4 mix0011(f32v4 a, f32v4 b) { return _mm_shuffle_ps(a, b, _MM_SHUFFLE(w, z, y, x)); }
template<int x, int y, int z, int w> f32v4 mix0101(f32v4 a, f32v4 b);  //!< Default configurations unimplemented

// Specializations (faster or shorter instructions)
template<> ILINE f32v4 mix0011<0, 1, 0, 1>(f32v4 a, f32v4 b) { return _mm_movelh_ps(a, b); }
template<> ILINE f32v4 mix0011<2, 3, 2, 3>(f32v4 a, f32v4 b) { return _mm_movehl_ps(b, a); }
template<> ILINE f32v4 mix0101<0, 0, 1, 1>(f32v4 a, f32v4 b) { return _mm_unpacklo_ps(a, b); }
template<> ILINE f32v4 mix0101<2, 2, 3, 3>(f32v4 a, f32v4 b) { return _mm_unpackhi_ps(a, b); }

// Transpose a 4x4 matrix
ILINE void transpose(f32v4 out[4], const f32v4 in[4])
{
	f32v4 tmp0 = mix0101<0, 0, 1, 1>(in[0], in[1]);
	f32v4 tmp1 = mix0101<2, 2, 3, 3>(in[0], in[1]);
	f32v4 tmp2 = mix0101<0, 0, 1, 1>(in[2], in[3]);
	f32v4 tmp3 = mix0101<2, 2, 3, 3>(in[2], in[3]);
	out[0] = mix0011<0, 1, 0, 1>(tmp0, tmp2);
	out[1] = mix0011<2, 3, 2, 3>(tmp0, tmp2);
	out[2] = mix0011<0, 1, 0, 1>(tmp1, tmp3);
	out[3] = mix0011<2, 3, 2, 3>(tmp1, tmp3);
}

// Extract scalar component
template<int i = 0> ILINE f32 get_element(f32v4 v)
{
	if (i == 0)
		return _mm_cvtss_f32(v);
	else
	{
	#if CRY_PLATFORM_SSE4
		f32 r;
		_MM_EXTRACT_FLOAT(r, v, i);
		return r;
	#else
		return _mm_cvtss_f32(shuffle<i, i, i, i>(v));
	#endif
	}
}

template<int i = 0> ILINE int32 get_element(i32v4 v)
{
	if (i == 0)
		return _mm_cvtsi128_si32(v);
	else
	{
	#if CRY_PLATFORM_SSE4
		return _mm_extract_epi32(v, i);
	#else
		return _mm_cvtsi128_si32(shuffle<i, i, i, i>(v));
	#endif
	}
}

template<int i = 0> ILINE uint32 get_element(u32v4 v)
{
	return get_element<i>((i32v4)v);
}

// Passive casting
template<typename D, typename S> ILINE D vcast(S val)          { return D(val); }
template<> ILINE f32v4                   vcast<f32v4>(i32v4 v) { return _mm_castsi128_ps(v); }
template<> ILINE f32v4                   vcast<f32v4>(u32v4 v) { return _mm_castsi128_ps(v); }
template<> ILINE i32v4                   vcast<i32v4>(f32v4 v) { return _mm_castps_si128(v); }
template<> ILINE u32v4                   vcast<u32v4>(f32v4 v) { return _mm_castps_si128(v); }

///////////////////////////////////////////////////////////////////////////
// Helpers vor defining regular and compound operators together
	#define COMMPOUND_OPERATOR(A, op, B) \
	  ILINE A & operator op ## = (A & a, B b) { return a = a op b; }

	#define COMMPOUND_OPERATORS(A, op, B) \
	  A operator op(A a, B b);            \
	  COMMPOUND_OPERATOR(A, op, B)        \
	  ILINE A operator op(A a, B b)       \


///////////////////////////////////////////////////////////////////////////
	#if CRY_COMPILER_MSVC

// Define operators for intrinsics, for compilers which don't already have them

///////////////////////////////////////////////////////////////////////////
// Integer, signed and unsigned

		#define INTV4_COMPARE_OPS(V, C, op, opneg, func)                                      \
		  ILINE C operator op(V a, V b) { return func(a, b); }                                \
		  ILINE C operator opneg(V a, V b) { return _mm_xor_si128(func(a, b), one_mask(a)); } \

INTV4_COMPARE_OPS(i32v4, mask32v4, ==, !=, _mm_cmpeq_epi32)
INTV4_COMPARE_OPS(i32v4, mask32v4, >, <=, _mm_cmpgt_epi32)
INTV4_COMPARE_OPS(i32v4, mask32v4, <, >=, _mm_cmplt_epi32)

INTV4_COMPARE_OPS(u32v4, mask32v4, ==, !=, _mm_cmpeq_epi32)

// Synthesize unsigned inequality comparisons
ILINE mask32v4 operator<(u32v4 a, u32v4 b)
{
	mask32v4 mask = _mm_cmplt_epi32(a, b);                     // Result valid if both pos or both neg
	u32v4 sign_diff = _mm_srai_epi32(_mm_xor_si128(a, b), 31); // All 1s if signs differ
	return _mm_xor_si128(mask, sign_diff);                     // Result negated if signs differ
}
ILINE mask32v4 operator>(u32v4 a, u32v4 b)
{
	mask32v4 mask = _mm_cmpgt_epi32(a, b);
	u32v4 sign_diff = _mm_srai_epi32(_mm_xor_si128(a, b), 31);
	return _mm_xor_si128(mask, sign_diff);
}
ILINE mask32v4 operator<=(u32v4 a, u32v4 b)
{
	return _mm_xor_si128(a > b, one_mask(a));
}
ILINE mask32v4 operator>=(u32v4 a, u32v4 b)
{
	return _mm_xor_si128(a < b, one_mask(a));
}

		#define INTV4_BINARY_OPS(V, op, func) \
		  COMMPOUND_OPERATORS(V, op, V) { return func(a, b); }

		#define SUINTV4_BINARY_OPS(op, func) \
		  INTV4_BINARY_OPS(i32v4, op, func)  \
		  INTV4_BINARY_OPS(u32v4, op, func)  \


SUINTV4_BINARY_OPS(&, _mm_and_si128)
SUINTV4_BINARY_OPS(|, _mm_or_si128)
SUINTV4_BINARY_OPS(^, _mm_xor_si128)

SUINTV4_BINARY_OPS(+, _mm_add_epi32)
SUINTV4_BINARY_OPS(-, _mm_sub_epi32)

//! Signed mul, low 32 bits only
COMMPOUND_OPERATORS(i32v4, *, i32v4)
{
		#if CRY_PLATFORM_SSE4
	return _mm_mullo_epi32(a, b);
		#else
	const i32v4 m02 = _mm_mul_epu32(a, b);
	const i32v4 m13 = _mm_mul_epu32(shuffle<1, 1, 3, 3>(a), shuffle<1, 1, 3, 3>(b));
	return _mm_unpacklo_epi32(shuffle<0, 2, 0, 0>(m02), shuffle<0, 2, 0, 0>(m13));
		#endif
}

//< Unsigned mul is identical, as only low 32 bits are returned
COMMPOUND_OPERATORS(u32v4, *, u32v4)
{
	return (u32v4)((i32v4)a * (i32v4)b);
}

		#define INTV4_SLOW_BINARY_OPS(V, op)             \
		  COMMPOUND_OPERATORS(V, op, V) {                \
		    return convert<V>(                           \
		      get_element<0>(a) op get_element<0>(b),    \
		      get_element<1>(a) op get_element<1>(b),    \
		      get_element<2>(a) op get_element<2>(b),    \
		      get_element<3>(a) op get_element<3>(b)); } \

INTV4_SLOW_BINARY_OPS(i32v4, /)
INTV4_SLOW_BINARY_OPS(u32v4, /)

INTV4_SLOW_BINARY_OPS(i32v4, %)
INTV4_SLOW_BINARY_OPS(u32v4, %)

ILINE i32v4 operator-(i32v4 a) { return convert<i32v4>() - a; }

ILINE u32v4 operator~(u32v4 a) { return a ^ one_mask(a); }
ILINE i32v4 operator~(i32v4 a) { return a ^ one_mask(a); }

///////////////////////////////////////////////////////////////////////////
// Float

		#define FV4_COMPARE_OP(op, func)                                                       \
		  ILINE mask32v4 operator op(f32v4 a, f32v4 b) { return vcast<mask32v4>(func(a, b)); } \

FV4_COMPARE_OP(==, _mm_cmpeq_ps)
FV4_COMPARE_OP(!=, _mm_cmpneq_ps)
FV4_COMPARE_OP(<, _mm_cmplt_ps)
FV4_COMPARE_OP(<=, _mm_cmple_ps)
FV4_COMPARE_OP(>, _mm_cmpgt_ps)
FV4_COMPARE_OP(>=, _mm_cmpge_ps)

		#define FV4_BINARY_OPS(op, func) \
		  COMMPOUND_OPERATORS(f32v4, op, f32v4) { return func(a, b); }

FV4_BINARY_OPS(+, _mm_add_ps)
FV4_BINARY_OPS(-, _mm_sub_ps)
FV4_BINARY_OPS(*, _mm_mul_ps)
FV4_BINARY_OPS(/, _mm_div_ps)

ILINE f32v4 operator-(f32v4 a) { return convert<f32v4>() - a; }

	#endif // CRY_COMPILER_MSVC

///////////////////////////////////////////////////////////////////////////
// SSE shift-immediate operators for all compilers
COMMPOUND_OPERATORS(i32v4, <<, Scalar)
{
	return _mm_slli_epi32(a, b);
}
COMMPOUND_OPERATORS(u32v4, <<, Scalar)
{
	return _mm_slli_epi32(a, b);
}
COMMPOUND_OPERATORS(i32v4, >>, Scalar)
{
	return _mm_srai_epi32(a, b);
}
COMMPOUND_OPERATORS(u32v4, >>, Scalar)
{
	return _mm_srli_epi32(a, b);
}

///////////////////////////////////////////////////////////////////////////
// Additional operators and functions for SSE intrinsics

//! Convert comparison results to boolean value
ILINE bool All(mask32v4 v)
{
	return _mm_movemask_ps(vcast<f32v4>(v)) == 0xF;
}
ILINE bool Any(mask32v4 v)
{
	return _mm_movemask_ps(vcast<f32v4>(v)) != 0;
}

template<typename T> ILINE T if_else(mask32v4 mask, T a, T b)
{
	#if CRY_PLATFORM_SSE4
	return vcast<T>(_mm_blendv_ps(vcast<f32v4>(b), vcast<f32v4>(a), vcast<f32v4>(mask)));
	#else
	return vcast<T>(_mm_or_si128(_mm_and_si128(mask, vcast<i32v4>(a)), _mm_andnot_si128(mask, vcast<i32v4>(b))));
	#endif
}
template<typename T> ILINE T if_else_zero(mask32v4 mask, T a)
{
	return vcast<T>(_mm_and_si128(mask, vcast<i32v4>(a)));
}

template<class T, class V> ILINE void check_range(V v, T lo, T hi)
{
	CRY_MATH_ASSERT(All(v >= convert<V>(lo)));
	CRY_MATH_ASSERT(All(v <= convert<V>(hi)));
}

///////////////////////////////////////////////////////////////////////////
// Specialize global functions
template<> ILINE f32v4 min(f32v4 v0, f32v4 v1)
{
	return _mm_min_ps(v0, v1);
}
template<> ILINE f32v4 max(f32v4 v0, f32v4 v1)
{
	return _mm_max_ps(v0, v1);
}

template<> ILINE i32v4 min(i32v4 v0, i32v4 v1)
{
	#if CRY_PLATFORM_SSE4
	return _mm_min_epi32(v0, v1);
	#else
	return if_else(v0 < v1, v0, v1);
	#endif
}
template<> ILINE i32v4 max(i32v4 v0, i32v4 v1)
{
	#if CRY_PLATFORM_SSE4
	return _mm_max_epi32(v0, v1);
	#else
	return if_else(v1 < v0, v0, v1);
	#endif
}

template<> ILINE u32v4 min(u32v4 v0, u32v4 v1)
{
	#if CRY_PLATFORM_SSE4
	return _mm_min_epu32(v0, v1);
	#else
	return if_else(v0 < v1, v0, v1);
	#endif
}
template<> ILINE u32v4 max(u32v4 v0, u32v4 v1)
{
	#if CRY_PLATFORM_SSE4
	return _mm_max_epu32(v0, v1);
	#else
	return if_else(v1 < v0, v0, v1);
	#endif
}

namespace crymath
{

ILINE f32v4 abs(f32v4 v)
{
	return _mm_andnot_ps(to_v4(-0.0f), v);
}
ILINE i32v4 abs(i32v4 v)
{
	#if CRY_PLATFORM_SSE4
	return _mm_abs_epi32(v);
	#else
	return if_else(v < convert<i32v4>(), -v, v);
	#endif
}
ILINE u32v4 abs(u32v4 v)
{
	return v;
}

ILINE f32v4 signnz(f32v4 v)
{
	f32v4 sign_bits = _mm_and_ps(v, to_v4(-0.0f));
	return _mm_or_ps(sign_bits, to_v4(1.0f));
}
ILINE i32v4 signnz(i32v4 v)
{
	return (v >> Scalar(31) << Scalar(1)) + to_v4(1);
}

ILINE f32v4 sign(f32v4 v)
{
	f32v4 s2 = signnz(v);
	return _mm_and_ps(s2, vcast<f32v4>(v != convert<f32v4>()));
}
ILINE i32v4 sign(i32v4 v)
{
	#if CRY_PLATFORM_SSE4
	return _mm_sign_epi32(to_v4(1), v);
	#else
	i32v4 zero = convert<i32v4>();
	return _mm_cmplt_epi32(v, zero) - _mm_cmpgt_epi32(v, zero);
	#endif
}

// Note: for SSE2, trunc, floor, and ceil currently only work for float values in the int32 range
ILINE f32v4 trunc(f32v4 v)
{
	#if CRY_PLATFORM_SSE4
	return _mm_round_ps(v, _MM_FROUND_TO_ZERO);
	#else
	return convert<f32v4>(convert<i32v4>(v));
	#endif
}

ILINE f32v4 floor(f32v4 v)
{
	#if CRY_PLATFORM_SSE4
	return _mm_floor_ps(v);
	#else
	f32v4 trunct = trunc(v);
	return trunct + if_else_zero(trunct > v, to_v4(-1.0f));
	#endif
}

ILINE f32v4 ceil(f32v4 v)
{
	#if CRY_PLATFORM_SSE4
	return _mm_ceil_ps(v);
	#else
	f32v4 trunct = trunc(v);
	return trunct + if_else_zero(trunct < v, to_v4(1.0f));
	#endif
}

ILINE f32v4 rcp_fast(f32v4 op)   { return _mm_rcp_ps(op); }

ILINE f32v4 rcp(f32v4 op)        { f32v4 r = _mm_rcp_ps(op); return r + r - op * r * r; }

ILINE f32v4 rcp_safe(f32v4 op)   { return rcp(_mm_max_ps(op, to_v4(FLT_MIN))); }

ILINE f32v4 rsqrt_fast(f32v4 op) { return _mm_rsqrt_ps(op); }

ILINE f32v4 rsqrt(f32v4 op)      { f32v4 r = _mm_rsqrt_ps(op); return r * (to_v4(1.5f) - op * r * r * to_v4(0.5f)); }

ILINE f32v4 rsqrt_safe(f32v4 op) { return rsqrt(_mm_max_ps(op, to_v4(FLT_MIN))); }

ILINE f32v4 sqrt_fast(f32v4 op)  { return _mm_rsqrt_ps(_mm_max_ps(op, to_v4(FLT_MIN))) * op; }

ILINE f32v4 sqrt(f32v4 op)       { return _mm_sqrt_ps(op); }

// Horizontally operating functions
// Note: the SSE3 hadd instrinsics are NOT faster than shuffle+add

	#define HORIZONTAL_FUNC(V, func, op)                        \
	  ILINE V func ## v(V v)                                    \
	  {                                                         \
	    V h = op(v, shuffle<2, 3, 0, 1>(v));                    \
	    return op(h, shuffle<1, 0, 1, 0>(h));                   \
	  }                                                         \
	  ILINE scalar_t<V> func(V v){ return to_s(func ## v(v)); } \

HORIZONTAL_FUNC(f32v4, hmin, min)
HORIZONTAL_FUNC(i32v4, hmin, min)
HORIZONTAL_FUNC(u32v4, hmin, min)

HORIZONTAL_FUNC(f32v4, hmax, max)
HORIZONTAL_FUNC(i32v4, hmax, max)
HORIZONTAL_FUNC(u32v4, hmax, max)

HORIZONTAL_FUNC(f32v4, hsum, _mm_add_ps)
HORIZONTAL_FUNC(i32v4, hsum, _mm_add_epi32)
HORIZONTAL_FUNC(u32v4, hsum, _mm_add_epi32)

} // namespace crymath

///////////////////////////////////////////////////////////////////////////
// Overrides for IsValid and IsEquivalent
template<> ILINE bool IsValid(const f32v4& val)
{
	const u32v4 expMask = convert<u32v4>((0xFF << 23));
	return All((vcast<u32v4>(val) & expMask) != expMask);
}
template<> ILINE bool IsValid(const i32v4& v) { return true; }
template<> ILINE bool IsValid(const u32v4& v) { return true; }

ILINE void SetInvalid(f32v4& val)
{
	val = convert<f32v4>(std::numeric_limits<f32>::signaling_NaN());
}

template<> ILINE bool IsEquivalent(const f32v4& a, const f32v4& b, f32 e)
{
	f32v4 ev = convert<f32v4>(e * e);
	if (e < 0) // Perform a relative comparison
		ev *= max(a * a, b * b);
	f32v4 diff = a - b;
	return All(diff * diff <= ev);
}


#endif // CRY_PLATFORM_SSE2

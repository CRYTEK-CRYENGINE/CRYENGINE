// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Implementation of Vector4 using SIMD types

#pragma once

#include "Cry_Math_SSE.h"

#ifdef CRY_TYPE_SIMD4

	#define CRY_HARDWARE_VECTOR4

	#include <type_traits>
	#include "Cry_Vector4.h"

///////////////////////////////////////////////////////////////////////////////
// 4D vector using SIMD type
template<typename F> struct Vec4H
{
	using value_type = F;
	using V = vector4_t<F>;

	enum { component_count = 4 };

	union
	{
		V v;
		struct
		{
			F x, y, z, w;
		};
	};

	ILINE Vec4H()
	{
#ifdef _DEBUG
		SetInvalid(v);
#endif
	}

	ILINE Vec4H(type_zero)                          { v = convert<V>(); }
	ILINE Vec4H& zero()                             { v = convert<V>(); return *this; }
	explicit ILINE Vec4H(F s)                       { v = convert<V>(s); CRY_MATH_ASSERT(IsValid()); }
	ILINE Vec4H(F x_, F y_, F z_, F w_)             { v = convert<V>(x_, y_, z_, w_); CRY_MATH_ASSERT(IsValid()); }
	ILINE Vec4H& operator()(F x_, F y_, F z_, F w_) { v = convert<V>(x_, y_, z_, w_); CRY_MATH_ASSERT(IsValid()); return *this; }

	// Conversions
	ILINE Vec4H(V v_)        { v = v_; }
	ILINE operator V() const { return v; }

	// Operators
	ILINE F&   operator[](int index)            { CRY_MATH_ASSERT(index >= 0 && index <= 3); return ((F*)this)[index]; }
	ILINE F    operator[](int index) const      { CRY_MATH_ASSERT(index >= 0 && index <= 3); return ((F*)this)[index]; }

	ILINE bool operator==(const Vec4H& b) const { return All(v == b.v);  }
	ILINE bool operator!=(const Vec4H& b) const { return !All(v == b.v); }

	COMPOUND_MEMBER_OP(Vec4H, +, Vec4H)         { return v + b.v; }
	COMPOUND_MEMBER_OP(Vec4H, -, Vec4H)         { return v - b.v; }
	COMPOUND_MEMBER_OP(Vec4H, *, Vec4H)         { return v * b.v; }  //!< Note component-wise multiplication, not dot-product
	COMPOUND_MEMBER_OP(Vec4H, /, Vec4H)         { return v / b.v; }

	COMPOUND_MEMBER_OP(Vec4H, *, V)             { return v * b; }
	COMPOUND_MEMBER_OP(Vec4H, /, V)             { return v / b; }

	COMPOUND_MEMBER_OP(Vec4H, *, F)             { return v * convert<V>(b);  }
	COMPOUND_MEMBER_OP(Vec4H, /, F)             { return v / convert<V>(b);  }

	ILINE Vec4H operator-() const               { return -v; }

	ILINE F      operator|(Vec4H b) const       { return crymath::hsum(v * b.v); }

	// Properties
	ILINE bool IsEquivalent(const Vec4H& v1, F epsilon = (F)VEC_EPSILON) const
	{
		return ::IsEquivalent(v, v1.v, epsilon);
	}

	ILINE bool IsValid() const             { return NumberValid(v); }
	ILINE F    GetLengthSquared() const    { return crymath::hsum(v * v); };
	ILINE F    GetLength() const           { return crymath::sqrt(GetLengthSquared()); }
	ILINE F    GetLengthFast() const       { return crymath::sqrt_fast(GetLengthSquared()); }
	ILINE F    GetInvLength() const        { return crymath::rsqrt(GetLengthSquared()); }
	ILINE F    GetInvLengthFast() const    { return crymath::rsqrt_fast(GetLengthSquared()); }
	ILINE Vec4H GetNormalized() const      { return v * crymath::rsqrt(crymath::hsumv(v * v));	}
	ILINE Vec4H GetNormalizedFast() const  { return v * crymath::rsqrt_fast(crymath::hsumv(v * v));	}

	ILINE void Normalize()                 { v *= crymath::rsqrt(crymath::hsumv(v * v)); }
	ILINE void NormalizeFast()             { v *= crymath::rsqrt_fast(crymath::hsumv(v * v)); }

	ILINE Vec4H ProjectionOn(Vec4H b) const
	{
		return b.v * crymath::hsumv(v * b.v) * crymath::rcp(crymath::hsumv(b.v * b.v));
	}

	//
	// Compatibility with Vec4_tpl
	//
	using Vec4 = Vec4_tpl<F>;
	using Vec3 = Vec3_tpl<F>;

	ILINE Vec4H(const Vec4& v_)                               { v = convert<V>(&v_.x); CRY_MATH_ASSERT(IsValid()); }
	ILINE Vec4H(const Vec3& v_, F w_ = 0)                     { v = convert<V>(v_.x, v_.y, v_.z, w_); CRY_MATH_ASSERT(IsValid()); }
	ILINE operator Vec4() const                               { return reinterpret_cast<const Vec4&>(v); }
	ILINE operator Vec3() const                               { return reinterpret_cast<const Vec3&>(v); }

	template<typename F2> ILINE Vec4H(const Vec4_tpl<F2>& v_) { v = convert<V>((F)v_.x, (F)v_.y, (F)v_.z, (F)v_.w); }
	template<typename F2> ILINE operator Vec4_tpl<F2>() const { return Vec4_tpl<F2>((F2)x, (F2)y, (F2)z, (F2)w); }

	ILINE void SetLerp(Vec4H a, Vec4H b, F t)                 { *this = Lerp(a, b, t); CRY_MATH_ASSERT(IsValid()); }
	ILINE F    Dot(Vec4H b) const                             { return *this | b; }

	// External functions
	ILINE friend Vec4H min(const Vec4H& a, const Vec4H& b)    { return min(a.v, b.v); }
	ILINE friend Vec4H max(const Vec4H& a, const Vec4H& b)    { return max(a.v, b.v); }

	const CTypeInfo& TypeInfo() const                         { return ::TypeInfo((Vec4*)0); }
};

///////////////////////////////////////////////////////////////////////////////
// Plane structure using SIMD type

#ifndef CRY_COMPILER_GCC // Currently has a problem with Vec3 in anonymous struct

#define CRY_HARDWARE_PLANE4

template<typename F> struct PlaneH
{
	using V = vector4_t<F>;

	union
	{
		V v;
		struct
		{
			Vec3 n;
			F    d;
		};
	};

	ILINE PlaneH()
	{
#ifdef _DEBUG
		SetInvalid(v);
#endif
	}

	// Construction from normal+point, or 3 points
	ILINE PlaneH(const Vec3& n_, F d_)
	{
		v = convert<V>(n_.x, n_.y, n_.z, d_);
		CRY_MATH_ASSERT(IsValid());
	}
	ILINE PlaneH(const Vec3& normal, const Vec3& point)
		: PlaneH(normal, -(point | normal)) {}
	ILINE PlaneH(const Vec3& v0, const Vec3& v1, const Vec3& v2)
		: PlaneH(
		  ((v1 - v0) ^ (v2 - v0)).GetNormalized(), // normal of 3-point triangle
		  v0
		  ) {}

	// Conversions
	ILINE PlaneH(V v_) { v = v_; CRY_MATH_ASSERT(IsValid()); }
	ILINE operator V() const { return v; }

	// Operators
	ILINE bool operator==(const PlaneH& b) const { return All(v == b.v);  }
	ILINE bool operator!=(const PlaneH& b) const { return !All(v == b.v); }

	COMPOUND_MEMBER_OP(PlaneH, -, PlaneH)        { return v - b; }
	ILINE PlaneH operator-() const               { return -v; }

	ILINE F operator|(const Vec4H<F>& vec) const { return Vec4H<F>(v) | vec; }
	ILINE F operator|(const Vec3& point) const   { return Vec4H<F>(v) | Vec4H<F>(point, F(1)); } //!< Distance of point to plane

	// Properties
	ILINE bool IsValid(f32 epsilon = VEC_EPSILON) const { return ::IsValid(v) && n.IsUnit(epsilon); }

	//
	// Compatibility with Plane_tpl
	//
	using Plane = Plane_tpl<F>;
	using Vec3 = Vec3_tpl<F>;

	ILINE                PlaneH(const Plane& pl)                                     { v = convert<V>(&pl.n.x); CRY_MATH_ASSERT(IsValid()); }
	ILINE                operator const Plane&() const                               { return reinterpret_cast<const Plane&>(v); }

	ILINE void           Set(const Vec3& normal, const F distance)                   { *this = PlaneH(normal, distance);   }
	ILINE void           SetPlane(const Vec3& normal, const Vec3& point)             { *this = PlaneH(normal, point);  }
	ILINE static PlaneH  CreatePlane(const Vec3& normal, const Vec3& point)          { return PlaneH(normal, point); }
	ILINE void           SetPlane(const Vec3& v0, const Vec3& v1, const Vec3& v2)    { *this = PlaneH(v0, v1, v2); }
	ILINE static PlaneH  CreatePlane(const Vec3& v0, const Vec3& v1, const Vec3& v2) { return PlaneH(v0, v1, v2); }

	ILINE F              DistFromPlane(const Vec3& point) const                      { return *this | point; }

	Vec3                 MirrorVector(const Vec3& vec)                               { return n * (2 * (n | vec)) - vec;  }
	Vec3                 MirrorPosition(const Vec3& pos)                             { return pos - n * (2 * ((n | pos) + d)); }

	const CTypeInfo&     TypeInfo() const                                            { return ::TypeInfo((Plane*)0); }
};
#endif

#endif // CRY_TYPE_SIMD4

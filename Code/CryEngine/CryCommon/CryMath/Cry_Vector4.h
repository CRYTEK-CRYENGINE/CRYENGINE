// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Cry_Vector3.h"
	
template<typename F> struct Vec4_tpl
	: INumberVector<F, 4, Vec4_tpl<F>>
{
	F x, y, z, w;

	typedef INumberVector<F, 4, Vec4_tpl<F>> NV;
	using NV::IsValid;
	
	ILINE Vec4_tpl() {}
	ILINE Vec4_tpl(type_zero) : NV(ZERO) {}
	ILINE explicit Vec4_tpl(F s) : NV(s) {}

	template<typename F2>
	ILINE Vec4_tpl(const Vec4_tpl<F2>& in) { NV::Set(in); }

	ILINE Vec4_tpl(F vx, F vy, F vz, F vw) { x = vx; y = vy; z = vz; w = vw; CRY_MATH_ASSERT(IsValid()); };
	ILINE Vec4_tpl(const Vec3_tpl<F>& v, F vw) { x = v.x; y = v.y; z = v.z; w = vw; CRY_MATH_ASSERT(IsValid()); };

	ILINE operator const Vec3_tpl<F> &() const { return reinterpret_cast<const Vec3_tpl<F>&>(*this); }

	ILINE void operator()(F vx, F vy, F vz, F vw)     { x = vx; y = vy; z = vz; w = vw; CRY_MATH_ASSERT(IsValid()); };
	ILINE void operator()(const Vec3_tpl<F>& v, F vw) { x = v.x; y = v.y; z = v.z; vw = vw; CRY_MATH_ASSERT(IsValid()); };

	ILINE Vec4_tpl& zero() { NV::SetZero(); return *this; }

	ILINE Vec4_tpl ProjectionOn(const Vec4_tpl& b) const
	{
		return b * ((*this | b) * crymath::rcp_safe(b | b));
	}

	ILINE void            SetLerp(const Vec4_tpl& p, const Vec4_tpl& q, F t)    { *this = p * (1.0f - t) + q * t; }
	ILINE static Vec4_tpl CreateLerp(const Vec4_tpl& p, const Vec4_tpl& q, F t) { return p * (1.0f - t) + q * t; }

	AUTO_STRUCT_INFO;
};


template<typename F> ILINE Vec4_tpl<F> min(const Vec4_tpl<F>& a, const Vec4_tpl<F>& b)
{
	return Vec4_tpl<F>(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w));
}

template<typename F> ILINE Vec4_tpl<F> max(const Vec4_tpl<F>& a, const Vec4_tpl<F>& b)
{
	return Vec4_tpl<F>(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w));
}

// Typedefs
#include "Cry_Vector4H.h"

#ifdef CRY_HARDWARE_VECTOR4

typedef Vec4H<f32>  Vec4;
typedef Vec4H<int>  Vec4i;

#else

typedef Vec4_tpl<f32>  Vec4;
typedef Vec4_tpl<int>  Vec4i;

#endif

#ifdef CRY_HARDWARE_PLANE4

typedef PlaneH<f32> Plane;

#else

typedef Plane_tpl<f32> Plane;

#endif

typedef Vec4_tpl<f32>  Vec4f;
typedef Vec4_tpl<f64>  Vec4d;
typedef Vec4_tpl<real> Vec4r; //!< Variable float precision. depending on the target system it can be 32, 64 or 80 bit.

typedef Plane_tpl<f32>  Planef;
typedef Plane_tpl<f64>  Planed;
typedef Plane_tpl<real> Planer;   //!< Variable float precision. depending on the target system it can be between 32, 64 or 80 bit.


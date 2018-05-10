// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

template<typename T> struct Vec2_tpl;

template<typename T> struct Vec2Constants
{
	static Vec2_tpl<T> fVec2_Zero;
	static Vec2_tpl<T> fVec2_OneX;
	static Vec2_tpl<T> fVec2_OneY;
	static Vec2_tpl<T> fVec2_OneZ;
	static Vec2_tpl<T> fVec2_One;
};

//! General-purpose 2D vector implementation
//! \see Vec2, Vec2i
template<class F> struct Vec2_tpl
	: INumberVector<F, 2, Vec2_tpl<F>>
{
	F x, y;

	typedef INumberVector<F, 2, Vec2_tpl<F>> NV;
	using NV::IsValid;

	ILINE Vec2_tpl() {}
	ILINE Vec2_tpl(type_zero) : NV(ZERO) {}
	ILINE explicit Vec2_tpl(F s) : NV(s) {}

	template<typename F2>
	ILINE Vec2_tpl(const Vec2_tpl<F2>& in) { NV::Set(in); }

	ILINE Vec2_tpl(F vx, F vy) { x = vx; y = vy; CRY_MATH_ASSERT(IsValid()); }

	ILINE Vec2_tpl& set(F nx, F ny) { x = F(nx); y = F(ny); return *this; }

	template<class F1> ILINE explicit Vec2_tpl(const F1* psrc) { x = F(psrc[0]); y = F(psrc[1]); }

	//! Normalize if non-0, otherwise set to specified "safe" value.
	Vec2_tpl& NormalizeSafe(const struct Vec2_tpl<F>& safe = Vec2Constants<F>::fVec2_Zero)
	{
		NV::NormalizeSafe(safe);
		return *this;
	}
	Vec2_tpl GetNormalizedSafe(const struct Vec2_tpl<F>& safe = Vec2Constants<F>::fVec2_OneX) const
	{
		return NV::GetNormalizedSafe(safe);
	}

	ILINE F GetLength2() const
	{
		return NV::GetLengthSquared();
	}

	void SetLength(F fLen)
	{
		F fLenMe = GetLength2();
		if (fLenMe < 0.00001f * 0.00001f)
			return;
		fLenMe = fLen * isqrt_tpl(fLenMe);
		x *= fLenMe;
		y *= fLenMe;
	}

	ILINE F         area() const              { return x * y; }

	ILINE           operator F*()             { return &x; }
	ILINE Vec2_tpl& flip()                    { x = -x; y = -y; return *this; }
	ILINE Vec2_tpl& zero()                    { x = y = 0; return *this; }
	ILINE Vec2_tpl  rot90ccw() const          { return Vec2_tpl(-y, x); }
	ILINE Vec2_tpl  rot90cw() const           { return Vec2_tpl(y, -x); }

#ifdef quotient_h
	quotient_tpl<F> fake_atan2() const
	{
		quotient_tpl<F> res;
		int quad = -(signnz(x * x - y * y) - 1 >> 1); // hope the optimizer will remove complement shifts and adds
		if (quad) { res.x = -y; res.y = x; }
		else { res.x = x; res.y = y; }
		int sgny = signnz(res.y);
		quad |= 1 - sgny;                           //(res.y<0)<<1;
		res.x *= sgny;
		res.y *= sgny;
		res += 1 + (quad << 1);
		return res;
	}
#endif
	ILINE F         atan2() const        { return atan2_tpl(y, x); }

	// Deprecated
	ILINE bool IsZeroFast(F e = (F)0.0003) const
	{
		return NV::IsZero(e);
	}

	//! \return A vector perpendicular to this one (this->Cross(newVec) points "up").
	ILINE Vec2_tpl Perp() const { return Vec2_tpl(-y, x); }

	//! The size of the "parallel-trapets" area spanned by the two vectors.
	ILINE F Cross(const Vec2_tpl& v) const
	{
		return float (x * v.y - y * v.x);
	}
	ILINE F operator^(const Vec2_tpl& v) const { return Cross(v); }
	ILINE F operator*(const Vec2_tpl& v) const { return NV::Dot(v); }
	using NV::operator*;

	ILINE bool operator==(const Vec2_tpl<F>& o) const { return NV::IsEqual(o); }
	ILINE bool operator!=(const Vec2_tpl<F>& o) const { return !NV::IsEqual(o); }


	//! Linear-Interpolation between Vec3 (lerp).
	//! Example:
	//!   Vec3 r=Vec3::CreateLerp( p, q, 0.345f );
	ILINE void               SetLerp(const Vec2_tpl& p, const Vec2_tpl& q, F t)    { *this = p * (1.0f - t) + q * t; }
	ILINE static Vec2_tpl CreateLerp(const Vec2_tpl& p, const Vec2_tpl& q, F t) { return p * (1.0f - t) + q * t; }

	//! Spherical-Interpolation between 3d-vectors (geometrical slerp).
	//! both vectors are assumed to be normalized.
	//! Example:
	//!   Vec3 r=Vec3::CreateSlerp( p, q, 0.5674f );
	void SetSlerp(const Vec2_tpl& p, const Vec2_tpl& q, F t)
	{
		CRY_MATH_ASSERT((fabs_tpl(1 - (p | p))) < 0.005); //check if unit-vector
		CRY_MATH_ASSERT((fabs_tpl(1 - (q | q))) < 0.005); //check if unit-vector
		// calculate cosine using the "inner product" between two vectors: p*q=cos(radiant)
		F cosine = (p | q);
		//we explore the special cases where the both vectors are very close together,
		//in which case we approximate using the more economical LERP and avoid "divisions by zero" since sin(Angle) = 0  as   Angle=0
		if (cosine >= (F)0.99)
		{
			SetLerp(p, q, t); //perform LERP:
			NV::Normalize();
		}
		else
		{
			//perform SLERP: because of the LERP-check above, a "division by zero" is not possible
			F rad = acos_tpl(cosine);
			F scale_0 = sin_tpl((1 - t) * rad);
			F scale_1 = sin_tpl(t * rad);
			*this = (p * scale_0 + q * scale_1) / sin_tpl(rad);
			NV::Normalize();
		}
	}
	ILINE static Vec2_tpl CreateSlerp(const Vec2_tpl& p, const Vec2_tpl& q, F t)
	{
		Vec2_tpl v;
		v.SetSlerp(p, q, t);
		return v;
	}

	AUTO_STRUCT_INFO;
};

// Typedefs

typedef Vec2_tpl<f32>  Vec2;  //!< Always 32 bit.
typedef Vec2_tpl<f64>  Vec2d; //!< Always 64 bit.
typedef Vec2_tpl<int>  Vec2i;
typedef Vec2_tpl<real> Vec2r; //!< Variable float precision. depending on the target system it can be 32, 64 or 80 bit.

#if !defined(SWIG)
template<typename T> Vec2_tpl<T> Vec2Constants<T >::fVec2_Zero(0, 0);
template<typename T> Vec2_tpl<T> Vec2Constants<T >::fVec2_OneX(1, 0);
template<typename T> Vec2_tpl<T> Vec2Constants<T >::fVec2_OneY(0, 1);
template<typename T> Vec2_tpl<T> Vec2Constants<T >::fVec2_One(1, 1);
#endif



template<typename F> ILINE Vec2_tpl<F> min(const Vec2_tpl<F>& a, const Vec2_tpl<F>& b)
{
	return Vec2_tpl<F>(min(a.x, b.x), min(a.y, b.y));
}

template<typename F> ILINE Vec2_tpl<F> max(const Vec2_tpl<F>& a, const Vec2_tpl<F>& b)
{
	return Vec2_tpl<F>(max(a.x, b.x), max(a.y, b.y));
}

template<typename F> ILINE F sqr(const Vec2_tpl<F>& op)
{
	return op | op;
}


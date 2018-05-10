// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File: Cry_Vector3.h
//	Description: Common vector class
//
//	History:
//	-Feb 27,2003: Created by Ivo Herzeg
//
//////////////////////////////////////////////////////////////////////

#pragma once
#define VECTOR_H

template<typename F> struct Vec3_tpl;
template<typename F> struct Matrix34H;

template<typename T> struct Vec3Constants
{
	static const Vec3_tpl<T> fVec3_Zero;
	static const Vec3_tpl<T> fVec3_OneX;
	static const Vec3_tpl<T> fVec3_OneY;
	static const Vec3_tpl<T> fVec3_OneZ;
	static const Vec3_tpl<T> fVec3_One;
};

//! General-purpose 3D vector implementation
//! \see Vec3, Vec3i
template<typename F> struct Vec3_tpl
	: INumberVector<F, 3, Vec3_tpl<F>>
{
	F x, y, z;

	typedef INumberVector<F, 3, Vec3_tpl<F>> NV;
	using NV::IsValid;
	using NV::GetLengthSquared;
	using NV::Normalize;

	ILINE Vec3_tpl() {}
	ILINE Vec3_tpl(type_zero) : NV(ZERO) {}
	ILINE explicit Vec3_tpl(F s) : NV(s) {}

	template<typename F2>
	ILINE Vec3_tpl(const Vec3_tpl<F2>& in) { NV::Set(in); }

	//! Template specialization to initialize a vector.
	//! Example:
	//!   Vec3 v0=Vec3(ZERO);
	//!   Vec3 v1=Vec3(MIN);
	//!   Vec3 v2=Vec3(MAX);
	Vec3_tpl(type_min);
	Vec3_tpl(type_max);

	//! Constructors and bracket-operator to initialize a vector.
	//! Example:
	//!   Vec3 v0=Vec3(1,2,3);
	//!   Vec3 v1(1,2,3);
	//!   v2.Set(1,2,3);
	ILINE Vec3_tpl(F vx, F vy, F vz) : x(vx), y(vy), z(vz){ CRY_MATH_ASSERT(IsValid()); }
	ILINE void         operator()(F vx, F vy, F vz)                  { x = vx; y = vy; z = vz; CRY_MATH_ASSERT(IsValid()); }
	ILINE Vec3_tpl<F>& Set(const F xval, const F yval, const F zval) { x = xval; y = yval; z = zval; CRY_MATH_ASSERT(IsValid()); return *this; }

	ILINE Vec3_tpl<F>(const Vec2_tpl<F> &v, F vz = 0) { x = v.x; y = v.y; z = vz;  CRY_MATH_ASSERT(IsValid()); }
	template<class F2> ILINE Vec3_tpl<F>(const Vec2_tpl<F2> &v, F2 vz = 0) { x = F(v.x); y = F(v.y); z = F(vz);  CRY_MATH_ASSERT(IsValid()); }

	ILINE operator const Vec2_tpl<F> &() const { return reinterpret_cast<const Vec2_tpl<F>&>(*this); }

	ILINE bool operator==(const Vec3_tpl<F>& o) const  { return NV::IsEqual(o); }
	ILINE bool operator!=(const Vec3_tpl<F>& o) const  { return !NV::IsEqual(o); }

	//! Overloaded arithmetic operator.
	//! Example:
	//!   Vec3 v0=v1*4;

	ILINE Vec3_tpl<F>& Flip()
	{
		x = -x;
		y = -y;
		z = -z;
		return *this;
	}

	// Deprecated
	ILINE bool IsZeroFast(F e = (F)0.0003) const
	{
		return NV::IsZero(e);
	}

	using NV::IsEquivalent;
	ILINE static bool IsEquivalent(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, f32 epsilon = VEC_EPSILON)
	{
		return sqr(v0 - v1) <= sqr(epsilon);
	}

	ILINE bool IsUnit(f32 epsilon = VEC_EPSILON) const
	{
		return (fabs_tpl(1 - GetLengthSquared()) <= epsilon);
	}

	//! force vector length by normalizing it
	ILINE void SetLength(F fLen)
	{
		F fLenMe = GetLengthSquared();
		if (fLenMe < 0.00001f * 0.00001f)
			return;
		fLenMe = fLen * isqrt_tpl(fLenMe);
		x *= fLenMe;
		y *= fLenMe;
		z *= fLenMe;
	}

	ILINE void ClampLength(F maxLength)
	{
		F sqrLength = GetLengthSquared();
		if (sqrLength > (maxLength * maxLength))
		{
			F scale = maxLength * isqrt_tpl(sqrLength);
			x *= scale;
			y *= scale;
			z *= scale;
		}
	}

	//! calculate the length of the vector ignoring the z component
	ILINE F GetLength2D() const
	{
		return sqrt_tpl(x * x + y * y);
	}

	//! calculate the squared length of the vector ignoring the z component
	ILINE F GetLengthSquared2D() const
	{
		return x * x + y * y;
	}

	ILINE F GetSquaredDistance2D(const Vec3_tpl<F>& v) const
	{
		return (x - v.x) * (x - v.x) + (y - v.y) * (y - v.y);
	}

	//! Normalize the vector.
	//! Check for null vector - set to the passed in vector (which should be normalised!) if it is null vector.
	//! \return The original length of the vector.
	ILINE F NormalizeSafe(const Vec3_tpl<F>& safe = Vec3Constants<F>::fVec3_Zero)
	{
		return NV::NormalizeSafe(safe, std::numeric_limits<F>::epsilon());
	}
	//! return a safely normalized vector - returns safe vector (should be normalised) if original is zero length
	ILINE Vec3_tpl GetNormalizedSafe(const Vec3_tpl<F>& safe = Vec3Constants<F>::fVec3_OneX) const
	{
		return NV::GetNormalizedSafe(safe, std::numeric_limits<F>::epsilon());
	}

	//! Permutate coordinates so that z goes to new_z slot.
	ILINE Vec3_tpl GetPermutated(int new_z) const { return Vec3_tpl(*(&x + inc_mod3[new_z]), *(&x + dec_mod3[new_z]), *(&x + new_z)); }

	//! \return Volume of a box with this vector as diagonal.
	ILINE F GetVolume() const { return x * y * z; }

	//! \return A vector that consists of absolute values of this one's coordinates.
	ILINE Vec3_tpl<F> abs() const
	{
		return Vec3_tpl(fabs_tpl(x), fabs_tpl(y), fabs_tpl(z));
	}

	//! check for min bounds
	ILINE void CheckMin(const Vec3_tpl<F>& other)
	{
		x = min(other.x, x);
		y = min(other.y, y);
		z = min(other.z, z);
	}
	//! check for max bounds
	ILINE void CheckMax(const Vec3_tpl<F>& other)
	{
		x = max(other.x, x);
		y = max(other.y, y);
		z = max(other.z, z);
	}

	/*!
	 * sets a vector orthogonal to the input vector
	 *
	 * Example:
	 *  Vec3 v;
	 *  v.SetOrthogonal( i );
	 */
	ILINE void SetOrthogonal(const Vec3_tpl<F>& v)
	{
		sqr(F(0.9)) * (v | v) - v.x * v.x < 0 ? (x = -v.z, y = 0, z = v.x) : (x = 0, y = v.z, z = -v.y);
	}
	//! \return A vector orthogonal to this one.
	ILINE Vec3_tpl GetOrthogonal() const
	{
		return sqr(F(0.9)) * (x * x + y * y + z * z) - x * x < 0 ? Vec3_tpl<F>(-z, 0, x) : Vec3_tpl<F>(0, z, -y);
	}

	//! Project a point/vector on a (virtual) plane.
	//! Consider we have a plane going through the origin.
	//! Because d=0 we need just the normal. The vector n is assumed to be a unit-vector.
	//! Example:
	//!   Vec3 result=Vec3::CreateProjection( incident, normal );
	ILINE void SetProjection(const Vec3_tpl& i, const Vec3_tpl& n)
	{
		*this = i - n * (n | i);
	}
	ILINE static Vec3_tpl<F> CreateProjection(const Vec3_tpl& i, const Vec3_tpl& n)
	{
		return i - n * (n | i);
	}
	ILINE Vec3_tpl ProjectionOn(const Vec3_tpl& b) const
	{
		return b * ((*this | b) * crymath::rcp_safe(b | b));
	}

	//! Calculate a reflection vector. Vec3 n is assumed to be a unit-vector.
	//! Example:
	//!   Vec3 result=Vec3::CreateReflection( incident, normal );
	ILINE void SetReflection(const Vec3_tpl<F>& i, const Vec3_tpl<F>& n)
	{
		*this = (n * (i | n) * 2) - i;
	}
	ILINE static Vec3_tpl<F> CreateReflection(const Vec3_tpl<F>& i, const Vec3_tpl<F>& n)
	{
		return (n * (i | n) * 2) - i;
	}

	//! Linear-Interpolation between Vec3 (lerp).
	//! Example:
	//!   Vec3 r=Vec3::CreateLerp( p, q, 0.345f );
	ILINE void SetLerp(const Vec3_tpl<F>& p, const Vec3_tpl<F>& q, F t)
	{
		const Vec3_tpl<F> diff = q - p;
		*this = p + (diff * t);
	}
	ILINE static Vec3_tpl<F> CreateLerp(const Vec3_tpl<F>& p, const Vec3_tpl<F>& q, F t)
	{
		const Vec3_tpl<F> diff = q - p;
		return p + (diff * t);
	}

	//! Spherical-Interpolation between 3d-vectors (geometrical slerp) both vectors are assumed to be normalized.
	//! Example:
	//!   Vec3 r=Vec3::CreateSlerp( p, q, 0.5674f );
	void SetSlerp(const Vec3_tpl<F>& p, const Vec3_tpl<F>& q, F t)
	{
		CRY_MATH_ASSERT(p.IsUnit(0.005f));
		CRY_MATH_ASSERT(q.IsUnit(0.005f));
		// calculate cosine using the "inner product" between two vectors: p*q=cos(radiant)
		F cosine = clamp_tpl((p | q), F(-1), F(1));
		//we explore the special cases where the both vectors are very close together,
		//in which case we approximate using the more economical LERP and avoid "divisions by zero" since sin(Angle) = 0  as   Angle=0
		if (cosine >= (F)0.99)
		{
			SetLerp(p, q, t); //perform LERP:
			Normalize();
		}
		else
		{
			//perform SLERP: because of the LERP-check above, a "division by zero" is not possible
			F rad = acos_tpl(cosine);
			F scale_0 = sin_tpl((1 - t) * rad);
			F scale_1 = sin_tpl(t * rad);
			*this = (p * scale_0 + q * scale_1) / sin_tpl(rad);
			Normalize();
		}
	}
	ILINE static Vec3_tpl<F> CreateSlerp(const Vec3_tpl<F>& p, const Vec3_tpl<F>& q, F t)
	{
		Vec3_tpl<F> v;
		v.SetSlerp(p, q, t);
		return v;
	}

	//! Quadratic-Interpolation between vectors v0,v1,v2.
	//! This is repeated linear interpolation from 3 points and the resulting curve is a parabola.
	//! If t is in the range [0...1], then the curve goes only through v0 and v2.
	//! Example:
	//!   Vec3 ip; ip.SetQuadraticCurve( v0,v1,v2, 0.345f );
	ILINE void SetQuadraticCurve(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, const Vec3_tpl<F>& v2, F t1)
	{
		F t0 = 1.0f - t1;
		*this = t0 * t0 * v0 + t0 * t1 * 2.0f * v1 + t1 * t1 * v2;
	}
	ILINE static Vec3_tpl<F> CreateQuadraticCurve(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, const Vec3_tpl<F>& v2, F t)
	{
		Vec3_tpl<F> ip;
		ip.SetQuadraticCurve(v0, v1, v2, t);
		return ip;
	}

	//! Cubic-Interpolation between vectors v0,v1,v2,v3.
	//! This is repeated linear interpolation from 4 points.
	//! If t is in the range [0...1], then the curve goes only through v0 and v3.
	//! Example:
	//!   Vec3 ip; ip.SetCubicCurve( v0,v1,v2,v3, 0.345f );
	ILINE void SetCubicCurve(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, const Vec3_tpl<F>& v2, const Vec3_tpl<F>& v3, F t1)
	{
		F t0 = 1.0f - t1;
		*this = t0 * t0 * t0 * v0 + 3 * t0 * t0 * t1 * v1 + 3 * t0 * t1 * t1 * v2 + t1 * t1 * t1 * v3;
	}
	ILINE static Vec3_tpl<F> CreateCubicCurve(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, const Vec3_tpl<F>& v2, const Vec3_tpl<F>& v3, F t)
	{
		Vec3_tpl<F> ip;
		ip.SetCubicCurve(v0, v1, v2, v3, t);
		return ip;
	}

	//! Spline-Interpolation between vectors v0,v1,v2.
	//! This is a variation of a quadratic curve.
	//! If t is in the range [0...1], then the spline goes through all 3 points.
	//! Example:
	//!   Vec3 ip; ip.SetSplineInterpolation( v0,v1,v2, 0.345f );
	ILINE void SetQuadraticSpline(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, const Vec3_tpl<F>& v2, F t)
	{
		SetQuadraticCurve(v0, v1 - (v0 * 0.5f + v1 + v2 * 0.5f - v1 * 2.0f), v2, t);
	}
	ILINE static Vec3_tpl<F> CreateQuadraticSpline(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, const Vec3_tpl<F>& v2, F t)
	{
		Vec3_tpl<F> ip;
		ip.SetQuadraticSpline(v0, v1, v2, t);
		return ip;
	}

	//! Rotate a vector using angle and axis.
	//! Example:
	//!   Vec3 r=v.GetRotated(axis,angle);
	ILINE Vec3_tpl<F> GetRotated(const Vec3_tpl<F>& axis, F angle) const
	{
		return GetRotated(axis, cos_tpl(angle), sin_tpl(angle));
	}
	ILINE Vec3_tpl<F> GetRotated(const Vec3_tpl<F>& axis, F cosa, F sina) const
	{
		Vec3_tpl<F> zax = axis * (*this | axis);
		Vec3_tpl<F> xax = *this - zax;
		Vec3_tpl<F> yax = axis % xax;
		return xax * cosa + yax * sina + zax;
	}

	//! Rotate a vector around a center using angle and axis.
	//! Example:
	//!   Vec3 r=v.GetRotated(axis,angle);
	ILINE Vec3_tpl<F> GetRotated(const Vec3_tpl& center, const Vec3_tpl<F>& axis, F angle) const
	{
		return center + (*this - center).GetRotated(axis, angle);
	}
	ILINE Vec3_tpl<F> GetRotated(const Vec3_tpl<F>& center, const Vec3_tpl<F>& axis, F cosa, F sina) const
	{
		return center + (*this - center).GetRotated(axis, cosa, sina);
	}

	//! Component-wise multiplication of two vectors.
	ILINE Vec3_tpl CompMul(const Vec3_tpl<F>& rhs) const
	{
		return(Vec3_tpl(x * rhs.x, y * rhs.y, z * rhs.z));
	}

	// Legacy compatibility
	using NV::Dot;
	ILINE F Dot(const Vec2_tpl<F>& v)  const
	{
		return x * v.x + y * v.y;
	}
	//! Two methods for a "cross-product" operation.
	ILINE Vec3_tpl<F> Cross(const Vec3_tpl<F>& vec2) const
	{
		return Vec3_tpl<F>(y * vec2.z - z * vec2.y, z * vec2.x - x * vec2.z, x * vec2.y - y * vec2.x);
	}

	//F* fptr = vec;
	CRY_DEPRECATED("Use begin() instead") operator F*() { return (F*)this; }
	template<class T> CRY_DEPRECATED("Use Vec3_tpl(const Vec3_tpl&) instead") explicit Vec3_tpl(const T* src) { x = src[0]; y = src[1]; z = src[2]; }

	ILINE Vec3_tpl& zero()                                 { x = y = z = 0; return *this; }
	ILINE F         len() const
	{
		return sqrt_tpl(x * x + y * y + z * z);
	}
	ILINE F len2() const
	{
		return x * x + y * y + z * z;
	}

	ILINE Vec3_tpl& normalize()
	{
		F len2 = x * x + y * y + z * z;
		if (len2 > (F)1e-20f)
		{
			F rlen = isqrt_tpl(len2);
			x *= rlen;
			y *= rlen;
			z *= rlen;
		}
		else
			Set(0, 0, 1);
		return *this;
	}
	ILINE Vec3_tpl normalized() const
	{
		F len2 = x * x + y * y + z * z;
		if (len2 > (F)1e-20f)
		{
			F rlen = isqrt_tpl(len2);
			return Vec3_tpl(x * rlen, y * rlen, z * rlen);
		}
		else
			return Vec3_tpl(0, 0, 1);
	}

	//! Vector subtraction.
	template<class F1> ILINE Vec3_tpl<F1> sub(const Vec3_tpl<F1>& v) const
	{
		return Vec3_tpl<F1>(x - v.x, y - v.y, z - v.z);
	}
	//! Vector scale.
	template<class F1> ILINE Vec3_tpl<F1> scale(const F1 k) const
	{
		return Vec3_tpl<F>(x * k, y * k, z * k);
	}

	//! Vector dot product.
	template<class F1>
	ILINE F1 dot(const Vec3_tpl<F1>& v) const
	{
		return (F1)(x * v.x + y * v.y + z * v.z);
	}
	//! Vector cross product.
	template<class F1> ILINE Vec3_tpl<F1> cross(const Vec3_tpl<F1>& v) const
	{
		return Vec3_tpl<F1>(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
	}

	AUTO_STRUCT_INFO;
};

//specialization of the generic version from Cry_Math with adjusted epsilon
template<class F> ILINE bool IsEquivalent(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, f32 epsilon = VEC_EPSILON)
{
	return Vec3_tpl<F>::IsEquivalent(v0, v1, epsilon);
}

//! Dot product (2 versions).
template<class F1, class F2> ILINE F1 operator*(const Vec3_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
	return v0.Dot(v1);
}
//! Cross product (2 versions).
template<class F1, class F2> ILINE Vec3_tpl<F1> operator^(const Vec3_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
	return v0.Cross(v1);
}
template<class F1, class F2> ILINE Vec3_tpl<F1> operator%(const Vec3_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
	return v0.Cross(v1);
}

//! Vector addition.
template<class F1, class F2> ILINE Vec3_tpl<F1> operator+(const Vec2_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
	return Vec3_tpl<F1>(v0.x + v1.x, v0.y + v1.y, v1.z);
}
template<class F1, class F2> ILINE Vec3_tpl<F1> operator+(const Vec3_tpl<F1>& v0, const Vec2_tpl<F2>& v1)
{
	return Vec3_tpl<F1>(v0.x + v1.x, v0.y + v1.y, v0.z);
}
//! Vector subtraction.
template<class F1, class F2> ILINE Vec3_tpl<F1> operator-(const Vec2_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
	return Vec3_tpl<F1>(v0.x - v1.x, v0.y - v1.y, 0.0f - v1.z);
}
template<class F1, class F2> ILINE Vec3_tpl<F1> operator-(const Vec3_tpl<F1>& v0, const Vec2_tpl<F2>& v1)
{
	return Vec3_tpl<F1>(v0.x - v1.x, v0.y - v1.y, v0.z);
}

//---------------------------------------------------------------------------

// Typedefs
typedef Vec3_tpl<f32>  Vec3;   //!< Always 32 bit.
typedef Vec3_tpl<f64>  Vec3d;  //!< Always 64 bit.
typedef Vec3_tpl<int>  Vec3i;
typedef Vec3_tpl<real> Vec3r;  //!< Variable float precision. Depending on the target system it can be 32, 64 or 80 bit.

template<> inline Vec3_tpl<f64>::Vec3_tpl(type_min) { x = y = z = -1.7E308; }
template<> inline Vec3_tpl<f64>::Vec3_tpl(type_max) { x = y = z = 1.7E308; }
template<> inline Vec3_tpl<f32>::Vec3_tpl(type_min) { x = y = z = -3.3E38f; }
template<> inline Vec3_tpl<f32>::Vec3_tpl(type_max) { x = y = z = 3.3E38f; }

template<typename F> struct Quat_tpl;
template<typename F> struct Matrix33_tpl;
template<typename F> struct Matrix34_tpl;
template<typename F> struct Matrix44_tpl;

//! General-purpose 3D Euler angle container
//! \see Ang3
template<typename F> struct Ang3_tpl
	: INumberVector<F, 3, Ang3_tpl<F>>
{
	F x, y, z;

	typedef INumberVector<F, 3, Ang3_tpl<F>> NV;
	using NV::IsValid;

	Ang3_tpl() {}
	ILINE Ang3_tpl(type_zero) : NV(ZERO) {}
	ILINE explicit Ang3_tpl(F s) : NV(s) {}
	ILINE Ang3_tpl(F vx, F vy, F vz) : x(vx), y(vy), z(vz) { CRY_MATH_ASSERT(IsValid()); }

	ILINE Ang3_tpl(const Vec3_tpl<F> &v) : x(v.x), y(v.y), z(v.z) { CRY_MATH_ASSERT(IsValid()); }
	ILINE operator const Vec3_tpl<F>&() const { return reinterpret_cast<const Vec3_tpl<F>&>(*this); }

	ILINE void  operator()(F vx, F vy, F vz) { x = vx; y = vy; z = vz; CRY_MATH_ASSERT(IsValid()); }
	ILINE Ang3_tpl<F>& Set(F vx, F vy, F vz) { x = vx; y = vy; z = vz; CRY_MATH_ASSERT(IsValid()); return *this; }
	
	ILINE bool IsInRangePI() const
	{
		F pi = (F)(gf_PI + 0.001); //we need this to fix fp-precision problem
		return  ((x > -pi) && (x < pi) && (y > -pi) && (y < pi) && (z > -pi) && (z < pi));
	}

	//! Normalize angle to -pi and +pi range.
	ILINE void RangePI()
	{
		x = crymath::wrap(x, -gf_PI, +gf_PI);
		y = crymath::wrap(y, -gf_PI, +gf_PI);
		z = crymath::wrap(z, -gf_PI, +gf_PI);
	}

	//! Convert unit quaternion to angle (xyz).
	explicit Ang3_tpl(const Quat_tpl<F>& q)
	{
		CRY_MATH_ASSERT(q.IsValid());
		y = asin_tpl(-(q.v.x * q.v.z - q.w * q.v.y) * 2);
		if (fabs_tpl(fabs_tpl(y) - (F)((F)g_PI * (F)0.5)) < (F)0.01)
		{
			x = F(0);
			z = F(atan2_tpl(-2 * (q.v.x * q.v.y - q.w * q.v.z), 1 - (q.v.x * q.v.x + q.v.z * q.v.z) * 2));
		}
		else
		{
			x = F(atan2_tpl((q.v.y * q.v.z + q.w * q.v.x) * 2, 1 - (q.v.x * q.v.x + q.v.y * q.v.y) * 2));
			z = F(atan2_tpl((q.v.x * q.v.y + q.w * q.v.z) * 2, 1 - (q.v.z * q.v.z + q.v.y * q.v.y) * 2));
		}
	}

	//! Convert matrix to angle (xyz).
	template<typename M> void FromMatrix(const M& m)
	{
		CRY_MATH_ASSERT(Matrix33_tpl<F>(m).IsOrthonormalRH(0.001f));
		y = asin_tpl(-m.m20);
		if (fabs_tpl(fabs_tpl(y) - (F)((F)g_PI * (F)0.5)) < (F)0.01)
		{
			x = F(0);
			z = F(atan2_tpl(-m.m01, m.m11));
		}
		else
		{
			x = F(atan2_tpl(m.m21, m.m22));
			z = F(atan2_tpl(m.m10, m.m00));
		}
	}

	explicit Ang3_tpl(const Matrix33_tpl<F>& m) { FromMatrix(m); }
	explicit Ang3_tpl(const Matrix34_tpl<F>& m) { FromMatrix(m); }
	explicit Ang3_tpl(const Matrix44_tpl<F>& m) { FromMatrix(m); }

	static ILINE F CreateRadZ(const Vec2_tpl<F>& v0, const Vec2_tpl<F>& v1)
	{
		F cz = v0.x * v1.y - v0.y * v1.x;
		F c = v0.x * v1.x + v0.y * v1.y;
		return F(atan2_tpl(cz, c));
	}

	static ILINE F CreateRadZ(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1)
	{
		F cz = v0.x * v1.y - v0.y * v1.x;
		F c = v0.x * v1.x + v0.y * v1.y;
		return F(atan2_tpl(cz, c));
	}

	ILINE static Ang3_tpl<F> GetAnglesXYZ(const Quat_tpl<F>& q)     { return Ang3_tpl<F>(q); }
	ILINE void               SetAnglesXYZ(const Quat_tpl<F>& q)     { *this = Ang3_tpl<F>(q); }

	ILINE static Ang3_tpl<F> GetAnglesXYZ(const Matrix33_tpl<F>& m) { return Ang3_tpl<F>(m); }
	ILINE void               SetAnglesXYZ(const Matrix33_tpl<F>& m) { *this = Ang3_tpl<F>(m); }

	ILINE static Ang3_tpl<F> GetAnglesXYZ(const Matrix34_tpl<F>& m) { return Ang3_tpl<F>(m); }
	ILINE void               SetAnglesXYZ(const Matrix34_tpl<F>& m) { *this = Ang3_tpl<F>(m); }

	AUTO_STRUCT_INFO;
};

typedef Ang3_tpl<f32>  Ang3;
typedef Ang3_tpl<real> Ang3r;
typedef Ang3_tpl<f64>  Ang3_f64;

//---------------------------------------

template<typename F> struct AngleAxis_tpl
{

	//! Storage for the Angle&Axis coordinates.
	F           angle;
	Vec3_tpl<F> axis;

	//! Default quaternion constructor.
	AngleAxis_tpl(void) {};
	AngleAxis_tpl(F a, F ax, F ay, F az) { angle = a; axis.x = ax; axis.y = ay; axis.z = az; }
	AngleAxis_tpl(F a, const Vec3_tpl<F>& n) { angle = a; axis = n; }
	void operator()(F a, const Vec3_tpl<F>& n) { angle = a; axis = n; }

	//! CAngleAxis aa=angleaxis.
	AngleAxis_tpl(const AngleAxis_tpl<F>& aa);
	const Vec3_tpl<F> operator*(const Vec3_tpl<F>& v) const;

	AngleAxis_tpl(const Quat_tpl<F>& q)
	{
		angle = acos_tpl(q.w) * 2;
		axis = q.v;
		axis.Normalize();
		F s = sin_tpl(angle * (F)0.5);
		if (s == 0)
		{
			angle = 0;
			axis.x = 0;
			axis.y = 0;
			axis.z = 1;
		}
	}

};

typedef AngleAxis_tpl<f32> AngleAxis;
typedef AngleAxis_tpl<f64> AngleAxis_f64;

template<typename F>
ILINE const Vec3_tpl<F> AngleAxis_tpl<F >::operator*(const Vec3_tpl<F>& v) const
{
	Vec3_tpl<F> origin = axis * (axis | v);
	return origin + (v - origin) * cos_tpl(angle) + (axis % v) * sin_tpl(angle);
}

//////////////////////////////////////////////////////////////////////
template<typename F> struct Plane_tpl
{

	//plane-equation: n.x*x + n.y*y + n.z*z + d > 0 is in front of the plane
	Vec3_tpl<F> n;   //!< Normal.
	F           d;   //!< Distance.

	//----------------------------------------------------------------

	ILINE Plane_tpl()
	{
#ifdef _DEBUG
		SetInvalid(d);
#endif
	}

	ILINE Plane_tpl(const Vec3_tpl<F>& normal, const F& distance) { n = normal; d = distance; }

	//! set normal and dist for this plane and  then calculate plane type
	ILINE void Set(const Vec3_tpl<F>& vNormal, const F fDist)
	{
		n = vNormal;
		d = fDist;
	}

	ILINE void SetPlane(const Vec3_tpl<F>& normal, const Vec3_tpl<F>& point)
	{
		n = normal;
		d = -(point | normal);
	}
	ILINE static Plane_tpl<F> CreatePlane(const Vec3_tpl<F>& normal, const Vec3_tpl<F>& point)
	{
		return Plane_tpl<F>(normal, -(point | normal));
	}

	ILINE Plane_tpl<F> operator-(void) const { return Plane_tpl<F>(-n, -d); }

	//! Constructs the plane by tree given Vec3s (=triangle) with a right-hand (anti-clockwise) winding.
	//! Example 1:
	//!   Vec3 v0(1,2,3),v1(4,5,6),v2(6,5,6);
	//!   Plane_tpl<F>  plane;
	//!   plane.SetPlane(v0,v1,v2);
	//! Example 2:
	//!   Vec3 v0(1,2,3),v1(4,5,6),v2(6,5,6);
	//!   Plane_tpl<F>  plane=Plane_tpl<F>::CreatePlane(v0,v1,v2);
	ILINE void SetPlane(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, const Vec3_tpl<F>& v2)
	{
		n = ((v1 - v0) % (v2 - v0)).GetNormalized(); //vector cross-product
		d = -(n | v0);                               //calculate d-value
	}
	ILINE static Plane_tpl<F> CreatePlane(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, const Vec3_tpl<F>& v2)
	{
		Plane_tpl<F> p;
		p.SetPlane(v0, v1, v2);
		return p;
	}

	//! Computes signed distance from point to plane.
	//! This is the standard plane-equation: d=Ax*By*Cz+D.
	//! The normal-vector is assumed to be normalized.
	//! Example:
	//!   Vec3 v(1,2,3);
	//!   Plane_tpl<F>  plane=CalculatePlane(v0,v1,v2);
	//!   F distance = plane|v;
	ILINE F            operator|(const Vec3_tpl<F>& point) const      { return (n | point) + d; }
	ILINE F            DistFromPlane(const Vec3_tpl<F>& vPoint) const { return (n * vPoint + d); }

	ILINE Plane_tpl<F> operator-(const Plane_tpl<F>& p) const         { return Plane_tpl<F>(n - p.n, d - p.d); }
	ILINE Plane_tpl<F> operator+(const Plane_tpl<F>& p) const         { return Plane_tpl<F>(n + p.n, d + p.d); }
	ILINE void         operator-=(const Plane_tpl<F>& p)              { d -= p.d; n -= p.n; }
	ILINE Plane_tpl<F> operator*(F s) const                           { return Plane_tpl<F>(n * s, d * s); }
	ILINE Plane_tpl<F> operator/(F s) const                           { return Plane_tpl<F>(n / s, d / s); }

	//! check for equality between two planes
	friend  bool operator==(const Plane_tpl<F>& p1, const Plane_tpl<F>& p2)
	{
		if (fabsf(p1.n.x - p2.n.x) > 0.0001f) return (false);
		if (fabsf(p1.n.y - p2.n.y) > 0.0001f) return (false);
		if (fabsf(p1.n.z - p2.n.z) > 0.0001f) return (false);
		if (fabsf(p1.d - p2.d) < 0.01f) return(true);
		return (false);
	}

	Vec3_tpl<F> MirrorVector(const Vec3_tpl<F>& i)   { return n * (2 * (n | i)) - i;  }
	Vec3_tpl<F> MirrorPosition(const Vec3_tpl<F>& i) { return i - n * (2 * ((n | i) + d)); }

	AUTO_STRUCT_INFO;
};

// define the constants

#if !defined(SWIG)
template<typename T> const Vec3_tpl<T> Vec3Constants<T >::fVec3_Zero(0, 0, 0);
template<typename T> const Vec3_tpl<T> Vec3Constants<T >::fVec3_OneX(1, 0, 0);
template<typename T> const Vec3_tpl<T> Vec3Constants<T >::fVec3_OneY(0, 1, 0);
template<typename T> const Vec3_tpl<T> Vec3Constants<T >::fVec3_OneZ(0, 0, 1);
template<typename T> const Vec3_tpl<T> Vec3Constants<T >::fVec3_One(1, 1, 1);
#endif

// Specialized functions
template<typename F> ILINE Vec3_tpl<F> min(const Vec3_tpl<F>& a, const Vec3_tpl<F>& b)
{
	return Vec3_tpl<F>(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
}

template<typename F> ILINE Vec3_tpl<F> max(const Vec3_tpl<F>& a, const Vec3_tpl<F>& b)
{
	return Vec3_tpl<F>(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
}

template<typename F> ILINE F sqr(const Vec3_tpl<F>& op)
{
	return op | op;
}

template<typename F> ILINE int32 idxmax3(const Vec3_tpl<F>& vec)
{
	int32 imax = isneg(vec.x - vec.y);
	imax |= isneg(vec[imax] - vec.z) << 1;
	return imax & (2 | (imax >> 1 ^ 1));
}
template<typename F> ILINE int32 idxmin3(const Vec3_tpl<F>& vec)
{
	int32 imin = isneg(vec.y - vec.x);
	imin |= isneg(vec.z - vec[imin]) << 1;
	return imin & (2 | (imin >> 1 ^ 1));
}


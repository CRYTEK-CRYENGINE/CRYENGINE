// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#define _CRYQUAT_H

#include <limits>

const float RAD_EPSILON = 0.01f;

//! General-purpose quaternion implementation
//! \see Quat
template<typename F> struct Quat_tpl
	: INumberVector<F, 4, Quat_tpl<F>>
{
	Vec3_tpl<F> v;
	F           w;

	typedef INumberVector<F, 4, Quat_tpl<F>> NV;
	using NV::IsValid;
	using NV::Normalize;

	ILINE Quat_tpl() {}
	ILINE Quat_tpl(type_zero) { NV::SetZero(); }
	ILINE Quat_tpl(type_identity): v(0), w(1) {}
	template<class F2> ILINE Quat_tpl(const Quat_tpl<F2> &q) { NV::Set(q); }

	//! CONSTRUCTOR to initialize a Quat from 4 floats.
	//! Quat q(1,0,0,0);
	ILINE Quat_tpl<F>(F qw, F qx, F qy, F qz)
	{
		w = qw;
		v.x = qx;
		v.y = qy;
		v.z = qz;
		CRY_MATH_ASSERT(IsValid());
	}
	//! CONSTRUCTOR to initialize a Quat with a scalar and a vector.
	//! Quat q(1,Vec3(0,0,0));
	ILINE Quat_tpl<F>(F scalar, const Vec3_tpl<F> &vector)
	{
		v = vector;
		w = scalar;
		CRY_MATH_ASSERT(IsValid());
	};


	//! CONSTRUCTOR for different types. It converts a Euler Angle into a Quat.
	//! Needs to be 'explicit' because we loose fp-precision in the conversion process.
	//! Quat(Ang3(1,2,3));
	explicit ILINE Quat_tpl<F>(const Ang3_tpl<F> &ang)
	{
		CRY_MATH_ASSERT(ang.IsValid());
		SetRotationXYZ(ang);
	}
	//! CONSTRUCTOR for different types. It converts a Euler Angle into a Quat and converts between double/float.
	//! Needs to be 'explicit' because we loose fp-precision in the conversion process.
	//! Quat(Ang3r(1,2,3));
	template<class F1> explicit ILINE Quat_tpl<F>(const Ang3_tpl<F1> &ang)
	{
		CRY_MATH_ASSERT(ang.IsValid());
		SetRotationXYZ(Ang3_tpl<F>(F(ang.x), F(ang.y), F(ang.z)));
	}

	//! Extract quat from any matrix type which has members of the form m12, etc
	template<class F1, class M>
	void SetFromMatrix(const M& m)
	{
		CRY_MATH_ASSERT(m.IsOrthonormalRH(0.1f));
		SetIdentity();
		F1 s, p, tr = m.m00 + m.m11 + m.m22;
		if (tr > 0)
			s = sqrt_tpl(tr + 1.0f), p = 0.5f / s, w = F(s * 0.5), v.x = F((m.m21 - m.m12) * p), v.y = F((m.m02 - m.m20) * p), v.z = F((m.m10 - m.m01) * p);
		else if ((m.m00 >= m.m11) && (m.m00 >= m.m22))
			s = sqrt_tpl(m.m00 - m.m11 - m.m22 + 1.0f), p = 0.5f / s, w = F((m.m21 - m.m12) * p), v.x = F(s * 0.5), v.y = F((m.m10 + m.m01) * p), v.z = F((m.m20 + m.m02) * p);
		else if ((m.m11 >= m.m00) && (m.m11 >= m.m22))
			s = sqrt_tpl(m.m11 - m.m22 - m.m00 + 1.0f), p = 0.5f / s, w = F((m.m02 - m.m20) * p), v.x = F((m.m01 + m.m10) * p), v.y = F(s * 0.5), v.z = F((m.m21 + m.m12) * p);
		else if ((m.m22 >= m.m00) && (m.m22 >= m.m11))
			s = sqrt_tpl(m.m22 - m.m00 - m.m11 + 1.0f), p = 0.5f / s, w = F((m.m10 - m.m01) * p), v.x = F((m.m02 + m.m20) * p), v.y = F((m.m12 + m.m21) * p), v.z = F(s * 0.5);
	}

	template<class F1> explicit Quat_tpl(const Matrix33_tpl<F1> &m) { SetFromMatrix<F1>(m); }
	template<class F1> explicit Quat_tpl(const Matrix34_tpl<F1> &m) { SetFromMatrix<F1>(m); }
	template<class F1> explicit Quat_tpl(const Matrix44_tpl<F1> &m) { SetFromMatrix<F1>(m); }

#ifdef CRY_TYPE_SIMD4
	template<class F1> explicit Quat_tpl(const Matrix34H<F1> &m) { SetFromMatrix<F1>(m); }
	template<class F1> explicit Quat_tpl(const Matrix44H<F1> &m) { SetFromMatrix<F1>(m); }
#endif

	//! Invert quaternion.
	//! Example 1:
	//!  Quat q=Quat::CreateRotationXYZ(Ang3(1,2,3));
	//!  Quat result = !q;
	//!  Quat result = GetInverted(q);
	//!  q.Invert();
	ILINE Quat_tpl<F> operator!() const   { return Quat_tpl(w, -v); }
	ILINE void        Invert(void)        { *this = !*this; }
	ILINE Quat_tpl<F> GetInverted() const { return !(*this); }

	//! A quaternion is a compressed matrix. Thus there is no problem extracting the rows & columns.
	ILINE Vec3_tpl<F> GetColumn(uint32 i)
	{
		if (i == 0) return GetColumn0();
		if (i == 1) return GetColumn1();
		if (i == 2) return GetColumn2();
		CRY_MATH_ASSERT(0); //bad index
		return Vec3(ZERO);
	}

	ILINE Vec3_tpl<F> GetColumn0() const { return Vec3_tpl<F>(2 * (v.x * v.x + w * w) - 1, 2 * (v.y * v.x + v.z * w), 2 * (v.z * v.x - v.y * w)); }
	ILINE Vec3_tpl<F> GetColumn1() const { return Vec3_tpl<F>(2 * (v.x * v.y - v.z * w), 2 * (v.y * v.y + w * w) - 1, 2 * (v.z * v.y + v.x * w)); }
	ILINE Vec3_tpl<F> GetColumn2() const { return Vec3_tpl<F>(2 * (v.x * v.z + v.y * w), 2 * (v.y * v.z - v.x * w), 2 * (v.z * v.z + w * w) - 1); }
	ILINE Vec3_tpl<F> GetRow0() const    { return Vec3_tpl<F>(2 * (v.x * v.x + w * w) - 1, 2 * (v.x * v.y - v.z * w), 2 * (v.x * v.z + v.y * w)); }
	ILINE Vec3_tpl<F> GetRow1() const    { return Vec3_tpl<F>(2 * (v.y * v.x + v.z * w), 2 * (v.y * v.y + w * w) - 1, 2 * (v.y * v.z - v.x * w)); }
	ILINE Vec3_tpl<F> GetRow2() const    { return Vec3_tpl<F>(2 * (v.z * v.x - v.y * w), 2 * (v.z * v.y + v.x * w), 2 * (v.z * v.z + w * w) - 1); }

	// These are just copy & pasted components of the GetColumn1() above.
	ILINE F GetFwdX() const { return (2 * (v.x * v.y - v.z * w)); }
	ILINE F GetFwdY() const { return (2 * (v.y * v.y + w * w) - 1); }
	ILINE F GetFwdZ() const { return (2 * (v.z * v.y + v.x * w)); }
	ILINE F GetRotZ() const { return atan2_tpl(-GetFwdX(), GetFwdY()); }

	//! Set identity quaternion.
	//! Example:
	//!   Quat q=Quat::CreateIdentity();
	//!   or
	//!   q.SetIdentity();
	//!   or
	//!   Quat p=Quat(IDENTITY);
	ILINE void SetIdentity(void)
	{
		w = 1;
		v.x = 0;
		v.y = 0;
		v.z = 0;
	}
	ILINE static Quat_tpl<F> CreateIdentity(void)
	{
		return Quat_tpl<F>(1, 0, 0, 0);
	}

	//! Check if identity quaternion.
	ILINE bool IsIdentity() const
	{
		return w == 1 && v.x == 0 && v.y == 0 && v.z == 0;
	}

	ILINE bool IsUnit(f32 e = VEC_EPSILON) const
	{
		return fabs_tpl(1 - (w * w + v.x * v.x + v.y * v.y + v.z * v.z)) < e;
	}

	ILINE void SetRotationAA(F rad, const Vec3_tpl<F>& axis)
	{
		F s, c;
		sincos_tpl(rad * (F)0.5, &s, &c);
		SetRotationAA(c, s, axis);
	}
	ILINE static Quat_tpl<F> CreateRotationAA(F rad, const Vec3_tpl<F>& axis)
	{
		Quat_tpl<F> q;
		q.SetRotationAA(rad, axis);
		return q;
	}

	ILINE void SetRotationAA(F cosha, F sinha, const Vec3_tpl<F>& axis)
	{
		CRY_MATH_ASSERT(axis.IsUnit(0.001f));
		w = cosha;
		v = axis * sinha;
	}
	ILINE static Quat_tpl<F> CreateRotationAA(F cosha, F sinha, const Vec3_tpl<F>& axis)
	{
		Quat_tpl<F> q;
		q.SetRotationAA(cosha, sinha, axis);
		return q;
	}

	//! Create rotation-quaternion that around the fixed coordinate axes.
	//! Example:
	//!   Quat q=Quat::CreateRotationXYZ( Ang3(1,2,3) );
	//!   or
	//!   q.SetRotationXYZ( Ang3(1,2,3) );
	void SetRotationXYZ(const Ang3_tpl<F>& a)
	{
		CRY_MATH_ASSERT(a.IsValid());
		F sx, cx;
		sincos_tpl(F(a.x * F(0.5)), &sx, &cx);
		F sy, cy;
		sincos_tpl(F(a.y * F(0.5)), &sy, &cy);
		F sz, cz;
		sincos_tpl(F(a.z * F(0.5)), &sz, &cz);
		w = cx * cy * cz + sx * sy * sz;
		v.x = cz * cy * sx - sz * sy * cx;
		v.y = cz * sy * cx + sz * cy * sx;
		v.z = sz * cy * cx - cz * sy * sx;
	}
	static Quat_tpl<F> CreateRotationXYZ(const Ang3_tpl<F>& a)
	{
		CRY_MATH_ASSERT(a.IsValid());
		Quat_tpl<F> q;
		q.SetRotationXYZ(a);
		return q;
	}

	//! Create rotation-quaternion that about the x-axis.
	//! Example:
	//!   Quat q=Quat::CreateRotationX( radiant );
	//!   or
	//!   q.SetRotationX( Ang3(1,2,3) );
	ILINE void SetRotationX(F r)
	{
		F s, c;
		sincos_tpl(F(r * F(0.5)), &s, &c);
		w = c;
		v.x = s;
		v.y = 0;
		v.z = 0;
	}
	ILINE static Quat_tpl<F> CreateRotationX(F r)
	{
		Quat_tpl<F> q;
		q.SetRotationX(r);
		return q;
	}

	//! Create rotation-quaternion that about the y-axis.
	//! Example:
	//!   Quat q=Quat::CreateRotationY( radiant );
	//!   or
	//!   q.SetRotationY( radiant );
	ILINE void SetRotationY(F r)
	{
		F s, c;
		sincos_tpl(F(r * F(0.5)), &s, &c);
		w = c;
		v.x = 0;
		v.y = s;
		v.z = 0;
	}
	ILINE static Quat_tpl<F> CreateRotationY(F r)
	{
		Quat_tpl<F> q;
		q.SetRotationY(r);
		return q;
	}

	//! Create rotation-quaternion that about the z-axis.
	//! Example:
	//!   Quat q=Quat::CreateRotationZ( radiant );
	//!   or
	//!   q.SetRotationZ( radiant );
	ILINE void SetRotationZ(F r)
	{
		F s, c;
		sincos_tpl(F(r * F(0.5)), &s, &c);
		w = c;
		v.x = 0;
		v.y = 0;
		v.z = s;
	}
	ILINE static Quat_tpl<F> CreateRotationZ(F r)
	{
		Quat_tpl<F> q;
		q.SetRotationZ(r);
		return q;
	}

	//! Create rotation-quaternion that rotates from one vector to another.
	//! Both vectors are assumed to be normalized.
	//! Example:
	//!   Quat q=Quat::CreateRotationV0V1( v0,v1 );
	//!   q.SetRotationV0V1( v0,v1 );
	void SetRotationV0V1(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1)
	{
		CRY_MATH_ASSERT(v0.IsUnit(0.01f));
		CRY_MATH_ASSERT(v1.IsUnit(0.01f));
		F dot = v0.x * v1.x + v0.y * v1.y + v0.z * v1.z + F(1.0);
		if (dot > F(0.0001))
		{
			F vx = v0.y * v1.z - v0.z * v1.y;
			F vy = v0.z * v1.x - v0.x * v1.z;
			F vz = v0.x * v1.y - v0.y * v1.x;
			F d = isqrt_tpl(dot * dot + vx * vx + vy * vy + vz * vz);
			w = F(dot * d);
			v.x = F(vx * d);
			v.y = F(vy * d);
			v.z = F(vz * d);
			return;
		}
		w = 0;
		v = v0.GetOrthogonal().GetNormalized();
	}
	static Quat_tpl<F> CreateRotationV0V1(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1) { Quat_tpl<F> q;  q.SetRotationV0V1(v0, v1);   return q; }

	//! \param vdir  Normalized view direction.
	//! \param roll  Radiant to rotate about Y-axis.
	//! Given a view-direction and a radiant to rotate about Y-axis, this function builds a 3x3 look-at quaternion
	//! using only simple vector arithmetic. This function is always using the implicit up-vector Vec3(0,0,1).
	//! The view-direction is always stored in column(1).
	//! IMPORTANT: The view-vector is assumed to be normalized, because all trig-values for the orientation are being
	//! extracted  directly out of the vector. This function must NOT be called with a view-direction
	//! that is close to Vec3(0,0,1) or Vec3(0,0,-1). If one of these rules is broken, the function returns a quaternion
	//! with an undefined rotation about the Z-axis.
	//! Rotation order for the look-at-quaternion is Z-X-Y. (Zaxis=YAW / Xaxis=PITCH / Yaxis=ROLL)
	//! COORDINATE-SYSTEM
	//! z-axis
	//!   ^
	//!   |
	//!   |  y-axis
	//!   |  /
	//!   | /
	//!   |/
	//!   +--------------->   x-axis
	//! Example:
	//!   Quat LookAtQuat=Quat::CreateRotationVDir( Vec3(0,1,0) );
	//!   or
	//!   Quat LookAtQuat=Quat::CreateRotationVDir( Vec3(0,1,0), 0.333f );
	void SetRotationVDir(const Vec3_tpl<F>& vdir)
	{
		CRY_MATH_ASSERT(vdir.IsUnit(0.01f));
		//set default initialization for up-vector
		w = F(0.70710676908493042);
		v.x = F(vdir.z * 0.70710676908493042);
		v.y = F(0.0);
		v.z = F(0.0);
		F l = sqrt_tpl(vdir.x * vdir.x + vdir.y * vdir.y);
		if (l > F(0.00001))
		{
			//calculate LookAt quaternion
			Vec3_tpl<F> hv = Vec3_tpl<F>(vdir.x / l, vdir.y / l + 1.0f, l + 1.0f);
			F r = sqrt_tpl(hv.x * hv.x + hv.y * hv.y);
			F s = sqrt_tpl(hv.z * hv.z + vdir.z * vdir.z);
			//generate the half-angle sine&cosine
			F hacos0 = 0.0;
			F hasin0 = -1.0;
			if (r > F(0.00001)) { hacos0 = hv.y / r; hasin0 = -hv.x / r; }  //yaw
			F hacos1 = hv.z / s;
			F hasin1 = vdir.z / s;                        //pitch
			w = F(hacos0 * hacos1);
			v.x = F(hacos0 * hasin1);
			v.y = F(hasin0 * hasin1);
			v.z = F(hasin0 * hacos1);
		}
	}
	static Quat_tpl<F> CreateRotationVDir(const Vec3_tpl<F>& vdir) { Quat_tpl<F> q;  q.SetRotationVDir(vdir); return q;  }

	void SetRotationVDir(const Vec3_tpl<F>& vdir, F r)
	{
		SetRotationVDir(vdir);
		F sy, cy;
		sincos_tpl(r * (F)0.5, &sy, &cy);
		F vx = v.x, vy = v.y;
		v.x = F(vx * cy - v.z * sy);
		v.y = F(w * sy + vy * cy);
		v.z = F(v.z * cy + vx * sy);
		w = F(w * cy - vy * sy);
	}
	static Quat_tpl<F> CreateRotationVDir(const Vec3_tpl<F>& vdir, F roll) { Quat_tpl<F> q; q.SetRotationVDir(vdir, roll);  return q; }

	ILINE void NormalizeSafe(void)
	{
		NV::NormalizeSafe(Quat_tpl(IDENTITY), 1e-8f);
	}
	ILINE Quat_tpl<F> GetNormalizedSafe() const
	{
		return NV::GetNormalizedSafe(Quat_tpl(IDENTITY), 1e-8f);
	}

	ILINE static bool IsEquivalent(const Quat_tpl<F>& q1, const Quat_tpl<F>& q2, f32 qe = RAD_EPSILON)
	{
		const F cosmin = F(1) - sqr(qe) * F(0.5f);  // Approx value of cos(qe)
		F cos = abs(q1 | q2);  // -q is equivalent to q
		return cos >= cosmin;
	}

	//! Exponent of a Quaternion.
	ILINE static Quat_tpl<F> exp(const Vec3_tpl<F>& v)
	{
		F lensqr = v.len2();
		if (lensqr > F(0))
		{
			F len = sqrt_tpl(lensqr);
			F s, c;
			sincos_tpl(len, &s, &c);
			s /= len;
			return Quat_tpl<F>(c, v.x * s, v.y * s, v.z * s);
		}
		return Quat_tpl<F>(IDENTITY);
	}

	//! Logarithm of a quaternion, imaginary part (the real part of the logarithm is always 0).
	ILINE static Vec3_tpl<F> log(const Quat_tpl<F>& q)
	{
		CRY_MATH_ASSERT(q.IsValid());
		F lensqr = q.v.len2();
		if (lensqr > 0.0f)
		{
			F len = sqrt_tpl(lensqr);
			F angle = atan2_tpl(len, q.w) / len;
			return q.v * angle;
		}
		// logarithm of a quaternion, imaginary part (the real part of the logarithm is always 0)
		return Vec3_tpl<F>(0, 0, 0);
	}

	//! Logarithm of Quaternion difference.
	ILINE static Quat_tpl<F> LnDif(const Quat_tpl<F>& q1, const Quat_tpl<F>& q2)
	{
		return Quat_tpl<F>(0, log(q2 / q1));
	}

	//! Linear-interpolation between quaternions (lerp).
	//! Example:
	//!   CQuaternion result,p,q;
	//!   result=qlerp( p, q, 0.5f );
	ILINE void SetNlerp(const Quat_tpl<F>& p, const Quat_tpl<F>& tq, F t)
	{
		Quat_tpl<F> q = tq;
		CRY_MATH_ASSERT(p.IsValid());
		CRY_MATH_ASSERT(q.IsValid());
		if ((p | q) < 0) { q = -q; }
		v.x = p.v.x * (1.0f - t) + q.v.x * t;
		v.y = p.v.y * (1.0f - t) + q.v.y * t;
		v.z = p.v.z * (1.0f - t) + q.v.z * t;
		w = p.w * (1.0f - t) + q.w * t;
		Normalize();
	}

	ILINE static Quat_tpl<F> CreateNlerp(const Quat_tpl<F>& p, const Quat_tpl<F>& tq, F t)
	{
		Quat_tpl<F> d;
		d.SetNlerp(p, tq, t);
		return d;
	}

	//! Linear-interpolation between unit quaternions (nlerp).
	//! In this case we convert the t-value into a 1d cubic spline to get closer to Slerp.
	//! Example:
	//!   Quat result,p,q;
	//!   result.SetNlerpCubic( p, q, 0.5f );
	void SetNlerpCubic(const Quat_tpl<F>& p, const Quat_tpl<F>& tq, F t)
	{
		Quat_tpl<F> q = tq;
		CRY_MATH_ASSERT((fabs_tpl(1 - (p | p))) < 0.001); //check if unit-quaternion
		CRY_MATH_ASSERT((fabs_tpl(1 - (q | q))) < 0.001); //check if unit-quaternion
		F cosine = (p | q);
		if (cosine < 0) q = -q;
		F k = (1 - fabs_tpl(cosine)) * F(0.4669269);
		F s = 2 * k * t * t * t - 3 * k * t * t + (1 + k) * t;
		v.x = p.v.x * (1.0f - s) + q.v.x * s;
		v.y = p.v.y * (1.0f - s) + q.v.y * s;
		v.z = p.v.z * (1.0f - s) + q.v.z * s;
		w = p.w * (1.0f - s) + q.w * s;
		Normalize();
	}
	static Quat_tpl<F> CreateNlerpCubic(const Quat_tpl<F>& p, const Quat_tpl<F>& tq, F t)
	{
		Quat_tpl<F> d;
		d.SetNlerpCubic(p, tq, t);
		return d;
	}

	//! Spherical-interpolation between unit quaternions (geometrical slerp).
	void SetSlerp(const Quat_tpl<F>& tp, const Quat_tpl<F>& tq, F t)
	{
		CRY_MATH_ASSERT(tp.IsValid());
		CRY_MATH_ASSERT(tq.IsValid());
		CRY_MATH_ASSERT(tp.IsUnit());
		CRY_MATH_ASSERT(tq.IsUnit());
		Quat_tpl<F> p = tp, q = tq;
		Quat_tpl<F> q2;

		F cosine = (p | q);
		if (cosine < 0.0f) { cosine = -cosine; q = -q; } //take shortest arc
		if (cosine > 0.9999f)
		{
			SetNlerp(p, q, t);
			return;
		}
		// from now on, a division by 0 is not possible any more
		q2.w = q.w - p.w * cosine;
		q2.v.x = q.v.x - p.v.x * cosine;
		q2.v.y = q.v.y - p.v.y * cosine;
		q2.v.z = q.v.z - p.v.z * cosine;
		F sine = sqrt(q2 | q2);

		// If sine is 0 here (so you get a divide by zero below), probably you passed in non-normalized quaternions!
		CRY_MATH_ASSERT(sine != 0.0000f);

		F s, c;
		sincos_tpl(atan2_tpl(sine, cosine) * t, &s, &c);
		w = F(p.w * c + q2.w * s / sine);
		v.x = F(p.v.x * c + q2.v.x * s / sine);
		v.y = F(p.v.y * c + q2.v.y * s / sine);
		v.z = F(p.v.z * c + q2.v.z * s / sine);
	}

	//! Create a quaternion by spherically interpolating between unit quaternions (geometrical slerp)
	static Quat_tpl<F> CreateSlerp(const Quat_tpl<F>& p, const Quat_tpl<F>& tq, F t)
	{
		Quat_tpl<F> d;
		d.SetSlerp(p, tq, t);
		return d;
	}

	//! Spherical-interpolation between unit quaternions (algebraic slerp_a).
	//! I have included this function just for the sake of completeness, because
	//! it is the only useful application to check if exp & log really work.
	//! Both slerp-functions return the same result.
	//! Example:
	//!   Quat result,p,q;
	//!   result.SetExpSlerp( p,q,0.3345f );
	ILINE void SetExpSlerp(const Quat_tpl<F>& p, const Quat_tpl<F>& tq, F t)
	{
		CRY_MATH_ASSERT((fabs_tpl(1 - (p | p))) < 0.001);   //check if unit-quaternion
		CRY_MATH_ASSERT((fabs_tpl(1 - (tq | tq))) < 0.001); //check if unit-quaternion
		Quat_tpl<F> q = tq;
		if ((p | q) < 0) { q = -q; }
		*this = p * exp(log(!p * q) * t);           //algebraic slerp (1)
		//...and some more exp-slerp-functions producing all the same result
		//*this = exp( log (p* !q) * (1-t)) * q;	//algebraic slerp (2)
		//*this = exp( log (q* !p) * t) * p;			//algebraic slerp (3)
		//*this = q * exp( log (!q*p) * (1-t));		//algebraic slerp (4)
	}
	ILINE static Quat_tpl<F> CreateExpSlerp(const Quat_tpl<F>& p, const Quat_tpl<F>& q, F t)
	{
		Quat_tpl<F> d;
		d.SetExpSlerp(p, q, t);
		return d;
	}

	//! squad(p,a,b,q,t) = slerp( slerp(p,q,t),slerp(a,b,t), 2(1-t)t).
	ILINE void SetSquad(const Quat_tpl<F>& p, const Quat_tpl<F>& a, const Quat_tpl<F>& b, const Quat_tpl<F>& q, F t)
	{
		SetSlerp(CreateSlerp(p, q, t), CreateSlerp(a, b, t), 2.0f * (1.0f - t) * t);
	}

	ILINE static Quat_tpl<F> CreateSquad(const Quat_tpl<F>& p, const Quat_tpl<F>& a, const Quat_tpl<F>& b, const Quat_tpl<F>& q, F t)
	{
		Quat_tpl<F> d;
		d.SetSquad(p, a, b, q, t);
		return d;
	}

	//! Useless, please delete.
	ILINE Quat_tpl<F> GetScaled(F scale) const
	{
		return CreateNlerp(IDENTITY, *this, scale);
	}

	AUTO_STRUCT_INFO;
};

template<typename F>
ILINE bool IsEquivalent(const Quat_tpl<F>& q1, const Quat_tpl<F>& q2, f32 qe = RAD_EPSILON)
{
	return Quat_tpl<F>::IsEquivalent(q1, q2, qe);
}


// Typedefs

#ifndef MAX_API_NUM
typedef Quat_tpl<f32>  Quat;  //!< Always 32 bit.
typedef Quat_tpl<f64>  Quatd; //!< Always 64 bit.
typedef Quat_tpl<real> Quatr; //!< Variable float precision. depending on the target system it can be between 32, 64 or 80 bit.
#endif

typedef Quat_tpl<f32>  CryQuat;
typedef Quat_tpl<f32>  quaternionf;
typedef Quat_tpl<real> quaternion;

// aligned versions
#ifndef MAX_API_NUM
typedef CRY_ALIGN (16) Quat QuatA;
typedef CRY_ALIGN (32) Quatd QuatrA;
#endif

//! Implements the multiplication operator: Qua=QuatA*QuatB.
//! AxB = operation B followed by operation A.
//! A multiplication takes 16 muls and 12 adds.
//! Example 1:
//!   Quat p(1,0,0,0),q(1,0,0,0);
//!   Quat result=p*q;
//! Example 2: (self-multiplication)
//!   Quat p(1,0,0,0),q(1,0,0,0);
//!   Quat p*=q;
template<class F1, class F2> Quat_tpl<F1> ILINE operator*(const Quat_tpl<F1>& q, const Quat_tpl<F2>& p)
{
	CRY_MATH_ASSERT(q.IsValid());
	CRY_MATH_ASSERT(p.IsValid());
	return Quat_tpl<F1>
	       (
	  q.w * p.w - (q.v.x * p.v.x + q.v.y * p.v.y + q.v.z * p.v.z),
	  q.v.y * p.v.z - q.v.z * p.v.y + q.w * p.v.x + q.v.x * p.w,
	  q.v.z * p.v.x - q.v.x * p.v.z + q.w * p.v.y + q.v.y * p.w,
	  q.v.x * p.v.y - q.v.y * p.v.x + q.w * p.v.z + q.v.z * p.w
	       );
}
template<class F1, class F2> ILINE void operator*=(Quat_tpl<F1>& q, const Quat_tpl<F2>& p)
{
	CRY_MATH_ASSERT(q.IsValid());
	CRY_MATH_ASSERT(p.IsValid());
	F1 s0 = q.w;
	q.w = q.w * p.w - (q.v | p.v);
	q.v = p.v * s0 + q.v * p.w + (q.v % p.v);
}

//! Implements the multiplication operator: QuatT=Quat*Quatpos.
//! AxB = operation B followed by operation A.
//! A multiplication takes 31 muls and 27 adds (=58 float operations).
//! Example:
//!   Quat  quat    = Quat::CreateRotationX(1.94192f);
//!   QuatT quatpos	= QuatT::CreateRotationZ(3.14192f,Vec3(11,22,33));
//!   QuatT qp      = quat*quatpos;
template<class F1, class F2> ILINE QuatT_tpl<F1> operator*(const Quat_tpl<F1>& q, const QuatT_tpl<F2>& p)
{
	return QuatT_tpl<F1>(q * p.q, q * p.t);
}

//! Example 1:
//!   Quat p(1,0,0,0),q(1,0,0,0);
//!   Quat result=p/q;
//! Example 2: (self-division)
//!   Quat p(1,0,0,0),q(1,0,0,0);
//!   Quat p/=q;
template<class F1, class F2> ILINE Quat_tpl<F1> operator/(const Quat_tpl<F1>& q, const Quat_tpl<F2>& p)
{
	return (!p * q);
}
template<class F1, class F2> ILINE void operator/=(Quat_tpl<F1>& q, const Quat_tpl<F2>& p)
{
	q = (!p * q);
}

//! Post-multiply of a quaternion and a Vec3 (3D rotations with quaternions).
//! Example:
//!   Quat q(1,0,0,0);
//!   Vec3 v(33,44,55);
//!   Vec3 result = q*v;
template<class F, class F2> ILINE Vec3_tpl<F> operator*(const Quat_tpl<F>& q, const Vec3_tpl<F2>& v)
{
	CRY_MATH_ASSERT(v.IsValid());
	CRY_MATH_ASSERT(q.IsValid());
	//muls=15 / adds=15
	Vec3_tpl<F> out, r2;
	r2.x = (q.v.y * v.z - q.v.z * v.y) + q.w * v.x;
	r2.y = (q.v.z * v.x - q.v.x * v.z) + q.w * v.y;
	r2.z = (q.v.x * v.y - q.v.y * v.x) + q.w * v.z;
	out.x = (r2.z * q.v.y - r2.y * q.v.z);
	out.x += out.x + v.x;
	out.y = (r2.x * q.v.z - r2.z * q.v.x);
	out.y += out.y + v.y;
	out.z = (r2.y * q.v.x - r2.x * q.v.y);
	out.z += out.z + v.z;
	return out;
}

//! Pre-multiply of a quaternion and a Vec3 (3D rotations with quaternions).
//! Example:
//!   Quat q(1,0,0,0);
//!   Vec3 v(33,44,55);
//!   Vec3 result = v*q;
template<class F, class F2> ILINE Vec3_tpl<F2> operator*(const Vec3_tpl<F>& v, const Quat_tpl<F2>& q)
{
	CRY_MATH_ASSERT(v.IsValid());
	CRY_MATH_ASSERT(q.IsValid());
	//muls=15 / adds=15
	Vec3_tpl<F> out, r2;
	r2.x = (q.v.z * v.y - q.v.y * v.z) + q.w * v.x;
	r2.y = (q.v.x * v.z - q.v.z * v.x) + q.w * v.y;
	r2.z = (q.v.y * v.x - q.v.x * v.y) + q.w * v.z;
	out.x = (r2.y * q.v.z - r2.z * q.v.y);
	out.x += out.x + v.x;
	out.y = (r2.z * q.v.x - r2.x * q.v.z);
	out.y += out.y + v.y;
	out.z = (r2.x * q.v.y - r2.y * q.v.x);
	out.z += out.z + v.z;
	return out;
}

template<class F1, class F2> ILINE Quat_tpl<F1> operator%(const Quat_tpl<F1>& q, const Quat_tpl<F2>& tp)
{
	Quat_tpl<F1> p = tp;
	if ((p | q) < 0) { p = -p; }
	return Quat_tpl<F1>(q.w + p.w, q.v + p.v);
}
template<class F1, class F2> ILINE void operator%=(Quat_tpl<F1>& q, const Quat_tpl<F2>& tp)
{
	Quat_tpl<F1> p = tp;
	if ((p | q) < 0) { p = -p; }
	q = Quat_tpl<F1>(q.w + p.w, q.v + p.v);
}

// Quaternion with translation vector.
template<typename F> struct QuatT_tpl
{
	Quat_tpl<F> q; //this is the quaternion
	Vec3_tpl<F> t; //this is the translation vector and a scalar (for uniform scaling?)

	ILINE QuatT_tpl(){}

	//! Initialize with zeros.
	ILINE QuatT_tpl(type_zero) : q(ZERO), t(ZERO) {}
	ILINE QuatT_tpl(type_identity) : q(IDENTITY), t(ZERO) {}

	ILINE QuatT_tpl(const Vec3_tpl<F>& _t, const Quat_tpl<F>& _q) { q = _q; t = _t; }

	//! CONSTRUCTOR: implement the copy/casting/assignment constructor.
	template<typename F1> ILINE QuatT_tpl(const QuatT_tpl<F1>& qt) : q(qt.q), t(qt.t) {}

	//! Convert unit DualQuat back to QuatT.
	ILINE QuatT_tpl(const DualQuat_tpl<F>& qd)
	{
		//copy quaternion part
		q = qd.nq;
		//convert translation vector:
		t = (qd.nq.w * qd.dq.v - qd.dq.w * qd.nq.v + qd.nq.v % qd.dq.v) * 2; //perfect for HLSL
	}

	explicit ILINE QuatT_tpl(const QuatTS_tpl<F>& qts) : q(qts.q), t(qts.t) {}

	explicit ILINE QuatT_tpl(const Matrix34_tpl<F>& m)
	{
		q = Quat_tpl<F>(Matrix33(m));
		t = m.GetTranslation();
	}

	ILINE QuatT_tpl(const Quat_tpl<F>& quat, const Vec3_tpl<F>& trans)
	{
		q = quat;
		t = trans;
	}

	ILINE void SetIdentity()
	{
		q.w = 1;
		q.v.x = 0;
		q.v.y = 0;
		q.v.z = 0;
		t.x = 0;
		t.y = 0;
		t.z = 0;
	}

	ILINE bool IsIdentity() const
	{
		return (q.IsIdentity() && t.IsZero());
	}

	//! Convert three Euler angle to mat33 (rotation order:XYZ).
	//! The Euler angles are assumed to be in radians.
	//! The translation-vector is set to zero by default.
	//! Example 1:
	//!   QuatT qp;
	//!   qp.SetRotationXYZ( Ang3(0.5f,0.2f,0.9f), translation );
	//! Example 2:
	//!   QuatT qp=QuatT::CreateRotationXYZ( Ang3(0.5f,0.2f,0.9f), translation );
	ILINE void SetRotationXYZ(const Ang3_tpl<F>& rad, const Vec3_tpl<F>& trans = Vec3(ZERO))
	{
		CRY_MATH_ASSERT(rad.IsValid());
		CRY_MATH_ASSERT(trans.IsValid());
		q.SetRotationXYZ(rad);
		t = trans;
	}
	ILINE static QuatT_tpl<F> CreateRotationXYZ(const Ang3_tpl<F>& rad, const Vec3_tpl<F>& trans = Vec3(ZERO))
	{
		CRY_MATH_ASSERT(rad.IsValid());
		CRY_MATH_ASSERT(trans.IsValid());
		QuatT_tpl<F> qp;
		qp.SetRotationXYZ(rad, trans);
		return qp;
	}

	ILINE void SetRotationAA(F cosha, F sinha, const Vec3_tpl<F> axis, const Vec3_tpl<F>& trans = Vec3(ZERO))
	{
		q.SetRotationAA(cosha, sinha, axis);
		t = trans;
	}
	ILINE static QuatT_tpl<F> CreateRotationAA(F cosha, F sinha, const Vec3_tpl<F> axis, const Vec3_tpl<F>& trans = Vec3(ZERO))
	{
		QuatT_tpl<F> qt;
		qt.SetRotationAA(cosha, sinha, axis, trans);
		return qt;
	}

	//! In-place inversion.
	ILINE void Invert()
	{
		CRY_MATH_ASSERT(q.IsValid());
		t = -t * q;
		q = !q;
	}
	ILINE QuatT_tpl<F> GetInverted() const
	{
		CRY_MATH_ASSERT(q.IsValid());
		QuatT_tpl<F> qpos;
		qpos.q = !q;
		qpos.t = -t * q;
		return qpos;
	}

	ILINE void        SetTranslation(const Vec3_tpl<F>& trans) { t = trans;  }

	ILINE Vec3_tpl<F> GetColumn0() const                       { return q.GetColumn0(); }
	ILINE Vec3_tpl<F> GetColumn1() const                       { return q.GetColumn1(); }
	ILINE Vec3_tpl<F> GetColumn2() const                       { return q.GetColumn2(); }
	ILINE Vec3_tpl<F> GetColumn3() const                       { return t; }
	ILINE Vec3_tpl<F> GetRow0() const                          { return q.GetRow0(); }
	ILINE Vec3_tpl<F> GetRow1() const                          { return q.GetRow1(); }
	ILINE Vec3_tpl<F> GetRow2() const                          { return q.GetRow2(); }

	ILINE static bool IsEquivalent(const QuatT_tpl<F>& qt1, const QuatT_tpl<F>& qt2, f32 qe = RAD_EPSILON, f32 ve = VEC_EPSILON)
	{
		return ::IsEquivalent(qt1.q, qt2.q, qe) && ::IsEquivalent(qt1.t, qt2.t, ve);
	}

	ILINE bool IsValid() const
	{
		if (!t.IsValid()) return false;
		if (!q.IsValid()) return false;
		return true;
	}

	//! Linear-interpolation between quaternions (lerp).
	//! Example:
	//!   CQuaternion result,p,q;
	//!   result=qlerp( p, q, 0.5f );
	ILINE void SetNLerp(const QuatT_tpl<F>& p, const QuatT_tpl<F>& tq, F ti)
	{
		CRY_MATH_ASSERT(p.q.IsValid());
		CRY_MATH_ASSERT(tq.q.IsValid());
		Quat_tpl<F> d = tq.q;
		if ((p.q | d) < 0) { d = -d; }
		Vec3_tpl<F> vDiff = d.v - p.q.v;
		q.v = p.q.v + (vDiff * ti);
		q.w = p.q.w + ((d.w - p.q.w) * ti);
		q.Normalize();
		vDiff = tq.t - p.t;
		t = p.t + (vDiff * ti);
	}
	ILINE static QuatT_tpl<F> CreateNLerp(const QuatT_tpl<F>& p, const QuatT_tpl<F>& q, F t)
	{
		QuatT_tpl<F> d;
		d.SetNLerp(p, q, t);
		return d;
	}

	//! \note All vectors are stored in columns.
	ILINE void SetFromVectors(const Vec3& vx, const Vec3& vy, const Vec3& vz, const Vec3& pos)
	{
		Matrix34 m34;
		m34.m00 = vx.x;
		m34.m01 = vy.x;
		m34.m02 = vz.x;
		m34.m03 = pos.x;
		m34.m10 = vx.y;
		m34.m11 = vy.y;
		m34.m12 = vz.y;
		m34.m13 = pos.y;
		m34.m20 = vx.z;
		m34.m21 = vy.z;
		m34.m22 = vz.z;
		m34.m23 = pos.z;
		*this = QuatT_tpl<F>(m34);
	}
	ILINE static QuatT_tpl<F> CreateFromVectors(const Vec3_tpl<F>& vx, const Vec3_tpl<F>& vy, const Vec3_tpl<F>& vz, const Vec3_tpl<F>& pos)
	{
		QuatT_tpl<F> qt;
		qt.SetFromVectors(vx, vy, vz, pos);
		return qt;
	}

	QuatT_tpl<F> GetScaled(F scale)
	{
		return QuatT_tpl<F>(t * scale, q.GetScaled(scale));
	}

	AUTO_STRUCT_INFO;
};

typedef QuatT_tpl<f32>  QuatT;   //!< Always 32 bit.
typedef QuatT_tpl<f64>  QuatTd;  //!< Always 64 bit.
typedef QuatT_tpl<real> QuatTr;  //!< Variable float precision. depending on the target system it can be between 32, 64 or bit.

//! aligned versions
typedef CRY_ALIGN (32) QuatT QuatTA;       //!< wastes 4byte per quatT
typedef CRY_ALIGN (16) QuatTd QuatTrA;

template<typename F>
ILINE bool IsEquivalent(const QuatT_tpl<F>& qt1, const QuatT_tpl<F>& qt2, f32 qe = RAD_EPSILON, f32 ve = VEC_EPSILON)
{
	return QuatT_tpl<F>::IsEquivalent(qt1, qt2, qe, ve);
}


//! Implements the multiplication operator: QuatT=Quatpos*Quat.
//! AxB = operation B followed by operation A.
//! A multiplication takes 16 muls and 12 adds (=28 float operations).
//! Example:
//!   Quat  quat    = Quat::CreateRotationX(1.94192f);
//!   QuatT quatpos	= QuatT::CreateRotationZ(3.14192f,Vec3(11,22,33));
//!   QuatT qp      = quatpos*quat;
template<class F1, class F2> ILINE QuatT_tpl<F1> operator*(const QuatT_tpl<F1>& p, const Quat_tpl<F2>& q)
{
	CRY_MATH_ASSERT(p.IsValid());
	CRY_MATH_ASSERT(q.IsValid());
	return QuatT_tpl<F1>(p.q * q, p.t);
}

//! Implements the multiplication operator: QuatT=QuatposA*QuatposB.
//! AxB = operation B followed by operation A.
//! A multiplication takes 31 muls and 30 adds  (=61 float operations).
//! Example:
//!   QuatT quatposA	=	QuatT::CreateRotationX(1.94192f,Vec3(77,55,44));
//!   QuatT quatposB	=	QuatT::CreateRotationZ(3.14192f,Vec3(11,22,33));
//!   QuatT qp				=	quatposA*quatposB;
template<class F1, class F2> ILINE QuatT_tpl<F1> operator*(const QuatT_tpl<F1>& q, const QuatT_tpl<F2>& p)
{
	CRY_MATH_ASSERT(q.IsValid());
	CRY_MATH_ASSERT(p.IsValid());
	return QuatT_tpl<F1>(q.q * p.q, q.q * p.t + q.t);
}

//! Post-multiply of a QuatT and a Vec3 (3D rotations with quaternions).
//! Example:
//!   Quat q(1,0,0,0);
//!   Vec3 v(33,44,55);
//!   Vec3 result = q*v;
template<class F, class F2> Vec3_tpl<F> operator*(const QuatT_tpl<F>& q, const Vec3_tpl<F2>& v)
{
	CRY_MATH_ASSERT(v.IsValid());
	CRY_MATH_ASSERT(q.IsValid());
	//muls=15 / adds=15+3
	Vec3_tpl<F> out, r2;
	r2.x = (q.q.v.y * v.z - q.q.v.z * v.y) + q.q.w * v.x;
	r2.y = (q.q.v.z * v.x - q.q.v.x * v.z) + q.q.w * v.y;
	r2.z = (q.q.v.x * v.y - q.q.v.y * v.x) + q.q.w * v.z;
	out.x = (r2.z * q.q.v.y - r2.y * q.q.v.z);
	out.x += out.x + v.x + q.t.x;
	out.y = (r2.x * q.q.v.z - r2.z * q.q.v.x);
	out.y += out.y + v.y + q.t.y;
	out.z = (r2.y * q.q.v.x - r2.x * q.q.v.y);
	out.z += out.z + v.z + q.t.z;
	return out;
}

//! Quaternion with translation vector and scale.
//! Similar to QuatT, but s is not ignored.
//! Most functions then differ, so we don't inherit.
template<typename F> struct QuatTS_tpl
{
	Quat_tpl<F> q;
	Vec3_tpl<F> t;
	F           s;

	// constructors
	ILINE QuatTS_tpl()
	{
#if defined(_DEBUG)
		SetInvalid(s);
#endif
	}

	ILINE QuatTS_tpl(const Quat_tpl<F>& quat, const Vec3_tpl<F>& trans, F scale = 1)  { q = quat; t = trans; s = scale; }
	ILINE QuatTS_tpl(type_identity) { SetIdentity(); }

	ILINE QuatTS_tpl(const QuatT_tpl<F>& qp)  { q = qp.q; t = qp.t; s = 1.0f; }

	ILINE void SetIdentity()
	{
		q.SetIdentity();
		t = Vec3(ZERO);
		s = 1;
	}

	explicit ILINE QuatTS_tpl(const Matrix34_tpl<F>& m)
	{
		t = m.GetTranslation();
		s = m.GetUniformScale();

		//! Orthonormalize using X and Z as anchors.
		const Vec3_tpl<F>& r0 = m.GetRow(0);
		const Vec3_tpl<F>& r2 = m.GetRow(2);

		const Vec3_tpl<F> v0 = r0.GetNormalized();
		const Vec3_tpl<F> v1 = (r2 % r0).GetNormalized();
		const Vec3_tpl<F> v2 = (v0 % v1);

		Matrix33_tpl<F> m3;
		m3.SetRow(0, v0);
		m3.SetRow(1, v1);
		m3.SetRow(2, v2);

		q = Quat_tpl<F>(m3);
	}

	void Invert()
	{
		s = 1 / s;
		q = !q;
		t = q * t * -s;
	}
	QuatTS_tpl<F> GetInverted() const
	{
		QuatTS_tpl<F> inv;
		inv.s = 1 / s;
		inv.q = !q;
		inv.t = inv.q * t * -inv.s;
		return inv;
	}

	//! Linear-interpolation between quaternions (nlerp).
	//! Example:
	//!   CQuaternion result,p,q;
	//!   result=qlerp( p, q, 0.5f );
	ILINE void SetNLerp(const QuatTS_tpl<F>& p, const QuatTS_tpl<F>& tq, F ti)
	{
		CRY_MATH_ASSERT(p.q.IsValid());
		CRY_MATH_ASSERT(tq.q.IsValid());
		Quat_tpl<F> d = tq.q;
		if ((p.q | d) < 0) { d = -d; }
		Vec3_tpl<F> vDiff = d.v - p.q.v;
		q.v = p.q.v + (vDiff * ti);
		q.w = p.q.w + ((d.w - p.q.w) * ti);
		q.Normalize();

		vDiff = tq.t - p.t;
		t = p.t + (vDiff * ti);

		s = p.s + ((tq.s - p.s) * ti);
	}

	ILINE static QuatTS_tpl<F> CreateNLerp(const QuatTS_tpl<F>& p, const QuatTS_tpl<F>& q, F t)
	{
		QuatTS_tpl<F> d;
		d.SetNLerp(p, q, t);
		return d;
	}

	ILINE static bool IsEquivalent(const QuatTS_tpl<F>& qts1, const QuatTS_tpl<F>& qts2, f32 qe = RAD_EPSILON, f32 ve = VEC_EPSILON)
	{
		return ::IsEquivalent(qts1.q, qts2.q, qe) && ::IsEquivalent(qts1.t, qts2.t, ve) && ::IsEquivalent(qts1.s, qts2.s, ve);
	}

	bool IsValid(f32 e = VEC_EPSILON) const
	{
		if (!q.IsUnit(e)) return false;
		if (!t.IsValid()) return false;
		if (!::IsValid(s)) return false;
		return true;
	}

	ILINE Vec3_tpl<F> GetColumn0() const { return q.GetColumn0(); }
	ILINE Vec3_tpl<F> GetColumn1() const { return q.GetColumn1(); }
	ILINE Vec3_tpl<F> GetColumn2() const { return q.GetColumn2(); }
	ILINE Vec3_tpl<F> GetColumn3() const { return t; }
	ILINE Vec3_tpl<F> GetRow0() const    { return q.GetRow0(); }
	ILINE Vec3_tpl<F> GetRow1() const    { return q.GetRow1(); }
	ILINE Vec3_tpl<F> GetRow2() const    { return q.GetRow2(); }

	AUTO_STRUCT_INFO;
};

typedef QuatTS_tpl<f32>  QuatTS;   //!< Always 32 bit.
typedef QuatTS_tpl<f64>  QuatTSd;  //!< Always 64 bit.
typedef QuatTS_tpl<real> QuatTSr;  //!< Variable float precision. depending on the target system it can be between 32, 64 or 80 bit.

// aligned versions
typedef CRY_ALIGN (16) QuatTS QuatTSA;
typedef CRY_ALIGN (64) QuatTSd QuatTSrA;

template<typename F>
ILINE bool IsEquivalent(const QuatTS_tpl<F>& qts1, const QuatTS_tpl<F>& qts2, f32 qe = RAD_EPSILON, f32 ve = VEC_EPSILON)
{
	return QuatTS_tpl<F>::IsEquivalent(qts1, qts2, qe, ve);
}


template<class F1, class F2> ILINE QuatTS_tpl<F1> operator*(const QuatTS_tpl<F1>& a, const Quat_tpl<F2>& b)
{
	return QuatTS_tpl<F1>(a.q * b, a.t, a.s);
}

template<class F1, class F2> ILINE QuatTS_tpl<F1> operator*(const QuatTS_tpl<F1>& a, const QuatT_tpl<F2>& b)
{
	return QuatTS_tpl<F1>(a.q * b.q, a.q * (b.t * a.s) + a.t, a.s);
}
template<class F1, class F2> ILINE QuatTS_tpl<F1> operator*(const QuatT_tpl<F1>& a, const QuatTS_tpl<F2>& b)
{
	return QuatTS_tpl<F1>(a.q * b.q, a.q * b.t + a.t, b.s);
}
template<class F1, class F2> ILINE QuatTS_tpl<F1> operator*(const Quat_tpl<F1>& a, const QuatTS_tpl<F2>& b)
{
	return QuatTS_tpl<F1>(a * b.q, a * b.t, b.s);
}

//! Implements the multiplication operator: QuatTS=QuatTS*QuatTS.
template<class F1, class F2> ILINE QuatTS_tpl<F1> operator*(const QuatTS_tpl<F1>& a, const QuatTS_tpl<F2>& b)
{
	CRY_MATH_ASSERT(a.IsValid());
	CRY_MATH_ASSERT(b.IsValid());
	return QuatTS_tpl<F1>(a.q * b.q, a.q * (b.t * a.s) + a.t, a.s * b.s);
}

//! Post-multiply of a QuatT and a Vec3 (3D rotations with quaternions).
template<class F, class F2> ILINE Vec3_tpl<F> operator*(const QuatTS_tpl<F>& q, const Vec3_tpl<F2>& v)
{
	CRY_MATH_ASSERT(q.IsValid());
	CRY_MATH_ASSERT(v.IsValid());
	return q.q * v * q.s + q.t;
}

//! Quaternion with translation vector and non-uniform scale.
template<typename F> struct QuatTNS_tpl
{
	Quat_tpl<F>   q;
	Vec3_tpl<F>   t;
	Diag33_tpl<F> s;

	ILINE QuatTNS_tpl()
	{
	}

	ILINE QuatTNS_tpl(type_identity) { SetIdentity(); }

	ILINE QuatTNS_tpl(const Quat_tpl<F>& quat, const Vec3_tpl<F>& trans, const Diag33_tpl<F>& scale)
		: q(quat)
		, t(trans)
		, s(scale)
	{
	}

	template<typename F1>
	ILINE QuatTNS_tpl(const QuatTS_tpl<F1>& qts)
		: q(qts.q)
		, t(qts.t)
		, s(qts.s)
	{
	}

	ILINE QuatTNS_tpl(const QuatT_tpl<F>& qp)
		: q(qp.q)
		, t(qp.t)
		, s(Diag33_tpl<F>(IDENTITY))
	{
	}

	ILINE void SetIdentity()
	{
		q.SetIdentity();
		t = Vec3_tpl<F>(ZERO);
		s = Diag33_tpl<F>(IDENTITY);
	}

	explicit ILINE QuatTNS_tpl(const Matrix34_tpl<F>& m)
	{
		t = m.GetTranslation();

		// Lengths of base vectors equates to scaling in matrix.
		s.x = m.GetColumn0().GetLength();
		s.y = m.GetColumn1().GetLength();
		s.z = m.GetColumn2().GetLength();

		// Orthonormalize using X and Z as anchors.
		const Vec3_tpl<F>& r0 = m.GetRow(0);
		const Vec3_tpl<F>& r2 = m.GetRow(2);

		const Vec3_tpl<F> v0 = r0.GetNormalized();
		const Vec3_tpl<F> v1 = (r2 % r0).GetNormalized();
		const Vec3_tpl<F> v2 = (v0 % v1);

		Matrix33_tpl<F> m3;
		m3.SetRow(0, v0);
		m3.SetRow(1, v1);
		m3.SetRow(2, v2);

		q = Quat_tpl<F>(m3);
	}

	void Invert()
	{
		s = Diag33_tpl<F>(IDENTITY) / s;
		q = !q;
		t = q * -t * s;
	}
	QuatTNS_tpl<F> GetInverted() const
	{
		QuatTNS_tpl<F> inv;
		inv.s = Diag33_tpl<F>(IDENTITY) / s;
		inv.q = !q;
		inv.t = inv.q * -t * inv.s;
		return inv;
	}

	//! Linear-interpolation between quaternions (nlerp).
	//! Example:
	//!   CQuaternion result,p,q;
	//!   result=qlerp( p, q, 0.5f );
	ILINE void SetNLerp(const QuatTNS_tpl<F>& p, const QuatTNS_tpl<F>& tq, F ti)
	{
		CRY_MATH_ASSERT(p.q.IsValid());
		CRY_MATH_ASSERT(tq.q.IsValid());
		Quat_tpl<F> d = tq.q;
		if ((p.q | d) < 0) { d = -d; }
		Vec3_tpl<F> vDiff = d.v - p.q.v;
		q.v = p.q.v + (vDiff * ti);
		q.w = p.q.w + ((d.w - p.q.w) * ti);
		q.Normalize();

		vDiff = tq.t - p.t;
		t = p.t + (vDiff * ti);

		s = p.s + ((tq.s - p.s) * ti);
	}

	ILINE static QuatTNS_tpl<F> CreateNLerp(const QuatTNS_tpl<F>& p, const QuatTNS_tpl<F>& q, F t)
	{
		QuatTNS_tpl<F> d;
		d.SetNLerp(p, q, t);
		return d;
	}

	ILINE static bool IsEquivalent(const QuatTNS_tpl<F>& qts1, const QuatTNS_tpl<F>& qts2, f32 qe = RAD_EPSILON, f32 ve = VEC_EPSILON)
	{
		return ::IsEquivalent(qts1.q, qts2.q, qe) && ::IsEquivalent(qts1.t, qts2.t, ve) && ::IsEquivalent(qts1.s, qts2.s, ve);
	}

	bool IsValid(f32 e = VEC_EPSILON) const
	{
		if (!q.IsUnit(e)) return false;
		if (!t.IsValid()) return false;
		if (!s.IsValid()) return false;
		return true;
	}

	ILINE Vec3_tpl<F> GetColumn0() const { return q.GetColumn0(); }
	ILINE Vec3_tpl<F> GetColumn1() const { return q.GetColumn1(); }
	ILINE Vec3_tpl<F> GetColumn2() const { return q.GetColumn2(); }
	ILINE Vec3_tpl<F> GetColumn3() const { return t; }
	ILINE Vec3_tpl<F> GetRow0() const    { return q.GetRow0(); }
	ILINE Vec3_tpl<F> GetRow1() const    { return q.GetRow1(); }
	ILINE Vec3_tpl<F> GetRow2() const    { return q.GetRow2(); }

	AUTO_STRUCT_INFO;
};

typedef QuatTNS_tpl<f32> QuatTNS;
typedef QuatTNS_tpl<f64> QuatTNSr;
typedef QuatTNS_tpl<f64> QuatTNS_f64;

// Aligned versions.
typedef CRY_ALIGN (16) QuatTNS QuatTNSA;
typedef CRY_ALIGN (64) QuatTNSr QuatTNSrA;
typedef CRY_ALIGN (64) QuatTNS_f64 QuatTNS_f64A;

template<typename F>
ILINE bool IsEquivalent(const QuatTNS_tpl<F>& qts1, const QuatTNS_tpl<F>& qts2, f32 qe = RAD_EPSILON, f32 ve = VEC_EPSILON)
{
	return QuatTNS_tpl<F>::IsEquivalent(qts1, qts2, qe, ve);
}

template<class F1, class F2> ILINE QuatTNS_tpl<F1> operator*(const QuatTNS_tpl<F1>& a, const Quat_tpl<F2>& b)
{
	return QuatTNS_tpl<F1>(
	  a.q * b,
	  a.t,
	  a.s
	  );
}

template<class F1, class F2> ILINE QuatTNS_tpl<F1> operator*(const Quat_tpl<F1>& a, const QuatTNS_tpl<F2>& b)
{
	return QuatTNS_tpl<F1>(
	  a * b.q,
	  a * b.t,
	  b.s
	  );
}

template<class F1, class F2> ILINE QuatTNS_tpl<F1> operator*(const QuatTNS_tpl<F1>& a, const QuatT_tpl<F2>& b)
{
	return QuatTNS_tpl<F1>(
	  a.q * b.q,
	  a.q * (b.t * a.s) + a.t,
	  a.s
	  );
}

template<class F1, class F2> ILINE QuatTNS_tpl<F1> operator*(const QuatT_tpl<F1>& a, const QuatTNS_tpl<F2>& b)
{
	return QuatTNS_tpl<F1>(
	  a.q * b.q,
	  a.q * b.t + a.t,
	  b.s
	  );
}

//! Implements the multiplication operator: QuatTNS=QuatTNS*QuatTNS.
template<class F1, class F2> ILINE QuatTNS_tpl<F1> operator*(const QuatTNS_tpl<F1>& a, const QuatTNS_tpl<F2>& b)
{
	CRY_MATH_ASSERT(a.IsValid());
	CRY_MATH_ASSERT(b.IsValid());
	return QuatTNS_tpl<F1>(
	  a.q * b.q,
	  a.q * (b.t * a.s) + a.t,
	  a.s * b.s
	  );
}

//! post-multiply of a QuatTNS and a Vec3 (3D rotations with quaternions).
template<class F1, class F2> ILINE Vec3_tpl<F1> operator*(const QuatTNS_tpl<F1>& q, const Vec3_tpl<F2>& v)
{
	CRY_MATH_ASSERT(q.IsValid());
	CRY_MATH_ASSERT(v.IsValid());
	return q.q * Vec3_tpl<F1>(v.x * q.s.x, v.y * q.s.y, v.z * q.s.z) + q.t;
}

//! Dual Quaternion.
template<typename F> struct DualQuat_tpl
{
	Quat_tpl<F> nq;
	Quat_tpl<F> dq;

	ILINE DualQuat_tpl()  {}

	ILINE DualQuat_tpl(const Quat_tpl<F>& q, const Vec3_tpl<F>& t)
	{
		// copy the quaternion part
		nq = q;
		// convert the translation into a dual quaternion part
		dq.w = -0.5f * (t.x * q.v.x + t.y * q.v.y + t.z * q.v.z);
		dq.v.x = 0.5f * (t.x * q.w + t.y * q.v.z - t.z * q.v.y);
		dq.v.y = 0.5f * (-t.x * q.v.z + t.y * q.w + t.z * q.v.x);
		dq.v.z = 0.5f * (t.x * q.v.y - t.y * q.v.x + t.z * q.w);
	}

	ILINE DualQuat_tpl(const QuatT_tpl<F>& qt)
	{
		// copy the quaternion part
		nq = qt.q;
		// convert the translation into a dual quaternion part
		dq.w = -0.5f * (qt.t.x * qt.q.v.x + qt.t.y * qt.q.v.y + qt.t.z * qt.q.v.z);
		dq.v.x = 0.5f * (qt.t.x * qt.q.w - qt.t.z * qt.q.v.y + qt.t.y * qt.q.v.z);
		dq.v.y = 0.5f * (qt.t.y * qt.q.w - qt.t.x * qt.q.v.z + qt.t.z * qt.q.v.x);
		dq.v.z = 0.5f * (qt.t.x * qt.q.v.y - qt.t.y * qt.q.v.x + qt.t.z * qt.q.w);
	}

	ILINE DualQuat_tpl(const Matrix34_tpl<F>& m34)
	{
		// non-dual part (just copy q0):
		nq = Quat_tpl<F>(m34);
		F tx = m34.m03;
		F ty = m34.m13;
		F tz = m34.m23;

		// dual part:
		dq.w = -0.5f * (tx * nq.v.x + ty * nq.v.y + tz * nq.v.z);
		dq.v.x = 0.5f * (tx * nq.w + ty * nq.v.z - tz * nq.v.y);
		dq.v.y = 0.5f * (-tx * nq.v.z + ty * nq.w + tz * nq.v.x);
		dq.v.z = 0.5f * (tx * nq.v.y - ty * nq.v.x + tz * nq.w);
	}

	ILINE DualQuat_tpl(type_identity)
	{
		SetIdentity();
	}

	ILINE DualQuat_tpl(type_zero)
	{
		SetZero();
	}

	template<typename F1> ILINE DualQuat_tpl(const DualQuat_tpl<F1>& QDual) : nq(QDual.nq), dq(QDual.dq) {}

	ILINE void SetIdentity()
	{
		nq.SetIdentity();
		dq.w = 0.0f;
		dq.v.x = 0.0f;
		dq.v.y = 0.0f;
		dq.v.z = 0.0f;
	}

	ILINE void SetZero()
	{
		nq.w = 0.0f;
		nq.v.x = 0.0f;
		nq.v.y = 0.0f;
		nq.v.z = 0.0f;
		dq.w = 0.0f;
		dq.v.x = 0.0f;
		dq.v.y = 0.0f;
		dq.v.z = 0.0f;
	}

	ILINE void Normalize()
	{
		// Normalize both components so that nq is unit
		F norm = isqrt_safe_tpl(nq.v.len2() + sqr(nq.w));
		nq *= norm;
		dq *= norm;
	}

	AUTO_STRUCT_INFO;
};

#ifndef MAX_API_NUM
typedef DualQuat_tpl<f32>  DualQuat;   //!< Always 32 bit.
typedef DualQuat_tpl<f64>  DualQuatd;  //!< Always 64 bit.
typedef DualQuat_tpl<real> DualQuatr;  //!< Variable float precision. depending on the target system it can be between 32, 64 or 80 bit.
#else
typedef DualQuat_tpl<f32>  CryDualQuat;
#endif

template<class F1, class F2>
ILINE DualQuat_tpl<F1> operator*(const DualQuat_tpl<F1>& l, const F2 r)
{
	DualQuat_tpl<F1> dual;
	dual.nq.w = l.nq.w * r;
	dual.nq.v.x = l.nq.v.x * r;
	dual.nq.v.y = l.nq.v.y * r;
	dual.nq.v.z = l.nq.v.z * r;

	dual.dq.w = l.dq.w * r;
	dual.dq.v.x = l.dq.v.x * r;
	dual.dq.v.y = l.dq.v.y * r;
	dual.dq.v.z = l.dq.v.z * r;
	return dual;
}

template<class F1, class F2>
ILINE DualQuat_tpl<F1> operator+(const DualQuat_tpl<F1>& l, const DualQuat_tpl<F2>& r)
{
	DualQuat_tpl<F1> dual;
	dual.nq.w = l.nq.w + r.nq.w;
	dual.nq.v.x = l.nq.v.x + r.nq.v.x;
	dual.nq.v.y = l.nq.v.y + r.nq.v.y;
	dual.nq.v.z = l.nq.v.z + r.nq.v.z;

	dual.dq.w = l.dq.w + r.dq.w;
	dual.dq.v.x = l.dq.v.x + r.dq.v.x;
	dual.dq.v.y = l.dq.v.y + r.dq.v.y;
	dual.dq.v.z = l.dq.v.z + r.dq.v.z;
	return dual;
}

template<class F1, class F2>
ILINE void operator+=(DualQuat_tpl<F1>& l, const DualQuat_tpl<F2>& r)
{
	l.nq.w += r.nq.w;
	l.nq.v.x += r.nq.v.x;
	l.nq.v.y += r.nq.v.y;
	l.nq.v.z += r.nq.v.z;

	l.dq.w += r.dq.w;
	l.dq.v.x += r.dq.v.x;
	l.dq.v.y += r.dq.v.y;
	l.dq.v.z += r.dq.v.z;
}

template<class F1, class F2>
ILINE Vec3_tpl<F1> operator*(const DualQuat_tpl<F1>& dq, const Vec3_tpl<F2>& v)
{
	F2 t;
	const F2 ax = dq.nq.v.y * v.z - dq.nq.v.z * v.y + dq.nq.w * v.x;
	const F2 ay = dq.nq.v.z * v.x - dq.nq.v.x * v.z + dq.nq.w * v.y;
	const F2 az = dq.nq.v.x * v.y - dq.nq.v.y * v.x + dq.nq.w * v.z;
	F2 x = dq.dq.v.x * dq.nq.w - dq.nq.v.x * dq.dq.w + dq.nq.v.y * dq.dq.v.z - dq.nq.v.z * dq.dq.v.y;
	x += x;
	t = (az * dq.nq.v.y - ay * dq.nq.v.z);
	x += t + t + v.x;
	F2 y = dq.dq.v.y * dq.nq.w - dq.nq.v.y * dq.dq.w + dq.nq.v.z * dq.dq.v.x - dq.nq.v.x * dq.dq.v.z;
	y += y;
	t = (ax * dq.nq.v.z - az * dq.nq.v.x);
	y += t + t + v.y;
	F2 z = dq.dq.v.z * dq.nq.w - dq.nq.v.z * dq.dq.w + dq.nq.v.x * dq.dq.v.y - dq.nq.v.y * dq.dq.v.x;
	z += z;
	t = (ay * dq.nq.v.x - ax * dq.nq.v.y);
	z += t + t + v.z;
	return Vec3_tpl<F2>(x, y, z);
}

template<class F1, class F2>
ILINE DualQuat_tpl<F1> operator*(const DualQuat_tpl<F1>& a, const DualQuat_tpl<F2>& b)
{
	DualQuat_tpl<F1> dual;

	dual.nq.v.x = a.nq.v.y * b.nq.v.z - a.nq.v.z * b.nq.v.y + a.nq.w * b.nq.v.x + a.nq.v.x * b.nq.w;
	dual.nq.v.y = a.nq.v.z * b.nq.v.x - a.nq.v.x * b.nq.v.z + a.nq.w * b.nq.v.y + a.nq.v.y * b.nq.w;
	dual.nq.v.z = a.nq.v.x * b.nq.v.y - a.nq.v.y * b.nq.v.x + a.nq.w * b.nq.v.z + a.nq.v.z * b.nq.w;
	dual.nq.w = a.nq.w * b.nq.w - (a.nq.v.x * b.nq.v.x + a.nq.v.y * b.nq.v.y + a.nq.v.z * b.nq.v.z);

	// dual.dq	= a.nq*b.dq + a.dq*b.nq;
	dual.dq.v.x = a.nq.v.y * b.dq.v.z - a.nq.v.z * b.dq.v.y + a.nq.w * b.dq.v.x + a.nq.v.x * b.dq.w;
	dual.dq.v.y = a.nq.v.z * b.dq.v.x - a.nq.v.x * b.dq.v.z + a.nq.w * b.dq.v.y + a.nq.v.y * b.dq.w;
	dual.dq.v.z = a.nq.v.x * b.dq.v.y - a.nq.v.y * b.dq.v.x + a.nq.w * b.dq.v.z + a.nq.v.z * b.dq.w;
	dual.dq.w = a.nq.w * b.dq.w - (a.nq.v.x * b.dq.v.x + a.nq.v.y * b.dq.v.y + a.nq.v.z * b.dq.v.z);

	dual.dq.v.x += a.dq.v.y * b.nq.v.z - a.dq.v.z * b.nq.v.y + a.dq.w * b.nq.v.x + a.dq.v.x * b.nq.w;
	dual.dq.v.y += a.dq.v.z * b.nq.v.x - a.dq.v.x * b.nq.v.z + a.dq.w * b.nq.v.y + a.dq.v.y * b.nq.w;
	dual.dq.v.z += a.dq.v.x * b.nq.v.y - a.dq.v.y * b.nq.v.x + a.dq.w * b.nq.v.z + a.dq.v.z * b.nq.w;
	dual.dq.w += a.dq.w * b.nq.w - (a.dq.v.x * b.nq.v.x + a.dq.v.y * b.nq.v.y + a.dq.v.z * b.nq.v.z);

	return dual;
}

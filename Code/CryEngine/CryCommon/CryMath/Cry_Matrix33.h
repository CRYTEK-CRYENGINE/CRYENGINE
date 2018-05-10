// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//! Represents a 3x3 rotational matrix
//! \see Matrix33
template<typename F> struct Matrix33_tpl
	: INumberArray<F, 9>
{
	typedef INumberArray<F, 9> NA;

	F m00, m01, m02;
	F m10, m11, m12;
	F m20, m21, m22;

	//---------------------------------------------------------------------------------
	ILINE Matrix33_tpl() {}
	ILINE Matrix33_tpl(type_zero) { NA::SetZero(); }

	template<class F1> ILINE Matrix33_tpl(const Matrix33_tpl<F1>& m) { NA::Set(m); }

	//! Set matrix to Identity.
	ILINE Matrix33_tpl(type_identity)
	{
		m00 = 1;
		m01 = 0;
		m02 = 0;
		m10 = 0;
		m11 = 1;
		m12 = 0;
		m20 = 0;
		m21 = 0;
		m22 = 1;
	}

	//! implementation of the constructors

	//! CONSTRUCTOR for identical float-types. It initializes a Matrix33 with 9 floats.
	//! Matrix33(0,1,2, 3,4,5, 6,7,8);
	explicit ILINE Matrix33_tpl<F>(F x00, F x01, F x02, F x10, F x11, F x12, F x20, F x21, F x22)
	{
		m00 = x00;
		m01 = x01;
		m02 = x02;
		m10 = x10;
		m11 = x11;
		m12 = x12;
		m20 = x20;
		m21 = x21;
		m22 = x22;
	}

	//! CONSTRUCTOR for different float-types. It initializes a Matrix33 with 3 vectors stored in the columns and converts between floats/doubles.
	//! Matrix33r(v0,v1,v2);
	template<class F1> explicit ILINE Matrix33_tpl<F>(const Vec3_tpl<F1> &vx, const Vec3_tpl<F1> &vy, const Vec3_tpl<F1> &vz)
	{
		m00 = F(vx.x);
		m01 = F(vy.x);
		m02 = F(vz.x);
		m10 = F(vx.y);
		m11 = F(vy.y);
		m12 = F(vz.y);
		m20 = F(vx.z);
		m21 = F(vy.z);
		m22 = F(vz.z);
	}

	//! CONSTRUCTOR for different float-types. It converts a Diag33 into a Matrix33 and also converts between double/float.
	//! Matrix33(diag33);
	template<class F1> ILINE Matrix33_tpl<F>(const Diag33_tpl<F1> &d)
	{
		CRY_MATH_ASSERT(d.IsValid());
		m00 = F(d.x);
		m01 = 0;
		m02 = 0;
		m10 = 0;
		m11 = F(d.y);
		m12 = 0;
		m20 = 0;
		m21 = 0;
		m22 = F(d.z);
	}

	//! CONSTRUCTOR for different float-types. It converts a Matrix34 into a Matrix33 and converts between double/float.
	//! Needs to be 'explicit' because we loose the translation vector in the conversion process.
	//! Matrix33(m34r);
	template<class F1> explicit ILINE Matrix33_tpl<F>(const Matrix34_tpl<F1> &m)
	{
		CRY_MATH_ASSERT(m.IsValid());
		m00 = F(m.m00);
		m01 = F(m.m01);
		m02 = F(m.m02);
		m10 = F(m.m10);
		m11 = F(m.m11);
		m12 = F(m.m12);
		m20 = F(m.m20);
		m21 = F(m.m21);
		m22 = F(m.m22);
	}

	//! CONSTRUCTOR for different float-types. It converts a Matrix44 into a Matrix33 and converts between double/float.
	//! Needs to be 'explicit' because we loose the translation vector and the 3rd row in the conversion process.
	//! Matrix33(m44r);
	template<class F1> explicit ILINE Matrix33_tpl<F>(const Matrix44_tpl<F1> &m)
	{
		CRY_MATH_ASSERT(m.IsValid());
		m00 = F(m.m00);
		m01 = F(m.m01);
		m02 = F(m.m02);
		m10 = F(m.m10);
		m11 = F(m.m11);
		m12 = F(m.m12);
		m20 = F(m.m20);
		m21 = F(m.m21);
		m22 = F(m.m22);
	}

	//! CONSTRUCTOR for different float-types. It converts a Quat into a Matrix33 and converts between double/float.
	//! Needs to be 'explicit' because we loose fp-precision in the conversion process.
	//! Matrix33(quatr);
	template<class F1> explicit ILINE Matrix33_tpl<F>(const Quat_tpl<F1> &q)
	{
		CRY_MATH_ASSERT(q.IsUnit(0.05f));
		Vec3_tpl<F1> v2 = q.v + q.v;
		F1 xx = 1 - v2.x * q.v.x;
		F1 yy = v2.y * q.v.y;
		F1 xw = v2.x * q.w;
		F1 xy = v2.y * q.v.x;
		F1 yz = v2.z * q.v.y;
		F1 yw = v2.y * q.w;
		F1 xz = v2.z * q.v.x;
		F1 zz = v2.z * q.v.z;
		F1 zw = v2.z * q.w;
		m00 = F(1 - yy - zz);
		m01 = F(xy - zw);
		m02 = F(xz + yw);
		m10 = F(xy + zw);
		m11 = F(xx - zz);
		m12 = F(yz - xw);
		m20 = F(xz - yw);
		m21 = F(yz + xw);
		m22 = F(xx - yy);
	}

	//! CONSTRUCTOR for different float-types. It converts a Euler Angle into a Matrix33 and converts between double/float.
	//! Needs to be 'explicit' because we loose fp-precision in the conversion process.
	//! Matrix33(Ang3r(1,2,3));
	template<class F1> explicit ILINE Matrix33_tpl<F>(const Ang3_tpl<F1> &ang)
	{
		CRY_MATH_ASSERT(ang.IsValid());
		SetRotationXYZ(Ang3_tpl<F>(F(ang.x), F(ang.y), F(ang.z)));
	}

	//! Bracket OPERATOR: initializes a Matrix33 with 9 floats.
	//! Matrix33 m; m(0,1,2, 3,4,5, 6,7,8);
	ILINE void operator()(F x00, F x01, F x02, F x10, F x11, F x12, F x20, F x21, F x22)
	{
		m00 = x00;
		m01 = x01;
		m02 = x02;
		m10 = x10;
		m11 = x11;
		m12 = x12;
		m20 = x20;
		m21 = x21;
		m22 = x22;
		CRY_MATH_ASSERT(NA::IsValid());
	}

	//---------------------------------------------------------------------------------------

	ILINE void SetIdentity(void)
	{
		m00 = 1;
		m01 = 0;
		m02 = 0;
		m10 = 0;
		m11 = 1;
		m12 = 0;
		m20 = 0;
		m21 = 0;
		m22 = 1;
	}

	ILINE static Matrix33_tpl<F> CreateIdentity()
	{
		Matrix33_tpl<F> m33;
		m33.SetIdentity();
		return m33;
	}

	//! Create a rotation matrix around an arbitrary axis (Eulers Theorem).
	//! The axis is specified as a normalized Vec3. The angle is assumed to be in radians.
	//! Example:
	//!	Matrix34 m34;
	//!	Vec3 axis=GetNormalized( Vec3(-1.0f,-0.3f,0.0f) );
	//!	m34.SetRotationAA( rad, axis );
	ILINE void SetRotationAA(F angle, const Vec3_tpl<F> axis)
	{
		F s, c;
		sincos_tpl(angle, &s, &c);
		SetRotationAA(c, s, axis);
	}
	ILINE static Matrix33_tpl<F> CreateRotationAA(const F rad, const Vec3_tpl<F>& axis)
	{
		Matrix33_tpl<F> m33;
		m33.SetRotationAA(rad, axis);
		return m33;
	}

	ILINE void SetRotationAA(F c, F s, const Vec3_tpl<F>& axis)
	{
		CRY_MATH_ASSERT(axis.IsUnit(0.001f));
		F mc = 1 - c;
		m00 = mc * axis.x * axis.x + c;
		m01 = mc * axis.x * axis.y - axis.z * s;
		m02 = mc * axis.x * axis.z + axis.y * s;
		m10 = mc * axis.y * axis.x + axis.z * s;
		m11 = mc * axis.y * axis.y + c;
		m12 = mc * axis.y * axis.z - axis.x * s;
		m20 = mc * axis.z * axis.x - axis.y * s;
		m21 = mc * axis.z * axis.y + axis.x * s;
		m22 = mc * axis.z * axis.z + c;
	}
	ILINE static Matrix33_tpl<F> CreateRotationAA(F c, F s, const Vec3_tpl<F>& axis)
	{
		Matrix33_tpl<F> m33;
		m33.SetRotationAA(c, s, axis);
		return m33;
	}

	ILINE void SetRotationAA(const Vec3_tpl<F> axis)
	{
		F angle = axis.GetLength();
		if (angle == F(0))
			SetIdentity();
		else
			SetRotationAA(angle, axis / angle);
	}
	ILINE static Matrix33_tpl<F> CreateRotationAA(const Vec3_tpl<F>& axis)
	{
		Matrix33_tpl<F> m33;
		m33.SetRotationAA(axis);
		return m33;
	}

	//! Create rotation-matrix about X axis using an angle.
	//! The angle is assumed to be in radians.
	//! Example:
	//! Matrix m33; m33.SetRotationX(0.5f);
	ILINE void SetRotationX(const F rad)
	{
		F s, c;
		sincos_tpl(rad, &s, &c);
		m00 = 1.0f;
		m01 = 0.0f;
		m02 = 0.0f;
		m10 = 0.0f;
		m11 = c;
		m12 = -s;
		m20 = 0.0f;
		m21 = s;
		m22 = c;
	}
	ILINE static Matrix33_tpl<F> CreateRotationX(const F rad)
	{
		Matrix33_tpl<F> m33;
		m33.SetRotationX(rad);
		return m33;
	}

	ILINE void SetRotationY(const F rad)
	{
		F s, c;
		sincos_tpl(rad, &s, &c);
		m00 = c;
		m01 = 0;
		m02 = s;
		m10 = 0;
		m11 = 1;
		m12 = 0;
		m20 = -s;
		m21 = 0;
		m22 = c;
	}
	ILINE static Matrix33_tpl<F> CreateRotationY(const F rad)
	{
		Matrix33_tpl<F> m33;
		m33.SetRotationY(rad);
		return m33;
	}

	ILINE void SetRotationZ(const F rad)
	{
		F s, c;
		sincos_tpl(rad, &s, &c);
		m00 = c;
		m01 = -s;
		m02 = 0.0f;
		m10 = s;
		m11 = c;
		m12 = 0.0f;
		m20 = 0.0f;
		m21 = 0.0f;
		m22 = 1.0f;
	}
	ILINE static Matrix33_tpl<F> CreateRotationZ(const F rad)
	{
		Matrix33_tpl<F> m33;
		m33.SetRotationZ(rad);
		return m33;
	}

	ILINE void SetRotationXYZ(const Ang3_tpl<F>& rad)
	{
		CRY_MATH_ASSERT(rad.IsValid());
		F sx, cx;
		sincos_tpl(rad.x, &sx, &cx);
		F sy, cy;
		sincos_tpl(rad.y, &sy, &cy);
		F sz, cz;
		sincos_tpl(rad.z, &sz, &cz);
		F sycz = (sy * cz), sysz = (sy * sz);
		m00 = cy * cz;
		m01 = sycz * sx - cx * sz;
		m02 = sycz * cx + sx * sz;
		m10 = cy * sz;
		m11 = sysz * sx + cx * cz;
		m12 = sysz * cx - sx * cz;
		m20 = -sy;
		m21 = cy * sx;
		m22 = cy * cx;
	}
	ILINE static Matrix33_tpl<F> CreateRotationXYZ(const Ang3_tpl<F>& rad)
	{
		CRY_MATH_ASSERT(rad.IsValid());
		Matrix33_tpl<F> m33;
		m33.SetRotationXYZ(rad);
		return m33;
	}

	//! Creates a rotation matrix that rotates the vector "v0" into "v1".
	//!	If both vectors are exactly parallel it returns an identity-matrix.
	//!	CAUTION: If both vectors are exactly diametrical it returns a matrix that rotates pi-radians about a "random" axis that is orthogonal to v0.
	//!	CAUTION: If both vectors are almost diametrical we have to normalize a very small vector and the result is inaccurate.
	//! It is recommended to use this function with 64-bit precision.
	ILINE void SetRotationV0V1(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1)
	{
		CRY_MATH_ASSERT((fabs_tpl(1 - (v0 | v0))) < 0.01); //check if unit-vector
		CRY_MATH_ASSERT((fabs_tpl(1 - (v1 | v1))) < 0.01); //check if unit-vector
		F dot = v0 | v1;
		if (dot < F(-0.9999f))
		{
			Vec3d axis = v0.GetOrthogonal().GetNormalized();
			m00 = F(2 * axis.x * axis.x - 1);
			m01 = F(2 * axis.x * axis.y);
			m02 = F(2 * axis.x * axis.z);
			m10 = F(2 * axis.y * axis.x);
			m11 = F(2 * axis.y * axis.y - 1);
			m12 = F(2 * axis.y * axis.z);
			m20 = F(2 * axis.z * axis.x);
			m21 = F(2 * axis.z * axis.y);
			m22 = F(2 * axis.z * axis.z - 1);
		}
		else
		{
			Vec3_tpl<F> v = v0 % v1;
			F h = 1 / (1 + dot);
			m00 = F(dot + h * v.x * v.x);
			m01 = F(h * v.x * v.y - v.z);
			m02 = F(h * v.x * v.z + v.y);
			m10 = F(h * v.x * v.y + v.z);
			m11 = F(dot + h * v.y * v.y);
			m12 = F(h * v.y * v.z - v.x);
			m20 = F(h * v.x * v.z - v.y);
			m21 = F(h * v.y * v.z + v.x);
			m22 = F(dot + h * v.z * v.z);
		}
	}
	ILINE static Matrix33_tpl<F> CreateRotationV0V1(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1)
	{
		Matrix33_tpl<F> m33;
		m33.SetRotationV0V1(v0, v1);
		return m33;
	}

	//! Given a view-direction and a radiant to rotate about Y-axis, this function builds a 3x3 look-at matrix using only simple vector arithmetic.
	//! This function is always using the implicit up-vector Vec3(0,0,1). The view-direction is always stored in column(1).
	//! IMPORTANT: The view-vector is assumed to be normalized, because all trig-values for the orientation are being
	//! extracted  directly out of the vector. This function must NOT be called with a view-direction
	//! that is close to Vec3(0,0,1) or Vec3(0,0,-1). If one of these rules is broken, the function returns a matrix
	//! with an undefined rotation about the Z-axis.
	//!
	//! Rotation order for the look-at-matrix is Z-X-Y. (Zaxis=YAW / Xaxis=PITCH / Yaxis=ROLL)
	//! Example: Matrix33 orientation=Matrix33::CreateRotationVDir( Vec3(0,1,0), 0 );
	//!
	//!  COORDINATE-SYSTEM
	//!
	//!  z-axis
	//!    ^
	//!    |
	//!    |  y-axis
	//!    |  /
	//!    | /
	//!    |/
	//!    +--------------->   x-axis
	//!
	//! \param vdir  normalized view direction.
	//! \param roll  radiant to rotate about Y-axis.
	ILINE void SetRotationVDir(const Vec3_tpl<F>& vdir)
	{
		CRY_MATH_ASSERT((fabs_tpl(1 - (vdir | vdir))) < 0.01);    //check if unit-vector
		//set default initialisation for up-vector
		m00 = 1;
		m01 = 0;
		m02 = 0;
		m10 = 0;
		m11 = 0;
		m12 = -vdir.z;
		m20 = 0;
		m21 = vdir.z;
		m22 = 0;
		//calculate look-at matrix
		F l = sqrt(vdir.x * vdir.x + vdir.y * vdir.y);
		if (l > F(0.00001))
		{
			F xl = -vdir.x / l;
			F yl = vdir.y / l;
			m00 = F(yl);
			m01 = F(vdir.x);
			m02 = F(xl * vdir.z);
			m10 = F(xl);
			m11 = F(vdir.y);
			m12 = F(-vdir.z * yl);
			m20 = 0;
			m21 = F(vdir.z);
			m22 = F(l);
		}
	}
	ILINE static Matrix33_tpl<F> CreateRotationVDir(const Vec3_tpl<F> vdir)
	{
		Matrix33_tpl<F> m33;
		m33.SetRotationVDir(vdir);
		return m33;
	}

	//! Look-at matrix with roll.
	ILINE void SetRotationVDir(const Vec3_tpl<F>& vdir, F roll)
	{
		SetRotationVDir(vdir);
		F s, c;
		sincos_tpl(roll, &s, &c);
		F x00 = m00, x10 = m10;
		m00 = m00 * c - m02 * s;
		m02 = x00 * s + m02 * c;
		m10 = m10 * c - m12 * s;
		m12 = x10 * s + m12 * c;
		m20 = -m22 * s;
		m22 = m22 * c;
	}
	ILINE static Matrix33_tpl<F> CreateRotationVDir(const Vec3_tpl<F>& vdir, F roll)
	{
		Matrix33_tpl<F> m33;
		m33.SetRotationVDir(vdir, roll);
		return m33;
	}

	//! Calculate 2 vector that form a orthogonal base with a given input vector (by M.M.).
	//! \param invDirection input direction (has to be normalized)
	//! \param outvA first output vector that is perpendicular to the input direction
	//! \param outvB second output vector that is perpendicular the input vector and the first output vector
	ILINE static Matrix33_tpl<F> CreateOrthogonalBase(const Vec3& invDirection)
	{
		Vec3 outvA;
		if (invDirection.z < -0.5f || invDirection.z > 0.5f) outvA = Vec3(invDirection.z, invDirection.y, -invDirection.x);
		else outvA = Vec3(invDirection.y, -invDirection.x, invDirection.z);
		Vec3 outvB = (invDirection % outvA).GetNormalized();
		outvA = (invDirection % outvB).GetNormalized();
		return CreateFromVectors(invDirection, outvA, outvB);
	}

	//////////////////////////////////////////////////////////////////////////
	ILINE static Matrix33_tpl<F> CreateOrientation(const Vec3_tpl<F>& dir, const Vec3_tpl<F>& up, float rollAngle)
	{
		// LookAt transform.
		Vec3 xAxis, yAxis, zAxis;
		Vec3 upVector = up;
		if (dir.IsZeroFast())
		{
			Matrix33_tpl<F> tm;
			tm.SetIdentity();
			return tm;
		}
		yAxis = dir.GetNormalized();

		if (yAxis.x == 0 && yAxis.y == 0 && up.IsEquivalent(Vec3_tpl<F>(0, 0, 1.0f)))
			upVector.Set(-yAxis.z, 0, 0);

		xAxis = (upVector % yAxis).GetNormalized();
		zAxis = (xAxis % yAxis).GetNormalized();

		Matrix33_tpl<F> tm;
		tm.SetFromVectors(xAxis, yAxis, zAxis);

		if (rollAngle != 0)
		{
			Matrix33_tpl<F> RollMtx;
			RollMtx.SetRotationY(rollAngle);
			tm = tm * RollMtx;
		}
		return tm;
	}

	//! Direct-Matrix-Slerp: for the sake of completeness, I have included the following expression for Spherical-Linear-Interpolation without using quaternions.
	//! This is much faster then converting both matrices into quaternions in order to do a quaternion slerp and then converting the slerped
	//! quaternion back into a matrix.
	//! This is a high-precision calculation. Given two orthonormal 3x3 matrices this function calculates
	//! the shortest possible interpolation-path between the two rotations. The interpolation curve forms
	//! a great arc on the rotation sphere (geodesic). Not only does Slerp follow a great arc it follows
	//! the shortest great arc.	Furthermore Slerp has constant angular velocity. All in all Slerp is the
	//! optimal interpolation curve between two rotations.
	//!
	//! STABILITY PROBLEM: There are two singularities at angle=0 and angle=PI. At 0 the interpolation-axis
	//! is arbitrary, which means any axis will produce the same result because we have no rotation. Thats
	//! why I'm using (1,0,0). At PI the rotations point away from each other and the interpolation-axis
	//! is unpredictable. In this case I'm also using the axis (1,0,0). If the angle is ~0 or ~PI, then we
	//! have to normalize a very small vector and this can cause numerical instability. The quaternion-slerp
	//! has exactly the same problems.
	//! Example: Matrix33 slerp=Matrix33::CreateSlerp( m,n,0.333f );
	ILINE void SetSlerp(const Matrix33_tpl<F>& m, const Matrix33_tpl<F>& n, F t)
	{
		CRY_MATH_ASSERT(m.IsValid());
		CRY_MATH_ASSERT(n.IsValid());
		//calculate delta-rotation between m and n (=39 flops)
		Matrix33_tpl<F> d, i;
		d.m00 = m.m00 * n.m00 + m.m10 * n.m10 + m.m20 * n.m20;
		d.m01 = m.m00 * n.m01 + m.m10 * n.m11 + m.m20 * n.m21;
		d.m02 = m.m00 * n.m02 + m.m10 * n.m12 + m.m20 * n.m22;
		d.m10 = m.m01 * n.m00 + m.m11 * n.m10 + m.m21 * n.m20;
		d.m11 = m.m01 * n.m01 + m.m11 * n.m11 + m.m21 * n.m21;
		d.m12 = m.m01 * n.m02 + m.m11 * n.m12 + m.m21 * n.m22;
		d.m20 = d.m01 * d.m12 - d.m02 * d.m11;
		d.m21 = d.m02 * d.m10 - d.m00 * d.m12;
		d.m22 = d.m00 * d.m11 - d.m01 * d.m10;
		CRY_MATH_ASSERT(d.IsOrthonormalRH(0.0001f));

		Vec3_tpl<F> axis(d.m21 - d.m12, d.m02 - d.m20, d.m10 - d.m01);
		F l = sqrt(axis | axis);
		if (l > F(0.00001)) axis /= l; else axis(1, 0, 0);
		i.SetRotationAA(acos(clamp_tpl((d.m00 + d.m11 + d.m22 - 1) * 0.5f, F(-1), F(+1))) * t, axis); //angle interpolation and calculation of new delta-matrix (=26 flops)

		//final concatenation (=39 flops)
		m00 = F(m.m00 * i.m00 + m.m01 * i.m10 + m.m02 * i.m20);
		m01 = F(m.m00 * i.m01 + m.m01 * i.m11 + m.m02 * i.m21);
		m02 = F(m.m00 * i.m02 + m.m01 * i.m12 + m.m02 * i.m22);
		m10 = F(m.m10 * i.m00 + m.m11 * i.m10 + m.m12 * i.m20);
		m11 = F(m.m10 * i.m01 + m.m11 * i.m11 + m.m12 * i.m21);
		m12 = F(m.m10 * i.m02 + m.m11 * i.m12 + m.m12 * i.m22);
		m20 = m01 * m12 - m02 * m11;
		m21 = m02 * m10 - m00 * m12;
		m22 = m00 * m11 - m01 * m10;
		CRY_MATH_ASSERT(IsOrthonormalRH(0.0001f));
	}
	ILINE static Matrix33_tpl<F> CreateSlerp(const Matrix33_tpl<F> m, const Matrix33_tpl<F> n, F t)
	{
		Matrix33_tpl<F> m33;
		m33.SetSlerp(m, n, t);
		return m33;
	}

	ILINE void SetScale(const Vec3_tpl<F>& s)
	{
		m00 = s.x;
		m01 = 0;
		m02 = 0;
		m10 = 0;
		m11 = s.y;
		m12 = 0;
		m20 = 0;
		m21 = 0;
		m22 = s.z;
	}
	ILINE static Matrix33_tpl<F> CreateScale(const Vec3_tpl<F>& s) { Matrix33_tpl<F> m; m.SetScale(s); return m; }

	//! \note All vectors are stored in columns.
	ILINE void SetFromVectors(const Vec3_tpl<F>& vx, const Vec3_tpl<F>& vy, const Vec3_tpl<F>& vz)
	{
		m00 = vx.x;
		m01 = vy.x;
		m02 = vz.x;
		m10 = vx.y;
		m11 = vy.y;
		m12 = vz.y;
		m20 = vx.z;
		m21 = vy.z;
		m22 = vz.z;
	}
	ILINE static Matrix33_tpl<F> CreateFromVectors(const Vec3_tpl<F>& vx, const Vec3_tpl<F>& vy, const Vec3_tpl<F>& vz) { Matrix33_tpl<F> dst; dst.SetFromVectors(vx, vy, vz); return dst;  }

	//! In-place transposition.
	ILINE void Transpose()
	{
		F t;
		t = m01;
		m01 = m10;
		m10 = t;
		t = m02;
		m02 = m20;
		m20 = t;
		t = m12;
		m12 = m21;
		m21 = t;
	}
	ILINE Matrix33_tpl<F> GetTransposed() const
	{
		Matrix33_tpl<F> dst;
		dst.m00 = m00;
		dst.m01 = m10;
		dst.m02 = m20;
		dst.m10 = m01;
		dst.m11 = m11;
		dst.m12 = m21;
		dst.m20 = m02;
		dst.m21 = m12;
		dst.m22 = m22;
		return dst;
	}
	ILINE Matrix33_tpl<F> T() const { return GetTransposed(); }

	ILINE Matrix33_tpl&   Fabs()
	{
		m00 = fabs_tpl(m00);
		m01 = fabs_tpl(m01);
		m02 = fabs_tpl(m02);
		m10 = fabs_tpl(m10);
		m11 = fabs_tpl(m11);
		m12 = fabs_tpl(m12);
		m20 = fabs_tpl(m20);
		m21 = fabs_tpl(m21);
		m22 = fabs_tpl(m22);
		return *this;
	}
	ILINE Matrix33_tpl<F> GetFabs() const { Matrix33_tpl<F> m = *this; m.Fabs();  return m; }

	ILINE void            Adjoint(void)
	{
		//rescue members
		Matrix33_tpl<F> m = *this;
		//calculate the adjoint-matrix
		m00 = m.m11 * m.m22 - m.m12 * m.m21;
		m01 = m.m12 * m.m20 - m.m10 * m.m22;
		m02 = m.m10 * m.m21 - m.m11 * m.m20;
		m10 = m.m21 * m.m02 - m.m22 * m.m01;
		m11 = m.m22 * m.m00 - m.m20 * m.m02;
		m12 = m.m20 * m.m01 - m.m21 * m.m00;
		m20 = m.m01 * m.m12 - m.m02 * m.m11;
		m21 = m.m02 * m.m10 - m.m00 * m.m12;
		m22 = m.m00 * m.m11 - m.m01 * m.m10;
	}
	ILINE Matrix33_tpl<F> GetAdjoint() const { Matrix33_tpl<F> dst = *this; dst.Adjoint(); return dst; }

	//! Calculate a real inversion of a Matrix33.
	//! An inverse-matrix is an UnDo-matrix for all kind of transformations.
	//! \note If the return value of Invert33() is zero, then the inversion failed!
	//! Example 1:
	//!   Matrix33 im33;
	//!   bool st=i33.Invert();
	//!   assert(st);
	//! Example 2: matrix33 im33=Matrix33::GetInverted(m33);
	ILINE bool Invert(void)
	{
		//rescue members
		Matrix33_tpl<F> m = *this;
		//calculate the cofactor-matrix (=transposed adjoint-matrix)
		m00 = m.m22 * m.m11 - m.m12 * m.m21;
		m01 = m.m02 * m.m21 - m.m22 * m.m01;
		m02 = m.m12 * m.m01 - m.m02 * m.m11;
		m10 = m.m12 * m.m20 - m.m22 * m.m10;
		m11 = m.m22 * m.m00 - m.m02 * m.m20;
		m12 = m.m02 * m.m10 - m.m12 * m.m00;
		m20 = m.m10 * m.m21 - m.m20 * m.m11;
		m21 = m.m20 * m.m01 - m.m00 * m.m21;
		m22 = m.m00 * m.m11 - m.m10 * m.m01;
		// calculate determinant
		F det = (m.m00 * m00 + m.m10 * m01 + m.m20 * m02);
		if (fabs_tpl(det) < 1E-20f)
			return 0;
		//divide the cofactor-matrix by the determinant
		F idet = (F)1.0 / det;
		m00 *= idet;
		m01 *= idet;
		m02 *= idet;
		m10 *= idet;
		m11 *= idet;
		m12 *= idet;
		m20 *= idet;
		m21 *= idet;
		m22 *= idet;
		return 1;
	}
	ILINE Matrix33_tpl<F> GetInverted() const
	{
		Matrix33_tpl<F> dst = *this;
		dst.Invert();
		return dst;
	}

	ILINE Vec3_tpl<F> TransformVector(const Vec3_tpl<F>& v) const
	{
		return Vec3_tpl<F>(m00 * v.x + m01 * v.y + m02 * v.z, m10 * v.x + m11 * v.y + m12 * v.z, m20 * v.x + m21 * v.y + m22 * v.z);
	}

	//! make a right-handed orthonormal matrix.
	ILINE void OrthonormalizeFast()
	{
		Vec3 x = Vec3(m00, m10, m20).GetNormalized();
		Vec3 y = (Vec3(m02, m12, m22) % x).GetNormalized();
		Vec3 z = (x % y);
		m00 = x.x;
		m01 = y.x;
		m02 = z.x;
		m10 = x.y;
		m11 = y.y;
		m12 = z.y;
		m20 = x.z;
		m21 = y.z;
		m22 = z.z;
	}

	//! make a left-handed orthonormal matrix.
	ILINE void orthonormalizeFastLH()
	{
		Vec3 x = Vec3(m00, m10, m20).GetNormalized();
		Vec3 y = (x % Vec3(m02, m12, m22)).GetNormalized();
		Vec3 z = (y % x);
		m00 = x.x;
		m01 = y.x;
		m02 = z.x;
		m10 = x.y;
		m11 = y.y;
		m12 = z.y;
		m20 = x.z;
		m21 = y.z;
		m22 = z.z;
	}

	ILINE F Determinant() const
	{
		return (m00 * m11 * m22) + (m01 * m12 * m20) + (m02 * m10 * m21) - (m02 * m11 * m20) - (m00 * m12 * m21) - (m01 * m10 * m22);
	}

	// helper functions to access matrix-members
	F*                     GetData()                              { return &m00; }
	const F*               GetData() const                        { return &m00; }

	ILINE F                operator()(uint32 i, uint32 j) const   { CRY_MATH_ASSERT((i < 3) && (j < 3)); const F* const p_data = (const F*)(&m00);   return p_data[i * 3 + j]; }
	ILINE F&               operator()(uint32 i, uint32 j)         { CRY_MATH_ASSERT((i < 3) && (j < 3)); F* p_data = (F*)(&m00);   return p_data[i * 3 + j]; }
	ILINE void             SetRow(int i, const Vec3_tpl<F>& v)    { CRY_MATH_ASSERT(i < 3); F* p = (F*)(&m00);  p[0 + 3 * i] = v.x; p[1 + 3 * i] = v.y; p[2 + 3 * i] = v.z;   }
	ILINE Vec3_tpl<F>      GetRow(int i) const                    { CRY_MATH_ASSERT(i < 3); const F* const p = (const F*)(&m00);  return Vec3_tpl<F>(p[0 + 3 * i], p[1 + 3 * i], p[2 + 3 * i]); }
	ILINE void             SetColumn(int i, const Vec3_tpl<F>& v) { CRY_MATH_ASSERT(i < 3); F* p = (F*)(&m00);  p[i + 3 * 0] = v.x; p[i + 3 * 1] = v.y; p[i + 3 * 2] = v.z;   }
	ILINE Vec3_tpl<F>      GetColumn(int i) const                 { CRY_MATH_ASSERT(i < 3); const F* const p = (const F*)(&m00);  return Vec3_tpl<F>(p[i + 3 * 0], p[i + 3 * 1], p[i + 3 * 2]); }

	ILINE Vec3_tpl<F>      GetColumn0() const                     { return Vec3_tpl<F>(m00, m10, m20); }
	ILINE Vec3_tpl<F>      GetColumn1() const                     { return Vec3_tpl<F>(m01, m11, m21); }
	ILINE Vec3_tpl<F>      GetColumn2() const                     { return Vec3_tpl<F>(m02, m12, m22); }
	ILINE void             SetColumn0(const Vec3_tpl<F>& v)       { m00 = v.x; m10 = v.y; m20 = v.z;  }
	ILINE void             SetColumn1(const Vec3_tpl<F>& v)       { m01 = v.x; m11 = v.y; m21 = v.z;  }
	ILINE void             SetColumn2(const Vec3_tpl<F>& v)       { m02 = v.x; m12 = v.y; m22 = v.z;  }

	ILINE Matrix33_tpl<F>& operator*=(F op)
	{
		m00 *= op;
		m01 *= op;
		m02 *= op;
		m10 *= op;
		m11 *= op;
		m12 *= op;
		m20 *= op;
		m21 *= op;
		m22 *= op;
		return *this;
	}

	ILINE Matrix33_tpl<F>& operator/=(F op)
	{
		F iop = (F)1.0 / op;
		m00 *= iop;
		m01 *= iop;
		m02 *= iop;
		m10 *= iop;
		m11 *= iop;
		m12 *= iop;
		m20 *= iop;
		m21 *= iop;
		m22 *= iop;
		return *this;
	}

	ILINE bool IsIdentity(F e) const
	{
		return  (
		  (fabs_tpl((F)1 - m00) <= e) && (fabs_tpl(m01) <= e) && (fabs_tpl(m02) <= e) &&
		  (fabs_tpl(m10) <= e) && (fabs_tpl((F)1 - m11) <= e) && (fabs_tpl(m12) <= e) &&
		  (fabs_tpl(m20) <= e) && (fabs_tpl(m21) <= e) && (fabs_tpl((F)1 - m22) <= e)
		  );
	}

	ILINE bool IsIdentity() const
	{
		return 0 == (fabs_tpl((F)1 - m00) + fabs_tpl(m01) + fabs_tpl(m02) + fabs_tpl(m10) + fabs_tpl((F)1 - m11) + fabs_tpl(m12) + fabs_tpl(m20) + fabs_tpl(m21) + fabs_tpl((F)1 - m22));
	}

	//! Check if we have an orthonormal-base (general case, works even with reflection matrices).
	ILINE int IsOrthonormal(F threshold = 0.001) const
	{
		F d0 = fabs_tpl(GetColumn0() | GetColumn1());
		if (d0 > threshold) return 0;
		F d1 = fabs_tpl(GetColumn0() | GetColumn2());
		if (d1 > threshold) return 0;
		F d2 = fabs_tpl(GetColumn1() | GetColumn2());
		if (d2 > threshold) return 0;
		int a = (fabs_tpl(1 - (GetColumn0() | GetColumn0()))) < threshold;
		int b = (fabs_tpl(1 - (GetColumn1() | GetColumn1()))) < threshold;
		int c = (fabs_tpl(1 - (GetColumn2() | GetColumn2()))) < threshold;
		return a & b & c;
	}

	//! Check if we have an orthonormal-base (assuming we are using a right-handed coordinate system).
	ILINE int IsOrthonormalRH(F threshold = 0.002) const
	{
		Vec3_tpl<F> x = GetColumn0();
		Vec3_tpl<F> y = GetColumn1();
		Vec3_tpl<F> z = GetColumn2();
		bool a = x.IsEquivalent(y % z, threshold);
		bool b = y.IsEquivalent(z % x, threshold);
		bool c = z.IsEquivalent(x % y, threshold);
		a &= x.IsUnit(0.01f);
		b &= y.IsUnit(0.01f);
		c &= z.IsUnit(0.01f);
		return a & b & c;
	}

	//! Remove uniform scale from matrix.
	ILINE void NoScale()
	{
		*this /= GetColumn(0).GetLength();
	}

	AUTO_STRUCT_INFO;
};

// Typedefs

typedef Matrix33_tpl<f32>  Matrix33;  //!< Always 32 bit.
typedef Matrix33_tpl<f64>  Matrix33d; //!< Always 64 bit.
typedef Matrix33_tpl<real> Matrix33r; //!< Variable float precision. depending on the target system it can be between 32, 64 or 80 bit.
#if CRY_COMPILER_GCC
/* GCC has a bug where TYPE_USER_ALIGN is ignored in certain situations with template layouts
 * the user speficied alignment is not correctly stored with the new type unless it's layed before.
 * This bug has been fixed for gcc5: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65734
 * Workaroud by forcing the template which needs to be aligned to be fully instantiated: (deprecated so it's not used by accident)
 */
struct __attribute__((deprecated)) Matrix33_f32_dont_use : public Matrix33_tpl<f32> {};
#endif
typedef CRY_ALIGN (16) Matrix33_tpl<f32> Matrix33A;

//! Implementation of Matrix33.
template<class F1, class F2>
ILINE Matrix33_tpl<F1> operator*(const Matrix33_tpl<F1>& l, const Diag33_tpl<F2>& r)
{
	CRY_MATH_ASSERT(l.IsValid());
	CRY_MATH_ASSERT(r.IsValid());
	Matrix33_tpl<F1> res;
	res.m00 = l.m00 * r.x;
	res.m01 = l.m01 * r.y;
	res.m02 = l.m02 * r.z;
	res.m10 = l.m10 * r.x;
	res.m11 = l.m11 * r.y;
	res.m12 = l.m12 * r.z;
	res.m20 = l.m20 * r.x;
	res.m21 = l.m21 * r.y;
	res.m22 = l.m22 * r.z;
	return res;
}
template<class F1, class F2>
ILINE Matrix33_tpl<F1>& operator*=(Matrix33_tpl<F1>& l, const Diag33_tpl<F2>& r)
{
	CRY_MATH_ASSERT(l.IsValid());
	CRY_MATH_ASSERT(r.IsValid());
	l.m00 *= r.x;
	l.m01 *= r.y;
	l.m02 *= r.z;
	l.m10 *= r.x;
	l.m11 *= r.y;
	l.m12 *= r.z;
	l.m20 *= r.x;
	l.m21 *= r.y;
	l.m22 *= r.z;
	return l;
}

//! Matrix33 operations with another Matrix33.
template<class F1, class F2>
ILINE Matrix33_tpl<F1> operator*(const Matrix33_tpl<F1>& l, const Matrix33_tpl<F2>& r)
{
	CRY_MATH_ASSERT(l.IsValid());
	CRY_MATH_ASSERT(r.IsValid());
	Matrix33_tpl<F1> m;
	m.m00 = l.m00 * r.m00 + l.m01 * r.m10 + l.m02 * r.m20;
	m.m01 = l.m00 * r.m01 + l.m01 * r.m11 + l.m02 * r.m21;
	m.m02 = l.m00 * r.m02 + l.m01 * r.m12 + l.m02 * r.m22;
	m.m10 = l.m10 * r.m00 + l.m11 * r.m10 + l.m12 * r.m20;
	m.m11 = l.m10 * r.m01 + l.m11 * r.m11 + l.m12 * r.m21;
	m.m12 = l.m10 * r.m02 + l.m11 * r.m12 + l.m12 * r.m22;
	m.m20 = l.m20 * r.m00 + l.m21 * r.m10 + l.m22 * r.m20;
	m.m21 = l.m20 * r.m01 + l.m21 * r.m11 + l.m22 * r.m21;
	m.m22 = l.m20 * r.m02 + l.m21 * r.m12 + l.m22 * r.m22;
	return m;
}

//! Implements the multiplication operator: Matrix34=Matrix33*Matrix34.
//! Matrix33 and Matrix34 are specified in column order for a right-handed coordinate-system.
//! AxB = operation B followed by operation A.
//! A multiplication takes 36 muls and 24 adds.
//! Examples:
//!  Matrix33 m33=Matrix33::CreateRotationX(1.94192f);;
//!  Matrix34 m34=Matrix34::CreateRotationZ(3.14192f);
//!  Matrix34 result=m33*m34;
template<class F1, class F2>
ILINE Matrix34_tpl<F2> operator*(const Matrix33_tpl<F1>& l, const Matrix34_tpl<F2>& r)
{
	CRY_MATH_ASSERT(l.IsValid());
	CRY_MATH_ASSERT(r.IsValid());
	Matrix34_tpl<F2> m;
	m.m00 = l.m00 * r.m00 + l.m01 * r.m10 + l.m02 * r.m20;
	m.m10 = l.m10 * r.m00 + l.m11 * r.m10 + l.m12 * r.m20;
	m.m20 = l.m20 * r.m00 + l.m21 * r.m10 + l.m22 * r.m20;
	m.m01 = l.m00 * r.m01 + l.m01 * r.m11 + l.m02 * r.m21;
	m.m11 = l.m10 * r.m01 + l.m11 * r.m11 + l.m12 * r.m21;
	m.m21 = l.m20 * r.m01 + l.m21 * r.m11 + l.m22 * r.m21;
	m.m02 = l.m00 * r.m02 + l.m01 * r.m12 + l.m02 * r.m22;
	m.m12 = l.m10 * r.m02 + l.m11 * r.m12 + l.m12 * r.m22;
	m.m22 = l.m20 * r.m02 + l.m21 * r.m12 + l.m22 * r.m22;
	m.m03 = l.m00 * r.m03 + l.m01 * r.m13 + l.m02 * r.m23;
	m.m13 = l.m10 * r.m03 + l.m11 * r.m13 + l.m12 * r.m23;
	m.m23 = l.m20 * r.m03 + l.m21 * r.m13 + l.m22 * r.m23;
	return m;
}

//! Implements the multiplication operator: Matrix44=Matrix33*Matrix44.
//! Matrix33 and Matrix44 are specified in collumn order for a right-handed coordinate-system.
//! AxB = operation B followed by operation A.
//! A multiplication takes 36 muls and 24 adds.
//! Example:
//!  Matrix33 m33=Matrix33::CreateRotationX(1.94192f);;
//!  Matrix44 m44=Matrix33::CreateRotationZ(3.14192f);
//!  Matrix44 result=m33*m44;
template<class F1, class F2>
ILINE Matrix44_tpl<F2> operator*(const Matrix33_tpl<F1>& l, const Matrix44_tpl<F2>& r)
{
	CRY_MATH_ASSERT(l.IsValid());
	CRY_MATH_ASSERT(r.IsValid());
	Matrix44_tpl<F2> m;
	m.m00 = l.m00 * r.m00 + l.m01 * r.m10 + l.m02 * r.m20;
	m.m10 = l.m10 * r.m00 + l.m11 * r.m10 + l.m12 * r.m20;
	m.m20 = l.m20 * r.m00 + l.m21 * r.m10 + l.m22 * r.m20;
	m.m30 = r.m30;
	m.m01 = l.m00 * r.m01 + l.m01 * r.m11 + l.m02 * r.m21;
	m.m11 = l.m10 * r.m01 + l.m11 * r.m11 + l.m12 * r.m21;
	m.m21 = l.m20 * r.m01 + l.m21 * r.m11 + l.m22 * r.m21;
	m.m31 = r.m31;
	m.m02 = l.m00 * r.m02 + l.m01 * r.m12 + l.m02 * r.m22;
	m.m12 = l.m10 * r.m02 + l.m11 * r.m12 + l.m12 * r.m22;
	m.m22 = l.m20 * r.m02 + l.m21 * r.m12 + l.m22 * r.m22;
	m.m32 = r.m32;
	m.m03 = l.m00 * r.m03 + l.m01 * r.m13 + l.m02 * r.m23;
	m.m13 = l.m10 * r.m03 + l.m11 * r.m13 + l.m12 * r.m23;
	m.m23 = l.m20 * r.m03 + l.m21 * r.m13 + l.m22 * r.m23;
	m.m33 = r.m33;
	return m;
}

template<class F1, class F2>
ILINE Matrix33_tpl<F1>& operator*=(Matrix33_tpl<F1>& l, const Matrix33_tpl<F2>& r)
{
	CRY_MATH_ASSERT(l.IsValid());
	CRY_MATH_ASSERT(r.IsValid());
	Matrix33_tpl<F1> tmp;
	tmp.m00 = l.m00 * r.m00 + l.m01 * r.m10 + l.m02 * r.m20;
	tmp.m01 = l.m00 * r.m01 + l.m01 * r.m11 + l.m02 * r.m21;
	tmp.m02 = l.m00 * r.m02 + l.m01 * r.m12 + l.m02 * r.m22;
	tmp.m10 = l.m10 * r.m00 + l.m11 * r.m10 + l.m12 * r.m20;
	tmp.m11 = l.m10 * r.m01 + l.m11 * r.m11 + l.m12 * r.m21;
	tmp.m12 = l.m10 * r.m02 + l.m11 * r.m12 + l.m12 * r.m22;
	tmp.m20 = l.m20 * r.m00 + l.m21 * r.m10 + l.m22 * r.m20;
	tmp.m21 = l.m20 * r.m01 + l.m21 * r.m11 + l.m22 * r.m21;
	tmp.m22 = l.m20 * r.m02 + l.m21 * r.m12 + l.m22 * r.m22;
	l = tmp;
	return l;
}

template<class F1, class F2>
ILINE Matrix33_tpl<F1> operator+(const Matrix33_tpl<F1>& l, const Matrix33_tpl<F2>& r)
{
	CRY_MATH_ASSERT(l.IsValid());
	CRY_MATH_ASSERT(r.IsValid());
	Matrix33_tpl<F1> res;
	res.m00 = l.m00 + r.m00;
	res.m01 = l.m01 + r.m01;
	res.m02 = l.m02 + r.m02;
	res.m10 = l.m10 + r.m10;
	res.m11 = l.m11 + r.m11;
	res.m12 = l.m12 + r.m12;
	res.m20 = l.m20 + r.m20;
	res.m21 = l.m21 + r.m21;
	res.m22 = l.m22 + r.m22;
	return res;
}
template<class F1, class F2>
ILINE Matrix33_tpl<F1>& operator+=(Matrix33_tpl<F1>& l, const Matrix33_tpl<F2>& r)
{
	CRY_MATH_ASSERT(l.IsValid());
	CRY_MATH_ASSERT(r.IsValid());
	l.m00 += r.m00;
	l.m01 += r.m01;
	l.m02 += r.m02;
	l.m10 += r.m10;
	l.m11 += r.m11;
	l.m12 += r.m12;
	l.m20 += r.m20;
	l.m21 += r.m21;
	l.m22 += r.m22;
	return l;
}

template<class F1, class F2>
ILINE Matrix33_tpl<F1> operator-(const Matrix33_tpl<F1>& l, const Matrix33_tpl<F2>& r)
{
	CRY_MATH_ASSERT(l.IsValid());
	CRY_MATH_ASSERT(r.IsValid());
	Matrix33_tpl<F1> res;
	res.m00 = l.m00 - r.m00;
	res.m01 = l.m01 - r.m01;
	res.m02 = l.m02 - r.m02;
	res.m10 = l.m10 - r.m10;
	res.m11 = l.m11 - r.m11;
	res.m12 = l.m12 - r.m12;
	res.m20 = l.m20 - r.m20;
	res.m21 = l.m21 - r.m21;
	res.m22 = l.m22 - r.m22;
	return res;
}
template<class F1, class F2>
ILINE Matrix33_tpl<F1>& operator-=(Matrix33_tpl<F1>& l, const Matrix33_tpl<F2>& r)
{
	CRY_MATH_ASSERT(l.IsValid());
	CRY_MATH_ASSERT(r.IsValid());
	l.m00 -= r.m00;
	l.m01 -= r.m01;
	l.m02 -= r.m02;
	l.m10 -= r.m10;
	l.m11 -= r.m11;
	l.m12 -= r.m12;
	l.m20 -= r.m20;
	l.m21 -= r.m21;
	l.m22 -= r.m22;
	return l;
}

template<class F>
ILINE Matrix33_tpl<F> operator*(const Matrix33_tpl<F>& m, F op)
{
	CRY_MATH_ASSERT(m.IsValid());
	Matrix33_tpl<F> res;
	res.m00 = m.m00 * op;
	res.m01 = m.m01 * op;
	res.m02 = m.m02 * op;
	res.m10 = m.m10 * op;
	res.m11 = m.m11 * op;
	res.m12 = m.m12 * op;
	res.m20 = m.m20 * op;
	res.m21 = m.m21 * op;
	res.m22 = m.m22 * op;
	return res;
}
template<class F>
ILINE Matrix33_tpl<F> operator/(const Matrix33_tpl<F>& src, F op) { return src * ((F)1.0 / op); }

//! Post-multiply.
template<class F1, class F2>
ILINE Vec3_tpl<F1> operator*(const Matrix33_tpl<F2>& m, const Vec3_tpl<F1>& p)
{
	CRY_MATH_ASSERT(m.IsValid());
	CRY_MATH_ASSERT(p.IsValid());
	Vec3_tpl<F1> tp;
	tp.x = F1(m.m00 * p.x + m.m01 * p.y + m.m02 * p.z);
	tp.y = F1(m.m10 * p.x + m.m11 * p.y + m.m12 * p.z);
	tp.z = F1(m.m20 * p.x + m.m21 * p.y + m.m22 * p.z);
	return tp;
}

//! Pre-multiply.
template<class F1, class F2>
ILINE Vec3_tpl<F1> operator*(const Vec3_tpl<F1>& p, const Matrix33_tpl<F2>& m)
{
	CRY_MATH_ASSERT(m.IsValid());
	CRY_MATH_ASSERT(p.IsValid());
	Vec3_tpl<F1> tp;
	tp.x = F1(p.x * m.m00 + p.y * m.m10 + p.z * m.m20);
	tp.y = F1(p.x * m.m01 + p.y * m.m11 + p.z * m.m21);
	tp.z = F1(p.x * m.m02 + p.y * m.m12 + p.z * m.m22);
	return tp;
}

//! Post-multiply.
template<class F1, class F2>
ILINE Vec2_tpl<F1> operator*(const Matrix33_tpl<F2>& m, const Vec2_tpl<F1>& v)
{
	CRY_MATH_ASSERT(m.IsValid());
	CRY_MATH_ASSERT(v.IsValid());
	return Vec2_tpl<F1>(v.x * m.m00 + v.y * m.m01, v.x * m.m10 + v.y * m.m11);
}

//! Pre-multiply.
template<class F1, class F2>
ILINE Vec2_tpl<F1> operator*(const Vec2_tpl<F1>& v, const Matrix33_tpl<F2>& m)
{
	CRY_MATH_ASSERT(m.IsValid());
	CRY_MATH_ASSERT(v.IsValid());
	return Vec2_tpl<F1>(v.x * m.m00 + v.y * m.m10, v.x * m.m01 + v.y * m.m11);
}

template<class F1>
ILINE Matrix33_tpl<F1>& crossproduct_matrix(const Vec3_tpl<F1>& v, Matrix33_tpl<F1>& m)
{
	m.m00 = 0;
	m.m01 = -v.z;
	m.m02 = v.y;
	m.m10 = v.z;
	m.m11 = 0;
	m.m12 = -v.x;
	m.m20 = -v.y;
	m.m21 = v.x;
	m.m22 = 0;
	return m;
}

template<class F1>
ILINE Matrix33_tpl<F1>& dotproduct_matrix(const Vec3_tpl<F1>& v, const Vec3_tpl<F1>& op, Matrix33_tpl<F1>& m)
{
	m.m00 = v.x * op.x;
	m.m10 = v.y * op.x;
	m.m20 = v.z * op.x;
	m.m01 = v.x * op.y;
	m.m11 = v.y * op.y;
	m.m21 = v.z * op.y;
	m.m02 = v.x * op.z;
	m.m12 = v.y * op.z;
	m.m22 = v.z * op.z;
	return m;
}



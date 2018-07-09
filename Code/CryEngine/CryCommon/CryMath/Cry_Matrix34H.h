// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Implementation of Matrix44 using SIMD types

#pragma once

#include "Cry_Math_SSE.h"

#ifdef CRY_TYPE_SIMD4

	#include "Cry_Vector4H.h"
	#include "Cry_Matrix33.h"

template<typename F> struct Matrix44H;

///////////////////////////////////////////////////////////////////////////////
//! General-purpose 3x4 matrix using SIMD types
template<typename F> struct Matrix34H
	: INumberArray<vector4_t<F>, 3>
{
	using V = vector4_t<F>;
	using Vec4 = Vec4H<F>;
	using Vec3 = Vec3_tpl<F>;

	union
	{
		struct
		{
			Vec4 x, y, z;
		};
		struct
		{
			F m00, m01, m02, m03;
			F m10, m11, m12, m13;
			F m20, m21, m22, m23;
		};
	};

	// Constructors
	ILINE Matrix34H() {}

	ILINE Matrix34H(type_zero) { this->SetZero(); }

	ILINE void SetIdentity()
	{
		x = Vec4(1, 0, 0, 0);
		y = Vec4(0, 1, 0, 0);
		z = Vec4(0, 0, 1, 0);
	}
	ILINE Matrix34H(type_identity) { SetIdentity(); }
	ILINE static Matrix34H CreateIdentity() { return Matrix34H(IDENTITY); }
	ILINE bool IsIdentity(F threshold = 0) const
	{
		return ::IsEquivalent(x, Vec4(1, 0, 0, 0), threshold)
		    && ::IsEquivalent(y, Vec4(0, 1, 0, 0), threshold)
		    && ::IsEquivalent(z, Vec4(0, 0, 1, 0), threshold);
	}

	ILINE Matrix34H(Vec4 x_, Vec4 y_, Vec4 z_)
		: x(x_), y(y_), z(z_)
	{
		CRY_MATH_ASSERT(this->IsValid());
	}

	ILINE Matrix34H(F v00, F v01, F v02, F v03,
	                F v10, F v11, F v12, F v13,
	                F v20, F v21, F v22, F v23)
		: x(v00, v01, v02, v03)
		, y(v10, v11, v12, v13)
		, z(v20, v21, v22, v23)
	{
		CRY_MATH_ASSERT(this->IsValid());
	}
	
	// Operations
	ILINE Matrix34H& Transpose(const Matrix34H& m)
	{
		V tmp0 = mix0101<0, 0, 1, 1>(m.x, m.y);
		V tmp1 = mix0101<2, 2, 3, 3>(m.x, m.y);
		V tmp2 = mix0101<0, 0, 1, 1>(m.z, convert<V>());
		V tmp3 = mix0101<2, 2, 3, 3>(m.z, convert<V>());
		x = mix0011<0, 1, 0, 1>(tmp0, tmp2);
		y = mix0011<2, 3, 2, 3>(tmp0, tmp2);
		z = mix0011<0, 1, 0, 1>(tmp1, tmp3);
		return *this;
	}
	ILINE void Transpose()
	{
		Transpose(*this);
	}
	ILINE Matrix44H<F> GetTransposed() const
	{
		Matrix44H<F> r;
		r.Transpose(*this);
		return r;
	}

	ILINE F Determinant() const
	{
		return mat().Determinant();
	}

	ILINE F GetUniformScale() const
	{
		return mat().GetUniformScale();
	}

	ILINE Vec3 GetScale() const
	{
		return mat().GetScale();
	}

	Matrix34H& Magnitude(const Matrix34H& m)
	{
		x = crymath::abs(m.x.v);
		y = crymath::abs(m.y.v);
		z = crymath::abs(m.z.v);
		return *this;
	}

	Matrix34H& Invert(const Matrix34H& m)
	{
		V tmp;
		V row0, row1, row2, row3;
		V minor0, minor1, minor2;

		tmp = mix0011<0,1,0,1>(m.x, m.y);
		row1 = mix0011<0,1,0,1>(m.z, convert<V>());
		row0 = mix0011<0,2,0,2>(tmp, row1);
		row1 = mix0011<1,3,1,3>(row1, tmp);
		tmp = mix0011<2,3,2,3>(m.x, m.y);
		row3 = mix0011<2,3,2,3>(m.z, m.w());
		row2 = mix0011<0,2,0,2>(tmp, row3);
		row3 = mix0011<1,3,1,3>(row3, tmp);

		tmp = row2 * row3;
		tmp = shuffle<1,0,3,2>(tmp);
		minor0 = row1 * tmp;
		minor1 = row0 * tmp;
		tmp = shuffle<2,3,0,1>(tmp);
		minor0 = row1 * tmp - minor0;
		minor1 = row0 * tmp - minor1;

		tmp = row1 * row2;
		tmp = shuffle<1,0,3,2>(tmp);
		minor0 = row3 * tmp + minor0;
		tmp = shuffle<2,3,0,1>(tmp);
		minor0 = minor0 - row3 * tmp;

		tmp = shuffle<2,3,0,1>(row1) * row3;
		tmp = shuffle<1,0,3,2>(tmp);
		row2 = shuffle<2,3,0,1>(row2);
		minor0 = row2 * tmp + minor0;
		minor2 = row0 * tmp;
		tmp = shuffle<2,3,0,1>(tmp);
		minor0 = minor0 - row2 * tmp;

		F det = crymath::hsum(row0 * minor0);
		F rdet = F(1) / det;

		minor1 = shuffle<2,3,0,1>(minor1);
		minor2 = row0 * tmp - minor2;
		minor2 = shuffle<2,3,0,1>(minor2);

		tmp = row0 * row1;
		tmp = shuffle<1,0,3,2>(tmp);
		minor2 = row3 * tmp + minor2;
		tmp = shuffle<2,3,0,1>(tmp);
		minor2 = row3 * tmp - minor2;

		tmp = row0 * row3;
		tmp = shuffle<1,0,3,2>(tmp);
		minor1 = minor1 - row2 * tmp;
		minor2 = row1 * tmp + minor2;
		tmp = shuffle<2,3,0,1>(tmp);
		minor1 = row2 * tmp + minor1;
		minor2 = minor2 - row1 * tmp;

		tmp = row0 * row2;
		tmp = shuffle<1,0,3,2>(tmp);
		minor1 = row3 * tmp + minor1;
		tmp = shuffle<2,3,0,1>(tmp);
		minor1 = minor1 - row3 * tmp;

		V vrdet = convert<V>(rdet);

		x = minor0 * vrdet;
		y = minor1 * vrdet;
		z = minor2 * vrdet;
		
		return *this;
	}
	ILINE void Invert()
	{
		Invert(*this);
	}
	ILINE Matrix34H GetInverted() const
	{
		Matrix34H r;
		r.Invert(*this);
		return r;
	}
	ILINE void InvertFast()
	{
		mat().InvertFast();
	}
	ILINE Matrix34H GetInvertedFast() const
	{
		Matrix34H r = *this;
		r.InvertFast();
		return r;
	}
	ILINE void Magnitude()
	{
		Magnitude(*this);
	}
	ILINE Matrix34H GetMagnitude() const
	{
		Matrix34H r;
		r.Magnitude(*this);
		return r;
	}

	// Operators
	COMPOUND_MEMBER_OP_EQ(Matrix34H, *, F)
	{
		x *= b;
		y *= b;
		z *= b;
		return *this;
	}

	COMPOUND_MEMBER_OP_EQ(Matrix34H, +, Matrix34H)
	{
		x += b.x;
		y += b.y;
		z += b.z;
		return *this;
	}
	COMPOUND_MEMBER_OP_EQ(Matrix34H, -, Matrix34H)
	{
		x -= b.x;
		y -= b.y;
		z -= b.z;
		return *this;
	}

	// General transformation

	//! V * M, for row-major layout, quite efficient
	ILINE friend Vec4 operator*(Vec4 v, const Matrix34H& m)
	{
		return broadcast<0>(v) * m.x
	         + broadcast<1>(v) * m.y
	         + broadcast<2>(v) * m.z
		     + v * m.w();
	}

	//! M * V, for column-major layout, less efficient
	ILINE Vec4 operator*(Vec4 v) const
	{
		// Transform matrix on the fly
		V tmp0 = mix0101<0, 0, 1, 1>(x.v, y.v);
		V tmp1 = mix0101<2, 2, 3, 3>(x.v, y.v);
		V tmp2 = mix0101<0, 0, 1, 1>(z.v, convert<V>());
		V tmp3 = mix0101<2, 2, 3, 3>(z.v, w());

		return broadcast<0>(v) * mix0011<0, 1, 0, 1>(tmp0, tmp2)
	         + broadcast<1>(v) * mix0011<2, 3, 2, 3>(tmp0, tmp2)
	         + broadcast<2>(v) * mix0011<0, 1, 0, 1>(tmp1, tmp3)
	         + broadcast<3>(v) * mix0011<2, 3, 2, 3>(tmp1, tmp3);
	}

	//! M * M
	COMPOUND_MEMBER_OP(Matrix34H, *, Matrix34H)
	{
		return Matrix34H(x * b, y * b, z * b);
	}

	//////////////////////////////////////////////////////////////////////
	// Matrix34_tpl compatibility
	using Matrix34 = Matrix34_tpl<F>;
	using Matrix33 = Matrix33_tpl<F>;
	using Quat = Quat_tpl<F>;
	using Ang3 = Ang3_tpl<F>;

	// Matrix conversions
	ILINE const Matrix34& mat() const      { return reinterpret_cast<const Matrix34&>(*this); }
	ILINE Matrix34& mat()                  { return reinterpret_cast<Matrix34&>(*this); }

	ILINE Matrix34H(const Matrix34& m)     { mat() = m; }
	ILINE operator const Matrix34&() const { return mat(); }

	// Convert between element types
	template<typename F2> Matrix34H(const Matrix34_tpl<F2>& m)
	{
		for (int i = 0; i < 12; ++i)
			(&m00)[i] = (F) (&m.m00)[i];
	}

	ILINE Matrix34H(const Matrix33& m, const Vec3& t = ZERO)
		: x(m.m00, m.m01, m.m02, t.x)
		, y(m.m10, m.m11, m.m12, t.y)
		, z(m.m20, m.m21, m.m22, t.z)
	{}
	explicit operator Matrix33() const
	{
		return Matrix33(m00, m01, m02, m10, m11, m12, m20, m21, m22);
	}

	ILINE explicit Matrix34H(const Matrix44H<F>& m)
		: x(m.x), y(m.y), z(m.z) {}

	// Quat conversions
	explicit Matrix34H(const Quat& q)
		: Matrix34H(Matrix33(q)) {}
	explicit Matrix34H(const QuatT_tpl<F>& q)
		: Matrix34H(Matrix33(q.q), q.t) {}
	explicit Matrix34H(const QuatTS_tpl<F>& q)
		: Matrix34H(Matrix33(q.q) * q.s, q.t) {}
	explicit Matrix34H(const QuatTNS_tpl<F>& q)
	{
		Construct(mat(), q);
	}

	// Special constructions

	ILINE Matrix34H(const Vec3& s, const Quat& q, const Vec3& t = ZERO) { mat().Set(s, q, t); }
	ILINE void Set(const Vec3& s, const Quat& q, const Vec3& t = ZERO) { mat().Set(s, q, t); }
	static Matrix34H Create(const Vec3& s, const Quat& q, const Vec3& t = ZERO) { Matrix34H r; r.Set(s, q, t); return r; }

	ILINE void SetTranslationMat(const Vec3& v)
	{
		mat().SetTranslationMat(v);
	}
	ILINE static Matrix34H CreateTranslationMat(const Vec3& v)
	{
		Matrix34H r; r.SetTranslationMat(v); return r;
	}

	// Rotations
	#define SET_CREATE1(Name, A) \
		ILINE void Set##Name(const A& a, const Vec3& t = ZERO) { new(this) Matrix34H(Matrix33::Create##Name(a), t); } \
		ILINE static Matrix34H Create##Name(const A& a, const Vec3& t = ZERO) { return Matrix34H(Matrix33::Create##Name(a), t); } \

	#define SET_CREATE2(Name, A, B) \
		ILINE void Set##Name(const A& a, const B& b, const Vec3& t = ZERO) { new(this) Matrix34H(Matrix33::Create##Name(a, b), t); } \
		ILINE static Matrix34H Create##Name(const A& a, const B& b, const Vec3& t = ZERO) { return Matrix34H(Matrix33::Create##Name(a, b), t); } \

	SET_CREATE2(RotationAA, F, Vec3)
	SET_CREATE1(RotationAA, Vec3)
	SET_CREATE1(RotationX, F)
	SET_CREATE1(RotationY, F)
	SET_CREATE1(RotationZ, F)
	SET_CREATE1(RotationXYZ, Ang3_tpl<F>)
	SET_CREATE1(Scale, Vec3)

	ILINE void SetFromVectors(const Vec3& vx, const Vec3& vy, const Vec3& vz, const Vec3& pos)
	{
		mat().SetFromVectors(vx, vy, vz, pos);
	}
	ILINE static Matrix34H CreateFromVectors(const Vec3& vx, const Vec3& vy, const Vec3& vz, const Vec3& pos)
	{
		Matrix34H r; r.SetFromVectors(vx, vy, vz, pos); return r;
	}

	// Transformation
	ILINE Vec3 TransformVector(const Vec3& v) const
	{
		// Transform matrix on the fly
		V tmp0 = mix0101<0, 0, 1, 1>(x.v, y.v);
		V tmp1 = mix0101<2, 2, 3, 3>(x.v, y.v);
		V tmp2 = mix0101<0, 0, 1, 1>(z.v, convert<V>());
		V tmp3 = mix0101<2, 2, 3, 3>(z.v, convert<V>());

		Vec4 r = convert<V>(v.x) * mix0011<0, 1, 0, 1>(tmp0, tmp2)
	           + convert<V>(v.y) * mix0011<2, 3, 2, 3>(tmp0, tmp2)
	           + convert<V>(v.z) * mix0011<0, 1, 0, 1>(tmp1, tmp3);
		return Vec3(r);
	}
	ILINE Vec3 TransformPoint(const Vec3& v) const
	{
		// Transform matrix on the fly
		V tmp0 = mix0101<0, 0, 1, 1>(x.v, y.v);
		V tmp1 = mix0101<2, 2, 3, 3>(x.v, y.v);
		V tmp2 = mix0101<0, 0, 1, 1>(z.v, convert<V>());
		V tmp3 = mix0101<2, 2, 3, 3>(z.v, convert<V>());

		Vec4 r = convert<V>(v.x) * mix0011<0, 1, 0, 1>(tmp0, tmp2)
	           + convert<V>(v.y) * mix0011<2, 3, 2, 3>(tmp0, tmp2)
	           + convert<V>(v.z) * mix0011<0, 1, 0, 1>(tmp1, tmp3)
	           +                   mix0011<2, 3, 2, 3>(tmp1, tmp3);
		return Vec3(r);
	}
	ILINE Vec3 operator*(const Vec3& v) const
	{
		return TransformPoint(v);
	}

	// Data access
	ILINE const Vec4& operator[](int i) const            { CRY_MATH_ASSERT((uint)i < 3); return (&x)[i]; }
	ILINE Vec4& operator[](int i)                        { CRY_MATH_ASSERT((uint)i < 3); return (&x)[i]; }

	ILINE F           operator()(int i, int j) const     { return (*this)[i][j]; }
	ILINE F&          operator()(int i, int j)           { return (*this)[i][j]; }

	F*                GetData()                          { return &m00; }
	const F*          GetData() const                    { return &m00; }

	ILINE void        SetRow(int i, const Vec3& v)       { (Vec3&)(*this)[i] = v; }
	ILINE void        SetRow4(int i, const Vec4& v)      { (*this)[i] = v; }

	ILINE const Vec3& GetRow(int i) const                { return (const Vec3&)(*this)[i]; }
	ILINE const Vec4& GetRow4(int i) const               { return (*this)[i]; }

	ILINE static Vec4  w()                               { return convert<V>(0,0,0,1); }

	ILINE void        SetColumn(int i, const Vec3& v)    { x[i] = v.x; y[i] = v.y; z[i] = v.z; }
	ILINE Vec3        GetColumn(int i) const             { return Vec3(x[i], y[i], z[i]); }
	ILINE Vec3        GetColumn0() const                 { return Vec3(m00, m10, m20);  }
	ILINE Vec3        GetColumn1() const                 { return Vec3(m01, m11, m21);  }
	ILINE Vec3        GetColumn2() const                 { return Vec3(m02, m12, m22);  }
	ILINE Vec3        GetColumn3() const                 { return Vec3(m03, m13, m23);  }

	ILINE void        SetTranslation(const Vec3& t)      { m03 = t.x;  m13 = t.y; m23 = t.z; }
	ILINE Vec3        GetTranslation() const             { return Vec3(m03, m13, m23); }
	
	ILINE void ScaleTranslation(F s)                     { m03 *= s;   m13 *= s;   m23 *= s; }
	ILINE const Matrix34H& AddTranslation(const Vec3& t) { m03 += t.x; m13 += t.y; m23 += t.z; return *this; }

	ILINE void ScaleColumn(const Vec3& s)	             { Vec4 v(s, 1);        x *= v;  y *= v;  z *= v; }
	ILINE void ScaleColumn(F s)	                         { Vec4 v(s, s, s, 1);  x *= v;  y *= v;  z *= v; }

	ILINE void SetRotation33(const Matrix33& m33)        { mat().SetRotation33(m33); }
	ILINE void GetRotation33(Matrix33& m33) const        { mat().GetRotation33(m33); }

	// Operations
	ILINE int IsOrthonormal(F threshold = 0.001) const   { return mat().IsOrthonormal(threshold); }
	ILINE int IsOrthonormalRH(F threshold = 0.001) const { return mat().IsOrthonormalRH(threshold); }
	ILINE void OrthonormalizeFast()                      { mat().OrthonormalizeFast(); }

	// Matrix**_tpl interoperation
	ILINE Matrix34H        operator*(const Matrix33& b) const               { return *this * Matrix34H(b); }
	ILINE friend Matrix34H operator*(const Matrix33& a, const Matrix34H& b) { return Matrix34H(a) * b; }

	COMPOUND_MEMBER_OP_EQ(Matrix34H, *, Diag33_tpl<F>)
	{
		Vec4 diag(b.x, b.y, b.z, 1);
		x *= diag;
		y *= diag;
		z *= diag;
		return *this;
	}

	const CTypeInfo& TypeInfo() const { return ::TypeInfo((Matrix34*)0); }
};



#endif // CRY_TYPE_SIMD4

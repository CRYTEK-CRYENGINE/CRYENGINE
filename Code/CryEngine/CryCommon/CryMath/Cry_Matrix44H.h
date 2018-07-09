// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Implementation of Matrix44 using SIMD types

#pragma once

#include "Cry_Math_SSE.h"

#ifdef CRY_TYPE_SIMD4

	#include "Cry_Vector4H.h"
	#include "Cry_Matrix44.h"
	#include "Cry_Matrix34.h"
	#include "Cry_Matrix34H.h"

///////////////////////////////////////////////////////////////////////////////
//! General-purpose 4x4 matrix using SIMD types
template<typename F> struct Matrix44H
	: INumberArray<vector4_t<F>, 4>
{
	using V = vector4_t<F>;
	using Vec4 = Vec4H<F>;
	using Matrix34 = Matrix34H<F>;

	union
	{
		struct
		{
			Vec4 x, y, z, w;
		};
		struct
		{
			F m00, m01, m02, m03;
			F m10, m11, m12, m13;
			F m20, m21, m22, m23;
			F m30, m31, m32, m33;
		};
	};

	// Constructors
	ILINE Matrix44H() {}

	ILINE Matrix44H(type_zero) { this->SetZero(); }

	ILINE void SetIdentity()
	{
		x = Vec4(1, 0, 0, 0);
		y = Vec4(0, 1, 0, 0);
		z = Vec4(0, 0, 1, 0);
		w = Vec4(0, 0, 0, 1);
	}
	ILINE Matrix44H(type_identity) { SetIdentity(); }

	ILINE Matrix44H(Vec4 x_, Vec4 y_, Vec4 z_, Vec4 w_)
		: x(x_), y(y_), z(z_), w(w_)
	{
		CRY_MATH_ASSERT(this->IsValid());
	}

	ILINE Matrix44H(F v00, F v01, F v02, F v03,
	                F v10, F v11, F v12, F v13,
	                F v20, F v21, F v22, F v23,
	                F v30, F v31, F v32, F v33)
		: x(v00, v01, v02, v03)
		, y(v10, v11, v12, v13)
		, z(v20, v21, v22, v23)
		, w(v30, v31, v32, v33)
	{
		CRY_MATH_ASSERT(this->IsValid());
	}

	ILINE Matrix44H(const Matrix34& m)
		: x(m.x), y(m.y), z(m.z), w(m.w()) {}

	// Operations
	ILINE Matrix44H& Transpose(const Matrix44H& m)
	{
		transpose(&x.v, &m.x.v);
		return *this;
	}
	ILINE void Transpose()
	{
		transpose(&x.v, &x.v);
	}
	ILINE Matrix44H GetTransposed() const
	{
		Matrix44H r;
		transpose(&r.x.v, &x.v);
		return r;
	}

	F DeterminantInvert(Matrix44H* pInv = 0) const
	{
		V tmp, tmp1, tmp2, tmp3, tmp4, tmp5;
		V row0, row1, row2, row3;
		V minor0;

		tmp = mix0011<0,1,0,1>(x, y);
		row1 = mix0011<0,1,0,1>(z, w);
		row0 = mix0011<0,2,0,2>(tmp, row1);
		row1 = mix0011<1,3,1,3>(row1, tmp);
		tmp = mix0011<2,3,2,3>(x, y);
		row3 = mix0011<2,3,2,3>(z, w);
		row2 = mix0011<0,2,0,2>(tmp, row3);
		row3 = mix0011<1,3,1,3>(row3, tmp);

		tmp = row2 * row3;
		tmp1 = shuffle<1,0,3,2>(tmp);
		minor0 = row1 * tmp1;
		tmp2 = shuffle<2,3,0,1>(tmp1);
		minor0 = row1 * tmp2 - minor0;

		tmp = row1 * row2;
		tmp3 = shuffle<1,0,3,2>(tmp);
		minor0 = row3 * tmp3 + minor0;
		tmp4 = shuffle<2,3,0,1>(tmp3);
		minor0 = minor0 - row3 * tmp4;

		tmp = shuffle<2,3,0,1>(row1) * row3;
		tmp5 = shuffle<1,0,3,2>(tmp);
		row2 = shuffle<2,3,0,1>(row2);
		minor0 = row2 * tmp5 + minor0;
		tmp = shuffle<2,3,0,1>(tmp5);
		minor0 = minor0 - row2 * tmp;

		F det = crymath::hsum(row0 * minor0);
		
		if (pInv)
		{
			F rdet = F(1) / det;

			V minor1, minor2, minor3;
			
			minor1 = row0 * tmp1;
			minor1 = row0 * tmp2 - minor1;
			minor1 = shuffle<2,3,0,1>(minor1);
			minor3 = row0 * tmp3;
			minor3 = row0 * tmp4 - minor3;
			minor3 = shuffle<2,3,0,1>(minor3);
			minor2 = row0 * tmp5;
			minor2 = row0 * tmp - minor2;
			minor2 = shuffle<2,3,0,1>(minor2);

			tmp = row0 * row1;
			tmp = shuffle<1,0,3,2>(tmp);
			minor2 = row3 * tmp + minor2;
			minor3 = row2 * tmp - minor3;
			tmp = shuffle<2,3,0,1>(tmp);
			minor2 = row3 * tmp - minor2;
			minor3 = minor3 - row2 * tmp;

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
			minor3 = minor3 - row1 * tmp;
			tmp = shuffle<2,3,0,1>(tmp);
			minor1 = minor1 - row3 * tmp;
			minor3 = row1 * tmp + minor3;

			V vrdet = convert<V>(rdet);

			pInv->x = minor0 * vrdet;
			pInv->y = minor1 * vrdet;
			pInv->z = minor2 * vrdet;
			pInv->w = minor3 * vrdet;
		}
		
		return det;
	}
	ILINE void Invert(const Matrix44H& m)
	{
		m.DeterminantInvert(this);
	}
	ILINE void Invert()
	{
		DeterminantInvert(this);
	}
	ILINE Matrix44H GetInverted() const
	{
		Matrix44H r;
		DeterminantInvert(&r);
		return r;
	}
	ILINE F Determinant() const
	{
		return DeterminantInvert();
	}

	// Operators
	COMPOUND_MEMBER_OP_EQ(Matrix44H, *, F)
	{
		x *= b;
		y *= b;
		z *= b;
		w *= b;
		return *this;
	}

	COMPOUND_MEMBER_OP_EQ(Matrix44H, +, Matrix44H)
	{
		x += b.x;
		y += b.y;
		z += b.z;
		w += b.w;
		return *this;
	}
	COMPOUND_MEMBER_OP_EQ(Matrix44H, -, Matrix44H)
	{
		x -= b.x;
		y -= b.y;
		z -= b.z;
		w -= b.w;
		return *this;
	}

	// General transformation

	//! V * M, for row-major layout, quite efficient
	ILINE friend Vec4 operator*(Vec4 v, const Matrix44H& m)
	{
		return broadcast<0>(v) * m.x
	         + broadcast<1>(v) * m.y
	         + broadcast<2>(v) * m.z
	         + broadcast<3>(v) * m.w;
	}

	//! M * V, for column-major layout, less efficient
	ILINE Vec4 operator*(Vec4 v) const
	{
		// Transform matrix on the fly
		V tmp0 = mix0101<0, 0, 1, 1>(x.v, y.v);
		V tmp1 = mix0101<2, 2, 3, 3>(x.v, y.v);
		V tmp2 = mix0101<0, 0, 1, 1>(z.v, w.v);
		V tmp3 = mix0101<2, 2, 3, 3>(z.v, w.v);

		return broadcast<0>(v) * mix0011<0, 1, 0, 1>(tmp0, tmp2)
	         + broadcast<1>(v) * mix0011<2, 3, 2, 3>(tmp0, tmp2)
	         + broadcast<2>(v) * mix0011<0, 1, 0, 1>(tmp1, tmp3)
	         + broadcast<3>(v) * mix0011<2, 3, 2, 3>(tmp1, tmp3);
	}

	//! M * M
	COMPOUND_MEMBER_OP(Matrix44H, *, Matrix44H)
	{
		return Matrix44H(x * b, y * b, z * b, w * b);
	}

	//! M * M34
	COMPOUND_MEMBER_OP(Matrix44H, *, Matrix34)
	{
		return Matrix44H(x * b, y * b, z * b, w * b);
	}
	ILINE friend Matrix44H operator*(const Matrix34& a, const Matrix44H& b)
	{
		return Matrix44H(a.x * b, a.y * b, a.z * b, b.w);
	}

	///////////////////////////////////////////////////////////////////////////////
	// Matrix44_tpl interface
	using Matrix44 = Matrix44_tpl<F>;
	using Matrix33 = Matrix33_tpl<F>;
	using Vec3 = Vec3_tpl<F>;

	// Matrix conversions
	ILINE const Matrix44& mat() const      { return reinterpret_cast<const Matrix44&>(*this); }
	ILINE Matrix44& mat()                  { return reinterpret_cast<Matrix44&>(*this); }

	ILINE Matrix44H(const Matrix44& m)     { mat() = m; }
	ILINE operator const Matrix44&() const { return mat(); }

	Matrix44H(const Matrix33& m)
		: x(m.m00, m.m01, m.m02, F(0))
		, y(m.m10, m.m11, m.m12, F(0))
		, z(m.m20, m.m21, m.m22, F(0))
		, w(0, 0, 0, 1)
	{}
	operator Matrix33() const
	{
		return Matrix33(m00, m01, m02, m10, m11, m12, m20, m21, m22);
	}

	// Convert between element types
	template<typename F2> Matrix44H(const Matrix44_tpl<F2>& m)
	{
		for (int i = 0; i < 16; ++i)
			GetData()[i] = (F) m.GetData()[i];
	}

	// Access elements
	ILINE const Vec4& operator[](int i) const   { CRY_MATH_ASSERT((uint)i < 4); return (&x)[i]; }
	ILINE Vec4& operator[](int i)               { CRY_MATH_ASSERT((uint)i < 4); return (&x)[i]; }

	ILINE F    operator()(int i, int j) const   { return (&m00)[i * 4 + j]; }
	ILINE F&   operator()(int i, int j)         { return (&m00)[i * 4 + j]; }

	ILINE F*         GetData()                  { return &m00; }
	ILINE const F*   GetData() const            { return &m00; }

	ILINE Vec4 GetRow4(int i) const             { return (*this)[i]; }
	ILINE void SetRow4(int i, const Vec4& v)    { (*this)[i] = v; }

	ILINE Vec4 GetColumn4(int i) const          { return Vec4(x[i], y[i], z[i], w[i]); }
	ILINE void SetColumn4(int i, const Vec4& v) { x[i] = v.x; y[i] = v.y; z[i] = v.z; w[i] = v.w; }

	ILINE Vec3 GetRow(int i) const              { return (*this)[i]; }
	ILINE void SetRow(int i, const Vec3& v)     { (*this)(i, 0) = v.x; (*this)(i, 1) = v.y; (*this)(i, 2) = v.z; }

	ILINE Vec3 GetColumn(int i) const           { return Vec3(x[i], y[i], z[i]); }
	ILINE void SetColumn(int i, const Vec3& v)  { x[i] = v.x; y[i] = v.y; z[i] = v.z; }

	ILINE Vec3 GetTranslation() const           { return GetColumn(3); }
	ILINE void SetTranslation(const Vec3& t)    { SetColumn(3, t); }

	// Transformation
	ILINE Vec3 TransformVector(const Vec3& v) const
	{
		// Transform matrix on the fly
		V tmp0 = mix0101<0, 0, 1, 1>(x.v, y.v);
		V tmp1 = mix0101<2, 2, 3, 3>(x.v, y.v);
		V tmp2 = mix0101<0, 0, 1, 1>(z.v, w.v);
		V tmp3 = mix0101<2, 2, 3, 3>(z.v, w.v);

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
		V tmp2 = mix0101<0, 0, 1, 1>(z.v, w.v);
		V tmp3 = mix0101<2, 2, 3, 3>(z.v, w.v);

		Vec4 r = convert<V>(v.x) * mix0011<0, 1, 0, 1>(tmp0, tmp2)
	           + convert<V>(v.y) * mix0011<2, 3, 2, 3>(tmp0, tmp2)
	           + convert<V>(v.z) * mix0011<0, 1, 0, 1>(tmp1, tmp3)
	           +                   mix0011<2, 3, 2, 3>(tmp1, tmp3);
		return Vec3(r);
	}

	// Matrix**_tpl interoperation
	ILINE Matrix44H        operator*(const Matrix33& b) const        { return *this * Matrix44H(b); }
	ILINE friend Matrix44H operator*(const Matrix33& a, Matrix44& b) { return Matrix44H(a) * b; }

	const CTypeInfo& TypeInfo() const { return ::TypeInfo((Matrix44*)0); }
};

///////////////////////////////////////////////////////////////////////////////
// Extension of Matrix44ST, stored row-major, transposed from Matrix44_tpl.
// More efficient for transforming vectors.
template<typename F> struct Matrix44HR : protected Matrix44H<F>
{
	using Base = Matrix44H<F>;
	using Base::x; using Base::y; using Base::z; using Base::w;
	using Base::mat;
	using Base::Transpose;

	using Vec4 = Vec4H<F>;
	using Vec3 = Vec3_tpl<F>;

	// Constructors
	ILINE Matrix44HR() {}

	ILINE Matrix44HR(const Matrix44_tpl<F>& m)
		: Base(m)
	{
		Transpose();
	}

	ILINE Matrix44HR(const Matrix34_tpl<F>& m)
		: Base(m)
	{
		Transpose();
	}

	ILINE Matrix44HR(const Matrix33_tpl<F>& m)
		: Base(m)
	{
		Transpose();
	}

	// Transformation
	ILINE Vec3 TransformVector(const Vec3& v) const
	{
		return convert<Vec4>(v.x) * x
		     + convert<Vec4>(v.y) * y
		     + convert<Vec4>(v.z) * z;
	}
	ILINE Vec3 TransformPoint(const Vec3& v) const
	{
		return convert<Vec4>(v.x) * x
		     + convert<Vec4>(v.y) * y
		     + convert<Vec4>(v.z) * z
		     + w;
	}

	ILINE Vec3 GetTranslation() const        { return w; }
	ILINE void SetTranslation(const Vec3& t) { w = Vec4(t, 1); }
};

#endif // CRY_TYPE_SIMD4

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#define MATRIX44_H

template<typename F> struct Matrix44H;
template<typename F> struct Matrix44HR;

//! General-purpose 4x4 matrix
template<typename F> struct Matrix44_tpl
	: INumberArray<F, 16>
{
	typedef INumberArray<F, 16> NA;

	F m00, m01, m02, m03;
	F m10, m11, m12, m13;
	F m20, m21, m22, m23;
	F m30, m31, m32, m33;

	ILINE Matrix44_tpl() {}
	ILINE Matrix44_tpl(type_zero) { NA::SetZero(); }

	template<class F1> ILINE Matrix44_tpl(const Matrix44_tpl<F1>& m) { NA::Set(m); }
	template<class F1> ILINE Matrix44_tpl(const Matrix44H<F1>& m) { NA::Set(m.mat()); }
	template<class F1> ILINE Matrix44_tpl(const Matrix44HR<F1>& m) { NA::Set(m.mat()); Transpose(); }

	// implementation of the constructors

	ILINE Matrix44_tpl(F v00, F v01, F v02, F v03,
	                   F v10, F v11, F v12, F v13,
	                   F v20, F v21, F v22, F v23,
	                   F v30, F v31, F v32, F v33)
	{
		m00 = v00;
		m01 = v01;
		m02 = v02;
		m03 = v03;
		m10 = v10;
		m11 = v11;
		m12 = v12;
		m13 = v13;
		m20 = v20;
		m21 = v21;
		m22 = v22;
		m23 = v23;
		m30 = v30;
		m31 = v31;
		m32 = v32;
		m33 = v33;
	}

	//! CONSTRUCTOR for different types. It converts a Matrix33 into a Matrix44 and also converts between double/float.
	//! Matrix44(Matrix33);
	template<class F1> ILINE Matrix44_tpl(const Matrix33_tpl<F1> &m)
	{
		CRY_MATH_ASSERT(m.IsValid());
		m00 = F(m.m00);
		m01 = F(m.m01);
		m02 = F(m.m02);
		m03 = 0;
		m10 = F(m.m10);
		m11 = F(m.m11);
		m12 = F(m.m12);
		m13 = 0;
		m20 = F(m.m20);
		m21 = F(m.m21);
		m22 = F(m.m22);
		m23 = 0;
		m30 = 0;
		m31 = 0;
		m32 = 0;
		m33 = 1;
	}

	//! CONSTRUCTOR for different types. It converts a Matrix34 into a Matrix44 and also converts between double/float.
	//! Matrix44(Matrix34);
	template<class F1> ILINE Matrix44_tpl(const Matrix34_tpl<F1> &m)
	{
		CRY_MATH_ASSERT(m.IsValid());
		m00 = F(m.m00);
		m01 = F(m.m01);
		m02 = F(m.m02);
		m03 = F(m.m03);
		m10 = F(m.m10);
		m11 = F(m.m11);
		m12 = F(m.m12);
		m13 = F(m.m13);
		m20 = F(m.m20);
		m21 = F(m.m21);
		m22 = F(m.m22);
		m23 = F(m.m23);
		m30 = 0;
		m31 = 0;
		m32 = 0;
		m33 = 1;
	}
#ifdef MATRIX34H
	template<class F1> ILINE Matrix44_tpl(const Matrix34H<F1> &m)
	{
		CRY_MATH_ASSERT(m.IsValid());
		m00 = F(m.m00);
		m01 = F(m.m01);
		m02 = F(m.m02);
		m03 = F(m.m03);
		m10 = F(m.m10);
		m11 = F(m.m11);
		m12 = F(m.m12);
		m13 = F(m.m13);
		m20 = F(m.m20);
		m21 = F(m.m21);
		m22 = F(m.m22);
		m23 = F(m.m23);
		m30 = 0;
		m31 = 0;
		m32 = 0;
		m33 = 1;
	}
#endif
	//---------------------------------------------------------------------

	//! multiply all m1 matrix's values by f and return the matrix
	friend  ILINE Matrix44_tpl<F> operator*(const Matrix44_tpl<F>& m, const F f)
	{
		CRY_MATH_ASSERT(m.IsValid());
		Matrix44_tpl<F> r;
		r.m00 = m.m00 * f;
		r.m01 = m.m01 * f;
		r.m02 = m.m02 * f;
		r.m03 = m.m03 * f;
		r.m10 = m.m10 * f;
		r.m11 = m.m11 * f;
		r.m12 = m.m12 * f;
		r.m13 = m.m13 * f;
		r.m20 = m.m20 * f;
		r.m21 = m.m21 * f;
		r.m22 = m.m22 * f;
		r.m23 = m.m23 * f;
		r.m30 = m.m30 * f;
		r.m31 = m.m31 * f;
		r.m32 = m.m32 * f;
		r.m33 = m.m33 * f;
		return r;
	}

	//! add all m matrix's values and return the matrix
	friend  ILINE Matrix44_tpl<F> operator+(const Matrix44_tpl<F>& mm0, const Matrix44_tpl<F>& mm1)
	{
		CRY_MATH_ASSERT(mm0.IsValid());
		CRY_MATH_ASSERT(mm1.IsValid());
		Matrix44_tpl<F> r;
		r.m00 = mm0.m00 + mm1.m00;
		r.m01 = mm0.m01 + mm1.m01;
		r.m02 = mm0.m02 + mm1.m02;
		r.m03 = mm0.m03 + mm1.m03;
		r.m10 = mm0.m10 + mm1.m10;
		r.m11 = mm0.m11 + mm1.m11;
		r.m12 = mm0.m12 + mm1.m12;
		r.m13 = mm0.m13 + mm1.m13;
		r.m20 = mm0.m20 + mm1.m20;
		r.m21 = mm0.m21 + mm1.m21;
		r.m22 = mm0.m22 + mm1.m22;
		r.m23 = mm0.m23 + mm1.m23;
		r.m30 = mm0.m30 + mm1.m30;
		r.m31 = mm0.m31 + mm1.m31;
		r.m32 = mm0.m32 + mm1.m32;
		r.m33 = mm0.m33 + mm1.m33;
		return r;
	}

	ILINE void SetIdentity()
	{
		m00 = 1;
		m01 = 0;
		m02 = 0;
		m03 = 0;
		m10 = 0;
		m11 = 1;
		m12 = 0;
		m13 = 0;
		m20 = 0;
		m21 = 0;
		m22 = 1;
		m23 = 0;
		m30 = 0;
		m31 = 0;
		m32 = 0;
		m33 = 1;
	}
	ILINE Matrix44_tpl(type_identity) { SetIdentity(); }

	ILINE Matrix44_tpl& Transpose(const Matrix44_tpl& m)
	{
		m00 = m.m00;
		m01 = m.m10;
		m02 = m.m20;
		m03 = m.m30;
		m10 = m.m01;
		m11 = m.m11;
		m12 = m.m21;
		m13 = m.m31;
		m20 = m.m02;
		m21 = m.m12;
		m22 = m.m22;
		m23 = m.m32;
		m30 = m.m03;
		m31 = m.m13;
		m32 = m.m23;
		m33 = m.m33;
		return *this;
	}
	ILINE void Transpose()
	{
		Transpose(Matrix44_tpl(*this));
	}
	ILINE Matrix44_tpl GetTransposed() const
	{
		Matrix44_tpl tmp;
		tmp.Transpose(*this);
		return tmp;
	}

	//! Calculate a real inversion of a Matrix44.
	//! Uses Cramer's Rule which is faster (branchless) but numerically less stable than other methods like Gaussian Elimination.
	//! Example 1:
	//!   Matrix44 im44; im44.Invert();
	//! Example 2:
	//!   Matrix44 im44 = m33.GetInverted();
	F DeterminantInvert(Matrix44_tpl* pInv = 0) const
	{
		F tmp[16];

		// Calculate pairs for first 8 elements (cofactors)
		tmp[0] = m22 * m33;
		tmp[1] = m32 * m23;
		tmp[2] = m12 * m33;
		tmp[3] = m32 * m13;
		tmp[4] = m12 * m23;
		tmp[5] = m22 * m13;
		tmp[6] = m02 * m33;
		tmp[7] = m32 * m03;
		tmp[8] = m02 * m23;
		tmp[9] = m22 * m03;
		tmp[10] = m02 * m13;
		tmp[11] = m12 * m03;

		// Calculate first 8 elements (cofactors)
		tmp[12] = tmp[0] * m11 + tmp[3] * m21 + tmp[4] * m31;
		tmp[12] -= tmp[1] * m11 + tmp[2] * m21 + tmp[5] * m31;
		tmp[13] = tmp[1] * m01 + tmp[6] * m21 + tmp[9] * m31;
		tmp[13] -= tmp[0] * m01 + tmp[7] * m21 + tmp[8] * m31;
		tmp[14] = tmp[2] * m01 + tmp[7] * m11 + tmp[10] * m31;
		tmp[14] -= tmp[3] * m01 + tmp[6] * m11 + tmp[11] * m31;
		tmp[15] = tmp[5] * m01 + tmp[8] * m11 + tmp[11] * m21;
		tmp[15] -= tmp[4] * m01 + tmp[9] * m11 + tmp[10] * m21;

		// Calculate determinant
		F det = (m00 * tmp[12] + m10 * tmp[13] + m20 * tmp[14] + m30 * tmp[15]);

		if (pInv)
		{
			F rdet = F(1) / det;

			pInv->m00 = tmp[12];
			pInv->m01 = tmp[13];
			pInv->m02 = tmp[14];
			pInv->m03 = tmp[15];
			pInv->m10 = tmp[1] * m10 + tmp[2] * m20 + tmp[5] * m30;
			pInv->m10 -= tmp[0] * m10 + tmp[3] * m20 + tmp[4] * m30;
			pInv->m11 = tmp[0] * m00 + tmp[7] * m20 + tmp[8] * m30;
			pInv->m11 -= tmp[1] * m00 + tmp[6] * m20 + tmp[9] * m30;
			pInv->m12 = tmp[3] * m00 + tmp[6] * m10 + tmp[11] * m30;
			pInv->m12 -= tmp[2] * m00 + tmp[7] * m10 + tmp[10] * m30;
			pInv->m13 = tmp[4] * m00 + tmp[9] * m10 + tmp[10] * m20;
			pInv->m13 -= tmp[5] * m00 + tmp[8] * m10 + tmp[11] * m20;

			// Calculate pairs for second 8 elements (cofactors)
			tmp[0] = m20 * m31;
			tmp[1] = m30 * m21;
			tmp[2] = m10 * m31;
			tmp[3] = m30 * m11;
			tmp[4] = m10 * m21;
			tmp[5] = m20 * m11;
			tmp[6] = m00 * m31;
			tmp[7] = m30 * m01;
			tmp[8] = m00 * m21;
			tmp[9] = m20 * m01;
			tmp[10] = m00 * m11;
			tmp[11] = m10 * m01;

			// Calculate second 8 elements (cofactors)
			pInv->m20 = tmp[0] * m13 + tmp[3] * m23 + tmp[4] * m33;
			pInv->m20 -= tmp[1] * m13 + tmp[2] * m23 + tmp[5] * m33;
			pInv->m21 = tmp[1] * m03 + tmp[6] * m23 + tmp[9] * m33;
			pInv->m21 -= tmp[0] * m03 + tmp[7] * m23 + tmp[8] * m33;
			pInv->m22 = tmp[2] * m03 + tmp[7] * m13 + tmp[10] * m33;
			pInv->m22 -= tmp[3] * m03 + tmp[6] * m13 + tmp[11] * m33;
			pInv->m23 = tmp[5] * m03 + tmp[8] * m13 + tmp[11] * m23;
			pInv->m23 -= tmp[4] * m03 + tmp[9] * m13 + tmp[10] * m23;
			pInv->m30 = tmp[2] * m22 + tmp[5] * m32 + tmp[1] * m12;
			pInv->m30 -= tmp[4] * m32 + tmp[0] * m12 + tmp[3] * m22;
			pInv->m31 = tmp[8] * m32 + tmp[0] * m02 + tmp[7] * m22;
			pInv->m31 -= tmp[6] * m22 + tmp[9] * m32 + tmp[1] * m02;
			pInv->m32 = tmp[6] * m12 + tmp[11] * m32 + tmp[3] * m02;
			pInv->m32 -= tmp[10] * m32 + tmp[2] * m02 + tmp[7] * m12;
			pInv->m33 = tmp[10] * m22 + tmp[4] * m02 + tmp[9] * m12;
			pInv->m33 -= tmp[8] * m12 + tmp[11] * m22 + tmp[5] * m02;

			// Divide the cofactor-matrix by the determinant
			pInv->m00 *= rdet;
			pInv->m01 *= rdet;
			pInv->m02 *= rdet;
			pInv->m03 *= rdet;
			pInv->m10 *= rdet;
			pInv->m11 *= rdet;
			pInv->m12 *= rdet;
			pInv->m13 *= rdet;
			pInv->m20 *= rdet;
			pInv->m21 *= rdet;
			pInv->m22 *= rdet;
			pInv->m23 *= rdet;
			pInv->m30 *= rdet;
			pInv->m31 *= rdet;
			pInv->m32 *= rdet;
			pInv->m33 *= rdet;
		}

		return det;
	}
	ILINE void Invert(const Matrix44_tpl<F>& m)
	{
		m.DeterminantInvert(this);
	}
	ILINE void Invert()
	{
		DeterminantInvert(this);
	}

	ILINE Matrix44_tpl<F> GetInverted() const
	{
		Matrix44_tpl<F> dst;
		DeterminantInvert(&dst);
		return dst;
	}

	ILINE F Determinant() const
	{
		return DeterminantInvert();
	}

	//! Transform a vector.
	ILINE Vec3 TransformVector(const Vec3& b) const
	{
		CRY_MATH_ASSERT(b.IsValid());
		Vec3 v;
		v.x = m00 * b.x + m01 * b.y + m02 * b.z;
		v.y = m10 * b.x + m11 * b.y + m12 * b.z;
		v.z = m20 * b.x + m21 * b.y + m22 * b.z;
		return v;
	}
	//! Transform a point.
	ILINE Vec3 TransformPoint(const Vec3& b) const
	{
		CRY_MATH_ASSERT(b.IsValid());
		Vec3 v;
		v.x = m00 * b.x + m01 * b.y + m02 * b.z + m03;
		v.y = m10 * b.x + m11 * b.y + m12 * b.z + m13;
		v.z = m20 * b.x + m21 * b.y + m22 * b.z + m23;
		return v;
	}

	// helper functions to access matrix-members
	ILINE F*                 GetData()                               { return &m00; }
	ILINE const F*           GetData() const                         { return &m00; }

	ILINE F                  operator()(uint32 i, uint32 j) const    { CRY_MATH_ASSERT((i < 4) && (j < 4)); F* p_data = (F*)(&m00); return p_data[i * 4 + j]; }
	ILINE F&                 operator()(uint32 i, uint32 j)          { CRY_MATH_ASSERT((i < 4) && (j < 4)); F* p_data = (F*)(&m00); return p_data[i * 4 + j]; }

	ILINE void               SetRow(int i, const Vec3_tpl<F>& v)     { CRY_MATH_ASSERT(i < 4); F* p = (F*)(&m00); p[0 + 4 * i] = v.x; p[1 + 4 * i] = v.y; p[2 + 4 * i] = v.z; }
	ILINE void               SetRow4(int i, const Vec4_tpl<F>& v)    { CRY_MATH_ASSERT(i < 4); F* p = (F*)(&m00); p[0 + 4 * i] = v.x; p[1 + 4 * i] = v.y; p[2 + 4 * i] = v.z; p[3 + 4 * i] = v.w; }
	ILINE const Vec3_tpl<F>& GetRow(int i) const                     { CRY_MATH_ASSERT(i < 4); return *(const Vec3_tpl<F>*)(&m00 + 4 * i); }
	ILINE const Vec4_tpl<F>& GetRow4(int i) const                    { CRY_MATH_ASSERT(i < 4); return *(const Vec4_tpl<F>*)(&m00 + 4 * i); }

	ILINE void               SetColumn(int i, const Vec3_tpl<F>& v)  { CRY_MATH_ASSERT(i < 4); F* p = (F*)(&m00); p[i + 4 * 0] = v.x; p[i + 4 * 1] = v.y; p[i + 4 * 2] = v.z; }
	ILINE void               SetColumn4(int i, const Vec4_tpl<F>& v) { CRY_MATH_ASSERT(i < 4); F* p = (F*)(&m00); p[i + 4 * 0] = v.x; p[i + 4 * 1] = v.y; p[i + 4 * 2] = v.z; p[i + 4 * 3] = v.w; }
	ILINE Vec3_tpl<F>        GetColumn(int i) const                  { CRY_MATH_ASSERT(i < 4); F* p = (F*)(&m00); return Vec3_tpl<F>(p[i + 4 * 0], p[i + 4 * 1], p[i + 4 * 2]); }
	ILINE Vec4_tpl<F>        GetColumn4(int i) const                 { CRY_MATH_ASSERT(i < 4); F* p = (F*)(&m00); return Vec4_tpl<F>(p[i + 4 * 0], p[i + 4 * 1], p[i + 4 * 2], p[i + 4 * 3]); }

	ILINE Vec3               GetTranslation() const                  { return Vec3(m03, m13, m23); }
	ILINE void               SetTranslation(const Vec3& t)           { m03 = t.x; m13 = t.y; m23 = t.z; }
	
	AUTO_STRUCT_INFO;
};

// implementation of Matrix44

//! Implements the multiplication operator: Matrix44=Matrix44*Matrix33diag.
//! Matrix44 and Matrix33diag are specified in collumn order.
//! AxB = operation B followed by operation A.
//! This operation takes 12 mults.
//! Example:
//!   Matrix33diag diag(1,2,3);
//!   Matrix44 m44=CreateRotationZ33(3.14192f);
//!   Matrix44 result=m44*diag;
template<class F1, class F2>
ILINE Matrix44_tpl<F1> operator*(const Matrix44_tpl<F1>& l, const Diag33_tpl<F2>& r)
{
	CRY_MATH_ASSERT(l.IsValid());
	CRY_MATH_ASSERT(r.IsValid());
	Matrix44_tpl<F1> m;
	m.m00 = l.m00 * r.x;
	m.m01 = l.m01 * r.y;
	m.m02 = l.m02 * r.z;
	m.m03 = l.m03;
	m.m10 = l.m10 * r.x;
	m.m11 = l.m11 * r.y;
	m.m12 = l.m12 * r.z;
	m.m13 = l.m13;
	m.m20 = l.m20 * r.x;
	m.m21 = l.m21 * r.y;
	m.m22 = l.m22 * r.z;
	m.m23 = l.m23;
	m.m30 = l.m30 * r.x;
	m.m31 = l.m31 * r.y;
	m.m32 = l.m32 * r.z;
	m.m33 = l.m33;
	return m;
}
template<class F1, class F2>
ILINE Matrix44_tpl<F1>& operator*=(Matrix44_tpl<F1>& l, const Diag33_tpl<F2>& r)
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
	l.m30 *= r.x;
	l.m31 *= r.y;
	l.m32 *= r.z;
	return l;
}

//! Implements the multiplication operator: Matrix44=Matrix44*Matrix33.
//! Matrix44 and Matrix33 are specified in collumn order.
//! AxB = rotation B followed by rotation A.
//! This operation takes 48 mults and 24 adds.
//! Example:
//!   Matrix33 m34=CreateRotationX33(1.94192f);;
//!   Matrix44 m44=CreateRotationZ33(3.14192f);
//!   Matrix44 result=m44*m33;
template<class F1, class F2>
ILINE Matrix44_tpl<F1> operator*(const Matrix44_tpl<F1>& l, const Matrix33_tpl<F2>& r)
{
	CRY_MATH_ASSERT(l.IsValid());
	CRY_MATH_ASSERT(r.IsValid());
	Matrix44_tpl<F1> m;
	m.m00 = l.m00 * r.m00 + l.m01 * r.m10 + l.m02 * r.m20;
	m.m10 = l.m10 * r.m00 + l.m11 * r.m10 + l.m12 * r.m20;
	m.m20 = l.m20 * r.m00 + l.m21 * r.m10 + l.m22 * r.m20;
	m.m30 = l.m30 * r.m00 + l.m31 * r.m10 + l.m32 * r.m20;
	m.m01 = l.m00 * r.m01 + l.m01 * r.m11 + l.m02 * r.m21;
	m.m11 = l.m10 * r.m01 + l.m11 * r.m11 + l.m12 * r.m21;
	m.m21 = l.m20 * r.m01 + l.m21 * r.m11 + l.m22 * r.m21;
	m.m31 = l.m30 * r.m01 + l.m31 * r.m11 + l.m32 * r.m21;
	m.m02 = l.m00 * r.m02 + l.m01 * r.m12 + l.m02 * r.m22;
	m.m12 = l.m10 * r.m02 + l.m11 * r.m12 + l.m12 * r.m22;
	m.m22 = l.m20 * r.m02 + l.m21 * r.m12 + l.m22 * r.m22;
	m.m32 = l.m30 * r.m02 + l.m31 * r.m12 + l.m32 * r.m22;
	m.m03 = l.m03;
	m.m13 = l.m13;
	m.m23 = l.m23;
	m.m33 = l.m33;
	return m;
}

//! Implements the multiplication operator: Matrix44=Matrix44*Matrix34.
//! Matrix44 and Matrix34 are specified in collumn order.
//! AxB = rotation B followed by rotation A.
//! This operation takes 48 mults and 36 adds.
//! Example:
//!   Matrix34 m34=CreateRotationX33(1.94192f);;
//!   Matrix44 m44=CreateRotationZ33(3.14192f);
//!   Matrix44 result=m44*m34;
template<class F>
ILINE Matrix44_tpl<F> operator*(const Matrix44_tpl<F>& l, const Matrix34_tpl<F>& r)
{
	CRY_MATH_ASSERT(l.IsValid());
	CRY_MATH_ASSERT(r.IsValid());
	Matrix44_tpl<F> m;
	m.m00 = l.m00 * r.m00 + l.m01 * r.m10 + l.m02 * r.m20;
	m.m10 = l.m10 * r.m00 + l.m11 * r.m10 + l.m12 * r.m20;
	m.m20 = l.m20 * r.m00 + l.m21 * r.m10 + l.m22 * r.m20;
	m.m30 = l.m30 * r.m00 + l.m31 * r.m10 + l.m32 * r.m20;
	m.m01 = l.m00 * r.m01 + l.m01 * r.m11 + l.m02 * r.m21;
	m.m11 = l.m10 * r.m01 + l.m11 * r.m11 + l.m12 * r.m21;
	m.m21 = l.m20 * r.m01 + l.m21 * r.m11 + l.m22 * r.m21;
	m.m31 = l.m30 * r.m01 + l.m31 * r.m11 + l.m32 * r.m21;
	m.m02 = l.m00 * r.m02 + l.m01 * r.m12 + l.m02 * r.m22;
	m.m12 = l.m10 * r.m02 + l.m11 * r.m12 + l.m12 * r.m22;
	m.m22 = l.m20 * r.m02 + l.m21 * r.m12 + l.m22 * r.m22;
	m.m32 = l.m30 * r.m02 + l.m31 * r.m12 + l.m32 * r.m22;
	m.m03 = l.m00 * r.m03 + l.m01 * r.m13 + l.m02 * r.m23 + l.m03;
	m.m13 = l.m10 * r.m03 + l.m11 * r.m13 + l.m12 * r.m23 + l.m13;
	m.m23 = l.m20 * r.m03 + l.m21 * r.m13 + l.m22 * r.m23 + l.m23;
	m.m33 = l.m30 * r.m03 + l.m31 * r.m13 + l.m32 * r.m23 + l.m33;
	return m;
}

//! Implements the multiplication operator: Matrix44=Matrix44*Matrix44.
//! Matrix44 and Matrix34 are specified in collumn order.
//! AxB = rotation B followed by rotation A.
//! This operation takes 48 mults and 36 adds.
//! Example:
//!   Matrix44 m44=CreateRotationX33(1.94192f);;
//!   Matrix44 m44=CreateRotationZ33(3.14192f);
//!   Matrix44 result=m44*m44;
template<class F1, class F2>
ILINE Matrix44_tpl<F1> operator*(const Matrix44_tpl<F1>& l, const Matrix44_tpl<F2>& r)
{
	CRY_MATH_ASSERT(l.IsValid());
	CRY_MATH_ASSERT(r.IsValid());
	Matrix44_tpl<F1> res;
	res.m00 = l.m00 * r.m00 + l.m01 * r.m10 + l.m02 * r.m20 + l.m03 * r.m30;
	res.m10 = l.m10 * r.m00 + l.m11 * r.m10 + l.m12 * r.m20 + l.m13 * r.m30;
	res.m20 = l.m20 * r.m00 + l.m21 * r.m10 + l.m22 * r.m20 + l.m23 * r.m30;
	res.m30 = l.m30 * r.m00 + l.m31 * r.m10 + l.m32 * r.m20 + l.m33 * r.m30;
	res.m01 = l.m00 * r.m01 + l.m01 * r.m11 + l.m02 * r.m21 + l.m03 * r.m31;
	res.m11 = l.m10 * r.m01 + l.m11 * r.m11 + l.m12 * r.m21 + l.m13 * r.m31;
	res.m21 = l.m20 * r.m01 + l.m21 * r.m11 + l.m22 * r.m21 + l.m23 * r.m31;
	res.m31 = l.m30 * r.m01 + l.m31 * r.m11 + l.m32 * r.m21 + l.m33 * r.m31;
	res.m02 = l.m00 * r.m02 + l.m01 * r.m12 + l.m02 * r.m22 + l.m03 * r.m32;
	res.m12 = l.m10 * r.m02 + l.m11 * r.m12 + l.m12 * r.m22 + l.m13 * r.m32;
	res.m22 = l.m20 * r.m02 + l.m21 * r.m12 + l.m22 * r.m22 + l.m23 * r.m32;
	res.m32 = l.m30 * r.m02 + l.m31 * r.m12 + l.m32 * r.m22 + l.m33 * r.m32;
	res.m03 = l.m00 * r.m03 + l.m01 * r.m13 + l.m02 * r.m23 + l.m03 * r.m33;
	res.m13 = l.m10 * r.m03 + l.m11 * r.m13 + l.m12 * r.m23 + l.m13 * r.m33;
	res.m23 = l.m20 * r.m03 + l.m21 * r.m13 + l.m22 * r.m23 + l.m23 * r.m33;
	res.m33 = l.m30 * r.m03 + l.m31 * r.m13 + l.m32 * r.m23 + l.m33 * r.m33;
	return res;
}

//! Post-multiply.
template<class F1, class F2>
ILINE Vec4_tpl<F1> operator*(const Matrix44_tpl<F2>& m, const Vec4_tpl<F1>& v)
{
	CRY_MATH_ASSERT(m.IsValid());
	CRY_MATH_ASSERT(v.IsValid());
	return Vec4_tpl<F1>(v.x * m.m00 + v.y * m.m01 + v.z * m.m02 + v.w * m.m03,
	                    v.x * m.m10 + v.y * m.m11 + v.z * m.m12 + v.w * m.m13,
	                    v.x * m.m20 + v.y * m.m21 + v.z * m.m22 + v.w * m.m23,
	                    v.x * m.m30 + v.y * m.m31 + v.z * m.m32 + v.w * m.m33);
}

//! Pre-multiply.
template<class F1, class F2>
ILINE Vec4_tpl<F1> operator*(const Vec4_tpl<F1>& v, const Matrix44_tpl<F2>& m)
{
	CRY_MATH_ASSERT(m.IsValid());
	CRY_MATH_ASSERT(v.IsValid());
	return Vec4_tpl<F1>(v.x * m.m00 + v.y * m.m10 + v.z * m.m20 + v.w * m.m30,
	                    v.x * m.m01 + v.y * m.m11 + v.z * m.m21 + v.w * m.m31,
	                    v.x * m.m02 + v.y * m.m12 + v.z * m.m22 + v.w * m.m32,
	                    v.x * m.m03 + v.y * m.m13 + v.z * m.m23 + v.w * m.m33);
}

// Typedefs

#include "Cry_Matrix44H.h"

#ifdef CRY_HARDWARE_VECTOR4

typedef Matrix44H<f32> Matrix44;
typedef Matrix44H<f32> Matrix44A;

#else

typedef Matrix44_tpl<f32> Matrix44;
typedef CRY_ALIGN (16) Matrix44_tpl<f32> Matrix44A;

#endif

typedef Matrix44_tpl<f32>  Matrix44f;
typedef Matrix44_tpl<f64>  Matrix44d;
typedef Matrix44_tpl<real> Matrix44r;  //!< Variable float precision. depending on the target system it can be between 32, 64 or 80 bit.

#if CRY_COMPILER_GCC
/* GCC has a bug where TYPE_USER_ALIGN is ignored in certain situations with template layouts
 * the user speficied alignment is not correctly stored with the new type unless it's layed before.
 * This bug has been fixed for gcc5: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65734
 * Workaroud by forcing the template which needs to be aligned to be fully instantiated: (deprecated so it's not used by accident)
 */
struct __attribute__((deprecated)) Matrix44_f32_dont_use: public Matrix44_tpl<f32> {};
#endif

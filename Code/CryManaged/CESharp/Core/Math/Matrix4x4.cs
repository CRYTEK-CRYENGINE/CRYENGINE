// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using CryEngine.Common;

using System;
using System.Globalization;

namespace CryEngine
{
	public struct Matrix4x4 : IEquatable<Matrix4x4>
	{
		public static readonly Matrix4x4 Identity =
			new Matrix4x4(1f, 0f, 0f, 0f,
										0f, 1f, 0f, 0f,
										0f, 0f, 1f, 0f,
										0f, 0f, 0f, 1f);

		public float m00, m01, m02, m03;
		public float m10, m11, m12, m13;
		public float m20, m21, m22, m23;
		public float m30, m31, m32, m33;

		#region Constructors
		public Matrix4x4(float p00, float p01, float p02, float p03,
						 float p10, float p11, float p12, float p13,
						 float p20, float p21, float p22, float p23,
						 float p30, float p31, float p32, float p33)
		{
			m00 = p00;
			m01 = p01;
			m02 = p02;
			m03 = p03;

			m10 = p10;
			m11 = p11;
			m12 = p12;
			m13 = p13;

			m20 = p20;
			m21 = p21;
			m22 = p22;
			m23 = p23;

			m30 = p30;
			m31 = p31;
			m32 = p32;
			m33 = p33;
		}

		public Matrix4x4(Matrix4x4 matrix4x4)
		{
			m00 = matrix4x4.m00;
			m01 = matrix4x4.m01;
			m02 = matrix4x4.m02;
			m03 = matrix4x4.m03;
			m10 = matrix4x4.m10;
			m11 = matrix4x4.m11;
			m12 = matrix4x4.m12;
			m13 = matrix4x4.m13;
			m20 = matrix4x4.m20;
			m21 = matrix4x4.m21;
			m22 = matrix4x4.m22;
			m23 = matrix4x4.m23;
			m30 = matrix4x4.m30;
			m31 = matrix4x4.m31;
			m32 = matrix4x4.m32;
			m33 = matrix4x4.m33;
		}

		public Matrix4x4(Matrix3x3 matrix3x3)
		{
			m00 = matrix3x3.m00;
			m01 = matrix3x3.m01;
			m02 = matrix3x3.m02;
			m03 = 0;
			m10 = matrix3x3.m10;
			m11 = matrix3x3.m11;
			m12 = matrix3x3.m12;
			m13 = 0;
			m20 = matrix3x3.m20;
			m21 = matrix3x3.m21;
			m22 = matrix3x3.m22;
			m23 = 0;
			m30 = 0;
			m31 = 0;
			m32 = 0;
			m33 = 1;
		}


		public Matrix4x4(Matrix3x4 matrix3x4)
		{
			m00 = matrix3x4.m00;
			m01 = matrix3x4.m01;
			m02 = matrix3x4.m02;
			m03 = matrix3x4.m03;
			m10 = matrix3x4.m10;
			m11 = matrix3x4.m11;
			m12 = matrix3x4.m12;
			m13 = matrix3x4.m13;
			m20 = matrix3x4.m20;
			m21 = matrix3x4.m21;
			m22 = matrix3x4.m22;
			m23 = matrix3x4.m23;
			m30 = 0;
			m31 = 0;
			m32 = 0;
			m33 = 1;
		}

		#endregion

		#region Overrides
		public override int GetHashCode()
		{
			unchecked // Overflow is fine, just wrap
			{
				int hash = 17;

				hash = hash * 23 + this[0].GetHashCode();
				hash = hash * 23 + this[1].GetHashCode();
				hash = hash * 23 + this[2].GetHashCode();
				hash = hash * 23 + this[3].GetHashCode();

				return hash;
			}
		}

		public bool Equals(Matrix4x4 obj)
		{
			return MathHelpers.Approximately(m00 , obj.m00) && MathHelpers.Approximately(m01 , obj.m01) && MathHelpers.Approximately(m02 , obj.m02) && MathHelpers.Approximately(m03 , obj.m03)
				  && MathHelpers.Approximately(m10 , obj.m10) && MathHelpers.Approximately(m11 , obj.m11) && MathHelpers.Approximately(m12 , obj.m12) && MathHelpers.Approximately(m13 , obj.m13)
				  && MathHelpers.Approximately(m20 , obj.m20) && MathHelpers.Approximately(m21 , obj.m21) && MathHelpers.Approximately(m22 , obj.m22) && MathHelpers.Approximately(m23 , obj.m23)
				  && MathHelpers.Approximately(m30 , obj.m30) && MathHelpers.Approximately(m31 , obj.m31) && MathHelpers.Approximately(m32 , obj.m32) && MathHelpers.Approximately(m33 , obj.m33);
			
		}

		public override bool Equals(object obj)
		{
			if(!(obj is Matrix4x4))
			{
				return false;
			}

			return Equals((Matrix4x4)obj);
		}

		public override string ToString()
		{
			return string.Format(CultureInfo.CurrentCulture, "[{0}],[{1}],[{2}],[{3}]", this[0].ToString(), this[1].ToString(), this[2].ToString(), this[3].ToString());
		}

		#endregion

		#region Conversion
		public static implicit operator Matrix44(Matrix4x4 managedMatrix)
		{
			return new Matrix44(managedMatrix.m00, managedMatrix.m01, managedMatrix.m02, managedMatrix.m03,
								managedMatrix.m10, managedMatrix.m11, managedMatrix.m12, managedMatrix.m13,
								managedMatrix.m20, managedMatrix.m21, managedMatrix.m22, managedMatrix.m23,
								managedMatrix.m30, managedMatrix.m31, managedMatrix.m32, managedMatrix.m33);
		}

		public static implicit operator Matrix4x4(Matrix44 nativeMatrix)
		{
			if(nativeMatrix == null)
			{
				return new Matrix4x4();
			}

			return new Matrix4x4(nativeMatrix.m00, nativeMatrix.m01, nativeMatrix.m02, nativeMatrix.m03,
								nativeMatrix.m10, nativeMatrix.m11, nativeMatrix.m12, nativeMatrix.m13,
								nativeMatrix.m20, nativeMatrix.m21, nativeMatrix.m22, nativeMatrix.m23,
								nativeMatrix.m30, nativeMatrix.m31, nativeMatrix.m32, nativeMatrix.m33);
		}
		#endregion

		#region Methods
		public void SetZero()
		{
			m00 = 0;
			m01 = 0;
			m02 = 0;
			m03 = 0;

			m10 = 0;
			m11 = 0;
			m12 = 0;
			m13 = 0;

			m20 = 0;
			m21 = 0;
			m22 = 0;
			m23 = 0;

			m30 = 0;
			m31 = 0;
			m32 = 0;
			m33 = 0;
		}

		public void SetIdentity()
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

		public void Transpose()
		{
			var tmp = new Matrix4x4(this);

			m01 = tmp.m10;
			m02 = tmp.m20;
			m03 = tmp.m30;

			m10 = tmp.m01;
			m12 = tmp.m21;
			m13 = tmp.m31;

			m20 = tmp.m02;
			m21 = tmp.m12;
			m23 = tmp.m32;

			m30 = tmp.m03;
			m31 = tmp.m13;
			m32 = tmp.m23;
		}

		public Matrix4x4 GetTransposed()
		{
			var tmp = new Matrix4x4(this);

			tmp.m01 = m10;
			tmp.m02 = m20;
			tmp.m03 = m30;

			tmp.m10 = m01;
			tmp.m12 = m21;
			tmp.m13 = m31;

			tmp.m20 = m02;
			tmp.m21 = m12;
			tmp.m23 = m32;

			tmp.m30 = m03;
			tmp.m31 = m13;
			tmp.m32 = m23;
			return tmp;
		}

		public void Invert()
		{
			var tmp = new float[12];
			var m = new Matrix4x4(this);

			// Calculate pairs for first 8 elements (cofactors)
			tmp[0] = m.m22 * m.m33;
			tmp[1] = m.m32 * m.m23;
			tmp[2] = m.m12 * m.m33;
			tmp[3] = m.m32 * m.m13;
			tmp[4] = m.m12 * m.m23;
			tmp[5] = m.m22 * m.m13;
			tmp[6] = m.m02 * m.m33;
			tmp[7] = m.m32 * m.m03;
			tmp[8] = m.m02 * m.m23;
			tmp[9] = m.m22 * m.m03;
			tmp[10] = m.m02 * m.m13;
			tmp[11] = m.m12 * m.m03;

			// Calculate first 8 elements (cofactors)
			m00 = tmp[0] * m.m11 + tmp[3] * m.m21 + tmp[4] * m.m31;
			m00 -= tmp[1] * m.m11 + tmp[2] * m.m21 + tmp[5] * m.m31;
			m01 = tmp[1] * m.m01 + tmp[6] * m.m21 + tmp[9] * m.m31;
			m01 -= tmp[0] * m.m01 + tmp[7] * m.m21 + tmp[8] * m.m31;
			m02 = tmp[2] * m.m01 + tmp[7] * m.m11 + tmp[10] * m.m31;
			m02 -= tmp[3] * m.m01 + tmp[6] * m.m11 + tmp[11] * m.m31;
			m03 = tmp[5] * m.m01 + tmp[8] * m.m11 + tmp[11] * m.m21;
			m03 -= tmp[4] * m.m01 + tmp[9] * m.m11 + tmp[10] * m.m21;
			m10 = tmp[1] * m.m10 + tmp[2] * m.m20 + tmp[5] * m.m30;
			m10 -= tmp[0] * m.m10 + tmp[3] * m.m20 + tmp[4] * m.m30;
			m11 = tmp[0] * m.m00 + tmp[7] * m.m20 + tmp[8] * m.m30;
			m11 -= tmp[1] * m.m00 + tmp[6] * m.m20 + tmp[9] * m.m30;
			m12 = tmp[3] * m.m00 + tmp[6] * m.m10 + tmp[11] * m.m30;
			m12 -= tmp[2] * m.m00 + tmp[7] * m.m10 + tmp[10] * m.m30;
			m13 = tmp[4] * m.m00 + tmp[9] * m.m10 + tmp[10] * m.m20;
			m13 -= tmp[5] * m.m00 + tmp[8] * m.m10 + tmp[11] * m.m20;

			// Calculate pairs for second 8 elements (cofactors)
			tmp[0] = m.m20 * m.m31;
			tmp[1] = m.m30 * m.m21;
			tmp[2] = m.m10 * m.m31;
			tmp[3] = m.m30 * m.m11;
			tmp[4] = m.m10 * m.m21;
			tmp[5] = m.m20 * m.m11;
			tmp[6] = m.m00 * m.m31;
			tmp[7] = m.m30 * m.m01;
			tmp[8] = m.m00 * m.m21;
			tmp[9] = m.m20 * m.m01;
			tmp[10] = m.m00 * m.m11;
			tmp[11] = m.m10 * m.m01;

			// Calculate second 8 elements (cofactors)
			m20 = tmp[0] * m.m13 + tmp[3] * m.m23 + tmp[4] * m.m33;
			m20 -= tmp[1] * m.m13 + tmp[2] * m.m23 + tmp[5] * m.m33;
			m21 = tmp[1] * m.m03 + tmp[6] * m.m23 + tmp[9] * m.m33;
			m21 -= tmp[0] * m.m03 + tmp[7] * m.m23 + tmp[8] * m.m33;
			m22 = tmp[2] * m.m03 + tmp[7] * m.m13 + tmp[10] * m.m33;
			m22 -= tmp[3] * m.m03 + tmp[6] * m.m13 + tmp[11] * m.m33;
			m23 = tmp[5] * m.m03 + tmp[8] * m.m13 + tmp[11] * m.m23;
			m23 -= tmp[4] * m.m03 + tmp[9] * m.m13 + tmp[10] * m.m23;
			m30 = tmp[2] * m.m22 + tmp[5] * m.m32 + tmp[1] * m.m12;
			m30 -= tmp[4] * m.m32 + tmp[0] * m.m12 + tmp[3] * m.m22;
			m31 = tmp[8] * m.m32 + tmp[0] * m.m02 + tmp[7] * m.m22;
			m31 -= tmp[6] * m.m22 + tmp[9] * m.m32 + tmp[1] * m.m02;
			m32 = tmp[6] * m.m12 + tmp[11] * m.m32 + tmp[3] * m.m02;
			m32 -= tmp[10] * m.m32 + tmp[2] * m.m02 + tmp[7] * m.m12;
			m33 = tmp[10] * m.m22 + tmp[4] * m.m02 + tmp[9] * m.m12;
			m33 -= tmp[8] * m.m12 + tmp[11] * m.m22 + tmp[5] * m.m02;

			// Calculate determinant
			float det = (m.m00 * m00 + m.m10 * m01 + m.m20 * m02 + m.m30 * m03);

			// Divide the cofactor-matrix by the determinant
			float idet = 1.0f / det;
			m00 *= idet;
			m01 *= idet;
			m02 *= idet;
			m03 *= idet;
			m10 *= idet;
			m11 *= idet;
			m12 *= idet;
			m13 *= idet;
			m20 *= idet;
			m21 *= idet;
			m22 *= idet;
			m23 *= idet;
			m30 *= idet;
			m31 *= idet;
			m32 *= idet;
			m33 *= idet;
		}

		public Matrix4x4 GetInverted()
		{
			var dst = new Matrix4x4(this);
			dst.Invert();
			return dst;
		}

		public float Determinant()
		{
			//determinant is ambiguous: only the upper-left-submatrix's determinant is calculated
			return (m00 * m11 * m22) + (m01 * m12 * m20) + (m02 * m10 * m21) - (m02 * m11 * m20) - (m00 * m12 * m21) - (m01 * m10 * m22);
		}

		public Vector3 TransformVector(Vector3 vector)
		{
			var v = new Vector3();
			v.x = m00 * vector.x + m01 * vector.y + m02 * vector.z;
			v.y = m10 * vector.x + m11 * vector.y + m12 * vector.z;
			v.z = m20 * vector.x + m21 * vector.y + m22 * vector.z;
			return v;
		}

		public Vector3 TransformPoint(Vector3 point)
		{
			var v = new Vector3();
			v.x = m00 * point.x + m01 * point.y + m02 * point.z + m03;
			v.y = m10 * point.x + m11 * point.y + m12 * point.z + m13;
			v.z = m20 * point.x + m21 * point.y + m22 * point.z + m23;
			return v;
		}

		public void SetColumn(int i, Vector3 vec)
		{
			switch(i)
			{
			case 0:
				{
					m00 = vec.x;
					m10 = vec.y;
					m20 = vec.z;
					break;
				}

			case 1:
				{
					m01 = vec.x;
					m11 = vec.y;
					m21 = vec.z;
					break;
				}
			case 2:
				{
					m02 = vec.x;
					m12 = vec.y;
					m22 = vec.z;
					break;
				}
			case 3:
				{
					m03 = vec.x;
					m13 = vec.y;
					m23 = vec.z;
					break;
				}
			}
		}

		public void SetColumn(int i, Vector4 vec)
		{
			switch(i)
			{
			case 0:
				{
					m00 = vec.x;
					m10 = vec.y;
					m20 = vec.z;
					m30 = vec.w;
					break;
				}

			case 1:
				{
					m01 = vec.x;
					m11 = vec.y;
					m21 = vec.z;
					m31 = vec.w;
					break;
				}
			case 2:
				{
					m02 = vec.x;
					m12 = vec.y;
					m22 = vec.z;
					m32 = vec.w;
					break;
				}
			case 3:
				{
					m03 = vec.x;
					m13 = vec.y;
					m23 = vec.z;
					m33 = vec.w;
					break;
				}
			}
		}

		public void GetColumn(int i, ref Vector4 vector)
		{
			switch(i)
			{
			case 0:
				{
					vector.x = m00;
					vector.y = m10;
					vector.z = m20;
					vector.w = m30;
					break;
				}

			case 1:
				{
					vector.x = m01;
					vector.y = m11;
					vector.z = m21;
					vector.w = m31;
					break;
				}
			case 2:
				{
					vector.x = m02;
					vector.y = m12;
					vector.z = m22;
					vector.w = m32;
					break;
				}
			case 3:
				{
					vector.x = m03;
					vector.y = m13;
					vector.z = m23;
					vector.w = m33;
					break;
				}
			}
		}
		public Vector3 GetTranslation()
		{
			return new Vector3(m03, m13, m23);
		}

		public void GetTranslation(ref Vector3 vec)
		{
			vec.x = m03;
			vec.y = m13;
			vec.z = m23;
		}

		public void SetTranslation(Vector3 vec)
		{
			m03 = vec.x;
			m13 = vec.y;
			m23 = vec.z;
		}

		static bool IsEquivalent(Matrix4x4 m0, Matrix4x4 m1, float epsilon = 0.05f)
		{
			return
				(m0.m00 - m1.m00) <= epsilon && (m0.m01 - m1.m01 <= epsilon) && (m0.m02 - m1.m02 <= epsilon) && (m0.m03 - m1.m03 <= epsilon) &&
				(m0.m10 - m1.m10 <= epsilon) && (m0.m11 - m1.m11 <= epsilon) && (m0.m12 - m1.m12 <= epsilon) && (m0.m13 - m1.m13 <= epsilon) &&
				(m0.m20 - m1.m20 <= epsilon) && (m0.m21 - m1.m21 <= epsilon) && (m0.m22 - m1.m22 <= epsilon) && (m0.m23 - m1.m23 <= epsilon) &&
				(m0.m30 - m1.m30 <= epsilon) && (m0.m31 - m1.m31 <= epsilon) && (m0.m32 - m1.m32 <= epsilon) && (m0.m33 - m1.m33 <= epsilon);
		}

		#endregion

		#region Operators
		public static bool operator ==(Matrix4x4 lhs, Matrix4x4 rhs)
		{
			return lhs.Equals(rhs);
		}

		public static bool operator !=(Matrix4x4 lhs, Matrix4x4 rhs)
		{
			return !(lhs == rhs);
		}

		public static Matrix4x4 operator *(Matrix4x4 matrix4x4, float multiplier)
		{
			var ret = new Matrix4x4(matrix4x4);
			ret.m00 = matrix4x4.m00 * multiplier;
			ret.m01 = matrix4x4.m01 * multiplier;
			ret.m02 = matrix4x4.m02 * multiplier;
			ret.m03 = matrix4x4.m03 * multiplier;
			ret.m10 = matrix4x4.m10 * multiplier;
			ret.m11 = matrix4x4.m11 * multiplier;
			ret.m12 = matrix4x4.m12 * multiplier;
			ret.m13 = matrix4x4.m13 * multiplier;
			ret.m20 = matrix4x4.m20 * multiplier;
			ret.m21 = matrix4x4.m21 * multiplier;
			ret.m22 = matrix4x4.m22 * multiplier;
			ret.m23 = matrix4x4.m23 * multiplier;
			ret.m30 = matrix4x4.m30 * multiplier;
			ret.m31 = matrix4x4.m31 * multiplier;
			ret.m32 = matrix4x4.m32 * multiplier;
			ret.m33 = matrix4x4.m33 * multiplier;
			return ret;
		}

		public static Matrix4x4 operator +(Matrix4x4 lhs, Matrix4x4 rhs)
		{
			var ret = new Matrix4x4();
			ret.m00 = lhs.m00 + rhs.m00;
			ret.m01 = lhs.m01 + rhs.m01;
			ret.m02 = lhs.m02 + rhs.m02;
			ret.m03 = lhs.m03 + rhs.m03;
			ret.m10 = lhs.m10 + rhs.m10;
			ret.m11 = lhs.m11 + rhs.m11;
			ret.m12 = lhs.m12 + rhs.m12;
			ret.m13 = lhs.m13 + rhs.m13;
			ret.m20 = lhs.m20 + rhs.m20;
			ret.m21 = lhs.m21 + rhs.m21;
			ret.m22 = lhs.m22 + rhs.m22;
			ret.m23 = lhs.m23 + rhs.m23;
			ret.m30 = lhs.m30 + rhs.m30;
			ret.m31 = lhs.m31 + rhs.m31;
			ret.m32 = lhs.m32 + rhs.m32;
			ret.m33 = lhs.m33 + rhs.m33;
			return ret;
		}

		public static Matrix4x4 operator *(Matrix4x4 l, Matrix4x4 r)
		{

			var res = new Matrix4x4();
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

		//Post-multiply.
		public static Vector4 operator *(Matrix4x4 m, Vector4 v)
		{
			return new Vector4(v.x * m.m00 + v.y * m.m01 + v.z * m.m02 + v.w * m.m03,
												v.x * m.m10 + v.y * m.m11 + v.z * m.m12 + v.w * m.m13,
												v.x * m.m20 + v.y * m.m21 + v.z * m.m22 + v.w * m.m23,
												v.x * m.m30 + v.y * m.m31 + v.z * m.m32 + v.w * m.m33);
		}

		public static Vector4 operator *(Vector4 v, Matrix4x4 m)
		{
			return new Vector4(v.x * m.m00 + v.y * m.m10 + v.z * m.m20 + v.w * m.m30,
												v.x * m.m01 + v.y * m.m11 + v.z * m.m21 + v.w * m.m31,
												v.x * m.m02 + v.y * m.m12 + v.z * m.m22 + v.w * m.m32,
												v.x * m.m03 + v.y * m.m13 + v.z * m.m23 + v.w * m.m33);
		}

		public static Matrix4x4 operator *(Matrix4x4 l, Matrix3x3 r)
		{
			var result = new Matrix4x4();
			result.m00 = l.m00 * r.m00 + l.m01 * r.m10 + l.m02 * r.m20;
			result.m10 = l.m10 * r.m00 + l.m11 * r.m10 + l.m12 * r.m20;
			result.m20 = l.m20 * r.m00 + l.m21 * r.m10 + l.m22 * r.m20;
			result.m30 = l.m30 * r.m00 + l.m31 * r.m10 + l.m32 * r.m20;
			result.m01 = l.m00 * r.m01 + l.m01 * r.m11 + l.m02 * r.m21;
			result.m11 = l.m10 * r.m01 + l.m11 * r.m11 + l.m12 * r.m21;
			result.m21 = l.m20 * r.m01 + l.m21 * r.m11 + l.m22 * r.m21;
			result.m31 = l.m30 * r.m01 + l.m31 * r.m11 + l.m32 * r.m21;
			result.m02 = l.m00 * r.m02 + l.m01 * r.m12 + l.m02 * r.m22;
			result.m12 = l.m10 * r.m02 + l.m11 * r.m12 + l.m12 * r.m22;
			result.m22 = l.m20 * r.m02 + l.m21 * r.m12 + l.m22 * r.m22;
			result.m32 = l.m30 * r.m02 + l.m31 * r.m12 + l.m32 * r.m22;
			result.m03 = l.m03;
			result.m13 = l.m13;
			result.m23 = l.m23;
			result.m33 = l.m33;
			return result;
		}

		#endregion

		#region Properties
		public Vector4 this[int row]
		{
			get
			{
				switch(row)
				{
				case 0:
					return new Vector4(m00, m01, m02, m03);

				case 1:
					return new Vector4(m10, m11, m12, m13);
					
				case 2:
					return new Vector4(m20, m21, m22, m23);
					
				case 3:
					return new Vector4(m30, m31, m32, m33);

				default:
					throw new ArgumentOutOfRangeException(nameof(row), "Row must run from 0 to 3!");
				}
			}
			set
			{
				switch(row)
				{
				case 0:
					m00 = value.x;
					m01 = value.y;
					m02 = value.z;
					m03 = value.w;
					break;

				case 1:
					m10 = value.x;
					m11 = value.y;
					m12 = value.z;
					m13 = value.w;
					break;

				case 2:
					m20 = value.x;
					m21 = value.y;
					m22 = value.z;
					m23 = value.w;
					break;

				case 3:
					m30 = value.x;
					m31 = value.y;
					m32 = value.z;
					m33 = value.w;
					break;

				default:
					throw new ArgumentOutOfRangeException(nameof(row), "Row must run from 0 to 3!");
				}
			}
		}

		public float this[int row, int column]
		{
			get
			{
				if(row < 0 || row > 3)
				{
					throw new ArgumentOutOfRangeException(nameof(row), "Row must run from 0 to 3!");
				}

				switch(column)
				{
				case 0:
					return this[row].x;

				case 1:
					return this[row].y;

				case 2:
					return this[row].z;

				case 3:
					return this[row].w;

				default:
					throw new ArgumentOutOfRangeException(nameof(column), "Column must run from 0 to 3!");
				}
			}
			set
			{
				if(row < 0 || row > 3)
				{
					throw new ArgumentOutOfRangeException(nameof(row), "Row must run from 0 to 3!");
				}

				Vector4 rowValue = this[row];
				switch(column)
				{
				case 0:
					{
						rowValue.x = value;
						this[row] = rowValue;
						break;
					}
				case 1:
					{
						rowValue.y = value;
						this[row] = rowValue;
						break;
					}
				case 2:
					{
						rowValue.z = value;
						this[row] = rowValue;
						break;
					}
				case 3:
					{
						rowValue.w = value;
						this[row] = rowValue;
						break;
					}
				default:
					throw new ArgumentOutOfRangeException(nameof(column), "Column must run from 0 to 3!");
				}
			}
		}
		#endregion
	}
}
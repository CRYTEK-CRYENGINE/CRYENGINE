// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.


using System;
using System.Globalization;
using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// DataType that represents a 4 by 4 matrix of <see cref="float"/> values.
	/// </summary>
	public struct Matrix4x4
	{
		/// <summary>
		/// Identity matrix for the <see cref="Matrix4x4"/>.
		/// </summary>
		public static readonly Matrix4x4 Identity = new Matrix4x4(1f, 0f, 0f, 0f,
		                                                          0f, 1f, 0f, 0f,
		                                                          0f, 0f, 1f, 0f,
		                                                          0f, 0f, 0f, 1f);

		private float m00, m01, m02, m03;
		private float m10, m11, m12, m13;
		private float m20, m21, m22, m23;
		private float m30, m31, m32, m33;

		#region Constructors
		/// <summary>
		/// Create a new matrix from the specified floats.
		/// </summary>
		/// <param name="p00"></param>
		/// <param name="p01"></param>
		/// <param name="p02"></param>
		/// <param name="p03"></param>
		/// <param name="p10"></param>
		/// <param name="p11"></param>
		/// <param name="p12"></param>
		/// <param name="p13"></param>
		/// <param name="p20"></param>
		/// <param name="p21"></param>
		/// <param name="p22"></param>
		/// <param name="p23"></param>
		/// <param name="p30"></param>
		/// <param name="p31"></param>
		/// <param name="p32"></param>
		/// <param name="p33"></param>
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

		/// <summary>
		/// Create a new <see cref="Matrix4x4"/> from a <see cref="Matrix3x3"/>.
		/// </summary>
		/// <param name="matrix3x3"></param>
		public Matrix4x4(Matrix3x3 matrix3x3)
		{
			m00 = matrix3x3[0, 0];
			m01 = matrix3x3[0, 1];
			m02 = matrix3x3[0, 2];
			m03 = 0;
			m10 = matrix3x3[1, 0];
			m11 = matrix3x3[1, 1];
			m12 = matrix3x3[1, 2];
			m13 = 0;
			m20 = matrix3x3[2, 0];
			m21 = matrix3x3[2, 1];
			m22 = matrix3x3[2, 2];
			m23 = 0;
			m30 = 0;
			m31 = 0;
			m32 = 0;
			m33 = 1;
		}

		/// <summary>
		/// Create a new <see cref="Matrix4x4"/> from a <see cref="Matrix3x4"/>.
		/// </summary>
		/// <param name="matrix3x4"></param>
		public Matrix4x4(Matrix3x4 matrix3x4)
		{
			m00 = matrix3x4[0, 0];
			m01 = matrix3x4[0, 1];
			m02 = matrix3x4[0, 2];
			m03 = matrix3x4[0, 3];
			m10 = matrix3x4[1, 0];
			m11 = matrix3x4[1, 1];
			m12 = matrix3x4[1, 2];
			m13 = matrix3x4[1, 3];
			m20 = matrix3x4[2, 0];
			m21 = matrix3x4[2, 1];
			m22 = matrix3x4[2, 2];
			m23 = matrix3x4[2, 3];
			m30 = 0;
			m31 = 0;
			m32 = 0;
			m33 = 1;
		}

		#endregion

		#region Overrides

		/// <summary>
		/// Converts this instance to a nicely formatted string.
		/// Example: [0.123, 0.123, 0.123, 0.123],[0.123, 0.123, 0.123, 0.123],[0.123, 0.123, 0.123, 0.123],[0.123, 0.123, 0.123, 0.123]
		/// </summary>
		/// <returns></returns>
		public override string ToString()
		{
			return string.Format(CultureInfo.CurrentCulture, "[{0}],[{1}],[{2}],[{3}]", this[0].ToString(), this[1].ToString(), this[2].ToString(), this[3].ToString());
		}

		#endregion

		#region Conversion
		/// <summary>
		/// Implicitly convert a managed <see cref="Matrix4x4"/> to an unmanaged <see cref="Matrix44"/>.
		/// </summary>
		/// <param name="managedMatrix"></param>
		public static implicit operator Matrix44(Matrix4x4 managedMatrix)
		{
			return new Matrix44(managedMatrix.m00, managedMatrix.m01, managedMatrix.m02, managedMatrix.m03,
								managedMatrix.m10, managedMatrix.m11, managedMatrix.m12, managedMatrix.m13,
								managedMatrix.m20, managedMatrix.m21, managedMatrix.m22, managedMatrix.m23,
								managedMatrix.m30, managedMatrix.m31, managedMatrix.m32, managedMatrix.m33);
		}

		/// <summary>
		/// Implicitly convert an unmanaged <see cref="Matrix44"/> to a managed <see cref="Matrix4x4"/>.
		/// </summary>
		/// <param name="nativeMatrix"></param>
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

		/// <summary>
		/// Set all values of this matrix to zero.
		/// </summary>
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

		/// <summary>
		/// Set this matrix to the values of the identity.
		/// </summary>
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

		/// <summary>
		/// Flips the matrix over its diagonal, that is it switches the row and column indices of the matrix.
		/// </summary>
		public void Transpose()
		{
			var tmp = this;

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

		/// <summary>
		/// Returns a matrix that is flipped diagonally. That is it switches the row and column indices of the matrix.
		/// </summary>
		/// <returns></returns>
		public Matrix4x4 GetTransposed()
		{
			var tmp = this;

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

		/// <summary>
		/// Invert this matrix.
		/// </summary>
		public void Invert()
		{
			var tmp = new float[12];
			var m = this;

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

		/// <summary>
		/// Returns this matrix but inverted.
		/// </summary>
		/// <returns></returns>
		public Matrix4x4 GetInverted()
		{
			var dst = this;
			dst.Invert();
			return dst;
		}

		/// <summary>
		/// Returns the determinant of this matrix.
		/// </summary>
		/// <returns></returns>
		public float Determinant()
		{
			//determinant is ambiguous: only the upper-left-submatrix's determinant is calculated
			return (m00 * m11 * m22) + (m01 * m12 * m20) + (m02 * m10 * m21) - (m02 * m11 * m20) - (m00 * m12 * m21) - (m01 * m10 * m22);
		}

		/// <summary>
		/// Converts a direction vector to the orientation of this matrix.
		/// </summary>
		/// <param name="vector"></param>
		/// <returns></returns>
		public Vector3 TransformVector(Vector3 vector)
		{
			var v = new Vector3();
			v.x = m00 * vector.x + m01 * vector.y + m02 * vector.z;
			v.y = m10 * vector.x + m11 * vector.y + m12 * vector.z;
			v.z = m20 * vector.x + m21 * vector.y + m22 * vector.z;
			return v;
		}

		/// <summary>
		/// Converts a point to the orientation of this matrix.
		/// </summary>
		/// <param name="point"></param>
		/// <returns></returns>
		public Vector3 TransformPoint(Vector3 point)
		{
			var v = new Vector3();
			v.x = m00 * point.x + m01 * point.y + m02 * point.z + m03;
			v.y = m10 * point.x + m11 * point.y + m12 * point.z + m13;
			v.z = m20 * point.x + m21 * point.y + m22 * point.z + m23;
			return v;
		}

		/// <summary>
		/// Set the specified column.
		/// </summary>
		/// <param name="i"></param>
		/// <param name="vec"></param>
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

		/// <summary>
		/// Set the specified column.
		/// </summary>
		/// <param name="i"></param>
		/// <param name="vec"></param>
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

		/// <summary>
		/// Returns the specified column.
		/// </summary>
		/// <param name="i"></param>
		/// <param name="vector"></param>
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

		/// <summary>
		/// Get the translation of this matrix.
		/// </summary>
		/// <returns></returns>
		public Vector3 GetTranslation()
		{
			return new Vector3(m03, m13, m23);
		}

		/// <summary>
		/// Get the translation of this matrix.
		/// </summary>
		/// <param name="vec"></param>
		public void GetTranslation(ref Vector3 vec)
		{
			vec.x = m03;
			vec.y = m13;
			vec.z = m23;
		}

		/// <summary>
		/// Set the translation of this matrix.
		/// </summary>
		/// <param name="vec"></param>
		public void SetTranslation(Vector3 vec)
		{
			m03 = vec.x;
			m13 = vec.y;
			m23 = vec.z;
		}

		/// <summary>
		/// Determines if two matrices are almost the same, with <paramref name="epsilon"/> being the allowed difference per value.
		/// </summary>
		/// <param name="m0"></param>
		/// <param name="m1"></param>
		/// <param name="epsilon"></param>
		/// <returns></returns>
		static bool IsEquivalent(Matrix4x4 m0, Matrix4x4 m1, float epsilon = 0.05f)
		{
			return (m0.m00 - m1.m00) <= epsilon && (m0.m01 - m1.m01 <= epsilon) && (m0.m02 - m1.m02 <= epsilon) && (m0.m03 - m1.m03 <= epsilon) &&
			       (m0.m10 - m1.m10 <= epsilon) && (m0.m11 - m1.m11 <= epsilon) && (m0.m12 - m1.m12 <= epsilon) && (m0.m13 - m1.m13 <= epsilon) &&
			       (m0.m20 - m1.m20 <= epsilon) && (m0.m21 - m1.m21 <= epsilon) && (m0.m22 - m1.m22 <= epsilon) && (m0.m23 - m1.m23 <= epsilon) &&
			       (m0.m30 - m1.m30 <= epsilon) && (m0.m31 - m1.m31 <= epsilon) && (m0.m32 - m1.m32 <= epsilon) && (m0.m33 - m1.m33 <= epsilon);
		}

		/// <summary>
		/// Determines if the <paramref name="obj"/> is a <see cref="Matrix4x4"/> and if the values are the same.
		/// </summary>
		/// <param name="obj"></param>
		/// <returns></returns>
		public override bool Equals(object obj)
		{
			if(!(obj is Matrix4x4))
			{
				return false;
			}

			var matrix = (Matrix4x4)obj;
			return m00 == matrix.m00 &&
			       m01 == matrix.m01 &&
			       m02 == matrix.m02 &&
			       m03 == matrix.m03 &&
			       m10 == matrix.m10 &&
			       m11 == matrix.m11 &&
			       m12 == matrix.m12 &&
			       m13 == matrix.m13 &&
			       m20 == matrix.m20 &&
			       m21 == matrix.m21 &&
			       m22 == matrix.m22 &&
			       m23 == matrix.m23 &&
			       m30 == matrix.m30 &&
			       m31 == matrix.m31 &&
			       m32 == matrix.m32 &&
			       m33 == matrix.m33;
		}

		/// <summary>
		/// Returns the hash code for this instance.
		/// </summary>
		/// <returns></returns>
		public override int GetHashCode()
		{
			var hashCode = 1860550904;
			hashCode = hashCode * -1521134295 + base.GetHashCode();
			hashCode = hashCode * -1521134295 + m00.GetHashCode();
			hashCode = hashCode * -1521134295 + m01.GetHashCode();
			hashCode = hashCode * -1521134295 + m02.GetHashCode();
			hashCode = hashCode * -1521134295 + m03.GetHashCode();
			hashCode = hashCode * -1521134295 + m10.GetHashCode();
			hashCode = hashCode * -1521134295 + m11.GetHashCode();
			hashCode = hashCode * -1521134295 + m12.GetHashCode();
			hashCode = hashCode * -1521134295 + m13.GetHashCode();
			hashCode = hashCode * -1521134295 + m20.GetHashCode();
			hashCode = hashCode * -1521134295 + m21.GetHashCode();
			hashCode = hashCode * -1521134295 + m22.GetHashCode();
			hashCode = hashCode * -1521134295 + m23.GetHashCode();
			hashCode = hashCode * -1521134295 + m30.GetHashCode();
			hashCode = hashCode * -1521134295 + m31.GetHashCode();
			hashCode = hashCode * -1521134295 + m32.GetHashCode();
			hashCode = hashCode * -1521134295 + m33.GetHashCode();
			return hashCode;
		}

		#endregion

		#region Operators

		/// <summary>
		/// Determines if the two matrices are the same.
		/// </summary>
		/// <param name="m1"></param>
		/// <param name="m2"></param>
		/// <returns></returns>
		public static bool operator ==(Matrix4x4 m1, Matrix4x4 m2)
		{
			return m1.Equals(m2);
		}

		/// <summary>
		/// Determines if the two matrices are not the same.
		/// </summary>
		/// <param name="m1"></param>
		/// <param name="m2"></param>
		/// <returns></returns>
		public static bool operator !=(Matrix4x4 m1, Matrix4x4 m2)
		{
			return !m1.Equals(m2);
		}

		/// <summary>
		/// Multiply the matrix by a float.
		/// </summary>
		/// <param name="matrix4x4"></param>
		/// <param name="multiplier"></param>
		/// <returns></returns>
		public static Matrix4x4 operator *(Matrix4x4 matrix4x4, float multiplier)
		{
			var ret = matrix4x4;
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

		/// <summary>
		/// Adds two matrices together.
		/// </summary>
		/// <param name="lhs"></param>
		/// <param name="rhs"></param>
		/// <returns></returns>
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

		/// <summary>
		/// Multiplies two matrices.
		/// </summary>
		/// <param name="l"></param>
		/// <param name="r"></param>
		/// <returns></returns>
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

		/// <summary>
		/// Transforms the vector by the matrix.
		/// </summary>
		/// <param name="m"></param>
		/// <param name="v"></param>
		/// <returns></returns>
		public static Vector4 operator *(Matrix4x4 m, Vector4 v)
		{
			return new Vector4(v.x * m.m00 + v.y * m.m01 + v.z * m.m02 + v.w * m.m03,
			                   v.x * m.m10 + v.y * m.m11 + v.z * m.m12 + v.w * m.m13,
			                   v.x * m.m20 + v.y * m.m21 + v.z * m.m22 + v.w * m.m23,
			                   v.x * m.m30 + v.y * m.m31 + v.z * m.m32 + v.w * m.m33);
		}

		/// <summary>
		/// Transforms the vector by the matrix.
		/// </summary>
		/// <param name="v"></param>
		/// <param name="m"></param>
		/// <returns></returns>
		public static Vector4 operator *(Vector4 v, Matrix4x4 m)
		{
			return new Vector4(v.x * m.m00 + v.y * m.m10 + v.z * m.m20 + v.w * m.m30,
			                   v.x * m.m01 + v.y * m.m11 + v.z * m.m21 + v.w * m.m31,
			                   v.x * m.m02 + v.y * m.m12 + v.z * m.m22 + v.w * m.m32,
			                   v.x * m.m03 + v.y * m.m13 + v.z * m.m23 + v.w * m.m33);
		}

		/// <summary>
		/// Multiplies two matrices.
		/// </summary>
		/// <param name="l"></param>
		/// <param name="r"></param>
		/// <returns></returns>
		public static Matrix4x4 operator *(Matrix4x4 l, Matrix3x3 r)
		{
			var result = new Matrix4x4();
			result.m00 = l.m00 * r[0, 0] + l.m01 * r[1, 0] + l.m02 * r[2, 0];
			result.m10 = l.m10 * r[0, 0] + l.m11 * r[1, 0] + l.m12 * r[2, 0];
			result.m20 = l.m20 * r[0, 0] + l.m21 * r[1, 0] + l.m22 * r[2, 0];
			result.m30 = l.m30 * r[0, 0] + l.m31 * r[1, 0] + l.m32 * r[2, 0];
			result.m01 = l.m00 * r[0, 1] + l.m01 * r[1, 1] + l.m02 * r[2, 1];
			result.m11 = l.m10 * r[0, 1] + l.m11 * r[1, 1] + l.m12 * r[2, 1];
			result.m21 = l.m20 * r[0, 1] + l.m21 * r[1, 1] + l.m22 * r[2, 1];
			result.m31 = l.m30 * r[0, 1] + l.m31 * r[1, 1] + l.m32 * r[2, 1];
			result.m02 = l.m00 * r[0, 2] + l.m01 * r[1, 2] + l.m02 * r[2, 2];
			result.m12 = l.m10 * r[0, 2] + l.m11 * r[1, 2] + l.m12 * r[2, 2];
			result.m22 = l.m20 * r[0, 2] + l.m21 * r[1, 2] + l.m22 * r[2, 2];
			result.m32 = l.m30 * r[0, 2] + l.m31 * r[1, 2] + l.m32 * r[2, 2];
			result.m03 = l.m03;
			result.m13 = l.m13;
			result.m23 = l.m23;
			result.m33 = l.m33;
			return result;
		}

		/// <summary>
		/// Get or set a row on the matrix.
		/// </summary>
		/// <param name="row"></param>
		/// <returns></returns>
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
					throw new ArgumentOutOfRangeException(nameof(row), string.Format("The value for {0} cannot be smaller than 0 or bigger than 3!", nameof(row)));
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
					throw new ArgumentOutOfRangeException(nameof(row), string.Format("The value for {0} cannot be smaller than 0 or bigger than 3!", nameof(row)));
				}
			}
		}

		/// <summary>
		/// Get or set the value at a specific row and column.
		/// </summary>
		/// <param name="row"></param>
		/// <param name="column"></param>
		/// <returns></returns>
		public float this[int row, int column]
		{
			get
			{
				if(row < 0 || row > 3)
				{
					throw new ArgumentOutOfRangeException(nameof(row), string.Format("The value for {0} cannot be smaller than 0 or bigger than 3!", nameof(row)));
				}

				if(column < 0 || column > 3)
				{
					throw new ArgumentOutOfRangeException(nameof(column), string.Format("The value for {0} cannot be smaller than 0 or bigger than 3!", nameof(column)));
				}

				switch(row)
				{
				case 0:
					switch(column)
					{
					case 0:
						return m00;
					case 1:
						return m01;
					case 2:
						return m02;
					case 3:
						return m03;
					}
					break;
				case 1:
					switch(column)
					{
					case 0:
						return m10;
					case 1:
						return m11;
					case 2:
						return m12;
					case 3:
						return m13;
					}
					break;
				case 2:
					switch(column)
					{
					case 0:
						return m20;
					case 1:
						return m21;
					case 2:
						return m22;
					case 3:
						return m23;
					}
					break;
				case 3:
					switch(column)
					{
					case 0:
						return m30;
					case 1:
						return m31;
					case 2:
						return m32;
					case 3:
						return m33;
					}
					break;
				}

				// This should never be able to happen, since the out of range index is already caught at the top.
				throw new ArgumentOutOfRangeException(nameof(row));
			}
			set
			{
				if(row < 0 || row > 3)
				{
					throw new ArgumentOutOfRangeException(nameof(row), string.Format("The value for {0} cannot be smaller than 0 or bigger than 3!", nameof(row)));
				}

				if(column < 0 || column > 2)
				{
					throw new ArgumentOutOfRangeException(nameof(column), string.Format("The value for {0} cannot be smaller than 0 or bigger than 2!", nameof(column)));
				}

				switch(row)
				{
				case 0:
					switch(column)
					{
					case 0:
						m00 = value;
						return;
					case 1:
						m01 = value;
						return;
					case 2:
						m02 = value;
						return;
					case 3:
						m03 = value;
						return;
					}
					return;
				case 1:
					switch(column)
					{
					case 0:
						m10 = value;
						return;
					case 1:
						m11 = value;
						return;
					case 2:
						m12 = value;
						return;
					case 3:
						m13 = value;
						return;
					}
					return;
				case 2:
					switch(column)
					{
					case 0:
						m20 = value;
						return;
					case 1:
						m21 = value;
						return;
					case 2:
						m22 = value;
						return;
					case 3:
						m23 = value;
						return;
					}
					return;
				case 3:
					switch(column)
					{
					case 0:
						m30 = value;
						return;
					case 1:
						m31 = value;
						return;
					case 2:
						m32 = value;
						return;
					case 3:
						m33 = value;
						return;
					}
					return;
				}
			}
		}
		#endregion
	}
}
// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using System;
using System.Globalization;

using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// DataType that represents a 3 by 3 matrix of <see cref="float"/> values, mostly used for rotations.
	/// </summary>
	public struct Matrix3x3
	{
		/// <summary>
		/// A default matrix that doesn't apply any rotation.
		/// </summary>
		public static readonly Matrix3x3 Identity = new Matrix3x3
		(
			1f, 0f, 0f,
			0f, 1f, 0f,
			0f, 0f, 1f
		);

		private float m00, m01, m02;
		private float m10, m11, m12;
		private float m20, m21, m22;

		#region Constructors

		/// <summary>
		/// Construct a new matrix with the specified float values.
		/// </summary>
		/// <param name="p00"></param>
		/// <param name="p01"></param>
		/// <param name="p02"></param>
		/// <param name="p10"></param>
		/// <param name="p11"></param>
		/// <param name="p12"></param>
		/// <param name="p20"></param>
		/// <param name="p21"></param>
		/// <param name="p22"></param>
		public Matrix3x3(float p00, float p01, float p02,
		                 float p10, float p11, float p12,
		                 float p20, float p21, float p22)
		{
			m00 = p00;
			m01 = p01;
			m02 = p02;

			m10 = p10;
			m11 = p11;
			m12 = p12;

			m20 = p20;
			m21 = p21;
			m22 = p22;
		}

		/// <summary>
		/// It initializes a <see cref="Matrix3x3"/> with 3 vectors stored in the columns.
		/// Matrix33(v0,v1,v2);
		/// </summary>
		/// <param name="col1"></param>
		/// <param name="col2"></param>
		/// <param name="col3"></param>
		public Matrix3x3(Vector3 col1, Vector3 col2, Vector3 col3)
		{
			m00 = col1.x;
			m01 = col2.x;
			m02 = col3.x;

			m10 = col1.y;
			m11 = col2.y;
			m12 = col3.y;

			m20 = col1.z;
			m21 = col2.z;
			m22 = col3.z;
		}

		/// <summary>
		/// It converts a <see cref="Matrix4x4"/> into a <see cref="Matrix3x3"/>. Loses the translation and scale vector in the conversion process.
		/// </summary>
		/// <param name="matrix"></param>
		public Matrix3x3(Matrix4x4 matrix)
		{
			m00 = matrix[0, 0];
			m01 = matrix[0, 1];
			m02 = matrix[0, 2];
			m10 = matrix[1, 0];
			m11 = matrix[1, 1];
			m12 = matrix[1, 2];
			m20 = matrix[2, 0];
			m21 = matrix[2, 1];
			m22 = matrix[2, 2];
		}

		/// <summary>
		/// It converts a <see cref="Matrix3x4"/> into a <see cref="Matrix3x3"/>. Loses translation vector in the conversion process.
		/// </summary>
		/// <param name="matrix"></param>
		public Matrix3x3(Matrix3x4 matrix)
		{
			m00 = matrix[0, 0];
			m01 = matrix[0, 1];
			m02 = matrix[0, 2];
			m10 = matrix[1, 0];
			m11 = matrix[1, 1];
			m12 = matrix[1, 2];
			m20 = matrix[2, 0];
			m21 = matrix[2, 1];
			m22 = matrix[2, 2];
		}

		/// <summary>
		/// Create a new <see cref="Matrix3x3"/> from a <see cref="Quaternion"/>.
		/// </summary>
		/// <param name="quat"></param>
		public Matrix3x3(Quaternion quat)
		{
			var v2 = quat.v + quat.v;
			float xx = 1 - v2.x * quat.v.x;
			float yy = v2.y * quat.v.y;
			float xw = v2.x * quat.w;
			float xy = v2.y * quat.v.x;
			float yz = v2.z * quat.v.y;
			float yw = v2.y * quat.w;
			float xz = v2.z * quat.v.x;
			float zz = v2.z * quat.v.z;
			float zw = v2.z * quat.w;
			m00 = 1 - yy - zz;
			m01 = xy - zw;
			m02 = xz + yw;
			m10 = xy + zw;
			m11 = xx - zz;
			m12 = yz - xw;
			m20 = xz - yw;
			m21 = yz + xw;
			m22 = xx - yy;
		}

		#endregion

		#region Conversion
		/// <summary>
		/// Implicitly converts a managed <see cref="Matrix3x3"/> to an unmanaged <see cref="Matrix33"/>
		/// </summary>
		/// <param name="managedMatrix"></param>
		public static implicit operator Matrix33(Matrix3x3 managedMatrix)
		{
			return new Matrix33(managedMatrix.m00, managedMatrix.m01, managedMatrix.m02,
			                    managedMatrix.m10, managedMatrix.m11, managedMatrix.m12,
			                    managedMatrix.m20, managedMatrix.m21, managedMatrix.m22);
		}

		/// <summary>
		/// Implicitly converts an unmanaged <see cref="Matrix33"/> to a managed <see cref="Matrix3x3"/>
		/// </summary>
		/// <param name="nativeMatrix"></param>
		public static implicit operator Matrix3x3(Matrix33 nativeMatrix)
		{
			if(nativeMatrix == null)
			{
				return new Matrix3x3();
			}

			return new Matrix3x3(nativeMatrix.m00, nativeMatrix.m01, nativeMatrix.m02,
			                     nativeMatrix.m10, nativeMatrix.m11, nativeMatrix.m12,
			                     nativeMatrix.m20, nativeMatrix.m21, nativeMatrix.m22);
		}
		#endregion

		#region Overrides
		/// <summary>
		/// Converts this instance to a nicely formatted string.
		/// Example: [0.123, 0.123, 0.123],[0.123, 0.123, 0.123],[0.123, 0.123, 0.123]
		/// </summary>
		/// <returns></returns>
		public override string ToString()
		{
			return string.Format(CultureInfo.CurrentCulture, "[{0}],[{1}],[{2}]", this[0], this[1], this[2]);
		}

		#endregion

		#region Methods

		/// <summary>
		/// Sets all values of this matrix to zero.
		/// </summary>
		public void SetZero()
		{
			m00 = 0;
			m01 = 0;
			m02 = 0;

			m10 = 0;
			m11 = 0;
			m12 = 0;

			m20 = 0;
			m21 = 0;
			m22 = 0;
		}

		/// <summary>
		/// Determines if the <paramref name="obj"/> is a <see cref="Matrix3x3"/> and if the values are the same.
		/// </summary>
		/// <param name="obj"></param>
		/// <returns></returns>
		public override bool Equals(object obj)
		{
			if(!(obj is Matrix3x3))
			{
				return false;
			}

			var matrix = (Matrix3x3)obj;
			return m00 == matrix.m00 &&
			       m01 == matrix.m01 &&
			       m02 == matrix.m02 &&
			       m10 == matrix.m10 &&
			       m11 == matrix.m11 &&
			       m12 == matrix.m12 &&
			       m20 == matrix.m20 &&
			       m21 == matrix.m21 &&
			       m22 == matrix.m22;
		}

		/// <summary>
		/// Returns the hash code for this instance.
		/// </summary>
		/// <returns></returns>
		public override int GetHashCode()
		{
			var hashCode = 1544463542;
			hashCode = hashCode * -1521134295 + base.GetHashCode();
			hashCode = hashCode * -1521134295 + m00.GetHashCode();
			hashCode = hashCode * -1521134295 + m01.GetHashCode();
			hashCode = hashCode * -1521134295 + m02.GetHashCode();
			hashCode = hashCode * -1521134295 + m10.GetHashCode();
			hashCode = hashCode * -1521134295 + m11.GetHashCode();
			hashCode = hashCode * -1521134295 + m12.GetHashCode();
			hashCode = hashCode * -1521134295 + m20.GetHashCode();
			hashCode = hashCode * -1521134295 + m21.GetHashCode();
			hashCode = hashCode * -1521134295 + m22.GetHashCode();
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
		public static bool operator ==(Matrix3x3 m1, Matrix3x3 m2)
		{
			return m1.Equals(m2);
		}

		/// <summary>
		/// Determines if the two matrices are not the same.
		/// </summary>
		/// <param name="m1"></param>
		/// <param name="m2"></param>
		/// <returns></returns>
		public static bool operator !=(Matrix3x3 m1, Matrix3x3 m2)
		{
			return !m1.Equals(m2);
		}

		/// <summary>
		/// Get a specific row of the <see cref="Matrix3x3"/> as a <see cref="Vector3"/>.
		/// </summary>
		/// <param name="row"></param>
		/// <returns></returns>
		public Vector3 this[int row]
		{
			get
			{
				switch(row)
				{
				case 0:
					return new Vector3(m00, m01, m02);
				case 1:
					return new Vector3(m10, m11, m12);
				case 2:
					return new Vector3(m20, m21, m22);

				default:
					throw new ArgumentOutOfRangeException(nameof(row), string.Format("The value for {0} cannot be smaller than 0 or bigger than 2!", nameof(row)));
				}
			}
			set
			{
				switch(row)
				{
				case 0:
				{
					m00 = value.x;
					m01 = value.y;
					m02 = value.z;
				}
				break;
				case 1:
				{
					m10 = value.x;
					m11 = value.y;
					m12 = value.z;
				}
				break;
				case 2:
				{
					m20 = value.x;
					m21 = value.y;
					m22 = value.z;
				}
				break;

				default:
					throw new ArgumentOutOfRangeException(nameof(row), string.Format("The value for {0} cannot be smaller than 0 or bigger than 2!", nameof(row)));
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
				if(row < 0 || row > 2)
				{
					throw new ArgumentOutOfRangeException(nameof(row), string.Format("The value for {0} cannot be smaller than 0 or bigger than 2!", nameof(row)));
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
						return m00;
					case 1:
						return m01;
					case 2:
						return m02;
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
					}
					break;
				}

				// This should never be able to happen, since the out of range index is already caught at the top.
				throw new ArgumentOutOfRangeException(nameof(row));
			}
			set
			{
				if(row < 0 || row > 2)
				{
					throw new ArgumentOutOfRangeException(nameof(row), string.Format("The value for {0} cannot be smaller than 0 or bigger than 2!", nameof(row)));
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
					}
					return;
				}
			}
		}
		#endregion
	}
}
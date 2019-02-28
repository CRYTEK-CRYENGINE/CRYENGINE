// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Globalization;

using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// DataType that represents a 3 by 4 matrix of <see cref="float"/> values, mostly used for translations, rotations and scale.
	/// </summary>
	public struct Matrix3x4
	{
		/// <summary>
		/// A default matrix that doesn't apply any rotation and translation.
		/// </summary>
		public static readonly Matrix3x4 Identity = new Matrix3x4(Quaternion.Identity);

		private float m00, m01, m02, m03;
		private float m10, m11, m12, m13;
		private float m20, m21, m22, m23;

		/// <summary>
		/// Construct a new matrix with the specified scale, rotation and position.
		/// </summary>
		/// <param name="scale"></param>
		/// <param name="rotation"></param>
		/// <param name="position"></param>
		public Matrix3x4(Vector3 scale, Quaternion rotation, Vector3 position)
		{
			float vxvx = rotation.v.x * rotation.v.x;
			float vzvz = rotation.v.z * rotation.v.z;
			float vyvy = rotation.v.y * rotation.v.y;
			float vxvy = rotation.v.x * rotation.v.y;
			float vxvz = rotation.v.x * rotation.v.z;
			float vyvz = rotation.v.y * rotation.v.z;
			float svx = rotation.w * rotation.v.x;
			float svy = rotation.w * rotation.v.y;
			float svz = rotation.w * rotation.v.z;
			m00 = (1 - (vyvy + vzvz) * 2) * scale.x;
			m01 = (vxvy - svz) * 2 * scale.y;
			m02 = (vxvz + svy) * 2 * scale.z;
			m03 = position.x;
			m10 = (vxvy + svz) * 2 * scale.x;
			m11 = (1 - (vxvx + vzvz) * 2) * scale.y;
			m12 = (vyvz - svx) * 2 * scale.z;
			m13 = position.y;
			m20 = (vxvz - svy) * 2 * scale.x;
			m21 = (vyvz + svx) * 2 * scale.y;
			m22 = (1 - (vxvx + vyvy) * 2) * scale.z;
			m23 = position.z;
		}

		/// <summary>
		/// Create a new <see cref="Matrix3x4"/> from a quaternion.
		/// </summary>
		/// <param name="quat"></param>
		public Matrix3x4(Quaternion quat)
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
			m03 = 0;
			m10 = xy + zw;
			m11 = xx - zz;
			m12 = yz - xw;
			m13 = 0;
			m20 = xz - yw;
			m21 = yz + xw;
			m22 = xx - yy;
			m23 = 0;
		}

		/// <summary>
		/// Create a new <see cref="Matrix3x4"/> from the specified <see cref="float"/> values.
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
		public Matrix3x4(float p00, float p01, float p02, float p03,
		                 float p10, float p11, float p12, float p13,
		                 float p20, float p21, float p22, float p23)
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
		}

		#region Overrides
		/// <summary>
		/// Converts this instance to a nicely formatted string.
		/// Example: [0.123, 0.123, 0.123, 0.123],[0.123, 0.123, 0.123, 0.123],[0.123, 0.123, 0.123, 0.123]
		/// </summary>
		/// <returns></returns>
		public override string ToString()
		{
			return string.Format(CultureInfo.CurrentCulture, "[{0}],[{1}],[{2}]", this[0], this[1], this[2]);
		}
		#endregion

		#region Conversions
		/// <summary>
		/// Implicitly converts the managed <see cref="Matrix3x4"/> to an unmanaged <see cref="Matrix34"/>.
		/// </summary>
		/// <param name="managedMatrix"></param>
		public static implicit operator Matrix34(Matrix3x4 managedMatrix)
		{
			return new Matrix34(managedMatrix.m00, managedMatrix.m01, managedMatrix.m02, managedMatrix.m03,
								managedMatrix.m10, managedMatrix.m11, managedMatrix.m12, managedMatrix.m13,
								managedMatrix.m20, managedMatrix.m21, managedMatrix.m22, managedMatrix.m23);
		}

		/// <summary>
		/// Implicitly converts the unmanaged <see cref="Matrix34"/> to a managed <see cref="Matrix3x4"/>.
		/// </summary>
		/// <param name="nativeMatrix"></param>
		public static implicit operator Matrix3x4(Matrix34 nativeMatrix)
		{
			if(nativeMatrix == null)
			{
				return new Matrix3x4();
			}

			return new Matrix3x4(nativeMatrix.m00, nativeMatrix.m01, nativeMatrix.m02, nativeMatrix.m03,
								nativeMatrix.m10, nativeMatrix.m11, nativeMatrix.m12, nativeMatrix.m13,
								nativeMatrix.m20, nativeMatrix.m21, nativeMatrix.m22, nativeMatrix.m23);
		}

		/// <summary>
		/// Converts the matrix to a <see cref="Quaternion"/>.
		/// </summary>
		/// <param name="matrix"></param>
		public static implicit operator Quaternion(Matrix3x4 matrix)
		{
			return new Quaternion(matrix);
		}
		#endregion

		#region Methods
		/// <summary>
		/// Transforms a direction to the orientation of this matrix.
		/// </summary>
		/// <param name="direction"></param>
		/// <returns></returns>
		public Vector3 TransformDirection(Vector3 direction)
		{
			return new Vector3(m00 * direction.x + m01 * direction.y + m02 * direction.z, m10 * direction.x + m11 * direction.y + m12 * direction.z, m20 * direction.x + m21 * direction.y + m22 * direction.z);
		}

		/// <summary>
		/// Transforms a point to the orientation of this matrix.
		/// </summary>
		/// <param name="point"></param>
		/// <returns></returns>
		public Vector3 TransformPoint(Vector3 point)
		{
			return new Vector3(m00 * point.x + m01 * point.y + m02 * point.z + m03, m10 * point.x + m11 * point.y + m12 * point.z + m13, m20 * point.x + m21 * point.y + m22 * point.z + m23);
		}

		/// <summary>
		/// Set the translation of this matrix.
		/// </summary>
		/// <param name="t"></param>
		public void SetTranslation(Vector3 t)
		{
			m03 = t.x; m13 = t.y; m23 = t.z;
		}

		/// <summary>
		/// Get the translation of this matrix. It's the same as getting the 4th row.
		/// </summary>
		/// <returns></returns>
		public Vector3 GetTranslation()
		{
			return new Vector3(m03, m13, m23);
		}

		/// <summary>
		/// Scale the translation by the length of <paramref name="s"/>.
		/// </summary>
		/// <param name="s"></param>
		public void ScaleTranslation(float s)
		{
			m03 *= s; m13 *= s; m23 *= s;
		}

		/// <summary>
		/// Add a translation to the matrix.
		/// </summary>
		/// <param name="t"></param>
		public void AddTranslation(Vector3 t)
		{
			m03 += t.x; m13 += t.y; m23 += t.z;
		}

		/// <summary>
		/// Determines if the <paramref name="obj"/> is a <see cref="Matrix3x4"/> and if the values are the same.
		/// </summary>
		/// <param name="obj"></param>
		/// <returns></returns>
		public override bool Equals(object obj)
		{
			if(!(obj is Matrix3x4))
			{
				return false;
			}

			var matrix = (Matrix3x4)obj;
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
			       m23 == matrix.m23;
		}

		/// <summary>
		/// Returns the hash code for this instance.
		/// </summary>
		/// <returns></returns>
		public override int GetHashCode()
		{
			var hashCode = 342415158;
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
		public static bool operator ==(Matrix3x4 m1, Matrix3x4 m2)
		{
			return m1.Equals(m2);
		}

		/// <summary>
		/// Determines if the two matrices are not the same.
		/// </summary>
		/// <param name="m1"></param>
		/// <param name="m2"></param>
		/// <returns></returns>
		public static bool operator !=(Matrix3x4 m1, Matrix3x4 m2)
		{
			return !m1.Equals(m2);
		}

		/// <summary>
		/// Returns the specified row as a <see cref="Vector4"/>
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
					m03 = value.w;
				}
				break;
				case 1:
				{
					m10 = value.x;
					m11 = value.y;
					m12 = value.z;
					m13 = value.w;
				}
				break;
				case 2:
				{
					m20 = value.x;
					m21 = value.y;
					m22 = value.z;
					m23 = value.w;
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
				}
			}
		}
		#endregion
	}
}
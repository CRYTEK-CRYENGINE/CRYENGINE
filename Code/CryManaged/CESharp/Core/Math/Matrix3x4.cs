// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Globalization;

using CryEngine.Common;

namespace CryEngine
{
	public struct Matrix3x4 :IEquatable<Matrix3x4>
	{
		public static readonly Matrix3x4 Identity = new Matrix3x4(Quaternion.Identity);

		public float m00, m01, m02, m03;
		public float m10, m11, m12, m13;
		public float m20, m21, m22, m23;

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
		public override int GetHashCode()
		{
			unchecked // Overflow is fine, just wrap
			{
				int hash = 17;

				hash = hash * 23 + this[0].GetHashCode();
				hash = hash * 23 + this[1].GetHashCode();
				hash = hash * 23 + this[2].GetHashCode();

				return hash;
			}
		}

		public override bool Equals(object obj)
		{
			if (obj == null)
				return false;

			if (!(obj is Matrix3x4 || obj is Matrix34))
				return false;

			return Equals((Matrix3x4)obj);
		}

		public bool Equals(Matrix3x4 other)
		{
			return MathHelpers.IsEqual(m00 , other.m00) && MathHelpers.IsEqual(m01 , other.m01) && MathHelpers.IsEqual(m02 , other.m02) && MathHelpers.IsEqual(m03 , other.m03)
				  && MathHelpers.IsEqual(m10 , other.m10) && MathHelpers.IsEqual(m11 , other.m11) && MathHelpers.IsEqual(m12 , other.m12) && MathHelpers.IsEqual(m13 , other.m13)
				  && MathHelpers.IsEqual(m20 , other.m20) && MathHelpers.IsEqual(m21 , other.m21) && MathHelpers.IsEqual(m22 , other.m22) && MathHelpers.IsEqual(m23 , other.m23);
		}

		public override string ToString()
		{
			return string.Format(CultureInfo.CurrentCulture, "[{0}],[{1}],[{2}]", this[0].ToString(), this[1].ToString(), this[2].ToString());
		}
		#endregion

		#region Conversions
		public static implicit operator Matrix34(Matrix3x4 managedMatrix)
		{
			return new Matrix34(managedMatrix.m00, managedMatrix.m01, managedMatrix.m02, managedMatrix.m03,
								managedMatrix.m10, managedMatrix.m11, managedMatrix.m12, managedMatrix.m13,
								managedMatrix.m20, managedMatrix.m21, managedMatrix.m22, managedMatrix.m23);
		}

		public static implicit operator Matrix3x4(Matrix34 nativeMatrix)
		{
			if (nativeMatrix == null)
			{
				return new Matrix3x4();
			}

			return new Matrix3x4(nativeMatrix.m00, nativeMatrix.m01, nativeMatrix.m02, nativeMatrix.m03,
								nativeMatrix.m10, nativeMatrix.m11, nativeMatrix.m12, nativeMatrix.m13,
								nativeMatrix.m20, nativeMatrix.m21, nativeMatrix.m22, nativeMatrix.m23);
		}

		public static implicit operator Quaternion(Matrix3x4 matrix)
		{
			return new Quaternion(matrix);
		}
		#endregion

		#region Operators
		public static bool operator ==(Matrix3x4 left, Matrix3x4 right)
		{
			if ((object)right == null)
				return (object)left == null;

			return left.Equals(right);
		}

		public static bool operator !=(Matrix3x4 left, Matrix3x4 right)
		{
			return !(left == right);
		}
		#endregion

		#region Methods
		public Vector3 TransformDirection(Vector3 direction)
		{
			return new Vector3(m00 * direction.x + m01 * direction.y + m02 * direction.z, m10 * direction.x + m11 * direction.y + m12 * direction.z, m20 * direction.x + m21 * direction.y + m22 * direction.z);
		}

		public Vector3 TransformPoint(Vector3 point)
		{
			return new Vector3(m00 * point.x + m01 * point.y + m02 * point.z + m03, m10 * point.x + m11 * point.y + m12 * point.z + m13, m20 * point.x + m21 * point.y + m22 * point.z + m23);
		}

		public void SetTranslation(Vector3 t)
		{
			m03 = t.x; m13 = t.y; m23 = t.z;
		}

		public Vector3 GetTranslation()
		{
			return new Vector3(m03, m13, m23);
		}

		public void ScaleTranslation(float s)
		{
			m03 *= s; m13 *= s; m23 *= s;
		}

		public void AddTranslation(Vector3 t)
		{
			m03 += t.x; m13 += t.y; m23 += t.z;
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

					default:
						throw new ArgumentOutOfRangeException("index", "Indices must run from 0 to 2!");
				}
			}
			set
			{
				switch (row)
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
						throw new ArgumentOutOfRangeException("index", "Indices must run from 0 to 2!");
				}
			}
		}

		public float this[int row, int column]
		{
			get
			{
				switch (column)
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
						throw new ArgumentOutOfRangeException("index", "Indices must run from 0 to 3!");
				}
			}
		}
		#endregion
	}
}
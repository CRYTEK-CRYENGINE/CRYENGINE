// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using System;
using System.Globalization;

using CryEngine.Common;

namespace CryEngine
{
	public struct Matrix3x3 :IEquatable<Matrix3x3>
	{
		public static readonly Matrix3x3 Identity = new Matrix3x3
		(
			1f,0f,0f,
			0f,1f,0f,
			0f,0f,1f
		);

		public float m00, m01, m02;
		public float m10, m11, m12;
		public float m20, m21, m22;

		#region Constructors

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

		public Matrix3x3(Matrix3x3 matrix)
		{
			m00 = matrix.m00;
			m01 = matrix.m01;
			m02 = matrix.m02;
			
			m10 = matrix.m10;
			m11 = matrix.m11;
			m12 = matrix.m12;
			
			m20 = matrix.m20;
			m21 = matrix.m21;
			m22 = matrix.m22;
		}


		/// <summary>
		/// It initializes a Matrix33 with 3 vectors stored in the columns.
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
		/// It converts a Matrix34 into a Matrix33. The translation vector in the conversion process.
		/// </summary>
		/// <param name="matrix"></param>
		public Matrix3x3(Matrix4x4 matrix)
		{
			m00 = matrix.m00;
			m01 = matrix.m01;
			m02 = matrix.m02;
			m10 = matrix.m10;
			m11 = matrix.m11;
			m12 = matrix.m12;
			m20 = matrix.m20;
			m21 = matrix.m21;
			m22 = matrix.m22;
		}

		/// <summary>
		/// It converts a Matrix34 into a Matrix33.. Loses translation vector in the conversion process.
		/// </summary>
		/// <param name="matrix"></param>
		public Matrix3x3(Matrix3x4 matrix)
		{
			m00 = matrix.m00;
			m01 = matrix.m01;
			m02 = matrix.m02;
			m10 = matrix.m10;
			m11 = matrix.m11;
			m12 = matrix.m12;
			m20 = matrix.m20;
			m21 = matrix.m21;
			m22 = matrix.m22;
		}

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
		public static implicit operator Matrix33(Matrix3x3 managedMatrix)
		{
			return new Matrix33(managedMatrix.m00, managedMatrix.m01, managedMatrix.m02,
													managedMatrix.m10, managedMatrix.m11, managedMatrix.m12,
													managedMatrix.m20, managedMatrix.m21, managedMatrix.m22);
		}

		public static implicit operator Matrix3x3(Matrix33 nativeMatrix)
		{
			if (nativeMatrix == null)
			{
				return new Matrix3x3();
			}

			return new Matrix3x3(nativeMatrix.m00, nativeMatrix.m01, nativeMatrix.m02, 
													nativeMatrix.m10, nativeMatrix.m11, nativeMatrix.m12,
													nativeMatrix.m20, nativeMatrix.m21, nativeMatrix.m22);
		}
		#endregion

		#region Operators

		public static bool operator ==(Matrix3x3 left, Matrix3x3 right)
		{
			if ((object)right == null)
				return (object)left == null;

			return left.Equals(right);
		}

		public static bool operator !=(Matrix3x3 left, Matrix3x3 right)
		{
			return !(left == right);
		}

		#endregion

		#region Overrides
		public override int GetHashCode()
		{
			unchecked
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

			if (!(obj is Matrix3x3 || obj is Matrix33))
				return false;

			return Equals((Matrix3x3)obj);
		}

		public bool Equals(Matrix3x3 other)
		{
			return MathHelpers.Approximately(m00, other.m00) && MathHelpers.Approximately(m01, other.m01) && MathHelpers.Approximately(m02, other.m02) 
					&& MathHelpers.Approximately(m10, other.m10) && MathHelpers.Approximately(m11, other.m11) && MathHelpers.Approximately(m12, other.m12) 
					&& MathHelpers.Approximately(m20, other.m20) && MathHelpers.Approximately(m21, other.m21) && MathHelpers.Approximately(m22, other.m22) ;
		}

		public override string ToString()
		{
			return string.Format(CultureInfo.CurrentCulture, "[{0}],[{1}],[{2}]", this[0], this[1], this[2]);
		}

		#endregion

		#region Methods

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

		#endregion

		#region Properties
		public Vector3 this[int row]
		{
			get
			{
				switch (row)
				{
					case 0:
						return new Vector3(m00, m01, m02);
					case 1:
						return new Vector3(m10, m11, m12);
					case 2:
						return new Vector3(m20, m21, m22);

					default:
					throw new ArgumentOutOfRangeException(nameof(row), "Indices must run from 0 to 2!");
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
					throw new ArgumentOutOfRangeException(nameof(row), "Indices must run from 0 to 2!");
				}
			}
		}

		public float this[int row, int column]
		{
			get
			{
				if(row < 0 || row > 2)
				{
					throw new ArgumentOutOfRangeException(nameof(row), "Row indices must run from 0 to 2!");
				}

				switch (column)
				{
					case 0:
						return this[row].x;
					case 1:
						return this[row].y;
					case 2:
						return this[row].z;
					default:
					throw new ArgumentOutOfRangeException(nameof(column), "Column indices must run from 0 to 2!");
				}
			}
		}
		#endregion
	}
}
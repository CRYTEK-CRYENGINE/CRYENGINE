// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Globalization;
using CryEngine.Common;

namespace CryEngine
{
	public struct Vector4
	{
		private float _x;
		private float _y;
		private float _z;
		private float _w;

		public float x { get { return _x; } set { _x = value; } }
		public float y { get { return _y; } set { _y = value; } }
		public float z { get { return _z; } set { _z = value; } }
		public float w { get { return _w; } set { _w = value; } }

		public float X { get { return _x; } set { _x = value; } }
		public float Y { get { return _y; } set { _y = value; } }
		public float Z { get { return _z; } set { _z = value; } }
		public float W { get { return _w; } set { _w = value; } }

		public Vector4(float xCoord, float yCoord, float zCoord, float wCoord)
		{
			_x = xCoord;
			_y = yCoord;
			_z = zCoord;
			_w = wCoord;
		}

		#region Overrides
		public override int GetHashCode()
		{
			unchecked // Overflow is fine, just wrap
			{
				int hash = 17;

				hash = hash * 23 + X.GetHashCode();
				hash = hash * 23 + Y.GetHashCode();
				hash = hash * 23 + Z.GetHashCode();
				hash = hash * 23 + W.GetHashCode();

				return hash;
			}
		}

		public override bool Equals(object obj)
		{
			if (obj == null)
				return false;

			if (obj is Vector4 || obj is Vec4)
				return this == (Vector4)obj;

			return false;
		}

		public override string ToString()
		{
			return string.Format(CultureInfo.CurrentCulture, "{0},{1},{2},{3}", X, Y, Z, W);
		}
		#endregion

		#region Conversions
		public static implicit operator Vec4(Vector4 managedVector)
		{
			return new Vec4(managedVector.X, managedVector.Y, managedVector.Z, managedVector.W);
		}

		public static implicit operator Vector4(Vec4 nativeVector)
		{
			if(nativeVector == null)
			{
				return new Vector4();
			}

			return new Vector4(nativeVector.x, nativeVector.y, nativeVector.z, nativeVector.w);
		}

		public static implicit operator Vector4(Vector3 vec3)
		{
			return new Vector4(vec3.x, vec3.y, vec3.z, 0);
		}
		#endregion

		#region Operators
		public static bool operator ==(Vector4 left, Vector4 right)
		{
			if ((object)right == null)
				return (object)left == null;

			return ((left.X == right.X) && (left.Y == right.Y) && (left.Z == right.Z) && (left.W == right.W));
		}

		public static bool operator !=(Vector4 left, Vector4 right)
		{
			return !(left == right);
		}
		#endregion

		#region Functions
		public bool IsZero(float epsilon = 0)
		{
			return (Math.Abs(x) <= epsilon) && (Math.Abs(y) <= epsilon) && (Math.Abs(z) <= epsilon) && (Math.Abs(w) <= epsilon);
		}
		#endregion

		#region Properties
		/// <summary>
		/// Gets individual axes by index
		/// </summary>
		/// <param name="index">Index, 0 - 3 where 0 is X and 3 is W</param>
		/// <returns>The axis value</returns>
		public float this[int index]
		{
			get
			{
				switch (index)
				{
					case 0:
						return x;
					case 1:
						return y;
					case 2:
						return z;
					case 3:
						return W;

					default:
						throw new ArgumentOutOfRangeException("index", "Indices must run from 0 to 3!");
				}
			}
			set
			{
				switch (index)
				{
					case 0:
						x = value;
						break;
					case 1:
						y = value;
						break;
					case 2:
						z = value;
						break;
					case 3:
						z = value;
						break;

					default:
						throw new ArgumentOutOfRangeException("index", "Indices must run from 0 to 3!");
				}
			}
		}
		#endregion

	}
}

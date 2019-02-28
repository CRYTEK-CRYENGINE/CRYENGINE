// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Globalization;
using System.Runtime.InteropServices;
using CryEngine.Common;

namespace CryEngine
{
	[StructLayout(LayoutKind.Sequential)]
	public struct Vector4 : IEquatable<Vector4>
	{
		[MarshalAs(UnmanagedType.R4)]
		private float _x;
		[MarshalAs(UnmanagedType.R4)]
		private float _y;
		[MarshalAs(UnmanagedType.R4)]
		private float _z;
		[MarshalAs(UnmanagedType.R4)]
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

#pragma warning disable RECS0025 // Non-readonly field referenced in 'GetHashCode()'
				hash = hash * 23 + _x.GetHashCode();
				hash = hash * 23 + _y.GetHashCode();
				hash = hash * 23 + _z.GetHashCode();
				hash = hash * 23 + _w.GetHashCode();
#pragma warning restore RECS0025 // Non-readonly field referenced in 'GetHashCode()'

				return hash;
			}
		}

		public override bool Equals(object obj)
		{
			if (obj == null)
				return false;

			if (!(obj is Vector4 || obj is Vec4))
				return false;

			return Equals((Vector4)obj);
		}

		public bool Equals(Vector4 other)
		{
			return MathHelpers.Approximately(_x, other.x) && MathHelpers.Approximately(_y, other.y) && MathHelpers.Approximately(_z, other.z) && MathHelpers.Approximately(_w, other.w);
		}

		public override string ToString()
		{
			return string.Format(CultureInfo.CurrentCulture, "{0},{1},{2},{3}", _x, _y, _z, _w);
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

		public static explicit operator Vector4(Vector2 vector)
		{
			return new Vector4
			{
				X = vector.X,
				Y = vector.Y,
				Z = 0.0f,
				W = 0.0f
			};
		}
		#endregion

		#region Operators

		public static Vector4 operator *(Vector4 v, float scale)
		{
			return new Vector4(v.x * scale, v.y * scale, v.z * scale, v.w * scale);
		}

		public static Vector4 operator *(float scale, Vector4 v)
		{
			return v * scale;
		}

		public static Vector4 operator /(Vector4 v, float scale)
		{
			scale = 1.0f / scale;
			return new Vector4(v.x * scale, v.y * scale, v.z * scale, v.w * scale);
		}

		public static Vector4 operator+(Vector4 left, Vector4 right)
		{
			return new Vector4(left.x + right.x, left.y + right.y, left.z + right.z, left.w + right.w);
		}

		public static Vector4 operator-(Vector4 left, Vector4 right)
		{
			return new Vector4(left.x - right.x, left.y - right.y, left.z - right.z, left.w - right.w);
		}

		public static bool operator ==(Vector4 left, Vector4 right)
		{
			return left.Equals(right);
		}

		public static bool operator !=(Vector4 left, Vector4 right)
		{
			return !(left == right);
		}
		#endregion

		#region Functions
		[Obsolete("Please use IsNearlyZero")]
		public bool IsZero(float epsilon = 0)
		{
			return (Math.Abs(x) <= epsilon) && (Math.Abs(y) <= epsilon) && (Math.Abs(z) <= epsilon) && (Math.Abs(w) <= epsilon);
		}

		public bool IsNearlyZero()
		{
			return (Math.Abs(_x) <= MathHelpers.Epsilon && Math.Abs(_y) <= MathHelpers.Epsilon) && Math.Abs(_z) <= MathHelpers.Epsilon && Math.Abs(_w) <= MathHelpers.Epsilon;
		}

		public static Vector4 Lerp(Vector4 p, Vector4 q, float t)
		{
			t = Math.Max(Math.Min(1.0f, t), 0f);
			return LerpUnclamped(p, q, t);
		}

		public static Vector4 LerpUnclamped(Vector4 p, Vector4 q, float t)
		{
			Vector4 diff = q - p;
			Vector4 r = p + (diff * t);
			return new Vector4(r.x, r.y, r.z, r.w);
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
						return _x;
					case 1:
						return _y;
					case 2:
						return _z;
					case 3:
						return _w;

					default:
						throw new ArgumentOutOfRangeException(nameof(index), "Indices must run from 0 to 3!");
				}
			}
			set
			{
				switch (index)
				{
					case 0:
						_x = value;
						break;
					case 1:
						_y = value;
						break;
					case 2:
						_z = value;
						break;
					case 3:
						_w = value;
						break;

					default:
						throw new ArgumentOutOfRangeException(nameof(index), "Indices must run from 0 to 3!");
				}
			}
		}

		public Vector4 Normalized
		{
			get
			{
				if (IsNearlyZero()) return new Vector4(0f, 0f, 0f, 0f);
				Vector4 result = this * 1.0f / (float)Math.Sqrt(_x * _x + _y * _y + _z * _z + _w * _w);
				return result;
			}
		}
		#endregion

	}
}

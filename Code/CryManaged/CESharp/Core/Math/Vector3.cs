// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Globalization;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using CryEngine.Common;

namespace CryEngine
{
	[StructLayout(LayoutKind.Sequential)]
	public struct Vector3 : IEquatable<Vector3>
	{
		public static readonly Vector3 Zero = new Vector3(0, 0, 0);
		public static readonly Vector3 One = new Vector3(1, 1, 1);

		public static readonly Vector3 Up = new Vector3(0, 0, 1);
		public static readonly Vector3 Down = new Vector3(0, 0, -1);
		public static readonly Vector3 Forward = new Vector3(0, 1, 0);
		public static readonly Vector3 Back = new Vector3(0, -1, 0);
		public static readonly Vector3 Left = new Vector3(-1, 0, 0);
		public static readonly Vector3 Right = new Vector3(1, 0, 0);

		[MarshalAs(UnmanagedType.R4)]
		private float _x;
		[MarshalAs(UnmanagedType.R4)]
		private float _y;
		[MarshalAs(UnmanagedType.R4)]
		private float _z;

		public float x { get { return _x; } set { _x = value; } }
		public float y { get { return _y; } set { _y = value; } }
		public float z { get { return _z; } set { _z = value; } }

		public float X { get { return _x; } set { _x = value; } }
		public float Y { get { return _y; } set { _y = value; } }
		public float Z { get { return _z; } set { _z = value; } }
		
		public Vector3(float xCoord, float yCoord, float zCoord)
		{
			_x = xCoord;
			_y = yCoord;
			_z = zCoord;
		}

		public Vector3(Vector2 v) : this(v.x, v.y, 0.0f) { }
		public Vector3(Vector4 v) : this(v.x, v.y, v.z) { }

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
#pragma warning restore RECS0025 // Non-readonly field referenced in 'GetHashCode()'
				return hash;
			}
		}

		public override bool Equals(object obj)
		{
			if (obj == null)
				return false;

			if (!(obj is Vector3 || obj is Vec3))
			{
				return false;
			}
			return Equals((Vector3)obj);
		}

		public bool Equals(Vector3 other)
		{
			return MathHelpers.Approximately(_x, other.x) && MathHelpers.Approximately(_y, other.y) && MathHelpers.Approximately(_z, other.z);
		}

		public override string ToString()
		{
			return string.Format(CultureInfo.CurrentCulture, "{0}, {1}, {2}", _x, _y, _z);
		}
		#endregion

		#region Conversions
		public static implicit operator Vec3(Vector3 managedVector)
		{
			return new Vec3(managedVector.X, managedVector.Y, managedVector.Z);
		}

		public static implicit operator Vector3(Vec3 nativeVector)
		{
			if(nativeVector == null)
			{
				return Zero;
			}

			return new Vector3(nativeVector.x, nativeVector.y, nativeVector.z);
		}

		public static explicit operator Vector3(Vector2 vector)
		{
			return new Vector3
			{
				X = vector.X,
				Y = vector.Y,
				Z = 0.0f
			};
		}

		public static explicit operator Vector3(Vector4 vector)
		{
			return new Vector3
			{
				X = vector.X,
				Y = vector.Y,
				Z = vector.Z
			};
		}
		#endregion

		#region Operators
		public static Vector3 operator *(Vector3 v, float scale)
		{
			return new Vector3(v.X * scale, v.Y * scale, v.Z * scale);
		}

		public static Vector3 operator *(float scale, Vector3 v)
		{
			return v * scale;
		}

		public static Vector3 operator /(Vector3 v, float scale)
		{
			scale = 1.0f / scale;

			return new Vector3(v.X * scale, v.Y * scale, v.Z * scale);
		}

		public static Vector3 operator +(Vector3 v0, Vector3 v1)
		{
			return new Vector3(v0.X + v1.X, v0.Y + v1.Y, v0.Z + v1.Z);
		}
		
		public static Vector3 operator -(Vector3 v0, Vector3 v1)
		{
			return new Vector3(v0.X - v1.X, v0.Y - v1.Y, v0.Z - v1.Z);
		}
		
		public static Vector3 operator -(Vector3 v)
		{
			return v.Flipped;
		}

		public static Vector3 operator !(Vector3 v)
		{
			return v.Flipped;
		}

		public static bool operator ==(Vector3 left, Vector3 right)
		{
			return left.Equals(right);
		}

		public static bool operator !=(Vector3 left, Vector3 right)
		{
			return !(left == right);
		}
		#endregion

		#region Functions
		/// <summary>
		/// Returns the dot product of this Vector3 and Vector3 v.
		/// If the vectors are normalized the value will always be between 1 (vectors are the same) and -1 (vectors are opposites).
		/// </summary>
		/// <returns>The dot product.</returns>
		/// <param name="v">Other direction vector.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public float Dot(Vector3 v)
		{
			return Dot(this, v);
		}

		/// <summary>
		/// Returns the dot product of Vector3 a and b.
		/// If the vectors are normalized the value will always be between 1 (vectors are the same) and -1 (vectors are opposites).
		/// </summary>
		/// <returns>The dot product.</returns>
		/// <param name="a">Direction vector a.</param>
		/// <param name="b">Direction vector b.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float Dot(Vector3 a, Vector3 b)
		{
			return a._x * b._x + a._y * b._y + a._z * b._z;
		}

		/// <summary>
		/// Cross this vector with the specified vector.
		/// </summary>
		/// <returns>The cross.</returns>
		/// <param name="v">The other vector.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public Vector3 Cross(Vector3 v)
		{
			return Cross(this, v);
		}

		/// <summary>
		/// Cross the specified a and b vectors.
		/// </summary>
		/// <returns>The cross.</returns>
		/// <param name="a">Vector a.</param>
		/// <param name="b">Vector b.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector3 Cross(Vector3 a, Vector3 b)
		{
			return new Vector3(a._y * b._z - a._z * b._y, a._z * b._x - a._x * b._z, a._x * b._y - a._y * b._x);
		}

		[Obsolete("Please use IsNearlyZero")]
		public bool IsZero(float epsilon = 0)
		{
			return (Math.Abs(x) <= epsilon) && (Math.Abs(y) <= epsilon) && (Math.Abs(z) <= epsilon);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public bool IsNearlyZero()
		{
			return (Math.Abs(_x) <= MathHelpers.Epsilon && Math.Abs(_y) <= MathHelpers.Epsilon) && Math.Abs(_z) <= MathHelpers.Epsilon;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector3 Lerp(Vector3 p, Vector3 q, float t)
		{
			t = Math.Max(Math.Min(1.0f, t), 0f);
			return LerpUnclamped(p, q, t);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector3 LerpUnclamped(Vector3 p, Vector3 q, float t)
		{
			Vector3 diff = q - p;
			return p + (diff * t);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector3 Slerp(Vector3 p, Vector3 q, float t)
		{
			t = Math.Max(Math.Min(1.0f, t), 0f);
			return SlerpUnclamped(p, q, t);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector3 SlerpUnclamped(Vector3 p, Vector3 q, float t)
		{
			bool errorFlag1 = false;
			bool errorFlag2 = false;
			CheckUnitVector(p, ref errorFlag1);
			CheckUnitVector(q, ref errorFlag2);
			if (errorFlag1)
			{
				p = p.Normalized;
			}
			if(errorFlag2)
			{
				q = q.Normalized;
			}
			float cosine = (p.x * q.x) + (p.y * q.y) + (p.z * q.z);
			cosine = MathHelpers.Clamp(-1, 1, cosine);

			if (MathHelpers.Approximately(cosine, 1, 0.000001f))
			{
				// use lerp
				var result = LerpUnclamped(p, q, t);
				return result.Normalized;
			}
			var radians = (float)Math.Acos(cosine);
			var scale_0 = (float)Math.Sin((1 - t) * radians);
			var scale_1 = (float)Math.Sin(t * radians);
			Vector3 result2 = (p * scale_0 + q * scale_1) / (float)Math.Sin(radians);
			return result2.Normalized;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		private static void CheckUnitVector(Vector3 vector, ref bool errorFlag)
		{
			if (1.0f - (vector.x * vector.x + vector.y * vector.y + vector.z * vector.z) > 0.005f)
			{
				#if DEBUG
					string messageIfFalse = vector.ToString() + " not unit vector";
					Log.Always<Vector3>(messageIfFalse);
				#endif	
				errorFlag = false;
			}
			errorFlag = true;
		}

		/// <summary>
		/// Returns the angle in radians between Vector3 a and b in radians.
		/// If the magnitude of a or b is 0, it will return 0.
		/// </summary>
		/// <returns>The angle in radians.</returns>
		/// <param name="a">The first Vector3.</param>
		/// <param name="b">The second Vector3.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float Angle(Vector3 a, Vector3 b)
		{
			if(a.IsNearlyZero() || b.IsNearlyZero())
			{
				return 0.0f;
			}
			var normA = Math.Abs(1.0f - a.LengthSquared) > 0.005f ? a.Normalized : a;
			var normB = Math.Abs(1.0f - b.LengthSquared) > 0.005f ? b.Normalized : b;
			return (float)Math.Acos(Dot(normA, normB));
		}

		/// <summary>
		/// Returns the angle in radians between Vector3 a and b, but disregards the Z-axis.
		/// If the magnitude of a or b is 0, it will return 0.
		/// </summary>
		/// <returns>The angle.</returns>
		/// <param name="a">The first Vector3.</param>
		/// <param name="b">The second Vector3.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float FlatAngle(Vector3 a, Vector3 b)
		{
			float cz = a.x * b.y - a.y * b.x;
			float c = a.x * b.x + a.y * b.y;
			return (float)Math.Atan2(cz, c);
		}
#endregion

#region Properties
		public float Length
		{
			get
			{
				return (float)Math.Sqrt(_x * _x + _y * _y + _z * _z);
			}
			set
			{
				float lengthSquared = _x * _x + _y * _y + _z * _z;
				if (lengthSquared < 0.00001f * 0.00001f)
					return;

				lengthSquared = value * MathHelpers.ISqrt(lengthSquared);
				_x *= lengthSquared;
				_y *= lengthSquared;
				_z *= lengthSquared;
			}
		}

		public float Magnitude {  get { return Length; } }

		public float LengthSquared
		{
			get
			{
				return _x * _x + _y * _y + _z * _z;
			}
		}

		public float Length2D
		{
			get
			{
				return (float)Math.Sqrt(_x * _x + _y * _y);
			}
		}

		public float Magnitude2D {  get { return Length2D; } }

		public float Length2DSquared
		{
			get
			{
				return _x * _x + _y * _y;
			}
		}

		public Vector3 Normalized
		{
			get
			{
				//if nearly (0,0,0,) return current
				if (IsNearlyZero()) return this;
				return this * 1.0f / (float)Math.Sqrt(_x * _x + _y * _y + _z * _z);
			}
		}

		public float Volume
		{
			get
			{
				return _x * _y * _z;
			}
		}

		public Vector3 Absolute
		{
			get
			{
				return new Vector3(Math.Abs(_x), Math.Abs(_y), Math.Abs(_z));
			}
		}

		public Vector3 Flipped
		{
			get
			{
				return new Vector3(-_x, -_y, -_z);
			}
		}

		/// <summary>
		/// Returns a vector that is orthogonal to this one
		/// </summary>
		public Vector3 Orthogonal
		{
			get
			{
				return MathHelpers.Square(0.9f) * (_x * _x + _y * _y + _z * _z) - _x * _x < 0 ? new Vector3(-_z, 0, _x) : new Vector3(0, _z, -_y);
			}
		}

		/// <summary>
		/// Gets individual axes by index
		/// </summary>
		/// <param name="index">Index, 0 - 2 where 0 is X and 2 is Z</param>
		/// <returns>The axis value</returns>
		public float this[int index]
		{
			get
			{
				switch(index)
				{
					case 0:
						return _x;
					case 1:
						return _y;
					case 2:
						return _z;

					default:
					throw new ArgumentOutOfRangeException(nameof(index), "Indices must run from 0 to 2!");
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

					default:
						throw new ArgumentOutOfRangeException(nameof(index), "Indices must run from 0 to 2!");
				}
			}
		}
#endregion
	}
}

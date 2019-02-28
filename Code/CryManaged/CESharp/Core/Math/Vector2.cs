// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Globalization;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Diagnostics;
using CryEngine.Common;

namespace CryEngine
{
	[StructLayout(LayoutKind.Sequential)]
	public struct Vector2 : IEquatable<Vector2>
	{
		public static readonly Vector2 Zero = new Vector2(0, 0);

		public static readonly Vector2 Up = new Vector2(0, 1);
		public static readonly Vector2 Down = new Vector2(0, -1);
		public static readonly Vector2 Left = new Vector2(-1, 0);
		public static readonly Vector2 Right = new Vector2(1, 0);

		[MarshalAs(UnmanagedType.R4)]
		private float _x;
		[MarshalAs(UnmanagedType.R4)]
		private float _y;

		public float x { get { return _x; } set { _x = value; } }
		public float y { get { return _y; } set { _y = value; } }
		public float X { get { return _x; } set { _x = value; } }
		public float Y { get { return _y; } set { _y = value; } }

		public Vector2(float xCoord, float yCoord)
		{
			_x = xCoord;
			_y = yCoord;
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
#pragma warning restore RECS0025 // Non-readonly field referenced in 'GetHashCode()'

				return hash;
			}
		}

		public override bool Equals(object obj)
		{
			if (obj == null)
				return false;

			if (!(obj is Vector2 || obj is Vec2))
			{
				return false;
			}
			return Equals((Vector2)obj);
		}

		public bool Equals(Vector2 other)
		{
			return MathHelpers.Approximately(_x, other.x) && MathHelpers.Approximately(_y, other.y);
		}

		public override string ToString()
		{
			return string.Format(CultureInfo.CurrentCulture, "{0},{1}", _x, _y);
		}
		#endregion

		#region Conversions
		public static implicit operator Vec2(Vector2 managedVector)
		{
			return new Vec2(managedVector.X, managedVector.Y);
		}

		public static implicit operator Vector2(Vec2 nativeVector)
		{
			if(nativeVector == null)
			{
				return Zero;
			}

			return new Vector2(nativeVector.x, nativeVector.y);
		}

		public static explicit operator Vector2(Vector3 vector)
		{
			return new Vector2
			{
				X = vector.X,
				Y = vector.Y
			};
		}

		public static explicit operator Vector2(Vector4 vector)
		{
			return new Vector2
			{
				X = vector.X,
				Y = vector.Y
			};
		}
		#endregion

		#region Operators
		public static Vector2 operator *(Vector2 v, float scale)
		{
			return new Vector2(v.X * scale, v.Y * scale);
		}

		public static Vector2 operator *(float scale, Vector2 v)
		{
			return v * scale;
		}

		public static Vector2 operator /(Vector2 v, float scale)
		{
			scale = 1.0f / scale;

			return new Vector2(v.X * scale, v.Y * scale);
		}

		public static Vector2 operator +(Vector2 v0, Vector2 v1)
		{
			return new Vector2(v0.X + v1.X, v0.Y + v1.Y);
		}

		public static Vector2 operator -(Vector2 v0, Vector2 v1)
		{
			return new Vector2(v0.X - v1.X, v0.Y - v1.Y);
		}
		
		public static Vector2 operator -(Vector2 v)
		{
			return v.Flipped;
		}

		public static Vector2 operator !(Vector2 v)
		{
			return v.Flipped;
		}

		public static bool operator ==(Vector2 left, Vector2 right)
		{
			return left.Equals(right);
		}

		public static bool operator !=(Vector2 left, Vector2 right)
		{
			return !(left == right);
		}
		#endregion

		#region Functions
		/// <summary>
		/// Returns the dot product of this Vector2 and Vector2 v.
		/// If the vectors are normalized the value will always be between 1 (vectors are the same) and -1 (vectors are opposites).
		/// </summary>
		/// <returns>The dot product.</returns>
		/// <param name="v">Other direction vector.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public float Dot(Vector2 v)
		{
			return Dot(this, v);
		}

		/// <summary>
		/// Returns the dot product of Vector2 a and b.
		/// If the vectors are normalized the value will always be between 1 (vectors are the same) and -1 (vectors are opposites).
		/// </summary>
		/// <returns>The dot product.</returns>
		/// <param name="a">Direction vector a.</param>
		/// <param name="b">Direction vector b.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float Dot(Vector2 a, Vector2 b)
		{
			return a._x * b._x + a._y * b._y;
		}

		[Obsolete("Please use IsNearlyZero")]
		public bool IsZero(float epsilon = 0)
		{
			return (Math.Abs(_x) <= epsilon) && (Math.Abs(_y) <= epsilon);
		}

		public bool IsNearlyZero()
		{
			return (Math.Abs(_x) <= MathHelpers.Epsilon && Math.Abs(_y) <= MathHelpers.Epsilon);
		}
		
		public static Vector2 Lerp(Vector2 p, Vector2 q, float t)
		{
			t = Math.Max(Math.Min(1.0f, t), 0f);
			return LerpUnclamped(p, q, t);
		}

		public static Vector2 LerpUnclamped(Vector2 p, Vector2 q, float t)
		{
			return p * (1.0f - t) + q * t;
		}

		public static Vector2 Slerp(Vector2 p, Vector2 q, float t)
		{
			t = Math.Max(Math.Min(1.0f, t), 0f);
			return SlerpUnclamped(p, q, t);
		}

		public static Vector2 SlerpUnclamped(Vector2 p, Vector2 q, float t)
		{
			CheckUnitVector(p, "p is not unit vector :"+p.ToString());
			CheckUnitVector(q, "q is not unit vector :"+q.ToString());
			float cosine = (p.x * q.x) + (p.y * q.y);

			if (MathHelpers.Approximately(cosine, 0.999999f, 0.000001f)) // vectors are almost parallel
			{
				// use lerp
				var result = Lerp(p, q, t);
				return result.Normalized;
			}
			var radians = (float)Math.Acos(cosine);
			var scale_0 = (float)Math.Sin((1 - t) * radians);
			var scale_1 = (float)Math.Sin(t * radians);
			Vector2 result2 = (p * scale_0 + q * scale_1) / (float)Math.Sin(radians);
			return result2.Normalized;
		}

		[Conditional("DEBUG")]
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		private static void CheckUnitVector(Vector2 vector, string messageIfFalse)
		{
			if (1.0f - (vector.x * vector.x + vector.y * vector.y) > 0.005f)
			{
				Log.Always<Vector2>(messageIfFalse);
			}
		}

		/// <summary>
		/// Returns the angle between Vector3 a and Vector3 b in radians.
		/// </summary>
		/// <returns>The angle in radians.</returns>
		/// <param name="a">The first Vector3.</param>
		/// <param name="b">The second Vector3.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float Angle(Vector2 a, Vector2 b)
		{
			var normA = (1.0f - a.LengthSquared) > 0.005f ? a.Normalized : a;
			var normB = (1.0f - b.LengthSquared) > 0.005f ? b.Normalized : b;
			return (float)Math.Acos(Dot(normA, normB));
		}
		#endregion

		#region Properties
		public float Length
		{
			get
			{
				return (float)Math.Sqrt(_x * _x + _y * _y);
			}
			set
			{
				float lengthSquared = _x * _x + _y * _y;
				if (lengthSquared < 0.00001f * 0.00001f)
					return;

				lengthSquared = value * MathHelpers.ISqrt(lengthSquared);
				_x *= lengthSquared;
				_y *= lengthSquared;
			}
		}

		public float Magnitude { get { return Length; } }

		public float LengthSquared
		{
			get
			{
				return _x * _x + _y * _y;
			}
		}
		
		public Vector2 Normalized
		{
			get
			{
				//if nearly (0,0) return current
				if (IsNearlyZero()) return this; 
				return this * 1.0f / (float)Math.Sqrt(_x * _x + _y * _y);
			}
		}
		
		public Vector2 Absolute
		{
			get
			{
				return new Vector2(Math.Abs(_x), Math.Abs(_y));
			}
		}

		public Vector2 Flipped
		{
			get
			{
				return new Vector2(-_x, -_y);
			}
		}

		/// <summary>
		/// Gets individual axes by index
		/// </summary>
		/// <param name="index">Index, 0 - 1 where 0 is X and 1 is Y</param>
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

					default:
					throw new ArgumentOutOfRangeException(nameof(index), "Indices must run from 0 to 1!");
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

					default:
					throw new ArgumentOutOfRangeException(nameof(index), "Indices must run from 0 to 1!");
				}
			}
		}
		#endregion
	}
}

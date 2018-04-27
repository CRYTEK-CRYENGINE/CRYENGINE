// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Globalization;
using System.Runtime.CompilerServices;
using System.Diagnostics;
using CryEngine.Common;

namespace CryEngine
{
	public struct Vector3 : IEquatable<Vector3>
	{
		public static readonly Vector3 Zero = new Vector3(0, 0, 0);
		public static readonly Vector3 One = new Vector3(1, 1, 1);

		public static readonly Vector3 Up = new Vector3(0, 0, 1);
		public static readonly Vector3 Down = new Vector3(0, 0, -1);
		public static readonly Vector3 Forward = new Vector3(0, 1, 0);
		public static readonly Vector3 Back = new Vector3(0, 1, 0);
		public static readonly Vector3 Left = new Vector3(-1, 0, 0);
		public static readonly Vector3 Right = new Vector3(1, 0, 0);

		private float _x;
		private float _y;
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

				hash = hash * 23 + _x.GetHashCode();
				hash = hash * 23 + _y.GetHashCode();
				hash = hash * 23 + _z.GetHashCode();

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
			return MathHelpers.IsEqual(_x, other.x) && MathHelpers.IsEqual(_y, other.y) && MathHelpers.IsEqual(_z, other.z);
		}

		public override string ToString()
		{
			return string.Format(CultureInfo.CurrentCulture, "{0},{1},{2}", _x, _y, _z);
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
				return Vector3.Zero;
			}

			return new Vector3(nativeVector.x, nativeVector.y, nativeVector.z);
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
		public float Dot(Vector3 v)
		{
			return _x * v.X + _y * v.Y + _z * v.Z;
		}

		public Vector3 Cross(Vector3 v)
		{
			return new Vector3(_y * v.Z - _z * v.Y, _z * v.X - _x * v.Z, _x * v.Y - _y * v.X);
		}

		[Obsolete("Please use IsNearlyZero")]
		public bool IsZero(float epsilon = 0)
		{
			return (Math.Abs(x) <= epsilon) && (Math.Abs(y) <= epsilon) && (Math.Abs(z) <= epsilon);
		}

		public bool IsNearlyZero()
		{
			return (Math.Abs(_x) <= MathHelpers.FloatEpsilon && Math.Abs(_y) <= MathHelpers.FloatEpsilon) && Math.Abs(_z) <= MathHelpers.FloatEpsilon;
		}

		
		public static Vector3 Lerp(Vector3 p, Vector3 q, float t)
		{
			t = Math.Max(Math.Min(1.0f, t), 0f);
			return Vector3.LerpUnclamped(p, q, t);
		}

		public static Vector3 LerpUnclamped(Vector3 p, Vector3 q, float t)
		{
			Vector3 diff = q - p;
			return p + (diff * t);
		}

		public static Vector3 Slerp(Vector3 p, Vector3 q, float t)
		{
			t = Math.Max(Math.Min(1.0f, t), 0f);
			return SlerpUnclamped(p, q, t);
		}

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
			
			if (MathHelpers.IsEqual(cosine-1, 0.000001f, MathHelpers.Precision.Precision_6))
			{
				// use lerp
				Vector3 result = LerpUnclamped(p, q, t);
				return result.Normalized;
			}
			float radians = (float)Math.Acos(cosine);
			float scale_0 = (float)Math.Sin((1 - t) * radians);
			float scale_1 = (float)Math.Sin(t * radians);
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
					Log.Always(messageIfFalse);
				#endif	
				errorFlag = false;
			}
			errorFlag = true;
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
						throw new ArgumentOutOfRangeException("index", "Indices must run from 0 to 2!");
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
						throw new ArgumentOutOfRangeException("index", "Indices must run from 0 to 2!");
				}
			}
		}
#endregion

	}
}

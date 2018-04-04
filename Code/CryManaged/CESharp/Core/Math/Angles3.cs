// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Globalization;
using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Helper class to indicate 3D angles in radians
	/// Allows for converting quaternions to human readable angles easily (and back to a quaternion)
	/// </summary>
	public struct Angles3 : IEquatable<Angles3>
	{
		private float _x;
		private float _y;
		private float _z;

		public float x { get { return _x; } set { _x = value; } }
		public float y { get { return _y; } set { _y = value; } }
		public float z { get { return _z; } set { _z = value; } }

		public float X { get { return _x; } set { _x = value; } }
		public float Y { get { return _y; } set { _y = value; } }
		public float Z { get { return _z; } set { _z = value; } }

		public Angles3(float xAngle, float yAngle, float zAngle)
		{
			_x = xAngle;
			_y = yAngle;
			_z = zAngle;
		}

		public Angles3(Quaternion quat)
		{
			_y = (float)Math.Asin(Math.Max(-1.0f, Math.Min(1.0f, -(quat.v.x * quat.v.z - quat.w * quat.v.y) * 2)));
			if (Math.Abs(Math.Abs(_y) - (Math.PI * 0.5f)) < 0.01f)
			{
				_x = 0;
				_z = (float)Math.Atan2(-2 * (quat.v.x * quat.v.y - quat.w * quat.v.z), 1 - (quat.v.x * quat.v.x + quat.v.z * quat.v.z) * 2);
			}
			else
			{
				_x = (float)Math.Atan2((quat.v.y * quat.v.z + quat.w * quat.v.x) * 2, 1 - (quat.v.x * quat.v.x + quat.v.y * quat.v.y) * 2);
				_z = (float)Math.Atan2((quat.v.x * quat.v.y + quat.w * quat.v.z) * 2, 1 - (quat.v.z * quat.v.z + quat.v.y * quat.v.y) * 2);
			}
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
#pragma warning restore RECS0025 // Non-readonly field referenced in 'GetHashCode()'

				return hash;
			}
		}

		public override bool Equals(object obj)
		{
			if (obj == null)
				return false;

			if (!(obj is Angles3 || obj is Ang3 || obj is Vec3))
				return false;

			return Equals((Angles3) obj);
		}

		public bool Equals(Angles3 other)
		{
			return MathHelpers.Approximately(_x, other.x) && MathHelpers.Approximately(_y, other.y) && MathHelpers.Approximately(_z, other.z);
		}

		public override string ToString()
		{
			return string.Format(CultureInfo.CurrentCulture, "{0},{1},{2}", _x, _y, _z);
		}
		#endregion

		#region Conversions
		public static implicit operator Vec3(Angles3 angles)
		{
			return new Vec3(angles.X, angles.Y, angles.Z);
		}

		public static implicit operator Ang3(Angles3 angles)
		{
			return new Ang3(angles.x, angles.y, angles.z);
		}
		public static implicit operator Angles3(Ang3 angles)
		{
			if (angles == null)
			{
				return new Angles3();
			}
			return new Angles3(angles.x, angles.y, angles.z);
		}

		public static implicit operator Angles3(Vec3 nativeAngles)
		{
			if(nativeAngles == null)
			{
				return Vector3.Zero;
			}

			return new Angles3(nativeAngles.x, nativeAngles.y, nativeAngles.z);
		}

		public static implicit operator Angles3(Vector3 vector)
		{
			return new Angles3(vector.x, vector.y, vector.z);
		}

		public static implicit operator Vector3(Angles3 angles)
		{
			return new Vector3(angles.x, angles.y, angles.z);
		}
		#endregion

		#region Operators
		public static Angles3 operator *(Angles3 v, float scale)
		{
			return new Angles3(v.X * scale, v.Y * scale, v.Z * scale);
		}

		public static Angles3 operator *(float scale, Angles3 v)
		{
			return v * scale;
		}

		public static Angles3 operator /(Angles3 v, float scale)
		{
			scale = 1.0f / scale;

			return new Vector3(v.X * scale, v.Y * scale, v.Z * scale);
		}

		public static Angles3 operator +(Angles3 v0, Angles3 v1)
		{
			return new Angles3(v0.X + v1.X, v0.Y + v1.Y, v0.Z + v1.Z);
		}

		public static Angles3 operator -(Angles3 v0, Angles3 v1)
		{
			return new Angles3(v0.X - v1.X, v0.Y - v1.Y, v0.Z - v1.Z);
		}

		public static Angles3 operator -(Angles3 v)
		{
			return v.Flipped;
		}

		public static Angles3 operator !(Angles3 v)
		{
			return v.Flipped;
		}

		public static bool operator ==(Angles3 left, Angles3 right)
		{
			return left.Equals(right);
		}

		public static bool operator !=(Angles3 left, Angles3 right)
		{
			return !(left == right);
		}
		#endregion

		#region Properties
		public Angles3 Absolute
		{
			get
			{
				return new Angles3(Math.Abs(_x), Math.Abs(_y), Math.Abs(_z));
			}
		}

		public Angles3 Flipped
		{
			get
			{
				return new Angles3(-_x, -_y, -_z);
			}
		}

		/// <summary>
		/// Converts the yaw and pitch to a view direction.
		/// x=yaw
		/// y=pitch
		/// z=roll (we ignore this element, since its not possible to convert the roll-component into a vector)
		/// </summary>
		/// <value>The view direction.</value>
		public Vector3 ViewDirection
		{
			get
			{
				return CCamera.CreateViewdir(this);
			}
		}
		
		public Quaternion Quaternion { get { return new Quaternion(this); } }

		/// <summary>
		/// Gets individual axes by index
		/// </summary>
		/// <param name="index">Index, 0 - 2 where 0 is X and 2 is Z</param>
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

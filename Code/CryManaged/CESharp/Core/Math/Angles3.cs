// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Globalization;
using System.Runtime.InteropServices;
using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Helper class to indicate 3D angles in radians
	/// Allows for converting quaternions to human readable angles easily (and back to a quaternion)
	/// </summary>
	[StructLayout(LayoutKind.Sequential)]
	public struct Angles3 : IEquatable<Angles3>
	{
		[MarshalAs(UnmanagedType.R4)]
		private float _x;
		[MarshalAs(UnmanagedType.R4)]
		private float _y;
		[MarshalAs(UnmanagedType.R4)]
		private float _z;

		/// <summary>
		/// The x-axis angle.
		/// </summary>
		public float x { get { return _x; } set { _x = value; } }
		/// <summary>
		/// The y-axis angle.
		/// </summary>
		public float y { get { return _y; } set { _y = value; } }
		/// <summary>
		/// The z-axis angle.
		/// </summary>
		public float z { get { return _z; } set { _z = value; } }

		/// <summary>
		/// The x-axis angle.
		/// </summary>
		public float X { get { return _x; } set { _x = value; } }
		/// <summary>
		/// The y-axis angle.
		/// </summary>
		public float Y { get { return _y; } set { _y = value; } }
		/// <summary>
		/// The z-axis angle.
		/// </summary>
		public float Z { get { return _z; } set { _z = value; } }

		/// <summary>
		/// Creates a new Angles3 object from three xyz radians.
		/// </summary>
		/// <param name="xAngle"></param>
		/// <param name="yAngle"></param>
		/// <param name="zAngle"></param>
		public Angles3(float xAngle, float yAngle, float zAngle)
		{
			_x = xAngle;
			_y = yAngle;
			_z = zAngle;
		}

		/// <summary>
		/// Creates a new Angles3 object from a Quaternion.
		/// </summary>
		/// <param name="quat"></param>
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
		/// <summary>
		/// Returns the hashcode of this instance.
		/// </summary>
		/// <returns></returns>
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

		/// <summary>
		/// Returns true if this <see cref="Angles3"/> is equal to the specified object.
		/// </summary>
		/// <param name="obj"></param>
		/// <returns></returns>
		public override bool Equals(object obj)
		{
			if (obj == null)
				return false;

			if (!(obj is Angles3 || obj is Ang3 || obj is Vec3))
				return false;

			return Equals((Angles3) obj);
		}

		/// <summary>
		/// Returns true if this <see cref="Angles3"/> is equal to the specified <see cref="Angles3"/>.
		/// </summary>
		/// <param name="other"></param>
		/// <returns></returns>
		public bool Equals(Angles3 other)
		{
			return MathHelpers.Approximately(_x, other.x) && MathHelpers.Approximately(_y, other.y) && MathHelpers.Approximately(_z, other.z);
		}

		/// <summary>
		/// Returns this <see cref="Angles3"/> in a formatted string. The format is "{X}, {Y}, {Z}".
		/// </summary>
		/// <returns></returns>
		public override string ToString()
		{
			return string.Format(CultureInfo.CurrentCulture, "{0}, {1}, {2}", _x, _y, _z);
		}
		#endregion

		#region Conversions
		/// <summary>
		/// Impicit converter from <see cref="Angles3"/> to <see cref="Vec3"/>.
		/// </summary>
		/// <param name="angles"></param>
		public static implicit operator Vec3(Angles3 angles)
		{
			return new Vec3(angles.X, angles.Y, angles.Z);
		}

		/// <summary>
		/// Impicit converter from <see cref="Angles3"/> to <see cref="Ang3"/>.
		/// </summary>
		/// <param name="angles"></param>
		public static implicit operator Ang3(Angles3 angles)
		{
			return new Ang3(angles.x, angles.y, angles.z);
		}

		/// <summary>
		/// Impicit converter from <see cref="Ang3"/> to <see cref="Angles3"/>.
		/// </summary>
		/// <param name="angles"></param>
		public static implicit operator Angles3(Ang3 angles)
		{
			if (angles == null)
			{
				return new Angles3();
			}
			return new Angles3(angles.x, angles.y, angles.z);
		}

		/// <summary>
		/// Impicit converter from <see cref="Vec3"/> to <see cref="Angles3"/>.
		/// </summary>
		/// <param name="angles"></param>
		public static implicit operator Angles3(Vec3 angles)
		{
			if(angles == null)
			{
				return Vector3.Zero;
			}

			return new Angles3(angles.x, angles.y, angles.z);
		}

		/// <summary>
		/// Impicit converter from <see cref="Vector3"/> to <see cref="Angles3"/>.
		/// </summary>
		/// <param name="angles"></param>
		public static implicit operator Angles3(Vector3 angles)
		{
			return new Angles3(angles.x, angles.y, angles.z);
		}

		/// <summary>
		/// Impicit converter from <see cref="Angles3"/> to <see cref="Vector3"/>.
		/// </summary>
		/// <param name="angles"></param>
		public static implicit operator Vector3(Angles3 angles)
		{
			return new Vector3(angles.x, angles.y, angles.z);
		}
		#endregion

		#region Operators
		/// <summary>
		/// Scale the angles by the value of <paramref name="scale"/>.
		/// </summary>
		/// <param name="v"></param>
		/// <param name="scale"></param>
		/// <returns></returns>
		public static Angles3 operator *(Angles3 v, float scale)
		{
			return new Angles3(v.X * scale, v.Y * scale, v.Z * scale);
		}

		/// <summary>
		/// Scale the angles by the value of <paramref name="scale"/>
		/// </summary>
		/// <param name="scale"></param>
		/// <param name="v"></param>
		/// <returns></returns>
		public static Angles3 operator *(float scale, Angles3 v)
		{
			return v * scale;
		}

		/// <summary>
		/// Divides the angles by <paramref name="scale"/>.
		/// </summary>
		/// <param name="v"></param>
		/// <param name="scale"></param>
		/// <returns></returns>
		public static Angles3 operator /(Angles3 v, float scale)
		{
			scale = 1.0f / scale;

			return new Vector3(v.X * scale, v.Y * scale, v.Z * scale);
		}

		/// <summary>
		/// Combine two <see cref="Angles3"/> component wise together.
		/// </summary>
		/// <param name="v0"></param>
		/// <param name="v1"></param>
		/// <returns></returns>
		public static Angles3 operator +(Angles3 v0, Angles3 v1)
		{
			return new Angles3(v0.X + v1.X, v0.Y + v1.Y, v0.Z + v1.Z);
		}

		/// <summary>
		/// Subtract two <see cref="Angles3"/> component wise from eachother.
		/// </summary>
		/// <param name="v0"></param>
		/// <param name="v1"></param>
		/// <returns></returns>
		public static Angles3 operator -(Angles3 v0, Angles3 v1)
		{
			return new Angles3(v0.X - v1.X, v0.Y - v1.Y, v0.Z - v1.Z);
		}

		/// <summary>
		/// Flip the values of the <see cref="Angles3"/>.
		/// </summary>
		/// <param name="v"></param>
		/// <returns></returns>
		public static Angles3 operator -(Angles3 v)
		{
			return v.Flipped;
		}

		/// <summary>
		/// Flip the values of the <see cref="Angles3"/>.
		/// </summary>
		/// <param name="v"></param>
		/// <returns></returns>
		public static Angles3 operator !(Angles3 v)
		{
			return v.Flipped;
		}

		/// <summary>
		/// Compare two values of <see cref="Angles3"/> component wise. Returns true if both are equal.
		/// </summary>
		/// <param name="left"></param>
		/// <param name="right"></param>
		/// <returns></returns>
		public static bool operator ==(Angles3 left, Angles3 right)
		{
			return left.Equals(right);
		}

		/// <summary>
		/// Compare two values of <see cref="Angles3"/> component wise. Returns true if both are inequal.
		/// </summary>
		/// <param name="left"></param>
		/// <param name="right"></param>
		/// <returns></returns>
		public static bool operator !=(Angles3 left, Angles3 right)
		{
			return !(left == right);
		}
		#endregion

		#region Properties
		/// <summary>
		/// Returns a new <see cref="Angles3"/> with absolute values.
		/// </summary>
		public Angles3 Absolute
		{
			get
			{
				return new Angles3(Math.Abs(_x), Math.Abs(_y), Math.Abs(_z));
			}
		}

		/// <summary>
		/// Returns a new <see cref="Angles3"/> with flipped values.
		/// </summary>
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
		
		/// <summary>
		/// Get this <see cref="Angles3"/> as a Quaternion.
		/// </summary>
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

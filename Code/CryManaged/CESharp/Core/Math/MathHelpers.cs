// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using System;
using System.Runtime.CompilerServices;

namespace CryEngine
{
	public static class MathHelpers
	{
		public enum Precision
		{
			Precision_Default,
			Precision_1,
			Precision_2,
			Precision_3,
			Precision_4,
			Precision_5,
			Precision_6,
			Precision_7

		}

		private const float MagicNumber = 1.175494E-37f;
		private const float FloatMinVal = float.Epsilon;
		public static bool IsDeNormalizedFloatEnabled = (double)FloatMinVal == 0.0d;

		public static readonly float FloatEpsilon = IsDeNormalizedFloatEnabled ? MagicNumber : FloatMinVal;
		
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static bool IsEqual(float lhs, float rhs)
		{
			return (Math.Abs(lhs - rhs) <= FloatEpsilon) ;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static bool IsEqual(float lhs, float rhs, Precision precision)
		{
			if (precision == Precision.Precision_Default)
			{
				return IsEqual(lhs, rhs);
			}
			else
			{
				float multiplier = (float)Math.Pow(10, (double)precision);
				float mlhs = ((int)(lhs * multiplier)) / ((float)(multiplier));
				float mrhs = ((int)(rhs * multiplier)) / ((float)(multiplier));
				float diffe = mlhs - mrhs;
				diffe = Math.Abs(diffe);
				return (diffe <= FloatEpsilon);
			}
			
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float Clamp(float min, float max, float value)
		{
			return Math.Min(Math.Max(min, value), max);
		}

		/// <summary>
		/// Convert degrees to radians.
		/// </summary>
		/// <returns>Angle in radians.</returns>
		/// <param name="degrees">Angle in degrees.</param>
		public static float DegreesToRadians(float degrees)
		{
			return degrees * ((float)Math.PI / 180f);
		}

		/// <summary>
		/// Convert Radians to degrees.
		/// </summary>
		/// <returns>Angle in degrees.</returns>
		/// <param name="rad">Angle in radians.</param>
		public static float RadiansToDegrees(float rad)
		{
			return rad * (180f / (float)Math.PI);
		}

		/// <summary>
		/// Determines whether a value is inside the specified range.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="value"></param>
		/// <param name="min"></param>
		/// <param name="max"></param>
		/// <returns></returns>
		public static bool IsInRange<T>(T value, T min, T max) where T : IComparable<T>
		{
			if (value.CompareTo(min) >= 0 && value.CompareTo(max) <= 0)
				return true;

			return false;
		}

		/// <summary>
		/// Clamps a value given a specified range.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="value"></param>
		/// <param name="min"></param>
		/// <param name="max"></param>
		/// <returns></returns>
		public static T Clamp<T>(T value, T min, T max) where T : IComparable<T>
		{
			if (value.CompareTo(min) < 0)
				return min;
			if (value.CompareTo(max) > 0)
				return max;

			return value;
		}

		public static float ClampAngleDegrees(float angle, float min, float max)
		{
			if (angle < -360)
				angle += 360;
			if (angle > 360)
				angle -= 360;

			return Clamp(angle, min, max);
		}

		public static T Max<T>(T val1, T val2) where T : IComparable<T>
		{
			if (val1.CompareTo(val2) > 0)
				return val1;

			return val2;
		}

		public static T Min<T>(T val1, T val2) where T : IComparable<T>
		{
			if (val1.CompareTo(val2) < 0)
				return val1;

			return val2;
		}

		public static bool IsPowerOfTwo(int value)
		{
			return (value & (value - 1)) == 0;
		}

		public static float ISqrt(float d)
		{
			return 1f / (float)Math.Sqrt(d);
		}

		public static void SinCos(float angle, out float sin, out float cos)
		{
			sin = (float)Math.Sin(angle);
			cos = (float)Math.Cos(angle);
		}

		public static float Square(float value)
		{
			return value * value;
		}

		public static float LerpUnclamped(float a, float b, float t)
		{
			return a + ((b - a) * t);
		}

		public static float Lerp(float a , float b, float t)
		{
			t = Math.Max(Math.Min(1.0f, t), 0f);
			return Lerp(a, b, t);
		}

		[Obsolete("Please use Vector3.Lerp")]
		public static Vector3 Lerp(Vector3 a, Vector3 b, float t)
		{
			return a + ((b - a) * t);
		}
	}
}

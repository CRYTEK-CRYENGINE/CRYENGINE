// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using System;
using System.Runtime.CompilerServices;

namespace CryEngine
{
	namespace Debug.MathHelpers
	{
		/// <summary>
		/// Helper class for calculations that involve epsilon.
		/// </summary>
		public static class EpsilonData
		{
			// The float.Epsilon can be flushed to 0 in certain cases. That's why a slightly higher value is used instead in those cases.
			internal const float MagicNumber = 1.175494E-37f;
			internal const float FloatMinVal = float.Epsilon;

			/// <summary>
			/// Determines whether floats are normalized to zero. If the minimal value is flushed to zero we use a slightly bigger number for Epsilon instead.
			/// </summary>
			public static bool IsDeNormalizedFloatEnabled = FloatMinVal == 0.0d;
		}
	}

	/// <summary>
	/// Helper class for common math operations.
	/// </summary>
	public static class MathHelpers
	{
		/// <summary>
		/// The smallest number a float can be, without being zero.
		/// </summary>
		public static readonly float Epsilon = Debug.MathHelpers.EpsilonData.IsDeNormalizedFloatEnabled ? Debug.MathHelpers.EpsilonData.MagicNumber : Debug.MathHelpers.EpsilonData.FloatMinVal;

		/// <summary>
		/// Returns true if absolute difference between lhs and rhs is less than or equal to Epsilon
		/// </summary>
		/// <param name="lhs"></param>
		/// <param name="rhs"></param>
		/// <returns></returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static bool Approximately(float lhs, float rhs)
		{
			return Approximately(lhs, rhs, Epsilon);
		}

		/// <summary>
		/// Returns true if absolute difference between lhs and rhs is less than or equal to precision
		/// </summary>
		/// <param name="lhs"></param>
		/// <param name="rhs"></param>
		/// <param name="precision"></param>
		/// <returns></returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static bool Approximately(float lhs, float rhs, float precision)
		{
			return Math.Abs(lhs - rhs) <= precision;
		}
			
		/// <summary>
		/// Clamps specified float value between minimum float and maximum float and returns the clamped float value
		/// </summary>
		/// <param name="value"></param>
		/// <param name="min"></param>
		/// <param name="max"></param>
		/// <returns></returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float Clamp(float value, float min, float max)
		{
			return Math.Min(Math.Max(min, value), max);
		}

		/// <summary>
		/// Clamps the specified integer value between minimum integer and maximum integer and returns the clamped integer value
		/// </summary>
		/// <param name="value"></param>
		/// <param name="min"></param>
		/// <param name="max"></param>
		/// <returns></returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static int Clamp(int value, int min, int max)
		{
			return Math.Min(Math.Max(min, value), max);
		}

		/// <summary>
		/// Clamps the specified value between 0.0 and 1.0.
		/// </summary>
		/// <returns>The value clamped between 0.0 and 1.0.</returns>
		/// <param name="value">Value that needs to be clamped.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float Clamp01(float value)
		{
			return Math.Min(Math.Max(0.0f, value), 1.0f);
		}

		/// <summary>
		/// Convert degrees to radians.
		/// </summary>
		/// <returns>Angle in radians.</returns>
		/// <param name="degrees">Angle in degrees.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float DegreesToRadians(float degrees)
		{
			return degrees * ((float)Math.PI / 180f);
		}

		/// <summary>
		/// Convert Radians to degrees.
		/// </summary>
		/// <returns>Angle in degrees.</returns>
		/// <param name="rad">Angle in radians.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
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
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
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
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		[Obsolete("Please use the non-generic Clamp functions")]
		public static T Clamp<T>(T value, T min, T max) where T : IComparable<T>
		{
			if (value.CompareTo(min) < 0)
				return min;
			if (value.CompareTo(max) > 0)
				return max;

			return value;
		}
		
		/// <summary>
		/// Clamps the specified angle between minimum angle and maximum angle and returns the clamped value
		/// </summary>
		/// <param name="angle"></param>
		/// <param name="min"></param>
		/// <param name="max"></param>
		/// <returns></returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float ClampAngleDegrees(float angle, float min, float max)
		{
			if (angle < -360)
				angle += 360;
			if (angle > 360)
				angle -= 360;

			return Clamp(angle, min, max);
		}

		/// <summary>
		/// Returns the largest of the 2 specified values
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="val1"></param>
		/// <param name="val2"></param>
		/// <returns></returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static T Max<T>(T val1, T val2) where T : IComparable<T>
		{
			if (val1.CompareTo(val2) > 0)
				return val1;

			return val2;
		}

		/// <summary>
		/// Returns the smallest of the 2 specified values
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="val1"></param>
		/// <param name="val2"></param>
		/// <returns></returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static T Min<T>(T val1, T val2) where T : IComparable<T>
		{
			if (val1.CompareTo(val2) < 0)
				return val1;

			return val2;
		}

		/// <summary>
		/// Returns true if the specified int value is a power of two
		/// </summary>
		/// <param name="value"></param>
		/// <returns></returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static bool IsPowerOfTwo(int value)
		{
			return (value & (value - 1)) == 0;
		}

		/// <summary>
		/// Returns the inversed square root of the specified value
		/// </summary>
		/// <param name="d"></param>
		/// <returns></returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float ISqrt(float d)
		{
			return 1f / (float)Math.Sqrt(d);
		}

		/// <summary>
		/// Outputs the sine and cosine of specified angle in radians when called
		/// </summary>
		/// <param name="angle"></param>
		/// <param name="sin"></param>
		/// <param name="cos"></param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static void SinCos(float angle, out float sin, out float cos)
		{
			sin = (float)Math.Sin(angle);
			cos = (float)Math.Cos(angle);
		}

		/// <summary>
		/// Returns the square of the specified value
		/// </summary>
		/// <param name="value"></param>
		/// <returns></returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float Square(float value)
		{
			return value * value;
		}

		/// <summary>
		/// Returns the linearly interpolated value between a and b with no restriction to t
		/// </summary>
		/// <param name="a"></param>
		/// <param name="b"></param>
		/// <param name="t"></param>
		/// <returns></returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float LerpUnclamped(float a, float b, float t)
		{
			return a + ((b - a) * t);
		}

		/// <summary>
		/// Returns the clamped value a and b
		/// </summary>
		/// <param name="a"></param>
		/// <param name="b"></param>
		/// <param name="t"></param>
		/// <returns></returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float Lerp(float a , float b, float t)
		{
			t = Math.Max(Math.Min(1.0f, t), 0f);
			return LerpUnclamped(a, b, t);
		}

		/// <summary>
		/// Returns the unclamped linearly interpolated vector between a and b
		/// </summary>
		/// <param name="a"></param>
		/// <param name="b"></param>
		/// <param name="t"></param>
		/// <returns></returns>
		[Obsolete("Please use Vector3.Lerp")]
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector3 Lerp(Vector3 a, Vector3 b, float t)
		{
			return a + ((b - a) * t);
		}

		/// <summary>
		/// Keeps the value between 0 and length, but instead of clamping the value it will loop it.
		/// </summary>
		/// <returns>The repeated value.</returns>
		/// <param name="t">T.</param>
		/// <param name="length">Length.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float Repeat(float length, float t)
		{
			return t - (float)Math.Floor(t / length) * length;
		}

		/// <summary>
		/// Keeps the value between 0 and length, but instead of clamping the value it will move back and forth between 0 and length.
		/// </summary>
		/// <returns>The ping ponged value.</returns>
		/// <param name="t">T.</param>
		/// <param name="length">Length.</param>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float PingPong(float length, float t)
		{
			t = Repeat(t, length * 2.0f);
			return length - Math.Abs(t - length);
		}
	}
}

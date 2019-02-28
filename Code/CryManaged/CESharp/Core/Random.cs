// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace CryEngine
{
	public static class Random
	{
		private static readonly System.Random _random = new System.Random();
		private static readonly object _syncLock = new object();

		/// <summary>
		/// Get a float that is greater than or equal to 0.0, and less than 1.0.
		/// </summary>
		/// <value>The random value.</value>
		public static float Value
		{
			get
			{
				lock(_syncLock)
				{
					return (float)_random.NextDouble();
				}
			}
		}

		/// <summary>
		/// Returns a random float that is greater or equal to <paramref name="minValue"/> and less than <paramref name="maxValue"/>.
		/// </summary>
		/// <returns>The random value.</returns>
		/// <param name="minValue">The inclusive lower bound of the random value returned.</param>
		/// <param name="maxValue">The exclusive upper bound of the random value returned. <paramref name="maxValue"/> must be greater than or equal to <paramref name="minValue"/>.</param>
		public static float Range(float minValue, float maxValue)
		{
			if(minValue > maxValue)
			{
				throw new System.ArgumentOutOfRangeException(nameof(maxValue), 
				                                             string.Format("The value of {0} is greater than the value of {1}!", nameof(minValue), nameof(maxValue)));
			}
			float value;
			lock(_syncLock)
			{
				value = (float)_random.NextDouble();
			}
			return (maxValue - minValue) * value + minValue;
		}

		/// <summary>
		/// Returns a random integer that is greater or equal to <paramref name="minValue"/> and less than <paramref name="maxValue"/>.
		/// </summary>
		/// <returns>A 32-bit signed integer greater than or equal to <paramref name="minValue"/> and less than <paramref name="maxValue"/>; 
		/// that is, the range of return values includes <paramref name="minValue"/> but not <paramref name="maxValue"/>. 
		/// If <paramref name="minValue"/> equals <paramref name="maxValue"/>, <paramref name="minValue"/> is returned.</returns>
		/// <param name="minValue">The inclusive lower bound of the random number returned.</param>
		/// <param name="maxValue">The exclusive upper bound of the random number returned. <paramref name="maxValue"/> must be greater than or equal to <paramref name="minValue"/>.</param>
		public static int Range(int minValue, int maxValue)
		{
			if(minValue > maxValue)
			{
				throw new System.ArgumentOutOfRangeException(nameof(maxValue), 
				                                             string.Format("The value of {0} is greater than the value of {1}!", nameof(minValue), nameof(maxValue)));
			}

			lock(_syncLock)
			{
				return _random.Next(minValue, maxValue);
			}
		}

		/// <summary>
		/// Returns a random integer that is greater or equal to <paramref name="minValue"/> and less than <paramref name="maxValue"/>.
		/// </summary>
		/// <returns>A 32-bit signed integer greater than or equal to <paramref name="minValue"/> and less than <paramref name="maxValue"/>; 
		/// that is, the range of return values includes <paramref name="minValue"/> but not <paramref name="maxValue"/>. 
		/// If <paramref name="minValue"/> equals <paramref name="maxValue"/>, <paramref name="minValue"/> is returned.</returns>
		/// <param name="minValue">The inclusive lower bound of the random number returned.</param>
		/// <param name="maxValue">The exclusive upper bound of the random number returned. <paramref name="maxValue"/> must be greater than or equal to <paramref name="minValue"/>.</param>
		public static int Next(int minValue, int maxValue)
		{
			return Range(minValue, maxValue);
		}

		/// <summary>
		/// Returns a random integer that is greater or equal to 0 and less than <paramref name="maxValue"/>.
		/// </summary>
		/// <returns>A 32-bit signed integer that is greater than or equal to 0, and less than <paramref name="maxValue"/>; 
		/// that is, the range of return values ordinarily includes 0 but not <paramref name="maxValue"/>. 
		/// However, if <paramref name="maxValue"/> equals 0, <paramref name="maxValue"/> is returned.</returns>
		/// <param name="maxValue">Max value.</param>
		public static int Next(int maxValue)
		{
			if(maxValue < 0)
			{
				throw new System.ArgumentOutOfRangeException(nameof(maxValue));
			}

			lock (_syncLock)
			{
				return _random.Next(maxValue);
			}
		}

		/// <summary>
		/// Returns a non-negative random integer.
		/// </summary>
		/// <returns>A 32-bit signed integer that is greater than or equal to 0 and less than int.MaxValue.</returns>
		public static int Next()
		{
			lock (_syncLock)
			{
				return _random.Next();
			}
		}
	}
}

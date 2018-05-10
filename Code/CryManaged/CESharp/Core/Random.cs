using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace CryEngine
{
	public static class Random
	{
		private static readonly System.Random _random = new System.Random();
		private static readonly object _syncLock = new object();

		public static int Next(int minValue, int maxValue)
		{
			lock (_syncLock)
			{
				return _random.Next(minValue, maxValue);
			}
		}

		public static int Next(int maxValue)
		{
			lock (_syncLock)
			{
				return _random.Next(maxValue);
			}
		}

		public static int Next()
		{
			lock (_syncLock)
			{
				return _random.Next();
			}
		}
	}
}

// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using NUnit.Framework;
using CryEngine;
using CryEngine.Common;
using System;
using System.Reflection;

namespace Math.Tests
{
	[TestFixture]
	public class MathHelpersTest
	{
		[Test]
		public void TestEpsilonRules()
		{
			float minVal = float.MinValue;
			float minValIncre = minVal + CryEngine.MathHelpers.Epsilon;
			Assert.IsTrue(MathHelpers.Approximately(minVal, minValIncre), "Epsilon test failed for float min increment");

			float maxValDecr = minVal - CryEngine.MathHelpers.Epsilon;
			Assert.IsTrue(MathHelpers.Approximately(maxValDecr, minVal), "Epsilon test failed for float min decrement");

			float zeroVal = 0f;
			Assert.IsTrue(MathHelpers.Approximately(zeroVal + CryEngine.MathHelpers.Epsilon, CryEngine.MathHelpers.Epsilon), "Epsilon test failed for 0 increment");
			Assert.IsTrue(MathHelpers.Approximately(zeroVal - CryEngine.MathHelpers.Epsilon, -CryEngine.MathHelpers.Epsilon), "Epsilon test failed for 0 decrement");
		}

		[Test]
		public void TestApproximately()
		{
			{
				float a = 1.10234567f;
				Assert.IsTrue(MathHelpers.Approximately(a, a, CryEngine.MathHelpers.Epsilon), "Approximately failed for no precision test");
			}
			{
				float a = 1.10234567f;
				float b = 1.1234567f;
				Assert.IsTrue(MathHelpers.Approximately(a, b, 0.1f), "Approximately failed for precision 1");
			}
			{
				float a = 1.120234567f;
				float b = 1.1234567f;
				Assert.IsTrue(MathHelpers.Approximately(a, b, 0.01f), "Approximately failed for precision 2 a");
			}
			{
				float a = 1.1230567f;
				float b = 1.1234567f;
				Assert.IsTrue(MathHelpers.Approximately(a, b, 0.001f), "Approximately failed for precision 3 a");
			}
			{
				float a = 1.1234567f;
				float b = 1.1234789f;
				Assert.IsTrue(MathHelpers.Approximately(a, b, 0.0001f), "Approximately failed for precision 4 a");
			}
			{
				float a = 1.1234512f;
				float b = 1.1234523f;
				Assert.IsTrue(MathHelpers.Approximately(a, b, 0.00001f), "Approximately failed for precision 5 a");
			}
			{
				float a = 1.1234560f;
				float b = 1.1234563f;
				Assert.IsTrue(MathHelpers.Approximately(a, b, 0.000001f), "Approximately failed for precision 6 a");
			}
			{
				float a = 1.12345678f;
				float b = 1.12345679f;
				Assert.IsTrue(MathHelpers.Approximately(a, b, 0.0000001f), "Approximately failed for precision 7 a");
			}
			{
				//test value "1"
				float a = 1.0f;
				float b = 1 / 1.0f;
				Assert.IsTrue(MathHelpers.Approximately(a, b, CryEngine.MathHelpers.Epsilon));

			}
			{
				//use Quaternion result to test 
			}
		}

		[Test]
		public void TestClamp()
		{
			{
				//float
				float minVal = 10f;
				float maxVal = 20.5f;
				float fval = 21.5f;
				float clampedValue = MathHelpers.Clamp(fval, minVal, maxVal);
				Assert.IsTrue(clampedValue <= maxVal && clampedValue >= minVal);
				Assert.IsTrue(MathHelpers.Approximately(clampedValue, 20.5000000f, 0.0000001f));

				float fval2 = 9.0f;
				float clampedValue2 = MathHelpers.Clamp(fval2, minVal, maxVal);
				Assert.IsTrue(clampedValue2 <= maxVal && clampedValue2 >= minVal);
				Assert.IsTrue(MathHelpers.Approximately(clampedValue2, 10.0000000f, 0.0000001f));
			}
			{
				//int
				int minVal = 10;
				int maxVal = 20;
				int ival = 21;
				int clampedValue = MathHelpers.Clamp(ival, minVal, maxVal);
				Assert.IsTrue(clampedValue <= maxVal && clampedValue >= minVal);
				ival = 20;
				clampedValue = MathHelpers.Clamp(ival, minVal, maxVal);
				Assert.IsTrue(clampedValue <= maxVal && clampedValue >= minVal);
				Assert.IsTrue(clampedValue == 20);

				int ival2 = 9;
				int clampedValue2 = MathHelpers.Clamp(ival2, minVal, maxVal);
				Assert.IsTrue(clampedValue2 <= maxVal && clampedValue2 >= minVal);
				ival2 = 10;
				clampedValue2 = MathHelpers.Clamp(ival2, minVal, maxVal);
				Assert.IsTrue(clampedValue2 <= maxVal && clampedValue2 >= minVal);
				Assert.IsTrue(clampedValue2 == 10);
			}
			{
				//clamp01
				float val = -0.1f;
				float clamped = MathHelpers.Clamp01(val);
				Assert.IsTrue(MathHelpers.Approximately(clamped, 0.0000000f, 0.0000001f));

				val = 1.0000001f;
				clamped = MathHelpers.Clamp01(val);
				Assert.IsTrue(MathHelpers.Approximately(clamped, 1.0000000f, 0.0000001f));
			}

		}
	}
}
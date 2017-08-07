// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using NUnit.Framework;
using CryEngine;
using CryEngine.Common;

using System.Diagnostics;
using System;

namespace Math.Tests
{

	public static partial class NativeExtensions
	{
		//native Vec4 doesn't overrride ToString to print correctly
		public static string PrintString(this Vec4 vec)
		{
			return "Vec4[" + vec.x + "," + vec.y + "," + vec.z + "," + vec.w + "]";
		}

		public static bool TestVectorsAreEqual(Vector4 vec, Vector4 vector2, float precision = 0.000001f)
		{
			return MathHelpers.Approximately(vec.x, vector2.x, precision) && MathHelpers.Approximately(vec.y, vector2.y, precision) && MathHelpers.Approximately(vec.z, vector2.z, precision) && MathHelpers.Approximately(vec.w, vector2.w, precision);
		}
	}

	[TestFixture]
	public class Vector4Test
	{
		[Test]
		public void TestLerp()
		{
			{
				//native
				Vec4 vec4p = new Vec4(1f, 1f, 0f, 0f);
				vec4p.Normalize();
				Vec4 vec4q = new Vec4(0f, 1f, 0f, 0f);
				vec4q.Normalize();
				Vec4 vec4Mid = Vec4.CreateLerp(vec4p, vec4q, 0.5f);
				//managed
				Vector4 vector4p = new Vector4(vec4p.x, vec4p.y, vec4p.z, vec4p.w);
				Vector4 vector4q = new Vector4(vec4q.x, vec4q.y, vec4q.z, vec4q.w);
				Vector4 vector4Mid = Vector4.Lerp(vector4p, vector4q, 0.5f);

				Assert.IsTrue(vec4Mid == vector4Mid, " Lerp tests fails for native and managed vector4 1" + vec4Mid.PrintString() + "," + vector4Mid.ToString());
			}

			{
				//native
				Vec4 vec4p = new Vec4(1f, 0f, 0f, 0f);
				Vec4 vec4q = new Vec4(0f, 1f, 0f, 0f);
				Vec4 vec4Mid = Vec4.CreateLerp(vec4p, vec4q, 0.5f);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(vec4Mid, new Vector4(0.5f, 0.5f, 0f, 0f)), "Lerp tests fails for native vec4 2" + vec4Mid.PrintString());

				//managed
				Vector4 vector4p = new Vector4(vec4p.x, vec4p.y, vec4p.z, vec4p.w);
				Vector4 vector4q = new Vector4(vec4q.x, vec4q.y, vec4q.z, vec4q.w);
				Vector4 vector4Mid = Vector4.Lerp(vector4p, vector4q, 0.5f);
				Assert.IsTrue(vec4Mid == vector4Mid, " Lerp tests fails for native and managed vector4 4" + vec4Mid.PrintString() + "," + vector4Mid.ToString());
			}

			{
				//native
				Vec4 vec4p = new Vec4(0.5f, 0f, 0f, 0f);
				Vec4 vec4q = new Vec4(0.5f, 0f, 0f, 0f);
				Vec4 lerpVec = Vec4.CreateLerp(vec4p, vec4q, 0.5f);
				Vec4 expectedResult = new Vec4(0.5f, 0f, 0f, 0f);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(lerpVec, expectedResult), "Lerp tests fail for native vec4 " + "Expected :" + expectedResult.PrintString() + ", Actual :" + lerpVec.PrintString());

				//managed
				Vector4 vector4p = new Vector4(0.5f, 0f, 0f, 0f);
				Vector4 vector4q = new Vector4(0.5f, 0f, 0f, 0f);
				Vector4 lerpVector4 = Vector4.Lerp(vector4p, vector4q, 0.5f);
				Vector4 expectedResult2 = new Vector4(0.5f, 0, 0, 0);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(lerpVector4, expectedResult2), "Lerp tests fail for managed vector4 " + "Expected :" + expectedResult2.ToString() + ", Actual :" + lerpVector4.ToString());
			}

			{
				Vector4 p = new Vector4(1.0f, 0f, 0f, 0f);
				Vector4 q = new Vector4(-1f, 0f, 0f, 0f);

				//middle of interpolation will be (0,0,0,0)
				Vector4 expectedResult = new Vector4(0, 0, 0, 0);

				Vector4 actualResult = Vector4.Lerp(p, q, 0.5f);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(expectedResult, actualResult), "Lerp fails for managed vector4 " + "Expected :" + expectedResult + ", Actual :" + actualResult);
			}
		}
	}
}
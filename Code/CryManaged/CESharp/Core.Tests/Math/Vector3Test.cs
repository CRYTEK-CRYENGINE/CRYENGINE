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
		//native Vec3 doesn't overrride ToString to print correctly
		public static string PrintString(this Vec3 vec)
		{
			return "Vec3[" + vec.x + "," + vec.y + "," + vec.z + "]";
		}

		public static bool TestVectorsAreEqual(Vector3 vec, Vector3 vector2, float precision = 0.000001f)
		{
			Global.CryLogAlways("vec :" + vec.ToString());
			Global.CryLogAlways("vector2 :" + vector2.ToString());
			return MathHelpers.Approximately(vec.x, vector2.x, precision) && MathHelpers.Approximately(vec.y, vector2.y, precision) && MathHelpers.Approximately(vec.z, vector2.z, precision);
		}
	}

	[TestFixture]
	public class Vector3Test
	{

		[Test]
		public void TestZero()
		{
			Vector3 vec3 = new Vector3(0, 0, 0);
			Assert.IsTrue(vec3 == Vector3.Zero, " Zero equality fails for Vector3.Zero");

			Vec3 vec3native = new Vec3(0, 0, 0);
			Assert.IsTrue(vec3native == vec3, "Zero equality fails for native and managed");

			//this should not throw an error
			Vector3 normalized = vec3.Normalized;
			Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(vec3, normalized), "Zero equality fails for normalized 'zero' point");
		}

		[Test]
		public void TestProperties()
		{
			float x = 123.4f;
			float y = 453156.05567f;
			float z = 123.098f;

			Vector3 vec3 = new Vector3(x, y, z);

			Assert.IsTrue(vec3.x == x, "x Property Vector3 fails");
			Assert.IsTrue(vec3.y == y, "y Property Vector3 fails");
			Assert.IsTrue(vec3.z == z, "z Property Vector3 fails");

			Assert.IsTrue(vec3.X == x, "X Property Vector3 fails");
			Assert.IsTrue(vec3.Y == y, "Y Property Vector3 fails");
			Assert.IsTrue(vec3.Z == z, "Z Property Vector3 fails");
		}

		[Test]
		public void TestEquality()
		{
			/**
			 * 3 properties for Equality test
			 * 1) Reflexive : a == a 
			 * 2) Symmetric : a == b => b == a (for both true or false)
			 * 3) Transitive : a ==b , b ==c => a == c 
			 * */
			Vector3 a = new Vector3(5.2f, 5.3f, 7.9f);
			Vec3 a2 = new Vec3(a.x, a.y, a.z);
#pragma warning disable 1718 // disable warning as test for comparison to ownself is intended
			Assert.IsTrue(a == a, "Reflexive test fails for Vector3");
			Assert.IsTrue(a2 == a2, "Reflexive test fails for Vec3");
#pragma warning restore 1718 // enable warning

			Vector3 b = new Vector3(a.x, a.y, a.z);
			Vec3 b2 = new Vec3(a.x, a.y, a.z);
			Assert.IsTrue((a == b) && (b == a), "Symmetric test fails for Vector3");

			Vector3 c = new Vector3(a.x, a.y, a.z);
			Assert.IsTrue((a == b && b == c && a == c), "Transistive test fails for Vector3");
		}

		[Test]
		public void TestGetHashCode()
		{
			Vector3 a = new Vector3(5.2f, 5.3f, 100.432f);
			Vector3 b = new Vector3(a.x, a.y, a.z);

			Assert.IsTrue(a.GetHashCode() == b.GetHashCode(), "GetHashcode test fails for Vector3");
		}

		[Test]
		public void TestOperators()
		{
			Vector3 a = new Vector3(4.5f, 65.9f, 98.32f);
			float scale = 15.6f;
			{
				Vector3 b = a * scale;
				Vector3 d = new Vector3(a.x * scale, a.y * scale, a.z * scale);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(b, d), "Operator* (Vector3 v, float scale) test fails for Vector3");
			}
			{
				Vector3 b = a * scale;
				Vector3 d = scale * a;
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(b, d), "Operator * (float scale,Vector3 v) test fails for Vector3");
			}
			{
				Vector3 b = a / scale;
				float multiplier = 1 / scale;
				Vector3 c = new Vector3(a.x * multiplier, a.y * multiplier, a.z * multiplier);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(b, c), "Operator / (Vector3 v, float scale) test fails for Vector3");
			}
			{
				Vector3 b = new Vector3(7.9f, 5.3f, 109.3f);
				Vector3 sum1 = a + b;

				Vector3 sum2 = new Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(sum1, sum2), "operator +(Vector3 v0, Vector3 v1) test fails for Vector3");
			}
			{
				Vector3 b = new Vector3(7.9f, 5.3f, 10.25f);
				Vector3 difference1 = a - b;

				Vector3 difference2 = new Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(difference1, difference2), "operator -(Vector3 v0, Vector3 v1) fails for Vector3");
			}
			{
				Vector3 b = new Vector3(-a.x, -a.y, -a.z);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(-a, b), "operator -(Vector3 v) fails for Vector3");
			}
			{
				Vector3 b = new Vector3((!a).x, (!a).y, (!a).z);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(!a, b), "operator !(Vector3 v) fails for Vector3");
			}
		}

		[Test]
		public void TestFunctions()
		{
			{
				Vector3 vec1 = new Vector3(2.2f, 6.3f, 12.36f);
				Vector3 vec2 = new Vector3(7.9f, 6.3f, 45.3f);
				float dotPrdt1 = vec1.Dot(vec2);
				float dotPrdt2 = vec2.Dot(vec1);

				Assert.IsTrue(dotPrdt1 == dotPrdt2, "Dot Product test fails for Vector3");

				Vec3 vec1Native = new Vec3(vec1.x, vec1.y, vec1.z);
				Vec3 vec2Native = new Vec3(vec2.x, vec2.y, vec2.z);
				dotPrdt1 = vec1Native.Dot(vec2Native);
				dotPrdt2 = vec2Native.Dot(vec1Native);
				Assert.IsTrue(dotPrdt1 == dotPrdt2, "Dot Product test fails for Vec3");
			}

			{
				Vector3 vec1 = new Vector3(2.2f, 6.3f, 2.36f);
				Vector3 vec2 = new Vector3(7.9f, 6.3f, 5.3f);
				Vector3 crossPrdt1 = vec1.Cross(vec2);
				Vector3 crossPrdt2 = vec2.Cross(vec1);

				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(crossPrdt1, -crossPrdt2), "Cross(Vector3) fails for Vector3 1");
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(-crossPrdt1, crossPrdt2), "Cross(Vector3) fails for Vector3 2");
			}
		}

		[Test]
		public void TestOtherProperties()
		{
			{
				//get length
				Vector3 v1 = new Vector3(1.1f, 2.2f, 3.3f);
				float len = v1.Length;

				float lenManual = (float)System.Math.Sqrt(v1.x * v1.x + v1.y * v1.y + v1.z * v1.z);
				Assert.IsTrue(len == lenManual, "Length get test fails for Vector3");

				Vec3 v2 = new Vec3(v1.x, v1.y, v1.z);
				float v2Len = v2.len();
				Assert.IsTrue(len == lenManual, "Length Comparison test fails for Vector3 and Vec3");

				//get magnitude
				float magnitude = v1.Magnitude;
				Assert.IsTrue(len == magnitude, "Length and magnitude are not equal");
			}

			{
				//LengthSquared
				Vector3 v1 = new Vector3(1.2f, 0.3f, 1.5f);
				float lengthSquared = v1.LengthSquared;
				float lengthSquared2 = (v1.x * v1.x) + (v1.y * v1.y) + (v1.z * v1.z);
				Assert.IsTrue(MathHelpers.Approximately(lengthSquared, lengthSquared2, 0.0000001f), "LengthSquared fails for Vector3");
			}
			{
				//Length2D
				Vector2 v2 = new Vector2(1.2f, 0.3f);
				Vector3 v2_to_v3 = new Vector3(v2);

				float v2Length = v2.Length;
				float v2_to_v3Length = v2_to_v3.Length2D;
				Assert.IsTrue(MathHelpers.Approximately(v2Length, v2_to_v3Length, 0.0000001f), "Length2D fails for Vector3");

				float v2LengthSquared = v2.LengthSquared;
				float v2_to_v3LengthSquared = v2_to_v3.Length2DSquared;
				Assert.IsTrue(MathHelpers.Approximately(v2LengthSquared, v2_to_v3LengthSquared, 0.0000001f), "Length2DSquared fails for Vector3");
			}
			{
				//Normalized
				Vector3 v3 = new Vector3(0, 0, 0);
				Vector3 v3Unit = v3.Normalized;
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(v3, v3Unit), "Normalized fails for Vector3");
			}

			{
				//Volume
				Vector3 v3 = new Vector3(1f, 2f, 10f);
				float volume = v3.Volume;

				float vol = v3.x * v3.y * v3.z;
				Assert.IsTrue(volume == vol, "Volume fails for Vector3");
			}
		}

		[Test]
		public void TestLerp()
		{
			{
				//native
				Vec3 vec3p = new Vec3(1f, 1f, 0f);
				vec3p.normalize();
				Vec3 vec3q = new Vec3(0f, 1f, 0f);
				vec3q.normalize();
				Vec3 vec3Mid = Vec3.CreateLerp(vec3p, vec3q, 0.5f);
				//managed
				Vector3 vector3p = new Vector3(vec3p.x, vec3p.y, vec3p.z);
				Vector3 vector3q = new Vector3(vec3q.x, vec3q.y, vec3q.z);
				Vector3 vector3Mid = Vector3.Lerp(vector3p, vector3q, 0.5f);

				Assert.IsTrue(vec3Mid == vector3Mid, " Lerp tests fails for native and managed vector3 1" + vec3Mid.PrintString() + "," + vector3Mid.ToString());
			}
			{
				//native
				Vec3 vec3p = new Vec3(1f, 0f, 0f);
				Vec3 vec3q = new Vec3(0f, 1f, 0f);
				Vec3 vec3Mid = Vec3.CreateLerp(vec3p, vec3q, 0.5f);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(vec3Mid, new Vector3(0.5f, 0.5f, 0)), "Lerp tests fails for native vec3 2" + vec3Mid.PrintString());

				//managed
				Vector3 vector3p = new Vector3(vec3p.x, vec3p.y, vec3p.z);
				Vector3 vector3q = new Vector3(vec3q.x, vec3q.y, vec3q.z);
				Vector3 vector3Mid = Vector3.Lerp(vector3p, vector3q, 0.5f);
				Assert.IsTrue(vec3Mid == vector3Mid, " Lerp tests fails for native and managed vector3 3" + vec3Mid.PrintString() + "," + vector3Mid.ToString());
			}
			{
				//native
				Vec3 vec3p = new Vec3(0.5f, 0f, 0f);
				Vec3 vec3q = new Vec3(0.5f, 0f, 0f);
				Vec3 lerpVec = Vec3.CreateLerp(vec3p, vec3q, 0.5f);
				Vec3 expectedResult = new Vec3(0.5f, 0f, 0f);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(lerpVec, expectedResult), "Lerp tests fail for native vec3 " + "Expected :" + expectedResult.PrintString() + ", Actual :" + lerpVec.PrintString());

				//managed
				Vector3 vector3p = new Vector3(0.5f, 0f, 0f);
				Vector3 vector3q = new Vector3(0.5f, 0f, 0f);
				Vector3 lerpVector3 = Vector3.Lerp(vector3p, vector3q, 0.5f);
				Vector3 expectedResult2 = new Vector3(0.5f, 0, 0);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(lerpVector3, expectedResult2), "Lerp tests fail for managed vector3 " + "Expected :" + expectedResult2.ToString() + ", Actual :" + lerpVector3.ToString());
			}
			{
				Vector3 p = new Vector3(1.0f, 0f, 0f);
				Vector3 q = new Vector3(-1f, 0f, 0f);

				//middle of interpolation will be (0,0,0)
				Vector3 expectedResult = new Vector3(0, 0, 0);

				Vector3 actualResult = Vector3.Lerp(p, q, 0.5f);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(expectedResult, actualResult), "Lerp fails for managed vector3 " + "Expected :" + expectedResult + ", Actual :" + actualResult);
			}
		}

		[Test]
		public void TestSlerp()
		{
			{ // t =0.5
			  //native
				Vec3 vec3p = new Vec3(1f, 1f, 1f);
				vec3p.normalize();
				Vec3 vec3q = new Vec3(0f, 1f, 1f);
				vec3q.normalize();
				Vec3 vec3Mid = Vec3.CreateSlerp(vec3p, vec3q, 0.5f);
				//managed
				Vector3 vector3p = new Vector3(vec3p.x, vec3p.y, vec3p.z);
				Vector3 vector3q = new Vector3(vec3q.x, vec3q.y, vec3q.z);
				Vector3 vector3Mid = Vector3.Slerp(vector3p, vector3q, 0.5f);

				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(vec3Mid, vector3Mid), " Slerp tests fails for native and managed vector3 1 " + vec3Mid.PrintString() + "," + vector3Mid.ToString());
			}
			{
				//t =0.0
				//native
				Vec3 vec3p = new Vec3(1f, 1f, 1f);
				vec3p.normalize();
				Vec3 vec3q = new Vec3(0f, 1f, 1f);
				vec3q.normalize();
				Vec3 vec3Mid = Vec3.CreateSlerp(vec3p, vec3q, 0.0f);
				//managed
				Vector3 vector3p = new Vector3(vec3p.x, vec3p.y, vec3p.z);
				Vector3 vector3q = new Vector3(vec3q.x, vec3q.y, vec3q.z);
				Vector3 vector3Mid = Vector3.Slerp(vector3p, vector3q, 0.0f);

				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(vec3Mid, vector3Mid), " Slerp tests fails for native and managed vector3 2 " + vec3Mid.PrintString() + "," + vector3Mid.ToString());
			}
			{
				//t =1.0
				//native
				Vec3 vec3p = new Vec3(1f, 1f, 1f);
				vec3p.normalize();
				Vec3 vec3q = new Vec3(0f, 1f, 1f);
				vec3q.normalize();
				Vec3 vec3Mid = Vec3.CreateSlerp(vec3p, vec3q, 1.0f);
				//managed
				Vector3 vector3p = new Vector3(vec3p.x, vec3p.y, vec3p.z);
				Vector3 vector3q = new Vector3(vec3q.x, vec3q.y, vec3q.z);
				Vector3 vector3Mid = Vector3.Slerp(vector3p, vector3q, 1.0f);

				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(vec3Mid, vector3Mid), " Slerp tests fails for native and managed vector3 3 " + vec3Mid.PrintString() + "," + vector3Mid.ToString());
			}

			//same vector but different t
			{
				Vec3 vec3p = new Vec3(1f, 1f, 1f);
				vec3p.normalize();
				Vec3 vec3q = new Vec3(1f, 1f, 1f);
				vec3q.normalize();
				Vec3 vec3Mid = Vec3.CreateSlerp(vec3p, vec3q, 0.5f);
				//managed
				Vector3 vector3p = new Vector3(1f, 1f, 1f);
				vector3p = vector3p.Normalized;
				Vector3 vector3q = new Vector3(1f, 1f, 1f);
				vector3q = vector3q.Normalized;
				Vector3 vector3Mid = Vector3.Slerp(vector3p, vector3q, 0.5f);

				//TODO
				//The following will be tested again as it passes in Launcher but fails in Nunit console runner
				//Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(vec3Mid, vector3Mid), " Slerp tests fails for native and managed vector3 4 " + vec3Mid.PrintString() + "," + vector3Mid.ToString());
				float length = (float)System.Math.Sqrt(3);
				length = 1.0f / length;
				Vector3 expectedResult = new Vector3(length, length, length);
			}
			{
				Vec3 vec3p = new Vec3(1f, 0f, 0f);
				vec3p.normalize();
				Vec3 vec3q = new Vec3(1f, 0f, 0f);
				vec3q.normalize();
				Vec3 vec3Mid = Vec3.CreateSlerp(vec3p, vec3q, 0.5f);
				//managed
				Vector3 vector3p = new Vector3(vec3p.x, vec3p.y, vec3p.z);
				Vector3 vector3q = new Vector3(vec3q.x, vec3q.y, vec3q.z);
				Vector3 vector3Mid = Vector3.Slerp(vector3p, vector3q, 0.5f);

				float differenceX = System.Math.Abs(vec3Mid.x - vector3Mid.x);
				float differenceY = System.Math.Abs(vec3Mid.y - vector3Mid.y);
				float differenceZ = System.Math.Abs(vec3Mid.z - vector3Mid.z);
				bool isSmall = differenceX < 0.0001f && differenceY < 0.0001f && differenceZ < 0.0001f;

				float length = 1f;
				Vector3 expectedResult = new Vector3(length, 0, 0);
			}
			{
				Vector3 p = new Vector3(1.0f, 0f, 0f);
				Vector3 q = new Vector3(-1.0f, 0f, 0f);

				//middle of interpolation will be (0,0)
				Vector3 expectedResult = new Vector3(0, 0, 0);

				Vector3 result = Vector3.SlerpUnclamped(p, q, 0.5f);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(result, expectedResult), "Slerp fails for managed vector3 8, expected result is :" + expectedResult.ToString() + ", actual result:" + result.ToString());
			}
			{
				Vector3 p2 = new Vector3(0.5f, 0f, 0f);
				Vector3 q2 = new Vector3(0.5f, 0f, 0f);

				//middle of interpolation will be (0,0)
				Vector3 expectedResult = new Vector3(1f, 0, 0); //after normalized

				Vector3 actualResult = Vector3.SlerpUnclamped(p2, q2, 0.5f);
			}
		}

	}
}
// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using NUnit.Framework;
using CryEngine;
using CryEngine.Common;

namespace Math.Tests
{

	public static partial class NativeExtensions
	{
		public static string PrintString(this Vec2 vec)
		{
			return "Vec2[" + vec.x + "," + vec.y + "]";
		}

		public static bool TestVectorsAreEqual(Vector2 vec, Vector2 vector2, float precision = 0.00001f)
		{
			return MathHelpers.Approximately(vec.x, vector2.x, precision) && MathHelpers.Approximately(vec.y, vector2.y, precision);
		}


		public static float Dot(this Vec2 vec1, Vec2 vec2)
		{
			return vec1.op_Multiply(vec2);
		}

		public static float GetLength(this Vec2 vec1)
		{
			return (float)System.Math.Sqrt(vec1.GetLength2());
		}

		public static Vec2 Normalize(this Vec2 vec1)
		{
			return vec1.NormalizeSafe();
		}

		public static Vec2 GetNormalized(this Vec2 vec1)
		{
			Vec2 vec = new Vec2(vec1.x, vec1.y);
			return vec.GetNormalizedSafe();
		}
	}
	[TestFixture]
	public class Vector2Test
	{

		[Test]
		public void TestZero()
		{
			Vector2 vec2 = new Vector2(0, 0);
			Assert.IsTrue(vec2 == Vector2.Zero, " Zero equality fails for Vector2.Zero");

			Vec2 vec2native = new Vec2(0, 0);
			Assert.IsTrue(vec2native == vec2, "Zero equality fails for native and managed");

			//this should not throw an error
			Vector2 normalized = vec2.Normalized;
			Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(vec2, normalized), "Zero equality fails for normalized 'zero' point");
		}

		[Test]
		public void TestProperties()
		{
			float x = 11.5f;
			float y = 99.5642316f;
			Vector2 vec2 = new Vector2(x, y);
			Assert.IsTrue(vec2.x == x, "x Property Vector2 fails");
			Assert.IsTrue(vec2.y == y, "y Property Vector2 fails");

			Assert.IsTrue(vec2.X == x, "X Property Vector2 fails");
			Assert.IsTrue(vec2.Y == y, "Y Property Vector2 fails");
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

			Vector2 a = new Vector2(5.2f, 5.3f);
			Vec2 a2 = new Vec2(a.x, a.y);
#pragma warning disable 1718 // disable warning as test for comparison to ownself is intended
			Assert.IsTrue(a == a, "Reflexive test fails for Vector2");
			Assert.IsTrue(a2 == a2, "Reflexive test fails for Vec2");
#pragma warning restore 1718 // enable warning
			Vector2 b = new Vector2(a.x, a.y);
			Vec2 b2 = new Vec2(a.x, a.y);
			Assert.IsTrue((a == b) && (b == a), "Symmetric test fails for Vector2");

			Vector2 c = new Vector2(a.x, a.y);
			Assert.IsTrue((a == b && b == c && a == c), "Transistive test fails for Vector2");
		}

		[Test]
		public void TestGetHashCode()
		{
			Vector2 a = new Vector2(5.2f, 5.3f);
			Vector2 b = new Vector2(a.x, a.y);

			Assert.IsTrue(a.GetHashCode() == b.GetHashCode(), "GetHashcode test fails for Vector2");
		}

		[Test]
		public void TestOperators()
		{
			Vector2 a = new Vector2(4.5f, 65.9f);
			float scale = 15.6f;

			{
				Vector2 b = a * scale;
				Vector2 c = new Vector2(a.x * scale, a.y * scale);
				Assert.IsTrue(b == c, "Operator* (Vector2 v, float scale) test fails for Vector2");
			}

			{
				Vector2 b = a * scale;
				Vector2 d = scale * a;
				Assert.IsTrue(b == d, "Operator * (float scale,Vector2 v) test fails for Vector2");
			}

			{
				Vector2 b = a / scale;
				float multiplier = 1 / scale;
				Vector2 c = new Vector2(a.x * multiplier, a.y * multiplier); // inner division uses multiplication for optimisation
				Assert.IsTrue(b == c, "Operator / (Vector2 v, float scale) test fails for Vector2");
			}
			{
				Vector2 b = new Vector2(7.9f, 5.3f);
				Vector2 sum1 = a + b;

				Vector2 sum2 = new Vector2(a.x + b.x, a.y + b.y);
				Assert.IsTrue(sum1 == sum2, "operator +(Vector2 v0, Vector2 v1) test fails for Vector2");
			}

			{
				Vector2 b = new Vector2(7.9f, 5.3f);
				Vector2 difference1 = a - b;

				Vector2 difference2 = new Vector2(a.x - b.x, a.y - b.y);
				Assert.IsTrue(difference1 == difference2, "operator -(Vector2 v0, Vector2 v1) fails for Vector2");
			}
			{
				Vector2 b = new Vector2(-a.x, -a.y);
				Assert.IsTrue(-a == b, "operator -(Vector2 v) fails for Vector2");
			}
			{
				Vector2 b = new Vector2((!a).x, (!a).y);
				Assert.IsTrue(!a == b, "operator !(Vector2 v) fails for Vector2");
			}

		}

		[Test]
		public void TestFunctions()
		{
			{
				Vector2 vec1 = new Vector2(2.2f, 6.3f);
				Vector2 vec2 = new Vector2(7.9f, 6.3f);
				float dotPrdt1 = vec1.Dot(vec2);
				float dotPrdt2 = vec2.Dot(vec1);

				Assert.IsTrue(dotPrdt1 == dotPrdt2, "Dot Product test fails for Vector2");

				Vec2 vec1Native = new Vec2(vec1.x, vec1.y);
				Vec2 vec2Native = new Vec2(vec2.x, vec2.y);
				dotPrdt1 = vec1Native.Dot(vec2Native);
				dotPrdt2 = vec2Native.Dot(vec1Native);
				Assert.IsTrue(dotPrdt1 == dotPrdt2, "Dot Product test fails for Vec2");
			}
		}

		[Test]
		public void TestOtherProperties()
		{
			{
				//get length
				Vector2 v1 = new Vector2(4.5f, 6.789f);
				float len = v1.Length;

				float lenManual = (float)System.Math.Sqrt(v1.x * v1.x + v1.y * v1.y);
				Assert.IsTrue(len == lenManual, "Length get test fails for Vector2");

				Vec2 v1b = new Vec2(v1.x, v1.y);
				float v1bLen = v1b.GetLength();
				Assert.IsTrue(v1bLen == len, "Length comparison fails for Vector2 and Vec2");

			}
			{
				//set length
				Vector2 v1 = new Vector2(10.0f, 10.0f);
				float len = v1.Length;

				Vector2 v2 = new Vector2(1f, 1f);
				v2.Length = len;
				Assert.IsTrue(v1.x == v2.x, "Length set fails for x for Vector2");
				Assert.IsTrue(v1.y == v2.y, "Length set fails for y for Vector2");
			}
			{
				//Normalized
				Vector2 vec2 = new Vector2(10.0f, 1.0f);
				Vector2 unitVec2 = vec2.Normalized;

				Assert.IsTrue(MathHelpers.Approximately(1.0f, unitVec2.Length), " Normalized Vector2 fails for Length");

				Vec2 vec2a = new Vec2(vec2.x, vec2.y);
				Assert.IsTrue(MathHelpers.Approximately(vec2a.GetNormalized().GetLength(), unitVec2.Length), " Normalized fails for Length comparison");

			}
		}

		[Test]
		public void TestLerp()
		{
			{
				//native
				Vec2 vec2p = new Vec2(1f, 1f);
				Vec2 vec2q = new Vec2(10.0f, 10.0f);
				Vec2 vec2Mid = Vec2.CreateLerp(vec2p, vec2q, 0.5f);
				//managed
				Vector2 vector2p = new Vector2(vec2p.x, vec2p.y);
				Vector2 vector2q = new Vector2(vec2q.x, vec2q.y);
				Vector2 vector2Mid = Vector2.Lerp(vector2p, vector2q, 0.5f);

				Assert.IsTrue(vec2Mid == vector2Mid, " Lerp tests fails for native and managed vector2 1");
			}

			{
				//native
				Vec2 vec2p = new Vec2(1f, 1f);
				Vec2 vec2q = new Vec2(10.0f, 10.0f);
				Vec2 vec2Start = Vec2.CreateLerp(vec2p, vec2q, 0.0f);
				//managed
				Vector2 vector2p = new Vector2(vec2p.x, vec2p.y);
				Vector2 vector2q = new Vector2(vec2q.x, vec2q.y);
				Vector2 vector2Start = Vector2.Lerp(vector2p, vector2q, 0f);

				Assert.IsTrue(vec2Start == vector2Start, " Lerp tests fails 2a");
				Assert.IsTrue(MathHelpers.Approximately(vec2Start.x, vec2p.x) && MathHelpers.Approximately(vec2Start.y, vec2p.y), " Lerp tests fails 2b");
				Assert.IsTrue(vector2Start == vector2p, " Lerp tests fails 2c");
			}
			{
				//native
				Vec2 vec2p = new Vec2(1f, 1f);
				Vec2 vec2q = new Vec2(10.0f, 10.0f);
				Vec2 vec2End = Vec2.CreateLerp(vec2p, vec2q, 1.0f);
				//managed
				Vector2 vector2p = new Vector2(vec2p.x, vec2p.y);
				Vector2 vector2q = new Vector2(vec2q.x, vec2q.y);
				Vector2 vector2End = Vector2.Lerp(vector2p, vector2q, 1.0f);

				Assert.IsTrue(vec2End == vector2End, " Lerp tests fails for native and managed vector2 3a");
				Assert.IsTrue(MathHelpers.Approximately(vec2End.x, vec2q.x) && MathHelpers.Approximately(vec2End.y, vec2q.y), " Lerp tests fails for native and managed vector2 3b");
				Assert.IsTrue(vector2End == vector2q, " Lerp tests fails for native and managed vector2 3c");
			}

			{
				//test clamping
				//managed
				Vector2 vector2p = new Vector2(1f, 1f);
				Vector2 vector2q = new Vector2(10f, 10f);
				Vector2 vector2End = Vector2.Lerp(vector2p, vector2q, 1.1f);

				Assert.IsTrue(vector2End == vector2q, " Lerp tests fails for managed vector2 3d");
			}

			{
				//test clamping
				//managed
				Vector2 vector2p = new Vector2(1f, 1f);
				Vector2 vector2q = new Vector2(10f, 10f);
				Vector2 vector2End = Vector2.Lerp(vector2p, vector2q, -0.1f);

				Assert.IsTrue(vector2End == vector2p, " Lerp tests fails for managed vector2 3e");
			}
		}

		[Test]
		public void TestSlerp()
		{
			{
				//native
				Vec2 vec2p = new Vec2(1f, 1f);
				vec2p.Normalize();
				Vec2 vec2q = new Vec2(10.0f, 10.0f);
				vec2q.Normalize();
				Vec2 vec2Mid = Vec2.CreateSlerp(vec2p, vec2q, 0.5f);
				//managed
				Vector2 vector2p = new Vector2(vec2p.x, vec2p.y);
				vector2p = vector2p.Normalized;
				Vector2 vector2q = new Vector2(vec2q.x, vec2q.y);
				vector2q = vector2q.Normalized;
				Vector2 vector2Mid = Vector2.Slerp(vector2p, vector2q, 0.5f);

				Assert.IsTrue(MathHelpers.Approximately(vec2Mid.x, vector2Mid.x, 0.000001f) && MathHelpers.Approximately(vec2Mid.y, vector2Mid.y, 0.000001f), " Slerp tests fails for native and managed vector2 1");
			}
			{
				Vec2 vec2p = new Vec2(1f, 1f);
				vec2p.Normalize();
				Vec2 vec2q = new Vec2(10.0f, 10.0f);
				vec2q.Normalize();
				Vec2 vec2Start = Vec2.CreateSlerp(vec2p, vec2q, 0.0f);

				//managed
				Vector2 vector2p = new Vector2(vec2p.x, vec2p.y);
				vector2p = vector2p.Normalized;
				Vector2 vector2q = new Vector2(vec2q.x, vec2q.y);
				vector2q = vector2q.Normalized;
				Vector2 vector2Start = Vector2.Slerp(vector2p, vector2q, 0.0f);

				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(vec2Start, vector2Start), "Slerp tests fails for vector2 2a");
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(vec2Start, vec2p), "Slerp tests fails for vector2 2b " + vec2Start.PrintString() + "," + vec2p.PrintString());
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(vector2Start, vector2p), "Slerp tests fails for vector2 2c");
			}
			{
				Vec2 vec2p = new Vec2(1f, 1f);
				vec2p.Normalize();
				Vec2 vec2q = new Vec2(10.0f, 10.0f);
				vec2q.Normalize();
				Vec2 vec2Start = Vec2.CreateSlerp(vec2p, vec2q, 1.0f);

				//managed
				Vector2 vector2p = new Vector2(vec2p.x, vec2p.y);
				vector2p = vector2p.Normalized;
				Vector2 vector2q = new Vector2(vec2q.x, vec2q.y);
				vector2q = vector2q.Normalized;
				Vector2 vector2Start = Vector2.Slerp(vector2p, vector2q, 1.0f);

				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(vec2Start, vector2Start), "Slerp tests fails for vector2 3a");
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(vec2Start, vec2q), "Slerp tests fails for vector2 3b");
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(vector2Start, vector2q), "Slerp tests fails for vector2 3c");
			}
		}

		[Test]
		public void TestLerpCollinearVectors()
		{
			{
				Vector2 p = new Vector2(1.0f, 0f);
				Vector2 q = new Vector2(-1.0f, 0f);

				//middle of interpolation will be (0,0)
				Vector2 yVector = new Vector2(0, 0);

				Vector2 result = Vector2.Lerp(p, q, 0.5f);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(result, yVector), "Lerp Collinear Vector2 fails, result is :" + result.ToString());
			}
			{
				Vec2 p = new Vec2(1.0f, 0f);
				Vec2 q = new Vec2(-1.0f, 0f);

				//middle of interpolation will be (0,0)
				Vec2 yVector = new Vec2(0, 0);

				Vec2 result = Vec2.CreateLerp(p, q, 0.5f);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(result, yVector), "Lerp Collinear Vec2 fails, result is :" + result.ToString());
			}
		}

		[Test]
		public void TestSlerpCollinearVectors()
		{
			{
				Vector2 p = new Vector2(1.0f, 0f);
				Vector2 q = new Vector2(-1.0f, 0f);

				//middle of interpolation will be (0,0)
				Vector2 yVector = new Vector2(0, 0);

				Vector2 result = Vector2.SlerpUnclamped(p, q, 0.5f);
				Assert.IsTrue(NativeExtensions.TestVectorsAreEqual(result, yVector), "Slerp Collinear Vector2 fails, result is :" + result.ToString());
			}
		}

	}
}
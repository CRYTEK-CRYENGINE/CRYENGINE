// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using NUnit.Framework;
using CryEngine;
using CryEngine.Common;
using System;

namespace Math.Tests
{
	public static partial class NativeExtensions
	{
		public static Quat Normalize(this Quat quat)
		{
			return quat.Normalize();
		}

		public static Quat GetNormalized(this Quat quat)
		{
			Quat quat2 = new Quat(quat.w, quat.v);
			return quat2.Normalize();
		}

		public static float GetLength(this Quat quat)
		{
			return quat.GetLength();
		}
	}

	//[TestFixture]
	public class QuaternionTest
	{
		[Test]
		public void TestIdentity()
		{
			//managed
			Quaternion managedQuat = Quaternion.Identity;

			//native
			Quat nativeQuat = Quat.CreateIdentity();

			Assert.IsTrue(managedQuat == nativeQuat);
		}

		[Test]
		public void TestNativeManagedEqualityMatrix3by4()
		{
			Matrix3x4 managedmatrix = new Matrix3x4(0f, 3f, 6f, 9f, 1f, 4f, 7f, 10f, 2f, 5f, 8f, 11f);
			Matrix34 nativematrix = new Matrix34(0f, 3f, 6f, 9f, 1f, 4f, 7f, 10f, 2f, 5f, 8f, 11f);

			//managed quaternion
			Quaternion managedQuat = new Quaternion(managedmatrix);
			//native quaternion
			Quat nativeQuat = Quat.CreateQuatFromMatrix(nativematrix);
			Assert.IsTrue(managedQuat == nativeQuat);
		}

		[Test]
		public void TestNativeManagedEqualityVector3()
		{
			Vector3 managedVector = new Vector3(0.6f, 0.7f, 0.8f);
			managedVector = managedVector.Normalized;

			Vec3 nativeVector = new Vec3(managedVector.x, managedVector.y, managedVector.z);

			//managed Quaternion
			Quaternion managedQuaternion = new Quaternion(managedVector);

			//native quaternion
			Quat nativeQuaternion = Quat.CreateIdentity();
			nativeQuaternion.SetRotationVDir(nativeVector);

			Assert.IsTrue(managedQuaternion == nativeQuaternion);
		}

		[Test]
		public void TestNativeManagedEqualityAngles3()
		{
			//managed 
			Angles3 angles = new Angles3(1f, 2f, 3f);
			Quaternion managedQuaternion = new Quaternion(angles);

			//native
			Ang3 angles2 = angles;
			Quat nativeQuaternion = Quat.CreateIdentity();
			nativeQuaternion.SetRotationXYZ(angles2);

			Assert.IsTrue(managedQuaternion == nativeQuaternion);
		}

		[Test]
		public void TestNativeManagedEqualityVec3by3()
		{
			//managed
			Vec3 right = new Vec3(1f, 0f, 0f);
			Vec3 forward = new Vec3(0f, 1f, 0f);
			Vec3 up = new Vec3(0f, 0f, 1f);

#pragma warning disable 618 //disable warning as usage is intended to test obsolete constructor
			Quaternion quat = new Quaternion(right, forward, up);
#pragma warning restore 618 //enable warning
			//native
			Matrix33 matrix33 = Matrix33.CreateFromVectors(right, forward, up);
			Quat quat2 = Quat.CreateQuatFromMatrix(matrix33);

			Assert.IsTrue(quat == quat2);
		}

		[Test]
		public void TestEquality()
		{
			Vector3 complexNo = new Vector3(1, 1, 3);
			float scalar = 2;

			//test equality between managed
			Quaternion quat1 = new Quaternion(scalar, complexNo);
			Quaternion quat2 = new Quaternion(complexNo, scalar);
			Quaternion quat3 = new Quaternion(complexNo.x, complexNo.y, complexNo.z, scalar);
			Assert.IsTrue(quat1 == quat2);

			//test equality between managed and native
			Vec3 complexNoNative = complexNo;
			Quat native1 = new Quat(scalar, complexNoNative);
			Assert.IsTrue(native1 == quat1);
			Assert.IsTrue(native1 == quat2);
			Assert.IsTrue(native1 == quat3);
		}

		[Test]
		public void TestProperties()
		{
			Vector3 complexNo = new Vector3(1, 1, 3);
			float scalar = 2;
			Quaternion quat1 = new Quaternion(scalar, complexNo);

			//test getters
			Assert.IsTrue(quat1.x == quat1.X);
			Assert.IsTrue(quat1.y == quat1.Y);
			Assert.IsTrue(quat1.z == quat1.Z);
			Assert.IsTrue(quat1.w == quat1.W);

			Assert.IsTrue(quat1.v == quat1.V);
		}

		[Test]
		public void TestDeprecation()
		{
			// test deprecated version against new version to ensure both are compatible
			Vec3 right = new Vec3(1f, 0f, 0f);
			Vec3 forward = new Vec3(0f, 1f, 0f);
			Vec3 up = new Vec3(0f, 0f, 1f);


#pragma warning disable 618  //disable warning as usage is intended to test obsolete constructor
			Quaternion quatOld = new Quaternion(right, forward, up);
#pragma warning restore 618  //enable warning

			Vector3 rightManaged = new Vector3(1f, 0f, 0f);
			Vector3 forwardManaged = new Vector3(0f, 1f, 0f);
			Vector3 upManaged = new Vector3(0f, 0f, 1f);
			Quaternion quatNew = new Quaternion(rightManaged, forwardManaged, upManaged);

			Assert.IsTrue(quatOld == quatNew);
		}

		[Test]
		public void TestMethods()
		{
			//we need 3 orthonormal vectors for equivalence test to pass
			Vector3 right = new Vec3(1f, 0f, -1f);
			right = right.Normalized;

			Vector3 forward = new Vec3(1f, 1.4142136f, 1f);
			forward = forward.Normalized;

			Vector3 up = new Vec3(1f, -1.4142136f, 1f);
			up = up.Normalized;

			//native
			Matrix34 nativematrix = new Matrix34(0f, 3f, 6f, 9f, 1f, 4f, 7f, 10f, 2f, 5f, 8f, 11f);
			Quat nativeQuat = Quat.CreateQuatFromMatrix(nativematrix);

			//managed
			Matrix3x4 managedmatrix = new Matrix3x4(0f, 3f, 6f, 9f, 1f, 4f, 7f, 10f, 2f, 5f, 8f, 11f);
			Quaternion managedQuat = Quat.CreateQuatFromMatrix(managedmatrix);

			float radians = 1.57f;

			//1 normalize
			{
				Quaternion test1a = managedQuat;
				test1a.Normalize();

				Quat test1b = nativeQuat;
				test1b.Normalize();

				Assert.IsTrue(Quat.IsEquivalent(test1a, test1b), "Normalize Equivalence test failed");
			}
			//2 Dot

			//3 Difference

			//4 CreateFromVectors
			{
				Matrix3x3 matrix33 = Matrix33.CreateFromVectors(right, forward, up);
				Quat quat2a = Quat.CreateQuatFromMatrix(matrix33);

				Vector3 right2 = new Vector3(right.x, right.y, right.z);
				Vector3 forward2 = new Vector3(forward.x, forward.y, forward.z);
				Vector3 up2 = new Vector3(up.x, up.y, up.z);
				Quaternion quat2b = Quaternion.CreateFromVectors(right2, forward2, up2);
				Assert.IsTrue(quat2a == quat2b, "CreateFromVectors equality failed");
				Assert.IsTrue(Quat.IsEquivalent(quat2a, quat2b), "CreateFromVectors equivalence failed");
			}

			//5 SetLookOrientation
			//5a
			{
				//compare to SetRotationVDir, up vector = (0,0,1)

				//get 2 orthogonal vectors
				Vector3 up5 = new Vector3(0f, 0f, 1f);
				Vector3 forward5 = new Vector3(1f, 1.4142136f, 0f);
				forward5 = forward5.Normalized;
				Vector3 right5 = forward5.Cross(up5);

				Quat nativeQuaternion2 = Quat.CreateIdentity();
				nativeQuaternion2.SetRotationVDir(forward5);

				Matrix33 nativeMatrix = Matrix33.CreateMatrix33(right5, forward5, up5);
				Quat nativeQuaternion3 = Quat.CreateQuatFromMatrix(nativeMatrix);

				Assert.IsTrue(Quat.IsEquivalent(nativeQuaternion2, nativeQuaternion3), "Native Quaternion constructor failed comparison with SetRotationVDir. Expected :" + ((Quaternion)nativeQuaternion2).ToString() + ", Actual :" + ((Quaternion)nativeQuaternion3).ToString());
			}

			//5b
			{
				// test new C# SetLookOrientation, DEV-3691
				// rotate forward around x-y plane, z=0
				// i) 20 values for x-y plane
				// ii) 20 values for y-z plane

				Vec3[] testVectors = new Vec3[40];
				for (uint i = 0; i < 20; ++i)
				{
					radians = ((float)System.Math.PI) / 20.0f * i;
					testVectors[i] = new Vec3((float)System.Math.Cos(radians), (float)System.Math.Sin(radians), 0f);
				}
				for (uint i = 20; i < 40; ++i)
				{
					radians = ((float)System.Math.PI) / 20.0f * i;
					testVectors[i] = new Vec3(0f, (float)System.Math.Cos(radians), (float)System.Math.Sin(radians));
				}

				const float test_margin_error = 0.05f;
				for (uint i = 0; i < testVectors.Length; ++i)
				{
					Vec3 forward5c = testVectors[i];
					forward5c = forward5c.normalized();
					Quat nativeQuat5c = Quat.CreateIdentity();
					nativeQuat5c.SetRotationVDir(forward5c);
					float dtPrdt1 = MathHelpers.Clamp(forward5c.Dot(nativeQuat5c.GetColumn1()), -1f, 1f);

					Quaternion managedQuat5c = Quaternion.Identity;
					Vector3 upManaged = Vector3.Up;
					managedQuat5c.SetLookOrientation(forward5c, upManaged);
					float dtPrdt2 = MathHelpers.Clamp(forward5c.Dot(managedQuat5c.Forward), -1f, 1f);

					float diffFromNative = (float)(System.Math.Acos(dtPrdt1) * (180f / System.Math.PI));
					float diffFromManaged = (float)(System.Math.Acos(dtPrdt2) * (180f / System.Math.PI));
					float absoluteDiff = System.Math.Abs(diffFromManaged - diffFromNative);
					Assert.IsTrue(absoluteDiff <= test_margin_error, "SetLookOrientation failed at loop index " + i + ".Absolute Difference:" + absoluteDiff + " Expected (Native) :" + diffFromNative + ", Actual (Managed) :" + diffFromManaged + ", Forward : " + NativeExtensions.PrintString(forward5c) + ", Up :" + upManaged.ToString());
				}
				{
					//boundary case where axis are flipped when comparing native to managed
					Quaternion quatManaged = Quaternion.Identity;
					Vector3 upManaged = Vector3.Up;
					Vector3 forwardManaged = new Vector3(-8.126793f, 3.401123f, -1.644333f);
					forwardManaged = forwardManaged.Normalized;
					quatManaged.SetLookOrientation(forwardManaged, upManaged);

					Quat quatNative = Quat.CreateIdentity();
					Vec3 forwardNative = new Vec3(-8.126793f, 3.401123f, -1.644333f);
					forwardNative = forwardNative.normalized();
					quatNative.SetRotationVDir(forwardNative);
					bool isEqui1 = Quat.IsEquivalent(quatManaged, quatNative, 0.00999999776f);
					Assert.IsTrue(isEqui1, String.Format("Native Quaternion {0} and Managed Quaternion {1} are not equivalent", ((Quaternion)quatNative).ToString(), quatManaged));
				}
			}

			//6 SetFromTORotation(Vector3 , Vector3)
			{
				Vec3 fromVec = new Vec3(0.5f, 0.5f, 0.5f);
				Vec3 toVec = new Vec3(0.5f, -0.5f, -0.5f);
				Quat quat6a = Quat.CreateIdentity();
				quat6a.SetRotationV0V1(fromVec, toVec);

				Vector3 fromVec2 = new Vector3(fromVec.x, fromVec.y, fromVec.z);
				Vector3 toVec2 = new Vector3(toVec.x, toVec.y, toVec.z);
				Quaternion quat6b = Quaternion.Identity;
				quat6b.SetFromToRotation(fromVec2, toVec2);
				Assert.IsTrue(Quat.IsEquivalent(quat6a, quat6b), "SetFromToRotation failed");
			}
			//7 CreateRotationX(float)
			{
				Quat quat7a = Quat.CreateRotationX(radians);
				Quaternion quat7b = Quaternion.CreateRotationX(radians);
				Assert.IsTrue(Quat.IsEquivalent(quat7a, quat7b), "CreateRotationX failed");
			}


			//8 CreateRotationY(float)
			{
				Quat quat8a = Quat.CreateRotationY(radians + 0.1f);
				Quaternion quat8b = Quaternion.CreateRotationY(radians + 0.1f);
				Assert.IsTrue(Quat.IsEquivalent(quat8a, quat8b), "CreateRotationY failed");
			}


			//9 CreateRotationZ(float)
			{
				Quat quat9a = Quat.CreateRotationZ(radians + 0.1f);
				Quaternion quat9b = Quaternion.CreateRotationZ(radians + 0.1f);
				Assert.IsTrue(Quat.IsEquivalent(quat9a, quat9b), "CreateRotationZ failed");
			}


			//10 CreateRotationXYZ(Angles3)
			{
				Angles3 angles = new Vec3(radians, radians, radians);
				Quat quat10a = Quat.CreateRotationXYZ(angles);

				Quaternion quat10b = Quaternion.CreateRotationXYZ(angles);
				Assert.IsTrue(Quat.IsEquivalent(quat10a, quat10b), "CreateRotationXYZ equivalence failed");
			}


			//11 Slerp(Quaternion, Quaternion, float)
			{
				float timeRatio = 0.5f;
				Quat quat11a = Quat.CreateRotationX(radians);
				Quat quat11b = Quat.CreateRotationY(radians);
				Quat quat11c = Quat.CreateIdentity();
				quat11c.SetSlerp(quat11a, quat11b, timeRatio);

				Quaternion quat11d = Quaternion.CreateRotationX(radians);
				Quaternion quat11e = Quaternion.CreateRotationY(radians);
				Quaternion quat11f = Quaternion.Slerp(quat11d, quat11e, timeRatio);
				Assert.IsTrue(Quat.IsEquivalent(quat11c, quat11f), "Slerp equivalence failed");

			}


			//12 Lerp(Quaternion, Quaternion, float)
			{
				float timeRatio = 0.5f;
				Quat quat12a = Quat.CreateRotationX(radians);
				Quat quat12b = Quat.CreateRotationY(radians);
				Quat quat12c = Quat.CreateIdentity();
				quat12c.SetNlerp(quat12a, quat12b, timeRatio);

				Quaternion quat12d = Quaternion.CreateRotationX(radians);
				Quaternion quat12e = Quaternion.CreateRotationY(radians);
				Quaternion quat12f = Quaternion.Lerp(quat12d, quat12e, timeRatio);
				Assert.IsTrue(Quat.IsEquivalent(quat12c, quat12f), "Lerp equivalence failed");
			}

			//13 Properties
			{
				Matrix3x3 matrix33 = Matrix33.CreateFromVectors(right, forward, up);
				Quat quat13a = Quat.CreateQuatFromMatrix(matrix33);

				Vector3 right2 = new Vector3(right.x, right.y, right.z);
				Vector3 forward2 = new Vector3(forward.x, forward.y, forward.z);
				Vector3 up2 = new Vector3(up.x, up.y, up.z);
				Quaternion quat13b = Quaternion.CreateFromVectors(right2, forward2, up2);
				Assert.IsTrue(quat13a == quat13b, "Quaternions equality test failed");
				Assert.IsTrue(Quat.IsEquivalent(quat13a, quat13b), "Quaternions equivalence test failed");

				Assert.IsTrue(Vec3.IsEquivalent(right2, quat13b.Right), "Right equivalence test failed");
				Assert.IsTrue(Vec3.IsEquivalent(forward2, quat13b.Forward), "Forward equivalence test failed");
				Assert.IsTrue(Vec3.IsEquivalent(up2, quat13b.Up), "Up equivalence test failed");

				//inversion
				Quat invertNative = quat13a.GetInverted();
				Quaternion invertManaged = quat13b.Inverted;
				Assert.IsTrue(Quat.IsEquivalent(invertNative, invertManaged), "Inversion equivalence test failed");

				//normalization
				Quat normalNative = quat13a.GetNormalized();
				Quaternion normalManaged = quat13b.Normalized;
				Assert.IsTrue(Quat.IsEquivalent(normalNative, normalManaged), "Normalization test failed");

				//length
				float lengthNative = quat13a.GetLength();
				float lengthManaged = quat13b.Length;
				Assert.IsTrue(System.Math.Abs(lengthNative - lengthManaged) <= Single.Epsilon, "Length test failed");

				//IsIdentity
				Quaternion managedIdentity = Quaternion.Identity;
				Assert.IsTrue(managedIdentity.IsIdentity, "Managed Identity test failed");

				Quat nativeIdentity = new Quat();
				nativeIdentity.SetIdentity();
				Assert.IsTrue(nativeIdentity.IsIdentity(), "Native Identity test failed");
				Assert.IsTrue(managedIdentity == nativeIdentity, "Identity comparison failed");

				// yaw pitch roll

			}
		}
	}
}
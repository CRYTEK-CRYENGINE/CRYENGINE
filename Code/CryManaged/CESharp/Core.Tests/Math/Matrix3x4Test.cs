// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using NUnit.Framework;
using CryEngine;
using CryEngine.Common;

namespace Math.Tests
{
	[TestFixture]
	public class Matrix3x4Test
	{
		[Test, Description("Convert from native to managed should retain same matrix data")]
		public void ConversionFromNativeToManaged()
		{
			Matrix34 nativeMatrix = new Matrix34(0,1,2,3,10,11,12,13,20,21,22,23);
			Matrix3x4 managedMatrix = nativeMatrix;

			Assert.That(nativeMatrix.m00, Is.EqualTo(managedMatrix[0, 0]));
			Assert.That(nativeMatrix.m01, Is.EqualTo(managedMatrix[0, 1]));
			Assert.That(nativeMatrix.m02, Is.EqualTo(managedMatrix[0, 2]));
			Assert.That(nativeMatrix.m03, Is.EqualTo(managedMatrix[0, 3]));
			Assert.That(nativeMatrix.m10, Is.EqualTo(managedMatrix[1, 0]));
			Assert.That(nativeMatrix.m11, Is.EqualTo(managedMatrix[1, 1]));
			Assert.That(nativeMatrix.m12, Is.EqualTo(managedMatrix[1, 2]));
			Assert.That(nativeMatrix.m13, Is.EqualTo(managedMatrix[1, 3]));
			Assert.That(nativeMatrix.m20, Is.EqualTo(managedMatrix[2, 0]));
			Assert.That(nativeMatrix.m21, Is.EqualTo(managedMatrix[2, 1]));
			Assert.That(nativeMatrix.m22, Is.EqualTo(managedMatrix[2, 2]));
			Assert.That(nativeMatrix.m23, Is.EqualTo(managedMatrix[2, 3]));
		}

		[Test, Description("Convert from managed to native")]
		public void ConversionFromManagedToNative()
		{
			Matrix3x4 managedMatrix = new Matrix3x4(0, 1, 2, 3, 10, 11, 12, 13, 20, 21, 22, 23);
			Matrix34 nativeMatrix = managedMatrix;

			Assert.That(nativeMatrix.m00, Is.EqualTo(managedMatrix[0, 0]));
			Assert.That(nativeMatrix.m01, Is.EqualTo(managedMatrix[0, 1]));
			Assert.That(nativeMatrix.m02, Is.EqualTo(managedMatrix[0, 2]));
			Assert.That(nativeMatrix.m03, Is.EqualTo(managedMatrix[0, 3]));
			Assert.That(nativeMatrix.m10, Is.EqualTo(managedMatrix[1, 0]));
			Assert.That(nativeMatrix.m11, Is.EqualTo(managedMatrix[1, 1]));
			Assert.That(nativeMatrix.m12, Is.EqualTo(managedMatrix[1, 2]));
			Assert.That(nativeMatrix.m13, Is.EqualTo(managedMatrix[1, 3]));
			Assert.That(nativeMatrix.m20, Is.EqualTo(managedMatrix[2, 0]));
			Assert.That(nativeMatrix.m21, Is.EqualTo(managedMatrix[2, 1]));
			Assert.That(nativeMatrix.m22, Is.EqualTo(managedMatrix[2, 2]));
			Assert.That(nativeMatrix.m23, Is.EqualTo(managedMatrix[2, 3]));

		}

		[Test]
		public void TestIdentity()
		{
			Matrix34 nativeMatrix = Matrix34.CreateIdentity();
			Matrix3x4 managedMatrix = Matrix3x4.Identity;

			// Compare native to managed identity
			Assert.That(nativeMatrix.m00, Is.EqualTo(managedMatrix[0, 0]));
			Assert.That(nativeMatrix.m01, Is.EqualTo(managedMatrix[0, 1]));
			Assert.That(nativeMatrix.m02, Is.EqualTo(managedMatrix[0, 2]));
			Assert.That(nativeMatrix.m03, Is.EqualTo(managedMatrix[0, 3]));
			Assert.That(nativeMatrix.m10, Is.EqualTo(managedMatrix[1, 0]));
			Assert.That(nativeMatrix.m11, Is.EqualTo(managedMatrix[1, 1]));
			Assert.That(nativeMatrix.m12, Is.EqualTo(managedMatrix[1, 2]));
			Assert.That(nativeMatrix.m13, Is.EqualTo(managedMatrix[1, 3]));
			Assert.That(nativeMatrix.m20, Is.EqualTo(managedMatrix[2, 0]));
			Assert.That(nativeMatrix.m21, Is.EqualTo(managedMatrix[2, 1]));
			Assert.That(nativeMatrix.m22, Is.EqualTo(managedMatrix[2, 2]));
			Assert.That(nativeMatrix.m23, Is.EqualTo(managedMatrix[2, 3]));

			// Compare managed matrix to real values.
			Assert.That(managedMatrix[0, 0], Is.EqualTo(1));
			Assert.That(managedMatrix[0, 1], Is.EqualTo(0));
			Assert.That(managedMatrix[0, 2], Is.EqualTo(0));
			Assert.That(managedMatrix[0, 3], Is.EqualTo(0));
			Assert.That(managedMatrix[1, 0], Is.EqualTo(0));
			Assert.That(managedMatrix[1, 1], Is.EqualTo(1));
			Assert.That(managedMatrix[1, 2], Is.EqualTo(0));
			Assert.That(managedMatrix[1, 3], Is.EqualTo(0));
			Assert.That(managedMatrix[2, 0], Is.EqualTo(0));
			Assert.That(managedMatrix[2, 1], Is.EqualTo(0));
			Assert.That(managedMatrix[2, 2], Is.EqualTo(1));
			Assert.That(managedMatrix[2, 3], Is.EqualTo(0));
		}

		[Test] 
		public void TestMatrix3x4Construction()
		{
			//native parts
			Vec3 scale = new Vec3(0.45f, 0.55f, 0.65f);
			Quat rotation = new Quat(0.2f, 0.1f, 0.1f, 0.1f);
			Vec3 position = new Vec3(0.1f, 0.2f, 0.3f);
			Matrix34 nativeM34 = new Matrix34(scale, rotation, position);

			//managed parts
			Vector3 scale2 = new Vector3(0.45f, 0.55f, 0.65f);
			Quaternion rotation2 = new Quaternion(0.2f, 0.1f, 0.1f, 0.1f);
			Vector3 position2 = new Vector3(0.1f, 0.2f, 0.3f);
			Matrix3x4 managedMatrix = new Matrix3x4(scale2, rotation2, position2);

			Assert.IsTrue(scale == scale2 ,"Scale are not equal");
			//TODO test fails
			//Assert.IsTrue(rotation == rotation2, "Rotation are not equal");
			Assert.IsTrue(position == position2, "Position are not equal");

			//TODO test fails
			//Assert.IsTrue(nativeM34 == (Matrix34)managedMatrix, "Native and managed matrix34 are not equal");
		}

		[Test]
		public void TestTransformDirection()
		{
			//managed
			Vector3 vector3 = new Vector3(0.2f, 0.2f, 0.2f);
			Matrix3x4 matrix3x4 = Matrix3x4.Identity;
			Vector3 transVector3 = matrix3x4.TransformDirection(vector3);

			//native
			Vec3 vec3 = new Vec3(vector3.x, vector3.y, vector3.z);
			Matrix34 matrix34 = Matrix34.CreateIdentity();
			Vec3 transVec3 = matrix34.TransformVector(vec3);

			Assert.IsTrue(transVector3.x == transVec3.x, "x is different");
			Assert.IsTrue(transVector3.y == transVec3.y ,"y is different");
			Assert.IsTrue(transVector3.z == transVec3.z, "z is different");
			Assert.IsTrue(transVector3 == transVec3, "vector is different");
		}

		[Test]
		public void TestTransformPoint()
		{
			//managed
			Vector3 point3 = new Vector3(0.2f, 0.2f, 0.2f);
			Matrix3x4 matrix3x4 = Matrix3x4.Identity;
			Vector3 transPoint3 = matrix3x4.TransformPoint(point3);

			//native
			Vec3 pt3 = new Vec3(point3.x, point3.y, point3.z);
			Matrix34 matrix34 = Matrix34.CreateIdentity();
			Vec3 transPt3 = matrix34.TransformPoint(pt3);

			Assert.IsTrue(point3.x == pt3.x, "x is different");
			Assert.IsTrue(point3.y == pt3.y, "y is different");
			Assert.IsTrue(point3.z == pt3.z, "z is different");
			Assert.IsTrue(point3 == pt3, "point is different");

			Assert.IsTrue(transPoint3.x == transPt3.x, "x is different");
			Assert.IsTrue(transPoint3.y == transPt3.y, "y is different");
			Assert.IsTrue(transPoint3.z == transPt3.z, "z is different");
			Assert.IsTrue(transPoint3 == transPt3, "point is different");
		}
		
		[Test]
		public void TestSetGetTranslation()
		{
			Vector3 point3 = new Vector3(1f, 2f, 3f);
			Matrix3x4 matrix3x4 = Matrix3x4.Identity;
			matrix3x4.SetTranslation(point3);
			Vector3 newPoint3 = matrix3x4.GetTranslation();
			Assert.IsTrue(point3.x == newPoint3.x, "x is different");
			Assert.IsTrue(point3.y == newPoint3.y , "y is different");
			Assert.IsTrue(point3.z == newPoint3.z, "z is different");
			Assert.IsTrue(point3 == newPoint3);
		}

		[Test]
		public void TestAddTranslation()
		{
			//managed operation
			Vector3 point = new Vector3(1f, -1f, 0.5f);
			Matrix3x4 matrix34 = new Matrix3x4(0f, 1f, 2f, 3f, 10f, 11f, 12f, 13f, 20f, 21f, 22f, 23f);
			Matrix3x4 managedMatrix = matrix34;
			Assert.IsTrue(!ReferenceEquals(matrix34, managedMatrix));
			managedMatrix.AddTranslation(point);

			//native operation
			Vec3 point2 = new Vec3(point.x, point.y, point.z);
			Matrix34 nativeMatrix = new Matrix34(0f, 1f, 2f, 3f, 10f, 11f, 12f, 13f, 20f, 21f, 22f, 23f);
			nativeMatrix.AddTranslation(point2);

			Assert.That(nativeMatrix.m00, Is.EqualTo(managedMatrix[0, 0]));
			Assert.That(nativeMatrix.m01, Is.EqualTo(managedMatrix[0, 1]));
			Assert.That(nativeMatrix.m02, Is.EqualTo(managedMatrix[0, 2]));
			Assert.That(nativeMatrix.m03, Is.EqualTo(managedMatrix[0, 3]));
			Assert.That(nativeMatrix.m10, Is.EqualTo(managedMatrix[1, 0]));
			Assert.That(nativeMatrix.m11, Is.EqualTo(managedMatrix[1, 1]));
			Assert.That(nativeMatrix.m12, Is.EqualTo(managedMatrix[1, 2]));
			Assert.That(nativeMatrix.m13, Is.EqualTo(managedMatrix[1, 3]));
			Assert.That(nativeMatrix.m20, Is.EqualTo(managedMatrix[2, 0]));
			Assert.That(nativeMatrix.m21, Is.EqualTo(managedMatrix[2, 1]));
			Assert.That(nativeMatrix.m22, Is.EqualTo(managedMatrix[2, 2]));
			Assert.That(nativeMatrix.m23, Is.EqualTo(managedMatrix[2, 3]));
		}

		[Test]
		public void TestIndexers()
		{
			Vector4 row1 = new Vector4(0.11f, 0.21f, 0.31f, 0.41f);
			Vector4 row2 = new Vector4(0.12f, 0.22f, 0.32f, 0.42f);
			Vector4 row3 = new Vector4(0.13f, 0.23f, 0.33f, 0.43f);
			Matrix3x4 matrix3x4 = 
				new Matrix3x4(row1.x, row1.y, row1.z, row1.w,
											row2.x, row2.y, row2.z, row2.w,
											row3.x, row3.y, row3.z, row3.w);
			//Get operations
			// by row
			Assert.IsTrue(matrix3x4[0] == row1);
			Assert.IsTrue(matrix3x4[1] == row2);
			Assert.IsTrue(matrix3x4[2] == row3);

			//by row and column
			Assert.IsTrue(matrix3x4[0][0] == row1.x);
			Assert.IsTrue(matrix3x4[0][1] == row1.y);
			Assert.IsTrue(matrix3x4[0][2] == row1.z);
			Assert.IsTrue(matrix3x4[0][3] == row1.w);

			Assert.IsTrue(matrix3x4[1][0] == row2.x);
			Assert.IsTrue(matrix3x4[1][1] == row2.y);
			Assert.IsTrue(matrix3x4[1][2] == row2.z);
			Assert.IsTrue(matrix3x4[1][3] == row2.w);

			Assert.IsTrue(matrix3x4[2][0] == row3.x);
			Assert.IsTrue(matrix3x4[2][1] == row3.y);
			Assert.IsTrue(matrix3x4[2][2] == row3.z);
			Assert.IsTrue(matrix3x4[2][3] == row3.w);

			//Set operations
			Vector4 newRow1 = new Vector4(0.51f, 0.61f, 0.71f, 0.81f);
			Vector4 newRow2 = new Vector4(0.52f, 0.62f, 0.72f, 0.82f);
			Vector4 newRow3 = new Vector4(0.53f, 0.63f, 0.73f, 0.83f);

			matrix3x4[0] = newRow1;
			Assert.IsTrue(matrix3x4[0].x == newRow1.x);
			Assert.IsTrue(matrix3x4[0].y == newRow1.y);
			Assert.IsTrue(matrix3x4[0].z == newRow1.z);
			Assert.IsTrue(matrix3x4[0].w == newRow1.w);
			Assert.IsTrue(matrix3x4[0] == newRow1, "Row 1 is equal");

			matrix3x4[1] = newRow2;
			Assert.IsTrue(matrix3x4[1].x == newRow2.x);
			Assert.IsTrue(matrix3x4[1].y == newRow2.y);
			Assert.IsTrue(matrix3x4[1].z == newRow2.z);
			Assert.IsTrue(matrix3x4[1].w == newRow2.w);
			Assert.IsTrue(matrix3x4[1] == newRow2, "Row 2 is equal");

			matrix3x4[2] = newRow3;
			Assert.IsTrue(matrix3x4[2].x == newRow3.x);
			Assert.IsTrue(matrix3x4[2].y == newRow3.y);
			Assert.IsTrue(matrix3x4[2].z == newRow3.z);
			Assert.IsTrue(matrix3x4[2].w == newRow3.w);
			Assert.IsTrue(matrix3x4[2] == newRow3, "Row 3 is equal");
		}
	}
}
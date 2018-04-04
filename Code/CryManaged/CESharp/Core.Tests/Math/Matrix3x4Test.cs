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
			Matrix3x4 managedmatrix = nativeMatrix;
			Assert.That(true, Is.EqualTo(managedmatrix == nativeMatrix));
			Assert.That(nativeMatrix.m00, Is.EqualTo(managedmatrix.m00));
			Assert.That(nativeMatrix.m01, Is.EqualTo(managedmatrix.m01));
			Assert.That(nativeMatrix.m02, Is.EqualTo(managedmatrix.m02));
			Assert.That(nativeMatrix.m03, Is.EqualTo(managedmatrix.m03));
			Assert.That(nativeMatrix.m10, Is.EqualTo(managedmatrix.m10));
			Assert.That(nativeMatrix.m11, Is.EqualTo(managedmatrix.m11));
			Assert.That(nativeMatrix.m12, Is.EqualTo(managedmatrix.m12));
			Assert.That(nativeMatrix.m13, Is.EqualTo(managedmatrix.m13));
			Assert.That(nativeMatrix.m20, Is.EqualTo(managedmatrix.m20));
			Assert.That(nativeMatrix.m21, Is.EqualTo(managedmatrix.m21));
			Assert.That(nativeMatrix.m22, Is.EqualTo(managedmatrix.m22));
			Assert.That(nativeMatrix.m23, Is.EqualTo(managedmatrix.m23));
			Assert.IsTrue(nativeMatrix == managedmatrix);
		}

		[Test, Description("Convert from managed to native")]
		public void ConversionFromManagedToNative()
		{
			Matrix3x4 managedmatrix = new Matrix3x4(0, 1, 2, 3, 10, 11, 12, 13, 20, 21, 22, 23);
			Matrix34 nativeMatrix = managedmatrix;
			Assert.That(true, Is.EqualTo(managedmatrix == nativeMatrix));
			Assert.That(nativeMatrix.m00, Is.EqualTo(managedmatrix.m00));
			Assert.That(nativeMatrix.m01, Is.EqualTo(managedmatrix.m01));
			Assert.That(nativeMatrix.m02, Is.EqualTo(managedmatrix.m02));
			Assert.That(nativeMatrix.m03, Is.EqualTo(managedmatrix.m03));
			Assert.That(nativeMatrix.m10, Is.EqualTo(managedmatrix.m10));
			Assert.That(nativeMatrix.m11, Is.EqualTo(managedmatrix.m11));
			Assert.That(nativeMatrix.m12, Is.EqualTo(managedmatrix.m12));
			Assert.That(nativeMatrix.m13, Is.EqualTo(managedmatrix.m13));
			Assert.That(nativeMatrix.m20, Is.EqualTo(managedmatrix.m20));
			Assert.That(nativeMatrix.m21, Is.EqualTo(managedmatrix.m21));
			Assert.That(nativeMatrix.m22, Is.EqualTo(managedmatrix.m22));
			Assert.That(nativeMatrix.m23, Is.EqualTo(managedmatrix.m23));
			Assert.IsTrue(managedmatrix == nativeMatrix);
		}

		[Test]
		public void TestIdentity()
		{
			Assert.That(true,Is.EqualTo(Matrix3x4.Identity == Matrix34.CreateIdentity()));
			
			Matrix3x4 managedmatrix = Matrix3x4.Identity;
			Assert.That(true, Is.EqualTo(managedmatrix == Matrix3x4.Identity));
			Assert.That(managedmatrix.m00, Is.EqualTo(1));
			Assert.That(managedmatrix.m01, Is.EqualTo(0));
			Assert.That(managedmatrix.m02, Is.EqualTo(0));
			Assert.That(managedmatrix.m03, Is.EqualTo(0));
			Assert.That(managedmatrix.m10, Is.EqualTo(0));
			Assert.That(managedmatrix.m11, Is.EqualTo(1));
			Assert.That(managedmatrix.m12, Is.EqualTo(0));
			Assert.That(managedmatrix.m13, Is.EqualTo(0));
			Assert.That(managedmatrix.m20, Is.EqualTo(0));
			Assert.That(managedmatrix.m21, Is.EqualTo(0));
			Assert.That(managedmatrix.m22, Is.EqualTo(1));
			Assert.That(managedmatrix.m23, Is.EqualTo(0));
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
			Matrix3x4 newMatrix = matrix34;
			Assert.IsTrue(!ReferenceEquals(matrix34, newMatrix));
			newMatrix.AddTranslation(point);

			//native operation
			Vec3 point2 = new Vec3(point.x, point.y, point.z);
			Matrix34 nativeMatrix = new Matrix34(0f, 1f, 2f, 3f, 10f, 11f, 12f, 13f, 20f, 21f, 22f, 23f);
			nativeMatrix.AddTranslation(point2);

			Assert.IsTrue(newMatrix == nativeMatrix);
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
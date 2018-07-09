// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using NUnit.Framework;
using CryEngine;
using CryEngine.Common;

namespace Math.Tests
{
	public static partial class NativeExtensions
	{
		public static Matrix33 CreateMatrix33(Vector3 right, Vector3 forward, Vector3 up)
		{
			return Matrix33.CreateMatrix33(right, forward, up);
		}
	}

	[TestFixture]
	public class Matrix3x3Test
	{

		[Test, Description("Convert from native to managed should retain same matrix data")]
		public void ConversionFromNativeToManaged()
		{
			Matrix33 nativeMatrix = new Matrix33(0, 1, 2, 10, 11, 12, 20, 21, 22);
			Matrix3x3 managedmatrix = nativeMatrix;
			Assert.That(nativeMatrix.m00, Is.EqualTo(managedmatrix[0, 0]));
			Assert.That(nativeMatrix.m01, Is.EqualTo(managedmatrix[0, 1]));
			Assert.That(nativeMatrix.m02, Is.EqualTo(managedmatrix[0, 2]));
			Assert.That(nativeMatrix.m10, Is.EqualTo(managedmatrix[1, 0]));
			Assert.That(nativeMatrix.m11, Is.EqualTo(managedmatrix[1, 1]));
			Assert.That(nativeMatrix.m12, Is.EqualTo(managedmatrix[1, 2]));
			Assert.That(nativeMatrix.m20, Is.EqualTo(managedmatrix[2, 0]));
			Assert.That(nativeMatrix.m21, Is.EqualTo(managedmatrix[2, 1]));
			Assert.That(nativeMatrix.m22, Is.EqualTo(managedmatrix[2, 2]));
		}



		[Test, Description("Convert from managed to native")]
		public void ConversionFromManagedToNative()
		{
			Matrix3x3 managedmatrix = new Matrix3x3(0, 1, 2, 10, 11, 12, 20, 21, 22);
			Matrix33 nativeMatrix = managedmatrix;
			Assert.That(nativeMatrix.m00, Is.EqualTo(managedmatrix[0, 0]));
			Assert.That(nativeMatrix.m01, Is.EqualTo(managedmatrix[0, 1]));
			Assert.That(nativeMatrix.m02, Is.EqualTo(managedmatrix[0, 2]));
			Assert.That(nativeMatrix.m10, Is.EqualTo(managedmatrix[1, 0]));
			Assert.That(nativeMatrix.m11, Is.EqualTo(managedmatrix[1, 1]));
			Assert.That(nativeMatrix.m12, Is.EqualTo(managedmatrix[1, 2]));
			Assert.That(nativeMatrix.m20, Is.EqualTo(managedmatrix[2, 0]));
			Assert.That(nativeMatrix.m21, Is.EqualTo(managedmatrix[2, 1]));
			Assert.That(nativeMatrix.m22, Is.EqualTo(managedmatrix[2, 2]));
		}


		[Test]
		public void TestIdentity()
		{
			Matrix33 nativeMatrix = Matrix33.CreateIdentity();
			Matrix3x3 managedmatrix = Matrix3x3.Identity;
			// Compare native vs managed identity
			Assert.That(nativeMatrix.m00, Is.EqualTo(managedmatrix[0, 0]));
			Assert.That(nativeMatrix.m01, Is.EqualTo(managedmatrix[0, 1]));
			Assert.That(nativeMatrix.m02, Is.EqualTo(managedmatrix[0, 2]));
			Assert.That(nativeMatrix.m10, Is.EqualTo(managedmatrix[1, 0]));
			Assert.That(nativeMatrix.m11, Is.EqualTo(managedmatrix[1, 1]));
			Assert.That(nativeMatrix.m12, Is.EqualTo(managedmatrix[1, 2]));
			Assert.That(nativeMatrix.m20, Is.EqualTo(managedmatrix[2, 0]));
			Assert.That(nativeMatrix.m21, Is.EqualTo(managedmatrix[2, 1]));
			Assert.That(nativeMatrix.m22, Is.EqualTo(managedmatrix[2, 2]));

			// Compare managed to the real values of the identity
			Assert.That(managedmatrix[0, 0], Is.EqualTo(1));
			Assert.That(managedmatrix[0, 1], Is.EqualTo(0));
			Assert.That(managedmatrix[0, 2], Is.EqualTo(0));
			Assert.That(managedmatrix[1, 0], Is.EqualTo(0));
			Assert.That(managedmatrix[1, 1], Is.EqualTo(1));
			Assert.That(managedmatrix[1, 2], Is.EqualTo(0));
			Assert.That(managedmatrix[2, 0], Is.EqualTo(0));
			Assert.That(managedmatrix[2, 1], Is.EqualTo(0));
			Assert.That(managedmatrix[2, 2], Is.EqualTo(1));
		}


		[Test]
		public void TestMatrix4x4Construction()
		{
			Matrix4x4 matrix4x4 = new Matrix4x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
			Matrix3x3 matrix3x3 = new Matrix3x3(matrix4x4);
			Assert.IsTrue(matrix3x3[0, 0] == matrix4x4[0, 0]);
			Assert.IsTrue(matrix3x3[0, 1] == matrix4x4[0, 1]);
			Assert.IsTrue(matrix3x3[0, 2] == matrix4x4[0, 2]);
			Assert.IsTrue(matrix3x3[1, 0] == matrix4x4[1, 0]);
			Assert.IsTrue(matrix3x3[1, 1] == matrix4x4[1, 1]);
			Assert.IsTrue(matrix3x3[1, 2] == matrix4x4[1, 2]);
			Assert.IsTrue(matrix3x3[2, 0] == matrix4x4[2, 0]);
			Assert.IsTrue(matrix3x3[2, 1] == matrix4x4[2, 1]);
			Assert.IsTrue(matrix3x3[2, 2] == matrix4x4[2, 2]);
		}

		[Test]
		public void TestMatrix3x4Construction()
		{
			Matrix3x4 matrix3x4 = new Matrix3x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
			Matrix3x3 matrix3x3 = new Matrix3x3(matrix3x4);
			Assert.IsTrue(matrix3x3[0, 0] == matrix3x4[0, 0]);
			Assert.IsTrue(matrix3x3[0, 1] == matrix3x4[0, 1]);
			Assert.IsTrue(matrix3x3[0, 2] == matrix3x4[0, 2]);
			Assert.IsTrue(matrix3x3[1, 0] == matrix3x4[1, 0]);
			Assert.IsTrue(matrix3x3[1, 1] == matrix3x4[1, 1]);
			Assert.IsTrue(matrix3x3[1, 2] == matrix3x4[1, 2]);
			Assert.IsTrue(matrix3x3[2, 0] == matrix3x4[2, 0]);
			Assert.IsTrue(matrix3x3[2, 1] == matrix3x4[2, 1]);
			Assert.IsTrue(matrix3x3[2, 2] == matrix3x4[2, 2]);
		}


		[Test]
		public void TestSetZero()
		{
			Matrix3x3 matrix3x3 = new Matrix3x3(0, 1, 2, 3, 4, 5, 6, 7, 8);
			matrix3x3.SetZero();
			Assert.IsTrue(matrix3x3[0, 0] == 0);
			Assert.IsTrue(matrix3x3[0, 1] == 0);
			Assert.IsTrue(matrix3x3[0, 2] == 0);
			Assert.IsTrue(matrix3x3[1, 0] == 0);
			Assert.IsTrue(matrix3x3[1, 1] == 0);
			Assert.IsTrue(matrix3x3[1, 2] == 0);
			Assert.IsTrue(matrix3x3[2, 0] == 0);
			Assert.IsTrue(matrix3x3[2, 1] == 0);
			Assert.IsTrue(matrix3x3[2, 2] == 0);
		}


		[Test]
		public void TestProperties()
		{
			Vector3 col0 = new Vector3(0, 1, 2);
			Vector3 col1 = new Vector3(3, 4, 5);
			Vector3 col2 = new Vector3(6, 7, 8);
			Matrix3x3 managedMatrix = new Matrix3x3(col0, col1, col2);

			Vector3 row0 = new Vector3(0, 3, 6);
			Vector3 row1 = new Vector3(1, 4, 7);
			Vector3 row2 = new Vector3(2, 5, 8);

			Assert.IsTrue(managedMatrix[0] == row0, "row 0 mismatched");
			Assert.IsTrue(managedMatrix[1] == row1, "row 1 mismatched");
			Assert.IsTrue(managedMatrix[2] == row2, "row 2 mismatched");

			Assert.IsTrue(managedMatrix[0, 0] == row0.x, "[0,0] mismatched");
			Assert.IsTrue(managedMatrix[0, 1] == row0.y, "[0,1] mismatched");
			Assert.IsTrue(managedMatrix[0, 2] == row0.z, "[0,2] mismatched");

			Assert.IsTrue(managedMatrix[1, 0] == row1.x, "[1,0] mismatched");
			Assert.IsTrue(managedMatrix[1, 1] == row1.y, "[1,1] mismatched");
			Assert.IsTrue(managedMatrix[1, 2] == row1.z, "[1,2] mismatched");

			Assert.IsTrue(managedMatrix[2, 0] == row2.x, "[2,0] mismatched");
			Assert.IsTrue(managedMatrix[2, 1] == row2.y, "[2,1] mismatched");
			Assert.IsTrue(managedMatrix[2, 2] == row2.z, "[2,2] mismatched");
		}
	}
}
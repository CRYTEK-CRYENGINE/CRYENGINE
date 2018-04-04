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
			Assert.That(true, Is.EqualTo(managedmatrix == nativeMatrix));
			Assert.That(nativeMatrix.m00, Is.EqualTo(managedmatrix.m00));
			Assert.That(nativeMatrix.m01, Is.EqualTo(managedmatrix.m01));
			Assert.That(nativeMatrix.m02, Is.EqualTo(managedmatrix.m02));
			Assert.That(nativeMatrix.m10, Is.EqualTo(managedmatrix.m10));
			Assert.That(nativeMatrix.m11, Is.EqualTo(managedmatrix.m11));
			Assert.That(nativeMatrix.m12, Is.EqualTo(managedmatrix.m12));
			Assert.That(nativeMatrix.m20, Is.EqualTo(managedmatrix.m20));
			Assert.That(nativeMatrix.m21, Is.EqualTo(managedmatrix.m21));
			Assert.That(nativeMatrix.m22, Is.EqualTo(managedmatrix.m22));
			Assert.IsTrue(nativeMatrix == managedmatrix);
		}



		[Test, Description("Convert from managed to native")]
		public void ConversionFromManagedToNative()
		{
			Matrix3x3 managedmatrix = new Matrix3x3(0, 1, 2, 10, 11, 12, 20, 21, 22);
			Matrix33 nativeMatrix = managedmatrix;
			Assert.That(true, Is.EqualTo(managedmatrix == nativeMatrix));
			Assert.That(nativeMatrix.m00, Is.EqualTo(managedmatrix.m00));
			Assert.That(nativeMatrix.m01, Is.EqualTo(managedmatrix.m01));
			Assert.That(nativeMatrix.m02, Is.EqualTo(managedmatrix.m02));
			Assert.That(nativeMatrix.m10, Is.EqualTo(managedmatrix.m10));
			Assert.That(nativeMatrix.m11, Is.EqualTo(managedmatrix.m11));
			Assert.That(nativeMatrix.m12, Is.EqualTo(managedmatrix.m12));
			Assert.That(nativeMatrix.m20, Is.EqualTo(managedmatrix.m20));
			Assert.That(nativeMatrix.m21, Is.EqualTo(managedmatrix.m21));
			Assert.That(nativeMatrix.m22, Is.EqualTo(managedmatrix.m22));
			Assert.IsTrue(managedmatrix == nativeMatrix);
		}


		[Test]
		public void TestIdentity()
		{
			Assert.That(true, Is.EqualTo(Matrix3x3.Identity == Matrix33.CreateIdentity()));

			Matrix3x3 managedmatrix = Matrix3x3.Identity;
			Assert.That(true, Is.EqualTo(managedmatrix == Matrix3x3.Identity));
			Assert.That(managedmatrix.m00, Is.EqualTo(1));
			Assert.That(managedmatrix.m01, Is.EqualTo(0));
			Assert.That(managedmatrix.m02, Is.EqualTo(0));
			Assert.That(managedmatrix.m10, Is.EqualTo(0));
			Assert.That(managedmatrix.m11, Is.EqualTo(1));
			Assert.That(managedmatrix.m12, Is.EqualTo(0));
			Assert.That(managedmatrix.m20, Is.EqualTo(0));
			Assert.That(managedmatrix.m21, Is.EqualTo(0));
			Assert.That(managedmatrix.m22, Is.EqualTo(1));
		}


		[Test]
		public void TestMatrix4x4Construction()
		{
			Matrix4x4 matrix4x4 = new Matrix4x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
			Matrix3x3 matrix3x3 = new Matrix3x3(matrix4x4);
			Assert.IsTrue(matrix3x3.m00 == matrix4x4.m00);
			Assert.IsTrue(matrix3x3.m01 == matrix4x4.m01);
			Assert.IsTrue(matrix3x3.m02 == matrix4x4.m02);
			Assert.IsTrue(matrix3x3.m10 == matrix4x4.m10);
			Assert.IsTrue(matrix3x3.m11 == matrix4x4.m11);
			Assert.IsTrue(matrix3x3.m12 == matrix4x4.m12);
			Assert.IsTrue(matrix3x3.m20 == matrix4x4.m20);
			Assert.IsTrue(matrix3x3.m21 == matrix4x4.m21);
			Assert.IsTrue(matrix3x3.m22 == matrix4x4.m22);
		}

		[Test]
		public void TestMatrix3x4Construction()
		{
			Matrix3x4 matrix3x4 = new Matrix3x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
			Matrix3x3 matrix3x3 = new Matrix3x3(matrix3x4);
			Assert.IsTrue(matrix3x3.m00 == matrix3x4.m00);
			Assert.IsTrue(matrix3x3.m01 == matrix3x4.m01);
			Assert.IsTrue(matrix3x3.m02 == matrix3x4.m02);
			Assert.IsTrue(matrix3x3.m10 == matrix3x4.m10);
			Assert.IsTrue(matrix3x3.m11 == matrix3x4.m11);
			Assert.IsTrue(matrix3x3.m12 == matrix3x4.m12);
			Assert.IsTrue(matrix3x3.m20 == matrix3x4.m20);
			Assert.IsTrue(matrix3x3.m21 == matrix3x4.m21);
			Assert.IsTrue(matrix3x3.m22 == matrix3x4.m22);
		}


		[Test]
		public void TestSetZero()
		{
			Matrix3x3 matrix3x3 = new Matrix3x3(0, 1, 2, 3, 4, 5, 6, 7, 8);
			matrix3x3.SetZero();
			Assert.IsTrue(matrix3x3.m00 == 0);
			Assert.IsTrue(matrix3x3.m01 == 0);
			Assert.IsTrue(matrix3x3.m02 == 0);
			Assert.IsTrue(matrix3x3.m10 == 0);
			Assert.IsTrue(matrix3x3.m11 == 0);
			Assert.IsTrue(matrix3x3.m12 == 0);
			Assert.IsTrue(matrix3x3.m20 == 0);
			Assert.IsTrue(matrix3x3.m21 == 0);
			Assert.IsTrue(matrix3x3.m22 == 0);
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
// Copyright 2001-2017 Crytek GmbH / CrytekGroup. All rights reserved.

using NUnit.Framework;
using CryEngine;
using CryEngine.Common;

namespace Math.Tests
{

	public static partial class NativeExtensions
	{
		public static string PrintString(this Matrix44 matrix)
		{
			Matrix4x4 m44 = matrix;
			return m44.ToString();
		}
	}

	[TestFixture]
	public class Matrix4x4Test
	{
		[Test, Description("Convert from native to managed should retain same matrix data")]
		public void ConversionFromNativeToManaged()
		{
			Matrix44 nativeMatrix = new Matrix44(0, 1, 2, 3, 10, 11, 12, 13, 20, 21, 22, 23, 30, 31, 32, 33);
			Matrix4x4 managedmatrix = nativeMatrix;
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
			Assert.That(nativeMatrix.m30, Is.EqualTo(managedmatrix.m30));
			Assert.That(nativeMatrix.m31, Is.EqualTo(managedmatrix.m31));
			Assert.That(nativeMatrix.m32, Is.EqualTo(managedmatrix.m32));
			Assert.That(nativeMatrix.m33, Is.EqualTo(managedmatrix.m33));
			Assert.IsTrue(nativeMatrix == managedmatrix);
		}

		[Test, Description("Convert from managed to native should retain same matrix data")]
		public void ConversionManagedToNative()
		{
			Matrix4x4 managedmatrix = new Matrix4x4(0, 1, 2, 3, 10, 11, 12, 13, 20, 21, 22, 23, 30, 31, 32, 33);
			Matrix44 nativeMatrix = managedmatrix;
			Assert.That(true, Is.EqualTo(nativeMatrix == managedmatrix));
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
			Assert.That(nativeMatrix.m30, Is.EqualTo(managedmatrix.m30));
			Assert.That(nativeMatrix.m31, Is.EqualTo(managedmatrix.m31));
			Assert.That(nativeMatrix.m32, Is.EqualTo(managedmatrix.m32));
			Assert.That(nativeMatrix.m33, Is.EqualTo(managedmatrix.m33));
			Assert.IsTrue(managedmatrix == nativeMatrix);
		}

		[Test]
		public void TestIdentity()
		{
			Matrix44 nativeMatrix = new Matrix44(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,16);
			nativeMatrix.SetIdentity();
			Assert.IsTrue(Matrix4x4.Identity == nativeMatrix);

			Matrix4x4 managedmatrix = Matrix4x4.Identity;
			Assert.That(true, Is.EqualTo(managedmatrix == Matrix4x4.Identity));
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

			Assert.That(managedmatrix.m30, Is.EqualTo(0));
			Assert.That(managedmatrix.m31, Is.EqualTo(0));
			Assert.That(managedmatrix.m32, Is.EqualTo(0));
			Assert.That(managedmatrix.m33, Is.EqualTo(1));
		}

		[Test]
		public void TestMatrix3x4Construction()
		{
			Matrix34 nativeMatrix = new Matrix34(1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15);
			Matrix4x4 managedMatrix = new Matrix4x4(nativeMatrix);
			Assert.That(nativeMatrix.m00, Is.EqualTo(managedMatrix.m00));
			Assert.That(nativeMatrix.m01, Is.EqualTo(managedMatrix.m01));
			Assert.That(nativeMatrix.m02, Is.EqualTo(managedMatrix.m02));
			Assert.That(nativeMatrix.m03, Is.EqualTo(managedMatrix.m03));
			Assert.That(nativeMatrix.m10, Is.EqualTo(managedMatrix.m10));
			Assert.That(nativeMatrix.m11, Is.EqualTo(managedMatrix.m11));
			Assert.That(nativeMatrix.m12, Is.EqualTo(managedMatrix.m12));
			Assert.That(nativeMatrix.m13, Is.EqualTo(managedMatrix.m13));
			Assert.That(nativeMatrix.m20, Is.EqualTo(managedMatrix.m20));
			Assert.That(nativeMatrix.m21, Is.EqualTo(managedMatrix.m21));
			Assert.That(nativeMatrix.m22, Is.EqualTo(managedMatrix.m22));
			Assert.That(nativeMatrix.m23, Is.EqualTo(managedMatrix.m23));
			
		}

		[Test]
		public void TestMethods()
		{
			{
				// test identity
				Matrix4x4 managedMatrix = new Matrix4x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
				managedMatrix.SetIdentity();
				Assert.IsTrue(managedMatrix == Matrix4x4.Identity, "Managed matrix fails identity");
			}
			
			{
				//test zero
				Matrix4x4 managedMatrix = new Matrix4x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
				managedMatrix.SetZero();
				Matrix4x4 zeroMatrix = new Matrix4x4(0, 0, 0, 0, 
																							0, 0, 0, 0, 
																							0, 0, 0, 0, 
																							0, 0, 0, 0);
				Assert.IsTrue(managedMatrix == zeroMatrix, "Managed matrix fails zero");
			}

			{
				//test transpose
				Matrix4x4 managedMatrix = new Matrix4x4(1, 1, 1, 0,
																								0, 3, 1, 2,
																								2, 3, 1, 0,
																								1, 0, 2, 1);
				Matrix4x4 transposedManagedMatrix = new Matrix4x4(1,0,2,1,
																													1,3,3,0,
																													1,1,1,2,
																													0,2,0,1);
				Matrix4x4 transposedMatrix = managedMatrix.GetTransposed();
				Assert.IsTrue(transposedMatrix == transposedManagedMatrix, "Managed matrix fails transpose 1: Expected "+transposedManagedMatrix.ToString()+ " Actual :"+ transposedManagedMatrix.ToString());
				Assert.IsTrue(transposedManagedMatrix == transposedMatrix, "Managed matrix fails transpose 2");

			}
			{
				//test inversion
				Matrix4x4 matrix4x4 = new Matrix4x4(1, 1, 1, 0,
																					0, 3, 1, 2,
																					2, 3, 1, 0,
																					1, 0, 2, 1);
				//inversed will be
				Matrix4x4 inverseMatrixExpected = new Matrix4x4(-3, -1f/2, 3f/2, 1f,
																								1f, 1f/4, -1f/4, -1f/2,
																								3f, 1f/4, -5f/4, -1f/2,
																								-3, 0, 1, 1);

				Matrix4x4 inverseMatrixCalculated = matrix4x4.GetInverted();
				Assert.IsTrue(inverseMatrixCalculated == inverseMatrixExpected, "Matrix4x4 inversion failed : Expected " + inverseMatrixExpected.ToString() + " Actual :" + inverseMatrixCalculated.ToString());

				Matrix44 matrixNative44 =new Matrix44(1, 1, 1, 0,
																					0, 3, 1, 2,
																					2, 3, 1, 0,
																					1, 0, 2, 1);

				//inversed will be
				Matrix44 inverseMatrix44Expected = new Matrix44(-3, -1f / 2, 3f / 2, 1f,
																								1f, 1f / 4, -1f / 4, -1f / 2,
																								3f, 1f / 4, -5f / 4, -1f / 2,
																								-3, 0, 1, 1);

				Matrix44 inverseMatrixNative44Calculated = matrixNative44.GetInverted();
				Assert.IsTrue(new Matrix4x4(inverseMatrix44Expected) == new Matrix4x4(inverseMatrixNative44Calculated), "Matrix4x4 inversion failed : Expected " + inverseMatrix44Expected.PrintString() + " Actual :" + inverseMatrixNative44Calculated.PrintString());
			}
			{
				//test Determinant
				Matrix4x4 matrix4x4 = new Matrix4x4(1, 1, 1, 0,
																						0, 3, 1, 2,
																						2, 3, 1, 0,
																						1, 0, 2, 1);
				float determinant1 = matrix4x4.Determinant();
				Matrix44 nativeMatrix44 = new Matrix44(1, 1, 1, 0,
																								0, 3, 1, 2,
																								2, 3, 1, 0,
																								1, 0, 2, 1);
				float determinant2 = nativeMatrix44.Determinant();
				Assert.IsTrue(determinant1 == determinant2, "Determinants are not equal ");
			}
		}

	}
}
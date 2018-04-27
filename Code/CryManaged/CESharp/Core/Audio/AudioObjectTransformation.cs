// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Runtime.CompilerServices;
using CryEngine.Common;
using CryEngine.Common.CryAudio;

namespace CryEngine
{
	using NativeAudioSystem = CryEngine.NativeInternals.IAudioSystem;

	internal sealed class AudioObjectTransformation : IDisposable
	{
		System.Runtime.InteropServices.HandleRef _objectTransformationPtr;
		Matrix3x4 _matrix3x4;
		private bool _isDisposed;

		~AudioObjectTransformation()
		{
			Dispose(false);
		}

		internal AudioObjectTransformation(Matrix3x4 managedMatrix)
		{
			_matrix3x4 = managedMatrix;
			_objectTransformationPtr = new System.Runtime.InteropServices.HandleRef(this, NativeAudioSystem.CreateAudioTransformation(_matrix3x4[0, 0], _matrix3x4[0, 1], _matrix3x4[0, 2], _matrix3x4[0, 3],
			                                                                                                                          _matrix3x4[1, 0], _matrix3x4[1, 1], _matrix3x4[1, 2], _matrix3x4[1, 3],
			                                                                                                                          _matrix3x4[2, 0], _matrix3x4[2, 1], _matrix3x4[2, 2], _matrix3x4[2, 3]));
		}

		internal IntPtr NativePtr
		{
			get
			{
				return _objectTransformationPtr.Handle;
			}
		}

		public Vector3 Forward
		{
			get
			{
				return new Vector3(_matrix3x4[0, 1], _matrix3x4[1, 1], _matrix3x4[2, 1]);
			}
		}

		public Vector3 Up
		{
			get
			{
				return new Vector3(_matrix3x4[0, 2], _matrix3x4[1, 2], _matrix3x4[2, 2]);
			}
		}
		public Vector3 Position
		{
			get
			{
				return new Vector3(_matrix3x4[0, 3], _matrix3x4[1, 3], _matrix3x4[2, 3]);
			}
		}

		public void SetTransform(Matrix3x4 matrix)
		{
			_matrix3x4 = matrix;
			//release old CObjectTransformation
			if (_objectTransformationPtr.Handle != IntPtr.Zero)
			{
				NativeAudioSystem.ReleaseAudioTransformation(_objectTransformationPtr.Handle);
			}
			_objectTransformationPtr = new System.Runtime.InteropServices.HandleRef(this, NativeAudioSystem.CreateAudioTransformation(_matrix3x4[0, 0], _matrix3x4[0, 1], _matrix3x4[0, 2], _matrix3x4[0, 3],
			                                                                                                                          _matrix3x4[1, 0], _matrix3x4[1, 1], _matrix3x4[1, 2], _matrix3x4[1, 3],
			                                                                                                                          _matrix3x4[2, 0], _matrix3x4[2, 1], _matrix3x4[2, 2], _matrix3x4[2, 3]));
		}

		public void Dispose()
		{
			Dispose(true);
			System.GC.SuppressFinalize(this);
		}

		private void Dispose(bool disposing)
		{
			if (_isDisposed) return;

			if (disposing)
			{
				_matrix3x4 = null;
			}
			if (_objectTransformationPtr.Handle != System.IntPtr.Zero)
			{
				NativeAudioSystem.ReleaseAudioTransformation(_objectTransformationPtr.Handle);
				_objectTransformationPtr = new System.Runtime.InteropServices.HandleRef(null, IntPtr.Zero);
			}
			_isDisposed = true;
		}
	}
}

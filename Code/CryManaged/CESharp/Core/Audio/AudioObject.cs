// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;

namespace CryEngine
{
	using NativeAudioSystem = NativeInternals.IAudioSystem;

	/// <summary>
	/// An audio object has more audio configuration options such as transformation in the 3D space.
	/// </summary>
	public sealed class AudioObject : IDisposable
	{
		private System.Runtime.InteropServices.HandleRef _cPtr;
		private AudioObjectTransformation _audioTransformation;
		private bool _isDisposed;

		/// <summary>
		/// Creates an Audio object 
		/// </summary>
		public AudioObject():this(NativeAudioSystem.CreateAudioObject())
		{
		}

		internal AudioObject(IntPtr nativePtr)
		{
			_cPtr = new System.Runtime.InteropServices.HandleRef(this, nativePtr);
		}

		/// <summary>
		/// Destructor for the AudioObject which ensures Dispose is called.
		/// </summary>
		~AudioObject()
		{
			Dispose(false);
		}

		private void Dispose(bool isDisposing)
		{
			if (_isDisposed) return;

			if(isDisposing)
			{
				_audioTransformation.Dispose();
				_audioTransformation = null;
			}
			if (_cPtr.Handle != System.IntPtr.Zero)
			{
				NativeAudioSystem.ReleaseAudioObject(_cPtr.Handle);
				_cPtr = new System.Runtime.InteropServices.HandleRef(null, IntPtr.Zero);
			}
			_isDisposed = true;
		}

		internal IntPtr NativePtr
		{
			get
			{
				return _cPtr.Handle;
			}
		}

		/// <summary>
		/// Sets the transformation of the audio object
		/// </summary>
		/// <param name="position"></param>
		/// <param name="forward"></param>
		/// <param name="up"></param>
		public void SetTransformation(Vector3 position, Vector3 forward, Vector3 up)
		{
			// create managed matrix and once done call native method 
			if(_audioTransformation != null)
			{
				_audioTransformation.Dispose();
				_audioTransformation = null;
			}
			_audioTransformation = new AudioObjectTransformation(new Matrix3x4(
				0, forward.x, up.x, position.x,
				0, forward.y, up.y, position.y,
				0, forward.z, up.z, position.z
				));
			NativeAudioSystem.SetAudioTransformation(_cPtr.Handle, _audioTransformation.NativePtr);
		}

		/// <summary>
		/// Sets the transformation of the audio object
		/// </summary>
		/// <param name="matrix"></param>
		public void SetTransformation(Matrix3x4 matrix)
		{
			if (_audioTransformation != null)
			{
				_audioTransformation.Dispose();
				_audioTransformation = null;
			}
			_audioTransformation = new AudioObjectTransformation(matrix);
			NativeAudioSystem.SetAudioTransformation(_cPtr.Handle, _audioTransformation.NativePtr);
		}
		
		/// <summary>
		/// Executes the specified trigger name on this audio object
		/// </summary>
		/// <param name="triggerName"></param>
		public void Play(string triggerName)
		{
			uint triggerId = Audio.InvalidControlId;
			AudioManager.AddOrGetTrigger(triggerName, out triggerId);
			if(triggerId != Audio.InvalidControlId)
			{
				NativeAudioSystem.ExecuteAudioObjectTrigger(_cPtr.Handle, triggerId, false);
			}
		}

		/// <summary>
		/// Executes the specified trigger id on this audio object
		/// </summary>
		/// <param name="triggerId"></param>
		public void Play(AudioTriggerId triggerId)
		{
			if(triggerId.IsValid)
			{
				NativeAudioSystem.ExecuteAudioObjectTrigger(_cPtr.Handle, triggerId._id, false);
			}
		}

		/// <summary>
		/// Stops the specified trigger name on this audio object
		/// </summary>
		/// <param name="triggerName"></param>
		public void Stop(string triggerName)
		{
			uint triggerId = Audio.InvalidControlId;
			AudioManager.AddOrGetTrigger(triggerName, out triggerId);
			if(triggerId != Audio.InvalidControlId)
			{
				NativeAudioSystem.StopAudioObjectTrigger(_cPtr.Handle, triggerId, false);
			}
		}

		/// <summary>
		/// Stops the specified trigger id on this audio object
		/// </summary>
		/// <param name="triggerId"></param>
		public void Stop(AudioTriggerId triggerId)
		{
			if(triggerId.IsValid)
			{
				NativeAudioSystem.StopAudioObjectTrigger(_cPtr.Handle, triggerId._id, false);
			}
		}

		/// <summary>
		/// Disposes this instance.
		/// </summary>
		public void Dispose()
		{
			Dispose(true);
			System.GC.SuppressFinalize(this);
		}
	}
}
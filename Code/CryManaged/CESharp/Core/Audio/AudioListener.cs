// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Runtime.CompilerServices;
using CryEngine.Common;
using CryEngine.Common.CryAudio;

namespace CryEngine
{
	using NativeAudioSystem = CryEngine.NativeInternals.IAudioSystem;

	/// <summary>
	/// The AudioListener allows the user to be notified of all the audio in the level. The most common usage is to use the AudioListener to set the transformation when the view matrix of the active camera (position in world) changes.
	/// There is only one AudioListener currently used in the audio system.
	/// 
	/// </summary>
	public sealed class AudioListener
	{
		private System.Runtime.InteropServices.HandleRef _cPtr;

		private static AudioListener _audioListener;
		private static readonly Object _lock = new Object();

		internal AudioListener()
		{
			_cPtr = new System.Runtime.InteropServices.HandleRef(this, NativeAudioSystem.CreateAudioListener());
		}

		/// <summary>
		/// Applies the transformation to any audio.
		/// </summary>
		/// <param name="matrix3x4"></param>
		public void SetTransformation(Matrix3x4 matrix3x4)
		{
			using (AudioObjectTransformation audioObjectTransformation = new AudioObjectTransformation(matrix3x4)) // can safely dispose this as the C++ implementation uses a copy
			{
				NativeAudioSystem.SetAudioListenerTransformation(_cPtr.Handle, audioObjectTransformation.NativePtr);
			}
		}

		/// <summary>
		/// Retrieves the AudioListener.
		/// </summary>
		/// <returns></returns>
		public static AudioListener GetAudioListener()
		{
			if(_audioListener == null)
			{
				lock(_lock)
				{
					if(_audioListener == null)
					{
						_audioListener = new AudioListener();
					}
				}
			}
			return _audioListener;
		}
	}
}

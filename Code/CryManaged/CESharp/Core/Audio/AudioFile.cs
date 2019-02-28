// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;

namespace CryEngine
{
	using NativeAudioSystem = NativeInternals.IAudioSystem;
	/// <summary>
	/// AudioFile is most direct with minimal setup in the Sandbox editor to playback an audio file
	/// 
	/// </summary>
	public sealed class AudioFile :IDisposable
	{
		private System.Runtime.InteropServices.HandleRef _audioPlayFileInfoPtr;
		private String _audioFilePath;
		private bool _isDisposed;

		/// <summary>
		/// Creates an audio file to playback with the specified file path. Note: filePath should not have the file extension. Eg "helloworld.wav" should be passed as "helloworld"
		/// </summary>
		/// <param name="filePath"></param>
		public AudioFile(string filePath)
		{
			_audioFilePath = filePath;
			_audioPlayFileInfoPtr = new System.Runtime.InteropServices.HandleRef(this, NativeAudioSystem.CreateSPlayFileInfo(_audioFilePath));
		}

		/// <summary>
		/// Destructor for the AudioFile which ensures Dispose is called.
		/// </summary>
		~AudioFile()
		{
			Dispose(false);
		}

		private void Dispose(bool isDisposing)
		{
			if (_isDisposed) return;

			if(isDisposing)
			{
				_audioFilePath = null;
			}
			if (_audioPlayFileInfoPtr.Handle != System.IntPtr.Zero)
			{
				NativeAudioSystem.ReleaseSPlayFileInfo(_audioPlayFileInfoPtr.Handle);
				_audioPlayFileInfoPtr = new System.Runtime.InteropServices.HandleRef(null, IntPtr.Zero);
			}
			_isDisposed = true;
		}

		/// <summary>
		/// Disposes this instance.
		/// </summary>
		public void Dispose()
		{
			Dispose(true);
			System.GC.SuppressFinalize(this);
		}

		internal IntPtr NativePtr
		{
			get
			{
				return _audioPlayFileInfoPtr.Handle;
			}
		}
		

		/// <summary>
		/// Returns the file path of audio file for playback
		/// </summary>
		public string FilePath
		{
			get
			{
				return _audioFilePath;
			}
		}

		/// <summary>
		/// Plays the audio file
		/// </summary>
		public void Play()
		{
			NativeAudioSystem.PlayFile(_audioPlayFileInfoPtr.Handle);
		}

		/// <summary>
		/// Stops the audio file
		/// </summary>
		public void Stop()
		{
			NativeAudioSystem.StopFile(_audioPlayFileInfoPtr.Handle);
		}
	}
}
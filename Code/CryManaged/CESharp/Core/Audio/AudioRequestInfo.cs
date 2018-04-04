// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Runtime.CompilerServices;
using CryEngine.Common;
using CryEngine.Common.CryAudio;

namespace CryEngine
{
	using NativeAudioSystem = CryEngine.NativeInternals.IAudioSystem;

	public sealed class AudioRequestInfo: IDisposable
	{
		private System.Runtime.InteropServices.HandleRef _cPtr;

		private bool _isCopy;
		private uint _requestResult;
		private uint _flagsType;
		private uint _controlId;
		private IntPtr _audioObject;
		private bool _isDisposed;

		public AudioRequestInfo(uint eRequestResult, uint audioSystemEvent, uint CtrlId, AudioObject audioObject) :this(NativeAudioSystem.CreateSRequestInfo(eRequestResult, audioSystemEvent, CtrlId, audioObject.NativePtr))
		{
		}

		internal AudioRequestInfo(IntPtr nativePtr,bool isCopy=false)
		{
			_isCopy = isCopy;
			_cPtr = new System.Runtime.InteropServices.HandleRef(this, nativePtr);
			if(_isCopy)
			{
				_requestResult = NativeAudioSystem.SRIGetRequestResult(nativePtr);
				_flagsType = NativeAudioSystem.SRIGetEnumFlagsType(nativePtr);
				_controlId = NativeAudioSystem.SRIGetControlId(nativePtr);
				_audioObject = NativeAudioSystem.SRIGetAudioObject(nativePtr);
			}
		}

		~AudioRequestInfo()
		{
			Dispose(false);
		}

		private void Dispose(bool isDisposing)
		{
			if (_isDisposed) return;
			
			if (_cPtr.Handle != IntPtr.Zero && !_isCopy)
			{
				NativeAudioSystem.ReleaseSRequestInfo(_cPtr.Handle);
				_cPtr = new System.Runtime.InteropServices.HandleRef(null, IntPtr.Zero);
			}
			_isDisposed = true;
		}

		public void Dispose()
		{
			Dispose(true);
			System.GC.SuppressFinalize(this);
			
		}

		public uint RequestResult
		{
			get
			{
				return _isCopy? _requestResult : NativeAudioSystem.SRIGetRequestResult(_cPtr.Handle);
			}
		}

		public uint FlagsType
		{
			get
			{
				return _isCopy? _flagsType : NativeAudioSystem.SRIGetEnumFlagsType(_cPtr.Handle);
			}
		}

		public uint ControlId
		{
			get
			{
				return _isCopy? _controlId : NativeAudioSystem.SRIGetControlId(_cPtr.Handle);
			}
		}
		internal IntPtr AudioObject
		{
			get
			{
				return _isCopy?_audioObject: NativeAudioSystem.SRIGetAudioObject(_cPtr.Handle);
			}
		}
	}
}
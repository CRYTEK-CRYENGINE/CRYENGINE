// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Runtime.CompilerServices;

namespace CryEngine.NativeInternals
{
	internal static class IAudioSystem
	{
		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static uint GetAudioTriggerId(string triggerName);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static uint GetAudioParameterId(string parameterName);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void SetAudioParameter(uint parameterId, float parameterValue);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static uint GetAudioSwitchId(string switchName);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static uint GetAudioSwitchStateId(uint audioSwitchId, string audioSwitchStateName);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void SetAudioSwitchState(uint audioSwitchId, uint audioSwitchStateId);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void StopTrigger(uint triggerId, bool isBlocking = false);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void ExecuteTrigger(uint triggerId, bool isBlocking = false);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void LoadTrigger(uint triggerId);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void UnloadTrigger(uint triggerId);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static string GetConfigPath();

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void PlayFile(IntPtr sPlayFileInfo);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void StopFile(IntPtr sPlayFileInfo);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void EnableAllSound(bool isEnabled);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void StopAllSounds();

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static IntPtr CreateAudioObject();

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static IntPtr CreateAudioTransformation(float m00, float m01, float m02, float m03,
																												float m10, float m11, float m12, float m13,
																												float m20, float m21, float m22, float m23);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void SetAudioTransformation(IntPtr audioObject, IntPtr audioTransformation);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void ReleaseAudioTransformation(IntPtr audioTransformation);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void ReleaseAudioObject(IntPtr audioObject);
		
		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void ExecuteAudioObjectTrigger(IntPtr audioObject, uint triggerId, bool isBlocking = false);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void StopAudioObjectTrigger(IntPtr audioObject, uint triggerId, bool isBlocking = false);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static IntPtr CreateSPlayFileInfo(string audioFilePath);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static IntPtr ReleaseSPlayFileInfo(IntPtr sPlayFileInfo);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static IntPtr CreateSRequestInfo(uint eRequestResult, uint audioSystemEvent, uint CtrlId, IntPtr audioObject);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void ReleaseSRequestInfo(IntPtr requestInfo);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static uint SRIGetRequestResult(IntPtr requestInfo);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static uint SRIGetEnumFlagsType(IntPtr requestInfo);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static uint SRIGetControlId(IntPtr requestInfo);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static IntPtr SRIGetAudioObject(IntPtr requestInfo);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void AddAudioRequestListener(IntPtr audioRequestInfoFunctionPointer);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void RemoveAudioRequestListener(IntPtr audioRequestInfoFunctionPointer);

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static IntPtr CreateAudioListener();

		[MethodImpl(MethodImplOptions.InternalCall)]
		extern public static void SetAudioListenerTransformation(IntPtr audioListener, IntPtr audioTransfromation);
	}
}
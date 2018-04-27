// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioCVars.h"
#include "Common/Logger.h"
#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
void CCVars::RegisterVariables()
{
#if CRY_PLATFORM_WINDOWS
	m_fileCacheManagerSize = 384 << 10;      // 384 MiB on PC
	m_audioObjectPoolSize = 256;
	m_audioEventPoolSize = 256;
	m_audioStandaloneFilePoolSize = 1;
	m_occlusionMaxSyncDistance = 10.0f;
	m_occlusionHighDistance = 10.0f;
	m_occlusionMediumDistance = 80.0f;
	m_fullObstructionMaxDistance = 5.0f;
	m_velocityTrackingThreshold = 0.1f;
	m_occlusionRayLengthOffset = 0.1f;
#elif CRY_PLATFORM_DURANGO
	m_fileCacheManagerSize = 384 << 10;      // 384 MiB on XboxOne
	m_audioObjectPoolSize = 256;
	m_audioEventPoolSize = 256;
	m_audioStandaloneFilePoolSize = 1;
	m_occlusionMaxSyncDistance = 10.0f;
	m_occlusionHighDistance = 10.0f;
	m_occlusionMediumDistance = 80.0f;
	m_fullObstructionMaxDistance = 5.0f;
	m_velocityTrackingThreshold = 0.1f;
	m_occlusionRayLengthOffset = 0.1f;
#elif CRY_PLATFORM_ORBIS
	m_fileCacheManagerSize = 384 << 10;      // 384 MiB on PS4
	m_audioObjectPoolSize = 256;
	m_audioEventPoolSize = 256;
	m_audioStandaloneFilePoolSize = 1;
	m_occlusionMaxSyncDistance = 10.0f;
	m_occlusionHighDistance = 10.0f;
	m_occlusionMediumDistance = 80.0f;
	m_fullObstructionMaxDistance = 5.0f;
	m_velocityTrackingThreshold = 0.1f;
	m_occlusionRayLengthOffset = 0.1f;
#elif CRY_PLATFORM_MAC
	m_fileCacheManagerSize = 384 << 10;      // 384 MiB on Mac
	m_audioObjectPoolSize = 256;
	m_audioEventPoolSize = 256;
	m_audioStandaloneFilePoolSize = 1;
	m_occlusionMaxSyncDistance = 10.0f;
	m_occlusionHighDistance = 10.0f;
	m_occlusionMediumDistance = 80.0f;
	m_fullObstructionMaxDistance = 5.0f;
	m_velocityTrackingThreshold = 0.1f;
	m_occlusionRayLengthOffset = 0.1f;
#elif CRY_PLATFORM_LINUX
	m_fileCacheManagerSize = 384 << 10;      // 384 MiB on Linux
	m_audioObjectPoolSize = 256;
	m_audioEventPoolSize = 256;
	m_audioStandaloneFilePoolSize = 1;
	m_occlusionMaxSyncDistance = 10.0f;
	m_occlusionHighDistance = 10.0f;
	m_occlusionMediumDistance = 80.0f;
	m_fullObstructionMaxDistance = 5.0f;
	m_velocityTrackingThreshold = 0.1f;
	m_occlusionRayLengthOffset = 0.1f;
#elif CRY_PLATFORM_IOS
	m_fileCacheManagerSize = 384 << 10;      // 384 MiB on iOS
	m_audioObjectPoolSize = 256;
	m_audioEventPoolSize = 256;
	m_audioStandaloneFilePoolSize = 1;
	m_occlusionMaxSyncDistance = 10.0f;
	m_occlusionHighDistance = 10.0f;
	m_occlusionMediumDistance = 80.0f;
	m_fullObstructionMaxDistance = 5.0f;
	m_velocityTrackingThreshold = 0.1f;
	m_occlusionRayLengthOffset = 0.1f;
#elif CRY_PLATFORM_ANDROID
	m_fileCacheManagerSize = 72 << 10;      // 72 MiB
	m_audioObjectPoolSize = 256;
	m_audioEventPoolSize = 256;
	m_audioStandaloneFilePoolSize = 1;
	m_occlusionMaxSyncDistance = 10.0f;
	m_occlusionHighDistance = 10.0f;
	m_occlusionMediumDistance = 80.0f;
	m_fullObstructionMaxDistance = 5.0f;
	m_velocityTrackingThreshold = 0.1f;
	m_occlusionRayLengthOffset = 0.1f;
#else
	#error "Undefined platform."
#endif

	REGISTER_CVAR2("s_OcclusionMaxDistance", &m_occlusionMaxDistance, m_occlusionMaxDistance, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Occlusion is not calculated for audio objects, whose distance to the listener is greater than this value. Setting this value to 0 disables obstruction/occlusion calculations.\n"
	               "Usage: s_OcclusionMaxDistance [0/...]\n"
	               "Default: 500 m\n");

	REGISTER_CVAR2("s_OcclusionMinDistance", &m_occlusionMinDistance, m_occlusionMinDistance, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Occlusion is not calculated for audio objects, whose distance to the listener is smaller than this value.\n"
	               "Usage: s_OcclusionMinDistance [0/...]\n"
	               "Default: 0.1 m\n");

	REGISTER_CVAR2("s_OcclusionMaxSyncDistance", &m_occlusionMaxSyncDistance, m_occlusionMaxSyncDistance, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Physics rays are processed synchronously for the sounds that are closer to the listener than this value, and asynchronously for the rest (possible performance optimization).\n"
	               "Usage: s_OcclusionMaxSyncDistance [0/...]\n"
	               "Default: 10 m\n");

	REGISTER_CVAR2("s_OcclusionHighDistance", &m_occlusionHighDistance, m_occlusionHighDistance, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Within this distance occlusion calculation uses the most sample points for highest granularity.\n"
	               "Usage: s_OcclusionHighDistance [0/...]\n"
	               "Default: 10 m\n");

	REGISTER_CVAR2("s_OcclusionMediumDistance", &m_occlusionMediumDistance, m_occlusionMediumDistance, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Between end of high and this distance occlusion calculation uses a medium amount of sample points for medium granularity.\n"
	               "Usage: s_OcclusionMediumDistance [0/...]\n"
	               "Default: 80 m\n");

	REGISTER_CVAR2("s_FullObstructionMaxDistance", &m_fullObstructionMaxDistance, m_fullObstructionMaxDistance, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "for the sounds, whose distance to the listener is greater than this value, the obstruction is value gets attenuated with distance.\n"
	               "Usage: s_FullObstructionMaxDistance [0/...]\n"
	               "Default: 5 m\n");

	REGISTER_CVAR2("s_PositionUpdateThresholdMultiplier", &m_positionUpdateThresholdMultiplier, m_positionUpdateThresholdMultiplier, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "An audio object's distance to the listener is multiplied by this value to determine the position update threshold.\n"
	               "Usage: s_PositionUpdateThresholdMultiplier [0/...]\n"
	               "Default: 0.02\n");

	REGISTER_CVAR2("s_VelocityTrackingThreshold", &m_velocityTrackingThreshold, m_velocityTrackingThreshold, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "An audio object has to change its velocity by at least this amount to issue an \"absolute_velocity\" parameter update request to the audio system.\n"
	               "Usage: s_VelocityTrackingThreshold [0/...]\n"
	               "Default: 0.1 (10 cm/s)\n");

	REGISTER_CVAR2("s_OcclusionRayLengthOffset", &m_occlusionRayLengthOffset, m_occlusionRayLengthOffset, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "A physics ray cast between audio listener and audio object stops at this distance before it hits the audio object.\n"
	               "Effectively forming a bubble of this radius around the audio object where occlusion is ignored.\n"
	               "Usage: s_OcclusionRayLengthOffset [0/...]\n"
	               "Default: 0.1 (10 cm)\n");

	REGISTER_CVAR2("s_ListenerOcclusionPlaneSize", &m_listenerOcclusionPlaneSize, 1.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Sets the size of the plane at listener position against which occlusion is calculated.\n"
	               "Usage: s_ListenerOcclusionPlaneSize [0/...]\n"
	               "Default: 1.0 (100 cm)\n");

	REGISTER_CVAR2("s_FileCacheManagerSize", &m_fileCacheManagerSize, m_fileCacheManagerSize, VF_REQUIRE_APP_RESTART,
	               "Sets the size in KiB the AFCM will allocate on the heap.\n"
	               "Usage: s_FileCacheManagerSize [0/...]\n"
	               "Default PC: 393216 (384 MiB), XboxOne: 393216 (384 MiB), PS4: 393216 (384 MiB), Mac: 393216 (384 MiB), Linux: 393216 (384 MiB), iOS: 2048 (2 MiB), Android: 73728 (72 MiB)\n");

	REGISTER_CVAR2("s_AudioObjectPoolSize", &m_audioObjectPoolSize, m_audioObjectPoolSize, VF_REQUIRE_APP_RESTART,
	               "Sets the number of preallocated audio objects and corresponding audio proxies.\n"
	               "Usage: s_AudioObjectPoolSize [0/...]\n"
	               "Default PC: 256, XboxOne: 256, PS4: 256, Mac: 256, Linux: 256, iOS: 256, Android: 256\n");

	REGISTER_CVAR2("s_AudioEventPoolSize", &m_audioEventPoolSize, m_audioEventPoolSize, VF_REQUIRE_APP_RESTART,
	               "Sets the number of preallocated audio events.\n"
	               "Usage: s_AudioEventPoolSize [0/...]\n"
	               "Default PC: 256, XboxOne: 256, PS4: 256, Mac: 256, Linux: 256, iOS: 256, Android: 256\n");

	REGISTER_CVAR2("s_AudioStandaloneFilePoolSize", &m_audioStandaloneFilePoolSize, m_audioStandaloneFilePoolSize, VF_REQUIRE_APP_RESTART,
	               "Sets the number of preallocated audio standalone files.\n"
	               "Usage: s_AudioStandaloneFilePoolSize [0/...]\n"
	               "Default PC: 1, XboxOne: 1, PS4: 1, Mac: 1, Linux: 1, iOS: 1, Android: 1\n");

	REGISTER_CVAR2("s_AccumulateOcclusion", &m_accumulateOcclusion, m_accumulateOcclusion, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Sets whether occlusion values encountered by a ray cast will be accumulated or only the highest value will be used.\n"
	               "Usage: s_AccumulateOcclusion [0/1] (off/on)\n"
	               "Default PC: 1, XboxOne: 1, PS4: 1, Mac: 1, Linux: 1, iOS: 1, Android: 1\n");

	REGISTER_CVAR2("s_IgnoreWindowFocus", &m_ignoreWindowFocus, 0, VF_NULL,
	               "If set to 1, the audio system will not execute the \"lose_focus\" and \"get_focus\" triggers when the application window focus changes.\n"
	               "Usage: s_IgnoreWindowFocus [0/1]\n"
	               "Default: 0 (off)\n");

	REGISTER_COMMAND("s_ExecuteTrigger", CmdExecuteTrigger, VF_CHEAT,
	                 "Execute an Audio Trigger.\n"
	                 "The first argument is the name of the AudioTrigger to be executed, the second argument is an optional AudioObject ID.\n"
	                 "If the second argument is provided, the AudioTrigger is executed on the AudioObject with the given ID,\n"
	                 "otherwise, the AudioTrigger is executed on the GlobalAudioObject\n"
	                 "Usage: s_ExecuteTrigger Play_chicken_idle 605 or s_ExecuteTrigger MuteDialog\n");

	REGISTER_COMMAND("s_StopTrigger", CmdStopTrigger, VF_CHEAT,
	                 "Execute an Audio Trigger.\n"
	                 "The first argument is the name of the AudioTrigger to be stopped, the second argument is an optional AudioObject ID.\n"
	                 "If the second argument is provided, the AudioTrigger is stopped on the AudioObject with the given ID,\n"
	                 "otherwise, the AudioTrigger is stopped on the GlobalAudioObject\n"
	                 "Usage: s_StopTrigger Play_chicken_idle 605 or s_StopTrigger MuteDialog\n");

	REGISTER_COMMAND("s_SetParameter", CmdSetParameter, VF_CHEAT,
	                 "Set an Audio Parameter value.\n"
	                 "The first argument is the name of the parameter to be set, the second argument is the float value to be set,"
	                 "the third argument is an optional AudioObject ID.\n"
	                 "If the third argument is provided, the parameter is set on the AudioObject with the given ID,\n"
	                 "otherwise, the AudioParameter is set on the GlobalAudioObject\n"
	                 "Usage: s_SetParameter character_speed  0.0  601 or s_SetParameter volume_music 1.0\n");

	REGISTER_COMMAND("s_SetSwitchState", CmdSetSwitchState, VF_CHEAT,
	                 "Set an Audio Switch to a provided State.\n"
	                 "The first argument is the name of the AudioSwitch to, the second argument is the name of the SwitchState to be set,"
	                 "the third argument is an optional AudioObject ID.\n"
	                 "If the third argument is provided, the AudioSwitch is set on the AudioObject with the given ID,\n"
	                 "otherwise, the AudioSwitch is set on the GlobalAudioObject\n"
	                 "Usage: s_SetSwitchState SurfaceType concrete 601 or s_SetSwitchState weather rain\n");

	REGISTER_STRING("s_DefaultStandaloneFilesAudioTrigger", s_szDoNothingTriggerName, 0,
	                "The name of the ATL AudioTrigger which is used for playing back standalone files, when you call 'PlayFile' without specifying\n"
	                "an override triggerId that should be used instead.\n"
	                "Usage: s_DefaultStandaloneFilesAudioTrigger audio_trigger_name.\n"
	                "If you change this CVar to be empty, the control will not be created automatically.\n"
	                "Default: \"do_nothing\" \n");

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	REGISTER_CVAR2("s_DebugDistance", &m_debugDistance, m_debugDistance, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Limits drawing of audio object debug info to the specified distance around the active listeners. Setting this cvar to 0 disables the limiting.\n"
	               "Usage: s_DebugDistance [0/...]\n"
	               "Default: 0 m (infinite)\n");

	REGISTER_CVAR2("s_DrawAudioDebug", &m_drawAudioDebug, 0, VF_CHEAT | VF_CHEAT_NOCHECK | VF_BITFIELD,
	               "Draws AudioTranslationLayer related debug data to the screen.\n"
	               "Usage: s_DrawAudioDebug [0ab...] (flags can be combined)\n"
	               "0: No audio debug info on the screen.\n"
	               "a: Draw spheres around active audio objects.\n"
	               "b: Show text labels for active audio objects.\n"
	               "c: Show trigger names for active audio objects.\n"
	               "d: Show current states for active audio objects.\n"
	               "e: Show Parameter values for active audio objects.\n"
	               "f: Show Environment amounts for active audio objects.\n"
	               "g: Show distance to listener for active audio objects.\n"
	               "h: Show occlusion ray labels.\n"
	               "i: Draw occlusion rays.\n"
	               "j: Show object standalone files.\n"
	               "m: Hide audio system memory info.\n"
	               "n: Apply filter also to inactive object debug info.\n"
	               "u: List standalone files.\n"
	               "v: List active Events.\n"
	               "w: List active Audio Objects.\n"
	               "x: Show FileCache Manager debug info.\n"
	               );

	REGISTER_CVAR2("s_FileCacheManagerDebugFilter", &m_fileCacheManagerDebugFilter, 0, VF_CHEAT | VF_CHEAT_NOCHECK | VF_BITFIELD,
	               "Allows for filtered display of the different AFCM entries such as Globals, Level Specifics, Game Hints and so on.\n"
	               "Usage: s_FileCacheManagerDebugFilter [0ab...] (flags can be combined)\n"
	               "Default: 0 (all)\n"
	               "a: Globals\n"
	               "b: Level Specifics\n"
	               "c: Game Hints\n");

	REGISTER_CVAR2("s_HideInactiveAudioObjects", &m_hideInactiveAudioObjects, 1, VF_DEV_ONLY,
	               "When drawing audio object names on the screen this cvar can be used to choose between all registered audio objects or only those that reference active audio triggers.\n"
	               "Usage: s_HideInactiveAudioObjects [0/1]\n"
	               "Default: 1 (active only)\n");

	REGISTER_CVAR2("s_AudioObjectsRayType", &m_audioObjectsRayType, m_audioObjectsRayType, VF_DEV_ONLY,
	               "Can override AudioObjects' obstruction/occlusion ray type on a global scale.\n"
	               "If set it determines whether AudioObjects use no, adaptive, low, medium or high granularity for rays.\n"
	               "This is a performance type cvar and can be used to turn audio ray casting globally off\n"
	               "or force it on every AudioObject to a given mode.\n"
	               "0: AudioObject specific ray casting.\n"
	               "1: All AudioObjects ignore ray casting.\n"
	               "2: All AudioObjects use adaptive ray casting.\n"
	               "3: All AudioObjects use low ray casting.\n"
	               "4: All AudioObjects use medium ray casting.\n"
	               "5: All AudioObjects use high ray casting.\n"
	               "Usage: s_AudioObjectsRayType [0/1/2/3/4/5]\n"
	               "Default PC: 0, XboxOne: 0, PS4: 0, Mac: 0, Linux: 0, iOS: 0, Android: 0\n");

	m_pDebugFilter = REGISTER_STRING("s_DebugFilter", "", 0,
	                                 "Allows for filtered display of audio debug info by a search string.\n"
	                                 "Usage: s_DebugFilter spaceship\n"
	                                 "Default: " " (all)\n");
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CCVars::UnregisterVariables()
{
	IConsole* const pConsole = gEnv->pConsole;

	if (pConsole != nullptr)
	{
		pConsole->UnregisterVariable("s_OcclusionMaxDistance");
		pConsole->UnregisterVariable("s_OcclusionMinDistance");
		pConsole->UnregisterVariable("s_OcclusionMaxSyncDistance");
		pConsole->UnregisterVariable("s_OcclusionHighDistance");
		pConsole->UnregisterVariable("s_OcclusionMediumDistance");
		pConsole->UnregisterVariable("s_FullObstructionMaxDistance");
		pConsole->UnregisterVariable("s_PositionUpdateThresholdMultiplier");
		pConsole->UnregisterVariable("s_VelocityTrackingThreshold");
		pConsole->UnregisterVariable("s_OcclusionRayLengthOffset");
		pConsole->UnregisterVariable("s_ListenerOcclusionPlaneSize");
		pConsole->UnregisterVariable("s_FileCacheManagerSize");
		pConsole->UnregisterVariable("s_AudioObjectPoolSize");
		pConsole->UnregisterVariable("s_AudioEventPoolSize");
		pConsole->UnregisterVariable("s_AudioStandaloneFilePoolSize");
		pConsole->UnregisterVariable("s_AccumulateOcclusion");
		pConsole->UnregisterVariable("s_ExecuteTrigger");
		pConsole->UnregisterVariable("s_StopTrigger");
		pConsole->UnregisterVariable("s_SetParameter");
		pConsole->UnregisterVariable("s_SetSwitchState");
		pConsole->UnregisterVariable("s_DefaultStandaloneFilesAudioTrigger");

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		pConsole->UnregisterVariable("s_DebugDistance");
		pConsole->UnregisterVariable("s_IgnoreWindowFocus");
		pConsole->UnregisterVariable("s_DrawAudioDebug");
		pConsole->UnregisterVariable("s_FileCacheManagerDebugFilter");
		pConsole->UnregisterVariable("s_HideInactiveAudioObjects");
		pConsole->UnregisterVariable("s_AudioObjectsRayType");
		pConsole->UnregisterVariable("s_DebugFilter");
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
void CCVars::CmdExecuteTrigger(IConsoleCmdArgs* pCmdArgs)
{
	int const numArgs = pCmdArgs->GetArgCount();

	if ((numArgs == 2) || (numArgs == 3))
	{
		ControlId const triggerId = CryAudio::StringToId(pCmdArgs->GetArg(1));
		gEnv->pAudioSystem->ExecuteTrigger(triggerId);
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Usage: s_ExecuteTrigger [TriggerName]");
	}
}

//////////////////////////////////////////////////////////////////////////
void CCVars::CmdStopTrigger(IConsoleCmdArgs* pCmdArgs)
{
	int const numArgs = pCmdArgs->GetArgCount();

	if ((numArgs == 2) || (numArgs == 3))
	{
		ControlId const triggerId = CryAudio::StringToId(pCmdArgs->GetArg(1));
		gEnv->pAudioSystem->StopTrigger(triggerId);
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Usage: s_StopTrigger [TriggerName]");
	}
}

//////////////////////////////////////////////////////////////////////////
void CCVars::CmdSetParameter(IConsoleCmdArgs* pCmdArgs)
{
	int const numArgs = pCmdArgs->GetArgCount();

	if ((numArgs == 3) || (numArgs == 4))
	{
		ControlId const parameterId = CryAudio::StringToId(pCmdArgs->GetArg(1));
		double const value = atof(pCmdArgs->GetArg(2));
		gEnv->pAudioSystem->SetParameter(parameterId, static_cast<float>(value));
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Usage: s_SetParameter [ParameterName] [ParameterValue]");
	}
}

//////////////////////////////////////////////////////////////////////////
void CCVars::CmdSetSwitchState(IConsoleCmdArgs* pCmdArgs)
{
	int const numArgs = pCmdArgs->GetArgCount();

	if ((numArgs == 3) || (numArgs == 4))
	{
		ControlId const switchId = CryAudio::StringToId(pCmdArgs->GetArg(1));
		SwitchStateId const switchStateId = CryAudio::StringToId(pCmdArgs->GetArg(2));
		gEnv->pAudioSystem->SetSwitchState(switchId, switchStateId);
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Usage: s_SetSwitchState [SwitchName] [SwitchStateName]");
	}
}
} // namespace CryAudio

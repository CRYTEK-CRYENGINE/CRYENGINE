// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SoundCVars.h"
#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>

//////////////////////////////////////////////////////////////////////////
void OnCVarChangedAudioSystemCallback(ICVar* pCVar)
{
	if (gEnv->pAudioSystem != nullptr)
	{
		gEnv->pAudioSystem->OnCVarChanged(pCVar);
	}
}

//////////////////////////////////////////////////////////////////////////
CAudioCVars::CAudioCVars()
	: m_audioPrimaryPoolSize(0)
	, m_fileCacheManagerSize(0)
	, m_audioObjectPoolSize(0)
	, m_nAudioEventPoolSize(0)
	, m_audioStandaloneFilePoolSize(0)
	, m_audioProxiesInitType(0)
	, m_occlusionMaxDistance(0.0f)
	, m_occlusionMaxSyncDistance(0.0f)
	, m_fullObstructionMaxDistance(0.0f)
	, m_positionUpdateThreshold(0.0f)
	, m_velocityTrackingThreshold(0.0f)
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	, m_ignoreWindowFocus(0)
	, m_drawAudioDebug(0)
	, m_fileCacheManagerDebugFilter(0)
	, m_audioLoggingOptions(0)
	, m_showActiveAudioObjectsOnly(0)
	, m_audioObjectsRayType(0)
	, m_pAudioTriggersDebugFilter(nullptr)
	, m_pAudioObjectsDebugFilter(nullptr)
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
{
}

//////////////////////////////////////////////////////////////////////////
CAudioCVars::~CAudioCVars()
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioCVars::RegisterVariables()
{
#if CRY_PLATFORM_WINDOWS
	m_audioPrimaryPoolSize = 16 << 10;       // 16 MiB on PC
	m_fileCacheManagerSize = 384 << 10;      // 384 MiB on PC
	m_audioObjectPoolSize = 256;
	m_nAudioEventPoolSize = 256;
	m_audioStandaloneFilePoolSize = 0;
	m_audioProxiesInitType = 0;
	m_occlusionMaxDistance = 500.0f;
	m_occlusionMaxSyncDistance = 10.0f;
	m_fullObstructionMaxDistance = 5.0f;
	m_positionUpdateThreshold = 0.1f;
	m_velocityTrackingThreshold = 0.1f;
#elif CRY_PLATFORM_DURANGO
	m_audioPrimaryPoolSize = 16 << 10;       // 16 MiB on XboxOne
	m_fileCacheManagerSize = 384 << 10;      // 384 MiB on XboxOne
	m_audioObjectPoolSize = 256;
	m_nAudioEventPoolSize = 256;
	m_audioStandaloneFilePoolSize = 0;
	m_audioProxiesInitType = 0;
	m_occlusionMaxDistance = 500.0f;
	m_occlusionMaxSyncDistance = 10.0f;
	m_fullObstructionMaxDistance = 5.0f;
	m_positionUpdateThreshold = 0.1f;
	m_velocityTrackingThreshold = 0.1f;
#elif CRY_PLATFORM_ORBIS
	m_audioPrimaryPoolSize = 16 << 10;       // 16 MiB on PS4
	m_fileCacheManagerSize = 384 << 10;      // 384 MiB on PS4
	m_audioObjectPoolSize = 256;
	m_nAudioEventPoolSize = 256;
	m_audioStandaloneFilePoolSize = 0;
	m_audioProxiesInitType = 0;
	m_occlusionMaxDistance = 500.0f;
	m_occlusionMaxSyncDistance = 10.0f;
	m_fullObstructionMaxDistance = 5.0f;
	m_positionUpdateThreshold = 0.1f;
	m_velocityTrackingThreshold = 0.1f;
#elif CRY_PLATFORM_MAC
	m_audioPrimaryPoolSize = 16 << 10;       // 16 MiB on Mac
	m_fileCacheManagerSize = 384 << 10;      // 384 MiB on Mac
	m_audioObjectPoolSize = 256;
	m_nAudioEventPoolSize = 256;
	m_audioStandaloneFilePoolSize = 0;
	m_audioProxiesInitType = 0;
	m_occlusionMaxDistance = 500.0f;
	m_occlusionMaxSyncDistance = 10.0f;
	m_fullObstructionMaxDistance = 5.0f;
	m_positionUpdateThreshold = 0.1f;
	m_velocityTrackingThreshold = 0.1f;
#elif CRY_PLATFORM_LINUX
	m_audioPrimaryPoolSize = 16 << 10;       // 16 MiB on Linux
	m_fileCacheManagerSize = 384 << 10;      // 384 MiB on Linux
	m_audioObjectPoolSize = 256;
	m_nAudioEventPoolSize = 256;
	m_audioStandaloneFilePoolSize = 0;
	m_audioProxiesInitType = 0;
	m_occlusionMaxDistance = 500.0f;
	m_occlusionMaxSyncDistance = 10.0f;
	m_fullObstructionMaxDistance = 5.0f;
	m_positionUpdateThreshold = 0.1f;
	m_velocityTrackingThreshold = 0.1f;
#elif CRY_PLATFORM_IOS
	m_audioPrimaryPoolSize = 16 << 10;       // 16 MiB on iOS
	m_fileCacheManagerSize = 384 << 10;      // 384 MiB on iOS
	m_audioObjectPoolSize = 256;
	m_nAudioEventPoolSize = 256;
	m_audioStandaloneFilePoolSize = 0;
	m_audioProxiesInitType = 0;
	m_occlusionMaxDistance = 500.0f;
	m_occlusionMaxSyncDistance = 10.0f;
	m_fullObstructionMaxDistance = 5.0f;
	m_positionUpdateThreshold = 0.1f;
	m_velocityTrackingThreshold = 0.1f;
#elif CRY_PLATFORM_ANDROID
	m_audioPrimaryPoolSize = 4 << 10;       // 4 MiB
	m_fileCacheManagerSize = 72 << 10;      // 72 MiB
	m_audioObjectPoolSize = 256;
	m_nAudioEventPoolSize = 256;
	m_audioStandaloneFilePoolSize = 0;
	m_audioProxiesInitType = 0;
	m_occlusionMaxDistance = 500.0f;
	m_occlusionMaxSyncDistance = 10.0f;
	m_fullObstructionMaxDistance = 5.0f;
	m_positionUpdateThreshold = 0.1f;
	m_velocityTrackingThreshold = 0.1f;
#else
	#error "Undefined platform."
#endif

	REGISTER_CVAR2("s_AudioPrimaryPoolSize", &m_audioPrimaryPoolSize, m_audioPrimaryPoolSize, VF_REQUIRE_APP_RESTART,
	               "Specifies the size (in KiB) of the memory pool to be used by the ATL.\n"
	               "Usage: s_AudioPrimaryPoolSize [0/...]\n"
	               "Default PC: 8192 (8 MiB), XboxOne: 8192 (8 MiB), PS4: 8192 (8 MiB), Mac: 8192 (8 MiB), Linux: 8192 (8 MiB), iOS: 8192 (8 MiB), Android: 4096 (4 MiB)\n");

	REGISTER_CVAR2("s_OcclusionMaxDistance", &m_occlusionMaxDistance, m_occlusionMaxDistance, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Obstruction/Occlusion is not calculated for the sounds, whose distance to the listener is greater than this value. Setting this value to 0 disables obstruction/occlusion calculations.\n"
	               "Usage: s_OcclusionMaxDistance [0/...]\n"
	               "Default: 500 m\n");

	REGISTER_CVAR2("s_OcclusionMaxSyncDistance", &m_occlusionMaxSyncDistance, m_occlusionMaxSyncDistance, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Physics rays are processed synchronously for the sounds that are closer to the listener than this value, and asynchronously for the rest (possible performance optimization).\n"
	               "Usage: s_OcclusionMaxSyncDistance [0/...]\n"
	               "Default: 10 m\n");

	REGISTER_CVAR2("s_FullObstructionMaxDistance", &m_fullObstructionMaxDistance, m_fullObstructionMaxDistance, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "for the sounds, whose distance to the listener is greater than this value, the obstruction is value gets attenuated with distance.\n"
	               "Usage: s_FullObstructionMaxDistance [0/...]\n"
	               "Default: 5 m\n");

	REGISTER_CVAR2("s_PositionUpdateThreshold", &m_positionUpdateThreshold, m_positionUpdateThreshold, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "An audio object has to move by at least this amount to issue a position update request to the audio system.\n"
	               "This kind of optimization should ideally be done by the parent system so this is here for convenience."
	               "Usage: s_PositionUpdateThreshold [0/...]\n"
	               "Default: 0.1 (10 cm)\n");

	REGISTER_CVAR2("s_VelocityTrackingThreshold", &m_velocityTrackingThreshold, m_velocityTrackingThreshold, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "An audio object has to change its velocity by at least this amount to issue an \"object_speed\" RTPC update request to the audio system.\n"
	               "Usage: s_VelocityTrackingThreshold [0/...]\n"
	               "Default: 0.1 (10 cm/s)\n");

	REGISTER_CVAR2("s_FileCacheManagerSize", &m_fileCacheManagerSize, m_fileCacheManagerSize, VF_REQUIRE_APP_RESTART,
	               "Sets the size in KiB the AFCM will allocate on the heap.\n"
	               "Usage: s_FileCacheManagerSize [0/...]\n"
	               "Default PC: 393216 (384 MiB), XboxOne: 393216 (384 MiB), PS4: 393216 (384 MiB), Mac: 393216 (384 MiB), Linux: 393216 (384 MiB), iOS: 2048 (2 MiB), Android: 73728 (72 MiB)\n");

	REGISTER_CVAR2("s_AudioObjectPoolSize", &m_audioObjectPoolSize, m_audioObjectPoolSize, VF_REQUIRE_APP_RESTART,
	               "Sets the number of preallocated audio objects and corresponding audio proxies.\n"
	               "Usage: s_AudioObjectPoolSize [0/...]\n"
	               "Default PC: 256, XboxOne: 256, PS4: 256, Mac: 256, Linux: 256, iOS: 256, Android: 256\n");

	REGISTER_CVAR2("s_AudioEventPoolSize", &m_nAudioEventPoolSize, m_nAudioEventPoolSize, VF_REQUIRE_APP_RESTART,
	               "Sets the number of preallocated audio events.\n"
	               "Usage: s_AudioEventPoolSize [0/...]\n"
	               "Default PC: 256, XboxOne: 256, PS4: 256, Mac: 256, Linux: 256, iOS: 256, Android: 256\n");

	REGISTER_CVAR2("s_AudioStandaloneFilePoolSize", &m_audioStandaloneFilePoolSize, m_audioStandaloneFilePoolSize, VF_REQUIRE_APP_RESTART,
	               "Sets the number of preallocated audio standalone files.\n"
	               "Usage: s_AudioStandaloneFilePoolSize [0/...]\n"
	               "Default PC: 0, XboxOne: 0, PS4: 0, Mac: 0, Linux: 0, iOS: 0, Android: 0\n");

	REGISTER_CVAR2("s_AudioProxiesInitType", &m_audioProxiesInitType, m_audioProxiesInitType, VF_NULL,
	               "Can override AudioProxies' init type on a global scale.\n"
	               "If set it determines whether AudioProxies initialize synchronously or asynchronously.\n"
	               "This is a performance type cvar as asynchronously initializing AudioProxies\n"
	               "will have a greatly reduced impact on the calling thread.\n"
	               "Be aware though that when set to initialize asynchronously that audio will play back delayed.\n"
	               "By how much will greatly depend on the audio thread's work load.\n"
	               "0: AudioProxy specific initialization.\n"
	               "1: All AudioProxies initialize synchronously.\n"
	               "2: All AudioProxies initialize asynchronously.\n"
	               "Usage: s_AudioProxiesInitType [0/1/2]\n"
	               "Default PC: 0, XboxOne: 0, PS4: 0, Mac: 0, Linux: 0, iOS: 0, Android: 0\n");

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

	REGISTER_COMMAND("s_SetRtpc", CmdSetRtpc, VF_CHEAT,
	                 "Set an Audio RTPC value.\n"
	                 "The first argument is the name of the AudioRtpc to be set, the second argument is the float value to be set,"
	                 "the third argument is an optional AudioObject ID.\n"
	                 "If the third argument is provided, the AudioRtpc is set on the AudioObject with the given ID,\n"
	                 "otherwise, the AudioRtpc is set on the GlobalAudioObject\n"
	                 "Usage: s_SetRtpc character_speed  0.0  601 or s_SetRtpc volume_music 1.0\n");

	REGISTER_COMMAND("s_SetSwitchState", CmdSetSwitchState, VF_CHEAT,
	                 "Set an Audio Switch to a provided State.\n"
	                 "The first argument is the name of the AudioSwitch to, the second argument is the name of the SwitchState to be set,"
	                 "the third argument is an optional AudioObject ID.\n"
	                 "If the third argument is provided, the AudioSwitch is set on the AudioObject with the given ID,\n"
	                 "otherwise, the AudioSwitch is set on the GlobalAudioObject\n"
	                 "Usage: s_SetSwitchState SurfaceType concrete 601 or s_SetSwitchState weather rain\n");

	REGISTER_STRING("s_DefaultStandaloneFilesAudioTrigger", "do_nothing", 0,
	                "The name of the ATL AudioTrigger which is used for playing back standalone files, when you call 'PlayFile' without specifying\n"
	                "an override audioTriggerId that should be used instead.\n"
	                "Usage: s_DefaultStandaloneFilesAudioTrigger audio_trigger_name.\n"
	                "If you change this CVar to be empty, the control will not be created automatically.\n"
	                "Default: \"do_nothing\" \n");

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	REGISTER_CVAR2("s_IgnoreWindowFocus", &m_ignoreWindowFocus, 0, VF_DEV_ONLY,
	               "If set to 1, the sound system will continue playing when the Editor or Game window loses focus.\n"
	               "Usage: s_IgnoreWindowFocus [0/1]\n"
	               "Default: 0 (off)\n");

	REGISTER_CVAR2("s_DrawAudioDebug", &m_drawAudioDebug, 0, VF_CHEAT | VF_CHEAT_NOCHECK | VF_BITFIELD,
	               "Draws AudioTranslationLayer related debug data to the screen.\n"
	               "Usage: s_DrawAudioDebug [0ab...] (flags can be combined)\n"
	               "0: No audio debug info on the screen.\n"
	               "a: Draw spheres around active audio objects.\n"
	               "b: Show text labels for active audio objects.\n"
	               "c: Show trigger names for active audio objects.\n"
	               "d: Show current states for active audio objects.\n"
	               "e: Show RTPC values for active audio objects.\n"
	               "f: Show Environment amounts for active audio objects.\n"
	               "g: Draw occlusion rays.\n"
	               "h: Show occlusion ray labels.\n"
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

	REGISTER_CVAR2("s_AudioLoggingOptions", &m_audioLoggingOptions, 0, VF_CHEAT | VF_CHEAT_NOCHECK | VF_BITFIELD,
	               "Toggles the logging of audio related messages.\n"
	               "Usage: s_AudioLoggingOptions [0ab...] (flags can be combined)\n"
	               "Default: 0 (none)\n"
	               "a: Errors\n"
	               "b: Warnings\n"
	               "c: Comments\n");

	REGISTER_CVAR2("s_ShowActiveAudioObjectsOnly", &m_showActiveAudioObjectsOnly, 1, VF_DEV_ONLY,
	               "When drawing audio object names on the screen this cvar can be used to choose between all registered audio objects or only those that reference active audio triggers.\n"
	               "Usage: s_ShowActiveAudioObjectsOnly [0/1]\n"
	               "Default: 1 (active only)\n");

	REGISTER_CVAR2("s_AudioObjectsRayType", &m_audioObjectsRayType, m_audioObjectsRayType, VF_DEV_ONLY,
	               "Can override AudioObjects' obstruction/occlusion ray type on a global scale.\n"
	               "If set it determines whether AudioObjects use no, single or multi rays.\n"
	               "This is a performance type cvar and can be used to turn audio ray casting globally off\n"
	               "or force it on every AudioObject to a given mode.\n"
	               "0: AudioObject specific ray casting.\n"
	               "1: All AudioObjects ignore ray casting.\n"
	               "2: All AudioObjects use single ray casting.\n"
	               "3: All AudioObjects use multi ray casting.\n"
	               "Usage: s_AudioObjectsRayType [0/1/2/3]\n"
	               "Default PC: 0, XboxOne: 0, PS4: 0, Mac: 0, Linux: 0, iOS: 0, Android: 0\n");

	m_pAudioTriggersDebugFilter = REGISTER_STRING("s_AudioTriggersDebugFilter", "", 0,
	                                              "Allows for filtered display of audio triggers by a search string.\n"
	                                              "Usage: s_AudioTriggersDebugFilter laser\n"
	                                              "Default: " " (all)\n");

	m_pAudioObjectsDebugFilter = REGISTER_STRING("s_AudioObjectsDebugFilter", "", 0,
	                                             "Allows for filtered display of audio objects by a search string.\n"
	                                             "Usage: s_AudioObjectsDebugFilter spaceship.\n"
	                                             "Default: " " (all)\n");

#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CAudioCVars::UnregisterVariables()
{
	IConsole* const pConsole = gEnv->pConsole;

	if (pConsole != nullptr)
	{
		pConsole->UnregisterVariable("s_AudioPrimaryPoolSize");
		pConsole->UnregisterVariable("s_OcclusionMaxDistance");
		pConsole->UnregisterVariable("s_OcclusionMaxSyncDistance");
		pConsole->UnregisterVariable("s_FullObstructionMaxDistance");
		pConsole->UnregisterVariable("s_PositionUpdateThreshold");
		pConsole->UnregisterVariable("s_VelocityTrackingThreshold");
		pConsole->UnregisterVariable("s_FileCacheManagerSize");
		pConsole->UnregisterVariable("s_AudioObjectPoolSize");
		pConsole->UnregisterVariable("s_AudioEventPoolSize");
		pConsole->UnregisterVariable("s_AudioStandaloneFilePoolSize");
		pConsole->UnregisterVariable("s_AudioProxiesInitType");
		pConsole->UnregisterVariable("s_ExecuteTrigger");
		pConsole->UnregisterVariable("s_StopTrigger");
		pConsole->UnregisterVariable("s_SetRtpc");
		pConsole->UnregisterVariable("s_SetSwitchState");

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		pConsole->UnregisterVariable("s_IgnoreWindowFocus");
		pConsole->UnregisterVariable("s_DrawAudioDebug");
		pConsole->UnregisterVariable("s_FileCacheManagerDebugFilter");
		pConsole->UnregisterVariable("s_AudioLoggingOptions");
		pConsole->UnregisterVariable("s_ShowActiveAudioObjectsOnly");
		pConsole->UnregisterVariable("s_AudioObjectsRayType");
		pConsole->UnregisterVariable("s_AudioTriggersDebugFilter");
		pConsole->UnregisterVariable("s_AudioObjectsDebugFilter");
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioCVars::CmdExecuteTrigger(IConsoleCmdArgs* pCmdArgs)
{
	AudioControlId audioTriggerId = INVALID_AUDIO_CONTROL_ID;
	AudioObjectId audioObjectId = INVALID_AUDIO_OBJECT_ID;

	int const numArgs = pCmdArgs->GetArgCount();

	if ((numArgs == 2) || (numArgs == 3))
	{
		gEnv->pAudioSystem->GetAudioTriggerId(pCmdArgs->GetArg(1), audioTriggerId);

		if (numArgs == 3)
		{
			int const tempId = atoi(pCmdArgs->GetArg(2));

			if (tempId > 0)
			{
				audioObjectId = static_cast<AudioObjectId>(tempId);
			}
			else
			{
				g_audioLogger.Log(eAudioLogType_Error, "Invalid Object Id: %s", pCmdArgs->GetArg(2));
				return;
			}
		}

		if (audioTriggerId == INVALID_AUDIO_CONTROL_ID)
		{
			g_audioLogger.Log(eAudioLogType_Error, "Unknown trigger name: %s", pCmdArgs->GetArg(1));
		}
		else
		{
			SAudioRequest request;
			SAudioObjectRequestData<eAudioObjectRequestType_ExecuteTrigger> requestData(audioTriggerId, 0.0f);

			request.audioObjectId = audioObjectId;
			request.flags = eAudioRequestFlags_PriorityNormal;
			request.pData = &requestData;

			gEnv->pAudioSystem->PushRequest(request);
		}
	}
	else
	{
		g_audioLogger.Log(eAudioLogType_Error, "Usage: s_ExecuteTrigger [TriggerName] [[Optional Object ID]]");
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioCVars::CmdStopTrigger(IConsoleCmdArgs* pCmdArgs)
{
	AudioControlId audioTriggerId = INVALID_AUDIO_CONTROL_ID;
	AudioObjectId audioObjectId = INVALID_AUDIO_OBJECT_ID;

	int const numArgs = pCmdArgs->GetArgCount();

	if ((numArgs == 2) || (numArgs == 3))
	{
		gEnv->pAudioSystem->GetAudioTriggerId(pCmdArgs->GetArg(1), audioTriggerId);

		if (numArgs == 3)
		{
			int const tempId = atoi(pCmdArgs->GetArg(2));

			if (tempId > 0)
			{
				audioObjectId = static_cast<AudioObjectId>(tempId);
			}
			else
			{
				g_audioLogger.Log(eAudioLogType_Error, "Invalid Object Id: %s", pCmdArgs->GetArg(2));
				return;
			}
		}

		if (audioTriggerId == INVALID_AUDIO_CONTROL_ID)
		{
			g_audioLogger.Log(eAudioLogType_Error, "Unknown trigger name: %s", pCmdArgs->GetArg(1));
		}
		else
		{
			SAudioRequest request;
			SAudioObjectRequestData<eAudioObjectRequestType_StopTrigger> requestData(audioTriggerId);

			request.audioObjectId = audioObjectId;
			request.flags = eAudioRequestFlags_PriorityNormal;
			request.pData = &requestData;

			gEnv->pAudioSystem->PushRequest(request);
		}
	}
	else
	{
		g_audioLogger.Log(eAudioLogType_Error, "Usage: s_StopTrigger [TriggerName] [[Optional Object ID]]");
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioCVars::CmdSetRtpc(IConsoleCmdArgs* pCmdArgs)
{
	AudioControlId audioRtpcId = INVALID_AUDIO_CONTROL_ID;
	AudioObjectId audioObjectId = INVALID_AUDIO_OBJECT_ID;

	int const numArgs = pCmdArgs->GetArgCount();

	if ((numArgs == 3) || (numArgs == 4))
	{
		gEnv->pAudioSystem->GetAudioRtpcId(pCmdArgs->GetArg(1), audioRtpcId);

		double const value = atof(pCmdArgs->GetArg(2));

		if (numArgs == 4)
		{
			int const tempId = atoi(pCmdArgs->GetArg(3));

			if (tempId > 0)
			{
				audioObjectId = static_cast<AudioObjectId>(tempId);
			}
			else
			{
				g_audioLogger.Log(eAudioLogType_Error, "Invalid Object Id: %s", pCmdArgs->GetArg(3));
				return;
			}
		}

		if (audioRtpcId == INVALID_AUDIO_CONTROL_ID)
		{
			g_audioLogger.Log(eAudioLogType_Error, "Unknown Rtpc name: %s", pCmdArgs->GetArg(1));
		}
		else
		{
			SAudioRequest request;
			SAudioObjectRequestData<eAudioObjectRequestType_SetRtpcValue> requestData(audioRtpcId, static_cast<float>(value));

			request.audioObjectId = audioObjectId;
			request.flags = eAudioRequestFlags_PriorityNormal;
			request.pData = &requestData;

			gEnv->pAudioSystem->PushRequest(request);
		}
	}
	else
	{
		g_audioLogger.Log(eAudioLogType_Error, "Usage: s_SetRtpc [RtpcName] [RtpcValue] [[Optional Object ID]]");
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioCVars::CmdSetSwitchState(IConsoleCmdArgs* pCmdArgs)
{
	AudioControlId audioSwitchId = INVALID_AUDIO_CONTROL_ID;
	AudioObjectId audioObjectId = INVALID_AUDIO_OBJECT_ID;
	AudioSwitchStateId audioSwitchStateId = INVALID_AUDIO_SWITCH_STATE_ID;

	int const numArgs = pCmdArgs->GetArgCount();

	if ((numArgs == 3) || (numArgs == 4))
	{
		gEnv->pAudioSystem->GetAudioSwitchId(pCmdArgs->GetArg(1), audioSwitchId);

		if (audioSwitchId != INVALID_AUDIO_CONTROL_ID)
		{
			gEnv->pAudioSystem->GetAudioSwitchStateId(audioSwitchId, pCmdArgs->GetArg(2), audioSwitchStateId);

			if (numArgs == 4)
			{
				int const tempId = atoi(pCmdArgs->GetArg(3));

				if (tempId > 0)
				{
					audioObjectId = static_cast<AudioObjectId>(tempId);
				}
				else
				{
					g_audioLogger.Log(eAudioLogType_Error, "Invalid Object Id: %s", pCmdArgs->GetArg(3));
					return;
				}
			}

			if (audioSwitchStateId != INVALID_AUDIO_SWITCH_STATE_ID)
			{
				SAudioRequest request;
				SAudioObjectRequestData<eAudioObjectRequestType_SetSwitchState> requestData(audioSwitchId, audioSwitchStateId);

				request.audioObjectId = audioObjectId;
				request.flags = eAudioRequestFlags_PriorityNormal;
				request.pData = &requestData;

				gEnv->pAudioSystem->PushRequest(request);
			}
			else
			{
				g_audioLogger.Log(eAudioLogType_Error, "Invalid  Switch State name: %s", pCmdArgs->GetArg(2));
			}
		}
		else
		{
			g_audioLogger.Log(eAudioLogType_Error, "Unknown Switch name: %s", pCmdArgs->GetArg(1));
		}
	}
	else
	{
		g_audioLogger.Log(eAudioLogType_Error, "Usage: s_SetSwitchState [SwitchName] [SwitchStateName] [[Optional Object Id]]");
	}
}

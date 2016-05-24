// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioImpl_wwise.h"
#include "AudioImplCVars_wwise.h"
#include "FileIOHandler_wwise.h"
#include "Common_wwise.h"
#include <SharedAudioData.h>

#include <AK/SoundEngine/Common/AkSoundEngine.h>     // Sound engine
#include <AK/MusicEngine/Common/AkMusicEngine.h>     // Music Engine
#include <AK/SoundEngine/Common/AkMemoryMgr.h>       // Memory Manager
#include <AK/SoundEngine/Common/AkModule.h>          // Default memory and stream managers
#include <AK/SoundEngine/Common/AkStreamMgrModule.h> // AkStreamMgrModule
#include <AK/SoundEngine/Common/AkQueryParameters.h>
#include <AK/SoundEngine/Common/AkCallback.h>

#include <AK/Plugin/AllPluginsRegistrationHelpers.h>

#if defined(WWISE_USE_OCULUS)
	#include <AK/Plugin/OculusSpatializer.h>
	#include <CryCore/Platform/CryLibrary.h>
#endif // WWISE_USE_OCULUS

#include <CrySystem/File/ICryPak.h>
#include <CryThreading/IThreadManager.h>
#include <CryThreading/IThreadConfigManager.h>

#if !defined(WWISE_FOR_RELEASE)
	#include <AK/Comm/AkCommunication.h> // Communication between Wwise and the game (excluded in release build)
	#include <AK/Tools/Common/AkMonitorError.h>
	#include <AK/Tools/Common/AkPlatformFuncs.h>
#endif // WWISE_FOR_RELEASE

using namespace CryAudio::Impl;

char const* const CAudioImpl_wwise::s_szWwiseEventTag = "WwiseEvent";
char const* const CAudioImpl_wwise::s_szWwiseRtpcTag = "WwiseRtpc";
char const* const CAudioImpl_wwise::s_szWwiseSwitchTag = "WwiseSwitch";
char const* const CAudioImpl_wwise::s_szWwiseStateTag = "WwiseState";
char const* const CAudioImpl_wwise::s_szWwiseRtpcSwitchTag = "WwiseRtpc";
char const* const CAudioImpl_wwise::s_szWwiseFileTag = "WwiseFile";
char const* const CAudioImpl_wwise::s_szWwiseAuxBusTag = "WwiseAuxBus";
char const* const CAudioImpl_wwise::s_szWwiseValueTag = "WwiseValue";
char const* const CAudioImpl_wwise::s_szWwiseNameAttribute = "wwise_name";
char const* const CAudioImpl_wwise::s_szWwiseValueAttribute = "wwise_value";
char const* const CAudioImpl_wwise::s_szWwiseMutiplierAttribute = "wwise_value_multiplier";
char const* const CAudioImpl_wwise::s_szWwiseShiftAttribute = "wwise_value_shift";
char const* const CAudioImpl_wwise::s_szWwiseLocalisedAttribute = "wwise_localised";
char const* const CAudioImpl_wwise::s_szSoundBanksInfoFilename = "SoundbanksInfo.xml";

/////////////////////////////////////////////////////////////////////////////////
//                              MEMORY HOOKS SETUP
//
//                             ##### IMPORTANT #####
//
// These custom alloc/free functions are declared as "extern" in AkMemoryMgr.h
// and MUST be defined by the game developer.
/////////////////////////////////////////////////////////////////////////////////

namespace AK
{
void* AllocHook(size_t in_size)
{
	return g_AudioImplMemoryPool_wwise.Allocate<void*>(in_size, AUDIO_MEMORY_ALIGNMENT, "AudioWwise");
}

void FreeHook(void* in_ptr)
{
	g_AudioImplMemoryPool_wwise.Free(in_ptr);
}

void* VirtualAllocHook(void* in_pMemAddress, size_t in_size, DWORD in_dwAllocationType, DWORD in_dwProtect)
{
	return g_AudioImplMemoryPool_wwise.Allocate<void*>(in_size, AUDIO_MEMORY_ALIGNMENT, "AudioWwise");
	//return VirtualAlloc(in_pMemAddress, in_size, in_dwAllocationType, in_dwProtect);
}

void VirtualFreeHook(void* in_pMemAddress, size_t in_size, DWORD in_dwFreeType)
{
	//VirtualFree(in_pMemAddress, in_size, in_dwFreeType);
	g_AudioImplMemoryPool_wwise.Free(in_pMemAddress);
}

#if CRY_PLATFORM_DURANGO
void* APUAllocHook(size_t in_size, unsigned int in_alignment)
{
	void* pAlloc = nullptr;

	#if defined(PROVIDE_WWISE_IMPL_SECONDARY_POOL)
	size_t const nSecondSize = g_AudioImplMemoryPoolSecondary_wwise.MemSize();

	if (nSecondSize > 0)
	{
		size_t const nAllocHandle = g_AudioImplMemoryPoolSecondary_wwise.Allocate<size_t>(in_size, in_alignment);

		if (nAllocHandle > 0)
		{
			pAlloc = g_AudioImplMemoryPoolSecondary_wwise.Resolve<void*>(nAllocHandle);
			g_AudioImplMemoryPoolSecondary_wwise.Item(nAllocHandle)->Lock();
		}
	}
	#endif // PROVIDE_AUDIO_IMPL_SECONDARY_POOL

	CRY_ASSERT(pAlloc != nullptr);
	return pAlloc;
}

void APUFreeHook(void* in_pMemAddress)
{
	#if defined(PROVIDE_WWISE_IMPL_SECONDARY_POOL)
	size_t const nAllocHandle = g_AudioImplMemoryPoolSecondary_wwise.AddressToHandle(in_pMemAddress);
	//size_t const nOldSize = g_MemoryPoolSoundSecondary.Size(nAllocHandle);
	g_AudioImplMemoryPoolSecondary_wwise.Free(nAllocHandle);
	#else
	CryFatalError("%s", "<Audio>: Called APUFreeHook without secondary pool");
	#endif // PROVIDE_AUDIO_IMPL_SECONDARY_POOL
}
#endif // CRY_PLATFORM_DURANGO
}

// AK callbacks
void EndEventCallback(AkCallbackType callbackType, AkCallbackInfo* pCallbackInfo)
{
	if (callbackType == AK_EndOfEvent)
	{
		SAudioEvent_wwise* const pAudioEvent = static_cast<SAudioEvent_wwise* const>(pCallbackInfo->pCookie);

		if (pAudioEvent != nullptr)
		{
			SAudioRequest request;
			SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportFinishedEvent> requestData(pAudioEvent->audioEventId, true);
			request.flags = eAudioRequestFlags_ThreadSafePush;
			request.pData = &requestData;

			gEnv->pAudioSystem->PushRequest(request);
		}
	}
}

void PrepareEventCallback(
  AkUniqueID eventId,
  void const* pBankPtr,
  AKRESULT wwiseResult,
  AkMemPoolId memPoolId,
  void* pCookie)
{
	SAudioEvent_wwise* const pAudioEvent = static_cast<SAudioEvent_wwise* const>(pCookie);

	if (pAudioEvent != nullptr)
	{
		pAudioEvent->id = eventId;

		SAudioRequest request;
		SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportFinishedEvent> requestData(pAudioEvent->audioEventId, wwiseResult == AK_Success);
		request.flags = eAudioRequestFlags_ThreadSafePush;
		request.pData = &requestData;

		gEnv->pAudioSystem->PushRequest(request);
	}
}

#if !defined(WWISE_FOR_RELEASE)
static void ErrorMonitorCallback(
  AK::Monitor::ErrorCode in_eErrorCode,   ///< Error code number value
  const AkOSChar* in_pszError,            ///< Message or error string to be displayed
  AK::Monitor::ErrorLevel in_eErrorLevel, ///< Specifies whether it should be displayed as a message or an error
  AkPlayingID in_playingID,               ///< Related Playing ID if applicable, AK_INVALID_PLAYING_ID otherwise
  AkGameObjectID in_gameObjID             ///< Related Game Object ID if applicable, AK_INVALID_GAME_OBJECT otherwise
  )
{
	char* sTemp = nullptr;
	CONVERT_OSCHAR_TO_CHAR(in_pszError, sTemp);
	g_AudioImplLogger_wwise.Log(
	  ((in_eErrorLevel& AK::Monitor::ErrorLevel_Error) != 0) ? eAudioLogType_Error : eAudioLogType_Comment,
	  "<Wwise> %s ErrorCode: %d PlayingID: %u GameObjID: %" PRISIZE_T, sTemp, in_eErrorCode, in_playingID, in_gameObjID);
}
#endif // WWISE_FOR_RELEASE

///////////////////////////////////////////////////////////////////////////
CAudioImpl_wwise::CAudioImpl_wwise()
	: m_dummyGameObjectId(static_cast<AkGameObjectID>(-2))
	, m_initBankId(AK_INVALID_BANK_ID)
#if !defined(WWISE_FOR_RELEASE)
	, m_bCommSystemInitialized(false)
#endif // !WWISE_FOR_RELEASE
#if defined(WWISE_USE_OCULUS)
	, m_pOculusSpatializerLibrary(nullptr)
#endif // WWISE_USE_OCULUS
{
	string sGameFolder = PathUtil::GetGameFolder().c_str();

	if (sGameFolder.empty())
	{
		CryFatalError("<Audio>: Needs a valid game folder to proceed!");
	}

	m_regularSoundBankFolder = sGameFolder.c_str();
	m_regularSoundBankFolder += CRY_NATIVE_PATH_SEPSTR WWISE_IMPL_DATA_ROOT;

#if defined(INCLUDE_WWISE_IMPL_PRODUCTION_CODE)
	m_fullImplString.Format("%s (Build: %d) (%s%s)", WWISE_IMPL_INFO_STRING, AK_WWISESDK_VERSION_BUILD, sGameFolder.c_str(), CRY_NATIVE_PATH_SEPSTR WWISE_IMPL_DATA_ROOT);
#endif // INCLUDE_WWISE_IMPL_PRODUCTION_CODE
}

///////////////////////////////////////////////////////////////////////////
CAudioImpl_wwise::~CAudioImpl_wwise()
{
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::Update(float const deltaTime)
{
	if (AK::SoundEngine::IsInitialized())
	{
#if defined(INCLUDE_WWISE_IMPL_PRODUCTION_CODE)
		AKRESULT wwiseResult = AK_Fail;
		static int nEnableOutputCapture = 0;

		if (g_AudioImplCVars_wwise.m_nEnableOutputCapture == 1 && nEnableOutputCapture == 0)
		{
			AkOSChar const* pOutputCaptureFileName = nullptr;
			CONVERT_CHAR_TO_OSCHAR("../wwise_audio_capture.wav", pOutputCaptureFileName);
			wwiseResult = AK::SoundEngine::StartOutputCapture(pOutputCaptureFileName);
			AKASSERT((wwiseResult == AK_Success) || !"StartOutputCapture failed!");
			nEnableOutputCapture = g_AudioImplCVars_wwise.m_nEnableOutputCapture;
		}
		else if (g_AudioImplCVars_wwise.m_nEnableOutputCapture == 0 && nEnableOutputCapture == 1)
		{
			wwiseResult = AK::SoundEngine::StopOutputCapture();
			AKASSERT((wwiseResult == AK_Success) || !"StopOutputCapture failed!");
			nEnableOutputCapture = g_AudioImplCVars_wwise.m_nEnableOutputCapture;
		}
#endif // INCLUDE_WWISE_IMPL_PRODUCTION_CODE

		AK::SoundEngine::RenderAudio();
	}
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::Init()
{
	// If something fails so severely during initialization that we need to fall back to the NULL implementation
	// we will need to shut down what has been initialized so far. Therefore make sure to call Shutdown() before returning eARS_FAILURE!

	AkMemSettings memSettings;
	memSettings.uMaxNumPools = 20;
	AKRESULT wwiseResult = AK::MemoryMgr::Init(&memSettings);

	if (wwiseResult != AK_Success)
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "AK::MemoryMgr::Init() returned AKRESULT %d", wwiseResult);
		ShutDown();

		return eAudioRequestStatus_Failure;
	}

	AkMemPoolId const prepareMemPoolId = AK::MemoryMgr::CreatePool(nullptr, g_AudioImplCVars_wwise.m_nPrepareEventMemoryPoolSize << 10, 16, AkMalloc, 16);

	if (prepareMemPoolId == AK_INVALID_POOL_ID)
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "AK::MemoryMgr::CreatePool() PrepareEventMemoryPool failed!\n");
		ShutDown();

		return eAudioRequestStatus_Failure;
	}

	wwiseResult = AK::MemoryMgr::SetPoolName(prepareMemPoolId, "PrepareEventMemoryPool");

	if (wwiseResult != AK_Success)
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "AK::MemoryMgr::SetPoolName() could not set name of event prepare memory pool!\n");
		ShutDown();

		return eAudioRequestStatus_Failure;
	}

	AkStreamMgrSettings streamSettings;
	AK::StreamMgr::GetDefaultSettings(streamSettings);
	streamSettings.uMemorySize = g_AudioImplCVars_wwise.m_nStreamManagerMemoryPoolSize << 10; // 64 KiB is the default value!

	if (AK::StreamMgr::Create(streamSettings) == nullptr)
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "AK::StreamMgr::Create() failed!\n");
		ShutDown();

		return eAudioRequestStatus_Failure;
	}

	IThreadConfigManager* pThreadConfigMngr = gEnv->pThreadManager->GetThreadConfigManager();
	const SThreadConfig* pDeviceThread = pThreadConfigMngr->GetThreadConfig("Wwise_Device");
	const SThreadConfig* pBankManger = pThreadConfigMngr->GetThreadConfig("Wwise_BankManager");
	const SThreadConfig* pLEngineThread = pThreadConfigMngr->GetThreadConfig("Wwise_LEngine");
	const SThreadConfig* pMonitorThread = pThreadConfigMngr->GetThreadConfig("Wwise_Monitor");

	AkDeviceSettings deviceSettings;
	AK::StreamMgr::GetDefaultDeviceSettings(deviceSettings);
	deviceSettings.uIOMemorySize = g_AudioImplCVars_wwise.m_nStreamDeviceMemoryPoolSize << 10; // 2 MiB is the default value!

	// Device thread settings
	if (pDeviceThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_Affinity)
		deviceSettings.threadProperties.dwAffinityMask = pDeviceThread->affinityFlag;
	if (pDeviceThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_Priority)
#if defined(CRY_PLATFORM_POSIX)
		if (!(pDeviceThread->priority > 0 && pDeviceThread->priority <= 99))
			CryWarning(VALIDATOR_MODULE_AUDIO, VALIDATOR_WARNING, "Thread 'Wwise_Device' requested priority %d when allowed range is 1 to 99. Applying default value of %d.", pDeviceThread->priority, deviceSettings.threadProperties.nPriority);
		else
#endif
		deviceSettings.threadProperties.nPriority = pDeviceThread->priority;

	if (pDeviceThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_StackSize)
		deviceSettings.threadProperties.uStackSize = pDeviceThread->stackSizeBytes;

	wwiseResult = m_fileIOHandler.Init(deviceSettings);

	if (wwiseResult != AK_Success)
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "m_oFileIOHandler.Init() returned AKRESULT %d", wwiseResult);
		ShutDown();

		return eAudioRequestStatus_Failure;
	}

	CryFixedStringT<AK_MAX_PATH> temp(PathUtil::GetGameFolder().c_str());
	temp.append(CRY_NATIVE_PATH_SEPSTR WWISE_IMPL_DATA_ROOT CRY_NATIVE_PATH_SEPSTR);
	AkOSChar const* szTemp = nullptr;
	CONVERT_CHAR_TO_OSCHAR(temp.c_str(), szTemp);
	m_fileIOHandler.SetBankPath(szTemp);

	AkInitSettings initSettings;
	AK::SoundEngine::GetDefaultInitSettings(initSettings);
	initSettings.uDefaultPoolSize = g_AudioImplCVars_wwise.m_nSoundEngineDefaultMemoryPoolSize << 10;
	initSettings.uCommandQueueSize = g_AudioImplCVars_wwise.m_nCommandQueueMemoryPoolSize << 10;
#if defined(INCLUDE_WWISE_IMPL_PRODUCTION_CODE)
	initSettings.uMonitorPoolSize = g_AudioImplCVars_wwise.m_nMonitorMemoryPoolSize << 10;
	initSettings.uMonitorQueuePoolSize = g_AudioImplCVars_wwise.m_nMonitorQueueMemoryPoolSize << 10;
#endif // INCLUDE_WWISE_IMPL_PRODUCTION_CODE
	initSettings.uPrepareEventMemoryPoolID = prepareMemPoolId;
	initSettings.bEnableGameSyncPreparation = false;//TODO: ???

	AkPlatformInitSettings platformInitSettings;
	AK::SoundEngine::GetDefaultPlatformInitSettings(platformInitSettings);
	platformInitSettings.uLEngineDefaultPoolSize = g_AudioImplCVars_wwise.m_nLowerEngineDefaultPoolSize << 10;

	// Bank Manager thread settings

	if (pBankManger->paramActivityFlag & SThreadConfig::eThreadParamFlag_Affinity)
		platformInitSettings.threadBankManager.dwAffinityMask = pBankManger->affinityFlag;

	if (pBankManger->paramActivityFlag & SThreadConfig::eThreadParamFlag_Priority)
#if defined(CRY_PLATFORM_POSIX)
		if (!(pBankManger->priority > 0 && pBankManger->priority <= 99))
			CryWarning(VALIDATOR_MODULE_AUDIO, VALIDATOR_WARNING, "Thread 'Wwise_BankManager' requested priority %d when allowed range is 1 to 99. Applying default value of %d.", pBankManger->priority, platformInitSettings.threadBankManager.nPriority);
		else
#endif
		platformInitSettings.threadBankManager.nPriority = pBankManger->priority;

	if (pBankManger->paramActivityFlag & SThreadConfig::eThreadParamFlag_StackSize)
		platformInitSettings.threadBankManager.uStackSize = pBankManger->stackSizeBytes;

	// LEngine thread settings
	if (pLEngineThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_Affinity)
		platformInitSettings.threadLEngine.dwAffinityMask = pLEngineThread->affinityFlag;

	if (pLEngineThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_Priority)
#if defined(CRY_PLATFORM_POSIX)
		if (!(pLEngineThread->priority > 0 && pLEngineThread->priority <= 99))
			CryWarning(VALIDATOR_MODULE_AUDIO, VALIDATOR_WARNING, "Thread 'Wwise_LEngine' requested priority %d when allowed range is 1 to 99. Applying default value of %d.", pLEngineThread->priority, platformInitSettings.threadLEngine.nPriority);
		else
#endif
		platformInitSettings.threadLEngine.nPriority = pLEngineThread->priority;

	if (pLEngineThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_StackSize)
		platformInitSettings.threadLEngine.uStackSize = pLEngineThread->stackSizeBytes;

	// Monitor thread settings
	if (pMonitorThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_Affinity)
		platformInitSettings.threadMonitor.dwAffinityMask = pMonitorThread->affinityFlag;

	if (pMonitorThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_Priority)
#if defined(CRY_PLATFORM_POSIX)
		if (!(pMonitorThread->priority > 0 && pMonitorThread->priority <= 99))
			CryWarning(VALIDATOR_MODULE_AUDIO, VALIDATOR_WARNING, "Thread 'Wwise_Monitor' requested priority %d when allowed range is 1 to 99. Applying default value of %d.", pMonitorThread->priority, platformInitSettings.threadMonitor.nPriority);
		else
#endif
		platformInitSettings.threadMonitor.nPriority = pMonitorThread->priority;

	if (pMonitorThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_StackSize)
		platformInitSettings.threadMonitor.uStackSize = pMonitorThread->stackSizeBytes;

	wwiseResult = AK::SoundEngine::Init(&initSettings, &platformInitSettings);

	if (wwiseResult != AK_Success)
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "AK::SoundEngine::Init() returned AKRESULT %d", wwiseResult);
		ShutDown();

		return eAudioRequestStatus_Failure;
	}

	AkMusicSettings musicSettings;
	AK::MusicEngine::GetDefaultInitSettings(musicSettings);

	wwiseResult = AK::MusicEngine::Init(&musicSettings);

	if (wwiseResult != AK_Success)
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "AK::MusicEngine::Init() returned AKRESULT %d", wwiseResult);
		ShutDown();

		return eAudioRequestStatus_Failure;
	}

#if !defined(WWISE_FOR_RELEASE)
	if (g_AudioImplCVars_wwise.m_nEnableCommSystem == 1)
	{
		m_bCommSystemInitialized = true;
		AkCommSettings commSettings;
		AK::Comm::GetDefaultInitSettings(commSettings);

		wwiseResult = AK::Comm::Init(commSettings);

		if (wwiseResult != AK_Success)
		{
			g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "AK::Comm::Init() returned AKRESULT %d. Communication between the Wwise authoring application and the game will not be possible\n", wwiseResult);
			m_bCommSystemInitialized = false;
		}

		wwiseResult = AK::Monitor::SetLocalOutput(AK::Monitor::ErrorLevel_All, ErrorMonitorCallback);

		if (wwiseResult != AK_Success)
		{
			AK::Comm::Term();
			g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "AK::Monitor::SetLocalOutput() returned AKRESULT %d", wwiseResult);
			m_bCommSystemInitialized = false;
		}
	}
#endif // !WWISE_FOR_RELEASE

	// Register plugins
	/// Note: This a convenience method for rapid prototyping.
	/// To reduce executable code size register/link only the plug-ins required by your game
	wwiseResult = AK::SoundEngine::RegisterAllPlugins();

	if (wwiseResult != AK_Success)
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Warning, "AK::SoundEngine::RegisterAllPlugins() returned AKRESULT %d", wwiseResult);
	}

#if defined(WWISE_USE_OCULUS)
	m_pOculusSpatializerLibrary = CryLoadLibrary("OculusSpatializer.dll");

	if (m_pOculusSpatializerLibrary != nullptr)
	{
		typedef bool (__stdcall * AkGetSoundEngineCallbacksType)
		  (unsigned short in_usCompanyID,
		  unsigned short in_usPluginID,
		  AkCreatePluginCallback& out_funcEffect,
		  AkCreateParamCallback& out_funcParam);

		AkGetSoundEngineCallbacksType AkGetSoundEngineCallbacks =
		  (AkGetSoundEngineCallbacksType)CryGetProcAddress(m_pOculusSpatializerLibrary, "AkGetSoundEngineCallbacks");

		if (AkGetSoundEngineCallbacks != nullptr)
		{
			AkCreatePluginCallback CreateOculusFX;
			AkCreateParamCallback CreateOculusFXParams;

			// Register plugin effect
			if (AkGetSoundEngineCallbacks(AKEFFECTID_OCULUS, AKEFFECTID_OCULUS_SPATIALIZER, CreateOculusFX, CreateOculusFXParams))
			{
				wwiseResult = AK::SoundEngine::RegisterPlugin(AkPluginTypeMixer, AKEFFECTID_OCULUS, AKEFFECTID_OCULUS_SPATIALIZER, CreateOculusFX, CreateOculusFXParams);

				if (wwiseResult != AK_Success)
				{
					g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Failed to register OculusSpatializer plugin.");
				}
			}
			else
			{
				g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Failed call to AkGetSoundEngineCallbacks in OculusSpatializer.dll");
			}

			// Register plugin attachment (for data attachment on individual sounds, like frequency hints etc.)
			if (AkGetSoundEngineCallbacks(AKEFFECTID_OCULUS, AKEFFECTID_OCULUS_SPATIALIZER_ATTACHMENT, CreateOculusFX, CreateOculusFXParams))
			{
				wwiseResult = AK::SoundEngine::RegisterPlugin(AkPluginTypeEffect, AKEFFECTID_OCULUS, AKEFFECTID_OCULUS_SPATIALIZER_ATTACHMENT, nullptr, CreateOculusFXParams);

				if (wwiseResult != AK_Success)
				{
					g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Failed to register OculusSpatializer attachment.");
				}
			}
			else
			{
				g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Failed call to AkGetSoundEngineCallbacks in OculusSpatializer.dll");
			}
		}
		else
		{
			g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Failed to load functions AkGetSoundEngineCallbacks in OculusSpatializer.dll");
		}
	}
	else
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Failed to load OculusSpatializer.dll");
	}
#endif // WWISE_USE_OCULUS

	REINST("Register Global Callback")

	//wwiseResult = AK::SoundEngine::RegisterGlobalCallback(GlobalCallback);
	//if (wwiseResult != AK_Success)
	//{
	//	g_AudioImplLogger_wwise.Log(eALT_WARNING, "AK::SoundEngine::RegisterGlobalCallback() returned AKRESULT %d", wwiseResult);
	//}

	// Register the DummyGameObject used for the events that don't need a location in the game world
	wwiseResult = AK::SoundEngine::RegisterGameObj(m_dummyGameObjectId, "DummyObject");

	if (wwiseResult != AK_Success)
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Warning, "AK::SoundEngine::RegisterGameObject() failed for the Dummyobject with AKRESULT %d", wwiseResult);
	}

	// Load Init.bnk before making the system available to the users
	temp = "Init.bnk";
	CONVERT_CHAR_TO_OSCHAR(temp.c_str(), szTemp);

	wwiseResult = AK::SoundEngine::LoadBank(szTemp, AK_DEFAULT_POOL_ID, m_initBankId);

	if (wwiseResult != AK_Success)
	{
		// This does not qualify for a fallback to the NULL implementation!
		// Still notify the user about this failure!
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Wwise failed to load Init.bnk, returned the AKRESULT: %d", wwiseResult);
		m_initBankId = AK_INVALID_BANK_ID;
	}

	LoadEventsMetadata();

	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::ShutDown()
{
	AKRESULT wwiseResult = AK_Fail;

#if !defined(WWISE_FOR_RELEASE)
	if (m_bCommSystemInitialized)
	{
		AK::Comm::Term();

		wwiseResult = AK::Monitor::SetLocalOutput(0, nullptr);

		if (wwiseResult != AK_Success)
		{
			g_AudioImplLogger_wwise.Log(eAudioLogType_Warning, "AK::Monitor::SetLocalOutput() returned AKRESULT %d", wwiseResult);
		}

		m_bCommSystemInitialized = false;
	}
#endif // !WWISE_FOR_RELEASE

	AK::MusicEngine::Term();

	if (AK::SoundEngine::IsInitialized())
	{
		// UnRegister the DummyGameObject
		wwiseResult = AK::SoundEngine::UnregisterGameObj(m_dummyGameObjectId);

		if (wwiseResult != AK_Success)
		{
			g_AudioImplLogger_wwise.Log(eAudioLogType_Warning, "AK::SoundEngine::UnregisterGameObject() failed for the Dummyobject with AKRESULT %d", wwiseResult);
		}

		wwiseResult = AK::SoundEngine::ClearBanks();

		if (wwiseResult != AK_Success)
		{
			g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Failed to clear banks\n");
		}

		REINST("Unregister global callback")
		//wwiseResult = AK::SoundEngine::UnregisterGlobalCallback(GlobalCallback);
		//ASSERT_WWISE_OK(wwiseResult);

		//if (wwiseResult != AK_Success)
		//{
		//	g_AudioImplLogger_wwise.Log(eALT_WARNING, "AK::SoundEngine::UnregisterGlobalCallback() returned AKRESULT %d", wwiseResult);
		//}

		AK::SoundEngine::Term();
	}

	// Terminate the streaming device and streaming manager
	// CAkFilePackageLowLevelIOBlocking::Term() destroys its associated streaming device
	// that lives in the Stream Manager, and unregisters itself as the File Location Resolver.
	if (AK::IAkStreamMgr::Get())
	{
		m_fileIOHandler.ShutDown();
		AK::IAkStreamMgr::Get()->Destroy();
	}

	// Terminate the Memory Manager
	if (AK::MemoryMgr::IsInitialized())
	{
		wwiseResult = AK::MemoryMgr::DestroyPool(0);
		AK::MemoryMgr::Term();
	}

#if defined(WWISE_USE_OCULUS)
	if (m_pOculusSpatializerLibrary != nullptr)
	{
		CryFreeLibrary(m_pOculusSpatializerLibrary);
		m_pOculusSpatializerLibrary = nullptr;
	}
#endif // WWISE_USE_OCULUS

	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::Release()
{
	POOL_FREE(this);

	// Freeing Memory Pool Memory again
	uint8* pMemSystem = g_AudioImplMemoryPool_wwise.Data();
	g_AudioImplMemoryPool_wwise.UnInitMem();

	if (pMemSystem)
	{
		delete[] (uint8*)(pMemSystem);
	}

	g_AudioImplCVars_wwise.UnregisterVariables();

	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::OnLoseFocus()
{
	// With Wwise we drive this via events.
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::OnGetFocus()
{
	// With Wwise we drive this via events.
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::MuteAll()
{
	// With Wwise we drive this via events.
	// Note: Still, make sure to return eARS_SUCCESS to signal the ATL.
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::UnmuteAll()
{
	// With Wwise we drive this via events.
	// Note: Still, make sure to return eARS_SUCCESS to signal the ATL.
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::StopAllSounds()
{
	AK::SoundEngine::StopAll();

	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::RegisterAudioObject(
  IAudioObject* const pAudioObject,
  char const* const szAudioObjectName)
{
	SAudioObject_wwise* const pWwiseAudioObject = static_cast<SAudioObject_wwise* const>(pAudioObject);

	AKRESULT const wwiseResult = AK::SoundEngine::RegisterGameObj(pWwiseAudioObject->id, szAudioObjectName);

	bool const bAKSuccess = IS_WWISE_OK(wwiseResult);

	if (!bAKSuccess)
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Warning, "Wwise RegisterGameObj failed with AKRESULT: %d", wwiseResult);
	}

	return BoolToARS(bAKSuccess);
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::RegisterAudioObject(IAudioObject* const pAudioObject)
{
	SAudioObject_wwise* const pWwiseAudioObject = static_cast<SAudioObject_wwise* const>(pAudioObject);

	AKRESULT const wwiseResult = AK::SoundEngine::RegisterGameObj(pWwiseAudioObject->id);

	bool const bAKSuccess = IS_WWISE_OK(wwiseResult);

	if (!bAKSuccess)
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Warning, "Wwise RegisterGameObj failed with AKRESULT: %d", wwiseResult);
	}

	return BoolToARS(bAKSuccess);
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::UnregisterAudioObject(IAudioObject* const pAudioObject)
{
	SAudioObject_wwise* const pWwiseAudioObject = static_cast<SAudioObject_wwise* const>(pAudioObject);

	AKRESULT const wwiseResult = AK::SoundEngine::UnregisterGameObj(pWwiseAudioObject->id);

	bool const bAKSuccess = IS_WWISE_OK(wwiseResult);

	if (!bAKSuccess)
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Warning, "Wwise UnregisterGameObj failed with AKRESULT: %d", wwiseResult);
	}

	return BoolToARS(bAKSuccess);
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::ResetAudioObject(IAudioObject* const pAudioObject)
{
	SAudioObject_wwise* const pWwiseAudioObject = static_cast<SAudioObject_wwise* const>(pAudioObject);

	pWwiseAudioObject->cEnvironemntImplAmounts.clear();
	pWwiseAudioObject->bNeedsToUpdateEnvironments = false;

	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::UpdateAudioObject(IAudioObject* const pAudioObject)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	SAudioObject_wwise* const pWwiseAudioObject = static_cast<SAudioObject_wwise* const>(pAudioObject);

	if (pWwiseAudioObject->bNeedsToUpdateEnvironments)
	{
		result = PostEnvironmentAmounts(pWwiseAudioObject);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::PlayFile(SAudioStandaloneFileInfo* const _pAudioStandaloneFileInfo)
{
	return eAudioRequestStatus_Success;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::StopFile(SAudioStandaloneFileInfo* const _pAudioStandaloneFileInfo)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::PrepareTriggerSync(
  IAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger)
{
	return PrepUnprepTriggerSync(pAudioTrigger, true);
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::UnprepareTriggerSync(
  IAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger)
{
	return PrepUnprepTriggerSync(pAudioTrigger, false);
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::PrepareTriggerAsync(
  IAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger,
  IAudioEvent* const pAudioEvent)
{
	return PrepUnprepTriggerAsync(pAudioTrigger, pAudioEvent, true);
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::UnprepareTriggerAsync(
  IAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger,
  IAudioEvent* const pAudioEvent)
{
	return PrepUnprepTriggerAsync(pAudioTrigger, pAudioEvent, false);
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::ActivateTrigger(
  IAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger,
  IAudioEvent* const pAudioEvent)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	SAudioObject_wwise* const pWwiseAudioObject = static_cast<SAudioObject_wwise* const>(pAudioObject);
	SAudioTrigger_wwise const* const pWwiseAudioTrigger = static_cast<SAudioTrigger_wwise const* const>(pAudioTrigger);
	SAudioEvent_wwise* const pWwiseAudioEvent = static_cast<SAudioEvent_wwise*>(pAudioEvent);

	if ((pWwiseAudioObject != nullptr) && (pWwiseAudioTrigger != nullptr) && (pWwiseAudioEvent != nullptr))
	{
		AkGameObjectID gameObjectId = AK_INVALID_GAME_OBJECT;

		if (pWwiseAudioObject->bHasPosition)
		{
			gameObjectId = pWwiseAudioObject->id;
			PostEnvironmentAmounts(pWwiseAudioObject);
		}
		else
		{
			gameObjectId = m_dummyGameObjectId;
		}

		AkPlayingID const id = AK::SoundEngine::PostEvent(
		  pWwiseAudioTrigger->id,
		  gameObjectId,
		  AK_EndOfEvent,
		  &EndEventCallback,
		  pWwiseAudioEvent);

		if (id != AK_INVALID_PLAYING_ID)
		{
			pWwiseAudioEvent->audioEventState = eAudioEventState_Playing;
			pWwiseAudioEvent->id = id;
			result = eAudioRequestStatus_Success;
		}
		else
		{
			// if Posting an Event failed, try to prepare it, if it isn't prepared already
			g_AudioImplLogger_wwise.Log(eAudioLogType_Warning, "Failed to Post Wwise event %" PRISIZE_T, pWwiseAudioEvent->id);
		}
	}
	else
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Invalid AudioObjectData, ATLTriggerData or EventData passed to the Wwise implementation of ActivateTrigger.");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::StopEvent(
  IAudioObject* const pAudioObject,
  IAudioEvent const* const pAudioEvent)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	SAudioEvent_wwise const* const pWwiseAudioEvent = static_cast<SAudioEvent_wwise const*>(pAudioEvent);

	if (pWwiseAudioEvent != nullptr)
	{
		switch (pWwiseAudioEvent->audioEventState)
		{
		case eAudioEventState_Playing:
			{
				AK::SoundEngine::StopPlayingID(pWwiseAudioEvent->id, 10);
				result = eAudioRequestStatus_Success;

				break;
			}
		default:
			{
				// Stopping an event of this type is not supported!
				CRY_ASSERT(false);

				break;
			}
		}
	}
	else
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Invalid EventData passed to the Wwise implementation of StopEvent.");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::StopAllEvents(IAudioObject* const _pAudioObject)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	SAudioObject_wwise* const pWwiseAudioObject = static_cast<SAudioObject_wwise* const>(_pAudioObject);

	if (pWwiseAudioObject != nullptr)
	{
		AkGameObjectID const gameObjectId = pWwiseAudioObject->bHasPosition ? pWwiseAudioObject->id : m_dummyGameObjectId;

		AK::SoundEngine::StopAll(gameObjectId);

		result = eAudioRequestStatus_Success;
	}
	else
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Invalid AudioObjectData passed to the Wwise implementation of StopAllEvents.");
	}
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CryAudio::Impl::CAudioImpl_wwise::Set3DAttributes(
  IAudioObject* const pAudioObject,
  SAudioObject3DAttributes const& attributes)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	SAudioObject_wwise* const pWwiseAudioObject = static_cast<SAudioObject_wwise* const>(pAudioObject);

	if (pWwiseAudioObject != nullptr)
	{
		AkSoundPosition soundPos;
		FillAKObjectPosition(attributes.transformation, soundPos);

		AKRESULT const wwiseResult = AK::SoundEngine::SetPosition(pWwiseAudioObject->id, soundPos);

		if (IS_WWISE_OK(wwiseResult))
		{
			result = eAudioRequestStatus_Success;
		}
		else
		{
			g_AudioImplLogger_wwise.Log(eAudioLogType_Warning, "Wwise SetPosition failed with AKRESULT: %d", wwiseResult);
		}
	}
	else
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Invalid AudioObjectData passed to the Wwise implementation of SetPosition.");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::SetEnvironment(
  IAudioObject* const pAudioObject,
  IAudioEnvironment const* const pAudioEnvironment,
  float const amount)
{
	static float const envEpsilon = 0.0001f;

	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	SAudioObject_wwise* const pWwiseAudioObject = static_cast<SAudioObject_wwise* const>(pAudioObject);
	SAudioEnvironment_wwise const* const pAKEnvironmentData = static_cast<SAudioEnvironment_wwise const* const>(pAudioEnvironment);

	if ((pWwiseAudioObject != nullptr) && (pAKEnvironmentData != nullptr))
	{
		switch (pAKEnvironmentData->type)
		{
		case eWAET_AUX_BUS:
			{
				float const fCurrentAmount = stl::find_in_map(
				  pWwiseAudioObject->cEnvironemntImplAmounts,
				  pAKEnvironmentData->busId,
				  -1.0f);

				if ((fCurrentAmount == -1.0f) || (fabs(fCurrentAmount - amount) > envEpsilon))
				{
					pWwiseAudioObject->cEnvironemntImplAmounts[pAKEnvironmentData->busId] = amount;
					pWwiseAudioObject->bNeedsToUpdateEnvironments = true;
				}

				result = eAudioRequestStatus_Success;

				break;
			}
		case eWAET_RTPC:
			{
				AkRtpcValue rtpcValue = static_cast<AkRtpcValue>(pAKEnvironmentData->multiplier * amount + pAKEnvironmentData->shift);

				AKRESULT const wwiseResult = AK::SoundEngine::SetRTPCValue(pAKEnvironmentData->rtpcId, rtpcValue, pWwiseAudioObject->id);

				if (IS_WWISE_OK(wwiseResult))
				{
					result = eAudioRequestStatus_Success;
				}
				else
				{
					g_AudioImplLogger_wwise.Log(
					  eAudioLogType_Warning,
					  "Wwise failed to set the Rtpc %u to value %f on object %u in SetEnvironement()",
					  pAKEnvironmentData->rtpcId,
					  rtpcValue,
					  pWwiseAudioObject->id);
				}

				break;
			}
		default:
			{
				CRY_ASSERT(false);//Unknown AudioEnvironmentImplementation type
			}

		}

	}
	else
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Invalid AudioObjectData or EnvironmentData passed to the Wwise implementation of SetEnvironment");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::SetRtpc(
  IAudioObject* const pAudioObject,
  IAudioRtpc const* const pAudioRtpc,
  float const value)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	SAudioObject_wwise* const pWwiseAudioObject = static_cast<SAudioObject_wwise* const>(pAudioObject);
	SAudioRtpc_wwise const* const pAKRtpcData = static_cast<SAudioRtpc_wwise const* const>(pAudioRtpc);

	if ((pWwiseAudioObject != nullptr) && (pAKRtpcData != nullptr))
	{
		AkRtpcValue rtpcValue = static_cast<AkRtpcValue>(pAKRtpcData->mult * value + pAKRtpcData->shift);

		AKRESULT const wwiseResult = AK::SoundEngine::SetRTPCValue(pAKRtpcData->id, rtpcValue, pWwiseAudioObject->id);

		if (IS_WWISE_OK(wwiseResult))
		{
			result = eAudioRequestStatus_Success;
		}
		else
		{
			g_AudioImplLogger_wwise.Log(
			  eAudioLogType_Warning,
			  "Wwise failed to set the Rtpc %" PRISIZE_T " to value %f on object %" PRISIZE_T,
			  pAKRtpcData->id,
			  static_cast<AkRtpcValue>(value),
			  pWwiseAudioObject->id);
		}
	}
	else
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Invalid AudioObjectData or RtpcData passed to the Wwise implementation of SetRtpc");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::SetSwitchState(
  IAudioObject* const pAudioObject,
  IAudioSwitchState const* const pAudioSwitchState)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	SAudioObject_wwise* const pWwiseAudioObject = static_cast<SAudioObject_wwise* const>(pAudioObject);
	SAudioSwitchState_wwise const* const pAKSwitchStateData = static_cast<SAudioSwitchState_wwise const* const>(pAudioSwitchState);

	if ((pWwiseAudioObject != nullptr) && (pAKSwitchStateData != nullptr))
	{
		switch (pAKSwitchStateData->type)
		{
		case eWST_SWITCH:
			{
				AkGameObjectID const gameObjectId = pWwiseAudioObject->bHasPosition ? pWwiseAudioObject->id : m_dummyGameObjectId;

				AKRESULT const wwiseResult = AK::SoundEngine::SetSwitch(
				  pAKSwitchStateData->switchId,
				  pAKSwitchStateData->stateId,
				  gameObjectId);

				if (IS_WWISE_OK(wwiseResult))
				{
					result = eAudioRequestStatus_Success;
				}
				else
				{
					g_AudioImplLogger_wwise.Log(
					  eAudioLogType_Warning,
					  "Wwise failed to set the switch group %" PRISIZE_T " to state %" PRISIZE_T " on object %" PRISIZE_T,
					  pAKSwitchStateData->switchId,
					  pAKSwitchStateData->stateId,
					  gameObjectId);
				}

				break;
			}
		case eWST_STATE:
			{
				AKRESULT const wwiseResult = AK::SoundEngine::SetState(
				  pAKSwitchStateData->switchId,
				  pAKSwitchStateData->stateId);

				if (IS_WWISE_OK(wwiseResult))
				{
					result = eAudioRequestStatus_Success;
				}
				else
				{
					g_AudioImplLogger_wwise.Log(
					  eAudioLogType_Warning,
					  "Wwise failed to set the state group %" PRISIZE_T "to state %" PRISIZE_T,
					  pAKSwitchStateData->switchId,
					  pAKSwitchStateData->stateId);
				}

				break;
			}
		case eWST_RTPC:
			{
				AkGameObjectID const gameObjectId = pWwiseAudioObject->id;

				AKRESULT const wwiseResult = AK::SoundEngine::SetRTPCValue(
				  pAKSwitchStateData->switchId,
				  static_cast<AkRtpcValue>(pAKSwitchStateData->rtpcValue),
				  gameObjectId);

				if (IS_WWISE_OK(wwiseResult))
				{
					result = eAudioRequestStatus_Success;
				}
				else
				{
					g_AudioImplLogger_wwise.Log(
					  eAudioLogType_Warning,
					  "Wwise failed to set the Rtpc %" PRISIZE_T " to value %f on object %" PRISIZE_T,
					  pAKSwitchStateData->switchId,
					  static_cast<AkRtpcValue>(pAKSwitchStateData->rtpcValue),
					  gameObjectId);
				}

				break;
			}
		case eWST_NONE:
			{
				break;
			}
		default:
			{
				g_AudioImplLogger_wwise.Log(eAudioLogType_Warning, "Unknown EWwiseSwitchType: %" PRISIZE_T, pAKSwitchStateData->type);

				break;
			}
		}
	}
	else
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Invalid AudioObjectData or RtpcData passed to the Wwise implementation of SetRtpc");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::SetObstructionOcclusion(
  IAudioObject* const pAudioObject,
  float const obstruction,
  float const occlusion)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	SAudioObject_wwise* const pWwiseAudioObject = static_cast<SAudioObject_wwise* const>(pAudioObject);

	if (pWwiseAudioObject != nullptr)
	{
		AKRESULT const wwiseResult = AK::SoundEngine::SetObjectObstructionAndOcclusion(
		  pWwiseAudioObject->id,
		  0,// only set the obstruction/occlusion for the default listener for now
		  static_cast<AkReal32>(occlusion), // Currently used on obstruction until the ATL produces a correct obstruction value.
		  static_cast<AkReal32>(occlusion));

		if (IS_WWISE_OK(wwiseResult))
		{
			result = eAudioRequestStatus_Success;
		}
		else
		{
			g_AudioImplLogger_wwise.Log(
			  eAudioLogType_Warning,
			  "Wwise failed to set Obstruction %f and Occlusion %f on object %" PRISIZE_T,
			  obstruction,
			  occlusion,
			  pWwiseAudioObject->id);
		}
	}
	else
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Invalid AudioObjectData passed to the Wwise implementation of SetObjectObstructionAndOcclusion");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CryAudio::Impl::CAudioImpl_wwise::SetListener3DAttributes(
  IAudioListener* const pAudioListener,
  SAudioObject3DAttributes const& attributes)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	SAudioListener_wwise* const pAKListenerData = static_cast<SAudioListener_wwise* const>(pAudioListener);

	if (pAKListenerData != nullptr)
	{
		AkListenerPosition listenerPos;
		FillAKListenerPosition(attributes.transformation, listenerPos);
		AKRESULT const wwiseResult = AK::SoundEngine::SetListenerPosition(listenerPos, pAKListenerData->id);

		if (IS_WWISE_OK(wwiseResult))
		{
			result = eAudioRequestStatus_Success;
		}
		else
		{
			g_AudioImplLogger_wwise.Log(eAudioLogType_Warning, "Wwise SetListenerPosition failed with AKRESULT: %" PRISIZE_T, wwiseResult);
		}
	}
	else
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Invalid ATLListenerData passed to the Wwise implementation of SetListenerPosition");
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::RegisterInMemoryFile(SAudioFileEntryInfo* const pFileEntryInfo)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	if (pFileEntryInfo != nullptr)
	{
		SAudioFileEntry_wwise* const pFileDataWwise = static_cast<SAudioFileEntry_wwise*>(pFileEntryInfo->pImplData);

		if (pFileDataWwise != nullptr)
		{
			AkBankID bankId = AK_INVALID_BANK_ID;

			AKRESULT const wwiseResult = AK::SoundEngine::LoadBank(
			  pFileEntryInfo->pFileData,
			  static_cast<AkUInt32>(pFileEntryInfo->size),
			  bankId);

			if (IS_WWISE_OK(wwiseResult))
			{
				pFileDataWwise->bankId = bankId;
				result = eAudioRequestStatus_Success;
			}
			else
			{
				pFileDataWwise->bankId = AK_INVALID_BANK_ID;
				g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Failed to load file %s\n", pFileEntryInfo->szFileName);
			}
		}
		else
		{
			g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Invalid AudioFileEntryData passed to the Wwise implementation of RegisterInMemoryFile");
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::UnregisterInMemoryFile(SAudioFileEntryInfo* const pFileEntryInfo)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	if (pFileEntryInfo != nullptr)
	{
		SAudioFileEntry_wwise* const pFileDataWwise = static_cast<SAudioFileEntry_wwise*>(pFileEntryInfo->pImplData);

		if (pFileDataWwise != nullptr)
		{
			AKRESULT const wwiseResult = AK::SoundEngine::UnloadBank(pFileDataWwise->bankId, pFileEntryInfo->pFileData);

			if (IS_WWISE_OK(wwiseResult))
			{
				result = eAudioRequestStatus_Success;
			}
			else
			{
				g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Wwise Failed to unregister in memory file %s\n", pFileEntryInfo->szFileName);
			}
		}
		else
		{
			g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Invalid AudioFileEntryData passed to the Wwise implementation of UnregisterInMemoryFile");
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::ParseAudioFileEntry(XmlNodeRef const pAudioFileEntryNode, SAudioFileEntryInfo* const pFileEntryInfo)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	if ((_stricmp(pAudioFileEntryNode->getTag(), s_szWwiseFileTag) == 0) && (pFileEntryInfo != nullptr))
	{
		char const* const szWwiseAudioFileEntryName = pAudioFileEntryNode->getAttr(s_szWwiseNameAttribute);

		if (szWwiseAudioFileEntryName != nullptr && szWwiseAudioFileEntryName[0] != '\0')
		{
			char const* const szWwiseLocalized = pAudioFileEntryNode->getAttr(s_szWwiseLocalisedAttribute);
			pFileEntryInfo->bLocalized = (szWwiseLocalized != nullptr) && (_stricmp(szWwiseLocalized, "true") == 0);
			pFileEntryInfo->szFileName = szWwiseAudioFileEntryName;
			pFileEntryInfo->memoryBlockAlignment = AK_BANK_PLATFORM_DATA_ALIGNMENT;

			POOL_NEW(SAudioFileEntry_wwise, pFileEntryInfo->pImplData);

			result = eAudioRequestStatus_Success;
		}
		else
		{
			pFileEntryInfo->szFileName = nullptr;
			pFileEntryInfo->memoryBlockAlignment = 0;
			pFileEntryInfo->pImplData = nullptr;
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::DeleteAudioFileEntry(IAudioFileEntry* const pOldAudioFileEntry)
{
	POOL_FREE(pOldAudioFileEntry);
}

//////////////////////////////////////////////////////////////////////////
char const* const CAudioImpl_wwise::GetAudioFileLocation(SAudioFileEntryInfo* const pFileEntryInfo)
{
	char const* szResult = nullptr;

	if (pFileEntryInfo != nullptr)
	{
		szResult = pFileEntryInfo->bLocalized ? m_localizedSoundBankFolder.c_str() : m_regularSoundBankFolder.c_str();
	}

	return szResult;
}

///////////////////////////////////////////////////////////////////////////
IAudioObject* CAudioImpl_wwise::NewGlobalAudioObject(AudioObjectId const audioObjectID)
{
	POOL_NEW_CREATE(SAudioObject_wwise, pWwiseAudioObject)(AK_INVALID_GAME_OBJECT, false);
	return pWwiseAudioObject;
}

///////////////////////////////////////////////////////////////////////////
IAudioObject* CAudioImpl_wwise::NewAudioObject(AudioObjectId const audioObjectID)
{
	POOL_NEW_CREATE(SAudioObject_wwise, pWwiseAudioObject)(static_cast<AkGameObjectID>(audioObjectID), true);
	return pWwiseAudioObject;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::DeleteAudioObject(IAudioObject const* const pOldObjectData)
{
	POOL_FREE_CONST(pOldObjectData);
}

///////////////////////////////////////////////////////////////////////////
CryAudio::Impl::IAudioListener* CAudioImpl_wwise::NewDefaultAudioListener(AudioObjectId const audioObjectId)
{
	POOL_NEW_CREATE(SAudioListener_wwise, pNewObject)(0);
	return pNewObject;
}

///////////////////////////////////////////////////////////////////////////
CryAudio::Impl::IAudioListener* CAudioImpl_wwise::NewAudioListener(AudioObjectId const audioObjectId)
{
	static AkUniqueID id = 0;
	POOL_NEW_CREATE(SAudioListener_wwise, pNewObject)(++id);
	return pNewObject;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::DeleteAudioListener(CryAudio::Impl::IAudioListener* const pOldAudioListener)
{
	POOL_FREE(pOldAudioListener);
}

//////////////////////////////////////////////////////////////////////////
IAudioEvent* CAudioImpl_wwise::NewAudioEvent(AudioEventId const audioEventID)
{
	POOL_NEW_CREATE(SAudioEvent_wwise, pWwiseAudioEvent)(audioEventID);
	return pWwiseAudioEvent;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::DeleteAudioEvent(IAudioEvent const* const pOldAudioEvent)
{
	POOL_FREE_CONST(pOldAudioEvent);
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::ResetAudioEvent(IAudioEvent* const pAudioEvent)
{
	SAudioEvent_wwise* const pWwiseAudioEvent = static_cast<SAudioEvent_wwise*>(pAudioEvent);

	if (pWwiseAudioEvent != nullptr)
	{
		pWwiseAudioEvent->audioEventState = eAudioEventState_None;
		pWwiseAudioEvent->id = AK_INVALID_UNIQUE_ID;
	}
}

//////////////////////////////////////////////////////////////////////////
IAudioStandaloneFile* CAudioImpl_wwise::NewAudioStandaloneFile()
{
	POOL_NEW_CREATE(SAudioStandaloneFile_wwise, pNewStandaloneFileData);
	return pNewStandaloneFileData;
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::DeleteAudioStandaloneFile(IAudioStandaloneFile const* const _pOldAudioStandaloneFile)
{
	POOL_FREE_CONST(_pOldAudioStandaloneFile);
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::ResetAudioStandaloneFile(IAudioStandaloneFile* const _pAudioStandaloneFile)
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::GamepadConnected(TAudioGamepadUniqueID const deviceUniqueID)
{
#if defined(AK_MOTION)
	AudioInputDevices::const_iterator Iter(m_mapInputDevices.find(deviceUniqueID));

	if (Iter == m_mapInputDevices.end())
	{
		AkUInt8 const deviceID = static_cast<AkUInt8>(m_mapInputDevices.size());
		Iter = m_mapInputDevices.insert(std::make_pair(deviceUniqueID, deviceID)).first;
		CRY_ASSERT(m_mapInputDevices.size() < 5); // Wwise does not allow for more than 4 motion devices.
	}

	AkUInt8 const deviceID = Iter->second;
	AKRESULT wwiseResult = AK::MotionEngine::AddPlayerMotionDevice(deviceID, AKCOMPANYID_AUDIOKINETIC, AKMOTIONDEVICEID_RUMBLE);

	if (IS_WWISE_OK(wwiseResult))
	{
		AK::MotionEngine::SetPlayerListener(deviceID, deviceID);
		wwiseResult = AK::SoundEngine::SetListenerPipeline(static_cast<AkUInt32>(deviceID), true, true);

		if (!IS_WWISE_OK(wwiseResult))
		{
			g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "SetListenerPipeline failed in GamepadConnected! (%u : %u)", deviceUniqueID, deviceID);
		}
	}
	else
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "AddPlayerMotionDevice failed! (%u : %u)", deviceUniqueID, deviceID);
	}
#endif // AK_MOTION
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::GamepadDisconnected(TAudioGamepadUniqueID const deviceUniqueID)
{
#if defined(AK_MOTION)
	AudioInputDevices::const_iterator const Iter(m_mapInputDevices.find(deviceUniqueID));

	if (Iter != m_mapInputDevices.end())
	{
		AkUInt8 const deviceID = Iter->second;
		AK::MotionEngine::RemovePlayerMotionDevice(deviceID, AKCOMPANYID_AUDIOKINETIC, AKMOTIONDEVICEID_RUMBLE);
		AKRESULT const wwiseResult = AK::SoundEngine::SetListenerPipeline(static_cast<AkUInt32>(deviceID), true, false);

		if (!IS_WWISE_OK(wwiseResult))
		{
			g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "SetListenerPipeline failed in GamepadDisconnected! (%u : %u)", deviceUniqueID, deviceID);
		}
	}
	else
	{
		CRY_ASSERT(m_mapInputDevices.empty());
	}
#endif // AK_MOTION
}

///////////////////////////////////////////////////////////////////////////
IAudioTrigger const* CAudioImpl_wwise::NewAudioTrigger(XmlNodeRef const pAudioTriggerNode, SAudioTriggerInfo& info)
{
	IAudioTrigger* pAudioTrigger = nullptr;

	if (_stricmp(pAudioTriggerNode->getTag(), s_szWwiseEventTag) == 0)
	{
		char const* const szWwiseEventName = pAudioTriggerNode->getAttr(s_szWwiseNameAttribute);
		AkUniqueID const uniqueId = AK::SoundEngine::GetIDFromString(szWwiseEventName);//Does not check if the string represents an event!!!!

		if (uniqueId != AK_INVALID_UNIQUE_ID)
		{
			POOL_NEW(SAudioTrigger_wwise, pAudioTrigger)(uniqueId);
			info.maxRadius = m_eventsInfoMap[AudioStringToId(szWwiseEventName)].maxRadius;
		}
		else
		{
			CRY_ASSERT(false);
		}
	}
	return pAudioTrigger;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::DeleteAudioTrigger(IAudioTrigger const* const pOldAudioTrigger)
{
	POOL_FREE_CONST(pOldAudioTrigger);
}

///////////////////////////////////////////////////////////////////////////
IAudioRtpc const* CAudioImpl_wwise::NewAudioRtpc(XmlNodeRef const pAudioRtpcNode)
{
	IAudioRtpc* pAudioRtpc = nullptr;

	AkRtpcID rtpcId = AK_INVALID_RTPC_ID;
	float multiplier = 1.0f;
	float shift = 0.0f;

	ParseRtpcImpl(pAudioRtpcNode, rtpcId, multiplier, shift);

	if (rtpcId != AK_INVALID_RTPC_ID)
	{
		POOL_NEW(SAudioRtpc_wwise, pAudioRtpc)(rtpcId, multiplier, shift);
	}

	return pAudioRtpc;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::DeleteAudioRtpc(IAudioRtpc const* const pOldRtpcImplData)
{
	POOL_FREE_CONST(pOldRtpcImplData);
}

///////////////////////////////////////////////////////////////////////////
IAudioSwitchState const* CAudioImpl_wwise::NewAudioSwitchState(XmlNodeRef const pAudioSwitchNode)
{
	char const* const szTag = pAudioSwitchNode->getTag();
	IAudioSwitchState const* pAudioSwitchState = nullptr;

	if (_stricmp(szTag, s_szWwiseSwitchTag) == 0)
	{
		pAudioSwitchState = ParseWwiseSwitchOrState(pAudioSwitchNode, eWST_SWITCH);
	}
	else if (_stricmp(szTag, s_szWwiseStateTag) == 0)
	{
		pAudioSwitchState = ParseWwiseSwitchOrState(pAudioSwitchNode, eWST_STATE);
	}
	else if (_stricmp(szTag, s_szWwiseRtpcSwitchTag) == 0)
	{
		pAudioSwitchState = ParseWwiseRtpcSwitch(pAudioSwitchNode);
	}
	else
	{
		// Unknown Wwise switch tag!
		g_AudioImplLogger_wwise.Log(eAudioLogType_Warning, "Unknown Wwise switch tag! (%s)", szTag);
	}

	return pAudioSwitchState;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::DeleteAudioSwitchState(IAudioSwitchState const* const pOldAudioSwitchState)
{
	POOL_FREE_CONST(pOldAudioSwitchState);
}

///////////////////////////////////////////////////////////////////////////
IAudioEnvironment const* CAudioImpl_wwise::NewAudioEnvironment(XmlNodeRef const pAudioEnvironmentNode)
{
	IAudioEnvironment* pAudioEnvironment = nullptr;

	if (_stricmp(pAudioEnvironmentNode->getTag(), s_szWwiseAuxBusTag) == 0)
	{
		char const* const szWwiseAuxBusName = pAudioEnvironmentNode->getAttr(s_szWwiseNameAttribute);
		AkUniqueID const busId = AK::SoundEngine::GetIDFromString(szWwiseAuxBusName);//Does not check if the string represents an event!!!!

		if (busId != AK_INVALID_AUX_ID)
		{
			POOL_NEW(SAudioEnvironment_wwise, pAudioEnvironment)(eWAET_AUX_BUS, static_cast<AkAuxBusID>(busId));
		}
		else
		{
			CRY_ASSERT(false);// unknown Aux Bus
		}
	}
	else if (_stricmp(pAudioEnvironmentNode->getTag(), s_szWwiseRtpcTag) == 0)
	{
		AkRtpcID rtpcId = AK_INVALID_RTPC_ID;
		float multiplier = 1.0f;
		float shift = 0.0f;

		ParseRtpcImpl(pAudioEnvironmentNode, rtpcId, multiplier, shift);

		if (rtpcId != AK_INVALID_RTPC_ID)
		{
			POOL_NEW(SAudioEnvironment_wwise, pAudioEnvironment)(eWAET_RTPC, rtpcId, multiplier, shift);
		}
		else
		{
			CRY_ASSERT(false); // Unknown RTPC
		}
	}

	return pAudioEnvironment;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::DeleteAudioEnvironment(IAudioEnvironment const* const pOldAudioEnvironment)
{
	POOL_FREE_CONST(pOldAudioEnvironment);
}

///////////////////////////////////////////////////////////////////////////
char const* const CAudioImpl_wwise::GetImplementationNameString() const
{
#if defined(INCLUDE_WWISE_IMPL_PRODUCTION_CODE)
	return m_fullImplString.c_str();
#endif // INCLUDE_WWISE_IMPL_PRODUCTION_CODE

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::GetMemoryInfo(SAudioImplMemoryInfo& memoryInfo) const
{
	memoryInfo.primaryPoolSize = g_AudioImplMemoryPool_wwise.MemSize();
	memoryInfo.primaryPoolUsedSize = memoryInfo.primaryPoolSize - g_AudioImplMemoryPool_wwise.MemFree();
	memoryInfo.primaryPoolAllocations = g_AudioImplMemoryPool_wwise.FragmentCount();

	memoryInfo.bucketUsedSize = g_AudioImplMemoryPool_wwise.GetSmallAllocsSize();
	memoryInfo.bucketAllocations = g_AudioImplMemoryPool_wwise.GetSmallAllocsCount();

#if defined(PROVIDE_WWISE_IMPL_SECONDARY_POOL)
	memoryInfo.secondaryPoolSize = g_AudioImplMemoryPoolSecondary_wwise.MemSize();
	memoryInfo.secondaryPoolUsedSize = memoryInfo.secondaryPoolSize - g_AudioImplMemoryPoolSecondary_wwise.MemFree();
	memoryInfo.secondaryPoolAllocations = g_AudioImplMemoryPoolSecondary_wwise.FragmentCount();
#else
	memoryInfo.secondaryPoolSize = 0;
	memoryInfo.secondaryPoolUsedSize = 0;
	memoryInfo.secondaryPoolAllocations = 0;
#endif // PROVIDE_AUDIO_IMPL_SECONDARY_POOL
}

//////////////////////////////////////////////////////////////////////////
bool CAudioImpl_wwise::SEnvPairCompare::operator()(std::pair<AkAuxBusID, float> const& pair1, std::pair<AkAuxBusID, float> const& pair2) const
{
	return (pair1.second > pair2.second);
}

//////////////////////////////////////////////////////////////////////////
void CryAudio::Impl::CAudioImpl_wwise::LoadEventsMetadata()
{
	string const path = m_regularSoundBankFolder + CRY_NATIVE_PATH_SEPSTR + s_szSoundBanksInfoFilename;
	XmlNodeRef pRootNode = GetISystem()->LoadXmlFromFile(path.c_str());
	if (pRootNode)
	{
		XmlNodeRef pSoundBanksNode = pRootNode->findChild("SoundBanks");
		if (pSoundBanksNode)
		{
			const int size = pSoundBanksNode->getChildCount();
			for (int i = 0; i < size; ++i)
			{
				XmlNodeRef pSoundBankNode = pSoundBanksNode->getChild(i);
				if (pSoundBankNode)
				{
					XmlNodeRef pIncludedEventsNode = pSoundBankNode->findChild("IncludedEvents");
					if (pIncludedEventsNode)
					{
						const int size2 = pIncludedEventsNode->getChildCount();
						for (int j = 0; j < size2; ++j)
						{
							XmlNodeRef pEventNode = pIncludedEventsNode->getChild(j);
							if (pEventNode)
							{
								SEventInfo info;
								const char* szName = pEventNode->getAttr("Name");
								if (pEventNode->haveAttr("MaxAttenuation"))
								{
									const char* szMaxRadius = pEventNode->getAttr("MaxAttenuation");
									info.maxRadius = static_cast<float>(atof(szMaxRadius));
								}
								else
								{
									info.maxRadius = 0.0f;
								}
								m_eventsInfoMap[AudioStringToId(szName)] = info;
							}
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::GetAudioFileData(char const* const szFilename, SAudioFileData& audioFileData) const
{}

//////////////////////////////////////////////////////////////////////////
SAudioSwitchState_wwise const* CAudioImpl_wwise::ParseWwiseSwitchOrState(
  XmlNodeRef const pNode,
  EWwiseSwitchType const switchType)
{
	SAudioSwitchState_wwise* pSwitchStateImpl = nullptr;

	char const* const szWwiseSwitchNodeName = pNode->getAttr(s_szWwiseNameAttribute);

	if ((szWwiseSwitchNodeName != nullptr) && (szWwiseSwitchNodeName[0] != 0) && (pNode->getChildCount() == 1))
	{
		XmlNodeRef const pValueNode(pNode->getChild(0));

		if (pValueNode && _stricmp(pValueNode->getTag(), s_szWwiseValueTag) == 0)
		{
			char const* const szWwiseSwitchStateName = pValueNode->getAttr(s_szWwiseNameAttribute);

			if ((szWwiseSwitchStateName != nullptr) && (szWwiseSwitchStateName[0] != 0))
			{
				AkUniqueID const switchId = AK::SoundEngine::GetIDFromString(szWwiseSwitchNodeName);
				AkUniqueID const switchStateId = AK::SoundEngine::GetIDFromString(szWwiseSwitchStateName);
				POOL_NEW(SAudioSwitchState_wwise, pSwitchStateImpl)(switchType, switchId, switchStateId);
			}
		}
	}
	else
	{
		g_AudioImplLogger_wwise.Log(
		  eAudioLogType_Warning,
		  "A Wwise Switch or State %s inside ATLSwitchState needs to have exactly one WwiseValue.",
		  szWwiseSwitchNodeName);
	}

	return pSwitchStateImpl;
}

///////////////////////////////////////////////////////////////////////////
SAudioSwitchState_wwise const* CAudioImpl_wwise::ParseWwiseRtpcSwitch(XmlNodeRef const pNode)
{
	SAudioSwitchState_wwise* pSwitchStateImpl = nullptr;

	char const* const szWwiseRtpcNodeName = pNode->getAttr(s_szWwiseNameAttribute);

	if ((szWwiseRtpcNodeName != nullptr) && (szWwiseRtpcNodeName[0] != '\0'))
	{
		float rtpcValue = 0.0f;

		if (pNode->getAttr(s_szWwiseValueAttribute, rtpcValue))
		{
			AkUniqueID const rtpcId = AK::SoundEngine::GetIDFromString(szWwiseRtpcNodeName);
			POOL_NEW(SAudioSwitchState_wwise, pSwitchStateImpl)(eWST_RTPC, rtpcId, rtpcId, rtpcValue);
		}
	}
	else
	{
		g_AudioImplLogger_wwise.Log(
		  eAudioLogType_Warning,
		  "The Wwise Rtpc %s inside ATLSwitchState does not have a valid name.",
		  szWwiseRtpcNodeName);
	}

	return pSwitchStateImpl;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::ParseRtpcImpl(XmlNodeRef const pNode, AkRtpcID& rtpcId, float& multiplier, float& shift)
{
	if (_stricmp(pNode->getTag(), s_szWwiseRtpcTag) == 0)
	{
		char const* const szRtpcName = pNode->getAttr(s_szWwiseNameAttribute);
		rtpcId = static_cast<AkRtpcID>(AK::SoundEngine::GetIDFromString(szRtpcName));

		if (rtpcId != AK_INVALID_RTPC_ID)
		{
			//the Wwise name is supplied
			pNode->getAttr(s_szWwiseMutiplierAttribute, multiplier);
			pNode->getAttr(s_szWwiseShiftAttribute, shift);
		}
		else
		{
			// Invalid Wwise RTPC name!
			g_AudioImplLogger_wwise.Log(eAudioLogType_Warning, "Invalid Wwise RTPC name %s", szRtpcName);
		}
	}
	else
	{
		// Unknown Wwise RTPC tag!
		g_AudioImplLogger_wwise.Log(eAudioLogType_Warning, "Unknown Wwise RTPC tag %s", pNode->getTag());

	}
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::PrepUnprepTriggerSync(
  IAudioTrigger const* const pAudioTrigger,
  bool bPrepare)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	SAudioTrigger_wwise const* const pAKTriggerImplData = static_cast<SAudioTrigger_wwise const* const>(pAudioTrigger);

	if (pAKTriggerImplData != nullptr)
	{
		AkUniqueID nImplAKID = pAKTriggerImplData->id;

		AKRESULT const wwiseResult = AK::SoundEngine::PrepareEvent(
		  bPrepare ? AK::SoundEngine::Preparation_Load : AK::SoundEngine::Preparation_Unload,
		  &nImplAKID,
		  1);

		if (IS_WWISE_OK(wwiseResult))
		{
			result = eAudioRequestStatus_Success;
		}
		else
		{
			g_AudioImplLogger_wwise.Log(
			  eAudioLogType_Warning,
			  "Wwise PrepareEvent with %s failed for Wwise event %" PRISIZE_T " with AKRESULT: %" PRISIZE_T,
			  bPrepare ? "Preparation_Load" : "Preparation_Unload",
			  nImplAKID,
			  wwiseResult);
		}

	}
	else
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error,
		                            "Invalid ATLTriggerData or EventData passed to the Wwise implementation of %sTriggerSync",
		                            bPrepare ? "Prepare" : "Unprepare");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::PrepUnprepTriggerAsync(
  IAudioTrigger const* const pAudioTrigger,
  IAudioEvent* const pAudioEvent,
  bool bPrepare)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	SAudioTrigger_wwise const* const pWwiseAudioTrigger = static_cast<SAudioTrigger_wwise const* const>(pAudioTrigger);
	SAudioEvent_wwise* const pWwiseAudioEvent = static_cast<SAudioEvent_wwise*>(pAudioEvent);

	if ((pWwiseAudioTrigger != nullptr) && (pWwiseAudioEvent != nullptr))
	{
		AkUniqueID eventId = pWwiseAudioTrigger->id;

		AKRESULT const wwiseResult = AK::SoundEngine::PrepareEvent(
		  bPrepare ? AK::SoundEngine::Preparation_Load : AK::SoundEngine::Preparation_Unload,
		  &eventId,
		  1,
		  &PrepareEventCallback,
		  pWwiseAudioEvent);

		if (IS_WWISE_OK(wwiseResult))
		{
			pWwiseAudioEvent->id = eventId;
			pWwiseAudioEvent->audioEventState = eAudioEventState_Unloading;

			result = eAudioRequestStatus_Success;
		}
		else
		{
			g_AudioImplLogger_wwise.Log(
			  eAudioLogType_Warning,
			  "Wwise PrepareEvent with %s failed for Wwise event %" PRISIZE_T " with AKRESULT: %d",
			  bPrepare ? "Preparation_Load" : "Preparation_Unload",
			  eventId,
			  wwiseResult);
		}
	}
	else
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error,
		                            "Invalid ATLTriggerData or EventData passed to the Wwise implementation of %sTriggerAsync",
		                            bPrepare ? "Prepare" : "Unprepare");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl_wwise::PostEnvironmentAmounts(IAudioObject* const pAudioObject)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;
	SAudioObject_wwise* const pWwiseAudioObject = static_cast<SAudioObject_wwise* const>(pAudioObject);

	if (pWwiseAudioObject != nullptr)
	{
		AkAuxSendValue auxValues[AK_MAX_AUX_PER_OBJ];
		uint32 auxIndex = 0;

		SAudioObject_wwise::TEnvironmentImplMap::iterator iEnvPair = pWwiseAudioObject->cEnvironemntImplAmounts.begin();
		SAudioObject_wwise::TEnvironmentImplMap::const_iterator const iEnvStart = pWwiseAudioObject->cEnvironemntImplAmounts.begin();
		SAudioObject_wwise::TEnvironmentImplMap::const_iterator const iEnvEnd = pWwiseAudioObject->cEnvironemntImplAmounts.end();

		if (pWwiseAudioObject->cEnvironemntImplAmounts.size() <= AK_MAX_AUX_PER_OBJ)
		{
			for (; iEnvPair != iEnvEnd; ++auxIndex)
			{
				float const fAmount = iEnvPair->second;

				auxValues[auxIndex].auxBusID = iEnvPair->first;
				auxValues[auxIndex].fControlValue = fAmount;

				// If an amount is zero, we still want to send it to the middleware, but we also want to remove it from the map.
				if (fAmount == 0.0f)
				{
					pWwiseAudioObject->cEnvironemntImplAmounts.erase(iEnvPair++);
				}
				else
				{
					++iEnvPair;
				}
			}
		}
		else
		{
			// sort the environments in order of decreasing amounts and take the first AK_MAX_AUX_PER_OBJ worth
			typedef std::set<std::pair<AkAuxBusID, float>, SEnvPairCompare> TEnvPairSet;
			TEnvPairSet cEnvPairs(iEnvStart, iEnvEnd);

			TEnvPairSet::const_iterator iSortedEnvPair = cEnvPairs.begin();
			TEnvPairSet::const_iterator const iSortedEnvEnd = cEnvPairs.end();

			for (; (iSortedEnvPair != iSortedEnvEnd) && (auxIndex < AK_MAX_AUX_PER_OBJ); ++iSortedEnvPair, ++auxIndex)
			{
				auxValues[auxIndex].auxBusID = iSortedEnvPair->first;
				auxValues[auxIndex].fControlValue = iSortedEnvPair->second;
			}

			//remove all Environments with 0.0 amounts
			while (iEnvPair != iEnvEnd)
			{
				if (iEnvPair->second == 0.0f)
				{
					pWwiseAudioObject->cEnvironemntImplAmounts.erase(iEnvPair++);
				}
				else
				{
					++iEnvPair;
				}
			}
		}

		CRY_ASSERT(auxIndex <= AK_MAX_AUX_PER_OBJ);

		AKRESULT const wwiseResult = AK::SoundEngine::SetGameObjectAuxSendValues(pWwiseAudioObject->id, auxValues, auxIndex);

		if (IS_WWISE_OK(wwiseResult))
		{
			result = eAudioRequestStatus_Success;
		}
		else
		{
			g_AudioImplLogger_wwise.Log(eAudioLogType_Warning,
			                            "Wwise SetGameObjectAuxSendValues failed on object %" PRISIZE_T " with AKRESULT: %d",
			                            pWwiseAudioObject->id,
			                            wwiseResult);
		}

		pWwiseAudioObject->bNeedsToUpdateEnvironments = false;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::OnAudioSystemRefresh()
{
	AKRESULT wwiseResult = AK_Fail;

	if (m_initBankId != AK_INVALID_BANK_ID)
	{
		wwiseResult = AK::SoundEngine::UnloadBank(m_initBankId, nullptr);

		if (wwiseResult != AK_Success)
		{
			g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Wwise failed to unload Init.bnk, returned the AKRESULT: %d", wwiseResult);
			CRY_ASSERT(false);
		}
	}

	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> const temp("Init.bnk");
	AkOSChar const* szTemp = nullptr;
	CONVERT_CHAR_TO_OSCHAR(temp.c_str(), szTemp);

	wwiseResult = AK::SoundEngine::LoadBank(szTemp, AK_DEFAULT_POOL_ID, m_initBankId);

	if (wwiseResult != AK_Success)
	{
		g_AudioImplLogger_wwise.Log(eAudioLogType_Error, "Wwise failed to load Init.bnk, returned the AKRESULT: %d", wwiseResult);
		m_initBankId = AK_INVALID_BANK_ID;
		CRY_ASSERT(false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl_wwise::SetLanguage(char const* const szLanguage)
{
	if (szLanguage != nullptr)
	{
		m_localizedSoundBankFolder = PathUtil::GetGameFolder().c_str();
		m_localizedSoundBankFolder += CRY_NATIVE_PATH_SEPSTR;
		m_localizedSoundBankFolder += PathUtil::GetLocalizationFolder();
		m_localizedSoundBankFolder += CRY_NATIVE_PATH_SEPSTR;
		m_localizedSoundBankFolder += szLanguage;
		m_localizedSoundBankFolder += CRY_NATIVE_PATH_SEPSTR;
		m_localizedSoundBankFolder += WWISE_IMPL_DATA_ROOT;

		CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> temp(m_localizedSoundBankFolder);
		temp += CRY_NATIVE_PATH_SEPSTR;
		AkOSChar const* pTemp = nullptr;
		CONVERT_CHAR_TO_OSCHAR(temp.c_str(), pTemp);
		m_fileIOHandler.SetLanguageFolder(pTemp);
	}
}

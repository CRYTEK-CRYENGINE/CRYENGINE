// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Impl.h"
#include "CVars.h"
#include "FileIOHandler.h"
#include "GlobalData.h"
#include "AuxThread.h"
#include "Common.h"
#include "AuxBus.h"
#include "Event.h"
#include "EventInstance.h"
#include "Parameter.h"
#include "ParameterAdvanced.h"
#include "ParameterEnvironment.h"
#include "ParameterEnvironmentAdvanced.h"
#include "ParameterState.h"
#include "Listener.h"
#include "ListenerInfo.h"
#include "Object.h"
#include "SoundBank.h"
#include "State.h"
#include "Switch.h"

#include <ISettingConnection.h>
#include <FileInfo.h>
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/IProjectManager.h>

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	#include <Logger.h>
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	#include <DebugStyle.h>
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

#include <AK/SoundEngine/Common/AkSoundEngine.h>     // Sound engine
#include <AK/MusicEngine/Common/AkMusicEngine.h>     // Music Engine
#include <AK/SoundEngine/Common/AkMemoryMgr.h>       // Memory Manager
#include <AK/SoundEngine/Common/AkModule.h>          // Default memory and stream managers
#include <AK/SoundEngine/Common/AkStreamMgrModule.h> // AkStreamMgrModule
#include <AK/SoundEngine/Common/AkQueryParameters.h>
#include <AK/SoundEngine/Common/AkCallback.h>

// PluginFactories that can't be loaded on runtime
#include <AK/Plugin/AkMeterFXFactory.h>
#include <AK/Plugin/AkVorbisDecoderFactory.h>
#include <AK/Plugin/AkOpusDecoderFactory.h>

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	#include <AK/Comm/AkCommunication.h> // Communication between Wwise and the game (excluded in release build)
	#include <AK/Tools/Common/AkMonitorError.h>
	#include <AK/Tools/Common/AkPlatformFuncs.h>
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

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
	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "AK::AllocHook");
	return CryModuleMalloc(in_size);
}

void FreeHook(void* in_ptr)
{
	CryModuleFree(in_ptr);
}

void* VirtualAllocHook(void* in_pMemAddress, size_t in_size, DWORD in_dwAllocationType, DWORD in_dwProtect)
{
	return AllocHook(in_size);
}

void VirtualFreeHook(void* in_pMemAddress, size_t in_size, DWORD in_dwFreeType)
{
	FreeHook(in_pMemAddress);
}

#if CRY_PLATFORM_DURANGO
void* APUAllocHook(size_t in_size, unsigned int in_alignment)
{
	void* pAlloc = nullptr;

	#if defined(CRY_AUDIO_IMPL_WWISE_PROVIDE_SECONDARY_POOL)
	size_t const nSecondSize = CryAudio::Impl::Wwise::g_audioImplMemoryPoolSecondary.MemSize();

	if (nSecondSize > 0)
	{
		size_t const nAllocHandle = CryAudio::Impl::Wwise::g_audioImplMemoryPoolSecondary.Allocate<size_t>(in_size, in_alignment);

		if (nAllocHandle > 0)
		{
			pAlloc = CryAudio::Impl::Wwise::g_audioImplMemoryPoolSecondary.Resolve<void*>(nAllocHandle);
			CryAudio::Impl::Wwise::g_audioImplMemoryPoolSecondary.Item(nAllocHandle)->Lock();
		}
	}
	#endif  // PROVIDE_AUDIO_IMPL_SECONDARY_POOL

	CRY_ASSERT(pAlloc != nullptr);
	return pAlloc;
}

void APUFreeHook(void* in_pMemAddress)
{
	#if defined(CRY_AUDIO_IMPL_WWISE_PROVIDE_SECONDARY_POOL)
	size_t const nAllocHandle = CryAudio::Impl::Wwise::g_audioImplMemoryPoolSecondary.AddressToHandle(in_pMemAddress);
	CryAudio::Impl::Wwise::g_audioImplMemoryPoolSecondary.Free(nAllocHandle);
	#else
	CryFatalError("%s", "<Audio>: Called APUFreeHook without secondary pool");
	#endif  // PROVIDE_AUDIO_IMPL_SECONDARY_POOL
}
#endif  // CRY_PLATFORM_DURANGO
}

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
static void ErrorMonitorCallback(
	AK::Monitor::ErrorCode in_eErrorCode,   ///< Error code number value
	const AkOSChar* in_pszError,            ///< Message or error string to be displayed
	AK::Monitor::ErrorLevel in_eErrorLevel, ///< Specifies whether it should be displayed as a message or an error
	AkPlayingID in_playingID,               ///< Related Playing ID if applicable, AK_INVALID_PLAYING_ID otherwise
	AkGameObjectID in_gameObjID             ///< Related Game Object ID if applicable, AK_INVALID_GAME_OBJECT otherwise
	)
{
	char* szTemp = nullptr;
	CONVERT_OSCHAR_TO_CHAR(in_pszError, szTemp);

	CryAutoLock<CryCriticalSection> const lock(CryAudio::Impl::Wwise::g_cs);
	CryAudio::Impl::Wwise::CEventInstance const* const pEventInstance = stl::find_in_map(CryAudio::Impl::Wwise::g_playingIds, in_playingID, nullptr);
	CryAudio::Impl::Wwise::CObject const* const pObject = stl::find_in_map(CryAudio::Impl::Wwise::g_gameObjectIds, in_gameObjID, nullptr);
	char const* const szEventName = (pEventInstance != nullptr) ? pEventInstance->GetEvent().GetName() : "Unknown PlayingID";
	char const* const szObjectName = (pObject != nullptr) ? pObject->GetName() : "Unknown GameObjID";
	Cry::Audio::Log(
		((in_eErrorLevel& AK::Monitor::ErrorLevel_Error) != 0) ? CryAudio::ELogType::Error : CryAudio::ELogType::Comment,
		"<Wwise> %s | ErrorCode: %d | PlayingID: %u (%s) | GameObjID: %" PRISIZE_T " (%s)", szTemp, in_eErrorCode, in_playingID, szEventName, in_gameObjID, szObjectName);
}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
CAuxThread g_auxAudioThread;
std::map<AkUniqueID, float> g_maxAttenuations;

SPoolSizes g_poolSizes;
std::map<ContextId, SPoolSizes> g_contextPoolSizes;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
SPoolSizes g_debugPoolSizes;
EventInstances g_constructedEventInstances;
uint16 g_objectPoolSize = 0;
uint16 g_eventInstancePoolSize = 0;
std::vector<CListener*> g_constructedListeners;
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
void CountPoolSizes(XmlNodeRef const& node, SPoolSizes& poolSizes)
{
	uint16 numEvents = 0;
	node->getAttr(g_szEventsAttribute, numEvents);
	poolSizes.events += numEvents;

	uint16 numParameters = 0;
	node->getAttr(g_szParametersAttribute, numParameters);
	poolSizes.parameters += numParameters;

	uint16 numParametersAdvanced = 0;
	node->getAttr(g_szParametersAdvancedAttribute, numParametersAdvanced);
	poolSizes.parametersAdvanced += numParametersAdvanced;

	uint16 numParameterEnvironments = 0;
	node->getAttr(g_szParameterEnvironmentsAttribute, numParameterEnvironments);
	poolSizes.parameterEnvironments += numParameterEnvironments;

	uint16 numParameterEnvironmentsAdvanced = 0;
	node->getAttr(g_szParameterEnvironmentsAdvancedAttribute, numParameterEnvironmentsAdvanced);
	poolSizes.parameterEnvironmentsAdvanced += numParameterEnvironmentsAdvanced;

	uint16 numParameterStates = 0;
	node->getAttr(g_szParameterStatesAttribute, numParameterStates);
	poolSizes.parameterStates += numParameterStates;

	uint16 numStates = 0;
	node->getAttr(g_szStatesAttribute, numStates);
	poolSizes.states += numStates;

	uint16 numSwitches = 0;
	node->getAttr(g_szSwitchesAttribute, numSwitches);
	poolSizes.switches += numSwitches;

	uint16 numAuxBuses = 0;
	node->getAttr(g_szAuxBusesAttribute, numAuxBuses);
	poolSizes.auxBuses += numAuxBuses;

	uint16 numSoundBanks = 0;
	node->getAttr(g_szSoundBanksAttribute, numSoundBanks);
	poolSizes.soundBanks += numSoundBanks;
}

//////////////////////////////////////////////////////////////////////////
void AllocateMemoryPools(uint16 const objectPoolSize, uint16 const eventPoolSize)
{
	CObject::CreateAllocator(objectPoolSize);
	CEventInstance::CreateAllocator(eventPoolSize);
	CEvent::CreateAllocator(g_poolSizes.events);
	CParameter::CreateAllocator(g_poolSizes.parameters);
	CParameterAdvanced::CreateAllocator(g_poolSizes.parametersAdvanced);
	CParameterEnvironment::CreateAllocator(g_poolSizes.parameterEnvironments);
	CParameterEnvironmentAdvanced::CreateAllocator(g_poolSizes.parameterEnvironmentsAdvanced);
	CParameterState::CreateAllocator(g_poolSizes.parameterStates);
	CState::CreateAllocator(g_poolSizes.states);
	CSwitch::CreateAllocator(g_poolSizes.switches);
	CAuxBus::CreateAllocator(g_poolSizes.auxBuses);
	CSoundBank::CreateAllocator(g_poolSizes.soundBanks);
}

//////////////////////////////////////////////////////////////////////////
void FreeMemoryPools()
{
	CObject::FreeMemoryPool();
	CEventInstance::FreeMemoryPool();
	CEvent::FreeMemoryPool();
	CParameter::FreeMemoryPool();
	CParameterAdvanced::FreeMemoryPool();
	CParameterEnvironment::FreeMemoryPool();
	CParameterEnvironmentAdvanced::FreeMemoryPool();
	CParameterState::FreeMemoryPool();
	CState::FreeMemoryPool();
	CSwitch::FreeMemoryPool();
	CAuxBus::FreeMemoryPool();
	CSoundBank::FreeMemoryPool();
}

//////////////////////////////////////////////////////////////////////////
void LoadEventsMaxAttenuations(CryFixedStringT<MaxFilePathLength> const& soundbanksPath)
{
	g_maxAttenuations.clear();
	CryFixedStringT<MaxFilePathLength> const bankInfoPath = soundbanksPath + "/SoundbanksInfo.xml";
	XmlNodeRef const rootNode = GetISystem()->LoadXmlFromFile(bankInfoPath.c_str());

	if (rootNode.isValid())
	{
		XmlNodeRef const soundBanksNode = rootNode->findChild("SoundBanks");

		if (soundBanksNode.isValid())
		{
			int const numSoundBankNodes = soundBanksNode->getChildCount();

			for (int i = 0; i < numSoundBankNodes; ++i)
			{
				XmlNodeRef const soundBankNode = soundBanksNode->getChild(i);

				if (soundBankNode.isValid())
				{
					XmlNodeRef const includedEventsNode = soundBankNode->findChild("IncludedEvents");

					if (includedEventsNode.isValid())
					{
						int const numEventNodes = includedEventsNode->getChildCount();

						for (int j = 0; j < numEventNodes; ++j)
						{
							XmlNodeRef const eventNode = includedEventsNode->getChild(j);

							if (eventNode.isValid() && eventNode->haveAttr("MaxAttenuation"))
							{
								float maxAttenuation = 0.0f;
								eventNode->getAttr("MaxAttenuation", maxAttenuation);

								uint32 id = 0;
								eventNode->getAttr("Id", id);

								g_maxAttenuations[static_cast<AkUniqueID>(id)] = maxAttenuation;
							}
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
AkUInt32 IntToAkSpeakerSetup(int const value)
{
	AkUInt32 speakerSetup = 0;

	switch (value)
	{
	case 10:
		{
			speakerSetup = AK_SPEAKER_SETUP_MONO;
			break;
		}
	case 11:
		{
			speakerSetup = AK_SPEAKER_SETUP_1POINT1;
			break;
		}
	case 20:
		{
			speakerSetup = AK_SPEAKER_SETUP_STEREO;
			break;
		}
	case 21:
		{
			speakerSetup = AK_SPEAKER_SETUP_2POINT1;
			break;
		}
	case 30:
		{
			speakerSetup = AK_SPEAKER_SETUP_3STEREO;
			break;
		}
	case 31:
		{
			speakerSetup = AK_SPEAKER_SETUP_3POINT1;
			break;
		}
	case 40:
		{
			speakerSetup = AK_SPEAKER_SETUP_4;
			break;
		}
	case 41:
		{
			speakerSetup = AK_SPEAKER_SETUP_4POINT1;
			break;
		}
	case 50:
		{
			speakerSetup = AK_SPEAKER_SETUP_5;
			break;
		}
	case 51:
		{
			speakerSetup = AK_SPEAKER_SETUP_5POINT1;
			break;
		}
	case 60:
		{
			speakerSetup = AK_SPEAKER_SETUP_6;
			break;
		}
	case 61:
		{
			speakerSetup = AK_SPEAKER_SETUP_6POINT1;
			break;
		}
	case 70:
		{
			speakerSetup = AK_SPEAKER_SETUP_7;
			break;
		}
	case 71:
		{
			speakerSetup = AK_SPEAKER_SETUP_7POINT1;
			break;
		}
	default:
		{
			break;
		}
	}

	return speakerSetup;
}

///////////////////////////////////////////////////////////////////////////
CImpl::CImpl()
	: m_gameObjectId(2) // Start with id 2, because id 1 would get ignored when setting a parameter on all constructed objects.
	, m_initBankId(AK_INVALID_BANK_ID)
	, m_toBeReleased(false)
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	, m_bCommSystemInitialized(false)
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
#if defined(WWISE_USE_OCULUS)
	, m_pOculusSpatializerLibrary(nullptr)
#endif // WWISE_USE_OCULUS
{
	g_pImpl = this;
	m_regularSoundBankFolder = CRY_AUDIO_DATA_ROOT "/";
	m_regularSoundBankFolder += g_szImplFolderName;
	m_regularSoundBankFolder += "/";
	m_regularSoundBankFolder += g_szAssetsFolderName;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	m_name.Format(
		"%s - Build: %d",
		CRY_AUDIO_IMPL_WWISE_INFO_STRING,
		AK_WWISESDK_VERSION_BUILD);
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
}

///////////////////////////////////////////////////////////////////////////
void CImpl::Update()
{
	if (AK::SoundEngine::IsInitialized())
	{
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
		AKRESULT wwiseResult = AK_Fail;
		static int enableOutputCapture = 0;

		if (g_cvars.m_enableOutputCapture == 1 && enableOutputCapture == 0)
		{
			AkOSChar const* pOutputCaptureFileName = nullptr;
			CONVERT_CHAR_TO_OSCHAR("../wwise_audio_capture.wav", pOutputCaptureFileName);
			wwiseResult = AK::SoundEngine::StartOutputCapture(pOutputCaptureFileName);
			AKASSERT((wwiseResult == AK_Success) || !"StartOutputCapture failed!");
			enableOutputCapture = g_cvars.m_enableOutputCapture;
		}
		else if (g_cvars.m_enableOutputCapture == 0 && enableOutputCapture == 1)
		{
			wwiseResult = AK::SoundEngine::StopOutputCapture();
			AKASSERT((wwiseResult == AK_Success) || !"StopOutputCapture failed!");
			enableOutputCapture = g_cvars.m_enableOutputCapture;
		}
#endif    // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

		AK::SoundEngine::RenderAudio();
	}
}

///////////////////////////////////////////////////////////////////////////
bool CImpl::Init(uint16 const objectPoolSize)
{
	// If something fails so severely during initialization that we need to fall back to the NULL implementation
	// we will need to shut down what has been initialized so far. Therefore make sure to call Shutdown() before returning eARS_FAILURE!

	if (g_cvars.m_eventPoolSize < 1)
	{
		g_cvars.m_eventPoolSize = 1;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
		Cry::Audio::Log(ELogType::Warning, R"(Event pool size must be at least 1. Forcing the cvar "s_WwiseEventPoolSize" to 1!)");
#endif    // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
	}

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	g_constructedEventInstances.reserve(static_cast<size_t>(g_cvars.m_eventPoolSize));
	g_objectPoolSize = objectPoolSize;
	g_eventInstancePoolSize = static_cast<uint16>(g_cvars.m_eventPoolSize);
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	AllocateMemoryPools(objectPoolSize, static_cast<uint16>(g_cvars.m_eventPoolSize));

	AkMemSettings memSettings;
	memSettings.uMaxNumPools = 20;
	AKRESULT wwiseResult = AK::MemoryMgr::Init(&memSettings);

	if (wwiseResult != AK_Success)
	{
		ShutDown();

		return false;
	}

	AkMemPoolId const prepareMemPoolId = AK::MemoryMgr::CreatePool(nullptr, g_cvars.m_prepareEventMemoryPoolSize << 10, 16, AkMalloc, 16);

	if (prepareMemPoolId == AK_INVALID_POOL_ID)
	{
		ShutDown();

		return false;
	}

	wwiseResult = AK::MemoryMgr::SetPoolName(prepareMemPoolId, "PrepareEventMemoryPool");

	if (wwiseResult != AK_Success)
	{
		ShutDown();

		return false;
	}

	AkStreamMgrSettings streamSettings;
	AK::StreamMgr::GetDefaultSettings(streamSettings);
	streamSettings.uMemorySize = g_cvars.m_streamManagerMemoryPoolSize << 10; // 64 KiB is the default value!

	if (AK::StreamMgr::Create(streamSettings) == nullptr)
	{
		ShutDown();

		return false;
	}

	IThreadConfigManager* pThreadConfigMngr = gEnv->pThreadManager->GetThreadConfigManager();
	const SThreadConfig* pDeviceThread = pThreadConfigMngr->GetThreadConfig("Wwise_Device");
	const SThreadConfig* pBankManger = pThreadConfigMngr->GetThreadConfig("Wwise_BankManager");
	const SThreadConfig* pLEngineThread = pThreadConfigMngr->GetThreadConfig("Wwise_LEngine");
	const SThreadConfig* pMonitorThread = pThreadConfigMngr->GetThreadConfig("Wwise_Monitor");

	AkDeviceSettings deviceSettings;
	AK::StreamMgr::GetDefaultDeviceSettings(deviceSettings);
	deviceSettings.uIOMemorySize = g_cvars.m_streamDeviceMemoryPoolSize << 10; // 2 MiB is the default value!

	// Device thread settings
	if ((pDeviceThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_Affinity) != 0)
	{
		deviceSettings.threadProperties.dwAffinityMask = pDeviceThread->affinityFlag;
	}

	if ((pDeviceThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_Priority) != 0)
	{
#if CRY_PLATFORM_POSIX
		if (!((pDeviceThread->priority > 0) && (pDeviceThread->priority <= 99)))
		{
			CryWarning(VALIDATOR_MODULE_AUDIO, VALIDATOR_WARNING, "Thread 'Wwise_Device' requested priority %d when allowed range is 1 to 99. Applying default value of %d.", pDeviceThread->priority, deviceSettings.threadProperties.nPriority);
		}
		else
		{
			deviceSettings.threadProperties.nPriority = pDeviceThread->priority;
		}
#else
		deviceSettings.threadProperties.nPriority = pDeviceThread->priority;
#endif    // CRY_PLATFORM_POSIX
	}

	if ((pDeviceThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_StackSize) != 0)
	{
		deviceSettings.threadProperties.uStackSize = pDeviceThread->stackSizeBytes;
	}

	wwiseResult = m_fileIOHandler.Init(deviceSettings);

	if (wwiseResult != AK_Success)
	{
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
		Cry::Audio::Log(ELogType::Error, "m_fileIOHandler.Init() returned AKRESULT %d", wwiseResult);
#endif    // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

		ShutDown();

		return false;
	}

	CryFixedStringT<AK_MAX_PATH> temp = CRY_AUDIO_DATA_ROOT "/";
	temp.append(g_szImplFolderName);
	temp.append("/");
	temp.append(g_szAssetsFolderName);
	temp.append("/");
	AkOSChar const* szTemp = nullptr;
	CONVERT_CHAR_TO_OSCHAR(temp.c_str(), szTemp);
	m_fileIOHandler.SetBankPath(szTemp);

	AkInitSettings initSettings;
	AK::SoundEngine::GetDefaultInitSettings(initSettings);
	initSettings.uDefaultPoolSize = g_cvars.m_soundEngineDefaultMemoryPoolSize << 10;
	initSettings.uCommandQueueSize = g_cvars.m_commandQueueMemoryPoolSize << 10;
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	initSettings.uMonitorPoolSize = g_cvars.m_monitorMemoryPoolSize << 10;
	initSettings.uMonitorQueuePoolSize = g_cvars.m_monitorQueueMemoryPoolSize << 10;
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
	initSettings.uPrepareEventMemoryPoolID = prepareMemPoolId;
	initSettings.bEnableGameSyncPreparation = false;//TODO: ???
	g_cvars.m_panningRule = crymath::clamp(g_cvars.m_panningRule, 0, 1);
	initSettings.settingsMainOutput.ePanningRule = static_cast<AkPanningRule>(g_cvars.m_panningRule);
	initSettings.settingsMainOutput.channelConfig.SetStandard(IntToAkSpeakerSetup(g_cvars.m_channelConfig));

	initSettings.bUseLEngineThread = g_cvars.m_enableEventManagerThread > 0;
	initSettings.bUseSoundBankMgrThread = g_cvars.m_enableSoundBankManagerThread > 0;
	initSettings.uNumSamplesPerFrame = static_cast<AkUInt32>(g_cvars.m_numSamplesPerFrame);

	// We need this additional thread during bank unloading if the user decided to run Wwise without the EventManager thread.
	if (g_cvars.m_enableEventManagerThread == 0)
	{
		g_auxAudioThread.Init();
	}

	AkPlatformInitSettings platformInitSettings;
	AK::SoundEngine::GetDefaultPlatformInitSettings(platformInitSettings);
	platformInitSettings.uLEngineDefaultPoolSize = g_cvars.m_lowerEngineDefaultPoolSize << 10;
	platformInitSettings.uNumRefillsInVoice = static_cast<AkUInt16>(g_cvars.m_numRefillsInVoice);

	// Bank Manager thread settings
	if ((pBankManger->paramActivityFlag & SThreadConfig::eThreadParamFlag_Affinity) != 0)
	{
		platformInitSettings.threadBankManager.dwAffinityMask = pBankManger->affinityFlag;
	}

	if ((pBankManger->paramActivityFlag & SThreadConfig::eThreadParamFlag_Priority) != 0)
	{
#if CRY_PLATFORM_POSIX
		if (!((pBankManger->priority > 0) && (pBankManger->priority <= 99)))
		{
			CryWarning(VALIDATOR_MODULE_AUDIO, VALIDATOR_WARNING, "Thread 'Wwise_BankManager' requested priority %d when allowed range is 1 to 99. Applying default value of %d.", pBankManger->priority, platformInitSettings.threadBankManager.nPriority);
		}
		else
		{
			platformInitSettings.threadBankManager.nPriority = pBankManger->priority;
		}
#else
		platformInitSettings.threadBankManager.nPriority = pBankManger->priority;
#endif    // CRY_PLATFORM_POSIX
	}

	if ((pBankManger->paramActivityFlag & SThreadConfig::eThreadParamFlag_StackSize) != 0)
	{
		platformInitSettings.threadBankManager.uStackSize = pBankManger->stackSizeBytes;
	}

	// LEngine thread settings
	if ((pLEngineThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_Affinity) != 0)
	{
		platformInitSettings.threadLEngine.dwAffinityMask = pLEngineThread->affinityFlag;
	}

	if ((pLEngineThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_Priority) != 0)
	{
#if CRY_PLATFORM_POSIX
		if (!((pLEngineThread->priority > 0) && (pLEngineThread->priority <= 99)))
		{
			CryWarning(VALIDATOR_MODULE_AUDIO, VALIDATOR_WARNING, "Thread 'Wwise_LEngine' requested priority %d when allowed range is 1 to 99. Applying default value of %d.", pLEngineThread->priority, platformInitSettings.threadLEngine.nPriority);
		}
		else
		{
			platformInitSettings.threadLEngine.nPriority = pLEngineThread->priority;
		}
#else
		platformInitSettings.threadLEngine.nPriority = pLEngineThread->priority;
#endif    // CRY_PLATFORM_POSIX
	}

	if ((pLEngineThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_StackSize) != 0)
	{
		platformInitSettings.threadLEngine.uStackSize = pLEngineThread->stackSizeBytes;
	}

	// Monitor thread settings
	if ((pMonitorThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_Affinity) != 0)
	{
		platformInitSettings.threadMonitor.dwAffinityMask = pMonitorThread->affinityFlag;
	}

	if ((pMonitorThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_Priority) != 0)
	{
#if CRY_PLATFORM_POSIX
		if (!((pMonitorThread->priority > 0) && (pMonitorThread->priority <= 99)))
		{
			CryWarning(VALIDATOR_MODULE_AUDIO, VALIDATOR_WARNING, "Thread 'Wwise_Monitor' requested priority %d when allowed range is 1 to 99. Applying default value of %d.", pMonitorThread->priority, platformInitSettings.threadMonitor.nPriority);
		}
		else
		{
			platformInitSettings.threadMonitor.nPriority = pMonitorThread->priority;
		}
#else
		platformInitSettings.threadMonitor.nPriority = pMonitorThread->priority;
#endif    // CRY_PLATFORM_POSIX
	}

	if ((pMonitorThread->paramActivityFlag & SThreadConfig::eThreadParamFlag_StackSize) != 0)
	{
		platformInitSettings.threadMonitor.uStackSize = pMonitorThread->stackSizeBytes;
	}

	wwiseResult = AK::SoundEngine::Init(&initSettings, &platformInitSettings);

	if (wwiseResult != AK_Success)
	{
		ShutDown();

		return false;
	}

	AkMusicSettings musicSettings;
	AK::MusicEngine::GetDefaultInitSettings(musicSettings);

	wwiseResult = AK::MusicEngine::Init(&musicSettings);

	if (wwiseResult != AK_Success)
	{
		ShutDown();

		return false;
	}

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	if (g_cvars.m_enableCommSystem == 1)
	{
		m_bCommSystemInitialized = true;
		AkCommSettings commSettings;
		AK::Comm::GetDefaultInitSettings(commSettings);

		wwiseResult = AK::Comm::Init(commSettings);

		if (wwiseResult != AK_Success)
		{
			m_bCommSystemInitialized = false;
		}

		wwiseResult = AK::Monitor::SetLocalOutput(AK::Monitor::ErrorLevel_All, ErrorMonitorCallback);

		if (wwiseResult != AK_Success)
		{
			AK::Comm::Term();
			m_bCommSystemInitialized = false;
		}
	}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	REINST("Register Global Callback")

	// AK::SoundEngine::RegisterGlobalCallback(GlobalCallback);
	// Load Init.bnk before making the system available to the users
	temp = "Init.bnk";
	CONVERT_CHAR_TO_OSCHAR(temp.c_str(), szTemp);

	wwiseResult = AK::SoundEngine::LoadBank(szTemp, AK_DEFAULT_POOL_ID, m_initBankId);

	if (wwiseResult != AK_Success)
	{
		// This does not qualify for a fallback to the NULL implementation!
		m_initBankId = AK_INVALID_BANK_ID;
	}

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	GetInitBankSize();
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	LoadEventsMaxAttenuations(m_regularSoundBankFolder);

	return true;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::ShutDown()
{
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	if (m_bCommSystemInitialized)
	{
		AK::Comm::Term();

		AK::Monitor::SetLocalOutput(0, nullptr);
		m_bCommSystemInitialized = false;
	}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	AK::MusicEngine::Term();

	if (AK::SoundEngine::IsInitialized())
	{
		if (g_cvars.m_enableEventManagerThread > 0)
		{
			AK::SoundEngine::ClearBanks();
		}
		else
		{
			SignalAuxAudioThread();
			AK::SoundEngine::ClearBanks();
			g_auxAudioThread.SignalStopWork();
		}

		REINST("Unregister global callback")
		// AK::SoundEngine::UnregisterGlobalCallback(GlobalCallback);

		AK::SoundEngine::Query::AkGameObjectsList objectList;
		AK::SoundEngine::Query::GetActiveGameObjects(objectList);
		AkUInt32 const length = objectList.Length();

		for (AkUInt32 i = 0; i < length; ++i)
		{
			// This call requires at least Wwise v2017.1.0.
			AK::SoundEngine::CancelEventCallbackGameObject(objectList[i]);
		}

		objectList.Term();
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
		AK::MemoryMgr::DestroyPool(0);
		AK::MemoryMgr::Term();
	}

#if defined(WWISE_USE_OCULUS)
	if (m_pOculusSpatializerLibrary != nullptr)
	{
		CryFreeLibrary(m_pOculusSpatializerLibrary);
		m_pOculusSpatializerLibrary = nullptr;
	}
#endif  // WWISE_USE_OCULUS
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnBeforeRelease()
{
	m_toBeReleased = true;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::Release()
{
	delete this;
	g_pImpl = nullptr;
	g_cvars.UnregisterVariables();

	FreeMemoryPools();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetLibraryData(XmlNodeRef const& node, ContextId const contextId)
{
	if (contextId == GlobalContextId)
	{
		CountPoolSizes(node, g_poolSizes);
	}
	else
	{
		CountPoolSizes(node, g_contextPoolSizes[contextId]);
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnBeforeLibraryDataChanged()
{
	ZeroStruct(g_poolSizes);
	g_contextPoolSizes.clear();

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	ZeroStruct(g_debugPoolSizes);
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnAfterLibraryDataChanged(int const poolAllocationMode)
{
	if (!g_contextPoolSizes.empty())
	{
		if (poolAllocationMode <= 0)
		{
			for (auto const& poolSizePair : g_contextPoolSizes)
			{
				SPoolSizes const& iterPoolSizes = g_contextPoolSizes[poolSizePair.first];

				g_poolSizes.events += iterPoolSizes.events;
				g_poolSizes.parameters += iterPoolSizes.parameters;
				g_poolSizes.parametersAdvanced += iterPoolSizes.parametersAdvanced;
				g_poolSizes.parameterEnvironments += iterPoolSizes.parameterEnvironments;
				g_poolSizes.parameterEnvironmentsAdvanced += iterPoolSizes.parameterEnvironmentsAdvanced;
				g_poolSizes.parameterStates += iterPoolSizes.parameterStates;
				g_poolSizes.states += iterPoolSizes.states;
				g_poolSizes.switches += iterPoolSizes.switches;
				g_poolSizes.auxBuses += iterPoolSizes.auxBuses;
				g_poolSizes.soundBanks += iterPoolSizes.soundBanks;
			}
		}
		else
		{
			SPoolSizes maxContextPoolSizes;

			for (auto const& poolSizePair : g_contextPoolSizes)
			{
				SPoolSizes const& iterPoolSizes = g_contextPoolSizes[poolSizePair.first];

				maxContextPoolSizes.events = std::max(maxContextPoolSizes.events, iterPoolSizes.events);
				maxContextPoolSizes.parameters = std::max(maxContextPoolSizes.parameters, iterPoolSizes.parameters);
				maxContextPoolSizes.parametersAdvanced = std::max(maxContextPoolSizes.parametersAdvanced, iterPoolSizes.parametersAdvanced);
				maxContextPoolSizes.parameterEnvironments = std::max(maxContextPoolSizes.parameterEnvironments, iterPoolSizes.parameterEnvironments);
				maxContextPoolSizes.parameterEnvironmentsAdvanced = std::max(maxContextPoolSizes.parameterEnvironmentsAdvanced, iterPoolSizes.parameterEnvironmentsAdvanced);
				maxContextPoolSizes.parameterStates = std::max(maxContextPoolSizes.parameterStates, iterPoolSizes.parameterStates);
				maxContextPoolSizes.states = std::max(maxContextPoolSizes.states, iterPoolSizes.states);
				maxContextPoolSizes.switches = std::max(maxContextPoolSizes.switches, iterPoolSizes.switches);
				maxContextPoolSizes.auxBuses = std::max(maxContextPoolSizes.auxBuses, iterPoolSizes.auxBuses);
				maxContextPoolSizes.soundBanks = std::max(maxContextPoolSizes.soundBanks, iterPoolSizes.soundBanks);
			}

			g_poolSizes.events += maxContextPoolSizes.events;
			g_poolSizes.parameters += maxContextPoolSizes.parameters;
			g_poolSizes.parametersAdvanced += maxContextPoolSizes.parametersAdvanced;
			g_poolSizes.parameterEnvironments += maxContextPoolSizes.parameterEnvironments;
			g_poolSizes.parameterEnvironmentsAdvanced += maxContextPoolSizes.parameterEnvironmentsAdvanced;
			g_poolSizes.parameterStates += maxContextPoolSizes.parameterStates;
			g_poolSizes.states += maxContextPoolSizes.states;
			g_poolSizes.switches += maxContextPoolSizes.switches;
			g_poolSizes.auxBuses += maxContextPoolSizes.auxBuses;
			g_poolSizes.soundBanks += maxContextPoolSizes.soundBanks;
		}
	}

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	// Used to hide pools without allocations in debug draw.
	g_debugPoolSizes = g_poolSizes;
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	g_poolSizes.events = std::max<uint16>(1, g_poolSizes.events);
	g_poolSizes.parameters = std::max<uint16>(1, g_poolSizes.parameters);
	g_poolSizes.parametersAdvanced = std::max<uint16>(1, g_poolSizes.parametersAdvanced);
	g_poolSizes.parameterEnvironments = std::max<uint16>(1, g_poolSizes.parameterEnvironments);
	g_poolSizes.parameterEnvironmentsAdvanced = std::max<uint16>(1, g_poolSizes.parameterEnvironmentsAdvanced);
	g_poolSizes.parameterStates = std::max<uint16>(1, g_poolSizes.parameterStates);
	g_poolSizes.states = std::max<uint16>(1, g_poolSizes.states);
	g_poolSizes.switches = std::max<uint16>(1, g_poolSizes.switches);
	g_poolSizes.auxBuses = std::max<uint16>(1, g_poolSizes.auxBuses);
	g_poolSizes.soundBanks = std::max<uint16>(1, g_poolSizes.soundBanks);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::OnLoseFocus()
{
	// With Wwise we drive this via events.
}

///////////////////////////////////////////////////////////////////////////
void CImpl::OnGetFocus()
{
	// With Wwise we drive this via events.
}

///////////////////////////////////////////////////////////////////////////
void CImpl::MuteAll()
{
	// With Wwise we drive this via events.
}

///////////////////////////////////////////////////////////////////////////
void CImpl::UnmuteAll()
{
	// With Wwise we drive this via events.
}

///////////////////////////////////////////////////////////////////////////
void CImpl::PauseAll()
{
	// With Wwise we drive this via events.
}

///////////////////////////////////////////////////////////////////////////
void CImpl::ResumeAll()
{
	// With Wwise we drive this via events.
}

///////////////////////////////////////////////////////////////////////////
void CImpl::StopAllSounds()
{
	AK::SoundEngine::StopAll();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::RegisterInMemoryFile(SFileInfo* const pFileInfo)
{
	if (pFileInfo != nullptr)
	{
		auto const pFileData = static_cast<CSoundBank*>(pFileInfo->pImplData);

		if (pFileData != nullptr)
		{
			AkBankID bankId = AK_INVALID_BANK_ID;

			AKRESULT const wwiseResult = AK::SoundEngine::LoadBank(
				pFileInfo->pFileData,
				static_cast<AkUInt32>(pFileInfo->size),
				bankId);

			if (CRY_AUDIO_IMPL_WWISE_IS_OK(wwiseResult))
			{
				pFileData->bankId = bankId;
			}
			else
			{
				pFileData->bankId = AK_INVALID_BANK_ID;
			}
		}
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Error, "Invalid AudioFileEntryData passed to the Wwise implementation of %s", __FUNCTION__);
		}
#endif    // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::UnregisterInMemoryFile(SFileInfo* const pFileInfo)
{
	if (pFileInfo != nullptr)
	{
		auto const pFileData = static_cast<CSoundBank*>(pFileInfo->pImplData);

		if (pFileData != nullptr)
		{
			// If the EventManager thread has been disabled the synchronous UnloadBank version will get stuck.
			if (g_cvars.m_enableEventManagerThread > 0)
			{
				AK::SoundEngine::UnloadBank(pFileData->bankId, pFileInfo->pFileData);
			}
			else
			{
				SignalAuxAudioThread();
				AK::SoundEngine::UnloadBank(pFileData->bankId, pFileInfo->pFileData);
				WaitForAuxAudioThread();
			}
		}
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Error, "Invalid AudioFileEntryData passed to the Wwise implementation of %s", __FUNCTION__);
		}
#endif    // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
bool CImpl::ConstructFile(XmlNodeRef const& rootNode, SFileInfo* const pFileInfo)
{
	bool isConstructed = false;

	if ((_stricmp(rootNode->getTag(), g_szFileTag) == 0) && (pFileInfo != nullptr))
	{
		char const* const szFileName = rootNode->getAttr(g_szNameAttribute);

		if ((szFileName != nullptr) && (szFileName[0] != '\0'))
		{
			char const* const szLocalized = rootNode->getAttr(g_szLocalizedAttribute);
			pFileInfo->isLocalized = (szLocalized != nullptr) && (_stricmp(szLocalized, g_szTrueValue) == 0);
			cry_strcpy(pFileInfo->fileName, szFileName);
			CryFixedStringT<MaxFilePathLength> const filePath = (pFileInfo->isLocalized ? m_localizedSoundBankFolder : m_regularSoundBankFolder) + "/" + szFileName;
			cry_strcpy(pFileInfo->filePath, filePath.c_str());
			pFileInfo->memoryBlockAlignment = AK_BANK_PLATFORM_DATA_ALIGNMENT;

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CSoundBank");
			pFileInfo->pImplData = new CSoundBank();
			isConstructed = true;
		}
	}

	return isConstructed;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructFile(IFile* const pIFile)
{
	delete pIFile;
}

//////////////////////////////////////////////////////////////////////////
char const* CImpl::GetFileLocation(IFile* const pIFile) const
{
	return m_localizedSoundBankFolder.c_str();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GetInfo(SImplInfo& implInfo) const
{
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	cry_strcpy(implInfo.name, m_name.c_str());
#else
	cry_fixed_size_strcpy(implInfo.name, g_implNameInRelease);
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
	cry_strcpy(implInfo.folderName, g_szImplFolderName, strlen(g_szImplFolderName));
}

///////////////////////////////////////////////////////////////////////////
IObject* CImpl::ConstructObject(CTransformation const& transformation, IListeners const& listeners, char const* const szName /*= nullptr*/)
{
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	AK::SoundEngine::RegisterGameObj(m_gameObjectId, szName);
#else
	AK::SoundEngine::RegisterGameObj(m_gameObjectId);
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	ListenerInfos listenerInfos;
	int const numListeners = listeners.size();

	for (int i = 0; i < numListeners; ++i)
	{
		listenerInfos.emplace_back(static_cast<CListener*>(listeners[i]), 0.0f);
	}

	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CObject");
	auto const pObject = new CObject(m_gameObjectId++, transformation, listenerInfos, szName);

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	{
		CryAutoLock<CryCriticalSection> const lock(CryAudio::Impl::Wwise::g_cs);
		g_gameObjectIds[pObject->GetId()] = pObject;
	}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	return static_cast<IObject*>(pObject);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructObject(IObject const* const pIObject)
{
	auto const pObject = static_cast<CObject const*>(pIObject);
	AkGameObjectID const objectID = pObject->GetId();
	AK::SoundEngine::UnregisterGameObj(objectID);

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	{
		CryAutoLock<CryCriticalSection> const lock(CryAudio::Impl::Wwise::g_cs);
		g_gameObjectIds.erase(pObject->GetId());
	}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	delete pObject;
}

///////////////////////////////////////////////////////////////////////////
IListener* CImpl::ConstructListener(CTransformation const& transformation, char const* const szName)
{
	IListener* pIListener = nullptr;

	AK::SoundEngine::RegisterGameObj(m_gameObjectId);
	AK::SoundEngine::SetDefaultListeners(&m_gameObjectId, 1);

	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CListener");
	auto const pListener = new CListener(transformation, m_gameObjectId++);

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	pListener->SetName(szName);
	g_constructedListeners.push_back(pListener);
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	pIListener = static_cast<IListener*>(pListener);

	return pIListener;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructListener(IListener* const pIListener)
{
	auto const pListener = static_cast<CListener*>(pIListener);
	AK::SoundEngine::UnregisterGameObj(pListener->GetId());

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	auto iter(g_constructedListeners.begin());
	auto const iterEnd(g_constructedListeners.cend());

	for (; iter != iterEnd; ++iter)
	{
		if ((*iter) == pListener)
		{
			if (iter != (iterEnd - 1))
			{
				(*iter) = g_constructedListeners.back();
			}

			g_constructedListeners.pop_back();
			break;
		}
	}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	delete pListener;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GamepadConnected(DeviceId const deviceUniqueID)
{
	CRY_ASSERT(m_mapInputDevices.find(deviceUniqueID) == m_mapInputDevices.end()); // Mustn't exist yet!
	AkOutputSettings settings("Wwise_Motion", static_cast<AkUniqueID>(deviceUniqueID));
	AkOutputDeviceID deviceID = AK_INVALID_OUTPUT_DEVICE_ID;
	AKRESULT const wwiseResult = AK::SoundEngine::AddOutput(settings, &deviceID);

	if (CRY_AUDIO_IMPL_WWISE_IS_OK(wwiseResult))
	{
		m_mapInputDevices[deviceUniqueID] = { true, deviceID };
	}
	else
	{
		m_mapInputDevices[deviceUniqueID] = { false, deviceID };
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GamepadDisconnected(DeviceId const deviceUniqueID)
{
	AudioInputDevices::const_iterator const Iter(m_mapInputDevices.find(deviceUniqueID));

	// If AK::SoundEngine::AddOutput failed, then the device is not in the list
	if (Iter != m_mapInputDevices.end())
	{
		if (Iter->second.akIsActiveOutputDevice)
		{
			AkOutputDeviceID const deviceID = Iter->second.akOutputDeviceID;
			AK::SoundEngine::RemoveOutput(deviceID);
		}
		m_mapInputDevices.erase(Iter);
	}
}

///////////////////////////////////////////////////////////////////////////
ITriggerConnection* CImpl::ConstructTriggerConnection(XmlNodeRef const& rootNode, float& radius)
{
	ITriggerConnection* pITriggerConnection = nullptr;

	if (_stricmp(rootNode->getTag(), g_szEventTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		AkUniqueID const uniqueId = AK::SoundEngine::GetIDFromString(szName); // Does not check if the string represents an event!

		if (uniqueId != AK_INVALID_UNIQUE_ID)
		{
			float maxAttenuation = 0.0f;
			auto const attenuationPair = g_maxAttenuations.find(uniqueId);

			if (attenuationPair != g_maxAttenuations.end())
			{
				maxAttenuation = attenuationPair->second;
			}

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CEvent");
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
			pITriggerConnection = static_cast<ITriggerConnection*>(new CEvent(uniqueId, maxAttenuation, szName));
			radius = maxAttenuation;
#else
			pITriggerConnection = static_cast<ITriggerConnection*>(new CEvent(uniqueId, maxAttenuation));
#endif      // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
		}
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Invalid Wwise event name %s", szName);
		}
#endif      // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
	}
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Wwise tag: %s", rootNode->getTag());
	}
#endif      // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	return pITriggerConnection;
}

//////////////////////////////////////////////////////////////////////////
ITriggerConnection* CImpl::ConstructTriggerConnection(ITriggerInfo const* const pITriggerInfo)
{
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	ITriggerConnection* pITriggerConnection = nullptr;
	auto const pTriggerInfo = static_cast<STriggerInfo const*>(pITriggerInfo);

	if (pTriggerInfo != nullptr)
	{
		char const* const szName = pTriggerInfo->name;
		AkUniqueID const uniqueId = AK::SoundEngine::GetIDFromString(szName);

		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CEvent");
		pITriggerConnection = static_cast<ITriggerConnection*>(new CEvent(uniqueId, 0.0f, szName));
	}

	return pITriggerConnection;
#else
	return nullptr;
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructTriggerConnection(ITriggerConnection* const pITriggerConnection)
{
	auto const pEvent = static_cast<CEvent*>(pITriggerConnection);
	pEvent->SetFlag(EEventFlags::ToBeDestructed);

	if (pEvent->CanBeDestructed())
	{
		delete pEvent;
	}
}

///////////////////////////////////////////////////////////////////////////
IParameterConnection* CImpl::ConstructParameterConnection(XmlNodeRef const& rootNode)
{
	IParameterConnection* pIParameterConnection = nullptr;

	if (_stricmp(rootNode->getTag(), g_szParameterTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		auto const rtpcId = static_cast<AkRtpcID>(AK::SoundEngine::GetIDFromString(szName));

		if (rtpcId != AK_INVALID_RTPC_ID)
		{
			bool isAdvanced = false;

			float multiplier = g_defaultParamMultiplier;
			float shift = g_defaultParamShift;

			isAdvanced |= rootNode->getAttr(g_szMutiplierAttribute, multiplier);
			isAdvanced |= rootNode->getAttr(g_szShiftAttribute, shift);

			if (isAdvanced)
			{
				MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CParameterAdvanced");

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
				pIParameterConnection = static_cast<IParameterConnection*>(new CParameterAdvanced(rtpcId, multiplier, shift, szName));
#else
				pIParameterConnection = static_cast<IParameterConnection*>(new CParameterAdvanced(rtpcId, multiplier, shift));
#endif        // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
			}
			else
			{
				MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CParameter");

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
				pIParameterConnection = static_cast<IParameterConnection*>(new CParameter(rtpcId, szName));
#else
				pIParameterConnection = static_cast<IParameterConnection*>(new CParameter(rtpcId));
#endif        // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
			}

		}
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Invalid Wwise parameter name %s", szName);
		}
#endif      // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
	}
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Wwise tag %s", rootNode->getTag());
	}
#endif      // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	return pIParameterConnection;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructParameterConnection(IParameterConnection const* const pIParameterConnection)
{
	delete pIParameterConnection;
}

///////////////////////////////////////////////////////////////////////////
ISwitchStateConnection* CImpl::ConstructSwitchStateConnection(XmlNodeRef const& rootNode)
{
	ISwitchStateConnection* pISwitchStateConnection = nullptr;

	char const* const szTag = rootNode->getTag();

	if (_stricmp(szTag, g_szStateGroupTag) == 0)
	{
		char const* const szStateGroupName = rootNode->getAttr(g_szNameAttribute);

		if ((szStateGroupName != nullptr) && (szStateGroupName[0] != 0) && (rootNode->getChildCount() == 1))
		{
			XmlNodeRef const valueNode(rootNode->getChild(0));

			if (valueNode.isValid() && (_stricmp(valueNode->getTag(), g_szValueTag) == 0))
			{
				char const* const szStateName = valueNode->getAttr(g_szNameAttribute);

				if ((szStateName != nullptr) && (szStateName[0] != 0))
				{
					AkUInt32 const stateGroupId = AK::SoundEngine::GetIDFromString(szStateGroupName);
					AkUInt32 const stateId = AK::SoundEngine::GetIDFromString(szStateName);

					MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CState");

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
					pISwitchStateConnection = static_cast<ISwitchStateConnection*>(new CState(stateGroupId, stateId, szStateGroupName, szStateName));
#else
					pISwitchStateConnection = static_cast<ISwitchStateConnection*>(new CState(stateGroupId, stateId));
#endif          // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
				}
			}
		}
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(
				ELogType::Warning,
				"A Wwise StateGroup %s inside SwitchState needs to have exactly one WwiseValue.",
				szStateGroupName);
		}
#endif      // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
	}
	else if (_stricmp(szTag, g_szSwitchGroupTag) == 0)
	{
		char const* const szSwitchGroupName = rootNode->getAttr(g_szNameAttribute);

		if ((szSwitchGroupName != nullptr) && (szSwitchGroupName[0] != 0) && (rootNode->getChildCount() == 1))
		{
			XmlNodeRef const valueNode(rootNode->getChild(0));

			if (valueNode.isValid() && (_stricmp(valueNode->getTag(), g_szValueTag) == 0))
			{
				char const* const szSwitchName = valueNode->getAttr(g_szNameAttribute);

				if ((szSwitchName != nullptr) && (szSwitchName[0] != 0))
				{
					AkUInt32 const switchGroupId = AK::SoundEngine::GetIDFromString(szSwitchGroupName);
					AkUInt32 const switchId = AK::SoundEngine::GetIDFromString(szSwitchName);

					MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CSwitch");

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
					pISwitchStateConnection = static_cast<ISwitchStateConnection*>(new CSwitch(switchGroupId, switchId, szSwitchGroupName, szSwitchName));
#else
					pISwitchStateConnection = static_cast<ISwitchStateConnection*>(new CSwitch(switchGroupId, switchId));
#endif          // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
				}
			}
		}
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(
				ELogType::Warning,
				"A Wwise SwitchGroup %s inside SwitchState needs to have exactly one WwiseValue.",
				szSwitchGroupName);
		}
#endif          // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
	}
	else if (_stricmp(szTag, g_szParameterTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		auto const rtpcId = static_cast<AkRtpcID>(AK::SoundEngine::GetIDFromString(szName));

		if (rtpcId != AK_INVALID_RTPC_ID)
		{
			float value = g_defaultStateValue;
			rootNode->getAttr(g_szValueAttribute, value);

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CParameterState");

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
			pISwitchStateConnection = static_cast<ISwitchStateConnection*>(new CParameterState(rtpcId, value, szName));
#else
			pISwitchStateConnection = static_cast<ISwitchStateConnection*>(new CParameterState(rtpcId, value));
#endif        // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
		}
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Invalid Wwise parameter name %s", szName);
		}
#endif          // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
	}
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Wwise tag: %s", szTag);
	}
#endif          // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	return pISwitchStateConnection;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructSwitchStateConnection(ISwitchStateConnection const* const pISwitchStateConnection)
{
	delete pISwitchStateConnection;
}

///////////////////////////////////////////////////////////////////////////
IEnvironmentConnection* CImpl::ConstructEnvironmentConnection(XmlNodeRef const& rootNode)
{
	IEnvironmentConnection* pIEnvironmentConnection = nullptr;

	char const* const szTag = rootNode->getTag();

	if (_stricmp(szTag, g_szAuxBusTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		AkUniqueID const busId = AK::SoundEngine::GetIDFromString(szName);

		if (busId != AK_INVALID_AUX_ID)
		{
			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CAuxBus");

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
			pIEnvironmentConnection = static_cast<IEnvironmentConnection*>(new CAuxBus(static_cast<AkAuxBusID>(busId), szName));
#else
			pIEnvironmentConnection = static_cast<IEnvironmentConnection*>(new CAuxBus(static_cast<AkAuxBusID>(busId)));
#endif      // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
		}
		else
		{
			CRY_ASSERT(false); // Unknown AuxBus
		}
	}
	else if (_stricmp(szTag, g_szParameterTag) == 0)
	{
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		auto const rtpcId = static_cast<AkRtpcID>(AK::SoundEngine::GetIDFromString(szName));

		if (rtpcId != AK_INVALID_RTPC_ID)
		{
			bool isAdvanced = false;

			float multiplier = g_defaultParamMultiplier;
			float shift = g_defaultParamShift;

			isAdvanced |= rootNode->getAttr(g_szMutiplierAttribute, multiplier);
			isAdvanced |= rootNode->getAttr(g_szShiftAttribute, shift);

			if (isAdvanced)
			{
				MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CParameterEnvironmentAdvanced");

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
				pIEnvironmentConnection = static_cast<IEnvironmentConnection*>(new CParameterEnvironmentAdvanced(rtpcId, multiplier, shift, szName));
#else
				pIEnvironmentConnection = static_cast<IEnvironmentConnection*>(new CParameterEnvironmentAdvanced(rtpcId, multiplier, shift));
#endif        // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
			}
			else
			{
				MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CParameterEnvironment");

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
				pIEnvironmentConnection = static_cast<IEnvironmentConnection*>(new CParameterEnvironment(rtpcId, szName));
#else
				pIEnvironmentConnection = static_cast<IEnvironmentConnection*>(new CParameterEnvironment(rtpcId));
#endif        // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
			}
		}
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Invalid Wwise parameter name %s", szName);
		}
#endif      // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
	}
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Wwise tag: %s", szTag);
	}
#endif      // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	return pIEnvironmentConnection;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructEnvironmentConnection(IEnvironmentConnection const* const pIEnvironmentConnection)
{
	delete pIEnvironmentConnection;
}

//////////////////////////////////////////////////////////////////////////
ISettingConnection* CImpl::ConstructSettingConnection(XmlNodeRef const& rootNode)
{
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructSettingConnection(ISettingConnection const* const pISettingConnection)
{
	delete pISettingConnection;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SignalAuxAudioThread()
{
	g_auxAudioThread.m_lock.Lock();
	g_auxAudioThread.m_threadState = CAuxThread::EAuxThreadState::Start;
	g_auxAudioThread.m_lock.Unlock();
	g_auxAudioThread.m_sem.Notify();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::WaitForAuxAudioThread()
{
	g_auxAudioThread.m_lock.Lock();
	g_auxAudioThread.m_threadState = CAuxThread::EAuxThreadState::Stop;

	// Wait until the AuxWwiseAudioThread is actually waiting again and not processing any work.
	while (g_auxAudioThread.m_threadState != CAuxThread::EAuxThreadState::Wait)
	{
		g_auxAudioThread.m_sem.Wait(g_auxAudioThread.m_lock);
	}

	g_auxAudioThread.m_lock.Unlock();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnRefresh()
{
	if (m_initBankId != AK_INVALID_BANK_ID)
	{
		if (g_cvars.m_enableEventManagerThread > 0)
		{
			AK::SoundEngine::UnloadBank(m_initBankId, nullptr);
		}
		else
		{
			SignalAuxAudioThread();
			AK::SoundEngine::UnloadBank(m_initBankId, nullptr);
			WaitForAuxAudioThread();
		}
	}

	CryFixedStringT<MaxFilePathLength> const temp("Init.bnk");
	AkOSChar const* szTemp = nullptr;
	CONVERT_CHAR_TO_OSCHAR(temp.c_str(), szTemp);

	AKRESULT const wwiseResult = AK::SoundEngine::LoadBank(szTemp, AK_DEFAULT_POOL_ID, m_initBankId);

	if (wwiseResult != AK_Success)
	{
		m_initBankId = AK_INVALID_BANK_ID;
	}

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	GetInitBankSize();
	g_debugStates.clear();
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	LoadEventsMaxAttenuations(m_regularSoundBankFolder);
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetLanguage(char const* const szLanguage)
{
	if (szLanguage != nullptr)
	{
		m_localizedSoundBankFolder = PathUtil::GetLocalizationFolder().c_str();
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += szLanguage;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += CRY_AUDIO_DATA_ROOT;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += g_szImplFolderName;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += g_szAssetsFolderName;

		CryFixedStringT<MaxFilePathLength> temp(m_localizedSoundBankFolder);
		temp += "/";
		AkOSChar const* pTemp = nullptr;
		CONVERT_CHAR_TO_OSCHAR(temp.c_str(), pTemp);
		m_fileIOHandler.SetLanguageFolder(pTemp);
	}
}

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
CEventInstance* CImpl::ConstructEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event, CObject const& object)
{
	event.IncrementNumInstances();
	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CEventInstance");

	auto const pEventInstance = new CEventInstance(triggerInstanceId, event, object);
	g_constructedEventInstances.push_back(pEventInstance);

	return pEventInstance;
}
#else
//////////////////////////////////////////////////////////////////////////
CEventInstance* CImpl::ConstructEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event)
{
	event.IncrementNumInstances();

	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CEventInstance");
	return new CEventInstance(triggerInstanceId, event);
}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructEventInstance(CEventInstance const* const pEventInstance)
{
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	CRY_ASSERT_MESSAGE(pEventInstance != nullptr, "pEventInstance is nullpter during %s", __FUNCTION__);

	auto iter(g_constructedEventInstances.begin());
	auto const iterEnd(g_constructedEventInstances.cend());

	for (; iter != iterEnd; ++iter)
	{
		if ((*iter) == pEventInstance)
		{
			if (iter != (iterEnd - 1))
			{
				(*iter) = g_constructedEventInstances.back();
			}

			g_constructedEventInstances.pop_back();
			break;
		}
	}

	{
		CryAutoLock<CryCriticalSection> const lock(CryAudio::Impl::Wwise::g_cs);
		g_playingIds.erase(pEventInstance->GetPlayingId());
	}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	CEvent* const pEvent = &pEventInstance->GetEvent();
	delete pEventInstance;

	pEvent->DecrementNumInstances();

	if (pEvent->CanBeDestructed())
	{
		delete pEvent;
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetPanningRule(int const panningRule)
{
	AK::SoundEngine::SetPanningRule(static_cast<AkPanningRule>(panningRule));
}

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
void CImpl::GetInitBankSize()
{
	CryFixedStringT<MaxFilePathLength> const initBankPath = m_regularSoundBankFolder + "/Init.bnk";
	m_initBankSize = gEnv->pCryPak->FGetSize(initBankPath.c_str());
}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
void CImpl::DrawDebugMemoryInfo(IRenderAuxGeom& auxGeom, float const posX, float& posY, bool const drawDetailedInfo)
{
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	float const headerPosY = posY;
	posY += Debug::g_systemHeaderLineSpacerHeight;

	size_t totalPoolSize = 0;

	{
		auto& allocator = CObject::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Objects", g_objectPoolSize);
		}
	}

	{
		auto& allocator = CEventInstance::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Event Instances", g_eventInstancePoolSize);
		}
	}

	if (g_debugPoolSizes.events > 0)
	{
		auto& allocator = CEvent::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Events", g_poolSizes.events);
		}
	}

	if (g_debugPoolSizes.parameters > 0)
	{
		auto& allocator = CParameter::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Parameters", g_poolSizes.parameters);
		}
	}

	if (g_debugPoolSizes.parametersAdvanced > 0)
	{
		auto& allocator = CParameterAdvanced::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Advanced Parameters", g_poolSizes.parametersAdvanced);
		}
	}

	if (g_debugPoolSizes.parameterEnvironments > 0)
	{
		auto& allocator = CParameterEnvironment::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Parameters on Environments", g_poolSizes.parameterEnvironments);
		}
	}

	if (g_debugPoolSizes.parameterEnvironmentsAdvanced > 0)
	{
		auto& allocator = CParameterEnvironmentAdvanced::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Advanced Parameters on Environment", g_poolSizes.parameterEnvironmentsAdvanced);
		}
	}

	if (g_debugPoolSizes.parameterStates > 0)
	{
		auto& allocator = CParameterState::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Parameters on States", g_poolSizes.parameterStates);
		}
	}

	if (g_debugPoolSizes.states > 0)
	{
		auto& allocator = CState::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "States", g_poolSizes.states);
		}
	}

	if (g_debugPoolSizes.switches > 0)
	{
		auto& allocator = CSwitch::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Switches", g_poolSizes.switches);
		}
	}

	if (g_debugPoolSizes.auxBuses > 0)
	{
		auto& allocator = CAuxBus::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Aux Buses", g_poolSizes.auxBuses);
		}
	}

	if (g_debugPoolSizes.soundBanks > 0)
	{
		auto& allocator = CSoundBank::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "SoundBanks", g_poolSizes.soundBanks);
		}
	}

	CryModuleMemoryInfo memInfo;
	ZeroStruct(memInfo);
	CryGetMemoryInfoForModule(&memInfo);

	CryFixedStringT<Debug::MaxMemInfoStringLength> memAllocSizeString;
	auto const memAllocSize = static_cast<size_t>(memInfo.allocated - memInfo.freed);
	Debug::FormatMemoryString(memAllocSizeString, memAllocSize);

	CryFixedStringT<Debug::MaxMemInfoStringLength> totalPoolSizeString;
	Debug::FormatMemoryString(totalPoolSizeString, totalPoolSize);

	CryFixedStringT<Debug::MaxMemInfoStringLength> initBankSizeString;
	Debug::FormatMemoryString(initBankSizeString, m_initBankSize);

	CryFixedStringT<Debug::MaxMemInfoStringLength> totalMemSizeString;
	size_t const totalMemSize = memAllocSize + totalPoolSize + m_initBankSize;
	Debug::FormatMemoryString(totalMemSizeString, totalMemSize);

	auxGeom.Draw2dLabel(posX, headerPosY, Debug::g_systemHeaderFontSize, Debug::s_globalColorHeader, false, "%s (System: %s | Pools: %s | Init Bank: %s | Total: %s)",
	                    m_name.c_str(), memAllocSizeString.c_str(), totalPoolSizeString.c_str(), initBankSizeString.c_str(), totalMemSizeString.c_str());

	size_t const numEvents = g_constructedEventInstances.size();
	size_t const numListeners = g_constructedListeners.size();

	posY += Debug::g_systemLineHeight;
	auxGeom.Draw2dLabel(posX, posY, Debug::g_systemFontSize, Debug::s_systemColorTextSecondary, false, "Active Events: %3" PRISIZE_T " | Listeners: %3" PRISIZE_T " | Objects with relative velocity calculation: %u",
	                    numEvents, numListeners, g_numObjectsWithRelativeVelocity);

	for (auto const pListener : g_constructedListeners)
	{
		Vec3 const& listenerPosition = pListener->GetPosition();
		Vec3 const& listenerDirection = pListener->GetTransformation().GetForward();
		float const listenerVelocity = pListener->GetVelocity().GetLength();
		char const* const szName = pListener->GetName();

		posY += Debug::g_systemLineHeight;
		auxGeom.Draw2dLabel(posX, posY, Debug::g_systemFontSize, Debug::s_systemColorListenerActive, false, "Listener: %s | PosXYZ: %.2f %.2f %.2f | FwdXYZ: %.2f %.2f %.2f | Velocity: %.2f m/s",
		                    szName, listenerPosition.x, listenerPosition.y, listenerPosition.z, listenerDirection.x, listenerDirection.y, listenerDirection.z, listenerVelocity);
	}

#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DrawDebugInfoList(IRenderAuxGeom& auxGeom, float& posX, float posY, Vec3 const& camPos, float const debugDistance, char const* const szTextFilter) const
{
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	if ((g_cvars.m_debugListFilter & g_debugListMask) != 0)
	{
		CryFixedStringT<MaxControlNameLength> lowerCaseSearchString(szTextFilter);
		lowerCaseSearchString.MakeLower();

		float const initialPosY = posY;

		if ((g_cvars.m_debugListFilter & EDebugListFilter::EventInstances) != 0)
		{
			auxGeom.Draw2dLabel(posX, posY, Debug::g_listHeaderFontSize, Debug::s_globalColorHeader, false, "Wwise Events [%" PRISIZE_T "]", g_constructedEventInstances.size());
			posY += Debug::g_listHeaderLineHeight;

			for (auto const pEventInstance : g_constructedEventInstances)
			{
				Vec3 const& position = pEventInstance->GetObject().GetTransformation().GetPosition();
				float const distance = position.GetDistance(camPos);

				if ((debugDistance <= 0.0f) || ((debugDistance > 0.0f) && (distance < debugDistance)))
				{
					char const* const szEventName = pEventInstance->GetEvent().GetName();
					CryFixedStringT<MaxControlNameLength> lowerCaseEventName(szEventName);
					lowerCaseEventName.MakeLower();
					bool const draw = ((lowerCaseSearchString.empty() || (lowerCaseSearchString == "0")) || (lowerCaseEventName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos));

					if (draw)
					{
						ColorF const& color = (pEventInstance->GetState() == EEventInstanceState::Virtual) ? Debug::s_globalColorVirtual : Debug::s_listColorItemActive;
						auxGeom.Draw2dLabel(posX, posY, Debug::g_listFontSize, color, false, "%s on %s (%s)",
						                    szEventName, pEventInstance->GetObject().GetName(), pEventInstance->GetObject().GetListenerNames());

						posY += Debug::g_listLineHeight;
					}
				}
			}

			posX += 600.0f;
		}

		if ((g_cvars.m_debugListFilter & EDebugListFilter::States) != 0)
		{
			posY = initialPosY;

			auxGeom.Draw2dLabel(posX, posY, Debug::g_listHeaderFontSize, Debug::s_globalColorHeader, false, "Wwise States [%" PRISIZE_T "]", g_debugStates.size());
			posY += Debug::g_listHeaderLineHeight;

			for (auto const& statePair : g_debugStates)
			{
				char const* const szStateGroupName = statePair.first.c_str();
				CryFixedStringT<MaxControlNameLength> lowerCaseStateGroupName(szStateGroupName);
				lowerCaseStateGroupName.MakeLower();

				char const* const szStateName = statePair.second.c_str();
				CryFixedStringT<MaxControlNameLength> lowerCaseStateName(szStateName);
				lowerCaseStateName.MakeLower();

				bool const draw = ((lowerCaseSearchString.empty() || (lowerCaseSearchString == "0")) ||
				                   (lowerCaseStateGroupName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos) ||
				                   (lowerCaseStateName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos));

				if (draw)
				{
					auxGeom.Draw2dLabel(posX, posY, Debug::g_listFontSize, Debug::s_listColorItemActive, false, "%s : %s", szStateGroupName, szStateName);

					posY += Debug::g_listLineHeight;
				}
			}

			posX += 300.0f;
		}
	}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
}
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio

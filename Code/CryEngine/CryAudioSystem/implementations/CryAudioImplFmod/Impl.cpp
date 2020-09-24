// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Impl.h"
#include "Common.h"
#include "CVars.h"
#include "Bank.h"
#include "Event.h"
#include "EventInstance.h"
#include "Listener.h"
#include "MasterBank.h"
#include "Object.h"
#include "Parameter.h"
#include "ParameterAdvanced.h"
#include "ParameterEnvironment.h"
#include "ParameterEnvironmentAdvanced.h"
#include "ParameterInfo.h"
#include "ParameterState.h"
#include "Return.h"
#include "Snapshot.h"
#include "Vca.h"
#include "VcaState.h"
#include "GlobalData.h"

#if CRY_PLATFORM_ORBIS
	#include "platforms/ps4/Functions.h"
#else
	#include "platforms/null/Functions.h"
#endif // CRY_PLATFORM_ORBIS

#include <ISettingConnection.h>
#include <FileInfo.h>
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/IProjectManager.h>
#include <CryAudio/IAudioSystem.h>

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	#include <Logger.h>
	#include <DebugStyle.h>
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
SPoolSizes g_poolSizes;
std::map<ContextId, SPoolSizes> g_contextPoolSizes;
std::vector<SMasterBank> g_masterBanks;

std::map<int, bool> g_listenerIds;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
SPoolSizes g_debugPoolSizes;
EventInstances g_constructedEventInstances;
uint16 g_objectPoolSize = 0;
uint16 g_eventInstancePoolSize = 0;
size_t g_masterBankSize = 0;
std::vector<CListener*> g_constructedListeners;
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

constexpr char const* g_szEventPrefix = "event:/";
constexpr char const* g_szParameterPrefix = "parameter:/";
constexpr char const* g_szSnapshotPrefix = "snapshot:/";
constexpr char const* g_szBusPrefix = "bus:/";
constexpr char const* g_szVcaPrefix = "vca:/";

constexpr size_t g_maxFileSize = size_t(1) << size_t(31);

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

	uint16 numSnapshots = 0;
	node->getAttr(g_szSnapshotsAttribute, numSnapshots);
	poolSizes.snapshots += numSnapshots;

	uint16 numReturns = 0;
	node->getAttr(g_szReturnsAttribute, numReturns);
	poolSizes.returns += numReturns;

	uint16 numVcas = 0;
	node->getAttr(g_szVcasAttribute, numVcas);
	poolSizes.vcas += numVcas;

	uint16 numVcaStates = 0;
	node->getAttr(g_szVcaStatesAttribute, numVcaStates);
	poolSizes.vcaStates += numVcaStates;

	uint16 numBanks = 0;
	node->getAttr(g_szBanksAttribute, numBanks);
	poolSizes.banks += numBanks;
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
	CSnapshot::CreateAllocator(g_poolSizes.snapshots);
	CReturn::CreateAllocator(g_poolSizes.returns);
	CVca::CreateAllocator(g_poolSizes.vcas);
	CVcaState::CreateAllocator(g_poolSizes.vcaStates);
	CBank::CreateAllocator(g_poolSizes.banks);
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
	CSnapshot::FreeMemoryPool();
	CReturn::FreeMemoryPool();
	CVca::FreeMemoryPool();
	CVcaState::FreeMemoryPool();
	CBank::FreeMemoryPool();
}

//////////////////////////////////////////////////////////////////////////
void ClearActiveSnapshots()
{
	for (auto const& snapshotInstancePair : g_activeSnapshots)
	{
		snapshotInstancePair.second->release();
	}

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	g_activeSnapshotNames.clear();
#endif    // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
}

///////////////////////////////////////////////////////////////////////////
CImpl::CImpl()
{
	g_pImpl = this;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::Update()
{
	if (g_pStudioSystem != nullptr)
	{
		CRY_VERIFY(g_pStudioSystem->update() == FMOD_OK);
	}
}

///////////////////////////////////////////////////////////////////////////
bool CImpl::Init(uint16 const objectPoolSize)
{
	PlatformSpecific::Initialize();

	if (g_cvars.m_eventPoolSize < 1)
	{
		g_cvars.m_eventPoolSize = 1;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		Cry::Audio::Log(ELogType::Warning, R"(Event pool size must be at least 1. Forcing the cvar "s_FmodEventPoolSize" to 1!)");
#endif    // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}

	g_constructedObjects.reserve(static_cast<size_t>(objectPoolSize));
	AllocateMemoryPools(objectPoolSize, static_cast<uint16>(g_cvars.m_eventPoolSize));

	m_regularSoundBankFolder = CRY_AUDIO_DATA_ROOT;
	m_regularSoundBankFolder += "/";
	m_regularSoundBankFolder += g_szImplFolderName;
	m_regularSoundBankFolder += "/";
	m_regularSoundBankFolder += g_szAssetsFolderName;
	m_localizedSoundBankFolder = m_regularSoundBankFolder;

	for (int i = 0; i < FMOD_MAX_LISTENERS; ++i)
	{
		g_listenerIds.emplace(i, false);
	}

	FMOD_RESULT fmodResult = FMOD::Studio::System::create(&g_pStudioSystem);
	CRY_AUDIO_IMPL_FMOD_ASSERT_OK;
	fmodResult = g_pStudioSystem->getCoreSystem(&g_pCoreSystem);
	CRY_AUDIO_IMPL_FMOD_ASSERT_OK;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	g_constructedEventInstances.reserve(static_cast<size_t>(g_cvars.m_eventPoolSize));

	g_objectPoolSize = objectPoolSize;
	g_eventInstancePoolSize = static_cast<uint16>(g_cvars.m_eventPoolSize);

	uint32 version = 0;
	fmodResult = g_pCoreSystem->getVersion(&version);
	CRY_AUDIO_IMPL_FMOD_ASSERT_OK;

	CryFixedStringT<MaxInfoStringLength> systemVersion;
	systemVersion.Format("%08x", version);
	CryFixedStringT<MaxInfoStringLength> headerVersion;
	headerVersion.Format("%08x", FMOD_VERSION);
	CreateVersionString(systemVersion);
	CreateVersionString(headerVersion);

	m_name = CRY_AUDIO_IMPL_FMOD_INFO_STRING;
	m_name += "System: ";
	m_name += systemVersion;
	m_name += " - Header: ";
	m_name += headerVersion;
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	int sampleRate = 0;
	int numRawSpeakers = 0;
	FMOD_SPEAKERMODE speakerMode = FMOD_SPEAKERMODE_DEFAULT;
	fmodResult = g_pCoreSystem->getSoftwareFormat(&sampleRate, &speakerMode, &numRawSpeakers);
	CRY_AUDIO_IMPL_FMOD_ASSERT_OK;
	fmodResult = g_pCoreSystem->setSoftwareFormat(sampleRate, speakerMode, numRawSpeakers);
	CRY_AUDIO_IMPL_FMOD_ASSERT_OK;

	fmodResult = g_pCoreSystem->set3DSettings(g_cvars.m_dopplerScale, g_cvars.m_distanceFactor, g_cvars.m_rolloffScale);
	CRY_AUDIO_IMPL_FMOD_ASSERT_OK;

	void* pExtraDriverData = nullptr;
	int initFlags = FMOD_INIT_NORMAL | FMOD_INIT_VOL0_BECOMES_VIRTUAL;
	int studioInitFlags = FMOD_STUDIO_INIT_NORMAL;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	if (g_cvars.m_enableLiveUpdate > 0)
	{
		studioInitFlags |= FMOD_STUDIO_INIT_LIVEUPDATE;
	}
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	if (g_cvars.m_enableSynchronousUpdate > 0)
	{
		studioInitFlags |= FMOD_STUDIO_INIT_SYNCHRONOUS_UPDATE;
	}

	fmodResult = g_pStudioSystem->initialize(g_cvars.m_maxChannels, studioInitFlags, initFlags, pExtraDriverData);
	CRY_AUDIO_IMPL_FMOD_ASSERT_OK;

	fmodResult = g_pStudioSystem->setNumListeners(FMOD_MAX_LISTENERS);
	CRY_AUDIO_IMPL_FMOD_ASSERT_OK;

	LoadMasterBanks();

	return fmodResult == FMOD_OK;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::ShutDown()
{
	if (g_pStudioSystem != nullptr)
	{
		ClearActiveSnapshots();
		UnloadMasterBanks();

		CRY_VERIFY(g_pStudioSystem->release() == FMOD_OK);
	}

	PlatformSpecific::Release();
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

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	ZeroStruct(g_debugPoolSizes);
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
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
				g_poolSizes.snapshots += iterPoolSizes.snapshots;
				g_poolSizes.returns += iterPoolSizes.returns;
				g_poolSizes.vcas += iterPoolSizes.vcas;
				g_poolSizes.vcaStates += iterPoolSizes.vcaStates;
				g_poolSizes.banks += iterPoolSizes.banks;
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
				maxContextPoolSizes.snapshots = std::max(maxContextPoolSizes.snapshots, iterPoolSizes.snapshots);
				maxContextPoolSizes.returns = std::max(maxContextPoolSizes.returns, iterPoolSizes.returns);
				maxContextPoolSizes.vcas = std::max(maxContextPoolSizes.vcas, iterPoolSizes.vcas);
				maxContextPoolSizes.vcaStates = std::max(maxContextPoolSizes.vcaStates, iterPoolSizes.vcaStates);
				maxContextPoolSizes.banks = std::max(maxContextPoolSizes.banks, iterPoolSizes.banks);
			}

			g_poolSizes.events += maxContextPoolSizes.events;
			g_poolSizes.parameters += maxContextPoolSizes.parameters;
			g_poolSizes.parametersAdvanced += maxContextPoolSizes.parametersAdvanced;
			g_poolSizes.parameterEnvironments += maxContextPoolSizes.parameterEnvironments;
			g_poolSizes.parameterEnvironmentsAdvanced += maxContextPoolSizes.parameterEnvironmentsAdvanced;
			g_poolSizes.parameterStates += maxContextPoolSizes.parameterStates;
			g_poolSizes.snapshots += maxContextPoolSizes.snapshots;
			g_poolSizes.returns += maxContextPoolSizes.returns;
			g_poolSizes.vcas += maxContextPoolSizes.vcas;
			g_poolSizes.vcaStates += maxContextPoolSizes.vcaStates;
			g_poolSizes.banks += maxContextPoolSizes.banks;
		}
	}

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	// Used to hide pools without allocations in debug draw.
	g_debugPoolSizes = g_poolSizes;
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	g_poolSizes.events = std::max<uint16>(1, g_poolSizes.events);
	g_poolSizes.parameters = std::max<uint16>(1, g_poolSizes.parameters);
	g_poolSizes.parametersAdvanced = std::max<uint16>(1, g_poolSizes.parametersAdvanced);
	g_poolSizes.parameterEnvironments = std::max<uint16>(1, g_poolSizes.parameterEnvironments);
	g_poolSizes.parameterEnvironmentsAdvanced = std::max<uint16>(1, g_poolSizes.parameterEnvironmentsAdvanced);
	g_poolSizes.parameterStates = std::max<uint16>(1, g_poolSizes.parameterStates);
	g_poolSizes.snapshots = std::max<uint16>(1, g_poolSizes.snapshots);
	g_poolSizes.returns = std::max<uint16>(1, g_poolSizes.returns);
	g_poolSizes.vcas = std::max<uint16>(1, g_poolSizes.vcas);
	g_poolSizes.vcaStates = std::max<uint16>(1, g_poolSizes.vcaStates);
	g_poolSizes.banks = std::max<uint16>(1, g_poolSizes.banks);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::OnLoseFocus()
{
	MuteMasterBus(true);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::OnGetFocus()
{
	MuteMasterBus(false);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::MuteAll()
{
	MuteMasterBus(true);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::UnmuteAll()
{
	MuteMasterBus(false);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::PauseAll()
{
	PauseMasterBus(true);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::ResumeAll()
{
	PauseMasterBus(false);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::StopAllSounds()
{
	FMOD::Studio::Bus* pMasterBus = nullptr;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	if (g_pStudioSystem->getBus(g_szBusPrefix, &pMasterBus) != FMOD_OK)
	{
		Cry::Audio::Log(ELogType::Error, "Failed to get master bus during %s", __FUNCTION__);
	}
#else
	g_pStudioSystem->getBus(g_szBusPrefix, &pMasterBus);
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	if (pMasterBus != nullptr)
	{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		if (pMasterBus->stopAllEvents(FMOD_STUDIO_STOP_IMMEDIATE) != FMOD_OK)
		{
			Cry::Audio::Log(ELogType::Error, "Failed to stop all events during %s", __FUNCTION__);
		}
#else
		pMasterBus->stopAllEvents(FMOD_STUDIO_STOP_IMMEDIATE);
#endif    // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::RegisterInMemoryFile(SFileInfo* const pFileInfo)
{
	if (pFileInfo != nullptr)
	{
		auto const pFileData = static_cast<CBank*>(pFileInfo->pImplData);

		if (pFileData != nullptr)
		{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
			// loadBankMemory requires 32-byte alignment when using FMOD_STUDIO_LOAD_MEMORY_POINT
			if ((reinterpret_cast<uintptr_t>(pFileInfo->pFileData) & (32 - 1)) > 0)
			{
				CryFatalError("<Audio>: allocation not %d byte aligned!", 32);
			}
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

			CRY_VERIFY(g_pStudioSystem->loadBankMemory(
									 static_cast<char*>(pFileInfo->pFileData),
									 static_cast<int>(pFileInfo->size),
									 FMOD_STUDIO_LOAD_MEMORY_POINT,
									 FMOD_STUDIO_LOAD_BANK_NORMAL,
									 &pFileData->pBank) == FMOD_OK);

			CryFixedStringT<MaxFilePathLength> streamsBankPath(pFileInfo->filePath);
			PathUtil::RemoveExtension(streamsBankPath);
			streamsBankPath += ".streams.bank";

			size_t const streamsBankFileSize = gEnv->pCryPak->FGetSize(streamsBankPath.c_str());

			if (streamsBankFileSize > 0)
			{
				pFileData->m_streamsBankPath = streamsBankPath;
				CRY_VERIFY(LoadBankCustom(pFileData->m_streamsBankPath.c_str(), &pFileData->pStreamsBank) == FMOD_OK);
			}
		}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Error, "Invalid FileData passed to the Fmod implementation of %s", __FUNCTION__);
		}
#endif    // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::UnregisterInMemoryFile(SFileInfo* const pFileInfo)
{
	if (pFileInfo != nullptr)
	{
		auto const pFileData = static_cast<CBank*>(pFileInfo->pImplData);

		if (pFileData != nullptr)
		{
			// Stop all playing events before unloading.
			int eventCount = 0;
			CRY_VERIFY(pFileData->pBank->getEventCount(&eventCount) == FMOD_OK, "Failed to retrieve event count during %s", __FUNCTION__);
			std::vector<::FMOD::Studio::EventDescription*> eventDescriptions(eventCount);

			if (!eventDescriptions.empty())
			{
				CRY_VERIFY(pFileData->pBank->getEventList(&eventDescriptions[0], eventCount, &eventCount) == FMOD_OK, "Failed to retrieve event list during %s", __FUNCTION__);

				for (::FMOD::Studio::EventDescription* pEventDescription : eventDescriptions)
				{
					int instanceCount = 0;
					CRY_VERIFY(pEventDescription->getInstanceCount(&instanceCount) == FMOD_OK, "Failed to retrieve instance count during %s", __FUNCTION__);
					std::vector<::FMOD::Studio::EventInstance*> eventInstances(instanceCount);

					if (!eventInstances.empty())
					{
						CRY_VERIFY(pEventDescription->getInstanceList(&eventInstances[0], instanceCount, &instanceCount) == FMOD_OK, "Failed to retrieve instance list during %s", __FUNCTION__);

						for (::FMOD::Studio::EventInstance* pEventInstance : eventInstances)
						{
							void* pUserData = nullptr;
							CRY_VERIFY(pEventInstance->getUserData(&pUserData) == FMOD_OK, "Failed to retrieve CEventInstance during %s", __FUNCTION__);
							CEventInstance* pCEventInstance = reinterpret_cast<CEventInstance*>(pUserData);
							pCEventInstance->SetToBeRemoved();
						}

						CRY_VERIFY(pEventDescription->releaseAllInstances() == FMOD_OK, "Failed to release playing instances during %s", __FUNCTION__);
					}
				}
			}

			CRY_VERIFY(pFileData->pBank->unload() == FMOD_OK, "Failed to unload bank \"%s\" during %s", pFileInfo->filePath, __FUNCTION__);

			FMOD_STUDIO_LOADING_STATE loadingState;

			do
			{
				CRY_VERIFY(g_pStudioSystem->update() == FMOD_OK);
				FMOD_RESULT const fmodResult = pFileData->pBank->getLoadingState(&loadingState);
				CRY_AUDIO_IMPL_FMOD_ASSERT_OK_OR_INVALID_HANDLE;
			}
			while (loadingState == FMOD_STUDIO_LOADING_STATE_UNLOADING);

			pFileData->pBank = nullptr;

			if (pFileData->pStreamsBank != nullptr)
			{
				CRY_VERIFY(pFileData->pStreamsBank->unload() == FMOD_OK);
				pFileData->pStreamsBank = nullptr;
			}
		}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Error, "Invalid FileData passed to the Fmod implementation of %s", __FUNCTION__);
		}
#endif    // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
bool CImpl::ConstructFile(XmlNodeRef const& rootNode, SFileInfo* const pFileInfo)
{
	bool isConstructed = false;

	if ((_stricmp(rootNode->getTag(), g_szFileTag) == 0) && (pFileInfo != nullptr))
	{
		char const* const szFileName = rootNode->getAttr(g_szNameAttribute);

		if (szFileName != nullptr && szFileName[0] != '\0')
		{
			char const* const szLocalized = rootNode->getAttr(g_szLocalizedAttribute);
			pFileInfo->isLocalized = (szLocalized != nullptr) && (_stricmp(szLocalized, g_szTrueValue) == 0);
			cry_strcpy(pFileInfo->fileName, szFileName);
			CryFixedStringT<MaxFilePathLength> const filePath = (pFileInfo->isLocalized ? m_localizedSoundBankFolder : m_regularSoundBankFolder) + "/" + szFileName;
			cry_strcpy(pFileInfo->filePath, filePath.c_str());

			// FMOD Studio always uses 32 byte alignment for preloaded banks regardless of the platform.
			pFileInfo->memoryBlockAlignment = 32;

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CBank");
			pFileInfo->pImplData = new CBank();

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
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	cry_strcpy(implInfo.name, m_name.c_str());
#else
	cry_fixed_size_strcpy(implInfo.name, g_implNameInRelease);
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	cry_strcpy(implInfo.folderName, g_szImplFolderName, strlen(g_szImplFolderName));
}

///////////////////////////////////////////////////////////////////////////
IObject* CImpl::ConstructObject(CTransformation const& transformation, IListeners const& listeners, char const* const szName /*= nullptr*/)
{
	int const numListeners = listeners.size();
	CRY_ASSERT_MESSAGE(numListeners <= FMOD_MAX_LISTENERS, "Number of listeners (%d) exceeded FMOD_MAX_LISTENERS (%d) during %s", numListeners, FMOD_MAX_LISTENERS, __FUNCTION__);

	int listenerMask = 0;
	Listeners objectListeners;

	for (int i = 0; (i < numListeners) && (i < FMOD_MAX_LISTENERS); ++i)
	{
		auto const pListener = static_cast<CListener*>(listeners[i]);
		listenerMask |= BIT(pListener->GetId());
		objectListeners.emplace_back(pListener);
	}

	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CObject");
	auto const pObject = new CObject(transformation, listenerMask, objectListeners);

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	if (szName != nullptr)
	{
		pObject->SetName(szName);
	}

	if (!stl::push_back_unique(g_constructedObjects, pObject))
	{
		Cry::Audio::Log(ELogType::Warning, "Trying to construct an already registered object.");
	}
#else
	stl::push_back_unique(g_constructedObjects, pObject);
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	return static_cast<IObject*>(pObject);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructObject(IObject const* const pIObject)
{
	auto const pObject = static_cast<CObject const*>(pIObject);

	if (!stl::find_and_erase(g_constructedObjects, pObject))
	{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		Cry::Audio::Log(ELogType::Warning, "Trying to delete a non-existing object.");
#endif    // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}

	delete pObject;
}

///////////////////////////////////////////////////////////////////////////
IListener* CImpl::ConstructListener(CTransformation const& transformation, char const* const szName)
{
	int id = FMOD_MAX_LISTENERS;

	for (auto& listenerIdPair : g_listenerIds)
	{
		if (!listenerIdPair.second)
		{
			id = listenerIdPair.first;
			listenerIdPair.second = true;
			break;
		}
	}

	CRY_ASSERT_MESSAGE(id < FMOD_MAX_LISTENERS, "Number of listeners (%d) exceeded FMOD_MAX_LISTENERS (%d) during %s", id, FMOD_MAX_LISTENERS, __FUNCTION__);

	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CListener");
	auto const pListener = new CListener(transformation, id);

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	pListener->SetName(szName);

	if (!stl::push_back_unique(g_constructedListeners, pListener))
	{
		Cry::Audio::Log(ELogType::Warning, "Trying to construct an already registered listener.");
	}
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	return static_cast<IListener*>(pListener);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructListener(IListener* const pIListener)
{
	auto const pListener = static_cast<CListener*>(pIListener);
	g_listenerIds[pListener->GetId()] = false;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	if (!stl::find_and_erase(g_constructedListeners, pListener))
	{
		Cry::Audio::Log(ELogType::Warning, "Trying to delete a non-existing listener.");
	}
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	delete pListener;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GamepadConnected(DeviceId const deviceUniqueID)
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GamepadDisconnected(DeviceId const deviceUniqueID)
{
}

///////////////////////////////////////////////////////////////////////////
ITriggerConnection* CImpl::ConstructTriggerConnection(XmlNodeRef const& rootNode, float& radius)
{
	ITriggerConnection* pITriggerConnection = nullptr;

	char const* const szTag = rootNode->getTag();

	if (_stricmp(szTag, g_szEventTag) == 0)
	{
		stack_string fullName(g_szEventPrefix);
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		fullName += szName;
		FMOD_GUID guid = { 0 };

		if (g_pStudioSystem->lookupID(fullName.c_str(), &guid) == FMOD_OK)
		{
			FMOD::Studio::EventDescription* pEventDescription = nullptr;
			g_pStudioSystem->getEventByID(&guid, &pEventDescription);

			CEvent::EActionType actionType = CEvent::EActionType::Start;
			char const* const szEventType = rootNode->getAttr(g_szTypeAttribute);

			if ((szEventType != nullptr) && (szEventType[0] != '\0'))
			{
				if (_stricmp(szEventType, g_szStopValue) == 0)
				{
					actionType = CEvent::EActionType::Stop;
				}
				else if (_stricmp(szEventType, g_szPauseValue) == 0)
				{
					actionType = CEvent::EActionType::Pause;
				}
				else if (_stricmp(szEventType, g_szResumeValue) == 0)
				{
					actionType = CEvent::EActionType::Resume;
				}
			}

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CEvent");

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
			pITriggerConnection = static_cast<ITriggerConnection*>(new CEvent(StringToId(fullName.c_str()), actionType, guid, szName));
#else
			pITriggerConnection = static_cast<ITriggerConnection*>(new CEvent(StringToId(fullName.c_str()), actionType, guid));
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
		}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod event: %s", szName);
		}
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
	else if (_stricmp(szTag, g_szKeyTag) == 0)
	{
		stack_string fullName(g_szEventPrefix);
		char const* const szEventName = rootNode->getAttr(g_szEventAttribute);
		fullName += szEventName;
		FMOD_GUID guid = { 0 };

		if (g_pStudioSystem->lookupID(fullName.c_str(), &guid) == FMOD_OK)
		{
			char const* const szKey = rootNode->getAttr(g_szNameAttribute);

			FMOD::Studio::EventDescription* pEventDescription = nullptr;
			g_pStudioSystem->getEventByID(&guid, &pEventDescription);

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CEvent");

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
			pITriggerConnection = static_cast<ITriggerConnection*>(new CEvent(StringToId(fullName.c_str()), guid, szKey, szEventName));
#else
			pITriggerConnection = static_cast<ITriggerConnection*>(new CEvent(StringToId(fullName.c_str()), guid, szKey));
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
		}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod event: %s", szEventName);
		}
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
	else if (_stricmp(szTag, g_szSnapshotTag) == 0)
	{
		stack_string fullName(g_szSnapshotPrefix);
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		fullName += szName;
		FMOD_GUID guid = { 0 };

		if (g_pStudioSystem->lookupID(fullName.c_str(), &guid) == FMOD_OK)
		{
			CSnapshot::EActionType actionType = CSnapshot::EActionType::Start;
			char const* const szActionType = rootNode->getAttr(g_szTypeAttribute);

			if ((szActionType != nullptr) && (szActionType[0] != '\0') && (_stricmp(szActionType, g_szStopValue) == 0))
			{
				actionType = CSnapshot::EActionType::Stop;
			}

			FMOD::Studio::EventDescription* pEventDescription = nullptr;
			g_pStudioSystem->getEventByID(&guid, &pEventDescription);

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CSnapshot");

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
			pITriggerConnection = static_cast<ITriggerConnection*>(new CSnapshot(StringToId(fullName.c_str()), actionType, pEventDescription, szName));
#else
			pITriggerConnection = static_cast<ITriggerConnection*>(new CSnapshot(StringToId(fullName.c_str()), actionType, pEventDescription));
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
		}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod snapshot: %s", szName);
		}
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Fmod tag: %s", szTag);
	}
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	return pITriggerConnection;
}

//////////////////////////////////////////////////////////////////////////
ITriggerConnection* CImpl::ConstructTriggerConnection(ITriggerInfo const* const pITriggerInfo)
{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	ITriggerConnection* pITriggerConnection = nullptr;

	auto const pTriggerInfo = static_cast<STriggerInfo const*>(pITriggerInfo);

	if (pTriggerInfo != nullptr)
	{
		stack_string fullName(g_szEventPrefix);
		char const* const szName = pTriggerInfo->name;
		fullName += szName;
		FMOD_GUID guid = { 0 };

		if (g_pStudioSystem->lookupID(fullName.c_str(), &guid) == FMOD_OK)
		{
			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CEvent");
			pITriggerConnection = static_cast<ITriggerConnection*>(new CEvent(StringToId(fullName.c_str()), CEvent::EActionType::Start, guid, szName));
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod event: %s", szName);
		}
	}

	return pITriggerConnection;
#else
	return nullptr;
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructTriggerConnection(ITriggerConnection* const pITriggerConnection)
{
	auto const pBaseTriggerConnection = static_cast<CBaseTriggerConnection*>(pITriggerConnection);

	if (pBaseTriggerConnection->GetType() == CBaseTriggerConnection::EType::Event)
	{
		auto const pEvent = static_cast<CEvent*>(pBaseTriggerConnection);
		pEvent->SetToBeDestructed();

		if (pEvent->CanBeDestructed())
		{
			delete pEvent;
		}
	}
	else
	{
		delete pBaseTriggerConnection;
	}
}

///////////////////////////////////////////////////////////////////////////
IParameterConnection* CImpl::ConstructParameterConnection(XmlNodeRef const& rootNode)
{
	IParameterConnection* pIParameterConnection = nullptr;

	char const* const szTag = rootNode->getTag();

	if (_stricmp(szTag, g_szParameterTag) == 0)
	{
		stack_string fullName(g_szParameterPrefix);
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		fullName += szName;
		FMOD_GUID guid = { 0 };

		if (g_pStudioSystem->lookupID(fullName.c_str(), &guid) == FMOD_OK)
		{
			FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;
			bool const isGlobal = g_pStudioSystem->getParameterDescriptionByName(fullName.c_str(), &parameterDescription) == FMOD_OK;

			CParameterInfo const parameterInfo(parameterDescription.id, isGlobal, fullName.c_str());

			bool isAdvanced = false;

			float multiplier = g_defaultParamMultiplier;
			float shift = g_defaultParamShift;

			isAdvanced |= rootNode->getAttr(g_szMutiplierAttribute, multiplier);
			isAdvanced |= rootNode->getAttr(g_szShiftAttribute, shift);

			if (isAdvanced)
			{
				MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CParameterAdvanced");
				pIParameterConnection = static_cast<IParameterConnection*>(new CParameterAdvanced(parameterInfo, multiplier, shift));
			}
			else
			{
				MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CParameter");
				pIParameterConnection = static_cast<IParameterConnection*>(new CParameter(parameterInfo));
			}
		}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod parameter: %s", szName);
		}
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	}
	else if (_stricmp(szTag, g_szVcaTag) == 0)
	{
		stack_string fullName(g_szVcaPrefix);
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		fullName += szName;
		FMOD_GUID guid = { 0 };

		if (g_pStudioSystem->lookupID(fullName.c_str(), &guid) == FMOD_OK)
		{
			FMOD::Studio::VCA* pVca = nullptr;
			CRY_VERIFY(g_pStudioSystem->getVCAByID(&guid, &pVca) == FMOD_OK);

			float multiplier = g_defaultParamMultiplier;
			float shift = g_defaultParamShift;
			rootNode->getAttr(g_szMutiplierAttribute, multiplier);
			rootNode->getAttr(g_szShiftAttribute, shift);

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CVca");

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
			pIParameterConnection = static_cast<IParameterConnection*>(new CVca(pVca, multiplier, shift, szName));
#else
			pIParameterConnection = static_cast<IParameterConnection*>(new CVca(pVca, multiplier, shift));
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
		}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod VCA: %s", szName);
		}
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Fmod tag: %s", szTag);
	}
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

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

	if (_stricmp(szTag, g_szParameterTag) == 0)
	{
		stack_string fullName(g_szParameterPrefix);
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		fullName += szName;
		FMOD_GUID guid = { 0 };

		if (g_pStudioSystem->lookupID(fullName.c_str(), &guid) == FMOD_OK)
		{
			char const* const szValue = rootNode->getAttr(g_szValueAttribute);
			auto const value = static_cast<float>(atof(szValue));

			FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;
			bool const isGlobal = g_pStudioSystem->getParameterDescriptionByName(fullName.c_str(), &parameterDescription) == FMOD_OK;

			CParameterInfo const parameterInfo(parameterDescription.id, isGlobal, fullName.c_str());

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CParameterState");
			pISwitchStateConnection = static_cast<ISwitchStateConnection*>(new CParameterState(parameterInfo, value));
		}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod parameter: %s", szName);
		}
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
	else if (_stricmp(szTag, g_szVcaTag) == 0)
	{
		stack_string fullName(g_szVcaPrefix);
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		fullName += szName;
		FMOD_GUID guid = { 0 };

		if (g_pStudioSystem->lookupID(fullName.c_str(), &guid) == FMOD_OK)
		{
			FMOD::Studio::VCA* pVca = nullptr;
			CRY_VERIFY(g_pStudioSystem->getVCAByID(&guid, &pVca) == FMOD_OK);

			char const* const szValue = rootNode->getAttr(g_szValueAttribute);
			auto const value = static_cast<float>(atof(szValue));

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CVcaState");

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
			pISwitchStateConnection = static_cast<ISwitchStateConnection*>(new CVcaState(pVca, value, szName));
#else
			pISwitchStateConnection = static_cast<ISwitchStateConnection*>(new CVcaState(pVca, value));
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
		}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod VCA: %s", szName);
		}
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Fmod tag: %s", szTag);
	}
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

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

	if (_stricmp(szTag, g_szBusTag) == 0)
	{
		stack_string fullName(g_szBusPrefix);
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		fullName += szName;
		FMOD_GUID guid = { 0 };

		if (g_pStudioSystem->lookupID(fullName.c_str(), &guid) == FMOD_OK)
		{
			FMOD::Studio::Bus* pBus = nullptr;
			CRY_VERIFY(g_pStudioSystem->getBusByID(&guid, &pBus) == FMOD_OK);

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CReturn");

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
			pIEnvironmentConnection = static_cast<IEnvironmentConnection*>(new CReturn(pBus, szName));
#else
			pIEnvironmentConnection = static_cast<IEnvironmentConnection*>(new CReturn(pBus));
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
		}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod bus: %s", szName);
		}
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
	else if (_stricmp(szTag, g_szParameterTag) == 0)
	{
		stack_string fullName(g_szParameterPrefix);
		char const* const szName = rootNode->getAttr(g_szNameAttribute);
		fullName += szName;
		FMOD_GUID guid = { 0 };

		if (g_pStudioSystem->lookupID(fullName.c_str(), &guid) == FMOD_OK)
		{
			CParameterInfo const parameterInfo(fullName.c_str());

			bool isAdvanced = false;

			float multiplier = g_defaultParamMultiplier;
			float shift = g_defaultParamShift;

			isAdvanced |= rootNode->getAttr(g_szMutiplierAttribute, multiplier);
			isAdvanced |= rootNode->getAttr(g_szShiftAttribute, shift);

			if (isAdvanced)
			{
				MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CParameterEnvironmentAdvanced");
				pIEnvironmentConnection = static_cast<IEnvironmentConnection*>(new CParameterEnvironmentAdvanced(parameterInfo, multiplier, shift));
			}
			else
			{
				MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CParameterEnvironment");
				pIEnvironmentConnection = static_cast<IEnvironmentConnection*>(new CParameterEnvironment(parameterInfo));
			}
		}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod parameter: %s", szName);
		}
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Fmod tag: %s", szTag);
	}
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

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
void CImpl::OnRefresh()
{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	g_vcaValues.clear();
#endif    // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	ClearActiveSnapshots();
	UnloadMasterBanks();
	LoadMasterBanks();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetLanguage(char const* const szLanguage)
{
	if (szLanguage != nullptr)
	{
		m_language = szLanguage;
		m_localizedSoundBankFolder = PathUtil::GetLocalizationFolder().c_str();
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += m_language.c_str();
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += CRY_AUDIO_DATA_ROOT;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += g_szImplFolderName;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += g_szAssetsFolderName;
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::CreateVersionString(CryFixedStringT<MaxInfoStringLength>& string) const
{
	// Remove the leading zeros on the upper 16 bit and inject the 2 dots between the 3 groups
	size_t const stringLength = string.size();
	for (size_t i = 0; i < stringLength; ++i)
	{
		if (string.c_str()[0] == '0')
		{
			string.erase(0, 1);
		}
		else
		{
			if (i < 4)
			{
				string.insert(4 - i, '.'); // First dot
				string.insert(7 - i, '.'); // Second dot
				break;
			}
			else
			{
				// This shouldn't happen therefore clear the string and back out
				string.clear();
				return;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
FMOD_RESULT F_CALLBACK FmodFileOpenCallback(const char* szName, unsigned int* pFileSize, void** pHandle, void* pUserData)
{
	FMOD_RESULT fmodResult = FMOD_ERR_FILE_NOTFOUND;

	char const* szFileName = (char const*)pUserData;
	FILE* const pFile = gEnv->pCryPak->FOpen(szFileName, "rb");

	if (pFile != nullptr)
	{
		gEnv->pCryPak->FSeek(pFile, 0, SEEK_END);
		auto const fileSize = static_cast<size_t>(gEnv->pCryPak->FTell(pFile));
		gEnv->pCryPak->FSeek(pFile, 0, SEEK_SET);

		if (fileSize >= g_maxFileSize)
		{
			gEnv->pCryPak->FClose(pFile);
			fmodResult = FMOD_ERR_FILE_BAD;
		}
		else
		{
			*pFileSize = static_cast<unsigned int>(fileSize);
			*pHandle = pFile;
			fmodResult = FMOD_OK;
		}
	}

	return fmodResult;
}

//////////////////////////////////////////////////////////////////////////
FMOD_RESULT F_CALLBACK FmodFileCloseCallback(void* pHandle, void* pUserData)
{
	FMOD_RESULT fmodResult = FMOD_ERR_FILE_NOTFOUND;
	auto const pFile = static_cast<FILE*>(pHandle);

	if (gEnv->pCryPak->FClose(pFile) == 0)
	{
		fmodResult = FMOD_OK;
	}

	return fmodResult;
}

//////////////////////////////////////////////////////////////////////////
FMOD_RESULT F_CALLBACK FmodFileReadCallback(void* pHandle, void* pBuffer, unsigned int sizeBytes, unsigned int* pBytesRead, void* pUserData)
{
	FMOD_RESULT fmodResult = FMOD_ERR_FILE_NOTFOUND;
	auto const pFile = static_cast<FILE*>(pHandle);
	size_t const bytesRead = gEnv->pCryPak->FReadRaw(pBuffer, 1, static_cast<size_t>(sizeBytes), pFile);
	*pBytesRead = bytesRead;

	if (bytesRead != sizeBytes)
	{
		fmodResult = FMOD_ERR_FILE_EOF;
	}
	else if (bytesRead > 0)
	{
		fmodResult = FMOD_OK;
	}

	return fmodResult;
}

//////////////////////////////////////////////////////////////////////////
FMOD_RESULT F_CALLBACK FmodFileSeekCallback(void* pHandle, unsigned int pos, void* pUserData)
{
	FMOD_RESULT fmodResult = FMOD_ERR_FILE_COULDNOTSEEK;
	auto const pFile = static_cast<FILE*>(pHandle);

	if (gEnv->pCryPak->FSeek(pFile, static_cast<long>(pos), 0) == 0)
	{
		fmodResult = FMOD_OK;
	}

	return fmodResult;
}

//////////////////////////////////////////////////////////////////////////
FMOD_RESULT CImpl::LoadBankCustom(char const* const szFileName, FMOD::Studio::Bank** ppBank)
{
	FMOD_STUDIO_BANK_INFO bankInfo;
	ZeroStruct(bankInfo);
	bankInfo.opencallback = &FmodFileOpenCallback;
	bankInfo.closecallback = &FmodFileCloseCallback;
	bankInfo.readcallback = &FmodFileReadCallback;
	bankInfo.seekcallback = &FmodFileSeekCallback;
	bankInfo.size = sizeof(bankInfo);
	bankInfo.userdata = reinterpret_cast<void*>(const_cast<char*>(szFileName));

	return g_pStudioSystem->loadBankCustom(&bankInfo, FMOD_STUDIO_LOAD_BANK_NORMAL, ppBank);
}

//////////////////////////////////////////////////////////////////////////
void CImpl::LoadMasterBanks()
{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	bool masterBanksLoaded = false;
#endif        // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	CRY_ASSERT_MESSAGE(g_masterBanks.empty(), "g_masterBanks must be empty before calling %s", __FUNCTION__);

	CryFixedStringT<MaxFilePathLength + MaxFileNameLength> search(m_regularSoundBankFolder + "/*.bank");
	_finddata_t fd;
	intptr_t const handle = gEnv->pCryPak->FindFirst(search.c_str(), &fd);

	if (handle != -1)
	{
		do
		{
			CryFixedStringT<MaxFilePathLength> masterStringsBankPath = fd.name;
			size_t const substrPos = masterStringsBankPath.find(".strings.bank");

			if (substrPos != masterStringsBankPath.npos)
			{
				CryFixedStringT<MaxFilePathLength> masterBankPath = m_regularSoundBankFolder.c_str();
				masterBankPath += "/";
				masterBankPath += masterStringsBankPath.substr(0, substrPos);
				CryFixedStringT<MaxFilePathLength> masterAssetsBankPath = masterBankPath;
				CryFixedStringT<MaxFilePathLength> masterStreamsBankPath = masterBankPath;
				masterBankPath += ".bank";
				masterAssetsBankPath += ".assets.bank";
				masterStreamsBankPath += ".streams.bank";
				masterStringsBankPath.insert(0, "/");
				masterStringsBankPath.insert(0, m_regularSoundBankFolder.c_str());

				g_masterBanks.emplace_back(SMasterBank(masterBankPath, masterStringsBankPath, masterAssetsBankPath, masterStreamsBankPath));
			}

		}
		while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}

	bool busesLoaded = false;

	for (auto& masterBank : g_masterBanks)
	{
		if (!masterBank.bankPath.empty() && !masterBank.stringsBankPath.empty())
		{
			size_t const masterBankFileSize = gEnv->pCryPak->FGetSize(masterBank.bankPath.c_str());
			CRY_ASSERT_MESSAGE(masterBankFileSize > 0, "Master bank doesn't exist during %s", __FUNCTION__);
			size_t const masterBankStringsFileSize = gEnv->pCryPak->FGetSize(masterBank.stringsBankPath.c_str());
			CRY_ASSERT_MESSAGE(masterBankStringsFileSize > 0, "Master strings bank doesn't exist during %s", __FUNCTION__);

			if (masterBankFileSize > 0 && masterBankStringsFileSize > 0)
			{
				CRY_VERIFY(LoadBankCustom(masterBank.bankPath.c_str(), &masterBank.pBank) == FMOD_OK);
				CRY_VERIFY(LoadBankCustom(masterBank.stringsBankPath.c_str(), &masterBank.pStringsBank) == FMOD_OK);

				if (masterBank.pBank != nullptr)
				{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
					g_masterBankSize += masterBankFileSize;
					g_masterBankSize += masterBankStringsFileSize;
#endif          // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

					if (!busesLoaded)
					{
						int numBuses = 0;
						CRY_VERIFY(masterBank.pBank->getBusCount(&numBuses) == FMOD_OK);

						if (numBuses > 0)
						{
							MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "FMOD::Studio::Bus*[]");
							FMOD::Studio::Bus** pBuses = new FMOD::Studio::Bus*[numBuses];
							int numRetrievedBuses = 0;
							CRY_VERIFY(masterBank.pBank->getBusList(pBuses, numBuses, &numRetrievedBuses) == FMOD_OK);
							CRY_ASSERT(numBuses == numRetrievedBuses);

							for (int i = 0; i < numRetrievedBuses; ++i)
							{
								CRY_VERIFY(pBuses[i]->lockChannelGroup() == FMOD_OK);
							}

							delete[] pBuses;

							busesLoaded = true;
						}
					}
				}

				size_t const masterBankAssetsFileSize = gEnv->pCryPak->FGetSize(masterBank.assetsBankPath.c_str());

				if (masterBankAssetsFileSize > 0)
				{
					CRY_VERIFY(LoadBankCustom(masterBank.assetsBankPath.c_str(), &masterBank.pAssetsBank) == FMOD_OK);

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
					g_masterBankSize += masterBankAssetsFileSize;
#endif          // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
				}

				size_t const masterBankStreamsFileSize = gEnv->pCryPak->FGetSize(masterBank.streamsBankPath.c_str());

				if (masterBankStreamsFileSize > 0)
				{
					CRY_VERIFY(LoadBankCustom(masterBank.streamsBankPath.c_str(), &masterBank.pStreamsBank) == FMOD_OK);
				}
			}

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
			masterBanksLoaded = true;
#endif        // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
		}
	}

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	if (!masterBanksLoaded)
	{
		// This does not qualify for a fallback to the NULL implementation!
		// Still notify the user about this failure!
		Cry::Audio::Log(ELogType::Error, "Fmod failed to load master banks during %s", __FUNCTION__);
	}
#endif        // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CImpl::UnloadMasterBanks()
{
	for (auto& masterBank : g_masterBanks)
	{
		if (masterBank.pStringsBank != nullptr)
		{
			CRY_VERIFY(masterBank.pStringsBank->unload() == FMOD_OK);
		}

		if (masterBank.pStreamsBank != nullptr)
		{
			CRY_VERIFY(masterBank.pStreamsBank->unload() == FMOD_OK);
		}

		if (masterBank.pAssetsBank != nullptr)
		{
			CRY_VERIFY(masterBank.pAssetsBank->unload() == FMOD_OK);
		}

		if (masterBank.pBank != nullptr)
		{
			CRY_VERIFY(masterBank.pBank->unload() == FMOD_OK);
		}
	}

	g_masterBanks.clear();

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	g_masterBankSize = 0;
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CImpl::MuteMasterBus(bool const shouldMute)
{
	FMOD::Studio::Bus* pMasterBus = nullptr;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	if (g_pStudioSystem->getBus(g_szBusPrefix, &pMasterBus) != FMOD_OK)
	{
		Cry::Audio::Log(ELogType::Error, "Failed to get master bus during %s", __FUNCTION__);
	}
#else
	g_pStudioSystem->getBus(g_szBusPrefix, &pMasterBus);
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	if (pMasterBus != nullptr)
	{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		if (pMasterBus->setMute(shouldMute) != FMOD_OK)
		{
			char const* const szMuteUnmute = shouldMute ? "mute" : "unmute";
			Cry::Audio::Log(ELogType::Error, "Failed to %s master bus during %s", szMuteUnmute, __FUNCTION__);
		}
#else
		pMasterBus->setMute(shouldMute);
#endif    // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::PauseMasterBus(bool const shouldPause)
{
	FMOD::Studio::Bus* pMasterBus = nullptr;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	if (g_pStudioSystem->getBus(g_szBusPrefix, &pMasterBus) != FMOD_OK)
	{
		Cry::Audio::Log(ELogType::Error, "Failed to get master bus during %s", __FUNCTION__);
	}
#else
	g_pStudioSystem->getBus(g_szBusPrefix, &pMasterBus);
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	if (pMasterBus != nullptr)
	{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		if (pMasterBus->setPaused(shouldPause) != FMOD_OK)
		{
			char const* const szPauseResume = shouldPause ? "pause" : "resume";
			Cry::Audio::Log(ELogType::Error, "Failed to %s master bus during %s", szPauseResume, __FUNCTION__);
		}
#else
		pMasterBus->setPaused(shouldPause);
#endif    // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

		g_masterBusPaused = shouldPause;
	}
}

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
CEventInstance* CImpl::ConstructEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event, CObject const& object)
{
	event.IncrementNumInstances();
	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CEventInstance");

	auto const pEventInstance = new CEventInstance(triggerInstanceId, event, object);
	g_constructedEventInstances.push_back(pEventInstance);

	return pEventInstance;
}
#else
//////////////////////////////////////////////////////////////////////////
CEventInstance* CImpl::ConstructEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event)
{
	event.IncrementNumInstances();

	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CEventInstance");
	return new CEventInstance(triggerInstanceId, event);
}
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructEventInstance(CEventInstance const* const pEventInstance)
{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
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
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	CEvent* const pEvent = &pEventInstance->GetEvent();
	delete pEventInstance;

	pEvent->DecrementNumInstances();

	if (pEvent->CanBeDestructed())
	{
		delete pEvent;
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DrawDebugMemoryInfo(IRenderAuxGeom& auxGeom, float const posX, float& posY, bool const drawDetailedInfo)
{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
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
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Parameter Environments", g_poolSizes.parameterEnvironments);
		}
	}

	if (g_debugPoolSizes.parameterEnvironmentsAdvanced > 0)
	{
		auto& allocator = CParameterEnvironmentAdvanced::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Advanced Parameter Environments", g_poolSizes.parameterEnvironmentsAdvanced);
		}
	}

	if (g_debugPoolSizes.parameterStates > 0)
	{
		auto& allocator = CParameterState::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Parameter States", g_poolSizes.parameterStates);
		}
	}

	if (g_debugPoolSizes.snapshots > 0)
	{
		auto& allocator = CSnapshot::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Snapshots", g_poolSizes.snapshots);
		}
	}

	if (g_debugPoolSizes.returns > 0)
	{
		auto& allocator = CReturn::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Returns", g_poolSizes.returns);
		}
	}

	if (g_debugPoolSizes.vcas > 0)
	{
		auto& allocator = CVca::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "VCAs", g_poolSizes.vcas);
		}
	}

	if (g_debugPoolSizes.vcaStates > 0)
	{
		auto& allocator = CVcaState::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "VCA States", g_poolSizes.vcaStates);
		}
	}

	if (g_debugPoolSizes.banks > 0)
	{
		auto& allocator = CBank::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Banks", g_poolSizes.banks);
		}
	}

	CryModuleMemoryInfo memInfo;
	ZeroStruct(memInfo);

	#if !defined(_LIB)
	// This works differently in monolithic builds and therefore doesn't cater to our needs.
	CryGetMemoryInfoForModule(&memInfo);
	#endif // _LIB

	CryFixedStringT<Debug::MaxMemInfoStringLength> memAllocSizeString;
	auto const memAllocSize = static_cast<size_t>(memInfo.allocated - memInfo.freed);
	Debug::FormatMemoryString(memAllocSizeString, memAllocSize);

	CryFixedStringT<Debug::MaxMemInfoStringLength> totalPoolSizeString;
	Debug::FormatMemoryString(totalPoolSizeString, totalPoolSize);

	CryFixedStringT<Debug::MaxMemInfoStringLength> totalMasterBankSizeString;
	Debug::FormatMemoryString(totalMasterBankSizeString, g_masterBankSize);

	CryFixedStringT<Debug::MaxMemInfoStringLength> totalMemSizeString;
	size_t const totalMemSize = memAllocSize + totalPoolSize + g_masterBankSize;
	Debug::FormatMemoryString(totalMemSizeString, totalMemSize);

	auxGeom.Draw2dLabel(posX, headerPosY, Debug::g_systemHeaderFontSize, Debug::s_globalColorHeader, false, "%s (System: %s | Pools: %s | Master Bank: %s | Total: %s)",
	                    m_name.c_str(), memAllocSizeString.c_str(), totalPoolSizeString.c_str(), totalMasterBankSizeString.c_str(), totalMemSizeString.c_str());

	size_t const numEvents = g_constructedEventInstances.size();
	size_t const numListeners = g_constructedListeners.size();

	posY += Debug::g_systemLineHeight;
	auxGeom.Draw2dLabel(posX, posY, Debug::g_systemFontSize, Debug::s_systemColorTextSecondary, false, "Active Events: %3" PRISIZE_T " | Listeners: %" PRISIZE_T " | Objects with Doppler: %u",
	                    numEvents, numListeners, g_numObjectsWithDoppler);

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
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DrawDebugInfoList(IRenderAuxGeom& auxGeom, float& posX, float posY, Vec3 const& camPos, float const debugDistance, char const* const szTextFilter) const
{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	if ((g_cvars.m_debugListFilter & g_debugListMask) != 0)
	{
		CryFixedStringT<MaxControlNameLength> lowerCaseSearchString(szTextFilter);
		lowerCaseSearchString.MakeLower();

		float const initialPosY = posY;

		if ((g_cvars.m_debugListFilter & EDebugListFilter::EventInstances) != 0)
		{
			auxGeom.Draw2dLabel(posX, posY, Debug::g_listHeaderFontSize, Debug::s_globalColorHeader, false, "Fmod Events [%" PRISIZE_T "]", g_constructedEventInstances.size());
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
						EEventState const state = pEventInstance->GetState();
						ColorF color = Debug::s_listColorItemActive;

						if (state == EEventState::Pending)
						{
							color = Debug::s_listColorItemLoading;
						}
						else if (state == EEventState::Virtual)
						{
							color = Debug::s_globalColorVirtual;
						}
						else if (pEventInstance->IsFadingOut())
						{
							color = Debug::s_listColorItemStopping;
						}

						auxGeom.Draw2dLabel(posX, posY, Debug::g_listFontSize, color, false, "%s on %s (%s)",
						                    szEventName, pEventInstance->GetObject().GetName(), pEventInstance->GetObject().GetListenerNames());

						posY += Debug::g_listLineHeight;
					}
				}
			}

			posX += 600.0f;
		}

		if ((g_cvars.m_debugListFilter & EDebugListFilter::Snapshots) != 0)
		{
			posY = initialPosY;

			auxGeom.Draw2dLabel(posX, posY, Debug::g_listHeaderFontSize, Debug::s_globalColorHeader, false, "Fmod Snapshots [%" PRISIZE_T "]", g_activeSnapshotNames.size());
			posY += Debug::g_listHeaderLineHeight;

			for (auto const& snapshotName : g_activeSnapshotNames)
			{
				CryFixedStringT<MaxControlNameLength> lowerCaseSnapshotName(snapshotName);
				lowerCaseSnapshotName.MakeLower();
				bool const draw = ((lowerCaseSearchString.empty() || (lowerCaseSearchString == "0")) || (lowerCaseSnapshotName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos));

				if (draw)
				{
					auxGeom.Draw2dLabel(posX, posY, Debug::g_listFontSize, Debug::s_listColorItemActive, false, "%s", snapshotName.c_str());

					posY += Debug::g_listLineHeight;
				}
			}

			posX += 300.0f;
		}

		if ((g_cvars.m_debugListFilter & EDebugListFilter::Vcas) != 0)
		{
			posY = initialPosY;

			auxGeom.Draw2dLabel(posX, posY, Debug::g_listHeaderFontSize, Debug::s_globalColorHeader, false, "Fmod VCAs [%" PRISIZE_T "]", g_vcaValues.size());
			posY += Debug::g_listHeaderLineHeight;

			for (auto const& vcaPair : g_vcaValues)
			{
				char const* const szVcaName = vcaPair.first.c_str();
				CryFixedStringT<MaxControlNameLength> lowerCaseVcaName(szVcaName);
				lowerCaseVcaName.MakeLower();
				bool const draw = ((lowerCaseSearchString.empty() || (lowerCaseSearchString == "0")) || (lowerCaseVcaName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos));

				if (draw)
				{
					auxGeom.Draw2dLabel(posX, posY, Debug::g_listFontSize, Debug::s_listColorItemActive, false, "%s : %3.2f", szVcaName, vcaPair.second);

					posY += Debug::g_listLineHeight;
				}
			}

			posX += 300.0f;
		}
	}
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
}
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio

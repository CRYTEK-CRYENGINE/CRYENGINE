// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioImpl.h"
#include "AudioEvent.h"
#include "AudioObject.h"
#include "AudioImplCVars.h"
#include "ATLEntities.h"
#include "GlobalData.h"
#include <Logger.h>
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/IProjectManager.h>
#include <CryAudio/IAudioSystem.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
TriggerToParameterIndexes g_triggerToParameterIndexes;

char const* const CImpl::s_szEventPrefix = "event:/";
char const* const CImpl::s_szSnapshotPrefix = "snapshot:/";
char const* const CImpl::s_szBusPrefix = "bus:/";
char const* const CImpl::s_szVcaPrefix = "vca:/";

static constexpr size_t s_maxFileSize = size_t(1) << size_t(31);

///////////////////////////////////////////////////////////////////////////
CImpl::CImpl()
	: m_pSystem(nullptr)
	, m_pLowLevelSystem(nullptr)
	, m_pMasterBank(nullptr)
	, m_pMasterAssetsBank(nullptr)
	, m_pMasterStreamsBank(nullptr)
	, m_pMasterStringsBank(nullptr)
	, m_isMuted(false)
{
	m_constructedObjects.reserve(256);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::Update()
{
	if (m_pSystem != nullptr)
	{
		FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;
		fmodResult = m_pSystem->update();
		ASSERT_FMOD_OK;
	}
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::Init(uint32 const objectPoolSize, uint32 const eventPoolSize)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Fmod Object Pool");
	CObject::CreateAllocator(objectPoolSize);

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Fmod Event Pool");
	CEvent::CreateAllocator(eventPoolSize);

	m_regularSoundBankFolder = AUDIO_SYSTEM_DATA_ROOT;
	m_regularSoundBankFolder += "/";
	m_regularSoundBankFolder += s_szImplFolderName;
	m_regularSoundBankFolder += "/";
	m_regularSoundBankFolder += s_szAssetsFolderName;
	m_localizedSoundBankFolder = m_regularSoundBankFolder;

	FMOD_RESULT fmodResult = FMOD::Studio::System::create(&m_pSystem);
	ASSERT_FMOD_OK;
	fmodResult = m_pSystem->getLowLevelSystem(&m_pLowLevelSystem);
	ASSERT_FMOD_OK;

#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	uint32 version = 0;
	fmodResult = m_pLowLevelSystem->getVersion(&version);
	ASSERT_FMOD_OK;

	CryFixedStringT<MaxInfoStringLength> systemVersion;
	systemVersion.Format("%08x", version);
	CryFixedStringT<MaxInfoStringLength> headerVersion;
	headerVersion.Format("%08x", FMOD_VERSION);
	CreateVersionString(systemVersion);
	CreateVersionString(headerVersion);

	m_name = FMOD_IMPL_INFO_STRING;
	m_name += "System: ";
	m_name += systemVersion;
	m_name += " - Header: ";
	m_name += headerVersion;
#endif  // INCLUDE_FMOD_IMPL_PRODUCTION_CODE

	int sampleRate = 0;
	int numRawSpeakers = 0;
	FMOD_SPEAKERMODE speakerMode = FMOD_SPEAKERMODE_DEFAULT;
	fmodResult = m_pLowLevelSystem->getSoftwareFormat(&sampleRate, &speakerMode, &numRawSpeakers);
	ASSERT_FMOD_OK;
	fmodResult = m_pLowLevelSystem->setSoftwareFormat(sampleRate, speakerMode, numRawSpeakers);
	ASSERT_FMOD_OK;

	fmodResult = m_pLowLevelSystem->set3DSettings(g_cvars.m_dopplerScale, g_cvars.m_distanceFactor, g_cvars.m_rolloffScale);
	ASSERT_FMOD_OK;

	void* pExtraDriverData = nullptr;
	int initFlags = FMOD_INIT_NORMAL | FMOD_INIT_VOL0_BECOMES_VIRTUAL;
	int studioInitFlags = FMOD_STUDIO_INIT_NORMAL;

	if (g_cvars.m_enableLiveUpdate > 0)
	{
		studioInitFlags |= FMOD_STUDIO_INIT_LIVEUPDATE;
	}

	if (g_cvars.m_enableSynchronousUpdate > 0)
	{
		studioInitFlags |= FMOD_STUDIO_INIT_SYNCHRONOUS_UPDATE;
	}

	fmodResult = m_pSystem->initialize(g_cvars.m_maxChannels, studioInitFlags, initFlags, pExtraDriverData);
	ASSERT_FMOD_OK;

	if (!LoadMasterBanks())
	{
		return ERequestStatus::Failure;
	}

	FMOD_3D_ATTRIBUTES attributes = {
		{ 0 }
	};
	attributes.forward.z = 1.0f;
	attributes.up.y = 1.0f;
	fmodResult = m_pSystem->setListenerAttributes(0, &attributes);
	ASSERT_FMOD_OK;

	CObjectBase::s_pSystem = m_pSystem;
	CListener::s_pSystem = m_pSystem;
	CStandaloneFileBase::s_pLowLevelSystem = m_pLowLevelSystem;

	return (fmodResult == FMOD_OK) ? ERequestStatus::Success : ERequestStatus::Failure;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::OnBeforeShutDown()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::ShutDown()
{
	FMOD_RESULT fmodResult = FMOD_OK;

	if (m_pSystem != nullptr)
	{
		UnloadMasterBanks();

		fmodResult = m_pSystem->release();
		ASSERT_FMOD_OK;
	}

	return (fmodResult == FMOD_OK) ? ERequestStatus::Success : ERequestStatus::Failure;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::Release()
{
	delete this;
	g_cvars.UnregisterVariables();

	CObject::FreeMemoryPool();
	CEvent::FreeMemoryPool();

	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::OnLoseFocus()
{
	if (!m_isMuted)
	{
		MuteMasterBus(true);
	}

	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::OnGetFocus()
{
	if (!m_isMuted)
	{
		MuteMasterBus(false);
	}

	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::MuteAll()
{
	MuteMasterBus(true);
	m_isMuted = true;
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::UnmuteAll()
{
	MuteMasterBus(false);
	m_isMuted = false;
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::PauseAll()
{
	PauseMasterBus(true);
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::ResumeAll()
{
	PauseMasterBus(false);
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::StopAllSounds()
{
	FMOD::Studio::Bus* pMasterBus = nullptr;
	FMOD_RESULT fmodResult = m_pSystem->getBus(s_szBusPrefix, &pMasterBus);
	ASSERT_FMOD_OK;

	if (pMasterBus != nullptr)
	{
		fmodResult = pMasterBus->stopAllEvents(FMOD_STUDIO_STOP_IMMEDIATE);
		ASSERT_FMOD_OK;
	}

	return (fmodResult == FMOD_OK) ? ERequestStatus::Success : ERequestStatus::Failure;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::RegisterInMemoryFile(SFileInfo* const pFileInfo)
{
	ERequestStatus requestResult = ERequestStatus::Failure;

	if (pFileInfo != nullptr)
	{
		auto const pFileData = static_cast<CFile*>(pFileInfo->pImplData);

		if (pFileData != nullptr)
		{
#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
			// loadBankMemory requires 32-byte alignment when using FMOD_STUDIO_LOAD_MEMORY_POINT
			if ((reinterpret_cast<uintptr_t>(pFileInfo->pFileData) & (32 - 1)) > 0)
			{
				CryFatalError("<Audio>: allocation not %d byte aligned!", 32);
			}
#endif      // INCLUDE_FMOD_IMPL_PRODUCTION_CODE

			FMOD_RESULT fmodResult = m_pSystem->loadBankMemory(static_cast<char*>(pFileInfo->pFileData), static_cast<int>(pFileInfo->size), FMOD_STUDIO_LOAD_MEMORY_POINT, FMOD_STUDIO_LOAD_BANK_NORMAL, &pFileData->pBank);
			ASSERT_FMOD_OK;

			CryFixedStringT<MaxFilePathLength> streamsBankPath = pFileInfo->szFilePath;
			PathUtil::RemoveExtension(streamsBankPath);
			streamsBankPath += ".streams.bank";

			size_t const streamsBankFileSize = gEnv->pCryPak->FGetSize(streamsBankPath.c_str());

			if (streamsBankFileSize > 0)
			{
				pFileData->m_streamsBankPath = streamsBankPath;
				fmodResult = LoadBankCustom(pFileData->m_streamsBankPath.c_str(), &pFileData->pStreamsBank);
				ASSERT_FMOD_OK;
			}

			requestResult = (fmodResult == FMOD_OK) ? ERequestStatus::Success : ERequestStatus::Failure;
		}
		else
		{
			Cry::Audio::Log(ELogType::Error, "Invalid FileData passed to the Fmod implementation of RegisterInMemoryFile");
		}
	}

	return requestResult;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::UnregisterInMemoryFile(SFileInfo* const pFileInfo)
{
	ERequestStatus requestResult = ERequestStatus::Failure;

	if (pFileInfo != nullptr)
	{
		auto const pFileData = static_cast<CFile*>(pFileInfo->pImplData);

		if (pFileData != nullptr)
		{
			FMOD_RESULT fmodResult = pFileData->pBank->unload();
			ASSERT_FMOD_OK;

			FMOD_STUDIO_LOADING_STATE loadingState;

			do
			{
				fmodResult = m_pSystem->update();
				ASSERT_FMOD_OK;
				fmodResult = pFileData->pBank->getLoadingState(&loadingState);
				ASSERT_FMOD_OK_OR_INVALID_HANDLE;
			}
			while (loadingState == FMOD_STUDIO_LOADING_STATE_UNLOADING);

			pFileData->pBank = nullptr;

			if (pFileData->pStreamsBank != nullptr)
			{
				fmodResult = pFileData->pStreamsBank->unload();
				ASSERT_FMOD_OK;
				pFileData->pStreamsBank = nullptr;
			}

			requestResult = (fmodResult == FMOD_OK) ? ERequestStatus::Success : ERequestStatus::Failure;
		}
		else
		{
			Cry::Audio::Log(ELogType::Error, "Invalid FileData passed to the Fmod implementation of UnregisterInMemoryFile");
		}
	}

	return requestResult;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::ConstructFile(XmlNodeRef const pRootNode, SFileInfo* const pFileInfo)
{
	ERequestStatus result = ERequestStatus::Failure;

	if ((_stricmp(pRootNode->getTag(), s_szFileTag) == 0) && (pFileInfo != nullptr))
	{
		char const* const szFileName = pRootNode->getAttr(s_szNameAttribute);

		if (szFileName != nullptr && szFileName[0] != '\0')
		{
			char const* const szLocalized = pRootNode->getAttr(s_szLocalizedAttribute);
			pFileInfo->bLocalized = (szLocalized != nullptr) && (_stricmp(szLocalized, s_szTrueValue) == 0);
			pFileInfo->szFileName = szFileName;

			// FMOD Studio always uses 32 byte alignment for preloaded banks regardless of the platform.
			pFileInfo->memoryBlockAlignment = 32;

			pFileInfo->pImplData = new CFile();

			result = ERequestStatus::Success;
		}
		else
		{
			pFileInfo->szFileName = nullptr;
			pFileInfo->memoryBlockAlignment = 0;
			pFileInfo->pImplData = nullptr;
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructFile(IFile* const pIFile)
{
	delete pIFile;
}

//////////////////////////////////////////////////////////////////////////
char const* const CImpl::GetFileLocation(SFileInfo* const pFileInfo)
{
	char const* szResult = nullptr;

	if (pFileInfo != nullptr)
	{
		szResult = pFileInfo->bLocalized ? m_localizedSoundBankFolder.c_str() : m_regularSoundBankFolder.c_str();
	}

	return szResult;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GetInfo(SImplInfo& implInfo) const
{
#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	implInfo.name = m_name.c_str();
#else
	implInfo.name = "name-not-present-in-release-mode";
#endif  // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
	implInfo.folderName = s_szImplFolderName;
}

///////////////////////////////////////////////////////////////////////////
IObject* CImpl::ConstructGlobalObject()
{
	CObjectBase* const pObject = new CGlobalObject(m_constructedObjects);

	if (!stl::push_back_unique(m_constructedObjects, pObject))
	{
		Cry::Audio::Log(ELogType::Warning, "Trying to construct an already registered audio object.");
	}

	return static_cast<IObject*>(pObject);
}

///////////////////////////////////////////////////////////////////////////
IObject* CImpl::ConstructObject(char const* const szName /*= nullptr*/)
{
	CObjectBase* pObject = new CObject();

	if (!stl::push_back_unique(m_constructedObjects, pObject))
	{
		Cry::Audio::Log(ELogType::Warning, "Trying to construct an already registered audio object.");
	}

	return static_cast<IObject*>(pObject);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructObject(IObject const* const pIObject)
{
	auto const pObject = static_cast<CObjectBase const*>(pIObject);

	if (!stl::find_and_erase(m_constructedObjects, pObject))
	{
		Cry::Audio::Log(ELogType::Warning, "Trying to delete a non-existing audio object.");
	}

	delete pObject;
}

///////////////////////////////////////////////////////////////////////////
IListener* CImpl::ConstructListener(char const* const szName /*= nullptr*/)
{
	static int id = 0;
	return static_cast<IListener*>(new CListener(id++));
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructListener(IListener* const pIListener)
{
	delete pIListener;
}

//////////////////////////////////////////////////////////////////////////
IEvent* CImpl::ConstructEvent(CATLEvent& event)
{
	return static_cast<IEvent*>(new CEvent(&event));
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructEvent(IEvent const* const pIEvent)
{
	CRY_ASSERT(pIEvent != nullptr);
	delete pIEvent;
}

//////////////////////////////////////////////////////////////////////////
IStandaloneFile* CImpl::ConstructStandaloneFile(CATLStandaloneFile& standaloneFile, char const* const szFile, bool const bLocalized, ITrigger const* pITrigger /*= nullptr*/)
{
	static string s_localizedfilesFolder = PathUtil::GetLocalizationFolder() + "/" + m_language.c_str() + "/";
	string filePath;

	if (bLocalized)
	{
		filePath = s_localizedfilesFolder + szFile + ".mp3";
	}
	else
	{
		filePath = string(szFile) + ".mp3";
	}

	CStandaloneFileBase* pFile = nullptr;

	if (pITrigger != nullptr)
	{
		pFile = new CProgrammerSoundFile(filePath, static_cast<CTrigger const* const>(pITrigger)->GetGuid(), standaloneFile);
	}
	else
	{
		pFile = new CStandaloneFile(filePath, standaloneFile);
	}

	return static_cast<IStandaloneFile*>(pFile);
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructStandaloneFile(IStandaloneFile const* const pIStandaloneFile)
{
	CRY_ASSERT(pIStandaloneFile != nullptr);
	delete pIStandaloneFile;
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
ITrigger const* CImpl::ConstructTrigger(XmlNodeRef const pRootNode)
{
	CTrigger* pTrigger = nullptr;
	char const* const szTag = pRootNode->getTag();

	if (_stricmp(szTag, s_szEventTag) == 0)
	{
		stack_string path(s_szEventPrefix);
		path += pRootNode->getAttr(s_szNameAttribute);
		FMOD_GUID guid = { 0 };

		if (m_pSystem->lookupID(path.c_str(), &guid) == FMOD_OK)
		{
			EEventType eventType = EEventType::Start;
			char const* const szEventType = pRootNode->getAttr(s_szTypeAttribute);

			if ((szEventType != nullptr) && (szEventType[0] != '\0'))
			{
				if (_stricmp(szEventType, s_szStopValue) == 0)
				{
					eventType = EEventType::Stop;
				}
				else if (_stricmp(szEventType, s_szPauseValue) == 0)
				{
					eventType = EEventType::Pause;
				}
				else if (_stricmp(szEventType, s_szResumeValue) == 0)
				{
					eventType = EEventType::Resume;
				}
			}

			pTrigger = new CTrigger(StringToId(path.c_str()), eventType, nullptr, guid);
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod event: %s", path.c_str());
		}
	}
	else if (_stricmp(szTag, s_szSnapshotTag) == 0)
	{
		stack_string path(s_szSnapshotPrefix);
		path += pRootNode->getAttr(s_szNameAttribute);
		FMOD_GUID guid = { 0 };

		if (m_pSystem->lookupID(path.c_str(), &guid) == FMOD_OK)
		{
			EEventType eventType = EEventType::Start;
			char const* const szFmodEventType = pRootNode->getAttr(s_szTypeAttribute);

			if ((szFmodEventType != nullptr) && (szFmodEventType[0] != '\0') && (_stricmp(szFmodEventType, s_szStopValue) == 0))
			{
				eventType = EEventType::Stop;
			}

			FMOD::Studio::EventDescription* pEventDescription = nullptr;
			m_pSystem->getEventByID(&guid, &pEventDescription);

			pTrigger = new CTrigger(StringToId(path.c_str()), eventType, pEventDescription, guid);
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod snapshot: %s", path.c_str());
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Fmod tag: %s", szTag);
	}

	return static_cast<ITrigger*>(pTrigger);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructTrigger(ITrigger const* const pITrigger)
{
	auto const pTrigger = static_cast<CTrigger const*>(pITrigger);
	g_triggerToParameterIndexes.erase(pTrigger);
	delete pTrigger;
}

///////////////////////////////////////////////////////////////////////////
IParameter const* CImpl::ConstructParameter(XmlNodeRef const pRootNode)
{
	CParameter* pParameter = nullptr;
	char const* const szTag = pRootNode->getTag();

	if (_stricmp(szTag, s_szParameterTag) == 0)
	{
		char const* const szName = pRootNode->getAttr(s_szNameAttribute);
		float multiplier = s_defaultParamMultiplier;
		float shift = s_defaultParamShift;
		pRootNode->getAttr(s_szMutiplierAttribute, multiplier);
		pRootNode->getAttr(s_szShiftAttribute, shift);

		pParameter = new CParameter(StringToId(szName), multiplier, shift, szName, EParameterType::Parameter);
	}
	else if (_stricmp(szTag, s_szVcaTag) == 0)
	{
		stack_string fullName(s_szVcaPrefix);
		char const* const szName = pRootNode->getAttr(s_szNameAttribute);
		fullName += szName;
		FMOD_GUID guid = { 0 };

		if (m_pSystem->lookupID(fullName.c_str(), &guid) == FMOD_OK)
		{
			FMOD::Studio::VCA* pVca = nullptr;
			FMOD_RESULT const fmodResult = m_pSystem->getVCAByID(&guid, &pVca);
			ASSERT_FMOD_OK;

			float multiplier = s_defaultParamMultiplier;
			float shift = s_defaultParamShift;
			pRootNode->getAttr(s_szMutiplierAttribute, multiplier);
			pRootNode->getAttr(s_szShiftAttribute, shift);

			pParameter = new CVcaParameter(StringToId(fullName.c_str()), multiplier, shift, szName, pVca);
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod VCA: %s", fullName.c_str());
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Fmod tag: %s", szTag);
	}

	return static_cast<IParameter*>(pParameter);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructParameter(IParameter const* const pIParameter)
{
	auto const pParameter = static_cast<CParameter const*>(pIParameter);

	for (auto const pObject : m_constructedObjects)
	{
		pObject->RemoveParameter(pParameter);
	}

	delete pParameter;
}

///////////////////////////////////////////////////////////////////////////
ISwitchState const* CImpl::ConstructSwitchState(XmlNodeRef const pRootNode)
{
	CSwitchState* pSwitchState = nullptr;
	char const* const szTag = pRootNode->getTag();

	if (_stricmp(szTag, s_szParameterTag) == 0)
	{
		char const* const szName = pRootNode->getAttr(s_szNameAttribute);
		char const* const szValue = pRootNode->getAttr(s_szValueAttribute);
		float const value = static_cast<float>(atof(szValue));
		pSwitchState = new CSwitchState(StringToId(szName), value, szName, EStateType::State);
	}
	else if (_stricmp(szTag, s_szVcaTag) == 0)
	{
		stack_string fullName(s_szVcaPrefix);
		char const* const szName = pRootNode->getAttr(s_szNameAttribute);
		fullName += szName;
		FMOD_GUID guid = { 0 };

		if (m_pSystem->lookupID(fullName.c_str(), &guid) == FMOD_OK)
		{
			FMOD::Studio::VCA* pVca = nullptr;
			FMOD_RESULT const fmodResult = m_pSystem->getVCAByID(&guid, &pVca);
			ASSERT_FMOD_OK;

			char const* const szValue = pRootNode->getAttr(s_szValueAttribute);
			float const value = static_cast<float>(atof(szValue));

			pSwitchState = new CVcaState(StringToId(fullName.c_str()), value, szName, pVca);
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod VCA: %s", fullName.c_str());
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Fmod tag: %s", szTag);
	}

	return static_cast<ISwitchState*>(pSwitchState);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructSwitchState(ISwitchState const* const pISwitchState)
{
	auto const pSwitchState = static_cast<CSwitchState const*>(pISwitchState);

	for (auto const pObject : m_constructedObjects)
	{
		pObject->RemoveSwitch(pSwitchState);
	}

	delete pSwitchState;
}

///////////////////////////////////////////////////////////////////////////
IEnvironment const* CImpl::ConstructEnvironment(XmlNodeRef const pRootNode)
{
	CEnvironment* pEnvironment = nullptr;
	char const* const szTag = pRootNode->getTag();

	if (_stricmp(szTag, s_szBusTag) == 0)
	{
		stack_string path(s_szBusPrefix);
		path += pRootNode->getAttr(s_szNameAttribute);
		FMOD_GUID guid = { 0 };

		if (m_pSystem->lookupID(path.c_str(), &guid) == FMOD_OK)
		{
			FMOD::Studio::Bus* pBus = nullptr;
			FMOD_RESULT const fmodResult = m_pSystem->getBusByID(&guid, &pBus);
			ASSERT_FMOD_OK;
			pEnvironment = new CEnvironmentBus(nullptr, pBus);
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod bus: %s", path.c_str());
		}
	}
	else if (_stricmp(szTag, s_szParameterTag) == 0)
	{

		char const* const szName = pRootNode->getAttr(s_szNameAttribute);
		float multiplier = s_defaultParamMultiplier;
		float shift = s_defaultParamShift;
		pRootNode->getAttr(s_szMutiplierAttribute, multiplier);
		pRootNode->getAttr(s_szShiftAttribute, shift);

		pEnvironment = new CEnvironmentParameter(StringToId(szName), multiplier, shift, szName);
	}
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown Fmod tag: %s", szTag);
	}

	return static_cast<IEnvironment*>(pEnvironment);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructEnvironment(IEnvironment const* const pIEnvironment)
{
	auto const pEnvironment = static_cast<CEnvironment const*>(pIEnvironment);

	for (auto const pObject : m_constructedObjects)
	{
		pObject->RemoveEnvironment(pEnvironment);
	}

	delete pEnvironment;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::GetMemoryInfo(SMemoryInfo& memoryInfo) const
{
	CryModuleMemoryInfo memInfo;
	ZeroStruct(memInfo);
	CryGetMemoryInfoForModule(&memInfo);

	memoryInfo.totalMemory = static_cast<size_t>(memInfo.allocated - memInfo.freed);

#if defined(PROVIDE_FMOD_IMPL_SECONDARY_POOL)
	memoryInfo.secondaryPoolSize = g_audioImplMemoryPoolSecondary.MemSize();
	memoryInfo.secondaryPoolUsedSize = memoryInfo.secondaryPoolSize - g_audioImplMemoryPoolSecondary.MemFree();
	memoryInfo.secondaryPoolAllocations = g_audioImplMemoryPoolSecondary.FragmentCount();
#else
	memoryInfo.secondaryPoolSize = 0;
	memoryInfo.secondaryPoolUsedSize = 0;
	memoryInfo.secondaryPoolAllocations = 0;
#endif  // PROVIDE_AUDIO_IMPL_SECONDARY_POOL

	{
		auto& allocator = CObject::GetAllocator();
		auto mem = allocator.GetTotalMemory();
		auto pool = allocator.GetCounts();
		memoryInfo.poolUsedObjects = pool.nUsed;
		memoryInfo.poolConstructedObjects = pool.nAlloc;
		memoryInfo.poolUsedMemory = mem.nUsed;
		memoryInfo.poolAllocatedMemory = mem.nAlloc;
	}

	{
		auto& allocator = CEvent::GetAllocator();
		auto mem = allocator.GetTotalMemory();
		auto pool = allocator.GetCounts();
		memoryInfo.poolUsedObjects += pool.nUsed;
		memoryInfo.poolConstructedObjects += pool.nAlloc;
		memoryInfo.poolUsedMemory += mem.nUsed;
		memoryInfo.poolAllocatedMemory += mem.nAlloc;
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnRefresh()
{
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
		m_localizedSoundBankFolder += AUDIO_SYSTEM_DATA_ROOT;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += s_szImplFolderName;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += s_szAssetsFolderName;
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::CreateVersionString(CryFixedStringT<MaxInfoStringLength>& stringOut) const
{
	// Remove the leading zeros on the upper 16 bit and inject the 2 dots between the 3 groups
	size_t const stringLength = stringOut.size();
	for (size_t i = 0; i < stringLength; ++i)
	{
		if (stringOut.c_str()[0] == '0')
		{
			stringOut.erase(0, 1);
		}
		else
		{
			if (i < 4)
			{
				stringOut.insert(4 - i, '.'); // First dot
				stringOut.insert(7 - i, '.'); // Second dot
				break;
			}
			else
			{
				// This shouldn't happen therefore clear the string and back out
				stringOut.clear();
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
	FILE* const pFile = gEnv->pCryPak->FOpen(szFileName, "rbx", ICryPak::FOPEN_HINT_DIRECT_OPERATION);

	if (pFile != nullptr)
	{
		gEnv->pCryPak->FSeek(pFile, 0, SEEK_END);
		auto const fileSize = static_cast<size_t>(gEnv->pCryPak->FTell(pFile));
		gEnv->pCryPak->FSeek(pFile, 0, SEEK_SET);

		if (fileSize >= s_maxFileSize)
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
	FILE* const pFile = static_cast<FILE*>(pHandle);

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
	FILE* const pFile = static_cast<FILE*>(pHandle);
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
	FILE* const pFile = static_cast<FILE*>(pHandle);

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

	return m_pSystem->loadBankCustom(&bankInfo, FMOD_STUDIO_LOAD_BANK_NORMAL, ppBank);
}

//////////////////////////////////////////////////////////////////////////
bool CImpl::LoadMasterBanks()
{
	FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;
	m_masterBankPath.clear();
	m_masterAssetsBankPath.clear();
	m_masterStreamsBankPath.clear();
	m_masterStringsBankPath.clear();
	CryFixedStringT<MaxFilePathLength + MaxFileNameLength> search(m_regularSoundBankFolder + "/*.bank");
	_finddata_t fd;
	intptr_t const handle = gEnv->pCryPak->FindFirst(search.c_str(), &fd);

	if (handle != -1)
	{
		do
		{
			m_masterStringsBankPath = fd.name;
			size_t const substrPos = m_masterStringsBankPath.find(".strings.bank");

			if (substrPos != m_masterStringsBankPath.npos)
			{
				m_masterBankPath = m_regularSoundBankFolder.c_str();
				m_masterBankPath += "/";
				m_masterBankPath += m_masterStringsBankPath.substr(0, substrPos);
				m_masterAssetsBankPath = m_masterBankPath;
				m_masterStreamsBankPath = m_masterBankPath;
				m_masterBankPath += ".bank";
				m_masterAssetsBankPath += ".assets.bank";
				m_masterStreamsBankPath += ".streams.bank";
				m_masterStringsBankPath.insert(0, "/");
				m_masterStringsBankPath.insert(0, m_regularSoundBankFolder.c_str());
				break;
			}

		}
		while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}

	if (!m_masterBankPath.empty() && !m_masterStringsBankPath.empty())
	{
		size_t const masterBankFileSize = gEnv->pCryPak->FGetSize(m_masterBankPath.c_str());
		CRY_ASSERT(masterBankFileSize > 0);
		size_t const masterBankStringsFileSize = gEnv->pCryPak->FGetSize(m_masterStringsBankPath.c_str());
		CRY_ASSERT(masterBankStringsFileSize > 0);

		if (masterBankFileSize > 0 && masterBankStringsFileSize > 0)
		{
			fmodResult = LoadBankCustom(m_masterBankPath.c_str(), &m_pMasterBank);
			ASSERT_FMOD_OK;

			fmodResult = LoadBankCustom(m_masterStringsBankPath.c_str(), &m_pMasterStringsBank);
			ASSERT_FMOD_OK;

			if (m_pMasterBank != nullptr)
			{
				int numBuses = 0;
				fmodResult = m_pMasterBank->getBusCount(&numBuses);
				ASSERT_FMOD_OK;

				if (numBuses > 0)
				{
					FMOD::Studio::Bus** pBuses = new FMOD::Studio::Bus*[numBuses];
					int numRetrievedBuses = 0;
					fmodResult = m_pMasterBank->getBusList(pBuses, numBuses, &numRetrievedBuses);
					ASSERT_FMOD_OK;
					CRY_ASSERT(numBuses == numRetrievedBuses);

					for (int i = 0; i < numRetrievedBuses; ++i)
					{
						fmodResult = pBuses[i]->lockChannelGroup();
						ASSERT_FMOD_OK;
					}

					delete[] pBuses;
				}
			}

			size_t const masterBankAssetsFileSize = gEnv->pCryPak->FGetSize(m_masterAssetsBankPath.c_str());

			if (masterBankAssetsFileSize > 0)
			{
				fmodResult = LoadBankCustom(m_masterAssetsBankPath.c_str(), &m_pMasterAssetsBank);
				ASSERT_FMOD_OK;
			}

			size_t const masterBankStreamsFileSize = gEnv->pCryPak->FGetSize(m_masterStreamsBankPath.c_str());

			if (masterBankStreamsFileSize > 0)
			{
				fmodResult = LoadBankCustom(m_masterStreamsBankPath.c_str(), &m_pMasterStreamsBank);
				ASSERT_FMOD_OK;
			}
		}
	}
	else
	{
		// This does not qualify for a fallback to the NULL implementation!
		// Still notify the user about this failure!
		Cry::Audio::Log(ELogType::Error, "Fmod failed to load master banks");
		return true;
	}

	return (fmodResult == FMOD_OK);
}

//////////////////////////////////////////////////////////////////////////
void CImpl::UnloadMasterBanks()
{
	FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

	if (m_pMasterStringsBank != nullptr)
	{
		fmodResult = m_pMasterStringsBank->unload();
		ASSERT_FMOD_OK;
		m_pMasterStringsBank = nullptr;
	}

	if (m_pMasterStreamsBank != nullptr)
	{
		fmodResult = m_pMasterStreamsBank->unload();
		ASSERT_FMOD_OK;
		m_pMasterStreamsBank = nullptr;
	}

	if (m_pMasterAssetsBank != nullptr)
	{
		fmodResult = m_pMasterAssetsBank->unload();
		ASSERT_FMOD_OK;
		m_pMasterAssetsBank = nullptr;
	}

	if (m_pMasterBank != nullptr)
	{
		fmodResult = m_pMasterBank->unload();
		ASSERT_FMOD_OK;
		m_pMasterBank = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::MuteMasterBus(bool const shouldMute)
{
	FMOD::Studio::Bus* pMasterBus = nullptr;
	FMOD_RESULT fmodResult = m_pSystem->getBus(s_szBusPrefix, &pMasterBus);
	ASSERT_FMOD_OK;

	if (pMasterBus != nullptr)
	{
		fmodResult = pMasterBus->setMute(shouldMute);
		ASSERT_FMOD_OK;
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::PauseMasterBus(bool const shouldPause)
{
	FMOD::Studio::Bus* pMasterBus = nullptr;
	FMOD_RESULT fmodResult = m_pSystem->getBus(s_szBusPrefix, &pMasterBus);
	ASSERT_FMOD_OK;

	if (pMasterBus != nullptr)
	{
		fmodResult = pMasterBus->setPaused(shouldPause);
		ASSERT_FMOD_OK;
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GetFileData(char const* const szName, SFileData& fileData) const
{
	FMOD::Sound* pSound = nullptr;
	FMOD_CREATESOUNDEXINFO info;
	ZeroStruct(info);
	info.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
	info.decodebuffersize = 1;
	FMOD_RESULT const fmodResult = m_pLowLevelSystem->createStream(szName, FMOD_OPENONLY, &info, &pSound);
	ASSERT_FMOD_OK;

	if (pSound != nullptr)
	{
		unsigned int length = 0;
		pSound->getLength(&length, FMOD_TIMEUNIT_MS);
		fileData.duration = length / 1000.0f; // convert to seconds
	}
}
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio

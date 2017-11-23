// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioImpl.h"
#include "AudioEvent.h"
#include "AudioObject.h"
#include "AudioImplCVars.h"
#include "ATLEntities.h"
#include <Logger.h>
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/IProjectManager.h>
#include <CryAudio/IAudioSystem.h>
#include <CryString/CryPath.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
ParameterToIndexMap g_parameterToIndex;
SwitchToIndexMap g_switchToIndex;

char const* const CImpl::s_szFmodEventTag = "FmodEvent";
char const* const CImpl::s_szFmodSnapshotTag = "FmodSnapshot";
char const* const CImpl::s_szFmodEventParameterTag = "FmodEventParameter";
char const* const CImpl::s_szFmodSnapshotParameterTag = "FmodSnapshotParameter";
char const* const CImpl::s_szFmodFileTag = "FmodFile";
char const* const CImpl::s_szFmodBusTag = "FmodBus";
char const* const CImpl::s_szFmodNameAttribute = "fmod_name";
char const* const CImpl::s_szFmodValueAttribute = "fmod_value";
char const* const CImpl::s_szFmodMutiplierAttribute = "fmod_value_multiplier";
char const* const CImpl::s_szFmodShiftAttribute = "fmod_value_shift";
char const* const CImpl::s_szFmodPathAttribute = "fmod_path";
char const* const CImpl::s_szFmodLocalizedAttribute = "fmod_localized";
char const* const CImpl::s_szFmodEventTypeAttribute = "fmod_event_type";
char const* const CImpl::s_szFmodEventPrefix = "event:/";
char const* const CImpl::s_szFmodSnapshotPrefix = "snapshot:/";
char const* const CImpl::s_szFmodBusPrefix = "bus:/";

///////////////////////////////////////////////////////////////////////////
CImpl::CImpl()
	: m_pSystem(nullptr)
	, m_pLowLevelSystem(nullptr)
	, m_pMasterBank(nullptr)
	, m_pStringsBank(nullptr)
{
	m_constructedObjects.reserve(256);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::Update(float const deltaTime)
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

	char const* szAssetDirectory = gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryRelative();

	if (strlen(szAssetDirectory) == 0)
	{
		Cry::Audio::Log(ELogType::Error, "<Audio - Fmod>: No asset folder set!");
		szAssetDirectory = "no-asset-folder-set";
	}

	m_regularSoundBankFolder = szAssetDirectory;
	m_regularSoundBankFolder += CRY_NATIVE_PATH_SEPSTR;
	m_regularSoundBankFolder += FMOD_IMPL_DATA_ROOT;
	m_localizedSoundBankFolder = m_regularSoundBankFolder;

	FMOD_RESULT fmodResult = FMOD::Studio::System::create(&m_pSystem);
	ASSERT_FMOD_OK;
	fmodResult = m_pSystem->getLowLevelSystem(&m_pLowLevelSystem);
	ASSERT_FMOD_OK;

#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	uint32 version = 0;
	fmodResult = m_pLowLevelSystem->getVersion(&version);
	ASSERT_FMOD_OK;

	CryFixedStringT<MaxMiscStringLength> systemVersion;
	systemVersion.Format("%08x", version);
	CryFixedStringT<MaxMiscStringLength> headerVersion;
	headerVersion.Format("%08x", FMOD_VERSION);
	CreateVersionString(systemVersion);
	CreateVersionString(headerVersion);

	m_name = FMOD_IMPL_INFO_STRING;
	m_name += "System: ";
	m_name += systemVersion;
	m_name += " Header: ";
	m_name += headerVersion;
	m_name += " (";
	m_name += szAssetDirectory;
	m_name += CRY_NATIVE_PATH_SEPSTR FMOD_IMPL_DATA_ROOT ")";
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
	return MuteMasterBus(true);
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::OnGetFocus()
{
	return MuteMasterBus(false);
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::MuteAll()
{
	return MuteMasterBus(true);
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::UnmuteAll()
{
	return MuteMasterBus(false);
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::StopAllSounds()
{
	FMOD::Studio::Bus* pMasterBus = nullptr;
	FMOD_RESULT fmodResult = m_pSystem->getBus("bus:/", &pMasterBus);
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
		CFile* const pFileData = static_cast<CFile*>(pFileInfo->pImplData);

		if (pFileData != nullptr)
		{
#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
			// loadBankMemory requires 32-byte alignment when using FMOD_STUDIO_LOAD_MEMORY_POINT
			if ((reinterpret_cast<uintptr_t>(pFileInfo->pFileData) & (32 - 1)) > 0)
			{
				CryFatalError("<Audio>: allocation not %d byte aligned!", 32);
			}
#endif      // INCLUDE_FMOD_IMPL_PRODUCTION_CODE

			FMOD_RESULT const fmodResult = m_pSystem->loadBankMemory(static_cast<char*>(pFileInfo->pFileData), static_cast<int>(pFileInfo->size), FMOD_STUDIO_LOAD_MEMORY_POINT, FMOD_STUDIO_LOAD_BANK_NORMAL, &pFileData->pBank);
			ASSERT_FMOD_OK;
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
		CFile* const pFileData = static_cast<CFile*>(pFileInfo->pImplData);

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

	if ((_stricmp(pRootNode->getTag(), s_szFmodFileTag) == 0) && (pFileInfo != nullptr))
	{
		char const* const szFileName = pRootNode->getAttr(s_szFmodNameAttribute);

		if (szFileName != nullptr && szFileName[0] != '\0')
		{
			char const* const szLocalized = pRootNode->getAttr(s_szFmodLocalizedAttribute);
			pFileInfo->bLocalized = (szLocalized != nullptr) && (_stricmp(szLocalized, "true") == 0);
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
	CObjectBase const* const pObject = static_cast<CObjectBase const* const>(pIObject);

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
	static string s_localizedfilesFolder = PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + PathUtil::GetLocalizationFolder() + CRY_NATIVE_PATH_SEPSTR + m_language.c_str() + CRY_NATIVE_PATH_SEPSTR;
	static string s_nonLocalizedfilesFolder = PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR;
	string filePath;

	if (bLocalized)
	{
		filePath = s_localizedfilesFolder + szFile + ".mp3";
	}
	else
	{
		filePath = s_nonLocalizedfilesFolder + szFile + ".mp3";
	}

	CStandaloneFileBase* pFile = nullptr;

	if (pITrigger != nullptr)
	{
		pFile = new CProgrammerSoundFile(filePath, static_cast<CTrigger const* const>(pITrigger)->m_guid, standaloneFile);
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

	if (_stricmp(szTag, s_szFmodEventTag) == 0)
	{
		stack_string path(s_szFmodEventPrefix);
		path += pRootNode->getAttr(s_szFmodNameAttribute);
		FMOD_GUID guid = { 0 };

		if (m_pSystem->lookupID(path.c_str(), &guid) == FMOD_OK)
		{
			EEventType eventType = EEventType::Start;
			char const* const szEventType = pRootNode->getAttr(s_szFmodEventTypeAttribute);

			if (szEventType != nullptr && szEventType[0] != '\0' && _stricmp(szEventType, "stop") == 0)
			{
				eventType = EEventType::Stop;
			}

#if defined (INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
			pTrigger = new CTrigger(StringToId(path.c_str()), eventType, nullptr, guid, path.c_str());
#else
			pTrigger = new CTrigger(StringToId(path.c_str()), eventType, nullptr, guid);
#endif      // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod event: %s", path.c_str());
		}
	}
	else if (_stricmp(szTag, s_szFmodSnapshotTag) == 0)
	{
		stack_string path(s_szFmodSnapshotPrefix);
		path += pRootNode->getAttr(s_szFmodNameAttribute);
		FMOD_GUID guid = { 0 };

		if (m_pSystem->lookupID(path.c_str(), &guid) == FMOD_OK)
		{
			EEventType eventType = EEventType::Start;
			char const* const szFmodEventType = pRootNode->getAttr(s_szFmodEventTypeAttribute);

			if (szFmodEventType != nullptr &&
			    szFmodEventType[0] != '\0' &&
			    _stricmp(szFmodEventType, "stop") == 0)
			{
				eventType = EEventType::Stop;
			}

			FMOD::Studio::EventDescription* pEventDescription = nullptr;
			m_pSystem->getEventByID(&guid, &pEventDescription);

#if defined (INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
			pTrigger = new CTrigger(StringToId(path.c_str()), eventType, pEventDescription, guid, path.c_str());
#else
			pTrigger = new CTrigger(StringToId(path.c_str()), eventType, pEventDescription, guid);
#endif      // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
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
	delete pITrigger;
}

///////////////////////////////////////////////////////////////////////////
IParameter const* CImpl::ConstructParameter(XmlNodeRef const pRootNode)
{
	CParameter* pParameter = nullptr;
	char const* const szTag = pRootNode->getTag();
	stack_string path;

	if (_stricmp(szTag, s_szFmodEventParameterTag) == 0)
	{
		path = s_szFmodEventPrefix;
	}
	else if (_stricmp(szTag, s_szFmodSnapshotParameterTag) == 0)
	{
		path = s_szFmodSnapshotPrefix;
	}

	if (!path.empty())
	{
		char const* const szName = pRootNode->getAttr(s_szFmodNameAttribute);
		char const* const szPath = pRootNode->getAttr(s_szFmodPathAttribute);
		path += szPath;
		uint32 const pathId = StringToId(path.c_str());

		float multiplier = 1.0f;
		float shift = 0.0f;
		pRootNode->getAttr(s_szFmodMutiplierAttribute, multiplier);
		pRootNode->getAttr(s_szFmodShiftAttribute, shift);

		pParameter = new CParameter(pathId, multiplier, shift, szName);
		g_parameterToIndex.emplace(std::piecewise_construct, std::make_tuple(pParameter), std::make_tuple(FMOD_IMPL_INVALID_INDEX));
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
	CParameter const* const pParameter = static_cast<CParameter const* const>(pIParameter);

	for (auto const pObject : m_constructedObjects)
	{
		pObject->RemoveParameter(pParameter);
	}

	g_parameterToIndex.erase(pParameter);
	delete pParameter;
}

///////////////////////////////////////////////////////////////////////////
ISwitchState const* CImpl::ConstructSwitchState(XmlNodeRef const pRootNode)
{
	CSwitchState* pSwitchState = nullptr;
	char const* const szTag = pRootNode->getTag();
	stack_string path;

	if (_stricmp(szTag, s_szFmodEventParameterTag) == 0)
	{
		path = s_szFmodEventPrefix;
	}
	else if (_stricmp(szTag, s_szFmodSnapshotParameterTag) == 0)
	{
		path = s_szFmodSnapshotPrefix;
	}

	if (!path.empty())
	{
		char const* const szFmodParameterName = pRootNode->getAttr(s_szFmodNameAttribute);
		char const* const szFmodPath = pRootNode->getAttr(s_szFmodPathAttribute);
		char const* const szFmodParameterValue = pRootNode->getAttr(s_szFmodValueAttribute);
		path += szFmodPath;
		uint32 const pathId = StringToId(path.c_str());
		float const value = static_cast<float>(atof(szFmodParameterValue));
		pSwitchState = new CSwitchState(pathId, value, szFmodParameterName);
		g_switchToIndex.emplace(std::piecewise_construct, std::make_tuple(pSwitchState), std::make_tuple(FMOD_IMPL_INVALID_INDEX));
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
	CSwitchState const* const pSwitchState = static_cast<CSwitchState const* const>(pISwitchState);

	for (auto const pObject : m_constructedObjects)
	{
		pObject->RemoveSwitch(pSwitchState);
	}

	g_switchToIndex.erase(pSwitchState);
	delete pSwitchState;
}

///////////////////////////////////////////////////////////////////////////
IEnvironment const* CImpl::ConstructEnvironment(XmlNodeRef const pRootNode)
{
	CEnvironment* pEnvironment = nullptr;
	char const* const szTag = pRootNode->getTag();

	if (_stricmp(szTag, s_szFmodBusTag) == 0)
	{
		stack_string path(s_szFmodBusPrefix);
		path += pRootNode->getAttr(s_szFmodNameAttribute);
		FMOD_GUID guid = { 0 };

		if (m_pSystem->lookupID(path.c_str(), &guid) == FMOD_OK)
		{
			FMOD::Studio::Bus* pBus = nullptr;
			FMOD_RESULT const fmodResult = m_pSystem->getBusByID(&guid, &pBus);
			ASSERT_FMOD_OK;
			pEnvironment = new CEnvironment(nullptr, pBus);
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod bus: %s", path.c_str());
		}
	}
	else if (_stricmp(szTag, s_szFmodSnapshotTag) == 0)
	{
		stack_string path(s_szFmodSnapshotPrefix);
		path += pRootNode->getAttr(s_szFmodNameAttribute);
		FMOD_GUID guid = { 0 };

		if (m_pSystem->lookupID(path.c_str(), &guid) == FMOD_OK)
		{
			FMOD::Studio::EventDescription* pEventDescription = nullptr;
			FMOD_RESULT const fmodResult = m_pSystem->getEventByID(&guid, &pEventDescription);
			ASSERT_FMOD_OK;
			pEnvironment = new CEnvironment(pEventDescription, nullptr);
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Unknown Fmod snapshot: %s", path.c_str());
		}
	}

	return static_cast<IEnvironment*>(pEnvironment);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructEnvironment(IEnvironment const* const pIEnvironment)
{
	CEnvironment const* const pEnvironment = static_cast<CEnvironment const* const>(pIEnvironment);

	for (auto const pObject : m_constructedObjects)
	{
		pObject->RemoveEnvironment(pEnvironment);
	}

	delete pEnvironment;
}

///////////////////////////////////////////////////////////////////////////
char const* const CImpl::GetName() const
{
#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	return m_name.c_str();
#endif  // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
	return nullptr;
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
		m_localizedSoundBankFolder = PathUtil::GetGameFolder().c_str();
		m_localizedSoundBankFolder += CRY_NATIVE_PATH_SEPSTR;
		m_localizedSoundBankFolder += PathUtil::GetLocalizationFolder();
		m_localizedSoundBankFolder += CRY_NATIVE_PATH_SEPSTR;
		m_localizedSoundBankFolder += m_language.c_str();
		m_localizedSoundBankFolder += CRY_NATIVE_PATH_SEPSTR;
		m_localizedSoundBankFolder += FMOD_IMPL_DATA_ROOT;
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::CreateVersionString(CryFixedStringT<MaxMiscStringLength>& stringOut) const
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

struct SFmodFileData
{
	void*        pData;
	int unsigned fileSize;
};

//////////////////////////////////////////////////////////////////////////
FMOD_RESULT F_CALLBACK FmodFileOpenCallback(const char* szName, unsigned int* pFileSize, void** pHandle, void* pUserData)
{
	SFmodFileData* const pFileData = static_cast<SFmodFileData*>(pUserData);
	*pHandle = pFileData->pData;
	*pFileSize = pFileData->fileSize;

	return FMOD_OK;
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
bool CImpl::LoadMasterBanks()
{
	FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;
	CryFixedStringT<MaxFileNameLength> masterBankPath;
	CryFixedStringT<MaxFileNameLength> masterBankStringsPath;
	CryFixedStringT<MaxFilePathLength + MaxFileNameLength> search(m_regularSoundBankFolder + CRY_NATIVE_PATH_SEPSTR "*.bank");
	_finddata_t fd;
	intptr_t const handle = gEnv->pCryPak->FindFirst(search.c_str(), &fd);

	if (handle != -1)
	{
		do
		{
			masterBankStringsPath = fd.name;
			size_t const substrPos = masterBankStringsPath.find(".strings.bank");

			if (substrPos != masterBankStringsPath.npos)
			{
				masterBankPath = m_regularSoundBankFolder.c_str();
				masterBankPath += CRY_NATIVE_PATH_SEPSTR;
				masterBankPath += masterBankStringsPath.substr(0, substrPos);
				masterBankPath += ".bank";
				masterBankStringsPath.insert(0, CRY_NATIVE_PATH_SEPSTR);
				masterBankStringsPath.insert(0, m_regularSoundBankFolder.c_str());
				break;
			}

		}
		while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}

	if (!masterBankPath.empty() && !masterBankStringsPath.empty())
	{
		size_t const masterBankFileSize = gEnv->pCryPak->FGetSize(masterBankPath.c_str());
		CRY_ASSERT(masterBankFileSize > 0);
		size_t const masterBankStringsFileSize = gEnv->pCryPak->FGetSize(masterBankStringsPath.c_str());
		CRY_ASSERT(masterBankStringsFileSize > 0);

		if (masterBankFileSize > 0 && masterBankStringsFileSize > 0)
		{
			FILE* const pMasterBank = gEnv->pCryPak->FOpen(masterBankPath.c_str(), "rbx", ICryPak::FOPEN_HINT_DIRECT_OPERATION);
			FILE* const pStringsBank = gEnv->pCryPak->FOpen(masterBankStringsPath.c_str(), "rbx", ICryPak::FOPEN_HINT_DIRECT_OPERATION);

			SFmodFileData fileData;
			fileData.pData = static_cast<void*>(pMasterBank);
			fileData.fileSize = static_cast<int>(masterBankFileSize);

			FMOD_STUDIO_BANK_INFO bankInfo;
			ZeroStruct(bankInfo);
			bankInfo.closecallback = &FmodFileCloseCallback;
			bankInfo.opencallback = &FmodFileOpenCallback;
			bankInfo.readcallback = &FmodFileReadCallback;
			bankInfo.seekcallback = &FmodFileSeekCallback;
			bankInfo.size = sizeof(bankInfo);
			bankInfo.userdata = static_cast<void*>(&fileData);
			bankInfo.userdatalength = sizeof(SFmodFileData);

			fmodResult = m_pSystem->loadBankCustom(&bankInfo, FMOD_STUDIO_LOAD_BANK_NORMAL, &m_pMasterBank);
			ASSERT_FMOD_OK;
			fileData.pData = static_cast<void*>(pStringsBank);
			fileData.fileSize = static_cast<int>(masterBankStringsFileSize);
			fmodResult = m_pSystem->loadBankCustom(&bankInfo, FMOD_STUDIO_LOAD_BANK_NORMAL, &m_pStringsBank);
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

	if (m_pStringsBank != nullptr)
	{
		fmodResult = m_pStringsBank->unload();
		ASSERT_FMOD_OK;
		m_pStringsBank = nullptr;
	}

	if (m_pMasterBank != nullptr)
	{
		fmodResult = m_pMasterBank->unload();
		ASSERT_FMOD_OK;
		m_pMasterBank = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::MuteMasterBus(bool const bMute)
{
	FMOD::Studio::Bus* pMasterBus = nullptr;
	FMOD_RESULT fmodResult = m_pSystem->getBus(s_szFmodBusPrefix, &pMasterBus);
	ASSERT_FMOD_OK;

	if (pMasterBus != nullptr)
	{
		fmodResult = pMasterBus->setMute(bMute);
		ASSERT_FMOD_OK;
	}

	return (fmodResult == FMOD_OK) ? ERequestStatus::Success : ERequestStatus::Failure;
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

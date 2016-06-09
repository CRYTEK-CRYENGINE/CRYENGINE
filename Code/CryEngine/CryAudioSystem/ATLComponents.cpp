// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ATLComponents.h"
#include "SoundCVars.h"
#include <IAudioImpl.h>
#include <CryPhysics/IPhysics.h>
#include <Cry3DEngine/ISurfaceType.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IRenderAuxGeom.h>

using namespace CryAudio::Impl;

//////////////////////////////////////////////////////////////////////////
CAudioStandaloneFileManager::CAudioStandaloneFileManager()
	: m_audioStandaloneFilePool(g_audioCVars.m_audioStandaloneFilePoolSize, 1)
	, m_pImpl(nullptr)
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	, m_pDebugNameStore(nullptr)
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
{
}

//////////////////////////////////////////////////////////////////////////
CAudioStandaloneFileManager::~CAudioStandaloneFileManager()
{
	if (m_pImpl != nullptr)
	{
		Release();
	}

	if (!m_audioStandaloneFilePool.m_reserved.empty())
	{
		for (auto const pStandaloneFile : m_audioStandaloneFilePool.m_reserved)
		{
			CRY_ASSERT(pStandaloneFile->m_pImplData == nullptr);
			POOL_FREE(pStandaloneFile);
		}

		m_audioStandaloneFilePool.m_reserved.clear();
	}

	if (!m_activeAudioStandaloneFiles.empty())
	{
		for (auto const& standaloneFilePair : m_activeAudioStandaloneFiles)
		{
			CATLStandaloneFile* const pStandaloneFile = standaloneFilePair.second;
			CRY_ASSERT(pStandaloneFile->m_pImplData == nullptr);
			POOL_FREE(pStandaloneFile);
		}

		m_activeAudioStandaloneFiles.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioStandaloneFileManager::Init(IAudioImpl* const pImpl)
{
	m_pImpl = pImpl;

	size_t const numPooledAudioStandaloneFiles = m_audioStandaloneFilePool.m_reserved.size();
	size_t const numActiveAudioStandaloneFiles = m_activeAudioStandaloneFiles.size();

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	if ((numPooledAudioStandaloneFiles + numActiveAudioStandaloneFiles) > std::numeric_limits<size_t>::max())
	{
		CryFatalError("Exceeding numeric limits during CAudioStandaloneFileManager::Init");
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	size_t const numTotalAudioStandaloneFiles = numPooledAudioStandaloneFiles + numActiveAudioStandaloneFiles;
	CRY_ASSERT(!(numTotalAudioStandaloneFiles > 0 && numTotalAudioStandaloneFiles < m_audioStandaloneFilePool.m_reserveSize));

	if (m_audioStandaloneFilePool.m_reserveSize > numTotalAudioStandaloneFiles)
	{
		for (size_t i = 0; i < m_audioStandaloneFilePool.m_reserveSize - numActiveAudioStandaloneFiles; ++i)
		{
			AudioStandaloneFileId const id = m_audioStandaloneFilePool.GetNextID();
			POOL_NEW_CREATE(CATLStandaloneFile, pNewStandaloneFile)(id, eAudioDataScope_Global);
			m_audioStandaloneFilePool.m_reserved.push_back(pNewStandaloneFile);
		}
	}

	for (auto const pStandaloneFile : m_audioStandaloneFilePool.m_reserved)
	{
		CRY_ASSERT(pStandaloneFile->m_pImplData == nullptr);
		pStandaloneFile->m_pImplData = m_pImpl->NewAudioStandaloneFile();
	}

	for (auto const& standaloneFilePair : m_activeAudioStandaloneFiles)
	{
		CATLStandaloneFile* const pStandaloneFile = standaloneFilePair.second;
		CRY_ASSERT(pStandaloneFile->m_pImplData == nullptr);
		pStandaloneFile->m_pImplData = m_pImpl->NewAudioStandaloneFile();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioStandaloneFileManager::Release()
{
	for (auto const pStandaloneFile : m_audioStandaloneFilePool.m_reserved)
	{
		m_pImpl->DeleteAudioStandaloneFile(pStandaloneFile->m_pImplData);
		pStandaloneFile->m_pImplData = nullptr;
	}

	for (auto const& standaloneFilePair : m_activeAudioStandaloneFiles)
	{
		CATLStandaloneFile* const pStandaloneFile = standaloneFilePair.second;
		m_pImpl->ResetAudioStandaloneFile(pStandaloneFile->m_pImplData);
		m_pImpl->DeleteAudioStandaloneFile(pStandaloneFile->m_pImplData);
		pStandaloneFile->m_pImplData = nullptr;
	}

	m_pImpl = nullptr;
}

//////////////////////////////////////////////////////////////////////////
CATLStandaloneFile* CAudioStandaloneFileManager::GetStandaloneFile(char const* const szFile)
{
	CATLStandaloneFile* pStandaloneFile = nullptr;

	if (!m_audioStandaloneFilePool.m_reserved.empty())
	{
		pStandaloneFile = m_audioStandaloneFilePool.m_reserved.back();
		m_audioStandaloneFilePool.m_reserved.pop_back();
	}
	else
	{
		AudioStandaloneFileId const instanceID = m_audioStandaloneFilePool.GetNextID();
		POOL_NEW(CATLStandaloneFile, pStandaloneFile)(instanceID, eAudioDataScope_Global);

		if (pStandaloneFile != nullptr)
		{
			pStandaloneFile->m_pImplData = m_pImpl->NewAudioStandaloneFile();
		}
		else
		{
			--m_audioStandaloneFilePool.m_idCounter;
			g_audioLogger.Log(eAudioLogType_Warning, "Failed to create a new instance of a standalone file during CAudioStandaloneFileManager::GetStandaloneFile!");
		}
	}

	if (pStandaloneFile != nullptr)
	{
		pStandaloneFile->m_id = static_cast<AudioStandaloneFileId>(AudioStringToId(szFile));
		m_activeAudioStandaloneFiles[pStandaloneFile->GetId()] = pStandaloneFile;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		m_pDebugNameStore->AddAudioStandaloneFile(pStandaloneFile->m_id, szFile);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}

	return pStandaloneFile;
}

//////////////////////////////////////////////////////////////////////////
CATLStandaloneFile* CAudioStandaloneFileManager::LookupId(AudioStandaloneFileId const instanceId) const
{
	CATLStandaloneFile* pStandaloneFile = nullptr;

	if (instanceId != INVALID_AUDIO_STANDALONE_FILE_ID)
	{
		ActiveStandaloneFilesMap::const_iterator const Iter(m_activeAudioStandaloneFiles.find(instanceId));

		if (Iter != m_activeAudioStandaloneFiles.end())
		{
			pStandaloneFile = Iter->second;
		}
	}

	return pStandaloneFile;
}

//////////////////////////////////////////////////////////////////////////
void CAudioStandaloneFileManager::ReleaseStandaloneFile(CATLStandaloneFile* const pStandaloneFile)
{
	if (pStandaloneFile != nullptr)
	{
		m_activeAudioStandaloneFiles.erase(pStandaloneFile->GetId());

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		m_pDebugNameStore->RemoveAudioStandaloneFile(pStandaloneFile->m_id);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

		pStandaloneFile->Clear();

		m_pImpl->ResetAudioStandaloneFile(pStandaloneFile->m_pImplData);
		if (m_audioStandaloneFilePool.m_reserved.size() < m_audioStandaloneFilePool.m_reserveSize)
		{
			m_audioStandaloneFilePool.m_reserved.push_back(pStandaloneFile);
		}
		else
		{
			m_pImpl->DeleteAudioStandaloneFile(pStandaloneFile->m_pImplData);
			POOL_FREE(pStandaloneFile);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CAudioEventManager::CAudioEventManager()
	: m_audioEventPool(g_audioCVars.m_audioEventPoolSize, 1)
	, m_pImpl(nullptr)
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	, m_pDebugNameStore(nullptr)
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
{
}

//////////////////////////////////////////////////////////////////////////
CAudioEventManager::~CAudioEventManager()
{
	if (m_pImpl != nullptr)
	{
		Release();
	}

	if (!m_audioEventPool.m_reserved.empty())
	{
		for (auto const pEvent : m_audioEventPool.m_reserved)
		{
			CRY_ASSERT(pEvent->m_pImplData == nullptr);
			POOL_FREE(pEvent);
		}

		m_audioEventPool.m_reserved.clear();
	}

	if (!m_activeAudioEvents.empty())
	{
		for (auto const& eventPair : m_activeAudioEvents)
		{
			CATLEvent* const pEvent = eventPair.second;
			CRY_ASSERT(pEvent->m_pImplData == nullptr);
			POOL_FREE(pEvent);
		}

		m_activeAudioEvents.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioEventManager::Init(IAudioImpl* const pImpl)
{
	m_pImpl = pImpl;

	size_t const numPooledAudioEvents = m_audioEventPool.m_reserved.size();
	size_t const numActiveAudioEvents = m_activeAudioEvents.size();

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	if ((numPooledAudioEvents + numActiveAudioEvents) > std::numeric_limits<size_t>::max())
	{
		CryFatalError("Exceeding numeric limits during CAudioEventManager::Init");
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	size_t const numTotalEvents = numPooledAudioEvents + numActiveAudioEvents;
	CRY_ASSERT(!(numTotalEvents > 0 && numTotalEvents < m_audioEventPool.m_reserveSize));

	if (m_audioEventPool.m_reserveSize > numTotalEvents)
	{
		for (size_t i = 0; i < m_audioEventPool.m_reserveSize - numActiveAudioEvents; ++i)
		{
			AudioEventId const audioEventId = m_audioEventPool.GetNextID();
			POOL_NEW_CREATE(CATLEvent, pNewEvent)(audioEventId, eAudioSubsystem_AudioImpl);
			m_audioEventPool.m_reserved.push_back(pNewEvent);
		}
	}

	for (auto const pEvent : m_audioEventPool.m_reserved)
	{
		CRY_ASSERT(pEvent->m_pImplData == nullptr);
		pEvent->m_pImplData = m_pImpl->NewAudioEvent(pEvent->GetId());
	}

	for (auto const& eventPair : m_activeAudioEvents)
	{
		CATLEvent* const pEvent = eventPair.second;
		CRY_ASSERT(pEvent->m_pImplData == nullptr);
		pEvent->m_pImplData = m_pImpl->NewAudioEvent(pEvent->GetId());
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioEventManager::Release()
{
	if (!m_activeAudioEvents.empty())
	{
		for (auto const& eventPair : m_activeAudioEvents)
		{
			ReleaseEventInternal(eventPair.second);
		}

		m_activeAudioEvents.clear();
	}

	for (auto const pEvent : m_audioEventPool.m_reserved)
	{
		m_pImpl->DeleteAudioEvent(pEvent->m_pImplData);
		pEvent->m_pImplData = nullptr;
	}

	m_pImpl = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CAudioEventManager::Update(float const deltaTime)
{
	//TODO: implement
}

//////////////////////////////////////////////////////////////////////////
CATLEvent* CAudioEventManager::GetEvent(EAudioSubsystem const audioSubsystem)
{
	CATLEvent* pATLEvent = nullptr;

	switch (audioSubsystem)
	{
	case eAudioSubsystem_AudioImpl:
		{
			pATLEvent = GetImplInstance();

			break;
		}
	case eAudioSubsystem_AudioInternal:
		{
			pATLEvent = GetInternalInstance();

			break;
		}
	default:
		{
			CRY_ASSERT(false); // Unknown sender!
		}
	}

	if (pATLEvent != nullptr)
	{
		m_activeAudioEvents[pATLEvent->GetId()] = pATLEvent;
	}

	return pATLEvent;
}

//////////////////////////////////////////////////////////////////////////
CATLEvent* CAudioEventManager::LookupId(AudioEventId const audioEventId) const
{
	TActiveEventMap::const_iterator iter(m_activeAudioEvents.begin());
	bool const bLookupResult = FindPlaceConst(m_activeAudioEvents, audioEventId, iter);

	return bLookupResult ? iter->second : nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CAudioEventManager::ReleaseEvent(CATLEvent* const pEvent)
{
	m_activeAudioEvents.erase(pEvent->GetId());
	ReleaseEventInternal(pEvent);
}

//////////////////////////////////////////////////////////////////////////
CATLEvent* CAudioEventManager::GetImplInstance()
{
	CATLEvent* pEvent = nullptr;

	if (!m_audioEventPool.m_reserved.empty())
	{
		pEvent = m_audioEventPool.m_reserved.back();
		m_audioEventPool.m_reserved.pop_back();
	}
	else
	{
		AudioEventId const nNewID = m_audioEventPool.GetNextID();
		POOL_NEW(CATLEvent, pEvent)(nNewID, eAudioSubsystem_AudioImpl);

		if (pEvent != nullptr)
		{
			pEvent->m_pImplData = m_pImpl->NewAudioEvent(nNewID);
		}
		else
		{
			--m_audioEventPool.m_idCounter;
			g_audioLogger.Log(eAudioLogType_Warning, "Failed to get a new instance of an AudioEvent from the implementation");
		}
	}

	return pEvent;
}

//////////////////////////////////////////////////////////////////////////
void CAudioEventManager::ReleaseEventInternal(CATLEvent* const pEvent)
{
	switch (pEvent->m_audioSubsystem)
	{
	case eAudioSubsystem_AudioImpl:
		ReleaseImplInstance(pEvent);
		break;
	case eAudioSubsystem_AudioInternal:
		ReleaseInternalInstance(pEvent);
		break;
	default:
		// Unknown sender.
		CRY_ASSERT(false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioEventManager::ReleaseImplInstance(CATLEvent* const pOldEvent)
{
	if (pOldEvent != nullptr)
	{
		pOldEvent->Clear();
		m_pImpl->ResetAudioEvent(pOldEvent->m_pImplData);

		if (m_audioEventPool.m_reserved.size() < m_audioEventPool.m_reserveSize)
		{
			// can return the instance to the reserved pool
			m_audioEventPool.m_reserved.push_back(pOldEvent);
		}
		else
		{
			// the reserve pool is full, can return the instance to the implementation to dispose
			m_pImpl->DeleteAudioEvent(pOldEvent->m_pImplData);

			POOL_FREE(pOldEvent);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CATLEvent* CAudioEventManager::GetInternalInstance()
{
	// must be called within a block protected by a critical section!

	CRY_ASSERT(false);// implement when it is needed
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CAudioEventManager::ReleaseInternalInstance(CATLEvent* const pOldEvent)
{
	// must be called within a block protected by a critical section!

	CRY_ASSERT(false);// implement when it is needed
}

//////////////////////////////////////////////////////////////////////////
size_t CAudioEventManager::GetNumActive() const
{
	return m_activeAudioEvents.size();
}

// IDs below that number are reserved for the various unique
// objects inside the AudioSystem a user may want to address
// (e.g. an AudioManager, a MusicSystem, AudioListeners?...)
AudioObjectId const CAudioObjectManager::s_minAudioObjectId = 100;

//////////////////////////////////////////////////////////////////////////
CAudioObjectManager::CAudioObjectManager(CAudioEventManager& _audioEventMgr, CAudioStandaloneFileManager& _audioStandaloneFileMgr)
	: m_audioObjectPool(g_audioCVars.m_audioObjectPoolSize, s_minAudioObjectId)
	, m_pImpl(nullptr)
	, m_timeSinceLastControlsUpdate(0.0f)
	, m_audioEventMgr(_audioEventMgr)
	, m_audioStandaloneFileMgr(_audioStandaloneFileMgr)
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	, m_pDebugNameStore(nullptr)
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
{
}

//////////////////////////////////////////////////////////////////////////
CAudioObjectManager::~CAudioObjectManager()
{
	if (m_pImpl != nullptr)
	{
		Release();
	}

	if (!m_audioObjectPool.m_reserved.empty())
	{
		for (auto const pAudioObject : m_audioObjectPool.m_reserved)
		{
			CRY_ASSERT(pAudioObject->GetImplDataPtr() == nullptr);
			POOL_FREE(pAudioObject);
		}

		m_audioObjectPool.m_reserved.clear();
	}

	if (!m_registeredAudioObjects.empty())
	{
		for (auto const& audioObjectPair : m_registeredAudioObjects)
		{
			CATLAudioObject* const pAudioObject = audioObjectPair.second;
			CRY_ASSERT(pAudioObject->GetImplDataPtr() == nullptr);
			POOL_FREE(pAudioObject);
		}

		m_registeredAudioObjects.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::Init(IAudioImpl* const pImpl)
{
	m_pImpl = pImpl;

	size_t const numPooledAudioObjects = m_audioObjectPool.m_reserved.size();
	size_t const numRegisteredAudioObjects = m_registeredAudioObjects.size();
	size_t const numTotalObjects = numPooledAudioObjects + numRegisteredAudioObjects;
	CRY_ASSERT(!(numTotalObjects > 0 && numTotalObjects < m_audioObjectPool.m_reserveSize));

	if (m_audioObjectPool.m_reserveSize > numTotalObjects)
	{
		for (size_t i = 0; i < m_audioObjectPool.m_reserveSize - numRegisteredAudioObjects; ++i)
		{
			AudioObjectId const id = m_audioObjectPool.GetNextID();
			POOL_NEW_CREATE(CATLAudioObject, pNewObject)(id);
			m_audioObjectPool.m_reserved.push_back(pNewObject);
		}
	}

	for (auto const pAudioObject : m_audioObjectPool.m_reserved)
	{
		CRY_ASSERT(pAudioObject->GetImplDataPtr() == nullptr);
		pAudioObject->SetImplDataPtr(m_pImpl->NewAudioObject(pAudioObject->GetId()));
	}

	EAudioRequestStatus result = eAudioRequestStatus_None;

	for (auto const& audioObjectPair : m_registeredAudioObjects)
	{
		CATLAudioObject* const pAudioObject = audioObjectPair.second;
		CRY_ASSERT(pAudioObject->GetImplDataPtr() == nullptr);
		pAudioObject->SetImplDataPtr(m_pImpl->NewAudioObject(pAudioObject->GetId()));

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		char const* const szAudioObjectName = m_pDebugNameStore->LookupAudioObjectName(pAudioObject->GetId());
		result = m_pImpl->RegisterAudioObject(pAudioObject->GetImplDataPtr(), szAudioObjectName);
		CRY_ASSERT(result == eAudioRequestStatus_Success);
#else
		result = m_pImpl->RegisterAudioObject(pAudioObject->GetImplDataPtr());
		CRY_ASSERT(result == eAudioRequestStatus_Success);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::Release()
{
	for (auto const pAudioObject : m_audioObjectPool.m_reserved)
	{
		m_pImpl->DeleteAudioObject(pAudioObject->GetImplDataPtr());
		pAudioObject->SetImplDataPtr(nullptr);
	}

	EAudioRequestStatus result = eAudioRequestStatus_None;

	for (auto const& audioObjectPair : m_registeredAudioObjects)
	{
		CATLAudioObject* const pAudioObject = audioObjectPair.second;
		IAudioObject* const pImplData = pAudioObject->GetImplDataPtr();
		result = m_pImpl->UnregisterAudioObject(pImplData);
		CRY_ASSERT(result == eAudioRequestStatus_Success);
		result = m_pImpl->ResetAudioObject(pImplData);
		CRY_ASSERT(result == eAudioRequestStatus_Success);
		m_pImpl->DeleteAudioObject(pImplData);
		pAudioObject->SetImplDataPtr(nullptr);
		pAudioObject->Release();
	}

	m_pImpl = nullptr;
}

float CAudioObjectManager::s_controlsUpdateInterval = 100.0f;

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::Update(float const deltaTime, SAudioObject3DAttributes const& listenerAttributes)
{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	CATLAudioObject::CPropagationProcessor::s_totalAsyncPhysRays = 0;
	CATLAudioObject::CPropagationProcessor::s_totalSyncPhysRays = 0;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	m_timeSinceLastControlsUpdate += deltaTime;
	bool const bUpdateControls = m_timeSinceLastControlsUpdate > s_controlsUpdateInterval;

	RegisteredAudioObjectsMap::const_iterator iterEnd = m_registeredAudioObjects.cend();
	for (RegisteredAudioObjectsMap::iterator iter = m_registeredAudioObjects.begin(); iter != iterEnd; )
	{
		CATLAudioObject* const pAudioObject = iter->second;

		SAudioObject3DAttributes const& attributes = pAudioObject->Get3DAttributes();
		float const distance = (attributes.transformation.GetPosition() - listenerAttributes.transformation.GetPosition()).GetLength();
		float const radius = pAudioObject->GetMaxRadius();

		if (radius <= 0.0f || distance < radius)
		{
			if ((pAudioObject->GetFlags() & eAudioObjectFlags_Virtual) > 0)
			{
				pAudioObject->RemoveFlag(eAudioObjectFlags_Virtual);
			}
		}
		else
		{
			if ((pAudioObject->GetFlags() & eAudioObjectFlags_Virtual) == 0)
			{
				pAudioObject->SetFlag(eAudioObjectFlags_Virtual);
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
				pAudioObject->ResetObstructionRays();
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE
			}
		}

		if (IsActive(pAudioObject))
		{
			pAudioObject->Update(deltaTime, distance, listenerAttributes.transformation.GetPosition());

			if (pAudioObject->CanRunObstructionOcclusion())
			{
				SATLSoundPropagationData propagationData;
				pAudioObject->GetPropagationData(propagationData);

				m_pImpl->SetObstructionOcclusion(pAudioObject->GetImplDataPtr(), propagationData.obstruction, propagationData.occlusion);
			}

			if (bUpdateControls)
			{
				pAudioObject->UpdateControls(m_timeSinceLastControlsUpdate, listenerAttributes);
			}

			m_pImpl->UpdateAudioObject(pAudioObject->GetImplDataPtr());
		}
		else
		{
			if (pAudioObject->CanBeReleased())
			{
				m_registeredAudioObjects.erase(iter++);
				iterEnd = m_registeredAudioObjects.cend();
				ReleaseInstance(pAudioObject);
				continue;
			}
		}
		++iter;
	}

	if (bUpdateControls)
	{
		m_timeSinceLastControlsUpdate = 0.0f;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAudioObjectManager::ReserveId(AudioObjectId& audioObjectId)
{
	CATLAudioObject* const pAudioObject = GetInstance();

	bool bSuccess = false;
	audioObjectId = INVALID_AUDIO_OBJECT_ID;

	if (pAudioObject != nullptr)
	{
		EAudioRequestStatus const result = (m_pImpl->RegisterAudioObject(pAudioObject->GetImplDataPtr()));

		if (result == eAudioRequestStatus_Success)
		{
			pAudioObject->SetFlag(eAudioObjectFlags_DoNotRelease);
			audioObjectId = pAudioObject->GetId();
			m_registeredAudioObjects.insert(std::make_pair(audioObjectId, pAudioObject));
			bSuccess = true;
		}
		else
		{
			ReleaseInstance(pAudioObject);
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
class CObjectIDPredicate
{
public:

	explicit CObjectIDPredicate(AudioObjectId const audioObjectId)
		: m_audioObjectId(audioObjectId)
	{}

	~CObjectIDPredicate() {}

	bool operator()(CATLAudioObject const* const pAudioObject)
	{
		return pAudioObject->GetId() == m_audioObjectId;
	}

private:

	DELETE_DEFAULT_CONSTRUCTOR(CObjectIDPredicate);
	AudioObjectId const m_audioObjectId;
};

//////////////////////////////////////////////////////////////////////////
bool CAudioObjectManager::ReserveThisId(AudioObjectId const audioObjectId)
{
	bool bSuccess = false;
	CATLAudioObject* pAudioObject = LookupId(audioObjectId);

	if (pAudioObject == nullptr)
	{
		//no active object uses nAudioObjectID, so we can create one
		TAudioObjectPool::TPointerContainer::iterator ipObject = m_audioObjectPool.m_reserved.begin();
		// not using const for .end() iterator, because find_if seems to expect two iterators of the same type
		TAudioObjectPool::TPointerContainer::iterator const ipObjectEnd = m_audioObjectPool.m_reserved.end();

		ipObject = std::find_if(ipObject, ipObjectEnd, CObjectIDPredicate(audioObjectId));

		if (ipObject != ipObjectEnd)
		{
			// there is a reserved instance with the required ID
			std::swap(*ipObject, m_audioObjectPool.m_reserved.back());
			pAudioObject = m_audioObjectPool.m_reserved.back();
			m_audioObjectPool.m_reserved.pop_back();
		}
		else
		{
			// none of the reserved instances have the required ID
			IAudioObject* const pObjectData = m_pImpl->NewAudioObject(audioObjectId);
			POOL_NEW(CATLAudioObject, pAudioObject)(audioObjectId, pObjectData);
		}

		m_registeredAudioObjects.insert(std::make_pair(audioObjectId, pAudioObject));

		bSuccess = true;
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
bool CAudioObjectManager::ReleaseId(AudioObjectId const audioObjectId)
{
	bool bSuccess = false;
	CATLAudioObject* const pAudioObject = LookupId(audioObjectId);

	if (pAudioObject != nullptr)
	{
		pAudioObject->RemoveFlag(eAudioObjectFlags_DoNotRelease);
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
CATLAudioObject* CAudioObjectManager::LookupId(AudioObjectId const audioObjectId) const
{
	RegisteredAudioObjectsMap::const_iterator iter;
	CATLAudioObject* pResult = nullptr;

	if (FindPlaceConst(m_registeredAudioObjects, audioObjectId, iter))
	{
		pResult = iter->second;
	}

	return pResult;
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::ReportStartedEvent(CATLEvent* const pEvent)
{
	if (pEvent != nullptr)
	{
		CATLAudioObject* const pAudioObject = LookupId(pEvent->m_audioObjectId);

		if (pAudioObject != nullptr)
		{
			pAudioObject->ReportStartedEvent(pEvent);
		}
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		else
		{
			g_audioLogger.Log(
			  eAudioLogType_Warning,
			  "Failed to report starting event %u on object %s as it does not exist!",
			  pEvent->GetId(),
			  m_pDebugNameStore->LookupAudioObjectName(pEvent->m_audioObjectId));
		}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}
	else
	{
		g_audioLogger.Log(eAudioLogType_Warning, "NULL pEvent in CAudioObjectManager::ReportStartedEvent");
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::ReportFinishedEvent(CATLEvent* const pEvent, bool const bSuccess)
{
	if (pEvent != nullptr)
	{
		CATLAudioObject* const pAudioObject = LookupId(pEvent->m_audioObjectId);

		if (pAudioObject != nullptr)
		{
			pAudioObject->ReportFinishedEvent(pEvent, bSuccess);
		}
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		else
		{
			g_audioLogger.Log(
			  eAudioLogType_Warning,
			  "Removing Event %u from Object %s: Object no longer exists!",
			  pEvent->GetId(),
			  m_pDebugNameStore->LookupAudioObjectName(pEvent->m_audioObjectId));
		}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}
	else
	{
		g_audioLogger.Log(eAudioLogType_Warning, "NULL pEvent in CAudioObjectManager::ReportFinishedEvent");
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::GetStartedStandaloneFileRequestData(CATLStandaloneFile* const _pStandaloneFile, CAudioRequestInternal& _request)
{
	if (_pStandaloneFile != nullptr)
	{
		CATLAudioObject* const pAudioObject = LookupId(_pStandaloneFile->m_audioObjectId);

		if (pAudioObject != nullptr)
		{
			pAudioObject->GetStartedStandaloneFileRequestData(_pStandaloneFile, _request);
		}
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		else
		{
			g_audioLogger.Log(
			  eAudioLogType_Warning,
			  "Requesting request data from standalone file %u on Object %s which no longer exists!",
			  _pStandaloneFile->GetId(),
			  m_pDebugNameStore->LookupAudioObjectName(_pStandaloneFile->m_audioObjectId));
		}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}
	else
	{
		g_audioLogger.Log(eAudioLogType_Warning, "NULL _pStandaloneFile in CAudioObjectManager::GetStartedStandaloneFileRequestData");
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::ReportFinishedStandaloneFile(CATLStandaloneFile* const _pStandaloneFile)
{
	if (_pStandaloneFile != nullptr)
	{
		CATLAudioObject* const pAudioObject = LookupId(_pStandaloneFile->m_audioObjectId);

		if (pAudioObject != nullptr)
		{
			pAudioObject->ReportFinishedStandaloneFile(_pStandaloneFile);
		}
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		else
		{
			g_audioLogger.Log(
			  eAudioLogType_Warning,
			  "Removing standalone file %u from Object %s: Object no longer exists!",
			  _pStandaloneFile->GetId(),
			  m_pDebugNameStore->LookupAudioObjectName(_pStandaloneFile->m_audioObjectId));
		}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}
	else
	{
		g_audioLogger.Log(eAudioLogType_Warning, "NULL _pStandaloneFile in CAudioObjectManager::ReportFinishedStandaloneFile");
	}
}

//////////////////////////////////////////////////////////////////////////
CATLAudioObject* CAudioObjectManager::GetInstance()
{
	CATLAudioObject* pAudioObject = nullptr;

	if (!m_audioObjectPool.m_reserved.empty())
	{
		//have reserved instances
		pAudioObject = m_audioObjectPool.m_reserved.back();
		m_audioObjectPool.m_reserved.pop_back();
	}
	else
	{
		//need to get a new instance
		AudioObjectId const nNewID = m_audioObjectPool.GetNextID();
		IAudioObject* const pObjectData = m_pImpl->NewAudioObject(nNewID);
		POOL_NEW(CATLAudioObject, pAudioObject)(nNewID, pObjectData);

		if (pAudioObject == nullptr)
		{
			--m_audioObjectPool.m_idCounter;

			g_audioLogger.Log(eAudioLogType_Warning, "Failed to get a new instance of an AudioObject from the implementation");
			//failed to get a new instance from the implementation
		}
	}

	return pAudioObject;
}

//////////////////////////////////////////////////////////////////////////
bool CAudioObjectManager::ReleaseInstance(CATLAudioObject* const pOldObject)
{
	CRY_ASSERT(pOldObject);
	bool bSuccess = false;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	m_pDebugNameStore->RemoveAudioObject(static_cast<AudioObjectId const>(pOldObject->GetId()));
	pOldObject->CheckBeforeRemoval(m_pDebugNameStore);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	pOldObject->Clear();
	bSuccess = (m_pImpl->UnregisterAudioObject(pOldObject->GetImplDataPtr()) == eAudioRequestStatus_Success);

	m_pImpl->ResetAudioObject(pOldObject->GetImplDataPtr());
	if (m_audioObjectPool.m_reserved.size() < m_audioObjectPool.m_reserveSize)
	{
		// can return the instance to the reserved pool
		m_audioObjectPool.m_reserved.push_back(pOldObject);
	}
	else
	{
		// the reserve pool is full, can return the instance to the implementation to dispose
		m_pImpl->DeleteAudioObject(pOldObject->GetImplDataPtr());
		POOL_FREE(pOldObject);
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::ReleasePendingRays()
{
	for (auto const& audioObjectPair : m_registeredAudioObjects)
	{
		CATLAudioObject* const pAudioObject = audioObjectPair.second;

		if (pAudioObject != nullptr)
		{
			pAudioObject->ReleasePendingRays();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAudioObjectManager::IsActive(CATLAudioObject const* const pAudioObject) const
{
	if ((pAudioObject->GetFlags() & eAudioObjectFlags_Virtual) == 0)
	{
		return HasActiveData(pAudioObject);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CAudioObjectManager::HasActiveData(CATLAudioObject const* const pAudioObject) const
{
	for (auto const pEvent : pAudioObject->GetActiveEvents())
	{
		if (pEvent->IsPlaying())
		{
			return true;
		}
	}

	for (auto const& standaloneFilePair : pAudioObject->GetActiveStandaloneFiles())
	{
		CATLStandaloneFile const* const pStandaloneFile = m_audioStandaloneFileMgr.LookupId(standaloneFilePair.first);

		if (pStandaloneFile->IsPlaying())
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
CAudioListenerManager::CAudioListenerManager()
	: m_numListeners(8)
	, m_pDefaultListenerObject(nullptr)
	, m_defaultListenerId(50) // IDs 50-57 are reserved for the AudioListeners.
	, m_pImpl(nullptr)
{
	m_listenerPool.reserve(static_cast<size_t>(m_numListeners));
}

//////////////////////////////////////////////////////////////////////////
CAudioListenerManager::~CAudioListenerManager()
{
	if (m_pImpl != nullptr)
	{
		Release();
	}

	stl::free_container(m_listenerPool);
}

//////////////////////////////////////////////////////////////////////////
void CAudioListenerManager::Init(IAudioImpl* const pImpl)
{
	m_pImpl = pImpl;
	IAudioListener* pAudioListener = m_pImpl->NewDefaultAudioListener(m_defaultListenerId);
	POOL_NEW(CATLListenerObject, m_pDefaultListenerObject)(m_defaultListenerId, pAudioListener);

	if (m_pDefaultListenerObject != nullptr)
	{
		m_activeListeners[m_defaultListenerId] = m_pDefaultListenerObject;
	}

	for (AudioObjectId i = 1; i < m_numListeners; ++i)
	{
		AudioObjectId const listenerId = m_defaultListenerId + i;
		pAudioListener = m_pImpl->NewAudioListener(listenerId);
		POOL_NEW_CREATE(CATLListenerObject, pListenerObject)(listenerId, pAudioListener);
		m_listenerPool.push_back(pListenerObject);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioListenerManager::Release()
{
	if (m_pDefaultListenerObject != nullptr) // guard against double deletions
	{
		m_activeListeners.erase(m_defaultListenerId);
		m_pImpl->DeleteAudioListener(m_pDefaultListenerObject->m_pImplData);
		POOL_FREE(m_pDefaultListenerObject);
		m_pDefaultListenerObject = nullptr;
	}

	for (auto const pListener : m_listenerPool)
	{
		m_pImpl->DeleteAudioListener(pListener->m_pImplData);
		POOL_FREE(pListener);
	}

	m_listenerPool.clear();
	m_pImpl = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CAudioListenerManager::Update(float const deltaTime)
{
	if (m_pDefaultListenerObject != nullptr)
	{
		m_pDefaultListenerObject->Update(m_pImpl);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAudioListenerManager::ReserveId(AudioObjectId& audioObjectId)
{
	bool bSuccess = false;

	if (!m_listenerPool.empty())
	{
		CATLListenerObject* pListener = m_listenerPool.back();
		m_listenerPool.pop_back();

		AudioObjectId const id = pListener->GetId();
		m_activeListeners.insert(std::make_pair(id, pListener));

		audioObjectId = id;
		bSuccess = true;
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
bool CAudioListenerManager::ReleaseId(AudioObjectId const audioObjectId)
{
	bool bSuccess = false;
	CATLListenerObject* pListener = LookupId(audioObjectId);

	if (pListener != nullptr)
	{
		m_activeListeners.erase(audioObjectId);
		m_listenerPool.push_back(pListener);
		bSuccess = true;
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
CATLListenerObject* CAudioListenerManager::LookupId(AudioObjectId const audioObjectId) const
{
	CATLListenerObject* pListenerObject = nullptr;

	TActiveListenerMap::const_iterator iPlace = m_activeListeners.begin();

	if (FindPlaceConst(m_activeListeners, audioObjectId, iPlace))
	{
		pListenerObject = iPlace->second;
	}

	return pListenerObject;
}

//////////////////////////////////////////////////////////////////////////
size_t CAudioListenerManager::GetNumActive() const
{
	return m_activeListeners.size();
}

//////////////////////////////////////////////////////////////////////////
SAudioObject3DAttributes const& CAudioListenerManager::GetDefaultListenerAttributes() const
{
	if (m_pDefaultListenerObject != nullptr)
	{
		return m_pDefaultListenerObject->Get3DAttributes();
	}

	return g_sNullAudioObjectAttributes;
}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
float CAudioListenerManager::GetDefaultListenerVelocity() const
{
	float velocity = 0.0f;

	if (m_pDefaultListenerObject != nullptr)
	{
		velocity = m_pDefaultListenerObject->GetVelocity();
	}

	return velocity;
}
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

//////////////////////////////////////////////////////////////////////////
CAudioEventListenerManager::CAudioEventListenerManager()
{
}

//////////////////////////////////////////////////////////////////////////
CAudioEventListenerManager::~CAudioEventListenerManager()
{
	stl::free_container(m_listeners);
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioEventListenerManager::AddRequestListener(SAudioManagerRequestDataInternal<eAudioManagerRequestType_AddRequestListener> const* const pRequestData)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	for (auto const& listener : m_listeners)
	{
		if (listener.OnEvent == pRequestData->func && listener.pObjectToListenTo == pRequestData->pObjectToListenTo
		    && listener.requestType == pRequestData->requestType && listener.specificRequestMask == pRequestData->specificRequestMask)
		{
			result = eAudioRequestStatus_Success;
			break;
		}
	}

	if (result == eAudioRequestStatus_Failure)
	{
		SAudioEventListener audioEventListener;
		audioEventListener.pObjectToListenTo = pRequestData->pObjectToListenTo;
		audioEventListener.OnEvent = pRequestData->func;
		audioEventListener.requestType = pRequestData->requestType;
		audioEventListener.specificRequestMask = pRequestData->specificRequestMask;
		m_listeners.push_back(audioEventListener);
		result = eAudioRequestStatus_Success;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioEventListenerManager::RemoveRequestListener(void (* func)(SAudioRequestInfo const* const), void const* const pObjectToListenTo)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;
	TListenerArray::iterator Iter(m_listeners.begin());
	TListenerArray::const_iterator const IterEnd(m_listeners.end());

	for (; Iter != IterEnd; ++Iter)
	{
		if ((Iter->OnEvent == func || func == nullptr) && Iter->pObjectToListenTo == pObjectToListenTo)
		{
			if (Iter != IterEnd - 1)
			{
				(*Iter) = m_listeners.back();
			}

			m_listeners.pop_back();
			result = eAudioRequestStatus_Success;

			break;
		}
	}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	if (result == eAudioRequestStatus_Failure)
	{
		g_audioLogger.Log(eAudioLogType_Warning, "Failed to remove a request listener!");
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CAudioEventListenerManager::NotifyListener(SAudioRequestInfo const* const pResultInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_AUDIO);

	for (auto const& listener : m_listeners)
	{
		if (((listener.specificRequestMask & pResultInfo->specificAudioRequest) > 0)                                                  //check: is the listener interested in this specific event?
		    && (listener.pObjectToListenTo == nullptr || listener.pObjectToListenTo == pResultInfo->pOwner)                           //check: is the listener interested in events from this sender
		    && (listener.requestType == eAudioRequestType_AudioAllRequests || listener.requestType == pResultInfo->audioRequestType)) //check: is the listener interested this eventType
		{
			listener.OnEvent(pResultInfo);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CATLXMLProcessor::CATLXMLProcessor(
  AudioTriggerLookup& triggers,
  AudioRtpcLookup& rtpcs,
  AudioSwitchLookup& switches,
  AudioEnvironmentLookup& environments,
  AudioPreloadRequestLookup& preloadRequests,
  CFileCacheManager& fileCacheMgr)
	: m_triggers(triggers)
	, m_rtpcs(rtpcs)
	, m_switches(switches)
	, m_environments(environments)
	, m_preloadRequests(preloadRequests)
	, m_triggerImplIdCounter(AUDIO_TRIGGER_IMPL_ID_NUM_RESERVED)
	, m_fileCacheMgr(fileCacheMgr)
	, m_pImpl(nullptr)
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	, m_pDebugNameStore(nullptr)
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
{
}

//////////////////////////////////////////////////////////////////////////
CATLXMLProcessor::~CATLXMLProcessor()
{
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::Init(IAudioImpl* const pImpl)
{
	m_pImpl = pImpl;
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::Release()
{
	m_pImpl = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::ParseControlsData(char const* const szFolderPath, EAudioDataScope const dataScope)
{
	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> sRootFolderPath(szFolderPath);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	CTimeValue const startTime(gEnv->pTimer->GetAsyncTime());
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	if (m_pImpl != nullptr)
	{
		sRootFolderPath.TrimRight("/\\");
		CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH + MAX_AUDIO_FILE_NAME_LENGTH> sSearch(sRootFolderPath + "/*.xml");
		_finddata_t fd;
		intptr_t handle = gEnv->pCryPak->FindFirst(sSearch.c_str(), &fd);

		if (handle != -1)
		{
			CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH + MAX_AUDIO_FILE_NAME_LENGTH> sFileName;

			do
			{
				sFileName = sRootFolderPath.c_str();
				sFileName += "/";
				sFileName += fd.name;

				XmlNodeRef const pATLConfigRoot(GetISystem()->LoadXmlFromFile(sFileName));

				if (pATLConfigRoot)
				{
					if (_stricmp(pATLConfigRoot->getTag(), SATLXMLTags::szRootNodeTag) == 0)
					{
						size_t const nATLConfigChildrenCount = static_cast<size_t>(pATLConfigRoot->getChildCount());

						for (size_t i = 0; i < nATLConfigChildrenCount; ++i)
						{
							XmlNodeRef const pAudioConfigNode(pATLConfigRoot->getChild(i));

							if (pAudioConfigNode)
							{
								char const* const sAudioConfigNodeTag = pAudioConfigNode->getTag();

								if (_stricmp(sAudioConfigNodeTag, SATLXMLTags::szTriggersNodeTag) == 0)
								{
									ParseAudioTriggers(pAudioConfigNode, dataScope);
								}
								else if (_stricmp(sAudioConfigNodeTag, SATLXMLTags::szRtpcsNodeTag) == 0)
								{
									ParseAudioRtpcs(pAudioConfigNode, dataScope);
								}
								else if (_stricmp(sAudioConfigNodeTag, SATLXMLTags::szSwitchesNodeTag) == 0)
								{
									ParseAudioSwitches(pAudioConfigNode, dataScope);
								}
								else if (_stricmp(sAudioConfigNodeTag, SATLXMLTags::szEnvironmentsNodeTag) == 0)
								{
									ParseAudioEnvironments(pAudioConfigNode, dataScope);
								}
								else if (_stricmp(sAudioConfigNodeTag, SATLXMLTags::szPreloadsNodeTag) == 0 ||
								         _stricmp(sAudioConfigNodeTag, SATLXMLTags::szEditorDataTag) == 0)
								{
									// This tag is valid but ignored here.
								}
								else
								{
									g_audioLogger.Log(eAudioLogType_Warning, "Unknown AudioConfig node: %s", sAudioConfigNodeTag);
									CRY_ASSERT(false);
								}
							}
						}
					}
				}
			}
			while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

			gEnv->pCryPak->FindClose(handle);
		}
	}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	char const* szDataScope = "unknown";

	switch (dataScope)
	{
	case eAudioDataScope_Global:
		{
			szDataScope = "Global";

			break;
		}
	case eAudioDataScope_LevelSpecific:
		{
			szDataScope = "Level Specific";

			break;
		}
	}

	float const duration = (gEnv->pTimer->GetAsyncTime() - startTime).GetMilliSeconds();
	g_audioLogger.Log(eAudioLogType_Warning, "Parsed controls data in \"%s\" for data scope \"%s\" in %.3f ms!", szFolderPath, szDataScope, duration);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::ParsePreloadsData(char const* const szFolderPath, EAudioDataScope const dataScope)
{
	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> rootFolderPath(szFolderPath);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	CTimeValue const startTime(gEnv->pTimer->GetAsyncTime());
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	if (m_pImpl != nullptr)
	{
		rootFolderPath.TrimRight("/\\");
		CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH + MAX_AUDIO_FILE_NAME_LENGTH> search(rootFolderPath + CRY_NATIVE_PATH_SEPSTR "*.xml");
		_finddata_t fd;
		intptr_t const handle = gEnv->pCryPak->FindFirst(search.c_str(), &fd);

		if (handle != -1)
		{
			CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH + MAX_AUDIO_FILE_NAME_LENGTH> fileName;

			do
			{
				fileName = rootFolderPath.c_str();
				fileName += CRY_NATIVE_PATH_SEPSTR;
				fileName += fd.name;

				XmlNodeRef const pATLConfigRoot(GetISystem()->LoadXmlFromFile(fileName));

				if (pATLConfigRoot)
				{
					if (_stricmp(pATLConfigRoot->getTag(), SATLXMLTags::szRootNodeTag) == 0)
					{

						uint versionNumber = 1;
						pATLConfigRoot->getAttr(SATLXMLTags::szATLVersionAttribute, versionNumber);
						size_t const numChildren = static_cast<size_t>(pATLConfigRoot->getChildCount());

						for (size_t i = 0; i < numChildren; ++i)
						{
							XmlNodeRef const pAudioConfigNode(pATLConfigRoot->getChild(i));

							if (pAudioConfigNode)
							{
								char const* const szAudioConfigNodeTag = pAudioConfigNode->getTag();

								if (_stricmp(szAudioConfigNodeTag, SATLXMLTags::szPreloadsNodeTag) == 0)
								{
									size_t const lastSlashIndex = rootFolderPath.rfind(CRY_NATIVE_PATH_SEPSTR[0]);

									if (rootFolderPath.npos != lastSlashIndex)
									{
										CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> const folderName(rootFolderPath.substr(lastSlashIndex + 1, rootFolderPath.size()));
										ParseAudioPreloads(pAudioConfigNode, dataScope, folderName.c_str(), versionNumber);
									}
									else
									{
										ParseAudioPreloads(pAudioConfigNode, dataScope, nullptr, versionNumber);
									}
								}
								else if (_stricmp(szAudioConfigNodeTag, SATLXMLTags::szTriggersNodeTag) == 0 ||
								         _stricmp(szAudioConfigNodeTag, SATLXMLTags::szRtpcsNodeTag) == 0 ||
								         _stricmp(szAudioConfigNodeTag, SATLXMLTags::szSwitchesNodeTag) == 0 ||
								         _stricmp(szAudioConfigNodeTag, SATLXMLTags::szEnvironmentsNodeTag) == 0 ||
								         _stricmp(szAudioConfigNodeTag, SATLXMLTags::szEditorDataTag) == 0)
								{
									// These tags are valid but ignored here.
								}
								else
								{
									g_audioLogger.Log(eAudioLogType_Warning, "Unknown AudioConfig node: %s", szAudioConfigNodeTag);
								}
							}
						}
					}
				}
			}
			while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

			gEnv->pCryPak->FindClose(handle);
		}
	}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	char const* szDataScope = "unknown";

	switch (dataScope)
	{
	case eAudioDataScope_Global:
		{
			szDataScope = "Global";

			break;
		}
	case eAudioDataScope_LevelSpecific:
		{
			szDataScope = "Level Specific";

			break;
		}
	}

	float const duration = (gEnv->pTimer->GetAsyncTime() - startTime).GetMilliSeconds();
	g_audioLogger.Log(eAudioLogType_Warning, "Parsed preloads data in \"%s\" for data scope \"%s\" in %.3f ms!", szFolderPath, szDataScope, duration);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::ClearControlsData(EAudioDataScope const dataScope)
{
	if (m_pImpl != nullptr)
	{
		AudioTriggerLookup::iterator iTriggerRemover = m_triggers.begin();
		AudioTriggerLookup::const_iterator const iTriggerEnd = m_triggers.end();

		while (iTriggerRemover != iTriggerEnd)
		{
			CATLTrigger const* const pTrigger = iTriggerRemover->second;

			if ((pTrigger->GetDataScope() == dataScope) || dataScope == eAudioDataScope_All)
			{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
				m_pDebugNameStore->RemoveAudioTrigger(pTrigger->GetId());
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE

				DeleteAudioTrigger(pTrigger);
				m_triggers.erase(iTriggerRemover++);
			}
			else
			{
				++iTriggerRemover;
			}
		}

		AudioRtpcLookup::iterator iRtpcRemover = m_rtpcs.begin();
		AudioRtpcLookup::const_iterator const iRtpcEnd = m_rtpcs.end();

		while (iRtpcRemover != iRtpcEnd)
		{
			CATLRtpc const* const pRtpc = iRtpcRemover->second;

			if ((pRtpc->GetDataScope() == dataScope) || dataScope == eAudioDataScope_All)
			{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
				m_pDebugNameStore->RemoveAudioRtpc(pRtpc->GetId());
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE

				DeleteAudioRtpc(pRtpc);
				m_rtpcs.erase(iRtpcRemover++);
			}
			else
			{
				++iRtpcRemover;
			}
		}

		AudioSwitchLookup::iterator iSwitchRemover = m_switches.begin();
		AudioSwitchLookup::const_iterator const iSwitchEnd = m_switches.end();

		while (iSwitchRemover != iSwitchEnd)
		{
			CATLSwitch const* const pSwitch = iSwitchRemover->second;

			if ((pSwitch->GetDataScope() == dataScope) || dataScope == eAudioDataScope_All)
			{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
				m_pDebugNameStore->RemoveAudioSwitch(pSwitch->GetId());
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE

				DeleteAudioSwitch(pSwitch);
				m_switches.erase(iSwitchRemover++);
			}
			else
			{
				++iSwitchRemover;
			}
		}

		AudioEnvironmentLookup::iterator iRemover = m_environments.begin();
		AudioEnvironmentLookup::const_iterator const iEnd = m_environments.end();

		while (iRemover != iEnd)
		{
			CATLAudioEnvironment const* const pEnvironment = iRemover->second;

			if ((pEnvironment->GetDataScope() == dataScope) || dataScope == eAudioDataScope_All)
			{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
				m_pDebugNameStore->RemoveAudioEnvironment(pEnvironment->GetId());
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE

				DeleteAudioEnvironment(pEnvironment);
				m_environments.erase(iRemover++);
			}
			else
			{
				++iRemover;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::ParseAudioPreloads(XmlNodeRef const pPreloadDataRoot, EAudioDataScope const dataScope, char const* const szFolderName, uint const version)
{
	LOADING_TIME_PROFILE_SECTION;

	size_t const preloadRequestCount = static_cast<size_t>(pPreloadDataRoot->getChildCount());

	for (size_t i = 0; i < preloadRequestCount; ++i)
	{
		XmlNodeRef const pPreloadRequestNode(pPreloadDataRoot->getChild(i));

		if (pPreloadRequestNode && _stricmp(pPreloadRequestNode->getTag(), SATLXMLTags::szATLPreloadRequestTag) == 0)
		{
			AudioPreloadRequestId audioPreloadRequestID = SATLInternalControlIDs::globalPreloadRequestId;
			char const* szAudioPreloadRequestName = "global_atl_preloads";
			bool const bAutoLoad = (_stricmp(pPreloadRequestNode->getAttr(SATLXMLTags::szATLTypeAttribute), SATLXMLTags::szATLDataLoadType) == 0);

			if (!bAutoLoad)
			{
				szAudioPreloadRequestName = pPreloadRequestNode->getAttr(SATLXMLTags::szATLNameAttribute);
				audioPreloadRequestID = static_cast<AudioPreloadRequestId>(AudioStringToId(szAudioPreloadRequestName));
			}
			else if (dataScope == eAudioDataScope_LevelSpecific)
			{
				szAudioPreloadRequestName = szFolderName;
				audioPreloadRequestID = static_cast<AudioPreloadRequestId>(AudioStringToId(szAudioPreloadRequestName));
			}

			if (audioPreloadRequestID != INVALID_AUDIO_PRELOAD_REQUEST_ID)
			{
				XmlNodeRef pFileListParentNode = nullptr;
				if (version >= 2)
				{
					size_t const platformCount = pPreloadRequestNode->getChildCount();

					for (size_t j = 0; j < platformCount; ++j)
					{
						XmlNodeRef const pPlatformNode(pPreloadRequestNode->getChild(j));

						if (pPlatformNode && _stricmp(pPlatformNode->getAttr(SATLXMLTags::szATLNameAttribute), SATLXMLTags::szPlatform) == 0)
						{
							pFileListParentNode = pPlatformNode;
							break;
						}
					}
				}
				else
				{
					size_t const preloadRequestChidrenCount = static_cast<size_t>(pPreloadRequestNode->getChildCount());

					if (preloadRequestChidrenCount > 1)
					{
						// We need to have at least two children: ATLPlatforms and at least one ATLConfigGroup
						XmlNodeRef const pPlatformsNode(pPreloadRequestNode->getChild(0));
						char const* szATLConfigGroupName = nullptr;

						if (pPlatformsNode && _stricmp(pPlatformsNode->getTag(), SATLXMLTags::szATLPlatformsTag) == 0)
						{
							size_t const platformCount = pPlatformsNode->getChildCount();

							for (size_t j = 0; j < platformCount; ++j)
							{
								XmlNodeRef const pPlatformNode(pPlatformsNode->getChild(j));

								if (pPlatformNode && _stricmp(pPlatformNode->getAttr(SATLXMLTags::szATLNameAttribute), SATLXMLTags::szPlatform) == 0)
								{
									szATLConfigGroupName = pPlatformNode->getAttr(SATLXMLTags::szATLConfigGroupAttribute);
									break;
								}
							}
						}

						if (szATLConfigGroupName != nullptr)
						{
							for (size_t j = 1; j < preloadRequestChidrenCount; ++j)
							{
								XmlNodeRef const pConfigGroupNode(pPreloadRequestNode->getChild(j));
								if (_stricmp(pConfigGroupNode->getAttr(SATLXMLTags::szATLNameAttribute), szATLConfigGroupName) == 0)
								{
									pFileListParentNode = pConfigGroupNode;
									break;
								}
							}
						}
					}
				}

				if (pFileListParentNode)
				{
					// Found the config group corresponding to the specified platform.
					size_t const fileCount = static_cast<size_t>(pFileListParentNode->getChildCount());

					CATLPreloadRequest::FileEntryIds cFileEntryIDs;
					cFileEntryIDs.reserve(fileCount);

					for (size_t k = 0; k < fileCount; ++k)
					{
						AudioFileEntryId const id = m_fileCacheMgr.TryAddFileCacheEntry(pFileListParentNode->getChild(k), dataScope, bAutoLoad);

						if (id != INVALID_AUDIO_FILE_ENTRY_ID)
						{
							cFileEntryIDs.push_back(id);
						}
						else
						{
							g_audioLogger.Log(eAudioLogType_Warning, "Preload request \"%s\" could not create file entry from tag \"%s\"!", szAudioPreloadRequestName, pFileListParentNode->getChild(k)->getTag());
						}
					}

					CATLPreloadRequest* pPreloadRequest = stl::find_in_map(m_preloadRequests, audioPreloadRequestID, nullptr);

					if (pPreloadRequest == nullptr)
					{
						POOL_NEW(CATLPreloadRequest, pPreloadRequest)(audioPreloadRequestID, dataScope, bAutoLoad, cFileEntryIDs);

						if (pPreloadRequest != nullptr)
						{
							m_preloadRequests[audioPreloadRequestID] = pPreloadRequest;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
							m_pDebugNameStore->AddAudioPreloadRequest(audioPreloadRequestID, szAudioPreloadRequestName);
#endif                // INCLUDE_AUDIO_PRODUCTION_CODE
						}
						else
						{
							CryFatalError("<Audio>: Failed to allocate CATLPreloadRequest");
						}
					}
					else
					{
						// Add to existing preload request.
						pPreloadRequest->m_fileEntryIds.insert(pPreloadRequest->m_fileEntryIds.end(), cFileEntryIDs.begin(), cFileEntryIDs.end());
					}
				}
			}
			else
			{
				g_audioLogger.Log(eAudioLogType_Error, "Preload request \"%s\" already exists! Skipping this entry!", szAudioPreloadRequestName);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::ClearPreloadsData(EAudioDataScope const dataScope)
{
	if (m_pImpl != nullptr)
	{
		AudioPreloadRequestLookup::iterator iRemover = m_preloadRequests.begin();
		AudioPreloadRequestLookup::const_iterator const iEnd = m_preloadRequests.end();

		while (iRemover != iEnd)
		{
			CATLPreloadRequest const* const pRequest = iRemover->second;

			if ((pRequest->GetDataScope() == dataScope) || dataScope == eAudioDataScope_All)
			{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
				m_pDebugNameStore->RemoveAudioPreloadRequest(pRequest->GetId());
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE

				DeleteAudioPreloadRequest(pRequest);
				m_preloadRequests.erase(iRemover++);
			}
			else
			{
				++iRemover;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::ParseAudioEnvironments(XmlNodeRef const pAudioEnvironmentRoot, EAudioDataScope const dataScope)
{
	size_t const nAudioEnvironmentCount = static_cast<size_t>(pAudioEnvironmentRoot->getChildCount());

	for (size_t i = 0; i < nAudioEnvironmentCount; ++i)
	{
		XmlNodeRef const pAudioEnvironmentNode(pAudioEnvironmentRoot->getChild(i));

		if (pAudioEnvironmentNode && _stricmp(pAudioEnvironmentNode->getTag(), SATLXMLTags::szATLEnvironmentTag) == 0)
		{
			char const* const sATLEnvironmentName = pAudioEnvironmentNode->getAttr(SATLXMLTags::szATLNameAttribute);
			AudioEnvironmentId const nATLEnvironmentID = static_cast<AudioEnvironmentId const>(AudioStringToId(sATLEnvironmentName));

			if ((nATLEnvironmentID != INVALID_AUDIO_CONTROL_ID) && (stl::find_in_map(m_environments, nATLEnvironmentID, nullptr) == nullptr))
			{
				//there is no entry yet with this ID in the container
				size_t const nAudioEnvironmentChildrenCount = static_cast<size_t>(pAudioEnvironmentNode->getChildCount());

				CATLAudioEnvironment::ImplPtrVec cImplPtrs;
				cImplPtrs.reserve(nAudioEnvironmentChildrenCount);

				for (size_t j = 0; j < nAudioEnvironmentChildrenCount; ++j)
				{
					XmlNodeRef const pEnvironmentImplNode(pAudioEnvironmentNode->getChild(j));

					if (pEnvironmentImplNode)
					{
						IAudioEnvironment const* pEnvirnomentImplData = nullptr;
						EAudioSubsystem eReceiver = eAudioSubsystem_None;

						if (_stricmp(pEnvironmentImplNode->getTag(), SATLXMLTags::szATLEnvironmentRequestTag) == 0)
						{
							pEnvirnomentImplData = NewInternalAudioEnvironment(pEnvironmentImplNode);
							eReceiver = eAudioSubsystem_AudioInternal;
						}
						else
						{
							pEnvirnomentImplData = m_pImpl->NewAudioEnvironment(pEnvironmentImplNode);
							eReceiver = eAudioSubsystem_AudioImpl;
						}

						if (pEnvirnomentImplData != nullptr)
						{
							POOL_NEW_CREATE(CATLEnvironmentImpl, pEnvirnomentImpl)(eReceiver, pEnvirnomentImplData);
							cImplPtrs.push_back(pEnvirnomentImpl);
						}
						else
						{
							g_audioLogger.Log(eAudioLogType_Warning, "Could not parse an Environment Implementation with XML tag %s", pEnvironmentImplNode->getTag());
						}
					}
				}

				cImplPtrs.shrink_to_fit();

				if (!cImplPtrs.empty())
				{
					POOL_NEW_CREATE(CATLAudioEnvironment, pNewEnvironment)(nATLEnvironmentID, dataScope, cImplPtrs);

					if (pNewEnvironment != nullptr)
					{
						m_environments[nATLEnvironmentID] = pNewEnvironment;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
						m_pDebugNameStore->AddAudioEnvironment(nATLEnvironmentID, sATLEnvironmentName);
#endif      // INCLUDE_AUDIO_PRODUCTION_CODE
					}
				}
			}
			else
			{
				g_audioLogger.Log(eAudioLogType_Error, "AudioEnvironment \"%s\" already exists!", sATLEnvironmentName);
				CRY_ASSERT(false);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::ParseAudioTriggers(XmlNodeRef const pXMLTriggerRoot, EAudioDataScope const dataScope)
{
	size_t const nAudioTriggersChildrenCount = static_cast<size_t>(pXMLTriggerRoot->getChildCount());

	for (size_t i = 0; i < nAudioTriggersChildrenCount; ++i)
	{
		XmlNodeRef const pAudioTriggerNode(pXMLTriggerRoot->getChild(i));

		if (pAudioTriggerNode && _stricmp(pAudioTriggerNode->getTag(), SATLXMLTags::szATLTriggerTag) == 0)
		{
			char const* const sATLTriggerName = pAudioTriggerNode->getAttr(SATLXMLTags::szATLNameAttribute);
			AudioControlId const nATLTriggerID = static_cast<AudioControlId const>(AudioStringToId(sATLTriggerName));

			if ((nATLTriggerID != INVALID_AUDIO_CONTROL_ID) && (stl::find_in_map(m_triggers, nATLTriggerID, nullptr) == nullptr))
			{
				//there is no entry yet with this ID in the container
				size_t const nAudioTriggerChildrenCount = static_cast<size_t>(pAudioTriggerNode->getChildCount());

				CATLTrigger::ImplPtrVec cImplPtrs;
				cImplPtrs.reserve(nAudioTriggerChildrenCount);

				float maxRadius = 0.0f;
				for (size_t m = 0; m < nAudioTriggerChildrenCount; ++m)
				{
					XmlNodeRef const pTriggerImplNode(pAudioTriggerNode->getChild(m));

					if (pTriggerImplNode)
					{
						IAudioTrigger const* pAudioTrigger = nullptr;
						EAudioSubsystem eReceiver = eAudioSubsystem_None;

						if (_stricmp(pTriggerImplNode->getTag(), SATLXMLTags::szATLTriggerRequestTag) == 0)
						{
							pAudioTrigger = NewInternalAudioTrigger(pTriggerImplNode);

							eReceiver = eAudioSubsystem_AudioInternal;
						}
						else
						{
							SAudioTriggerInfo triggerInfo;
							pAudioTrigger = m_pImpl->NewAudioTrigger(pTriggerImplNode, triggerInfo);
							maxRadius = std::max(maxRadius, triggerInfo.maxRadius);
							eReceiver = eAudioSubsystem_AudioImpl;
						}

						if (pAudioTrigger != nullptr)
						{
							POOL_NEW_CREATE(CATLTriggerImpl, pTriggerImpl)(++m_triggerImplIdCounter, nATLTriggerID, eReceiver, pAudioTrigger);
							cImplPtrs.push_back(pTriggerImpl);
						}
						else
						{
							g_audioLogger.Log(eAudioLogType_Warning, "Could not parse a Trigger Implementation with XML tag %s", pTriggerImplNode->getTag());
						}
					}
				}

				cImplPtrs.shrink_to_fit();

				POOL_NEW_CREATE(CATLTrigger, pNewTrigger)(nATLTriggerID, dataScope, cImplPtrs, maxRadius);

				if (pNewTrigger != nullptr)
				{
					m_triggers[nATLTriggerID] = pNewTrigger;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
					m_pDebugNameStore->AddAudioTrigger(nATLTriggerID, sATLTriggerName);
#endif    // INCLUDE_AUDIO_PRODUCTION_CODE
				}
			}
			else
			{
				g_audioLogger.Log(eAudioLogType_Error, "trigger \"%s\" already exists!", sATLTriggerName);
				CRY_ASSERT(false);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::ParseAudioSwitches(XmlNodeRef const pXMLSwitchRoot, EAudioDataScope const dataScope)
{
	size_t const numAudioSwitchChildren = static_cast<size_t>(pXMLSwitchRoot->getChildCount());

	for (size_t i = 0; i < numAudioSwitchChildren; ++i)
	{
		XmlNodeRef const pATLSwitchNode(pXMLSwitchRoot->getChild(i));

		if (pATLSwitchNode && _stricmp(pATLSwitchNode->getTag(), SATLXMLTags::szATLSwitchTag) == 0)
		{
			char const* const szAudioSwitchName = pATLSwitchNode->getAttr(SATLXMLTags::szATLNameAttribute);
			AudioControlId const audioSwitchId = static_cast<AudioControlId const>(AudioStringToId(szAudioSwitchName));

			if ((audioSwitchId != INVALID_AUDIO_CONTROL_ID) && (stl::find_in_map(m_switches, audioSwitchId, nullptr) == nullptr))
			{
				POOL_NEW_CREATE(CATLSwitch, pNewSwitch)(audioSwitchId, dataScope);
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
				m_pDebugNameStore->AddAudioSwitch(audioSwitchId, szAudioSwitchName);
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE

				size_t const numAudioSwitchStates = static_cast<size_t>(pATLSwitchNode->getChildCount());

				for (size_t j = 0; j < numAudioSwitchStates; ++j)
				{
					XmlNodeRef const pATLSwitchStateNode(pATLSwitchNode->getChild(j));

					if (pATLSwitchStateNode && _stricmp(pATLSwitchStateNode->getTag(), SATLXMLTags::szATLSwitchStateTag) == 0)
					{
						char const* const szAudioSwitchStateName = pATLSwitchStateNode->getAttr(SATLXMLTags::szATLNameAttribute);
						AudioSwitchStateId const audioSwitchStateId = static_cast<AudioSwitchStateId const>(AudioStringToId(szAudioSwitchStateName));

						if (audioSwitchStateId != INVALID_AUDIO_SWITCH_STATE_ID)
						{
							size_t const numAudioSwitchStateImpl = static_cast<size_t>(pATLSwitchStateNode->getChildCount());

							CATLSwitchState::ImplPtrVec switchStateImplVec;
							switchStateImplVec.reserve(numAudioSwitchStateImpl);

							for (size_t k = 0; k < numAudioSwitchStateImpl; ++k)
							{
								XmlNodeRef const pStateImplNode(pATLSwitchStateNode->getChild(k));

								if (pStateImplNode)
								{
									char const* const szStateImplNodeTag = pStateImplNode->getTag();
									IAudioSwitchState const* pNewSwitchStateImplData = nullptr;
									EAudioSubsystem audioSubsystem = eAudioSubsystem_None;

									if (_stricmp(szStateImplNodeTag, SATLXMLTags::szATLSwitchRequestTag) == 0)
									{
										pNewSwitchStateImplData = NewInternalAudioSwitchState(pStateImplNode);
										audioSubsystem = eAudioSubsystem_AudioInternal;
									}
									else
									{
										pNewSwitchStateImplData = m_pImpl->NewAudioSwitchState(pStateImplNode);
										audioSubsystem = eAudioSubsystem_AudioImpl;
									}

									if (pNewSwitchStateImplData != nullptr)
									{
										POOL_NEW_CREATE(CATLSwitchStateImpl, pSwitchStateImpl)(audioSubsystem, pNewSwitchStateImplData);
										switchStateImplVec.push_back(pSwitchStateImpl);
									}
								}
							}

							POOL_NEW_CREATE(CATLSwitchState, pNewState)(audioSwitchId, audioSwitchStateId, switchStateImplVec);
							pNewSwitch->audioSwitchStates[audioSwitchStateId] = pNewState;
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
							m_pDebugNameStore->AddAudioSwitchState(audioSwitchId, audioSwitchStateId, szAudioSwitchStateName);
#endif        // INCLUDE_AUDIO_PRODUCTION_CODE
						}
					}
				}

				m_switches[audioSwitchId] = pNewSwitch;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::ParseAudioRtpcs(XmlNodeRef const pXMLRtpcRoot, EAudioDataScope const dataScope)
{
	size_t const nAudioRtpcChildrenCount = static_cast<size_t>(pXMLRtpcRoot->getChildCount());

	for (size_t i = 0; i < nAudioRtpcChildrenCount; ++i)
	{
		XmlNodeRef const pAudioRtpcNode(pXMLRtpcRoot->getChild(i));

		if (pAudioRtpcNode && _stricmp(pAudioRtpcNode->getTag(), SATLXMLTags::szATLRtpcTag) == 0)
		{
			char const* const sATLRtpcName = pAudioRtpcNode->getAttr(SATLXMLTags::szATLNameAttribute);
			AudioControlId const nATLRtpcID = static_cast<AudioControlId const>(AudioStringToId(sATLRtpcName));

			if ((nATLRtpcID != INVALID_AUDIO_CONTROL_ID) && (stl::find_in_map(m_rtpcs, nATLRtpcID, nullptr) == nullptr))
			{
				size_t const nRtpcNodeChildrenCount = static_cast<size_t>(pAudioRtpcNode->getChildCount());
				CATLRtpc::ImplPtrVec cImplPtrs;
				cImplPtrs.reserve(nRtpcNodeChildrenCount);

				for (size_t j = 0; j < nRtpcNodeChildrenCount; ++j)
				{
					XmlNodeRef const pRtpcImplNode(pAudioRtpcNode->getChild(j));

					if (pRtpcImplNode)
					{
						char const* const sRtpcImplNodeTag = pRtpcImplNode->getTag();
						IAudioRtpc const* pNewRtpcImplData = nullptr;
						EAudioSubsystem eReceiver = eAudioSubsystem_None;

						if (_stricmp(sRtpcImplNodeTag, SATLXMLTags::szATLRtpcRequestTag) == 0)
						{
							pNewRtpcImplData = NewInternalAudioRtpc(pRtpcImplNode);
							eReceiver = eAudioSubsystem_AudioInternal;
						}
						else
						{
							pNewRtpcImplData = m_pImpl->NewAudioRtpc(pRtpcImplNode);
							eReceiver = eAudioSubsystem_AudioImpl;
						}

						if (pNewRtpcImplData != nullptr)
						{
							POOL_NEW_CREATE(CATLRtpcImpl, pRtpcImpl)(eReceiver, pNewRtpcImplData);
							cImplPtrs.push_back(pRtpcImpl);
						}
					}
				}
				cImplPtrs.shrink_to_fit();

				POOL_NEW_CREATE(CATLRtpc, pNewRtpc)(nATLRtpcID, dataScope, cImplPtrs);

				if (pNewRtpc != nullptr)
				{
					m_rtpcs[nATLRtpcID] = pNewRtpc;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
					m_pDebugNameStore->AddAudioRtpc(nATLRtpcID, sATLRtpcName);
#endif    // INCLUDE_AUDIO_PRODUCTION_CODE
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IAudioTrigger const* CATLXMLProcessor::NewInternalAudioTrigger(XmlNodeRef const pXMLTriggerRoot)
{
	//TODO: implement
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
IAudioRtpc const* CATLXMLProcessor::NewInternalAudioRtpc(XmlNodeRef const pXMLRtpcRoot)
{
	//TODO: implement
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
IAudioSwitchState const* CATLXMLProcessor::NewInternalAudioSwitchState(XmlNodeRef const pXMLSwitchRoot)
{
	SATLSwitchStateImplData_internal* pSwitchStateImpl = nullptr;

	char const* const sInternalSwitchNodeName = pXMLSwitchRoot->getAttr(SATLXMLTags::szATLNameAttribute);

	if ((sInternalSwitchNodeName != nullptr) && (sInternalSwitchNodeName[0] != 0) && (pXMLSwitchRoot->getChildCount() == 1))
	{
		XmlNodeRef const pValueNode(pXMLSwitchRoot->getChild(0));

		if (pValueNode && _stricmp(pValueNode->getTag(), SATLXMLTags::szATLValueTag) == 0)
		{
			char const* sInternalSwitchStateName = pValueNode->getAttr(SATLXMLTags::szATLNameAttribute);

			if ((sInternalSwitchStateName != nullptr) && (sInternalSwitchStateName[0] != 0))
			{
				AudioControlId const nATLInternalSwitchID = static_cast<AudioControlId>(AudioStringToId(sInternalSwitchNodeName));
				AudioSwitchStateId const nATLInternalStateID = static_cast<AudioSwitchStateId>(AudioStringToId(sInternalSwitchStateName));
				POOL_NEW(SATLSwitchStateImplData_internal, pSwitchStateImpl)(nATLInternalSwitchID, nATLInternalStateID);
			}
		}
	}
	else
	{
		g_audioLogger.Log(
		  eAudioLogType_Warning,
		  "An ATLSwitchRequest %s inside ATLSwitchState needs to have exactly one ATLValue.",
		  sInternalSwitchNodeName);
	}

	return pSwitchStateImpl;
}

//////////////////////////////////////////////////////////////////////////
IAudioEnvironment const* CATLXMLProcessor::NewInternalAudioEnvironment(XmlNodeRef const pXMLEnvironmentRoot)
{
	// TODO: implement
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::DeleteAudioTrigger(CATLTrigger const* const pOldTrigger)
{
	if (pOldTrigger != nullptr)
	{
		for (auto const pTriggerImpl : pOldTrigger->m_implPtrs)
		{
			if (pTriggerImpl->GetReceiver() == eAudioSubsystem_AudioInternal)
			{
				POOL_FREE_CONST(pTriggerImpl->m_pImplData);
			}
			else
			{
				m_pImpl->DeleteAudioTrigger(pTriggerImpl->m_pImplData);
			}

			POOL_FREE_CONST(pTriggerImpl);
		}

		POOL_FREE_CONST(pOldTrigger);
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::DeleteAudioRtpc(CATLRtpc const* const pOldRtpc)
{
	if (pOldRtpc != nullptr)
	{
		for (auto const pRtpcImpl : pOldRtpc->m_implPtrs)
		{
			if (pRtpcImpl->GetReceiver() == eAudioSubsystem_AudioInternal)
			{
				POOL_FREE_CONST(pRtpcImpl->m_pImplData);
			}
			else
			{
				m_pImpl->DeleteAudioRtpc(pRtpcImpl->m_pImplData);
			}

			POOL_FREE_CONST(pRtpcImpl);
		}

		POOL_FREE_CONST(pOldRtpc);
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::DeleteAudioSwitch(CATLSwitch const* const pOldSwitch)
{
	if (pOldSwitch != nullptr)
	{
		for (auto const& statePair : pOldSwitch->audioSwitchStates)
		{
			CATLSwitchState const* const pSwitchState = statePair.second;

			if (pSwitchState != nullptr)
			{
				for (auto const pStateImpl : pSwitchState->m_implPtrs)
				{
					if (pStateImpl->GetReceiver() == eAudioSubsystem_AudioInternal)
					{
						POOL_FREE_CONST(pStateImpl->m_pImplData);
					}
					else
					{
						m_pImpl->DeleteAudioSwitchState(pStateImpl->m_pImplData);
					}

					POOL_FREE_CONST(pStateImpl);
				}

				POOL_FREE_CONST(pSwitchState);
			}
		}

		POOL_FREE_CONST(pOldSwitch);
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::DeleteAudioPreloadRequest(CATLPreloadRequest const* const pOldPreloadRequest)
{
	if (pOldPreloadRequest != nullptr)
	{
		EAudioDataScope const dataScope = pOldPreloadRequest->GetDataScope();

		for (auto const fileId : pOldPreloadRequest->m_fileEntryIds)
		{
			m_fileCacheMgr.TryRemoveFileCacheEntry(fileId, dataScope);
		}

		POOL_FREE_CONST(pOldPreloadRequest);
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::DeleteAudioEnvironment(CATLAudioEnvironment const* const pOldEnvironment)
{
	if (pOldEnvironment != nullptr)
	{
		for (auto const pEnvImpl : pOldEnvironment->m_implPtrs)
		{
			if (pEnvImpl->GetReceiver() == eAudioSubsystem_AudioInternal)
			{
				POOL_FREE_CONST(pEnvImpl->m_pImplData);
			}
			else
			{
				m_pImpl->DeleteAudioEnvironment(pEnvImpl->m_pImplData);
			}

			POOL_FREE_CONST(pEnvImpl);
		}

		POOL_FREE_CONST(pOldEnvironment);
	}
}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
void CAudioStandaloneFileManager::DrawDebugInfo(IRenderAuxGeom& auxGeom, float posX, float posY) const
{
	static float const headerColor[4] = { 1.0f, 1.0f, 1.0f, 0.9f };
	static float const itemPlayingColor[4] = { 0.1f, 0.6f, 0.1f, 0.9f };
	static float const itemStoppingColor[4] = { 0.8f, 0.7f, 0.1f, 0.9f };
	static float const itemLoadingColor[4] = { 0.9f, 0.2f, 0.2f, 0.9f };
	static float const itemOtherColor[4] = { 0.8f, 0.8f, 0.8f, 0.9f };

	auxGeom.Draw2dLabel(posX, posY, 1.6f, headerColor, false, "Standalone Files [%" PRISIZE_T "]", m_activeAudioStandaloneFiles.size());
	posX += 20.0f;
	posY += 17.0f;

	for (auto const& standaloneFilePair : m_activeAudioStandaloneFiles)
	{
		CATLStandaloneFile* const pStandaloneFile = standaloneFilePair.second;
		char const* const szName = m_pDebugNameStore->LookupAudioStandaloneFileName(pStandaloneFile->m_id);

		float const* pColor = itemOtherColor;

		switch (pStandaloneFile->m_state)
		{
		case eAudioStandaloneFileState_Playing:
			{
				pColor = itemPlayingColor;

				break;
			}
		case eAudioStandaloneFileState_Loading:
			{
				pColor = itemLoadingColor;

				break;
			}
		case eAudioStandaloneFileState_Stopping:
			{
				pColor = itemStoppingColor;

				break;
			}
		}

		auxGeom.Draw2dLabel(posX, posY, 1.2f,
		                    pColor,
		                    false,
		                    "%s on %s : %u",
		                    szName,
		                    m_pDebugNameStore->LookupAudioObjectName(pStandaloneFile->m_audioObjectId),
		                    pStandaloneFile->GetId());

		posY += 10.0f;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioEventManager::DrawDebugInfo(IRenderAuxGeom& auxGeom, float posX, float posY) const
{
	static float const headerColor[4] = { 1.0f, 1.0f, 1.0f, 0.9f };
	static float const itemPlayingColor[4] = { 0.1f, 0.6f, 0.1f, 0.9f };
	static float const itemLoadingColor[4] = { 0.9f, 0.2f, 0.2f, 0.9f };
	static float const itemVirtualColor[4] = { 0.1f, 0.8f, 0.8f, 0.9f };
	static float const itemOtherColor[4] = { 0.8f, 0.8f, 0.8f, 0.9f };

	auxGeom.Draw2dLabel(posX, posY, 1.6f, headerColor, false, "Audio Events [%" PRISIZE_T "]", m_activeAudioEvents.size());
	posX += 20.0f;
	posY += 17.0f;

	for (auto const& eventPair : m_activeAudioEvents)
	{
		CATLEvent* const pEvent = eventPair.second;
		if (pEvent->m_pTrigger != nullptr)
		{
			char const* const szOriginalName = m_pDebugNameStore->LookupAudioTriggerName(pEvent->m_pTrigger->GetId());
			CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH> sLowerCaseAudioTriggerName(szOriginalName);
			sLowerCaseAudioTriggerName.MakeLower();
			CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH> sLowerCaseSearchString(g_audioCVars.m_pAudioTriggersDebugFilter->GetString());
			sLowerCaseSearchString.MakeLower();
			bool const bDraw = (sLowerCaseSearchString.empty() || (sLowerCaseSearchString.compareNoCase("0") == 0)) || (sLowerCaseAudioTriggerName.find(sLowerCaseSearchString) != CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>::npos);

			if (bDraw)
			{
				float const* pColor = itemOtherColor;

				if (pEvent->IsPlaying())
				{
					pColor = itemPlayingColor;
				}
				else if (pEvent->m_audioEventState == eAudioEventState_Loading)
				{
					pColor = itemLoadingColor;
				}
				else if (pEvent->m_audioEventState == eAudioEventState_Virtual)
				{
					pColor = itemVirtualColor;
				}

				auxGeom.Draw2dLabel(posX, posY, 1.2f,
				                    pColor,
				                    false,
				                    "%s on %s : %u",
				                    szOriginalName,
				                    m_pDebugNameStore->LookupAudioObjectName(pEvent->m_audioObjectId),
				                    pEvent->GetId());

				posY += 10.0f;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAudioObjectManager::ReserveId(AudioObjectId& audioObjectId, char const* const szAudioObjectName)
{
	CATLAudioObject* const pNewObject = GetInstance();

	bool bSuccess = false;
	audioObjectId = INVALID_AUDIO_OBJECT_ID;

	if (pNewObject != nullptr)
	{
		EAudioRequestStatus const result = (m_pImpl->RegisterAudioObject(pNewObject->GetImplDataPtr(), szAudioObjectName));

		if (result == eAudioRequestStatus_Success)
		{
			pNewObject->SetFlag(eAudioObjectFlags_DoNotRelease);
			audioObjectId = pNewObject->GetId();
			m_registeredAudioObjects.insert(std::make_pair(audioObjectId, pNewObject));
			m_pDebugNameStore->AddAudioObject(audioObjectId, szAudioObjectName);
			bSuccess = true;
		}
		else
		{
			m_registeredAudioObjects.erase(static_cast<AudioObjectId const>(pNewObject->GetId()));
			ReleaseInstance(pNewObject);
			bSuccess = false;
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
size_t CAudioObjectManager::GetNumAudioObjects() const
{
	return m_registeredAudioObjects.size();
}

//////////////////////////////////////////////////////////////////////////
size_t CAudioObjectManager::GetNumActiveAudioObjects() const
{
	size_t numActiveAudioObjects = 0;
	for (auto const& audioObjectPair : m_registeredAudioObjects)
	{
		CATLAudioObject* const pAudioObject = audioObjectPair.second;

		if (IsActive(pAudioObject))
		{
			++numActiveAudioObjects;
		}
	}

	return numActiveAudioObjects;
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::DrawPerObjectDebugInfo(IRenderAuxGeom& auxGeom, Vec3 const& listenerPos) const
{
	for (auto const& audioObjectPair : m_registeredAudioObjects)
	{
		CATLAudioObject* const pAudioObject = audioObjectPair.second;

		if (IsActive(pAudioObject))
		{
			pAudioObject->DrawDebugInfo(auxGeom, listenerPos, m_pDebugNameStore);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::DrawDebugInfo(IRenderAuxGeom& auxGeom, float posX, float posY) const
{
	static float const fHeaderColor[4] = { 1.0f, 1.0f, 1.0f, 0.9f };
	static float const fItemActiveColor[4] = { 0.1f, 0.6f, 0.1f, 0.9f };
	static float const fItemInactiveColor[4] = { 0.8f, 0.8f, 0.8f, 0.9f };
	static float const fItemVirtualColor[4] = { 0.1f, 0.8f, 0.8f, 0.9f };

	size_t nObjects = 0;
	float const fHeaderPosY = posY;
	posX += 20.0f;
	posY += 17.0f;

	for (auto const& audioObjectPair : m_registeredAudioObjects)
	{
		CATLAudioObject* const pAudioObject = audioObjectPair.second;
		char const* const sOriginalName = m_pDebugNameStore->LookupAudioObjectName(pAudioObject->GetId());
		CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH> sLowerCaseAudioObjectName(sOriginalName);
		sLowerCaseAudioObjectName.MakeLower();
		CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH> sLowerCaseSearchString(g_audioCVars.m_pAudioObjectsDebugFilter->GetString());
		sLowerCaseSearchString.MakeLower();
		bool const bHasActiveData = HasActiveData(pAudioObject);
		bool const bIsVirtual = (pAudioObject->GetFlags() & eAudioObjectFlags_Virtual) > 0;
		bool const bStringFound = (sLowerCaseSearchString.empty() || (sLowerCaseSearchString.compareNoCase("0") == 0)) || (sLowerCaseAudioObjectName.find(sLowerCaseSearchString) != CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>::npos);
		bool const bDraw = bStringFound && ((g_audioCVars.m_showActiveAudioObjectsOnly == 0) || (g_audioCVars.m_showActiveAudioObjectsOnly > 0 && bHasActiveData && !bIsVirtual));

		if (bDraw)
		{
			auxGeom.Draw2dLabel(posX, posY, 1.2f,
			                    bIsVirtual ? fItemVirtualColor : (bHasActiveData ? fItemActiveColor : fItemInactiveColor),
			                    false,
			                    "%u : %s : %.2f",
			                    pAudioObject->GetId(), sOriginalName, pAudioObject->GetMaxRadius());

			posY += 10.0f;
			++nObjects;
		}
	}

	auxGeom.Draw2dLabel(posX, fHeaderPosY, 1.6f, fHeaderColor, false, "Audio Objects [%" PRISIZE_T "]", nObjects);
}

//////////////////////////////////////////////////////////////////////////
void CAudioStandaloneFileManager::SetDebugNameStore(CATLDebugNameStore* const pDebugNameStore)
{
	m_pDebugNameStore = pDebugNameStore;
}

//////////////////////////////////////////////////////////////////////////
void CAudioEventManager::SetDebugNameStore(CATLDebugNameStore const* const pDebugNameStore)
{
	m_pDebugNameStore = pDebugNameStore;
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::SetDebugNameStore(CATLDebugNameStore* const pDebugNameStore)
{
	m_pDebugNameStore = pDebugNameStore;
}

//////////////////////////////////////////////////////////////////////////
void CATLXMLProcessor::SetDebugNameStore(CATLDebugNameStore* const pDebugNameStore)
{
	m_pDebugNameStore = pDebugNameStore;
}

#endif // INCLUDE_AUDIO_PRODUCTION_CODE

// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioImpl.h"
#include "AudioEvent.h"
#include "AudioObject.h"
#include "AudioImplCVars.h"
#include "ATLEntities.h"
#include <CrySystem/File/ICryPak.h>
#include <CryAudio/IAudioSystem.h>

using namespace CryAudio::Impl;
using namespace CryAudio::Impl::Fmod;

AudioParameterToIndexMap g_parameterToIndex;
FmodSwitchToIndexMap g_switchToIndex;

char const* const CAudioImpl::s_szFmodEventTag = "FmodEvent";
char const* const CAudioImpl::s_szFmodSnapshotTag = "FmodSnapshot";
char const* const CAudioImpl::s_szFmodEventParameterTag = "FmodEventParameter";
char const* const CAudioImpl::s_szFmodSnapshotParameterTag = "FmodSnapshotParameter";
char const* const CAudioImpl::s_szFmodFileTag = "FmodFile";
char const* const CAudioImpl::s_szFmodBusTag = "FmodBus";
char const* const CAudioImpl::s_szFmodNameAttribute = "fmod_name";
char const* const CAudioImpl::s_szFmodValueAttribute = "fmod_value";
char const* const CAudioImpl::s_szFmodMutiplierAttribute = "fmod_value_multiplier";
char const* const CAudioImpl::s_szFmodShiftAttribute = "fmod_value_shift";
char const* const CAudioImpl::s_szFmodPathAttribute = "fmod_path";
char const* const CAudioImpl::s_szFmodLocalizedAttribute = "fmod_localized";
char const* const CAudioImpl::s_szFmodEventTypeAttribute = "fmod_event_type";
char const* const CAudioImpl::s_szFmodEventPrefix = "event:/";
char const* const CAudioImpl::s_szFmodSnapshotPrefix = "snapshot:/";
char const* const CAudioImpl::s_szFmodBusPrefix = "bus:/";

//////////////////////////////////////////////////////////////////////////
FMOD_RESULT F_CALLBACK EventCallback(FMOD_STUDIO_EVENT_CALLBACK_TYPE type, FMOD_STUDIO_EVENTINSTANCE* event, void* parameters)
{
	FMOD::Studio::EventInstance* const pEvent = reinterpret_cast<FMOD::Studio::EventInstance*>(event);

	if (pEvent != nullptr)
	{
		CAudioEvent* pAudioEvent = nullptr;
		FMOD_RESULT const fmodResult = pEvent->getUserData(reinterpret_cast<void**>(&pAudioEvent));
		ASSERT_FMOD_OK;

		if (pAudioEvent != nullptr)
		{
			SAudioRequest request;
			SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportFinishedEvent> requestData(pAudioEvent->GetId(), true);
			request.flags = eAudioRequestFlags_ThreadSafePush;
			request.pData = &requestData;

			gEnv->pAudioSystem->PushRequest(request);
		}
	}

	return FMOD_OK;
}

//////////////////////////////////////////////////////////////////////////
FMOD_RESULT F_CALLBACK StandaloneFileCallback(FMOD_STUDIO_EVENT_CALLBACK_TYPE type, FMOD_STUDIO_EVENTINSTANCE* pEvent, void* pInOutParameters)
{
	if (pEvent != nullptr)
	{
		FMOD::Studio::EventInstance* const pEventInstance = reinterpret_cast<FMOD::Studio::EventInstance*>(pEvent);
		CAudioStandaloneFile* pStandaloneFileEvent = nullptr;
		FMOD_RESULT fmodResult = pEventInstance->getUserData(reinterpret_cast<void**>(&pStandaloneFileEvent));
		ASSERT_FMOD_OK;

		if (pStandaloneFileEvent != nullptr)
		{
			SAudioRequest request;
			if (type == FMOD_STUDIO_EVENT_CALLBACK_CREATE_PROGRAMMER_SOUND)
			{
				CRY_ASSERT(pInOutParameters);
				FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES* const pInOutProperties = reinterpret_cast<FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES*>(pInOutParameters);
				// Create the sound
				fmodResult = pStandaloneFileEvent->pLowLevelSystem->createSound(pStandaloneFileEvent->fileName, (pStandaloneFileEvent->bShouldBeStreamed) ? (FMOD_CREATESTREAM | FMOD_NONBLOCKING) : (FMOD_CREATECOMPRESSEDSAMPLE | FMOD_NONBLOCKING), NULL, &pStandaloneFileEvent->pLowLevelSound);
				ASSERT_FMOD_OK;
				// Pass the sound to FMOD
				pInOutProperties->sound = reinterpret_cast<FMOD_SOUND*>(pStandaloneFileEvent->pLowLevelSound);
			}
			else if (type == FMOD_STUDIO_EVENT_CALLBACK_STARTED)
			{
				pStandaloneFileEvent->bWaitingForData = true;  //will be evaluated in the main loop
			}
			else if (type == FMOD_STUDIO_EVENT_CALLBACK_STOPPED || type == FMOD_STUDIO_EVENT_CALLBACK_START_FAILED)
			{
				pStandaloneFileEvent->bHasFinished = true;  //will be evaluated in the main loop
			}
			else if (type == FMOD_STUDIO_EVENT_CALLBACK_DESTROY_PROGRAMMER_SOUND)
			{
				pStandaloneFileEvent->pLowLevelSound = nullptr;
				CRY_ASSERT(pInOutParameters);
				FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES* const pInOutProperties = reinterpret_cast<FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES*>(pInOutParameters);
				// Obtain the sound
				FMOD::Sound* pSound = reinterpret_cast<FMOD::Sound*>(pInOutProperties->sound);
				// Release the sound
				fmodResult = pSound->release();
				ASSERT_FMOD_OK;
			}
		}
	}

	return FMOD_OK;
}

///////////////////////////////////////////////////////////////////////////
CAudioImpl::CAudioImpl()
	: m_globalAudioObjectID(INVALID_AUDIO_OBJECT_ID)
	, m_pSystem(nullptr)
	, m_pLowLevelSystem(nullptr)
	, m_pMasterBank(nullptr)
	, m_pStringsBank(nullptr)
{
	m_registeredAudioObjects.reserve(256);
}

///////////////////////////////////////////////////////////////////////////
CAudioImpl::~CAudioImpl()
{
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::Update(float const deltaTime)
{
	if (m_pSystem != nullptr)
	{
		FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;
		fmodResult = m_pSystem->update();
		ASSERT_FMOD_OK;

		if (!m_pendingAudioEvents.empty())
		{
			AudioEvents::iterator iter(m_pendingAudioEvents.begin());
			AudioEvents::const_iterator iterEnd(m_pendingAudioEvents.cend());

			while (iter != iterEnd)
			{
				if ((*iter)->GetAudioObjectData()->SetAudioEvent(*iter))
				{
					iter = m_pendingAudioEvents.erase(iter);
					iterEnd = m_pendingAudioEvents.cend();
					continue;
				}

				++iter;
			}
		}

		for (CAudioStandaloneFile* pCurrentStandaloneFile : m_pendingStandaloneFiles)
		{
			if (pCurrentStandaloneFile->bWaitingForData)
			{
				FMOD_OPENSTATE state = FMOD_OPENSTATE_ERROR;
				if (pCurrentStandaloneFile->pLowLevelSound)
				{
					pCurrentStandaloneFile->pLowLevelSound->getOpenState(&state, nullptr, nullptr, nullptr);
				}
				if (state != FMOD_OPENSTATE_LOADING)
				{
					bool bStarted = (state == FMOD_OPENSTATE_READY || state == FMOD_OPENSTATE_PLAYING);

					SAudioRequest request;
					SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportStartedFile> requestData(pCurrentStandaloneFile->fileInstanceId, pCurrentStandaloneFile->fileName.c_str(), bStarted);
					request.pData = &requestData;
					request.flags = eAudioRequestFlags_ThreadSafePush;
					gEnv->pAudioSystem->PushRequest(request);
					pCurrentStandaloneFile->bWaitingForData = false;
				}
			}
			if (pCurrentStandaloneFile->bHasFinished)
			{
				//send finished request
				SAudioRequest request;
				SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportStoppedFile> requestData(pCurrentStandaloneFile->fileInstanceId, pCurrentStandaloneFile->fileName.c_str());
				request.pData = &requestData;
				request.flags = eAudioRequestFlags_ThreadSafePush;
				gEnv->pAudioSystem->PushRequest(request);
				pCurrentStandaloneFile->bHasFinished = false;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::Init()
{
	string const gameFolder = PathUtil::GetGameFolder().c_str();

	if (gameFolder.empty())
	{
		CryFatalError("<Audio>: Needs a valid game folder to proceed!");
	}

	m_regularSoundBankFolder = gameFolder.c_str();
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

	CryFixedStringT<MAX_AUDIO_MISC_STRING_LENGTH> systemVersion;
	systemVersion.Format("%08x", version);
	CryFixedStringT<MAX_AUDIO_MISC_STRING_LENGTH> headerVersion;
	headerVersion.Format("%08x", FMOD_VERSION);
	CreateVersionString(systemVersion);
	CreateVersionString(headerVersion);

	m_fullImplString = FMOD_IMPL_INFO_STRING;
	m_fullImplString += "System: ";
	m_fullImplString += systemVersion;
	m_fullImplString += " Header: ";
	m_fullImplString += headerVersion;
	m_fullImplString += " (";
	m_fullImplString += gameFolder + CRY_NATIVE_PATH_SEPSTR FMOD_IMPL_DATA_ROOT ")";
#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE

	int sampleRate = 0;
	int numRawSpeakers = 0;
	FMOD_SPEAKERMODE speakerMode = FMOD_SPEAKERMODE_DEFAULT;
	fmodResult = m_pLowLevelSystem->getSoftwareFormat(&sampleRate, &speakerMode, &numRawSpeakers);
	ASSERT_FMOD_OK;
	fmodResult = m_pLowLevelSystem->setSoftwareFormat(sampleRate, speakerMode, numRawSpeakers);
	ASSERT_FMOD_OK;

	fmodResult = m_pLowLevelSystem->set3DSettings(g_audioImplCVars.m_dopplerScale, g_audioImplCVars.m_distanceFactor, g_audioImplCVars.m_rolloffScale);
	ASSERT_FMOD_OK;

	void* pExtraDriverData = nullptr;
	int initFlags = FMOD_INIT_NORMAL | FMOD_INIT_VOL0_BECOMES_VIRTUAL;
	int studioInitFlags = FMOD_STUDIO_INIT_NORMAL;

	if (g_audioImplCVars.m_enableLiveUpdate > 0)
	{
		studioInitFlags |= FMOD_STUDIO_INIT_LIVEUPDATE;
	}

	if (g_audioImplCVars.m_enableSynchronousUpdate > 0)
	{
		studioInitFlags |= FMOD_STUDIO_INIT_SYNCHRONOUS_UPDATE;
	}

	fmodResult = m_pSystem->initialize(g_audioImplCVars.m_maxChannels, studioInitFlags, initFlags, pExtraDriverData);
	ASSERT_FMOD_OK;

	if (!LoadMasterBanks())
	{
		return eAudioRequestStatus_Failure;
	}

	FMOD_3D_ATTRIBUTES attributes = {
		{ 0 }
	};
	attributes.forward.z = 1.0f;
	attributes.up.y = 1.0f;
	fmodResult = m_pSystem->setListenerAttributes(0, &attributes);
	ASSERT_FMOD_OK;

	return (fmodResult == FMOD_OK) ? eAudioRequestStatus_Success : eAudioRequestStatus_Failure;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::ShutDown()
{
	FMOD_RESULT fmodResult = FMOD_OK;

	if (m_pSystem != nullptr)
	{
		UnloadMasterBanks();

		fmodResult = m_pSystem->release();
		ASSERT_FMOD_OK;
	}

	return (fmodResult == FMOD_OK) ? eAudioRequestStatus_Success : eAudioRequestStatus_Failure;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::Release()
{
	POOL_FREE(this);

	// Freeing Memory Pool Memory again
	uint8* pMemSystem = g_audioImplMemoryPool.Data();
	g_audioImplMemoryPool.UnInitMem();

	if (pMemSystem)
	{
		delete[] (uint8*)(pMemSystem);
	}

	g_audioImplCVars.UnregisterVariables();

	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::OnLoseFocus()
{
	return MuteMasterBus(true);
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::OnGetFocus()
{
	return MuteMasterBus(false);
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::MuteAll()
{
	return MuteMasterBus(true);
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UnmuteAll()
{
	return MuteMasterBus(false);
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::StopAllSounds()
{
	FMOD::Studio::Bus* pMasterBus = nullptr;
	FMOD_RESULT fmodResult = m_pSystem->getBus("bus:/", &pMasterBus);
	ASSERT_FMOD_OK;

	if (pMasterBus != nullptr)
	{
		fmodResult = pMasterBus->stopAllEvents(FMOD_STUDIO_STOP_IMMEDIATE);
		ASSERT_FMOD_OK;
	}

	return (fmodResult == FMOD_OK) ? eAudioRequestStatus_Success : eAudioRequestStatus_Failure;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::RegisterAudioObject(IAudioObject* const pAudioObject)
{
	CAudioObject* const pFmodAudioObject = static_cast<CAudioObject* const>(pAudioObject);

	if (!stl::push_back_unique(m_registeredAudioObjects, pFmodAudioObject))
	{
		g_audioImplLogger.Log(eAudioLogType_Warning, "Trying to register an already registered audio object. (%u)", pFmodAudioObject->GetId());
	}

	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::RegisterAudioObject(
  IAudioObject* const pAudioObject,
  char const* const szAudioObjectName)
{
	CAudioObject* const pFmodAudioObject = static_cast<CAudioObject* const>(pAudioObject);

	if (!stl::push_back_unique(m_registeredAudioObjects, pFmodAudioObject))
	{
		g_audioImplLogger.Log(eAudioLogType_Warning, "Trying to register an already registered audio object. (%u)", pFmodAudioObject->GetId());
	}

	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UnregisterAudioObject(IAudioObject* const pAudioObject)
{
	CAudioObject* const pFmodAudioObject = static_cast<CAudioObject* const>(pAudioObject);

	if (!stl::find_and_erase(m_registeredAudioObjects, pFmodAudioObject))
	{
		g_audioImplLogger.Log(eAudioLogType_Warning, "Trying to unregister a non-existing audio object. (%u)", pFmodAudioObject->GetId());
	}

	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::ResetAudioObject(IAudioObject* const pAudioObject)
{
	CAudioObject* const pFmodAudioObject = static_cast<CAudioObject* const>(pAudioObject);
	pFmodAudioObject->Reset();
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UpdateAudioObject(IAudioObject* const pAudioObject)
{
	CAudioObject* const pFmodAudioObject = static_cast<CAudioObject* const>(pAudioObject);
	return pFmodAudioObject ? eAudioRequestStatus_Success : eAudioRequestStatus_Failure;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::PlayFile(SAudioStandaloneFileInfo* const _pAudioStandaloneFileInfo)
{
	CAudioObject* const pFmodAudioObject = static_cast<CAudioObject* const>(_pAudioStandaloneFileInfo->pAudioObject);
	CAudioTrigger const* const pFmodAudioTrigger = static_cast<CAudioTrigger const* const>(_pAudioStandaloneFileInfo->pUsedAudioTrigger);
	CAudioStandaloneFile* const pPlayStandaloneEvent = static_cast<CAudioStandaloneFile* const>(_pAudioStandaloneFileInfo->pImplData);

	if ((pFmodAudioObject != nullptr) && (pFmodAudioTrigger != nullptr) && (pPlayStandaloneEvent != nullptr))
	{
		if (pFmodAudioTrigger->m_eventType == eFmodEventType_Start)
		{
			FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;
			FMOD::Studio::EventDescription* pEventDescription = pFmodAudioTrigger->m_pEventDescription;

			if (pEventDescription == nullptr)
			{
				fmodResult = m_pSystem->getEventByID(&pFmodAudioTrigger->m_guid, &pEventDescription);
				ASSERT_FMOD_OK;
			}

			if (pEventDescription != nullptr)
			{
				CRY_ASSERT_MESSAGE(pPlayStandaloneEvent->programmerSoundEvent.GetInstance() == nullptr, "must not be set yet");

				FMOD::Studio::EventInstance* pInstance = nullptr;
				fmodResult = pEventDescription->createInstance(&pInstance);
				ASSERT_FMOD_OK;
				pPlayStandaloneEvent->programmerSoundEvent.SetInstance(pInstance);
				fmodResult = pInstance->setCallback(StandaloneFileCallback, FMOD_STUDIO_EVENT_CALLBACK_ALL);
				ASSERT_FMOD_OK;
				fmodResult = pInstance->setUserData(pPlayStandaloneEvent);
				ASSERT_FMOD_OK;
				fmodResult = pInstance->set3DAttributes(&pFmodAudioObject->Get3DAttributes());
				ASSERT_FMOD_OK;

				FMOD_STUDIO_USER_PROPERTY userProperty;
				fmodResult = pEventDescription->getUserProperty("Streamed", &userProperty);
				pPlayStandaloneEvent->bShouldBeStreamed = (fmodResult == FMOD_OK); //if the event has the property "Streamed" we stream the programmer sound later on

				CRY_ASSERT(pPlayStandaloneEvent->programmerSoundEvent.GetEventPathId() == AUDIO_INVALID_CRC32);
				pPlayStandaloneEvent->fileId = _pAudioStandaloneFileInfo->fileId;
				pPlayStandaloneEvent->fileInstanceId = _pAudioStandaloneFileInfo->fileInstanceId;

				static string s_localizedfilesFolder = PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + PathUtil::GetLocalizationFolder() + CRY_NATIVE_PATH_SEPSTR + m_language.c_str() + CRY_NATIVE_PATH_SEPSTR;
				static string s_nonLocalizedfilesFolder = PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR;
				static string filePath;

				if (_pAudioStandaloneFileInfo->bLocalized)
				{
					filePath = s_localizedfilesFolder + _pAudioStandaloneFileInfo->szFileName + ".mp3";
				}
				else
				{
					filePath = s_nonLocalizedfilesFolder + _pAudioStandaloneFileInfo->szFileName + ".mp3";
				}
				pPlayStandaloneEvent->fileName = filePath.c_str();
				pPlayStandaloneEvent->pLowLevelSystem = m_pLowLevelSystem;
				pPlayStandaloneEvent->programmerSoundEvent.SetEventPathId(pFmodAudioTrigger->m_eventPathId);
				pPlayStandaloneEvent->programmerSoundEvent.SetAudioObjectData(pFmodAudioObject);

				CRY_ASSERT_MESSAGE(std::find(m_pendingStandaloneFiles.begin(), m_pendingStandaloneFiles.end(), pPlayStandaloneEvent) == m_pendingStandaloneFiles.end(), "standalone file was already in the pending standalone files list");
				m_pendingStandaloneFiles.push_back(pPlayStandaloneEvent);
				CRY_ASSERT_MESSAGE(std::find(m_pendingAudioEvents.begin(), m_pendingAudioEvents.end(), &pPlayStandaloneEvent->programmerSoundEvent) == m_pendingAudioEvents.end(), "Event was already in the pending event list");
				m_pendingAudioEvents.push_back(&pPlayStandaloneEvent->programmerSoundEvent);
				return eAudioRequestStatus_Success;
			}
		}
	}

	g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioObject, AudioTrigger or StandaloneFile passed to the Fmod implementation of PlayFile.");
	return eAudioRequestStatus_Failure;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::StopFile(SAudioStandaloneFileInfo* const _pAudioStandaloneFileInfo)
{
	CAudioStandaloneFile* const pFmodStandaloneFile = static_cast<CAudioStandaloneFile* const>(_pAudioStandaloneFileInfo->pImplData);

	if (pFmodStandaloneFile != nullptr)
	{
		pFmodStandaloneFile->bWaitingForData = false;
		FMOD::Studio::EventInstance* const pEventInstance = pFmodStandaloneFile->programmerSoundEvent.GetInstance();
		CRY_ASSERT(pEventInstance != nullptr);

		FMOD_RESULT const fmodResult = pEventInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
		ASSERT_FMOD_OK;
		return eAudioRequestStatus_Pending;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid SAudioStandaloneFileInfo passed to the Fmod implementation of StopFile.");
	}

	return eAudioRequestStatus_Failure;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::PrepareTriggerSync(
  IAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UnprepareTriggerSync(
  IAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::PrepareTriggerAsync(
  IAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger,
  IAudioEvent* const pAudioEvent)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UnprepareTriggerAsync(
  IAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger,
  IAudioEvent* const pAudioEvent)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::ActivateTrigger(
  IAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger,
  IAudioEvent* const pAudioEvent)
{
	EAudioRequestStatus requestResult = eAudioRequestStatus_Failure;
	CAudioObject* const pFmodAudioObject = static_cast<CAudioObject* const>(pAudioObject);
	CAudioTrigger const* const pFmodAudioTrigger = static_cast<CAudioTrigger const* const>(pAudioTrigger);
	CAudioEvent* const pFmodAudioEvent = static_cast<CAudioEvent*>(pAudioEvent);

	if ((pFmodAudioObject != nullptr) && (pFmodAudioTrigger != nullptr) && (pFmodAudioEvent != nullptr))
	{
		if (pFmodAudioTrigger->m_eventType == eFmodEventType_Start)
		{
			FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;
			FMOD::Studio::EventDescription* pEventDescription = pFmodAudioTrigger->m_pEventDescription;

			if (pEventDescription == nullptr)
			{
				fmodResult = m_pSystem->getEventByID(&pFmodAudioTrigger->m_guid, &pEventDescription);
				ASSERT_FMOD_OK;
			}

			if (pEventDescription != nullptr)
			{
				CRY_ASSERT(pFmodAudioEvent->GetInstance() == nullptr);

				FMOD::Studio::EventInstance* pInstance = nullptr;
				fmodResult = pEventDescription->createInstance(&pInstance);
				ASSERT_FMOD_OK;
				pFmodAudioEvent->SetInstance(pInstance);
				fmodResult = pFmodAudioEvent->GetInstance()->setCallback(EventCallback, FMOD_STUDIO_EVENT_CALLBACK_START_FAILED | FMOD_STUDIO_EVENT_CALLBACK_STOPPED);
				ASSERT_FMOD_OK;
				fmodResult = pFmodAudioEvent->GetInstance()->setUserData(pFmodAudioEvent);
				ASSERT_FMOD_OK;
				fmodResult = pFmodAudioEvent->GetInstance()->set3DAttributes(&pFmodAudioObject->Get3DAttributes());
				ASSERT_FMOD_OK;

				CRY_ASSERT(pFmodAudioEvent->GetEventPathId() == AUDIO_INVALID_CRC32);
				pFmodAudioEvent->SetEventPathId(pFmodAudioTrigger->m_eventPathId);
				pFmodAudioEvent->SetAudioObjectData(pFmodAudioObject);

				CRY_ASSERT_MESSAGE(std::find(m_pendingAudioEvents.begin(), m_pendingAudioEvents.end(), pFmodAudioEvent) == m_pendingAudioEvents.end(), "Event was already in the pending list");
				m_pendingAudioEvents.push_back(pFmodAudioEvent);
				requestResult = eAudioRequestStatus_Success;
			}
		}
		else
		{
			pFmodAudioObject->StopEvent(pFmodAudioTrigger->m_eventPathId);

			// Return failure here so the ATL does not keep track of this event.
			requestResult = eAudioRequestStatus_Failure;
		}
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioObjectData, ATLTriggerData or EventData passed to the Fmod implementation of ActivateTrigger.");
	}

	return requestResult;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::StopEvent(
  IAudioObject* const pAudioObject,
  IAudioEvent const* const pAudioEvent)
{
	EAudioRequestStatus requestResult = eAudioRequestStatus_Failure;
	CAudioEvent const* const pFmodAudioEvent = static_cast<CAudioEvent const* const>(pAudioEvent);

	if (pFmodAudioEvent != nullptr)
	{
		FMOD_RESULT const fmodResult = pFmodAudioEvent->GetInstance()->stop(FMOD_STUDIO_STOP_IMMEDIATE);
		ASSERT_FMOD_OK;
		requestResult = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid EventData passed to the Fmod implementation of StopEvent.");
	}

	return requestResult;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::StopAllEvents(IAudioObject* const pAudioObject)
{
	EAudioRequestStatus requestResult = eAudioRequestStatus_Failure;
	CAudioObject* const pFmodAudioObject = static_cast<CAudioObject* const>(pAudioObject);

	if (pFmodAudioObject != nullptr)
	{
		pFmodAudioObject->StopAllEvents();
		requestResult = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioObjectData passed to the Fmod implementation of StopAllEvents.");
	}

	return requestResult;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::Set3DAttributes(
  IAudioObject* const pAudioObject,
  SAudioObject3DAttributes const& attributes)
{
	EAudioRequestStatus requestResult = eAudioRequestStatus_Failure;
	CAudioObject* const pFmodAudioObject = static_cast<CAudioObject* const>(pAudioObject);

	if (pFmodAudioObject != nullptr)
	{
		pFmodAudioObject->Set3DAttributes(attributes);
		requestResult = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioObjectData passed to the Fmod implementation of SetPosition.");
	}

	return requestResult;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::SetEnvironment(
  IAudioObject* const pAudioObject,
  IAudioEnvironment const* const pAudioEnvironment,
  float const amount)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;
	CAudioObject* const pFmodAudioObject = static_cast<CAudioObject* const>(pAudioObject);
	CAudioEnvironment const* const pFmodAudioEnvironment = static_cast<CAudioEnvironment const* const>(pAudioEnvironment);

	if ((pFmodAudioObject != nullptr) && (pFmodAudioEnvironment != nullptr))
	{
		if (pFmodAudioObject->GetId() != m_globalAudioObjectID)
		{
			pFmodAudioObject->SetEnvironment(pFmodAudioEnvironment, amount);
		}
		else
		{
			for (auto const pRegisteredAudioObject : m_registeredAudioObjects)
			{
				pRegisteredAudioObject->SetEnvironment(pFmodAudioEnvironment, amount);
			}
		}

		result = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioObjectData or EnvironmentData passed to the Fmod implementation of SetEnvironment");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::SetRtpc(
  IAudioObject* const pAudioObject,
  IAudioRtpc const* const pAudioRtpc,
  float const value)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;
	CAudioObject* const pFmodAudioObject = static_cast<CAudioObject* const>(pAudioObject);
	CAudioParameter const* const pFmodAudioParameter = static_cast<CAudioParameter const* const>(pAudioRtpc);

	if ((pFmodAudioObject != nullptr) && (pFmodAudioParameter != nullptr))
	{
		if (pFmodAudioObject->GetId() != m_globalAudioObjectID)
		{
			pFmodAudioObject->SetParameter(pFmodAudioParameter, value);
		}
		else
		{
			for (auto const pRegisteredAudioObject : m_registeredAudioObjects)
			{
				pRegisteredAudioObject->SetParameter(pFmodAudioParameter, value);
			}
		}

		result = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioObjectData or RtpcData passed to the Fmod implementation of SetRtpc");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::SetSwitchState(
  IAudioObject* const pAudioObject,
  IAudioSwitchState const* const pAudioSwitchState)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;
	CAudioObject* const pFmodAudioObject = static_cast<CAudioObject* const>(pAudioObject);
	CAudioSwitchState const* const pFmodAudioSwitchState = static_cast<CAudioSwitchState const* const>(pAudioSwitchState);

	if ((pFmodAudioObject != nullptr) && (pFmodAudioSwitchState != nullptr))
	{
		if (pFmodAudioObject->GetId() != m_globalAudioObjectID)
		{
			pFmodAudioObject->SetSwitch(pFmodAudioSwitchState);
		}
		else
		{
			for (auto const pRegisteredAudioObject : m_registeredAudioObjects)
			{
				pRegisteredAudioObject->SetSwitch(pFmodAudioSwitchState);
			}
		}

		result = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioObjectData or RtpcData passed to the Fmod implementation of SetSwitchState");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::SetObstructionOcclusion(
  IAudioObject* const pAudioObject,
  float const obstruction,
  float const occlusion)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;
	CAudioObject* const pFmodAudioObject = static_cast<CAudioObject* const>(pAudioObject);

	if (pFmodAudioObject != nullptr)
	{
		if (pFmodAudioObject->GetId() != m_globalAudioObjectID)
		{
			pFmodAudioObject->SetObstructionOcclusion(obstruction, occlusion);
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Error, "Trying to set occlusion and obstruction values on the global audio object!");
		}

		result = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioObjectData passed to the Fmod implementation of SetObjectObstructionAndOcclusion");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::SetListener3DAttributes(
  IAudioListener* const pAudioListener,
  SAudioObject3DAttributes const& attributes)
{
	EAudioRequestStatus requestResult = eAudioRequestStatus_Failure;
	CAudioListener* const pFmodAudioListener = static_cast<CAudioListener* const>(pAudioListener);

	if (pFmodAudioListener != nullptr)
	{
		FillFmodObjectPosition(attributes, pFmodAudioListener->Get3DAttributes());
		FMOD_RESULT const fmodResult = m_pSystem->setListenerAttributes(pFmodAudioListener->GetId(), &pFmodAudioListener->Get3DAttributes());
		ASSERT_FMOD_OK;
		requestResult = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid ATLListenerData passed to the Fmod implementation of SetListenerPosition");
	}

	return requestResult;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::RegisterInMemoryFile(SAudioFileEntryInfo* const pFileEntryInfo)
{
	EAudioRequestStatus requestResult = eAudioRequestStatus_Failure;

	if (pFileEntryInfo != nullptr)
	{
		CAudioFileEntry* const pFmodAudioFileEntry = static_cast<CAudioFileEntry*>(pFileEntryInfo->pImplData);

		if (pFmodAudioFileEntry != nullptr)
		{
#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
			// loadBankMemory requires 32-byte alignment when using FMOD_STUDIO_LOAD_MEMORY_POINT
			if ((reinterpret_cast<uintptr_t>(pFileEntryInfo->pFileData) & (32 - 1)) > 0)
			{
				CryFatalError("<Audio>: allocation not %d byte aligned!", 32);
			}
#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE

			FMOD_RESULT const fmodResult = m_pSystem->loadBankMemory(static_cast<char*>(pFileEntryInfo->pFileData), static_cast<int>(pFileEntryInfo->size), FMOD_STUDIO_LOAD_MEMORY_POINT, FMOD_STUDIO_LOAD_BANK_NORMAL, &pFmodAudioFileEntry->pBank);
			ASSERT_FMOD_OK;
			requestResult = (fmodResult == FMOD_OK) ? eAudioRequestStatus_Success : eAudioRequestStatus_Failure;
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioFileEntryData passed to the Fmod implementation of RegisterInMemoryFile");
		}
	}

	return requestResult;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UnregisterInMemoryFile(SAudioFileEntryInfo* const pFileEntryInfo)
{
	EAudioRequestStatus requestResult = eAudioRequestStatus_Failure;

	if (pFileEntryInfo != nullptr)
	{
		CAudioFileEntry* const pFmodAudioFileEntry = static_cast<CAudioFileEntry*>(pFileEntryInfo->pImplData);

		if (pFmodAudioFileEntry != nullptr)
		{
			FMOD_RESULT fmodResult = pFmodAudioFileEntry->pBank->unload();
			ASSERT_FMOD_OK;

			FMOD_STUDIO_LOADING_STATE loadingState;

			do
			{
				fmodResult = m_pSystem->update();
				ASSERT_FMOD_OK;
				fmodResult = pFmodAudioFileEntry->pBank->getLoadingState(&loadingState);
				ASSERT_FMOD_OK_OR_INVALID_HANDLE;
			}
			while (loadingState == FMOD_STUDIO_LOADING_STATE_UNLOADING);

			pFmodAudioFileEntry->pBank = nullptr;
			requestResult = (fmodResult == FMOD_OK) ? eAudioRequestStatus_Success : eAudioRequestStatus_Failure;
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioFileEntryData passed to the Fmod implementation of UnregisterInMemoryFile");
		}
	}

	return requestResult;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::ParseAudioFileEntry(
  XmlNodeRef const pAudioFileEntryNode,
  SAudioFileEntryInfo* const pFileEntryInfo)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	if ((_stricmp(pAudioFileEntryNode->getTag(), s_szFmodFileTag) == 0) && (pFileEntryInfo != nullptr))
	{
		char const* const szFmodAudioFileEntryName = pAudioFileEntryNode->getAttr(s_szFmodNameAttribute);

		if (szFmodAudioFileEntryName != nullptr && szFmodAudioFileEntryName[0] != '\0')
		{
			char const* const szFmodLocalized = pAudioFileEntryNode->getAttr(s_szFmodLocalizedAttribute);
			pFileEntryInfo->bLocalized = (szFmodLocalized != nullptr) && (_stricmp(szFmodLocalized, "true") == 0);
			pFileEntryInfo->szFileName = szFmodAudioFileEntryName;

			// FMOD Studio always uses 32 byte alignment for preloaded banks regardless of the platform.
			pFileEntryInfo->memoryBlockAlignment = 32;

			POOL_NEW(CAudioFileEntry, pFileEntryInfo->pImplData);

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
void CAudioImpl::DeleteAudioFileEntry(IAudioFileEntry* const pOldAudioFileEntry)
{
	POOL_FREE(pOldAudioFileEntry);
}

//////////////////////////////////////////////////////////////////////////
char const* const CAudioImpl::GetAudioFileLocation(SAudioFileEntryInfo* const pFileEntryInfo)
{
	char const* sResult = nullptr;

	if (pFileEntryInfo != nullptr)
	{
		sResult = pFileEntryInfo->bLocalized ? m_localizedSoundBankFolder.c_str() : m_regularSoundBankFolder.c_str();
	}

	return sResult;
}

///////////////////////////////////////////////////////////////////////////
IAudioObject* CAudioImpl::NewGlobalAudioObject(AudioObjectId const audioObjectID)
{
	POOL_NEW_CREATE(CAudioObject, pFmodAudioObject)(audioObjectID);
	m_globalAudioObjectID = audioObjectID;
	return pFmodAudioObject;
}

///////////////////////////////////////////////////////////////////////////
IAudioObject* CAudioImpl::NewAudioObject(AudioObjectId const audioObjectID)
{
	POOL_NEW_CREATE(CAudioObject, pFmodAudioObject)(audioObjectID);
	return pFmodAudioObject;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioObject(IAudioObject const* const pOldAudioObject)
{
	POOL_FREE_CONST(pOldAudioObject);
}

///////////////////////////////////////////////////////////////////////////
CryAudio::Impl::IAudioListener* CAudioImpl::NewDefaultAudioListener(AudioObjectId const audioObjectId)
{
	POOL_NEW_CREATE(CAudioListener, pAudioListener)(0);
	return pAudioListener;
}

///////////////////////////////////////////////////////////////////////////
CryAudio::Impl::IAudioListener* CAudioImpl::NewAudioListener(AudioObjectId const audioObjectId)
{
	static int id = 0;
	POOL_NEW_CREATE(CAudioListener, pAudioListener)(++id);
	return pAudioListener;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioListener(CryAudio::Impl::IAudioListener* const pOldAudioListener)
{
	POOL_FREE(pOldAudioListener);
}

//////////////////////////////////////////////////////////////////////////
IAudioEvent* CAudioImpl::NewAudioEvent(AudioEventId const audioEventID)
{
	POOL_NEW_CREATE(CAudioEvent, pNewEvent)(audioEventID);
	return pNewEvent;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioEvent(IAudioEvent const* const pOldAudioEvent)
{
	POOL_FREE_CONST(pOldAudioEvent);
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::ResetAudioEvent(IAudioEvent* const pAudioEvent)
{
	CRY_ASSERT(pAudioEvent);
	CAudioEvent* const pFmodAudioEvent = static_cast<CAudioEvent*>(pAudioEvent);
	AudioEvents::iterator it = std::find(m_pendingAudioEvents.begin(), m_pendingAudioEvents.end(), pFmodAudioEvent);
	if (it != m_pendingAudioEvents.end())
	{
		m_pendingAudioEvents.erase(it);
	}
	pFmodAudioEvent->Reset();
}

//////////////////////////////////////////////////////////////////////////
IAudioStandaloneFile* CAudioImpl::NewAudioStandaloneFile()
{
	POOL_NEW_CREATE(CAudioStandaloneFile, pAudioStandaloneFile);
	return pAudioStandaloneFile;
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioStandaloneFile(IAudioStandaloneFile const* const _pOldAudioStandaloneFile)
{
	POOL_FREE_CONST(_pOldAudioStandaloneFile);
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::ResetAudioStandaloneFile(IAudioStandaloneFile* const _pAudioStandaloneFile)
{
	CRY_ASSERT(_pAudioStandaloneFile);
	CAudioStandaloneFile* const pStandaloneImpl = static_cast<CAudioStandaloneFile* const>(_pAudioStandaloneFile);
	CRY_ASSERT_MESSAGE(pStandaloneImpl->pLowLevelSound == nullptr, "Sound must not play when the file is reset");

	StandaloneFiles::iterator itStandaloneFile = std::find(m_pendingStandaloneFiles.begin(), m_pendingStandaloneFiles.end(), pStandaloneImpl);
	if (itStandaloneFile != m_pendingStandaloneFiles.end())
	{
		m_pendingStandaloneFiles.erase(itStandaloneFile);
	}
	AudioEvents::iterator itEvent = std::find(m_pendingAudioEvents.begin(), m_pendingAudioEvents.end(), &pStandaloneImpl->programmerSoundEvent);
	if (itEvent != m_pendingAudioEvents.end())
	{
		m_pendingAudioEvents.erase(itEvent);
	}
	pStandaloneImpl->Reset();
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::GamepadConnected(TAudioGamepadUniqueID const deviceUniqueID)
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::GamepadDisconnected(TAudioGamepadUniqueID const deviceUniqueID)
{
}

///////////////////////////////////////////////////////////////////////////
IAudioTrigger const* CAudioImpl::NewAudioTrigger(XmlNodeRef const pAudioTriggerNode, SAudioTriggerInfo& info)
{
	IAudioTrigger* pAudioTrigger = nullptr;
	char const* const szTag = pAudioTriggerNode->getTag();

	if (_stricmp(szTag, s_szFmodEventTag) == 0)
	{
		stack_string path(s_szFmodEventPrefix);
		path += pAudioTriggerNode->getAttr(s_szFmodNameAttribute);
		FMOD_GUID guid = { 0 };

		if (m_pSystem->lookupID(path.c_str(), &guid) == FMOD_OK)
		{
			EFmodEventType eventType = eFmodEventType_Start;
			char const* const szFmodEventType = pAudioTriggerNode->getAttr(s_szFmodEventTypeAttribute);

			if (szFmodEventType != nullptr &&
			    szFmodEventType[0] != '\0' &&
			    _stricmp(szFmodEventType, "stop") == 0)
			{
				eventType = eFmodEventType_Stop;
			}

#if defined (INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
			POOL_NEW(CAudioTrigger, pAudioTrigger)(AudioStringToId(path.c_str()), eventType, nullptr, guid, path.c_str());
#else
			POOL_NEW(CAudioTrigger, pAudioTrigger)(AudioStringToId(path.c_str()), eventType, nullptr, guid);
#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Warning, "Unknown Fmod event: %s", path.c_str());
		}
	}
	else if (_stricmp(szTag, s_szFmodSnapshotTag) == 0)
	{
		stack_string path(s_szFmodSnapshotPrefix);
		path += pAudioTriggerNode->getAttr(s_szFmodNameAttribute);
		FMOD_GUID guid = { 0 };

		if (m_pSystem->lookupID(path.c_str(), &guid) == FMOD_OK)
		{
			FMOD::Studio::EventDescription* pEventDescription = nullptr;
			m_pSystem->getEventByID(&guid, &pEventDescription);

#if defined (INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
			POOL_NEW(CAudioTrigger, pAudioTrigger)(AudioStringToId(path.c_str()), eFmodEventType_Start, pEventDescription, guid, path.c_str());
#else
			POOL_NEW(CAudioTrigger, pAudioTrigger)(AudioStringToId(path.c_str()), eFmodEventType_Start, pEventDescription, guid);
#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Warning, "Unknown Fmod snapshot: %s", path.c_str());
		}
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Warning, "Unknown Fmod tag: %s", szTag);
	}

	info.maxRadius = 0;
	return pAudioTrigger;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioTrigger(IAudioTrigger const* const pOldAudioTrigger)
{
	POOL_FREE_CONST(pOldAudioTrigger);
}

///////////////////////////////////////////////////////////////////////////
IAudioRtpc const* CAudioImpl::NewAudioRtpc(XmlNodeRef const pAudioParameterNode)
{
	CAudioParameter* pFmodAudioParameter = nullptr;
	char const* const szTag = pAudioParameterNode->getTag();
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
		char const* const szName = pAudioParameterNode->getAttr(s_szFmodNameAttribute);
		char const* const szPath = pAudioParameterNode->getAttr(s_szFmodPathAttribute);
		path += szPath;
		uint32 const pathId = AudioStringToId(path.c_str());

		float multiplier = 1.0f;
		float shift = 0.0f;
		pAudioParameterNode->getAttr(s_szFmodMutiplierAttribute, multiplier);
		pAudioParameterNode->getAttr(s_szFmodShiftAttribute, shift);

		POOL_NEW(CAudioParameter, pFmodAudioParameter)(pathId, multiplier, shift, szName);
		g_parameterToIndex.emplace(std::piecewise_construct, std::make_tuple(pFmodAudioParameter), std::make_tuple(FMOD_IMPL_INVALID_INDEX));
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Warning, "Unknown Fmod tag: %s", szTag);
	}

	return static_cast<IAudioRtpc*>(pFmodAudioParameter);
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioRtpc(IAudioRtpc const* const pOldAudioRtpc)
{
	CAudioParameter const* const pFmodAudioParameter = static_cast<CAudioParameter const* const>(pOldAudioRtpc);

	for (auto const pAudioObject : m_registeredAudioObjects)
	{
		pAudioObject->RemoveParameter(pFmodAudioParameter);
	}

	g_parameterToIndex.erase(pFmodAudioParameter);
	POOL_FREE_CONST(pOldAudioRtpc);
}

///////////////////////////////////////////////////////////////////////////
IAudioSwitchState const* CAudioImpl::NewAudioSwitchState(XmlNodeRef const pAudioSwitchNode)
{
	CAudioSwitchState* pFmodAudioSwitchState = nullptr;
	char const* const szTag = pAudioSwitchNode->getTag();
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
		char const* const szFmodParameterName = pAudioSwitchNode->getAttr(s_szFmodNameAttribute);
		char const* const szFmodPath = pAudioSwitchNode->getAttr(s_szFmodPathAttribute);
		char const* const szFmodParameterValue = pAudioSwitchNode->getAttr(s_szFmodValueAttribute);
		path += szFmodPath;
		uint32 const pathId = AudioStringToId(path.c_str());
		float const value = static_cast<float>(atof(szFmodParameterValue));
		POOL_NEW(CAudioSwitchState, pFmodAudioSwitchState)(pathId, value, szFmodParameterName);
		g_switchToIndex.emplace(std::piecewise_construct, std::make_tuple(pFmodAudioSwitchState), std::make_tuple(FMOD_IMPL_INVALID_INDEX));
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Warning, "Unknown Fmod tag: %s", szTag);
	}

	return static_cast<IAudioSwitchState*>(pFmodAudioSwitchState);
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioSwitchState(IAudioSwitchState const* const pOldAudioSwitchState)
{
	CAudioSwitchState const* const pFmodAudioSwitchState = static_cast<CAudioSwitchState const* const>(pOldAudioSwitchState);

	for (auto const pAudioObject : m_registeredAudioObjects)
	{
		pAudioObject->RemoveSwitch(pFmodAudioSwitchState);
	}

	g_switchToIndex.erase(pFmodAudioSwitchState);
	POOL_FREE_CONST(pOldAudioSwitchState);
}

///////////////////////////////////////////////////////////////////////////
IAudioEnvironment const* CAudioImpl::NewAudioEnvironment(XmlNodeRef const pAudioEnvironmentNode)
{
	IAudioEnvironment* pAudioEnvironment = nullptr;
	char const* const szTag = pAudioEnvironmentNode->getTag();

	if (_stricmp(szTag, s_szFmodBusTag) == 0)
	{
		stack_string path(s_szFmodBusPrefix);
		path += pAudioEnvironmentNode->getAttr(s_szFmodNameAttribute);
		FMOD_GUID guid = { 0 };

		if (m_pSystem->lookupID(path.c_str(), &guid) == FMOD_OK)
		{
			FMOD::Studio::Bus* pBus = nullptr;
			FMOD_RESULT const fmodResult = m_pSystem->getBusByID(&guid, &pBus);
			ASSERT_FMOD_OK;
			POOL_NEW(CAudioEnvironment, pAudioEnvironment)(nullptr, pBus);
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Warning, "Unknown Fmod bus: %s", path.c_str());
		}
	}
	else if (_stricmp(szTag, s_szFmodSnapshotTag) == 0)
	{
		stack_string path(s_szFmodSnapshotPrefix);
		path += pAudioEnvironmentNode->getAttr(s_szFmodNameAttribute);
		FMOD_GUID guid = { 0 };

		if (m_pSystem->lookupID(path.c_str(), &guid) == FMOD_OK)
		{
			FMOD::Studio::EventDescription* pEventDescription = nullptr;
			FMOD_RESULT const fmodResult = m_pSystem->getEventByID(&guid, &pEventDescription);
			ASSERT_FMOD_OK;
			POOL_NEW(CAudioEnvironment, pAudioEnvironment)(pEventDescription, nullptr);
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Warning, "Unknown Fmod snapshot: %s", path.c_str());
		}
	}

	return pAudioEnvironment;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioEnvironment(IAudioEnvironment const* const pOldAudioEnvironment)
{
	CAudioEnvironment const* const pFmodAudioEnvironment = static_cast<CAudioEnvironment const* const>(pOldAudioEnvironment);

	for (auto const pAudioObject : m_registeredAudioObjects)
	{
		pAudioObject->RemoveEnvironment(pFmodAudioEnvironment);
	}

	POOL_FREE_CONST(pOldAudioEnvironment);
}

///////////////////////////////////////////////////////////////////////////
char const* const CAudioImpl::GetImplementationNameString() const
{
#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	return m_fullImplString.c_str();
#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::GetMemoryInfo(SAudioImplMemoryInfo& memoryInfo) const
{
	memoryInfo.primaryPoolSize = g_audioImplMemoryPool.MemSize();
	memoryInfo.primaryPoolUsedSize = memoryInfo.primaryPoolSize - g_audioImplMemoryPool.MemFree();
	memoryInfo.primaryPoolAllocations = g_audioImplMemoryPool.FragmentCount();

	memoryInfo.bucketUsedSize = g_audioImplMemoryPool.GetSmallAllocsSize();
	memoryInfo.bucketAllocations = g_audioImplMemoryPool.GetSmallAllocsCount();

#if defined(PROVIDE_FMOD_IMPL_SECONDARY_POOL)
	memoryInfo.secondaryPoolSize = g_audioImplMemoryPoolSecondary.MemSize();
	memoryInfo.secondaryPoolUsedSize = memoryInfo.secondaryPoolSize - g_audioImplMemoryPoolSecondary.MemFree();
	memoryInfo.secondaryPoolAllocations = g_audioImplMemoryPoolSecondary.FragmentCount();
#else
	memoryInfo.secondaryPoolSize = 0;
	memoryInfo.secondaryPoolUsedSize = 0;
	memoryInfo.secondaryPoolAllocations = 0;
#endif // PROVIDE_AUDIO_IMPL_SECONDARY_POOL
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::OnAudioSystemRefresh()
{
	UnloadMasterBanks();
	LoadMasterBanks();
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::SetLanguage(char const* const szLanguage)
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
void CAudioImpl::CreateVersionString(CryFixedStringT<MAX_AUDIO_MISC_STRING_LENGTH>& stringOut) const
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

struct SFileData
{
	void*        pData;
	int unsigned fileSize;
};

//////////////////////////////////////////////////////////////////////////
FMOD_RESULT F_CALLBACK FmodFileOpenCallback(const char* szName, unsigned int* pFileSize, void** pHandle, void* pUserData)
{
	SFileData* const pFileData = static_cast<SFileData*>(pUserData);
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
bool CAudioImpl::LoadMasterBanks()
{
	FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;
	CryFixedStringT<MAX_AUDIO_FILE_NAME_LENGTH> masterBankPath;
	CryFixedStringT<MAX_AUDIO_FILE_NAME_LENGTH> masterBankStringsPath;
	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH + MAX_AUDIO_FILE_NAME_LENGTH> search(m_regularSoundBankFolder + CRY_NATIVE_PATH_SEPSTR "*.bank");
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

			SFileData fileData;
			fileData.pData = static_cast<void*>(pMasterBank);
			fileData.fileSize = static_cast<int>(masterBankFileSize);

			FMOD_STUDIO_BANK_INFO bankInfo;
			ZeroStruct(bankInfo);
			bankInfo.closeCallback = &FmodFileCloseCallback;
			bankInfo.openCallback = &FmodFileOpenCallback;
			bankInfo.readCallback = &FmodFileReadCallback;
			bankInfo.seekCallback = &FmodFileSeekCallback;
			bankInfo.size = sizeof(bankInfo);
			bankInfo.userData = static_cast<void*>(&fileData);
			bankInfo.userDataLength = sizeof(SFileData);

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
					FMOD::Studio::Bus** pBuses = nullptr;
					POOL_NEW_CUSTOM(FMOD::Studio::Bus*, pBuses, numBuses);
					int numRetrievedBuses = 0;
					fmodResult = m_pMasterBank->getBusList(pBuses, numBuses, &numRetrievedBuses);
					ASSERT_FMOD_OK;
					CRY_ASSERT(numBuses == numRetrievedBuses);

					for (int i = 0; i < numRetrievedBuses; ++i)
					{
						fmodResult = pBuses[i]->lockChannelGroup();
						ASSERT_FMOD_OK;
					}

					POOL_FREE(pBuses);
				}
			}
		}
	}
	else
	{
		// This does not qualify for a fallback to the NULL implementation!
		// Still notify the user about this failure!
		g_audioImplLogger.Log(eAudioLogType_Error, "Fmod failed to load master banks");
		return true;
	}

	return (fmodResult == FMOD_OK);
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::UnloadMasterBanks()
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
EAudioRequestStatus CAudioImpl::MuteMasterBus(bool const bMute)
{
	FMOD::Studio::Bus* pMasterBus = nullptr;
	FMOD_RESULT fmodResult = m_pSystem->getBus(s_szFmodBusPrefix, &pMasterBus);
	ASSERT_FMOD_OK;

	if (pMasterBus != nullptr)
	{
		fmodResult = pMasterBus->setMute(bMute);
		ASSERT_FMOD_OK;
	}

	return (fmodResult == FMOD_OK) ? eAudioRequestStatus_Success : eAudioRequestStatus_Failure;
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::GetAudioFileData(char const* const szFilename, SAudioFileData& audioFileData) const
{
	FMOD::Sound* pSound = nullptr;
	FMOD_CREATESOUNDEXINFO info;
	ZeroStruct(info);
	info.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
	info.decodebuffersize = 1;
	FMOD_RESULT const fmodResult = m_pLowLevelSystem->createStream(szFilename, FMOD_OPENONLY, &info, &pSound);
	ASSERT_FMOD_OK;

	if (pSound != nullptr)
	{
		unsigned int length = 0;
		pSound->getLength(&length, FMOD_TIMEUNIT_MS);
		audioFileData.duration = length / 1000.0f; // convert to seconds
	}
}

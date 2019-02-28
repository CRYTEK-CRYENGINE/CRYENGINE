// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioSystem.h"
#include "AudioCVars.h"
#include "ATLAudioObject.h"
#include "PropagationProcessor.h"
#include <CrySystem/ITimer.h>
#include <CryString/CryPath.h>
#include <CryEntitySystem/IEntitySystem.h>

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	#include <CryRenderer/IRenderer.h>
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

namespace CryAudio
{
enum class ELoggingOptions : EnumFlagsType
{
	None,
	Errors   = BIT(6), // a
	Warnings = BIT(7), // b
	Comments = BIT(8), // c
};
CRY_CREATE_ENUM_FLAG_OPERATORS(ELoggingOptions);

///////////////////////////////////////////////////////////////////////////
void CMainThread::Init(CSystem* const pSystem)
{
	m_pSystem = pSystem;
}

//////////////////////////////////////////////////////////////////////////
void CMainThread::ThreadEntry()
{
	while (!m_bQuit)
	{
		m_pSystem->InternalUpdate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMainThread::SignalStopWork()
{
	m_bQuit = true;
}

///////////////////////////////////////////////////////////////////////////
void CMainThread::Activate()
{
	if (!gEnv->pThreadManager->SpawnThread(this, "AudioMainThread"))
	{
		CryFatalError(R"(Error spawning "AudioMainThread" thread.)");
	}
}

///////////////////////////////////////////////////////////////////////////
void CMainThread::Deactivate()
{
	SignalStopWork();
	gEnv->pThreadManager->JoinThread(this, eJM_Join);
}

///////////////////////////////////////////////////////////////////////////
bool CMainThread::IsActive()
{
	// JoinThread returns true if thread is not running.
	// JoinThread returns false if thread is still running
	return !gEnv->pThreadManager->JoinThread(this, eJM_TryJoin);
}

//////////////////////////////////////////////////////////////////////////
CSystem::CSystem()
	: m_isInitialized(false)
	, m_didThreadWait(false)
	, m_accumulatedFrameTime(0.0f)
	, m_externalThreadFrameId(0)
	, m_lastExternalThreadFrameId(0)
	, m_atl()
{
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CryAudio::CSystem");

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	m_currentRenderAuxGeom.exchange(nullptr);
	m_lastRenderAuxGeom.exchange(nullptr);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

}

//////////////////////////////////////////////////////////////////////////
CSystem::~CSystem()
{
	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::AddRequestListener(void (* func)(SRequestInfo const* const), void* const pObjectToListenTo, ESystemEvents const eventMask)
{
	SAudioManagerRequestData<EAudioManagerRequestType::AddRequestListener> requestData(pObjectToListenTo, func, eventMask);
	CAudioRequest request(&requestData);
	request.flags = ERequestFlags::ExecuteBlocking;
	request.pOwner = pObjectToListenTo; // This makes sure that the listener is notified.
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::RemoveRequestListener(void (* func)(SRequestInfo const* const), void* const pObjectToListenTo)
{
	SAudioManagerRequestData<EAudioManagerRequestType::RemoveRequestListener> requestData(pObjectToListenTo, func);
	CAudioRequest request(&requestData);
	request.flags = ERequestFlags::ExecuteBlocking;
	request.pOwner = pObjectToListenTo; // This makes sure that the listener is notified.
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ExternalUpdate()
{
	CRY_PROFILE_REGION(PROFILE_AUDIO, "Audio: External Update");

	CRY_ASSERT(gEnv->mMainThreadId == CryGetCurrentThreadId());

	CAudioRequest request;
	while (m_syncCallbacks.dequeue(request))
	{
		m_atl.NotifyListener(request);
		if (request.pObject == nullptr)
		{
			m_atl.DecrementGlobalObjectSyncCallbackCounter();
		}
		else
		{
			request.pObject->DecrementSyncCallbackCounter();
		}
	}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	DrawAudioDebugData();
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	m_accumulatedFrameTime += gEnv->pTimer->GetFrameTime();
	++m_externalThreadFrameId;

	// If sleeping, wake up the audio thread to start processing requests again.
	m_audioThreadWakeupEvent.Set();
}

//////////////////////////////////////////////////////////////////////////
void CSystem::PushRequest(CAudioRequest const& request)
{
	CRY_PROFILE_FUNCTION(PROFILE_AUDIO);

	if (m_atl.CanProcessRequests())
	{
		m_requestQueue.enqueue(request);

		if ((request.flags & ERequestFlags::ExecuteBlocking) != 0)
		{
			// If sleeping, wake up the audio thread to start processing requests again.
			m_audioThreadWakeupEvent.Set();

			m_mainEvent.Wait();
			m_mainEvent.Reset();

			if ((request.flags & ERequestFlags::CallbackOnExternalOrCallingThread) != 0)
			{
				m_atl.NotifyListener(m_syncRequest);
			}
		}
	}
	else
	{
		Log(ELogType::Warning, "Discarded PushRequest due to ATL not allowing for new ones!");
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_LOAD_START:
		{
			string const levelNameOnly = PathUtil::GetFileName(reinterpret_cast<const char*>(wparam));

			if (!levelNameOnly.empty() && levelNameOnly.compareNoCase("Untitled") != 0)
			{
				OnLoadLevel(levelNameOnly.c_str());
			}

			break;
		}
	case ESYSTEM_EVENT_LEVEL_UNLOAD:
		{
			// This event is issued in Editor and Game mode.
			CPropagationProcessor::s_bCanIssueRWIs = false;

			SAudioManagerRequestData<EAudioManagerRequestType::ReleasePendingRays> requestData;
			CAudioRequest request(&requestData);
			request.flags = ERequestFlags::ExecuteBlocking;
			PushRequest(request);

			break;
		}
	case ESYSTEM_EVENT_LEVEL_LOAD_END:
		{
			// This event is issued in Editor and Game mode.
			CPropagationProcessor::s_bCanIssueRWIs = true;

			break;
		}
	case ESYSTEM_EVENT_CRYSYSTEM_INIT_DONE:
		{
			if (gEnv->pInput != nullptr)
			{
				gEnv->pInput->AddConsoleEventListener(&m_atl);
			}

			break;
		}
	case ESYSTEM_EVENT_FULL_SHUTDOWN:
	case ESYSTEM_EVENT_FAST_SHUTDOWN:
		{
			if (gEnv->pInput != nullptr)
			{
				gEnv->pInput->RemoveConsoleEventListener(&m_atl);
			}

			break;
		}
	default:
		{
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::InternalUpdate()
{
	CRY_PROFILE_REGION(PROFILE_AUDIO, "Audio: Internal Update");

	if (m_lastExternalThreadFrameId != m_externalThreadFrameId)
	{
		m_atl.Update(m_accumulatedFrameTime);
		ProcessRequests(m_requestQueue);
		m_lastExternalThreadFrameId = m_externalThreadFrameId;
		m_accumulatedFrameTime = 0.0f;
		m_didThreadWait = false;
	}
	else if (m_didThreadWait)
	{
		// Effectively no time has passed for the external thread as it didn't progress.
		// Consequently 0.0f is passed for deltaTime.
		m_atl.Update(0.0f);
		ProcessRequests(m_requestQueue);
		m_didThreadWait = false;
	}
	else
	{
		// If we're faster than the external thread let's wait to make room for other threads.
		CRY_PROFILE_REGION_WAITING(PROFILE_AUDIO, "Wait - Audio Update");

		// The external thread will wake the audio thread up effectively syncing it to itself.
		// If not however, the audio thread will execute at a minimum of roughly 30 fps.
		if (m_audioThreadWakeupEvent.Wait(30))
		{
			// Only reset if the event was signaled, not timed-out!
			m_audioThreadWakeupEvent.Reset();
		}

		m_didThreadWait = true;
	}
}

///////////////////////////////////////////////////////////////////////////
bool CSystem::Initialize()
{
	if (!m_isInitialized)
	{
#if defined(ENABLE_AUDIO_LOGGING)
		REGISTER_CVAR2("s_LoggingOptions", &m_loggingOptions, AlphaBits64("abc"), VF_CHEAT | VF_CHEAT_NOCHECK | VF_BITFIELD,
		               "Toggles the logging of audio related messages.\n"
		               "Usage: s_LoggingOptions [0ab...] (flags can be combined)\n"
		               "Default: abc\n"
		               "0: Logging disabled\n"
		               "a: Errors\n"
		               "b: Warnings\n"
		               "c: Comments\n");
#endif // ENABLE_AUDIO_LOGGING

		g_cvars.RegisterVariables();
		m_atl.Initialize(this);
		CRY_ASSERT_MESSAGE(!m_mainThread.IsActive(), "AudioSystem thread active before initialization!");
		m_mainThread.Init(this);
		m_mainThread.Activate();
		AddRequestListener(&CSystem::OnCallback, nullptr, ESystemEvents::TriggerExecuted | ESystemEvents::TriggerFinished);
		m_isInitialized = true;
	}
	else
	{
		CRY_ASSERT_MESSAGE(false, "AudioSystem has already been initialized!");
	}

	return m_isInitialized;
}

///////////////////////////////////////////////////////////////////////////
void CSystem::Release()
{
	if (m_isInitialized)
	{
		RemoveRequestListener(&CSystem::OnCallback, nullptr);

		SAudioManagerRequestData<EAudioManagerRequestType::ReleaseAudioImpl> releaseImplData;
		CAudioRequest releaseImplRequest(&releaseImplData);
		releaseImplRequest.flags = ERequestFlags::ExecuteBlocking;
		PushRequest(releaseImplRequest);

		m_mainThread.Deactivate();
		bool const bSuccess = m_atl.ShutDown();
		g_cvars.UnregisterVariables();

#if defined(ENABLE_AUDIO_LOGGING)
		IConsole* const pIConsole = gEnv->pConsole;

		if (pIConsole != nullptr)
		{
			pIConsole->UnregisterVariable("s_LoggingOptions");
		}
#endif // ENABLE_AUDIO_LOGGING

		m_isInitialized = false;
		delete this;
	}
	else
	{
		CRY_ASSERT_MESSAGE(false, "AudioSystem has already been released or was never properly initialized!");
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetImpl(Impl::IImpl* const pIImpl, SRequestUserData const& userData /*= SAudioRequestUserData::GetEmptyObject()*/)
{
	SAudioManagerRequestData<EAudioManagerRequestType::SetAudioImpl> requestData(pIImpl);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::LoadTrigger(ControlId const triggerId, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::LoadTrigger> requestData(triggerId);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::UnloadTrigger(ControlId const triggerId, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::UnloadTrigger> requestData(triggerId);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ExecuteTriggerEx(SExecuteTriggerData const& triggerData, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::ExecuteTriggerEx> requestData(triggerData);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ExecuteTrigger(ControlId const triggerId, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::ExecuteTrigger> requestData(triggerId);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::StopTrigger(ControlId const triggerId /* = CryAudio::InvalidControlId */, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	if (triggerId != InvalidControlId)
	{
		SAudioObjectRequestData<EAudioObjectRequestType::StopTrigger> requestData(triggerId);
		CAudioRequest request(&requestData);
		request.flags = userData.flags;
		request.pOwner = userData.pOwner;
		request.pUserData = userData.pUserData;
		request.pUserDataOwner = userData.pUserDataOwner;
		PushRequest(request);
	}
	else
	{
		SAudioObjectRequestData<EAudioObjectRequestType::StopAllTriggers> requestData;
		CAudioRequest request(&requestData);
		request.flags = userData.flags;
		request.pOwner = userData.pOwner;
		request.pUserData = userData.pUserData;
		request.pUserDataOwner = userData.pUserDataOwner;
		PushRequest(request);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::SetParameter> requestData(parameterId, value);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetSwitchState(ControlId const switchId, SwitchStateId const switchStateId, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SAudioObjectRequestData<EAudioObjectRequestType::SetSwitchState> requestData(switchId, switchStateId);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::PlayFile(SPlayFileInfo const& playFileInfo, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::PlayFile> requestData(playFileInfo.szFile, playFileInfo.usedTriggerForPlayback, playFileInfo.bLocalized);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = (userData.pOwner != nullptr) ? userData.pOwner : this;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::StopFile(char const* const szName, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SAudioObjectRequestData<EAudioObjectRequestType::StopFile> requestData(szName);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ReportStartedFile(
  CATLStandaloneFile& standaloneFile,
  bool const bSuccessfullyStarted,
  SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStartedFile> requestData(standaloneFile, bSuccessfullyStarted);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ReportStoppedFile(CATLStandaloneFile& standaloneFile, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStoppedFile> requestData(standaloneFile);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ReportFinishedEvent(
  CATLEvent& event,
  bool const bSuccess,
  SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportFinishedEvent> requestData(event, bSuccess);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::StopAllSounds(SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioManagerRequestData<EAudioManagerRequestType::StopAllSounds> requestData;
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::Refresh(char const* const szLevelName, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioManagerRequestData<EAudioManagerRequestType::RefreshAudioSystem> requestData(szLevelName);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ParseControlsData(
  char const* const szFolderPath,
  EDataScope const dataScope,
  SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioManagerRequestData<EAudioManagerRequestType::ParseControlsData> requestData(szFolderPath, dataScope);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ParsePreloadsData(
  char const* const szFolderPath,
  EDataScope const dataScope,
  SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioManagerRequestData<EAudioManagerRequestType::ParsePreloadsData> requestData(szFolderPath, dataScope);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::PreloadSingleRequest(PreloadRequestId const id, bool const bAutoLoadOnly, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SAudioManagerRequestData<EAudioManagerRequestType::PreloadSingleRequest> requestData(id, bAutoLoadOnly);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::UnloadSingleRequest(PreloadRequestId const id, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	SAudioManagerRequestData<EAudioManagerRequestType::UnloadSingleRequest> requestData(id);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::RetriggerAudioControls(SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioManagerRequestData<EAudioManagerRequestType::RetriggerAudioControls> requestData;
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ReloadControlsData(
  char const* const szFolderPath,
  char const* const szLevelName,
  SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioManagerRequestData<EAudioManagerRequestType::ReloadControlsData> requestData(szFolderPath, szLevelName);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

///////////////////////////////////////////////////////////////////////////
char const* CSystem::GetConfigPath() const
{
	return m_atl.GetConfigPath();
}

///////////////////////////////////////////////////////////////////////////
CryAudio::IListener* CSystem::CreateListener(char const* const szName /*= nullptr*/)
{
	CATLListener* pListener = nullptr;
	SAudioListenerRequestData<EAudioListenerRequestType::RegisterListener> requestData(&pListener, szName);
	CAudioRequest request(&requestData);
	request.flags = ERequestFlags::ExecuteBlocking;
	PushRequest(request);

	return static_cast<CryAudio::IListener*>(pListener);
}

///////////////////////////////////////////////////////////////////////////
void CSystem::ReleaseListener(CryAudio::IListener* const pIListener)
{
	SAudioListenerRequestData<EAudioListenerRequestType::ReleaseListener> requestData(static_cast<CATLListener*>(pIListener));
	CAudioRequest request(&requestData);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
CryAudio::IObject* CSystem::CreateObject(SCreateObjectData const& objectData /*= SCreateObjectData::GetEmptyObject()*/, SRequestUserData const& userData /*= SRequestUserData::GetEmptyObject()*/)
{
	CATLAudioObject* const pObject = new CATLAudioObject;
	SAudioObjectRequestData<EAudioObjectRequestType::RegisterObject> requestData(objectData);
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pObject = pObject;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
	return static_cast<CryAudio::IObject*>(pObject);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ReleaseObject(CryAudio::IObject* const pIObject)
{
	SAudioObjectRequestData<EAudioObjectRequestType::ReleaseObject> requestData;
	CAudioRequest request(&requestData);
	request.pObject = static_cast<CATLAudioObject*>(pIObject);
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::GetFileData(char const* const szName, SFileData& fileData)
{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	SAudioManagerRequestData<EAudioManagerRequestType::GetAudioFileData> requestData(szName, fileData);
	CAudioRequest request(&requestData);
	request.flags = ERequestFlags::ExecuteBlocking;
	PushRequest(request);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSystem::GetTriggerData(ControlId const triggerId, STriggerData& triggerData)
{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	m_atl.GetAudioTriggerData(triggerId, triggerData);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OnLoadLevel(char const* const szLevelName)
{
	// Requests need to be blocking so data is available for next preloading request!
	CryFixedStringT<MaxFilePathLength> audioLevelPath(m_atl.GetConfigPath());
	audioLevelPath += s_szLevelsFolderName;
	audioLevelPath += "/";
	audioLevelPath += szLevelName;
	SAudioManagerRequestData<EAudioManagerRequestType::ParseControlsData> requestData1(audioLevelPath, EDataScope::LevelSpecific);
	CAudioRequest request1(&requestData1);
	request1.flags = ERequestFlags::ExecuteBlocking;
	PushRequest(request1);

	SAudioManagerRequestData<EAudioManagerRequestType::ParsePreloadsData> requestData2(audioLevelPath, EDataScope::LevelSpecific);
	CAudioRequest request2(&requestData2);
	request2.flags = ERequestFlags::ExecuteBlocking;
	PushRequest(request2);

	PreloadRequestId const preloadRequestId = CryAudio::StringToId(szLevelName);
	SAudioManagerRequestData<EAudioManagerRequestType::PreloadSingleRequest> requestData3(preloadRequestId, true);
	CAudioRequest request3(&requestData3);
	request3.flags = ERequestFlags::ExecuteBlocking;
	PushRequest(request3);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OnUnloadLevel()
{
	SAudioManagerRequestData<EAudioManagerRequestType::UnloadAFCMDataByScope> requestData1(EDataScope::LevelSpecific);
	CAudioRequest request1(&requestData1);
	request1.flags = ERequestFlags::ExecuteBlocking;
	PushRequest(request1);

	SAudioManagerRequestData<EAudioManagerRequestType::ClearControlsData> requestData2(EDataScope::LevelSpecific);
	CAudioRequest request2(&requestData2);
	request2.flags = ERequestFlags::ExecuteBlocking;
	PushRequest(request2);

	SAudioManagerRequestData<EAudioManagerRequestType::ClearPreloadsData> requestData3(EDataScope::LevelSpecific);
	CAudioRequest request3(&requestData3);
	request3.flags = ERequestFlags::ExecuteBlocking;
	PushRequest(request3);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OnLanguageChanged()
{
	SAudioManagerRequestData<EAudioManagerRequestType::ChangeLanguage> requestData;
	CAudioRequest request(&requestData);
	request.flags = ERequestFlags::ExecuteBlocking;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::Log(ELogType const type, char const* const szFormat, ...)
{
#if defined(ENABLE_AUDIO_LOGGING)
	if (szFormat != nullptr && szFormat[0] != '\0' && gEnv->pLog->GetVerbosityLevel() != -1)
	{
		CRY_PROFILE_REGION(PROFILE_AUDIO, "CSystem::Log");

		char buffer[256];
		va_list ArgList;
		va_start(ArgList, szFormat);
		cry_vsprintf(buffer, szFormat, ArgList);
		va_end(ArgList);

		ELoggingOptions const loggingOptions = static_cast<ELoggingOptions>(m_loggingOptions);

		switch (type)
		{
		case ELogType::Warning:
			{
				if ((loggingOptions& ELoggingOptions::Warnings) != 0)
				{
					gEnv->pSystem->Warning(VALIDATOR_MODULE_AUDIO, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, nullptr, "<Audio>: %s", buffer);
				}

				break;
			}
		case ELogType::Error:
			{
				if ((loggingOptions& ELoggingOptions::Errors) != 0)
				{
					gEnv->pSystem->Warning(VALIDATOR_MODULE_AUDIO, VALIDATOR_ERROR, VALIDATOR_FLAG_AUDIO, nullptr, "<Audio>: %s", buffer);
				}

				break;
			}
		case ELogType::Comment:
			{
				if ((gEnv->pLog != nullptr) && (gEnv->pLog->GetVerbosityLevel() >= 4) && ((loggingOptions& ELoggingOptions::Comments) != 0))
				{
					CryLogAlways("<Audio>: %s", buffer);
				}

				break;
			}
		case ELogType::Always:
			{
				CryLogAlways("<Audio>: %s", buffer);

				break;
			}
		default:
			{
				CRY_ASSERT(false);

				break;
			}
		}
	}
#endif // ENABLE_AUDIO_LOGGING
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ProcessRequests(AudioRequests& requestQueue)
{
	while (requestQueue.dequeue(m_request))
	{
		if (m_request.status == ERequestStatus::None)
		{
			m_request.status = ERequestStatus::Pending;
			m_atl.ProcessRequest(m_request);
		}
		else
		{
			// TODO: handle pending requests!
		}

		if (m_request.status != ERequestStatus::Pending)
		{
			if ((m_request.flags & ERequestFlags::CallbackOnAudioThread) != 0)
			{
				m_atl.NotifyListener(m_request);

				if ((m_request.flags & ERequestFlags::ExecuteBlocking) != 0)
				{
					m_mainEvent.Set();
				}
			}
			else if ((m_request.flags & ERequestFlags::CallbackOnExternalOrCallingThread) != 0)
			{
				if ((m_request.flags & ERequestFlags::ExecuteBlocking) != 0)
				{
					m_syncRequest = m_request;
					m_mainEvent.Set();
				}
				else
				{
					if (m_request.pObject == nullptr)
					{
						m_atl.IncrementGlobalObjectSyncCallbackCounter();
					}
					else
					{
						m_request.pObject->IncrementSyncCallbackCounter();
					}
					m_syncCallbacks.enqueue(m_request);
				}
			}
			else if ((m_request.flags & ERequestFlags::ExecuteBlocking) != 0)
			{
				m_mainEvent.Set();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OnCallback(SRequestInfo const* const pRequestInfo)
{
	if (gEnv->mMainThreadId == CryGetCurrentThreadId() && pRequestInfo->pAudioObject != nullptr)
	{
		IEntity* const pIEntity = gEnv->pEntitySystem->GetEntity(pRequestInfo->pAudioObject->GetEntityId());

		if (pIEntity != nullptr)
		{
			SEntityEvent eventData;  //converting audio events to entityEvents
			eventData.nParam[0] = reinterpret_cast<intptr_t>(pRequestInfo);

			if (pRequestInfo->systemEvent == CryAudio::ESystemEvents::TriggerExecuted)
			{
				eventData.event = ENTITY_EVENT_AUDIO_TRIGGER_STARTED;
				pIEntity->SendEvent(eventData);
			}

			//if the trigger failed to start or has finished, we (also) send ENTITY_EVENT_AUDIO_TRIGGER_ENDED
			if (pRequestInfo->systemEvent == CryAudio::ESystemEvents::TriggerFinished
			    || (pRequestInfo->systemEvent == CryAudio::ESystemEvents::TriggerExecuted && pRequestInfo->requestResult != CryAudio::ERequestResult::Success))
			{
				eventData.event = ENTITY_EVENT_AUDIO_TRIGGER_ENDED;
				pIEntity->SendEvent(eventData);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::GetImplInfo(SImplInfo& implInfo)
{
	SAudioManagerRequestData<EAudioManagerRequestType::GetImplInfo> requestData(implInfo);
	CAudioRequest request(&requestData);
	request.flags = ERequestFlags::ExecuteBlocking;
	PushRequest(request);
}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
void CSystem::ScheduleIRenderAuxGeomForRendering(IRenderAuxGeom* pRenderAuxGeom)
{
	auto oldRenderAuxGeom = m_currentRenderAuxGeom.exchange(pRenderAuxGeom);
	CRY_ASSERT(oldRenderAuxGeom != pRenderAuxGeom);

	// Kill FIFO entries beyond 1, only the head survives in m_currentRenderAuxGeom
	// Throw away all older entries
	if (oldRenderAuxGeom && oldRenderAuxGeom != pRenderAuxGeom)
	{
		gEnv->pRenderer->DeleteAuxGeom(oldRenderAuxGeom);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SubmitLastIRenderAuxGeomForRendering()
{
	// Consume the FIFO head
	auto curRenderAuxGeom = m_currentRenderAuxGeom.exchange(nullptr);
	if (curRenderAuxGeom)
	{
		// Replace the active Aux rendering by a new one only if there is a new one
		// Otherwise keep rendering the currently active one
		auto oldRenderAuxGeom = m_lastRenderAuxGeom.exchange(curRenderAuxGeom);
		if (oldRenderAuxGeom)
		{
			gEnv->pRenderer->DeleteAuxGeom(oldRenderAuxGeom);
		}
	}

	if (m_lastRenderAuxGeom != (volatile IRenderAuxGeom*)nullptr)
	{
		gEnv->pRenderer->SubmitAuxGeom(m_lastRenderAuxGeom);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::DrawAudioDebugData()
{
	if (g_cvars.m_drawAudioDebug > 0)
	{
		SubmitLastIRenderAuxGeomForRendering();

		SAudioManagerRequestData<EAudioManagerRequestType::DrawDebugInfo> requestData;
		CAudioRequest request(&requestData);
		PushRequest(request);
	}
}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}      // namespace CryAudio

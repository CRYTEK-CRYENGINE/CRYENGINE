// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioSystem.h"
#include "AudioCVars.h"
#include "ATLAudioObject.h"
#include "PropagationProcessor.h"
#include "ProfileData.h"
#include <CrySystem/ITimer.h>
#include <CryString/CryPath.h>

#include <CryEntitySystem/IEntitySystem.h>

namespace CryAudio
{
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
	if (!gEnv->pThreadManager->SpawnThread(this, "MainAudioThread"))
	{
		CryFatalError(R"(Error spawning "MainAudioThread" thread.)");
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
	: m_bSystemInitialized(false)
	, m_lastUpdateTime()
	, m_deltaTime(0.0f)
	, m_atl()
{
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CryAudio::CSystem");
	m_mainThread.Init(this);
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
}

//////////////////////////////////////////////////////////////////////////
void CSystem::PushRequest(CAudioRequest const& request)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_AUDIO);

	if (m_atl.CanProcessRequests())
	{
		m_requestQueue.enqueue(request);

		if ((request.flags & ERequestFlags::ExecuteBlocking) > 0)
		{
			// If sleeping, wake up the audio thread to start processing requests again
			m_audioThreadWakeupEvent.Set();

			m_mainEvent.Wait();
			m_mainEvent.Reset();

			if ((request.flags & ERequestFlags::CallbackOnExternalOrCallingThread) > 0)
			{
				m_atl.NotifyListener(m_syncRequest);
			}
		}
	}
	else
	{
		g_logger.Log(ELogType::Warning, "Discarded PushRequest due to ATL not allowing for new ones!");
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::UpdateTime()
{
	CTimeValue const currentAsyncTime(gEnv->pTimer->GetAsyncTime());
	m_deltaTime += (currentAsyncTime - m_lastUpdateTime).GetMilliSeconds();
	m_lastUpdateTime = currentAsyncTime;
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
	bool bWorkDone = false;

	{
		CRY_PROFILE_REGION(PROFILE_AUDIO, "Audio: Internal Update");

		UpdateTime();
		m_atl.Update(m_deltaTime);
		m_deltaTime = 0.0f;

		bWorkDone = ProcessRequests(m_requestQueue);
	}

	if (bWorkDone == false)
	{
		CRY_PROFILE_REGION_WAITING(PROFILE_AUDIO, "Wait - Audio Update");

		if (m_audioThreadWakeupEvent.Wait(10))
		{
			// Only reset if the event was signalled, not timed-out!
			m_audioThreadWakeupEvent.Reset();
		}
	}
}

///////////////////////////////////////////////////////////////////////////
bool CSystem::Initialize()
{
	if (!m_bSystemInitialized)
	{
		if (m_mainThread.IsActive())
		{
			m_mainThread.Deactivate();
		}

		m_configPath = CryFixedStringT<MaxFilePathLength>((PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "ace" CRY_NATIVE_PATH_SEPSTR).c_str());
		m_atl.Initialize(this);
		m_mainThread.Activate();
		m_bSystemInitialized = true;
	}

	AddRequestListener(&CSystem::OnCallback, nullptr, ESystemEvents::TriggerExecuted | ESystemEvents::TriggerFinished);

	return m_bSystemInitialized;
}

///////////////////////////////////////////////////////////////////////////
void CSystem::Release()
{
	RemoveRequestListener(&CSystem::OnCallback, nullptr);

	SAudioManagerRequestData<EAudioManagerRequestType::ReleaseAudioImpl> releaseImplData;
	CAudioRequest releaseImplRequest(&releaseImplData);
	releaseImplRequest.flags = ERequestFlags::ExecuteBlocking;
	PushRequest(releaseImplRequest);

	m_mainThread.Deactivate();
	bool const bSuccess = m_atl.ShutDown();
	m_bSystemInitialized = false;

	delete this;

	g_cvars.UnregisterVariables();

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
void CSystem::LostFocus(SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioManagerRequestData<EAudioManagerRequestType::LoseFocus> requestData;
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::GotFocus(SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioManagerRequestData<EAudioManagerRequestType::GetFocus> requestData;
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::MuteAll(SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioManagerRequestData<EAudioManagerRequestType::MuteAll> requestData;
	CAudioRequest request(&requestData);
	request.flags = userData.flags;
	request.pOwner = userData.pOwner;
	request.pUserData = userData.pUserData;
	request.pUserDataOwner = userData.pUserDataOwner;
	PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::UnmuteAll(SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioManagerRequestData<EAudioManagerRequestType::UnmuteAll> requestData;
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
	return m_configPath.c_str();
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
	CryFixedStringT<MaxFilePathLength> audioLevelPath(m_configPath.c_str());
	audioLevelPath += "levels" CRY_NATIVE_PATH_SEPSTR;
	audioLevelPath += szLevelName;
	SAudioManagerRequestData<EAudioManagerRequestType::ParseControlsData> requestData1(audioLevelPath, EDataScope::LevelSpecific);
	CAudioRequest request1(&requestData1);
	request1.flags = ERequestFlags::ExecuteBlocking;
	PushRequest(request1);

	SAudioManagerRequestData<EAudioManagerRequestType::ParsePreloadsData> requestData2(audioLevelPath, EDataScope::LevelSpecific);
	CAudioRequest request2(&requestData2);
	request2.flags = ERequestFlags::ExecuteBlocking;
	PushRequest(request2);

	PreloadRequestId const preloadRequestId = CryAudio::StringToId_RunTime(szLevelName);
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
IProfileData* CSystem::GetProfileData() const
{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	return static_cast<IProfileData*>(m_atl.GetProfileData());
#else
	return nullptr;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::ProcessRequests(AudioRequests& requestQueue)
{
	bool bSuccess = false;
	CAudioRequest request;

	while (requestQueue.dequeue(request))
	{
		if (request.status == ERequestStatus::None)
		{
			request.status = ERequestStatus::Pending;
			m_atl.ProcessRequest(request);
		}
		else
		{
			// TODO: handle pending requests!
		}

		if (request.status != ERequestStatus::Pending)
		{
			if ((request.flags & ERequestFlags::CallbackOnAudioThread) > 0)
			{
				m_atl.NotifyListener(request);

				if ((request.flags & ERequestFlags::ExecuteBlocking) > 0)
				{
					m_mainEvent.Set();
				}
			}
			else if ((request.flags & ERequestFlags::CallbackOnExternalOrCallingThread) > 0)
			{
				if ((request.flags & ERequestFlags::ExecuteBlocking) > 0)
				{
					m_syncRequest = request;
					m_mainEvent.Set();
				}
				else
				{
					if (request.pObject == nullptr)
					{
						m_atl.IncrementGlobalObjectSyncCallbackCounter();
					}
					else
					{
						request.pObject->IncrementSyncCallbackCounter();
					}
					m_syncCallbacks.enqueue(request);
				}
			}
			else if ((request.flags & ERequestFlags::ExecuteBlocking) > 0)
			{
				m_mainEvent.Set();
			}
		}

		bSuccess = true;
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OnCallback(SRequestInfo const* const pRequestInfo)
{
	if (gEnv->mMainThreadId == CryGetCurrentThreadId() && pRequestInfo->pAudioObject != nullptr)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pRequestInfo->pAudioObject->GetEntityId());
		if (pEntity)
		{
			SEntityEvent eventData;  //converting audio events to entityEvents
			eventData.nParam[0] = reinterpret_cast<intptr_t>(pRequestInfo);
			if (pRequestInfo->systemEvent == CryAudio::ESystemEvents::TriggerExecuted)
			{
				eventData.event = ENTITY_EVENT_AUDIO_TRIGGER_STARTED;
				pEntity->SendEvent(eventData);
			}

			//if the trigger failed to start or has finished, we (also) send ENTITY_EVENT_AUDIO_TRIGGER_ENDED
			if (pRequestInfo->systemEvent == CryAudio::ESystemEvents::TriggerFinished
			    || (pRequestInfo->systemEvent == CryAudio::ESystemEvents::TriggerExecuted && pRequestInfo->requestResult != CryAudio::ERequestResult::Success))
			{
				eventData.event = ENTITY_EVENT_AUDIO_TRIGGER_ENDED;
				pEntity->SendEvent(eventData);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
void CSystem::DrawAudioDebugData()
{
	if (g_cvars.m_drawAudioDebug > 0)
	{
		SAudioManagerRequestData<EAudioManagerRequestType::DrawDebugInfo> requestData;
		CAudioRequest request(&requestData);
		request.flags = ERequestFlags::ExecuteBlocking;
		PushRequest(request);
	}
}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}      // namespace CryAudio

// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "stdafx.h"
#include "ATL.h"
#include "AudioImpl.h"
#include "AudioCVars.h"
#include "ATLAudioObject.h"
#include "PoolObject_impl.h"
#include <CrySystem/ISystem.h>
#include <CryPhysics/IPhysics.h>
#include <CryRenderer/IRenderer.h>
#include <CryString/CryPath.h>
#include <CryEntitySystem/IEntitySystem.h>

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	#include "ProfileData.h"
	#include <CryRenderer/IRenderAuxGeom.h>
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

namespace CryAudio
{
///////////////////////////////////////////////////////////////////////////
inline ERequestResult ConvertToRequestResult(ERequestStatus const eAudioRequestStatus)
{
	ERequestResult result;

	switch (eAudioRequestStatus)
	{
	case ERequestStatus::Success:
		{
			result = ERequestResult::Success;
			break;
		}
	case ERequestStatus::Failure:
	case ERequestStatus::FailureInvalidObjectId:
	case ERequestStatus::FailureInvalidControlId:
	case ERequestStatus::FailureInvalidRequest:
	case ERequestStatus::PartialSuccess:
		{
			result = ERequestResult::Failure;
			break;
		}
	default:
		{
			g_logger.Log(ELogType::Error, "Invalid AudioRequestStatus '%u'. Cannot be converted to an AudioRequestResult. ", eAudioRequestStatus);
			CRY_ASSERT(false);
			result = ERequestResult::Failure;
			break;
		}
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
CAudioTranslationLayer::CAudioTranslationLayer()
	: m_audioObjectMgr(m_audioEventMgr, m_audioStandaloneFileMgr, m_audioListenerMgr)
	, m_fileCacheMgr(m_preloadRequests)
	, m_xmlProcessor(m_triggers, m_parameters, m_switches, m_environments, m_preloadRequests, m_fileCacheMgr, m_internalControls)
{
	if (g_cvars.m_audioObjectPoolSize < 1)
	{
		g_cvars.m_audioObjectPoolSize = 1;
		g_logger.Log(ELogType::Warning, R"(Audio Object pool size should be greater than zero. Forcing the cvar "s_AudioObjectPoolSize" to 1!)");
	}
	CATLAudioObject::CreateAllocator(g_cvars.m_audioObjectPoolSize);

	if (g_cvars.m_audioEventPoolSize < 1)
	{
		g_cvars.m_audioEventPoolSize = 1;
		g_logger.Log(ELogType::Warning, R"(Audio Event pool size should be greater than zero. Forcing the cvar "s_AudioEventPoolSize" to 1!)");
	}
	CATLEvent::CreateAllocator(g_cvars.m_audioEventPoolSize);

	if (g_cvars.m_audioStandaloneFilePoolSize < 1)
	{
		g_cvars.m_audioStandaloneFilePoolSize = 1;
		g_logger.Log(ELogType::Warning, R"(Audio Standalone File pool size should be greater than zero. Forcing the cvar "s_AudioStandaloneFilePoolSize" to 1!)");
	}
	CATLStandaloneFile::CreateAllocator(g_cvars.m_audioStandaloneFilePoolSize);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	m_pProfileData = new CProfileData;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
CAudioTranslationLayer::~CAudioTranslationLayer()
{
	if (m_pIImpl != nullptr)
	{
		// the ATL has not yet been properly shut down
		ShutDown();
	}

	CRY_ASSERT_MESSAGE(!m_pGlobalAudioObject, "Global audio object should have been destroyed in the audio thread, before the ATL is destructed.");

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	delete m_pProfileData;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

///////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::Initialize(CSystem* const pAudioSystem)
{
	// Add the callback for the obstruction calculation.
	gEnv->pPhysicalWorld->AddEventClient(
	  EventPhysRWIResult::id,
	  &CPropagationProcessor::OnObstructionTest,
	  1);

	CATLAudioObject::s_pEventManager = &m_audioEventMgr;
	CATLAudioObject::s_pAudioSystem = pAudioSystem;
	CATLAudioObject::s_pStandaloneFileManager = &m_audioStandaloneFileMgr;

	return true;
}

///////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::ShutDown()
{
	if (gEnv->pPhysicalWorld != nullptr)
	{
		// remove the callback for the obstruction calculation
		gEnv->pPhysicalWorld->RemoveEventClient(
		  EventPhysRWIResult::id,
		  &CPropagationProcessor::OnObstructionTest,
		  1);
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::ProcessRequest(CAudioRequest& request)
{
	ERequestStatus result = ERequestStatus::None;

	if (request.GetData())
	{
		switch (request.GetData()->type)
		{
		case EAudioRequestType::AudioObjectRequest:
			{
				result = ProcessAudioObjectRequest(request);

				break;
			}
		case EAudioRequestType::AudioListenerRequest:
			{
				result = ProcessAudioListenerRequest(request.GetData());
				break;
			}
		case EAudioRequestType::AudioCallbackManagerRequest:
			{
				result = ProcessAudioCallbackManagerRequest(request);

				break;
			}
		case EAudioRequestType::AudioManagerRequest:
			{
				result = ProcessAudioManagerRequest(request);

				break;
			}
		default:
			{
				g_logger.Log(ELogType::Error, "Unknown audio request type: %d", static_cast<int>(request.GetData()->type));
				CRY_ASSERT(false);

				break;
			}
		}
	}

	request.status = result;
}

///////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::Update(float const deltaTime)
{
	if (m_pIImpl != nullptr)
	{
		if (m_lastMainThreadFrameId != gEnv->nMainFrameID)
		{
			g_lastMainThreadFrameStartTime = gEnv->pTimer->GetFrameStartTime();
			m_lastMainThreadFrameId = gEnv->nMainFrameID;

			if (g_cvars.m_tickWithMainThread > 0)
			{
				m_audioListenerMgr.Update(deltaTime);
				m_audioEventMgr.Update(deltaTime);
				m_audioObjectMgr.Update(deltaTime, m_audioListenerMgr.GetActiveListenerAttributes());
				m_pGlobalAudioObject->GetImplDataPtr()->Update();
				m_fileCacheMgr.Update();

				m_pIImpl->Update(deltaTime);
			}
		}

		if (g_cvars.m_tickWithMainThread == 0)
		{
			m_audioListenerMgr.Update(deltaTime);
			m_audioEventMgr.Update(deltaTime);
			m_audioObjectMgr.Update(deltaTime, m_audioListenerMgr.GetActiveListenerAttributes());
			m_pGlobalAudioObject->GetImplDataPtr()->Update();
			m_fileCacheMgr.Update();

			m_pIImpl->Update(deltaTime);
		}
	}
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CAudioTranslationLayer::ParseControlsData(char const* const szFolderPath, EDataScope const dataScope)
{
	m_xmlProcessor.ParseControlsData(szFolderPath, dataScope);

	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CAudioTranslationLayer::ParsePreloadsData(char const* const szFolderPath, EDataScope const dataScope)
{
	m_xmlProcessor.ParsePreloadsData(szFolderPath, dataScope);

	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CAudioTranslationLayer::ClearControlsData(EDataScope const dataScope)
{
	m_xmlProcessor.ClearControlsData(dataScope);
	if (dataScope == EDataScope::All || dataScope == EDataScope::Global)
	{
		InitInternalControls();
	}

	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CAudioTranslationLayer::ClearPreloadsData(EDataScope const dataScope)
{
	m_xmlProcessor.ClearPreloadsData(dataScope);

	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::IncrementGlobalObjectSyncCallbackCounter()
{
	m_pGlobalAudioObject->IncrementSyncCallbackCounter();
}

///////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::DecrementGlobalObjectSyncCallbackCounter()
{
	m_pGlobalAudioObject->DecrementSyncCallbackCounter();
}

//////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::NotifyListener(CAudioRequest const& request)
{
	ESystemEvents audioSystemEvent = ESystemEvents::None;
	CATLStandaloneFile* pStandaloneFile = nullptr;
	ControlId audioControlID = InvalidControlId;
	CATLEvent* pAudioEvent = nullptr;
	switch (request.GetData()->type)
	{
	case EAudioRequestType::AudioManagerRequest:
		{
			SAudioManagerRequestDataBase const* const pRequestDataBase = static_cast<SAudioManagerRequestDataBase const* const>(request.GetData());

			switch (pRequestDataBase->type)
			{
			case EAudioManagerRequestType::SetAudioImpl:
				audioSystemEvent = ESystemEvents::ImplSet;
				break;
			}

			break;
		}
	case EAudioRequestType::AudioCallbackManagerRequest:
		{
			SAudioCallbackManagerRequestDataBase const* const pRequestDataBase = static_cast<SAudioCallbackManagerRequestDataBase const* const>(request.GetData());

			switch (pRequestDataBase->type)
			{
			case EAudioCallbackManagerRequestType::ReportFinishedTriggerInstance:
				{
					SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportFinishedTriggerInstance> const* const pRequestData = static_cast<SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportFinishedTriggerInstance> const* const>(pRequestDataBase);
					audioControlID = pRequestData->audioTriggerId;
					audioSystemEvent = ESystemEvents::TriggerFinished;

					break;
				}
			case EAudioCallbackManagerRequestType::ReportStartedEvent:
				{
					SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStartedEvent> const* const pRequestData = static_cast<SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStartedEvent> const* const>(pRequestDataBase);
					pAudioEvent = &pRequestData->audioEvent;

					break;
				}
			case EAudioCallbackManagerRequestType::ReportStartedFile:
				{
					SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStartedFile> const* const pRequestData = static_cast<SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStartedFile> const* const>(pRequestDataBase);
					pStandaloneFile = &pRequestData->audioStandaloneFile;
					audioSystemEvent = ESystemEvents::FileStarted;

					break;
				}
			case EAudioCallbackManagerRequestType::ReportStoppedFile:
				{
					SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStoppedFile> const* const pRequestData = static_cast<SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStoppedFile> const* const>(pRequestDataBase);
					pStandaloneFile = &pRequestData->audioStandaloneFile;
					audioSystemEvent = ESystemEvents::FileStopped;

					break;
				}
			}

			break;
		}
	case EAudioRequestType::AudioObjectRequest:
		{
			SAudioObjectRequestDataBase const* const pRequestDataBase = static_cast<SAudioObjectRequestDataBase const* const>(request.GetData());

			switch (pRequestDataBase->type)
			{
			case EAudioObjectRequestType::ExecuteTrigger:
				{
					SAudioObjectRequestData<EAudioObjectRequestType::ExecuteTrigger> const* const pRequestData = static_cast<SAudioObjectRequestData<EAudioObjectRequestType::ExecuteTrigger> const* const>(pRequestDataBase);
					audioControlID = pRequestData->audioTriggerId;
					audioSystemEvent = ESystemEvents::TriggerExecuted;

					break;
				}
			case EAudioObjectRequestType::PlayFile:
				audioSystemEvent = ESystemEvents::FilePlay;
				break;
			}

			break;
		}
	case EAudioRequestType::AudioListenerRequest:
		{
			// Nothing to do currently for this type of request.

			break;
		}
	default:
		{
			CryFatalError("Unknown request type during CAudioTranslationLayer::NotifyListener!");

			break;
		}
	}

	SRequestInfo const requestInfo(
	  ConvertToRequestResult(request.status),
	  request.pOwner,
	  request.pUserData,
	  request.pUserDataOwner,
	  audioSystemEvent,
	  audioControlID,
	  static_cast<IObject*>(request.pObject),
	  pStandaloneFile,
	  pAudioEvent);

	m_audioEventListenerMgr.NotifyListener(&requestInfo);
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CAudioTranslationLayer::ProcessAudioManagerRequest(CAudioRequest const& request)
{
	ERequestStatus result = ERequestStatus::Failure;
	SAudioManagerRequestDataBase const* const pRequestDataBase = static_cast<SAudioManagerRequestDataBase const*>(request.GetData());

	switch (pRequestDataBase->type)
	{
	case EAudioManagerRequestType::AddRequestListener:
		{
			SAudioManagerRequestData<EAudioManagerRequestType::AddRequestListener> const* const pRequestData = static_cast<SAudioManagerRequestData<EAudioManagerRequestType::AddRequestListener> const*>(request.GetData());
			result = m_audioEventListenerMgr.AddRequestListener(pRequestData);

			break;
		}
	case EAudioManagerRequestType::RemoveRequestListener:
		{
			SAudioManagerRequestData<EAudioManagerRequestType::RemoveRequestListener> const* const pRequestData = static_cast<SAudioManagerRequestData<EAudioManagerRequestType::RemoveRequestListener> const*>(request.GetData());
			result = m_audioEventListenerMgr.RemoveRequestListener(pRequestData->func, pRequestData->pObjectToListenTo);

			break;
		}
	case EAudioManagerRequestType::SetAudioImpl:
		{
			SAudioManagerRequestData<EAudioManagerRequestType::SetAudioImpl> const* const pRequestData = static_cast<SAudioManagerRequestData<EAudioManagerRequestType::SetAudioImpl> const*>(request.GetData());
			result = SetImpl(pRequestData->pIImpl);

			break;
		}
	case EAudioManagerRequestType::RefreshAudioSystem:
		{
			SAudioManagerRequestData<EAudioManagerRequestType::RefreshAudioSystem> const* const pRequestData = static_cast<SAudioManagerRequestData<EAudioManagerRequestType::RefreshAudioSystem> const*>(request.GetData());
			result = RefreshAudioSystem(pRequestData->levelName);

			break;
		}
	case EAudioManagerRequestType::LoseFocus:
		{
			if (
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			  (g_cvars.m_ignoreWindowFocus == 0) &&
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE
			  (m_flags& EInternalStates::IsMuted) == 0)
			{
				CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, LoseFocusTriggerId, nullptr);

				if (pTrigger != nullptr)
				{
					m_pGlobalAudioObject->HandleExecuteTrigger(pTrigger);
				}
				else
				{
					g_logger.Log(ELogType::Warning, "Could not find definition of lose focus trigger");
				}

				result = m_pIImpl->OnLoseFocus();
			}

			break;
		}
	case EAudioManagerRequestType::GetFocus:
		{
			if (
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			  (g_cvars.m_ignoreWindowFocus == 0) &&
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE
			  (m_flags& EInternalStates::IsMuted) == 0)
			{
				result = m_pIImpl->OnGetFocus();
				CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, GetFocusTriggerId, nullptr);

				if (pTrigger != nullptr)
				{
					m_pGlobalAudioObject->HandleExecuteTrigger(pTrigger);
				}
				else
				{
					g_logger.Log(ELogType::Warning, "Could not find definition of get focus trigger");
				}
			}

			break;
		}
	case EAudioManagerRequestType::MuteAll:
		{
			CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, MuteAllTriggerId, nullptr);

			if (pTrigger != nullptr)
			{
				m_pGlobalAudioObject->HandleExecuteTrigger(pTrigger);
			}
			else
			{
				g_logger.Log(ELogType::Warning, "Could not find definition of mute all trigger");
			}

			result = m_pIImpl->MuteAll();

			if (result == ERequestStatus::Success)
			{
				m_flags |= EInternalStates::IsMuted;
			}
			else
			{
				m_flags &= ~EInternalStates::IsMuted;
			}

			break;
		}
	case EAudioManagerRequestType::UnmuteAll:
		{
			result = m_pIImpl->UnmuteAll();

			if (result != ERequestStatus::Success)
			{
				m_flags |= EInternalStates::IsMuted;
			}
			else
			{
				m_flags &= ~EInternalStates::IsMuted;
			}

			CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, UnmuteAllTriggerId, nullptr);

			if (pTrigger != nullptr)
			{
				m_pGlobalAudioObject->HandleExecuteTrigger(pTrigger);
			}
			else
			{
				g_logger.Log(ELogType::Warning, "Could not find definition of unmute trigger");
			}

			break;
		}
	case EAudioManagerRequestType::StopAllSounds:
		{
			result = m_pIImpl->StopAllSounds();

			break;
		}
	case EAudioManagerRequestType::ParseControlsData:
		{
			SAudioManagerRequestData<EAudioManagerRequestType::ParseControlsData> const* const pRequestData = static_cast<SAudioManagerRequestData<EAudioManagerRequestType::ParseControlsData> const*>(request.GetData());
			result = ParseControlsData(pRequestData->folderPath.c_str(), pRequestData->dataScope);

			break;
		}
	case EAudioManagerRequestType::ParsePreloadsData:
		{
			SAudioManagerRequestData<EAudioManagerRequestType::ParsePreloadsData> const* const pRequestData = static_cast<SAudioManagerRequestData<EAudioManagerRequestType::ParsePreloadsData> const*>(request.GetData());
			result = ParsePreloadsData(pRequestData->folderPath.c_str(), pRequestData->dataScope);

			break;
		}
	case EAudioManagerRequestType::ClearControlsData:
		{
			SAudioManagerRequestData<EAudioManagerRequestType::ClearControlsData> const* const pRequestData = static_cast<SAudioManagerRequestData<EAudioManagerRequestType::ClearControlsData> const*>(request.GetData());
			result = ClearControlsData(pRequestData->dataScope);

			break;
		}
	case EAudioManagerRequestType::ClearPreloadsData:
		{
			SAudioManagerRequestData<EAudioManagerRequestType::ClearPreloadsData> const* const pRequestData = static_cast<SAudioManagerRequestData<EAudioManagerRequestType::ClearPreloadsData> const*>(request.GetData());
			result = ClearPreloadsData(pRequestData->dataScope);

			break;
		}
	case EAudioManagerRequestType::PreloadSingleRequest:
		{
			SAudioManagerRequestData<EAudioManagerRequestType::PreloadSingleRequest> const* const pRequestData = static_cast<SAudioManagerRequestData<EAudioManagerRequestType::PreloadSingleRequest> const*>(request.GetData());
			result = m_fileCacheMgr.TryLoadRequest(pRequestData->audioPreloadRequestId, ((request.flags & ERequestFlags::ExecuteBlocking) != 0), pRequestData->bAutoLoadOnly);

			break;
		}
	case EAudioManagerRequestType::UnloadSingleRequest:
		{
			SAudioManagerRequestData<EAudioManagerRequestType::UnloadSingleRequest> const* const pRequestData = static_cast<SAudioManagerRequestData<EAudioManagerRequestType::UnloadSingleRequest> const*>(request.GetData());
			result = m_fileCacheMgr.TryUnloadRequest(pRequestData->audioPreloadRequestId);

			break;
		}
	case EAudioManagerRequestType::UnloadAFCMDataByScope:
		{
			SAudioManagerRequestData<EAudioManagerRequestType::UnloadAFCMDataByScope> const* const pRequestData = static_cast<SAudioManagerRequestData<EAudioManagerRequestType::UnloadAFCMDataByScope> const*>(request.GetData());
			result = m_fileCacheMgr.UnloadDataByScope(pRequestData->dataScope);

			break;
		}
	case EAudioManagerRequestType::ReleaseAudioImpl:
		{
			ReleaseImpl();
			result = ERequestStatus::Success;

			break;
		}
	case EAudioManagerRequestType::ChangeLanguage:
		{
			SetImplLanguage();

			m_fileCacheMgr.UpdateLocalizedFileCacheEntries();
			result = ERequestStatus::Success;

			break;
		}
	case EAudioManagerRequestType::RetriggerAudioControls:
		{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			RetriggerAudioControls();
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE
			result = ERequestStatus::Success;

			break;
		}
	case EAudioManagerRequestType::ReleasePendingRays:
		{
			m_audioObjectMgr.ReleasePendingRays();
			result = ERequestStatus::Success;

			break;
		}
	case EAudioManagerRequestType::ReloadControlsData:
		{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			SAudioManagerRequestData<EAudioManagerRequestType::ReloadControlsData> const* const pRequestData = static_cast<SAudioManagerRequestData<EAudioManagerRequestType::ReloadControlsData> const*>(request.GetData());
			for (auto pAudioObject : m_audioObjectMgr.GetAudioObjects())
			{
				for (auto pEvent : pAudioObject->GetActiveEvents())
				{
					if (pEvent != nullptr)
					{
						result = pEvent->Reset();
					}
				}
			}

			if (ClearControlsData(EDataScope::All) == ERequestStatus::Success)
			{
				if (ParseControlsData(pRequestData->folderPath, EDataScope::Global) == ERequestStatus::Success)
				{
					if (strcmp(pRequestData->levelName, "") != 0)
					{
						if (ParseControlsData(pRequestData->folderPath + pRequestData->levelName, EDataScope::LevelSpecific) == ERequestStatus::Success)
						{
							RetriggerAudioControls();
							result = ERequestStatus::Success;
						}
					}
					else
					{
						RetriggerAudioControls();
						result = ERequestStatus::Success;
					}
				}
			}
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE
			break;
		}
	case EAudioManagerRequestType::DrawDebugInfo:
		{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			DrawAudioSystemDebugInfo();
			result = ERequestStatus::Success;
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE

			break;
		}
	case EAudioManagerRequestType::GetAudioFileData:
		{
			SAudioManagerRequestData<EAudioManagerRequestType::GetAudioFileData> const* const pRequestData =
			  static_cast<SAudioManagerRequestData<EAudioManagerRequestType::GetAudioFileData> const* const>(request.GetData());
			m_pIImpl->GetFileData(pRequestData->name.c_str(), pRequestData->fileData);
			break;
		}
	case EAudioManagerRequestType::None:
		{
			result = ERequestStatus::Success;

			break;
		}
	default:
		{
			g_logger.Log(ELogType::Warning, "ATL received an unknown AudioManager request: %u", pRequestDataBase->type);
			result = ERequestStatus::FailureInvalidRequest;

			break;
		}
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CAudioTranslationLayer::ProcessAudioCallbackManagerRequest(CAudioRequest& request)
{
	ERequestStatus result = ERequestStatus::Failure;
	SAudioCallbackManagerRequestDataBase const* const pRequestDataBase =
	  static_cast<SAudioCallbackManagerRequestDataBase const* const>(request.GetData());

	switch (pRequestDataBase->type)
	{
	case EAudioCallbackManagerRequestType::ReportStartedEvent:
		{
			SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStartedEvent> const* const pRequestData =
			  static_cast<SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStartedEvent> const* const>(request.GetData());
			CATLEvent& audioEvent = pRequestData->audioEvent;

			audioEvent.m_state = EEventState::PlayingDelayed;

			if (audioEvent.m_pAudioObject != m_pGlobalAudioObject)
			{
				m_audioObjectMgr.ReportStartedEvent(&audioEvent);
			}
			else
			{
				m_pGlobalAudioObject->ReportStartedEvent(&audioEvent);
			}

			result = ERequestStatus::Success;

			break;
		}
	case EAudioCallbackManagerRequestType::ReportFinishedEvent:
		{
			SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportFinishedEvent> const* const pRequestData =
			  static_cast<SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportFinishedEvent> const* const>(request.GetData());
			CATLEvent& event = pRequestData->audioEvent;

			if (event.m_pAudioObject != m_pGlobalAudioObject)
			{
				m_audioObjectMgr.ReportFinishedEvent(&event, pRequestData->bSuccess);
			}
			else
			{
				m_pGlobalAudioObject->ReportFinishedEvent(&event, pRequestData->bSuccess);
			}

			m_audioEventMgr.ReleaseEvent(&event);

			result = ERequestStatus::Success;

			break;
		}
	case EAudioCallbackManagerRequestType::ReportVirtualizedEvent:
		{
			SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportVirtualizedEvent> const* const pRequestData =
			  static_cast<SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportVirtualizedEvent> const* const>(request.GetData());

			pRequestData->audioEvent.m_state = EEventState::Virtual;

			result = ERequestStatus::Success;

			break;
		}
	case EAudioCallbackManagerRequestType::ReportPhysicalizedEvent:
		{
			SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportPhysicalizedEvent> const* const pRequestData =
			  static_cast<SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportPhysicalizedEvent> const* const>(request.GetData());

			pRequestData->audioEvent.m_state = EEventState::Playing;

			result = ERequestStatus::Success;

			break;
		}
	case EAudioCallbackManagerRequestType::ReportStartedFile:
		{
			SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStartedFile> const* const pRequestData =
			  static_cast<SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStartedFile> const* const>(request.GetData());

			CATLStandaloneFile& audioStandaloneFile = pRequestData->audioStandaloneFile;

			m_audioObjectMgr.GetStartedStandaloneFileRequestData(&audioStandaloneFile, request);
			audioStandaloneFile.m_state = (pRequestData->bSuccess) ? EAudioStandaloneFileState::Playing : EAudioStandaloneFileState::None;

			result = (pRequestData->bSuccess) ? ERequestStatus::Success : ERequestStatus::Failure;

			break;
		}
	case EAudioCallbackManagerRequestType::ReportStoppedFile:
		{
			SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStoppedFile> const* const pRequestData =
			  static_cast<SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportStoppedFile> const* const>(request.GetData());

			CATLStandaloneFile& audioStandaloneFile = pRequestData->audioStandaloneFile;

			m_audioObjectMgr.GetStartedStandaloneFileRequestData(&audioStandaloneFile, request);

			if (audioStandaloneFile.m_pAudioObject != m_pGlobalAudioObject)
			{
				m_audioObjectMgr.ReportFinishedStandaloneFile(&audioStandaloneFile);
			}
			else
			{
				m_pGlobalAudioObject->ReportFinishedStandaloneFile(&audioStandaloneFile);
			}

			m_audioStandaloneFileMgr.ReleaseStandaloneFile(&audioStandaloneFile);

			result = ERequestStatus::Success;

			break;
		}
	case EAudioCallbackManagerRequestType::ReportFinishedTriggerInstance:
	case EAudioCallbackManagerRequestType::None:
		{
			result = ERequestStatus::Success;

			break;
		}
	default:
		{
			result = ERequestStatus::FailureInvalidRequest;
			g_logger.Log(ELogType::Warning, "ATL received an unknown AudioCallbackManager request: %u", pRequestDataBase->type);

			break;
		}
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CAudioTranslationLayer::ProcessAudioObjectRequest(CAudioRequest const& request)
{
	ERequestStatus result = ERequestStatus::Failure;
	CATLAudioObject* const pObject = (request.pObject != nullptr) ? request.pObject : m_pGlobalAudioObject;

	SAudioObjectRequestDataBase const* const pBaseRequestData =
	  static_cast<SAudioObjectRequestDataBase const* const>(request.GetData());

	switch (pBaseRequestData->type)
	{
	case EAudioObjectRequestType::LoadTrigger:
		{
			SAudioObjectRequestData<EAudioObjectRequestType::LoadTrigger> const* const pRequestData =
			  static_cast<SAudioObjectRequestData<EAudioObjectRequestType::LoadTrigger> const* const>(request.GetData());

			CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, pRequestData->audioTriggerId, nullptr);

			if (pTrigger != nullptr)
			{
				result = pObject->LoadTriggerAsync(pTrigger, true);
			}
			else
			{
				result = ERequestStatus::FailureInvalidControlId;
			}

			break;
		}
	case EAudioObjectRequestType::UnloadTrigger:
		{
			SAudioObjectRequestData<EAudioObjectRequestType::UnloadTrigger> const* const pRequestData =
			  static_cast<SAudioObjectRequestData<EAudioObjectRequestType::UnloadTrigger> const* const>(request.GetData());

			CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, pRequestData->audioTriggerId, nullptr);

			if (pTrigger != nullptr)
			{
				result = pObject->LoadTriggerAsync(pTrigger, false);
			}
			else
			{
				result = ERequestStatus::FailureInvalidControlId;
			}

			break;
		}
	case EAudioObjectRequestType::PlayFile:
		{
			SAudioObjectRequestData<EAudioObjectRequestType::PlayFile> const* const pRequestData =
			  static_cast<SAudioObjectRequestData<EAudioObjectRequestType::PlayFile> const* const>(request.GetData());

			if (pRequestData != nullptr && !pRequestData->file.empty())
			{
				Impl::ITrigger const* pITrigger = nullptr;

				if (pRequestData->usedAudioTriggerId != InvalidControlId)
				{
					CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, pRequestData->usedAudioTriggerId, nullptr);

					if (pTrigger != nullptr && !pTrigger->m_implPtrs.empty())
					{
						pITrigger = pTrigger->m_implPtrs[0]->m_pImplData;
					}
				}

				CATLStandaloneFile* const pFile = m_audioStandaloneFileMgr.ConstructStandaloneFile(pRequestData->file.c_str(), pRequestData->bLocalized, pITrigger);
				result = pObject->HandlePlayFile(pFile, request.pOwner, request.pUserData, request.pUserDataOwner);
			}

			break;
		}
	case EAudioObjectRequestType::StopFile:
		{
			SAudioObjectRequestData<EAudioObjectRequestType::StopFile> const* const pRequestData =
			  static_cast<SAudioObjectRequestData<EAudioObjectRequestType::StopFile> const* const>(request.GetData());

			if (pRequestData != nullptr && !pRequestData->file.empty())
			{
				result = pObject->HandleStopFile(pRequestData->file.c_str());
			}

			break;
		}
	case EAudioObjectRequestType::ExecuteTrigger:
		{
			SAudioObjectRequestData<EAudioObjectRequestType::ExecuteTrigger> const* const pRequestData =
			  static_cast<SAudioObjectRequestData<EAudioObjectRequestType::ExecuteTrigger> const* const>(request.GetData());

			CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, pRequestData->audioTriggerId, nullptr);

			if (pTrigger != nullptr)
			{
				result = pObject->HandleExecuteTrigger(pTrigger, request.pOwner, request.pUserData, request.pUserDataOwner, request.flags);
			}
			else
			{
				result = ERequestStatus::FailureInvalidControlId;
			}

			break;
		}
	case EAudioObjectRequestType::ExecuteTriggerEx:
		{
			SAudioObjectRequestData<EAudioObjectRequestType::ExecuteTriggerEx> const* const pRequestData =
			  static_cast<SAudioObjectRequestData<EAudioObjectRequestType::ExecuteTriggerEx> const* const>(request.GetData());

			CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, pRequestData->triggerId, nullptr);

			if (pTrigger != nullptr)
			{
				CATLAudioObject* const pObject = new CATLAudioObject;
				m_audioObjectMgr.RegisterObject(pObject);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
				pObject->Init(pRequestData->name.c_str(), m_pIImpl->ConstructObject(pRequestData->name.c_str()), m_audioListenerMgr.GetActiveListenerAttributes().transformation.GetPosition(), pRequestData->entityId);
#else
				pObject->Init(nullptr, m_pIImpl->ConstructObject(nullptr), m_audioListenerMgr.GetActiveListenerAttributes().transformation.GetPosition(), pRequestData->entityId);
#endif    // INCLUDE_AUDIO_PRODUCTION_CODE

				result = pObject->HandleSetTransformation(pRequestData->transformation, 0.0f);

				if (pRequestData->bSetCurrentEnvironments)
				{
					SetCurrentEnvironmentsOnObject(pObject, INVALID_ENTITYID, pRequestData->transformation.GetPosition());
				}

				if (pRequestData->occlusionType < EOcclusionType::Count)
				{
					// TODO: Can we prevent these lookups and access the switch and state directly?
					CATLSwitch const* const pSwitch = stl::find_in_map(m_switches, OcclusionTypeSwitchId, nullptr);

					if (pSwitch != nullptr)
					{
						CATLSwitchState const* const pState = stl::find_in_map(pSwitch->audioSwitchStates, OcclusionTypeStateIds[IntegralValue(pRequestData->occlusionType)], nullptr);

						if (pState != nullptr)
						{
							result = pObject->HandleSetSwitchState(pSwitch, pState);
						}
					}
				}

				result = pObject->HandleExecuteTrigger(pTrigger, request.pOwner, request.pUserData, request.pUserDataOwner, request.flags);
				pObject->RemoveFlag(EObjectFlags::InUse);
			}
			else
			{
				result = ERequestStatus::FailureInvalidControlId;
			}

			break;
		}
	case EAudioObjectRequestType::StopTrigger:
		{
			SAudioObjectRequestData<EAudioObjectRequestType::StopTrigger> const* const pRequestData =
			  static_cast<SAudioObjectRequestData<EAudioObjectRequestType::StopTrigger> const* const>(request.GetData());

			CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, pRequestData->audioTriggerId, nullptr);

			if (pTrigger != nullptr)
			{
				result = pObject->HandleStopTrigger(pTrigger);
			}
			else
			{
				result = ERequestStatus::FailureInvalidControlId;
			}

			break;
		}
	case EAudioObjectRequestType::StopAllTriggers:
		{
			result = pObject->StopAllTriggers();
			;

			break;
		}
	case EAudioObjectRequestType::SetTransformation:
		{
			if (pObject != m_pGlobalAudioObject)
			{
				float const distanceToListener = (m_audioListenerMgr.GetActiveListenerAttributes().transformation.GetPosition() - pObject->GetTransformation().GetPosition()).GetLength();
				SAudioObjectRequestData<EAudioObjectRequestType::SetTransformation> const* const pRequestData =
				  static_cast<SAudioObjectRequestData<EAudioObjectRequestType::SetTransformation> const* const>(request.GetData());

				result = pObject->HandleSetTransformation(pRequestData->transformation, distanceToListener);
			}
			else
			{
				g_logger.Log(ELogType::Warning, "ATL received a request to set a position on a global object");
			}

			break;
		}
	case EAudioObjectRequestType::SetParameter:
		{
			result = ERequestStatus::FailureInvalidControlId;

			SAudioObjectRequestData<EAudioObjectRequestType::SetParameter> const* const pRequestData =
			  static_cast<SAudioObjectRequestData<EAudioObjectRequestType::SetParameter> const* const>(request.GetData());

			CParameter const* const pParameter = stl::find_in_map(m_parameters, pRequestData->parameterId, nullptr);

			if (pParameter != nullptr)
			{
				result = pObject->HandleSetParameter(pParameter, pRequestData->value);
			}

			break;
		}
	case EAudioObjectRequestType::SetSwitchState:
		{
			result = ERequestStatus::FailureInvalidControlId;
			SAudioObjectRequestData<EAudioObjectRequestType::SetSwitchState> const* const pRequestData =
			  static_cast<SAudioObjectRequestData<EAudioObjectRequestType::SetSwitchState> const* const>(request.GetData());

			CATLSwitch const* const pSwitch = stl::find_in_map(m_switches, pRequestData->audioSwitchId, nullptr);

			if (pSwitch != nullptr)
			{
				CATLSwitchState const* const pState = stl::find_in_map(pSwitch->audioSwitchStates, pRequestData->audioSwitchStateId, nullptr);

				if (pState != nullptr)
				{
					result = pObject->HandleSetSwitchState(pSwitch, pState);
				}
			}

			break;
		}
	case EAudioObjectRequestType::SetCurrentEnvironments:
		{
			SAudioObjectRequestData<EAudioObjectRequestType::SetCurrentEnvironments> const* const pRequestData =
			  static_cast<SAudioObjectRequestData<EAudioObjectRequestType::SetCurrentEnvironments> const* const>(request.GetData());

			SetCurrentEnvironmentsOnObject(pObject, pRequestData->entityToIgnore, pRequestData->position);

			break;
		}
	case EAudioObjectRequestType::SetEnvironment:
		{
			if (pObject != m_pGlobalAudioObject)
			{
				result = ERequestStatus::FailureInvalidControlId;
				SAudioObjectRequestData<EAudioObjectRequestType::SetEnvironment> const* const pRequestData =
				  static_cast<SAudioObjectRequestData<EAudioObjectRequestType::SetEnvironment> const* const>(request.GetData());

				CATLAudioEnvironment const* const pEnvironment = stl::find_in_map(m_environments, pRequestData->audioEnvironmentId, nullptr);

				if (pEnvironment != nullptr)
				{
					result = pObject->HandleSetEnvironment(pEnvironment, pRequestData->amount);
				}
			}
			else
			{
				g_logger.Log(ELogType::Warning, "ATL received a request to set an environment on a global object");
			}

			break;
		}
	case EAudioObjectRequestType::ResetEnvironments:
		{
			result = pObject->HandleResetEnvironments(m_environments);
			break;
		}
	case EAudioObjectRequestType::RegisterObject:
		{
			SAudioObjectRequestData<EAudioObjectRequestType::RegisterObject> const* const pRequestData =
			  static_cast<SAudioObjectRequestData<EAudioObjectRequestType::RegisterObject> const*>(request.GetData());

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			pObject->Init(pRequestData->name.c_str(), m_pIImpl->ConstructObject(pRequestData->name.c_str()), m_audioListenerMgr.GetActiveListenerAttributes().transformation.GetPosition(), pRequestData->entityId);
#else
			pObject->Init(nullptr, m_pIImpl->ConstructObject(nullptr), m_audioListenerMgr.GetActiveListenerAttributes().transformation.GetPosition(), pRequestData->entityId);
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE

			result = pObject->HandleSetTransformation(pRequestData->transformation, 0.0f);
			CRY_ASSERT(result == ERequestStatus::Success);

			if (pRequestData->bSetCurrentEnvironments)
			{
				SetCurrentEnvironmentsOnObject(pObject, INVALID_ENTITYID, pRequestData->transformation.GetPosition());
			}

			if (pRequestData->occlusionType < EOcclusionType::Count)
			{
				// TODO: Can we prevent these lookups and access the switch and state directly?
				CATLSwitch const* const pSwitch = stl::find_in_map(m_switches, OcclusionTypeSwitchId, nullptr);

				if (pSwitch != nullptr)
				{
					CATLSwitchState const* const pState = stl::find_in_map(pSwitch->audioSwitchStates, OcclusionTypeStateIds[IntegralValue(pRequestData->occlusionType)], nullptr);

					if (pState != nullptr)
					{
						result = pObject->HandleSetSwitchState(pSwitch, pState);
						CRY_ASSERT(result == ERequestStatus::Success);
					}
				}
			}

			m_audioObjectMgr.RegisterObject(pObject);
			result = ERequestStatus::Success;
			break;
		}
	case EAudioObjectRequestType::ReleaseObject:
		{
			if (pObject != m_pGlobalAudioObject)
			{
				pObject->RemoveFlag(EObjectFlags::InUse);
				result = ERequestStatus::Success;
			}
			else
			{
				g_logger.Log(ELogType::Warning, "ATL received a request to release the GlobalAudioObject");
			}

			break;
		}
	case EAudioObjectRequestType::ProcessPhysicsRay:
		{
			SAudioObjectRequestData<EAudioObjectRequestType::ProcessPhysicsRay> const* const pRequestData =
			  static_cast<SAudioObjectRequestData<EAudioObjectRequestType::ProcessPhysicsRay> const* const>(request.GetData());

			pObject->ProcessPhysicsRay(pRequestData->pAudioRayInfo);
			result = ERequestStatus::Success;
			break;
		}
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	case EAudioObjectRequestType::SetName:
		{
			SAudioObjectRequestData<EAudioObjectRequestType::SetName> const* const pRequestData =
			  static_cast<SAudioObjectRequestData<EAudioObjectRequestType::SetName> const* const>(request.GetData());

			result = pObject->HandleSetName(pRequestData->name.c_str());

			if (result == ERequestStatus::SuccessNeedsRefresh)
			{
				pObject->ForceImplementationRefresh(m_triggers, m_parameters, m_switches, m_environments, true);
				result = ERequestStatus::Success;
			}

			break;
		}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	case EAudioObjectRequestType::None:
		{
			result = ERequestStatus::Success;
			break;
		}
	default:
		{
			g_logger.Log(ELogType::Warning, "ATL received an unknown AudioObject request type: %u", pBaseRequestData->type);
			result = ERequestStatus::FailureInvalidRequest;
			break;
		}
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CAudioTranslationLayer::ProcessAudioListenerRequest(SAudioRequestData const* const pPassedRequestData)
{
	ERequestStatus result = ERequestStatus::Failure;
	SAudioListenerRequestDataBase const* const pBaseRequestData =
	  static_cast<SAudioListenerRequestDataBase const* const>(pPassedRequestData);

	switch (pBaseRequestData->type)
	{
	case EAudioListenerRequestType::SetTransformation:
		{
			SAudioListenerRequestData<EAudioListenerRequestType::SetTransformation> const* const pRequestData =
			  static_cast<SAudioListenerRequestData<EAudioListenerRequestType::SetTransformation> const* const>(pPassedRequestData);

			CRY_ASSERT(pRequestData->pListener != nullptr);

			if (pRequestData->pListener != nullptr)
			{
				result = pRequestData->pListener->HandleSetTransformation(pRequestData->transformation);
			}
		}
		break;
	case EAudioListenerRequestType::RegisterListener:
		{
			SAudioListenerRequestData<EAudioListenerRequestType::RegisterListener> const* const pRequestData =
			  static_cast<SAudioListenerRequestData<EAudioListenerRequestType::RegisterListener> const*>(pPassedRequestData);
			*pRequestData->ppListener = m_audioListenerMgr.CreateListener(pRequestData->name.c_str());
		}
		break;
	case EAudioListenerRequestType::ReleaseListener:
		{
			SAudioListenerRequestData<EAudioListenerRequestType::ReleaseListener> const* const pRequestData =
			  static_cast<SAudioListenerRequestData<EAudioListenerRequestType::ReleaseListener> const* const>(pPassedRequestData);

			CRY_ASSERT(pRequestData->pListener != nullptr);

			if (pRequestData->pListener != nullptr)
			{
				m_audioListenerMgr.ReleaseListener(pRequestData->pListener);
				result = ERequestStatus::Success;
			}
		}
		break;
	case EAudioListenerRequestType::None:
		result = ERequestStatus::Success;
		break;
	default:
		result = ERequestStatus::FailureInvalidRequest;
		g_logger.Log(ELogType::Warning, "ATL received an unknown AudioListener request type: %u", pPassedRequestData->type);
		break;
	}

	return result;
}
///////////////////////////////////////////////////////////////////////////
ERequestStatus CAudioTranslationLayer::SetImpl(Impl::IImpl* const pIImpl)
{
	ERequestStatus result = ERequestStatus::Failure;

	if (m_pIImpl != nullptr && pIImpl != m_pIImpl)
	{
		ReleaseImpl();
	}

	m_pIImpl = pIImpl;

	if (m_pIImpl == nullptr)
	{
		g_logger.Log(ELogType::Warning, "nullptr passed to SetImpl, will run with the null implementation");

		auto const pImpl = new Impl::Null::CImpl();
		CRY_ASSERT(pImpl != nullptr);
		m_pIImpl = static_cast<Impl::IImpl*>(pImpl);
	}

	result = m_pIImpl->Init(g_cvars.m_audioObjectPoolSize, g_cvars.m_audioEventPoolSize);

	if (result != ERequestStatus::Success)
	{
		// The impl failed to initialize, allow it to shut down and release then fall back to the null impl.
		g_logger.Log(ELogType::Error, "Failed to set the AudioImpl %s. Will run with the null implementation.", m_pIImpl->GetName());

		// There's no need to call Shutdown when the initialization failed as
		// we expect the implementation to clean-up itself if it couldn't be initialized

		result = m_pIImpl->Release(); // Release the engine specific data.
		CRY_ASSERT(result == ERequestStatus::Success);

		auto const pImpl = new Impl::Null::CImpl();
		CRY_ASSERT(pImpl != nullptr);
		m_pIImpl = static_cast<Impl::IImpl*>(pImpl);
	}

	if (m_pGlobalAudioObject == nullptr)
	{
		m_pGlobalAudioObject = new CATLAudioObject;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		m_pGlobalAudioObject->m_name = "Global Audio Object";
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}

	CRY_ASSERT(m_pGlobalAudioObject->GetImplDataPtr() == nullptr);
	m_pGlobalAudioObject->SetImplDataPtr(m_pIImpl->ConstructGlobalObject());

	m_audioObjectMgr.Init(m_pIImpl);
	m_audioEventMgr.Init(m_pIImpl);
	m_audioStandaloneFileMgr.Init(m_pIImpl);
	m_audioListenerMgr.Init(m_pIImpl);
	m_xmlProcessor.Init(m_pIImpl);
	m_fileCacheMgr.Init(m_pIImpl);

	CATLControlImpl::SetImpl(m_pIImpl);

	SetImplLanguage();
	InitInternalControls();

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	m_pProfileData->SetImplName(m_pIImpl->GetName());
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

	return result;
}

///////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::ReleaseImpl()
{
	// During audio middleware shutdown we do not allow for any new requests originating from the "dying" audio middleware!
	m_flags |= EInternalStates::AudioMiddlewareShuttingDown;

	m_pIImpl->OnBeforeShutDown();

	m_audioStandaloneFileMgr.Release();
	m_audioEventMgr.Release();
	m_audioObjectMgr.Release();
	m_audioListenerMgr.Release();

	m_pIImpl->DestructObject(m_pGlobalAudioObject->GetImplDataPtr());
	m_pGlobalAudioObject->SetImplDataPtr(nullptr);

	delete m_pGlobalAudioObject;
	m_pGlobalAudioObject = nullptr;

	m_xmlProcessor.ClearControlsData(EDataScope::All);
	m_xmlProcessor.ClearPreloadsData(EDataScope::All);

	m_fileCacheMgr.Release();

	ERequestStatus result = m_pIImpl->ShutDown(); // Shut down the audio middleware.
	CRY_ASSERT(result == ERequestStatus::Success);

	result = m_pIImpl->Release(); // Release the engine specific data.
	CRY_ASSERT(result == ERequestStatus::Success);

	m_pIImpl = nullptr;
	m_flags &= ~EInternalStates::AudioMiddlewareShuttingDown;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CAudioTranslationLayer::RefreshAudioSystem(char const* const szLevelName)
{
	g_logger.Log(ELogType::Warning, "Beginning to refresh the AudioSystem!");

	ERequestStatus result = m_pIImpl->StopAllSounds();
	CRY_ASSERT(result == ERequestStatus::Success);

	result = m_fileCacheMgr.UnloadDataByScope(EDataScope::LevelSpecific);
	CRY_ASSERT(result == ERequestStatus::Success);

	result = m_fileCacheMgr.UnloadDataByScope(EDataScope::Global);
	CRY_ASSERT(result == ERequestStatus::Success);

	result = ClearControlsData(EDataScope::All);
	CRY_ASSERT(result == ERequestStatus::Success);

	result = ClearPreloadsData(EDataScope::All);
	CRY_ASSERT(result == ERequestStatus::Success);

	m_pIImpl->OnRefresh();

	SetImplLanguage();

	CryFixedStringT<MaxFilePathLength> configPath(gEnv->pAudioSystem->GetConfigPath());
	result = ParseControlsData(configPath.c_str(), EDataScope::Global);
	CRY_ASSERT(result == ERequestStatus::Success);
	result = ParsePreloadsData(configPath.c_str(), EDataScope::Global);
	CRY_ASSERT(result == ERequestStatus::Success);

	// The global preload might not exist if no preloads have been created, for that reason we don't check the result of this call
	result = m_fileCacheMgr.TryLoadRequest(GlobalPreloadRequestId, true, true);

	if (szLevelName != nullptr && szLevelName[0] != '\0')
	{
		configPath += "levels" CRY_NATIVE_PATH_SEPSTR;
		configPath += szLevelName;
		result = ParseControlsData(configPath.c_str(), EDataScope::LevelSpecific);
		CRY_ASSERT(result == ERequestStatus::Success);

		result = ParsePreloadsData(configPath.c_str(), EDataScope::LevelSpecific);
		CRY_ASSERT(result == ERequestStatus::Success);

		PreloadRequestId const preloadRequestId = StringToId(szLevelName);
		result = m_fileCacheMgr.TryLoadRequest(preloadRequestId, true, true);
		
		if (result != ERequestStatus::Success)
		{
			g_logger.Log(ELogType::Warning, R"(No preload request found for level - "%s"!)", szLevelName);
		}
	}

	g_logger.Log(ELogType::Warning, "Done refreshing the AudioSystem!");

	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::SetImplLanguage()
{
	if (ICVar* pCVar = gEnv->pConsole->GetCVar("g_languageAudio"))
	{
		m_pIImpl->SetLanguage(pCVar->GetString());
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::OnInputEvent(SInputEvent const& event)
{
	if (event.state == eIS_Changed && event.deviceType == eIDT_Gamepad)
	{
		if (event.keyId == eKI_SYS_ConnectDevice)
		{
			m_pIImpl->GamepadConnected(event.deviceUniqueID);
		}
		else if (event.keyId == eKI_SYS_DisconnectDevice)
		{
			m_pIImpl->GamepadDisconnected(event.deviceUniqueID);
		}
	}

	// Do not consume event
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::InitInternalControls()
{
	m_internalControls.m_switchStates.clear();
	m_internalControls.m_parameters.clear();

	// Occlusion
	COcclusionObstructionState* pOcclusionIgnore = new COcclusionObstructionState(IgnoreStateId, m_audioListenerMgr, *m_pGlobalAudioObject);
	m_internalControls.m_switchStates[std::make_pair(OcclusionCalcSwitchId, IgnoreStateId)]
	  = pOcclusionIgnore;

	COcclusionObstructionState* pOcclusionAdaptive = new COcclusionObstructionState(AdaptiveStateId, m_audioListenerMgr, *m_pGlobalAudioObject);
	m_internalControls.m_switchStates[std::make_pair(OcclusionCalcSwitchId, AdaptiveStateId)]
	  = pOcclusionAdaptive;

	COcclusionObstructionState* pOcclusionLow = new COcclusionObstructionState(LowStateId, m_audioListenerMgr, *m_pGlobalAudioObject);
	m_internalControls.m_switchStates[std::make_pair(OcclusionCalcSwitchId, LowStateId)]
	  = pOcclusionLow;

	COcclusionObstructionState* pOcclusionMedium = new COcclusionObstructionState(MediumStateId, m_audioListenerMgr, *m_pGlobalAudioObject);
	m_internalControls.m_switchStates[std::make_pair(OcclusionCalcSwitchId, MediumStateId)]
	  = pOcclusionMedium;

	COcclusionObstructionState* pOcclusionHigh = new COcclusionObstructionState(HighStateId, m_audioListenerMgr, *m_pGlobalAudioObject);
	m_internalControls.m_switchStates[std::make_pair(OcclusionCalcSwitchId, HighStateId)]
	  = pOcclusionHigh;

	// Relative Velocity
	CDopplerTrackingState* pDopplerOn = new CDopplerTrackingState(OnStateId, *m_pGlobalAudioObject);
	m_internalControls.m_switchStates[std::make_pair(RelativeVelocityTrackingSwitchId, OnStateId)]
	  = pDopplerOn;

	CDopplerTrackingState* pDopplerOff = new CDopplerTrackingState(OffStateId, *m_pGlobalAudioObject);
	m_internalControls.m_switchStates[std::make_pair(RelativeVelocityTrackingSwitchId, OffStateId)]
	  = pDopplerOff;

	// Absolute Velocity
	CVelocityTrackingState* pVelocityOn = new CVelocityTrackingState(OnStateId, *m_pGlobalAudioObject);
	m_internalControls.m_switchStates[std::make_pair(AbsoluteVelocityTrackingSwitchId, OnStateId)]
	  = pVelocityOn;

	CVelocityTrackingState* pVelocityOff = new CVelocityTrackingState(OffStateId, *m_pGlobalAudioObject);
	m_internalControls.m_switchStates[std::make_pair(AbsoluteVelocityTrackingSwitchId, OffStateId)]
	  = pVelocityOff;
}

//////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::SetCurrentEnvironmentsOnObject(CATLAudioObject* const pObject, EntityId const entityToIgnore, Vec3 const& position)
{
	IAreaManager* const pIAreaManager = gEnv->pEntitySystem->GetAreaManager();
	size_t numAreas = 0;
	static size_t const s_maxAreas = 10;
	static SAudioAreaInfo s_areaInfos[s_maxAreas];

	if (pIAreaManager->QueryAudioAreas(position, s_areaInfos, s_maxAreas, numAreas))
	{
		for (size_t i = 0; i < numAreas; ++i)
		{
			SAudioAreaInfo const& areaInfo = s_areaInfos[i];

			if (entityToIgnore == INVALID_ENTITYID || entityToIgnore != areaInfo.envProvidingEntityId)
			{
				CATLAudioEnvironment const* const pEnvironment = stl::find_in_map(m_environments, areaInfo.audioEnvironmentId, nullptr);

				if (pEnvironment != nullptr)
				{
					pObject->HandleSetEnvironment(pEnvironment, areaInfo.amount);
				}
			}
		}
	}
}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::DrawAudioSystemDebugInfo()
{
	CRY_PROFILE_FUNCTION(PROFILE_AUDIO);

	if (IRenderAuxGeom* const pAuxGeom = g_cvars.m_drawAudioDebug > 0 && gEnv->pRenderer != nullptr ? gEnv->pRenderer->GetIRenderAuxGeom() : nullptr)
	{
		DrawAudioObjectDebugInfo(*pAuxGeom); // needs to be called first so that the rest of the labels are printed
		// on top (Draw2dLabel doesn't provide a way set which labels are printed on top)

		float posX = 0.0f;
		float posY = 4.0f;
		float const lineHeight = 17.0f;
		float const indentation = 20.0f;

		static float const s_colorWhite[4] = { 1.0f, 1.0f, 1.0f, 0.9f };
		static float const s_colorRed[4] = { 1.0f, 0.0f, 0.0f, 0.7f };
		static float const s_colorGreen[4] = { 0.0f, 1.0f, 0.0f, 0.7f };
		static float const s_colorBlue[4] = { 0.4f, 0.4f, 1.0f, 1.0f };

		pAuxGeom->Draw2dLabel(posX, posY, 1.6f, s_colorBlue, false, "AudioTranslationLayer with %s", m_pProfileData->GetImplName());
		posX += indentation;
		posY += lineHeight;

		CryModuleMemoryInfo memInfo;
		ZeroStruct(memInfo);
		CryGetMemoryInfoForModule(&memInfo);

		pAuxGeom->Draw2dLabel(posX, posY, 1.35f, s_colorWhite, false,
		                      "[ATL] Total Memory Used: %uKiB",
		                      static_cast<uint32>((memInfo.allocated - memInfo.freed) / 1024));

		posX += indentation;
		{
			CPoolObject<CATLAudioObject, stl::PSyncMultiThread>::Allocator& allocator = CATLAudioObject::GetAllocator();
			posY += lineHeight;
			auto mem = allocator.GetTotalMemory();
			auto pool = allocator.GetCounts();
			pAuxGeom->Draw2dLabel(posX, posY, 1.35f, s_colorWhite, false, "[%s] InUse: %u | Constructed: %u (%uKiB) | Memory Pool: %uKiB", "Objects", pool.nUsed, pool.nAlloc, mem.nUsed / 1024, mem.nAlloc / 1024);
		}

		{
			CPoolObject<CATLEvent, stl::PSyncNone>::Allocator& allocator = CATLEvent::GetAllocator();
			posY += lineHeight;
			auto mem = allocator.GetTotalMemory();
			auto pool = allocator.GetCounts();
			pAuxGeom->Draw2dLabel(posX, posY, 1.35f, s_colorWhite, false, "[%s] InUse: %u | Constructed: %u (%uKiB) | Memory Pool: %uKiB", "Events", pool.nUsed, pool.nAlloc, mem.nUsed / 1024, mem.nAlloc / 1024);
		}

		{
			CPoolObject<CATLStandaloneFile, stl::PSyncNone>::Allocator& allocator = CATLStandaloneFile::GetAllocator();
			posY += lineHeight;
			auto mem = allocator.GetTotalMemory();
			auto pool = allocator.GetCounts();
			pAuxGeom->Draw2dLabel(posX, posY, 1.35f, s_colorWhite, false, "[%s] InUse: %u | Constructed: %u (%uKiB) | Memory Pool: %uKiB", "Files", pool.nUsed, pool.nAlloc, mem.nUsed / 1024, mem.nAlloc / 1024);
		}
		posX -= indentation;

		if (m_pIImpl != nullptr)
		{
			Impl::SMemoryInfo memoryInfo;
			m_pIImpl->GetMemoryInfo(memoryInfo);

			posY += lineHeight;
			pAuxGeom->Draw2dLabel(posX, posY, 1.35f, s_colorWhite, false, "[Impl] Total Memory Used: %uKiB | Secondary Memory: %.2f / %.2f MiB NumAllocs: %d",
			                      static_cast<uint32>(memoryInfo.totalMemory / 1024),
			                      (memoryInfo.secondaryPoolUsedSize / 1024) / 1024.0f,
			                      (memoryInfo.secondaryPoolSize / 1024) / 1024.0f,
			                      static_cast<int>(memoryInfo.secondaryPoolAllocations));

			posX += indentation;
			posY += lineHeight;
			pAuxGeom->Draw2dLabel(posX, posY, 1.35f, s_colorWhite, false, "[Impl Object Pool] InUse: %u | Constructed: %u (%uKiB) | Memory Pool: %uKiB",
			                      memoryInfo.poolUsedObjects, memoryInfo.poolConstructedObjects, memoryInfo.poolUsedMemory, memoryInfo.poolAllocatedMemory);
			posX -= indentation;
		}

		posY += lineHeight;
		static float const SMOOTHING_ALPHA = 0.2f;
		static float syncRays = 0;
		static float asyncRays = 0;

		size_t const numObjects = m_audioObjectMgr.GetNumAudioObjects();
		size_t const numActiveObjects = m_audioObjectMgr.GetNumActiveAudioObjects();
		size_t const numEvents = m_audioEventMgr.GetNumConstructed();
		size_t const numListeners = m_audioListenerMgr.GetNumActiveListeners();
		size_t const numEventListeners = m_audioEventListenerMgr.GetNumEventListeners();
		syncRays += (CPropagationProcessor::s_totalSyncPhysRays - syncRays) * SMOOTHING_ALPHA;
		asyncRays += (CPropagationProcessor::s_totalAsyncPhysRays - asyncRays) * SMOOTHING_ALPHA * 0.1f;

		bool const bActive = true;
		float const colorListener[4] =
		{
			bActive ? s_colorGreen[0] : s_colorRed[0],
			bActive ? s_colorGreen[1] : s_colorRed[1],
			bActive ? s_colorGreen[2] : s_colorRed[2],
			1.0f
		};

		float const* colorNumbers = s_colorBlue;

		if (numListeners > 0)
		{
			Vec3 const& listenerPosition = m_audioListenerMgr.GetActiveListenerAttributes().transformation.GetPosition();
			Vec3 const& listenerDirection = m_audioListenerMgr.GetActiveListenerAttributes().transformation.GetForward();
			float const listenerVelocity = m_audioListenerMgr.GetActiveListenerAttributes().velocity.GetLength();
			char const* const szName = m_audioListenerMgr.GetActiveListenerName();

			posY += lineHeight;
			pAuxGeom->Draw2dLabel(posX, posY, 1.35f, colorListener, false, "%s PosXYZ: %.2f %.2f %.2f FwdXYZ: %.2f %.2f %.2f Velocity: %.2f m/s", szName, listenerPosition.x, listenerPosition.y, listenerPosition.z, listenerDirection.x, listenerDirection.y, listenerDirection.z, listenerVelocity);
		}

		posY += lineHeight;
		pAuxGeom->Draw2dLabel(posX, posY, 1.35f, colorNumbers, false,
		                      "Objects: %3" PRISIZE_T "/%3" PRISIZE_T " Events: %3" PRISIZE_T " EventListeners %3" PRISIZE_T " Listeners: %" PRISIZE_T " | SyncRays: %3.1f AsyncRays: %3.1f",
		                      numActiveObjects, numObjects, numEvents, numEventListeners, numListeners, syncRays, asyncRays);

		posY += lineHeight;
		DrawATLComponentDebugInfo(*pAuxGeom, posX, posY);

		pAuxGeom->Commit(7);
	}
}

///////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::GetAudioTriggerData(ControlId const audioTriggerId, STriggerData& audioTriggerData) const
{
	CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, audioTriggerId, nullptr);

	if (pTrigger != nullptr)
	{
		audioTriggerData.radius = pTrigger->m_maxRadius;
		audioTriggerData.occlusionFadeOutDistance = pTrigger->m_occlusionFadeOutDistance;
	}
}

//////////////////////////////////////////////////////////////////////////
CProfileData* CAudioTranslationLayer::GetProfileData() const
{
	return m_pProfileData;
}

///////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::DrawATLComponentDebugInfo(IRenderAuxGeom& auxGeom, float posX, float const posY)
{
	m_fileCacheMgr.DrawDebugInfo(auxGeom, posX, posY);

	if ((g_cvars.m_drawAudioDebug & EAudioDebugDrawFilter::ShowActiveObjects) > 0)
	{
		m_audioObjectMgr.DrawDebugInfo(auxGeom, posX, posY);
		posX += 300.0f;
	}

	if ((g_cvars.m_drawAudioDebug & EAudioDebugDrawFilter::ShowActiveEvents) > 0)
	{
		m_audioEventMgr.DrawDebugInfo(auxGeom, posX, posY);
		posX += 600.0f;
	}

	if ((g_cvars.m_drawAudioDebug & EAudioDebugDrawFilter::ShowStandaloneFiles) > 0)
	{
		m_audioStandaloneFileMgr.DrawDebugInfo(auxGeom, posX, posY);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::RetriggerAudioControls()
{
	auto const& registeredAudioObjects = m_audioObjectMgr.GetAudioObjects();

	for (auto const pAudioObject : registeredAudioObjects)
	{
		pAudioObject->ForceImplementationRefresh(m_triggers, m_parameters, m_switches, m_environments, true);
	}

	m_pGlobalAudioObject->ForceImplementationRefresh(m_triggers, m_parameters, m_switches, m_environments, false);
}

///////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::DrawAudioObjectDebugInfo(IRenderAuxGeom& auxGeom)
{
	m_audioObjectMgr.DrawPerObjectDebugInfo(
	  auxGeom,
	  m_audioListenerMgr.GetActiveListenerAttributes().transformation.GetPosition(),
	  m_triggers,
	  m_parameters,
	  m_switches,
	  m_preloadRequests,
	  m_environments);
}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}      // namespace CryAudio

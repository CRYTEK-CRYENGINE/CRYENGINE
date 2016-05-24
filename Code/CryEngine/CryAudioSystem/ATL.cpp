// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ATL.h"
#include "ATLComponents.h"
#include "AudioSystemImpl_NULL.h"
#include "SoundCVars.h"
#include "AudioProxy.h"
#include <CrySystem/ISystem.h>
#include <CryPhysics/IPhysics.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>

using namespace CryAudio::Impl;

///////////////////////////////////////////////////////////////////////////
inline EAudioRequestResult ConvertToRequestResult(EAudioRequestStatus const eAudioRequestStatus)
{
	EAudioRequestResult result;

	switch (eAudioRequestStatus)
	{
	case eAudioRequestStatus_Success:
		{
			result = eAudioRequestResult_Success;
			break;
		}
	case eAudioRequestStatus_Failure:
	case eAudioRequestStatus_FailureInvalidObjectId:
	case eAudioRequestStatus_FailureInvalidControlId:
	case eAudioRequestStatus_FailureInvalidRequest:
	case eAudioRequestStatus_PartialSuccess:
		{
			result = eAudioRequestResult_Failure;
			break;
		}
	default:
		{
			g_audioLogger.Log(eAudioLogType_Error, "Invalid AudioRequestStatus '%u'. Cannot be converted to an AudioRequestResult. ", eAudioRequestStatus);
			CRY_ASSERT(false);
			result = eAudioRequestResult_Failure;
			break;
		}
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
CAudioTranslationLayer::CAudioTranslationLayer()
	: m_pGlobalAudioObject(nullptr)
	, m_globalAudioObjectId(GLOBAL_AUDIO_OBJECT_ID)
	, m_triggerInstanceIDCounter(1)
	, m_pDefaultStandaloneFileTrigger(nullptr)
	, m_audioStandaloneFileMgr()
	, m_audioEventMgr()
	, m_audioObjectMgr(m_audioEventMgr, m_audioStandaloneFileMgr)
	, m_audioListenerMgr()
	, m_fileCacheMgr(m_preloadRequests)
	, m_audioEventListenerMgr()
	, m_xmlProcessor(m_triggers, m_rtpcs, m_switches, m_environments, m_preloadRequests, m_fileCacheMgr)
	, m_lastMainThreadFramId(0)
	, m_flags(eAudioInternalStates_None)
	, m_pImpl(nullptr)
{
	InitATLControlIDs();

	POOL_NEW(CATLAudioObject, m_pGlobalAudioObject)(m_globalAudioObjectId, nullptr);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	m_audioStandaloneFileMgr.SetDebugNameStore(&m_debugNameStore);
	m_audioEventMgr.SetDebugNameStore(&m_debugNameStore);
	m_audioObjectMgr.SetDebugNameStore(&m_debugNameStore);
	m_xmlProcessor.SetDebugNameStore(&m_debugNameStore);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
CAudioTranslationLayer::~CAudioTranslationLayer()
{
	if (m_pImpl != nullptr)
	{
		// the ATL has not yet been properly shut down
		ShutDown();
	}

	POOL_FREE(m_pGlobalAudioObject);
}

///////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::Initialize()
{
	// Add the callback for the obstruction calculation.
	gEnv->pPhysicalWorld->AddEventClient(
	  EventPhysRWIResult::id,
	  &CATLAudioObject::CPropagationProcessor::OnObstructionTest,
	  1);

	// TODO: Rather parse a "default data" type XML to import ATL required controls!
	CAudioProxy::s_occlusionTypeSwitchId = static_cast<AudioControlId>(AudioStringToId("ObstrOcclCalcType"));
	CAudioProxy::s_occlusionTypeStateIds[eAudioOcclusionType_Ignore] = static_cast<AudioSwitchStateId>(AudioStringToId("Ignore"));
	CAudioProxy::s_occlusionTypeStateIds[eAudioOcclusionType_SingleRay] = static_cast<AudioSwitchStateId>(AudioStringToId("SingleRay"));
	CAudioProxy::s_occlusionTypeStateIds[eAudioOcclusionType_MultiRay] = static_cast<AudioSwitchStateId>(AudioStringToId("MultiRay"));

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
		  &CATLAudioObject::CPropagationProcessor::OnObstructionTest,
		  1);
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::ProcessRequest(CAudioRequestInternal& request)
{
	EAudioRequestStatus result = eAudioRequestStatus_None;

	if (request.pData)
	{
		switch (request.pData->type)
		{
		case eAudioRequestType_AudioObjectRequest:
			{
				result = ProcessAudioObjectRequest(request);

				break;
			}
		case eAudioRequestType_AudioListenerRequest:
			{
				if (request.audioObjectId == INVALID_AUDIO_OBJECT_ID)
				{
					// No specific listener ID supplied, therefore we assume default listener ID.
					request.audioObjectId = m_audioListenerMgr.GetDefaultListenerId();
				}

				CATLListenerObject* const pListener = m_audioListenerMgr.LookupId(request.audioObjectId);

				if (pListener != nullptr)
				{
					result = ProcessAudioListenertRequest(pListener, request.pData);
				}
				else
				{
					g_audioLogger.Log(eAudioLogType_Error, "Could not find listener with ID: %u", request.audioObjectId);
					CRY_ASSERT(false);
				}

				break;
			}
		case eAudioRequestType_AudioCallbackManagerRequest:
			{
				result = ProcessAudioCallbackManagerRequest(request);

				break;
			}
		case eAudioRequestType_AudioManagerRequest:
			{
				result = ProcessAudioManagerRequest(request);

				break;
			}
		default:
			{
				g_audioLogger.Log(eAudioLogType_Error, "Unknown audio request type: %d", static_cast<int>(request.pData->type));
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
	if (m_pImpl != nullptr)
	{
		if (m_lastMainThreadFramId != gEnv->nMainFrameID)
		{
			g_lastMainThreadFrameStartTime = gEnv->pTimer->GetFrameStartTime();
			m_lastMainThreadFramId = gEnv->nMainFrameID;
		}

		m_audioListenerMgr.Update(deltaTime);
		m_audioEventMgr.Update(deltaTime);
		m_audioObjectMgr.Update(deltaTime, m_audioListenerMgr.GetDefaultListenerAttributes());
		m_fileCacheMgr.Update();

		m_pImpl->Update(deltaTime);
	}
}

///////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::GetAudioTriggerId(char const* const szAudioTriggerName, AudioControlId& audioTriggerId) const
{
	AudioControlId const tempAudioTriggerId = static_cast<AudioControlId const>(AudioStringToId(szAudioTriggerName));

	if (stl::find_in_map(m_triggers, tempAudioTriggerId, nullptr) != nullptr)
	{
		audioTriggerId = tempAudioTriggerId;
	}
	else
	{
		audioTriggerId = INVALID_AUDIO_CONTROL_ID;
	}

	return (audioTriggerId != INVALID_AUDIO_CONTROL_ID);
}

///////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::GetAudioRtpcId(char const* const szAudioRtpcName, AudioControlId& audioRtpcId) const
{
	AudioControlId const nRtpcID = static_cast<AudioControlId const>(AudioStringToId(szAudioRtpcName));

	if (stl::find_in_map(m_rtpcs, nRtpcID, nullptr) != nullptr)
	{
		audioRtpcId = nRtpcID;
	}
	else
	{
		audioRtpcId = INVALID_AUDIO_CONTROL_ID;
	}

	return (audioRtpcId != INVALID_AUDIO_CONTROL_ID);
}

///////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::GetAudioSwitchId(char const* const szAudioSwitchName, AudioControlId& audioSwitchId) const
{
	AudioControlId const switchId = static_cast<AudioControlId const>(AudioStringToId(szAudioSwitchName));

	if (stl::find_in_map(m_switches, switchId, nullptr) != nullptr)
	{
		audioSwitchId = switchId;
	}
	else
	{
		audioSwitchId = INVALID_AUDIO_CONTROL_ID;
	}

	return (audioSwitchId != INVALID_AUDIO_CONTROL_ID);
}

///////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::GetAudioSwitchStateId(
  AudioControlId const switchId,
  char const* const szAudioSwitchStateName,
  AudioSwitchStateId& audioSwitchStateId) const
{
	audioSwitchStateId = INVALID_AUDIO_CONTROL_ID;

	CATLSwitch const* const pSwitch = stl::find_in_map(m_switches, switchId, nullptr);

	if (pSwitch != nullptr)
	{
		AudioSwitchStateId const nStateID = static_cast<AudioSwitchStateId const>(AudioStringToId(szAudioSwitchStateName));

		CATLSwitchState const* const pState = stl::find_in_map(pSwitch->audioSwitchStates, nStateID, nullptr);

		if (pState != nullptr)
		{
			audioSwitchStateId = nStateID;
		}
	}

	return (audioSwitchStateId != INVALID_AUDIO_CONTROL_ID);
}

//////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::GetAudioPreloadRequestId(char const* const szAudioPreloadRequestName, AudioPreloadRequestId& audioPreloadRequestId) const
{
	AudioPreloadRequestId const nAudioPreloadRequestID = static_cast<AudioPreloadRequestId const>(AudioStringToId(szAudioPreloadRequestName));

	if (stl::find_in_map(m_preloadRequests, nAudioPreloadRequestID, nullptr) != nullptr)
	{
		audioPreloadRequestId = nAudioPreloadRequestID;
	}
	else
	{
		audioPreloadRequestId = INVALID_AUDIO_PRELOAD_REQUEST_ID;
	}

	return (audioPreloadRequestId != INVALID_AUDIO_PRELOAD_REQUEST_ID);
}

///////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::GetAudioEnvironmentId(char const* const szAudioEnvironmentName, AudioEnvironmentId& audioEnvironmentId) const
{
	AudioEnvironmentId const nEnvironmentID = static_cast<AudioControlId const>(AudioStringToId(szAudioEnvironmentName));

	if (stl::find_in_map(m_environments, nEnvironmentID, nullptr) != nullptr)
	{
		audioEnvironmentId = nEnvironmentID;
	}
	else
	{
		audioEnvironmentId = INVALID_AUDIO_CONTROL_ID;
	}

	return (audioEnvironmentId != INVALID_AUDIO_CONTROL_ID);
}

///////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::ReserveAudioObjectId(AudioObjectId& audioObjectId)
{
	return m_audioObjectMgr.ReserveId(audioObjectId);
}

///////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::ReleaseAudioObjectId(AudioObjectId const audioObjectId)
{
	return m_audioObjectMgr.ReleaseId(audioObjectId);
}

///////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::ReserveAudioListenerId(AudioObjectId& audioObjectId)
{
	return m_audioListenerMgr.ReserveId(audioObjectId);
}

///////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::ReleaseAudioListenerId(AudioObjectId const audioObjectId)
{
	return m_audioListenerMgr.ReleaseId(audioObjectId);
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::ParseControlsData(char const* const szFolderPath, EAudioDataScope const dataScope)
{
	m_xmlProcessor.ParseControlsData(szFolderPath, dataScope);

	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::ParsePreloadsData(char const* const szFolderPath, EAudioDataScope const dataScope)
{
	m_xmlProcessor.ParsePreloadsData(szFolderPath, dataScope);

	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::ClearControlsData(EAudioDataScope const dataScope)
{
	m_xmlProcessor.ClearControlsData(dataScope);

	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::ClearPreloadsData(EAudioDataScope const dataScope)
{
	m_xmlProcessor.ClearPreloadsData(dataScope);

	return eAudioRequestStatus_Success;
}

//////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::NotifyListener(CAudioRequestInternal const& request)
{
	AudioEnumFlagsType specificAudioRequest = INVALID_AUDIO_ENUM_FLAG_TYPE;
	AudioStandaloneFileId audioStandaloneFileID = INVALID_AUDIO_STANDALONE_FILE_ID;
	AudioControlId audioControlID = INVALID_AUDIO_CONTROL_ID;
	AudioEventId audioEventID = INVALID_AUDIO_EVENT_ID;
	char const* szValue = nullptr;

	switch (request.pData->type)
	{
	case eAudioRequestType_AudioManagerRequest:
		{
			SAudioManagerRequestDataInternalBase const* const pRequestDataBase = static_cast<SAudioManagerRequestDataInternalBase const* const>(request.pData.get());
			specificAudioRequest = static_cast<AudioEnumFlagsType>(pRequestDataBase->type);

			break;
		}
	case eAudioRequestType_AudioCallbackManagerRequest:
		{
			SAudioCallbackManagerRequestDataInternalBase const* const pRequestDataBase = static_cast<SAudioCallbackManagerRequestDataInternalBase const* const>(request.pData.get());
			specificAudioRequest = static_cast<AudioEnumFlagsType>(pRequestDataBase->type);

			switch (pRequestDataBase->type)
			{
			case eAudioCallbackManagerRequestType_ReportFinishedTriggerInstance:
				{
					SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportFinishedTriggerInstance> const* const pRequestData = static_cast<SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportFinishedTriggerInstance> const* const>(pRequestDataBase);
					audioControlID = pRequestData->audioTriggerId;

					break;
				}
			case eAudioCallbackManagerRequestType_ReportStartedEvent:
				{
					SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportStartedEvent> const* const pRequestData = static_cast<SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportStartedEvent> const* const>(pRequestDataBase);
					audioEventID = pRequestData->audioEventId;

					break;
				}
			case eAudioCallbackManagerRequestType_ReportStartedFile:
				{
					SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportStartedFile> const* const pRequestData = static_cast<SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportStartedFile> const* const>(pRequestDataBase);
					audioStandaloneFileID = pRequestData->audioStandaloneFileInstanceId;
					szValue = pRequestData->file.c_str();

					break;
				}
			case eAudioCallbackManagerRequestType_ReportStoppedFile:
				{
					SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportStoppedFile> const* const pRequestData = static_cast<SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportStoppedFile> const* const>(pRequestDataBase);
					audioStandaloneFileID = pRequestData->audioStandaloneFileInstanceId;
					szValue = pRequestData->file.c_str();

					break;
				}
			}

			break;
		}
	case eAudioRequestType_AudioObjectRequest:
		{
			SAudioObjectRequestDataInternalBase const* const pRequestDataBase = static_cast<SAudioObjectRequestDataInternalBase const* const>(request.pData.get());
			specificAudioRequest = static_cast<AudioEnumFlagsType>(pRequestDataBase->type);

			switch (pRequestDataBase->type)
			{
			case eAudioObjectRequestType_ExecuteTrigger:
				{
					SAudioObjectRequestDataInternal<eAudioObjectRequestType_ExecuteTrigger> const* const pRequestData = static_cast<SAudioObjectRequestDataInternal<eAudioObjectRequestType_ExecuteTrigger> const* const>(pRequestDataBase);
					audioControlID = pRequestData->audioTriggerId;

					break;
				}
			}

			break;
		}
	case eAudioRequestType_AudioListenerRequest:
		{
			SAudioListenerRequestDataInternalBase const* const pRequestDataBase = static_cast<SAudioListenerRequestDataInternalBase const* const>(request.pData.get());
			specificAudioRequest = static_cast<AudioEnumFlagsType>(pRequestDataBase->type);

			break;
		}
	default:
		{
			CryFatalError("Unknown request type during CAudioTranslationLayer::NotifyListener!");

			break;
		}
	}

	SAudioRequestInfo const requestInfo(
	  ConvertToRequestResult(request.status),
	  request.pOwner,
	  request.pUserData,
	  request.pUserDataOwner,
	  szValue,
	  request.pData->type,
	  specificAudioRequest,
	  audioControlID,
	  request.audioObjectId,
	  audioStandaloneFileID,
	  audioEventID);

	m_audioEventListenerMgr.NotifyListener(&requestInfo);
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::ProcessAudioManagerRequest(CAudioRequestInternal const& request)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;
	SAudioManagerRequestDataInternalBase const* const pRequestDataBase = static_cast<SAudioManagerRequestDataInternalBase const*>(request.pData.get());

	switch (pRequestDataBase->type)
	{
	case eAudioManagerRequestType_ReserveAudioObjectId:
		{
			SAudioManagerRequestDataInternal<eAudioManagerRequestType_ReserveAudioObjectId> const* const pRequestData = static_cast<SAudioManagerRequestDataInternal<eAudioManagerRequestType_ReserveAudioObjectId> const*>(request.pData.get());
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			result = BoolToARS(ReserveAudioObjectId(*pRequestData->pAudioObjectId, pRequestData->audioObjectName.c_str()));
#else
			result = BoolToARS(ReserveAudioObjectId(*pRequestData->pAudioObjectId));
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
			break;
		}
	case eAudioManagerRequestType_AddRequestListener:
		{
			SAudioManagerRequestDataInternal<eAudioManagerRequestType_AddRequestListener> const* const pRequestData = static_cast<SAudioManagerRequestDataInternal<eAudioManagerRequestType_AddRequestListener> const*>(request.pData.get());
			result = m_audioEventListenerMgr.AddRequestListener(pRequestData);

			break;
		}
	case eAudioManagerRequestType_RemoveRequestListener:
		{
			SAudioManagerRequestDataInternal<eAudioManagerRequestType_RemoveRequestListener> const* const pRequestData = static_cast<SAudioManagerRequestDataInternal<eAudioManagerRequestType_RemoveRequestListener> const*>(request.pData.get());
			result = m_audioEventListenerMgr.RemoveRequestListener(pRequestData->func, pRequestData->pObjectToListenTo);

			break;
		}
	case eAudioManagerRequestType_SetAudioImpl:
		{
			SAudioManagerRequestDataInternal<eAudioManagerRequestType_SetAudioImpl> const* const pRequestData = static_cast<SAudioManagerRequestDataInternal<eAudioManagerRequestType_SetAudioImpl> const*>(request.pData.get());
			result = SetImpl(pRequestData->pImpl);

			break;
		}
	case eAudioManagerRequestType_RefreshAudioSystem:
		{
			SAudioManagerRequestDataInternal<eAudioManagerRequestType_RefreshAudioSystem> const* const pRequestData = static_cast<SAudioManagerRequestDataInternal<eAudioManagerRequestType_RefreshAudioSystem> const*>(request.pData.get());
			result = RefreshAudioSystem(pRequestData->levelName);

			break;
		}
	case eAudioManagerRequestType_LoseFocus:
		{
			if (
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			  (g_audioCVars.m_ignoreWindowFocus == 0) &&
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
			  (m_flags & eAudioInternalStates_IsMuted) == 0)
			{
				CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, SATLInternalControlIDs::loseFocusTriggerId, nullptr);

				if (pTrigger != nullptr)
				{
					ActivateTrigger(m_pGlobalAudioObject, pTrigger, 0.0f);
				}
				else
				{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
					g_audioLogger.Log(eAudioLogType_Warning, "Could not find definition of lose focus trigger \"%s\"", m_debugNameStore.LookupAudioTriggerName(SATLInternalControlIDs::loseFocusTriggerId));
#else
					g_audioLogger.Log(eAudioLogType_Warning, "Could not find definition of lose focus trigger");
#endif
				}

				result = m_pImpl->OnLoseFocus();
			}

			break;
		}
	case eAudioManagerRequestType_GetFocus:
		{
			if (
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			  (g_audioCVars.m_ignoreWindowFocus == 0) &&
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
			  (m_flags & eAudioInternalStates_IsMuted) == 0)
			{
				result = m_pImpl->OnGetFocus();

				CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, SATLInternalControlIDs::getFocusTriggerId, nullptr);

				if (pTrigger != nullptr)
				{
					ActivateTrigger(m_pGlobalAudioObject, pTrigger, 0.0f);
				}
				else
				{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
					g_audioLogger.Log(eAudioLogType_Warning, "Could not find definition of get focus trigger \"%s\"", m_debugNameStore.LookupAudioTriggerName(SATLInternalControlIDs::getFocusTriggerId));
#else
					g_audioLogger.Log(eAudioLogType_Warning, "Could not find definition of get focus trigger");
#endif
				}
			}

			break;
		}
	case eAudioManagerRequestType_MuteAll:
		{
			CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, SATLInternalControlIDs::muteAllTriggerId, nullptr);

			if (pTrigger != nullptr)
			{
				ActivateTrigger(m_pGlobalAudioObject, pTrigger, 0.0f);
			}
			else
			{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
				g_audioLogger.Log(eAudioLogType_Warning, "Could not find definition of mute all trigger \"%s\"", m_debugNameStore.LookupAudioTriggerName(SATLInternalControlIDs::muteAllTriggerId));
#else
				g_audioLogger.Log(eAudioLogType_Warning, "Could not find definition of mute all trigger");
#endif
			}

			result = m_pImpl->MuteAll();

			if (result == eAudioRequestStatus_Success)
			{
				m_flags |= eAudioInternalStates_IsMuted;
			}
			else
			{
				m_flags &= ~eAudioInternalStates_IsMuted;
			}

			break;
		}
	case eAudioManagerRequestType_UnmuteAll:
		{
			result = m_pImpl->UnmuteAll();

			if (result != eAudioRequestStatus_Success)
			{
				m_flags |= eAudioInternalStates_IsMuted;
			}
			else
			{
				m_flags &= ~eAudioInternalStates_IsMuted;
			}

			CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, SATLInternalControlIDs::unmuteAllTriggerId, nullptr);

			if (pTrigger != nullptr)
			{
				ActivateTrigger(m_pGlobalAudioObject, pTrigger, 0.0f);
			}
			else
			{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
				g_audioLogger.Log(eAudioLogType_Warning, "Could not find definition of unmute trigger \"%s\"", m_debugNameStore.LookupAudioTriggerName(SATLInternalControlIDs::unmuteAllTriggerId));
#else
				g_audioLogger.Log(eAudioLogType_Warning, "Could not find definition of unmute trigger");
#endif
			}

			break;
		}
	case eAudioManagerRequestType_StopAllSounds:
		{
			result = m_pImpl->StopAllSounds();

			break;
		}
	case eAudioManagerRequestType_ParseControlsData:
		{
			SAudioManagerRequestDataInternal<eAudioManagerRequestType_ParseControlsData> const* const pRequestData = static_cast<SAudioManagerRequestDataInternal<eAudioManagerRequestType_ParseControlsData> const*>(request.pData.get());
			result = ParseControlsData(pRequestData->folderPath.c_str(), pRequestData->dataScope);

			break;
		}
	case eAudioManagerRequestType_ParsePreloadsData:
		{
			SAudioManagerRequestDataInternal<eAudioManagerRequestType_ParsePreloadsData> const* const pRequestData = static_cast<SAudioManagerRequestDataInternal<eAudioManagerRequestType_ParsePreloadsData> const*>(request.pData.get());
			result = ParsePreloadsData(pRequestData->folderPath.c_str(), pRequestData->dataScope);

			break;
		}
	case eAudioManagerRequestType_ClearControlsData:
		{
			SAudioManagerRequestDataInternal<eAudioManagerRequestType_ClearControlsData> const* const pRequestData = static_cast<SAudioManagerRequestDataInternal<eAudioManagerRequestType_ClearControlsData> const*>(request.pData.get());
			result = ClearControlsData(pRequestData->dataScope);

			break;
		}
	case eAudioManagerRequestType_ClearPreloadsData:
		{
			SAudioManagerRequestDataInternal<eAudioManagerRequestType_ClearPreloadsData> const* const pRequestData = static_cast<SAudioManagerRequestDataInternal<eAudioManagerRequestType_ClearPreloadsData> const*>(request.pData.get());
			result = ClearPreloadsData(pRequestData->dataScope);

			break;
		}
	case eAudioManagerRequestType_PreloadSingleRequest:
		{
			SAudioManagerRequestDataInternal<eAudioManagerRequestType_PreloadSingleRequest> const* const pRequestData = static_cast<SAudioManagerRequestDataInternal<eAudioManagerRequestType_PreloadSingleRequest> const*>(request.pData.get());
			result = m_fileCacheMgr.TryLoadRequest(pRequestData->audioPreloadRequestId, ((request.flags & eAudioRequestFlags_ExecuteBlocking) != 0), pRequestData->bAutoLoadOnly);

			break;
		}
	case eAudioManagerRequestType_UnloadSingleRequest:
		{
			SAudioManagerRequestDataInternal<eAudioManagerRequestType_UnloadSingleRequest> const* const pRequestData = static_cast<SAudioManagerRequestDataInternal<eAudioManagerRequestType_UnloadSingleRequest> const*>(request.pData.get());
			result = m_fileCacheMgr.TryUnloadRequest(pRequestData->audioPreloadRequestId);

			break;
		}
	case eAudioManagerRequestType_UnloadAFCMDataByScope:
		{
			SAudioManagerRequestDataInternal<eAudioManagerRequestType_UnloadAFCMDataByScope> const* const pRequestData = static_cast<SAudioManagerRequestDataInternal<eAudioManagerRequestType_UnloadAFCMDataByScope> const*>(request.pData.get());
			result = m_fileCacheMgr.UnloadDataByScope(pRequestData->dataScope);

			break;
		}
	case eAudioManagerRequestType_ReleaseAudioImpl:
		{
			ReleaseImpl();
			result = eAudioRequestStatus_Success;

			break;
		}
	case eAudioManagerRequestType_ChangeLanguage:
		{
			SetImplLanguage();

			m_fileCacheMgr.UpdateLocalizedFileCacheEntries();
			result = eAudioRequestStatus_Success;

			break;
		}
	case eAudioManagerRequestType_RetriggerAudioControls:
		{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			RetriggerAudioControls();
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
			result = eAudioRequestStatus_Success;

			break;
		}
	case eAudioManagerRequestType_ReleasePendingRays:
		{
			m_audioObjectMgr.ReleasePendingRays();
			result = eAudioRequestStatus_Success;

			break;
		}
	case eAudioManagerRequestType_ReloadControlsData:
		{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			SAudioManagerRequestDataInternal<eAudioManagerRequestType_ReloadControlsData> const* const pRequestData = static_cast<SAudioManagerRequestDataInternal<eAudioManagerRequestType_ReloadControlsData> const*>(request.pData.get());
			for (auto const& audioObjectPair : m_audioObjectMgr.GetRegisteredAudioObjects())
			{
				CATLAudioObject* pAudioObject = audioObjectPair.second;
				for (auto pEvent : pAudioObject->GetActiveEvents())
				{
					if (pEvent != nullptr)
					{
						pEvent->m_pTrigger = nullptr; // we set the pointer to null because we are reloading all the triggers again
						switch (pEvent->m_audioSubsystem)
						{
						case eAudioSubsystem_AudioImpl:
							{
								result = m_pImpl->StopEvent(pAudioObject->GetImplDataPtr(), pEvent->m_pImplData);
								break;
							}
						}
					}
				}
			}

			if (ClearControlsData(eAudioDataScope_All) == eAudioRequestStatus_Success)
			{
				if (ParseControlsData(pRequestData->folderPath, eAudioDataScope_Global) == eAudioRequestStatus_Success)
				{
					if (strcmp(pRequestData->levelName, "") != 0)
					{
						if (ParseControlsData(pRequestData->folderPath + pRequestData->levelName, eAudioDataScope_LevelSpecific) == eAudioRequestStatus_Success)
						{
							RetriggerAudioControls();
							result = eAudioRequestStatus_Success;
						}
					}
					else
					{
						RetriggerAudioControls();
						result = eAudioRequestStatus_Success;
					}
				}
			}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
			break;
		}
	case eAudioManagerRequestType_DrawDebugInfo:
		{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			DrawAudioSystemDebugInfo();
			result = eAudioRequestStatus_Success;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

			break;
		}
	case eAudioManagerRequestType_GetAudioFileData:
		{
			SAudioManagerRequestDataInternal<eAudioManagerRequestType_GetAudioFileData> const* const pRequestData =
			  static_cast<SAudioManagerRequestDataInternal<eAudioManagerRequestType_GetAudioFileData> const* const>(request.pData.get());
			m_pImpl->GetAudioFileData(pRequestData->szFilename, pRequestData->audioFileData);
			break;
		}
	case eAudioManagerRequestType_None:
		{
			result = eAudioRequestStatus_Success;

			break;
		}
	default:
		{
			g_audioLogger.Log(eAudioLogType_Warning, "ATL received an unknown AudioManager request: %u", pRequestDataBase->type);
			result = eAudioRequestStatus_FailureInvalidRequest;

			break;
		}
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::ProcessAudioCallbackManagerRequest(CAudioRequestInternal& request)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;
	SAudioCallbackManagerRequestDataInternalBase const* const pRequestDataBase =
	  static_cast<SAudioCallbackManagerRequestDataInternalBase const* const>(request.pData.get());

	switch (pRequestDataBase->type)
	{
	case eAudioCallbackManagerRequestType_ReportStartedEvent:
		{
			SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportStartedEvent> const* const pRequestData =
			  static_cast<SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportStartedEvent> const* const>(request.pData.get());
			CATLEvent* const pEvent = m_audioEventMgr.LookupId(pRequestData->audioEventId);

			if (pEvent != nullptr)
			{
				pEvent->m_audioEventState = eAudioEventState_PlayingDelayed;

				if (pEvent->m_audioObjectId != m_globalAudioObjectId)
				{
					m_audioObjectMgr.ReportStartedEvent(pEvent);
				}
				else
				{
					m_pGlobalAudioObject->ReportStartedEvent(pEvent);
				}
			}

			result = eAudioRequestStatus_Success;

			break;
		}
	case eAudioCallbackManagerRequestType_ReportFinishedEvent:
		{
			SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportFinishedEvent> const* const pRequestData =
			  static_cast<SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportFinishedEvent> const* const>(request.pData.get());
			CATLEvent* const pEvent = m_audioEventMgr.LookupId(pRequestData->audioEventId);

			if (pEvent != nullptr)
			{
				if (pEvent->m_audioObjectId != m_globalAudioObjectId)
				{
					m_audioObjectMgr.ReportFinishedEvent(pEvent, pRequestData->bSuccess);
				}
				else
				{
					m_pGlobalAudioObject->ReportFinishedEvent(pEvent, pRequestData->bSuccess);
				}

				m_audioEventMgr.ReleaseEvent(pEvent);
			}

			result = eAudioRequestStatus_Success;

			break;
		}
	case eAudioCallbackManagerRequestType_ReportVirtualizedEvent:
		{
			SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportVirtualizedEvent> const* const pRequestData =
			  static_cast<SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportVirtualizedEvent> const* const>(request.pData.get());
			CATLEvent* const pEvent = m_audioEventMgr.LookupId(pRequestData->audioEventId);

			if (pEvent != nullptr)
			{
				pEvent->m_audioEventState = eAudioEventState_Virtual;
			}

			result = eAudioRequestStatus_Success;

			break;
		}
	case eAudioCallbackManagerRequestType_ReportPhysicalizedEvent:
		{
			SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportPhysicalizedEvent> const* const pRequestData =
			  static_cast<SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportPhysicalizedEvent> const* const>(request.pData.get());
			CATLEvent* const pEvent = m_audioEventMgr.LookupId(pRequestData->audioEventId);

			if (pEvent != nullptr)
			{
				pEvent->m_audioEventState = eAudioEventState_Playing;
			}

			result = eAudioRequestStatus_Success;

			break;
		}
	case eAudioCallbackManagerRequestType_ReportProcessedObstructionRay:
		{
			SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportProcessedObstructionRay> const* const pRequestData =
			  static_cast<SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportProcessedObstructionRay> const* const>(request.pData.get());

			m_audioObjectMgr.ReportObstructionRay(pRequestData->audioObjectId, pRequestData->rayId);

			result = eAudioRequestStatus_Success;

			break;
		}
	case eAudioCallbackManagerRequestType_ReportStartedFile:
		{
			SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportStartedFile> const* const pRequestData =
			  static_cast<SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportStartedFile> const* const>(request.pData.get());
			CATLStandaloneFile* const pAudioStandaloneFile = m_audioStandaloneFileMgr.LookupId(pRequestData->audioStandaloneFileInstanceId);

			if (pAudioStandaloneFile != nullptr)
			{
				m_audioObjectMgr.GetStartedStandaloneFileRequestData(pAudioStandaloneFile, request);
				pAudioStandaloneFile->m_state = (pRequestData->bSuccess) ? eAudioStandaloneFileState_Playing : eAudioStandaloneFileState_None;
			}

			result = (pRequestData->bSuccess) ? eAudioRequestStatus_Success : eAudioRequestStatus_Failure;

			break;
		}
	case eAudioCallbackManagerRequestType_ReportStoppedFile:
		{
			SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportStoppedFile> const* const pRequestData =
			  static_cast<SAudioCallbackManagerRequestDataInternal<eAudioCallbackManagerRequestType_ReportStoppedFile> const* const>(request.pData.get());
			CATLStandaloneFile* const pAudioStandaloneFile = m_audioStandaloneFileMgr.LookupId(pRequestData->audioStandaloneFileInstanceId);

			if (pAudioStandaloneFile != nullptr)
			{
				m_audioObjectMgr.GetStartedStandaloneFileRequestData(pAudioStandaloneFile, request);

				if (pAudioStandaloneFile->m_audioObjectId != m_globalAudioObjectId)
				{
					m_audioObjectMgr.ReportFinishedStandaloneFile(pAudioStandaloneFile);
				}
				else
				{
					m_pGlobalAudioObject->ReportFinishedStandaloneFile(pAudioStandaloneFile);
				}

				m_audioStandaloneFileMgr.ReleaseStandaloneFile(pAudioStandaloneFile);
			}

			result = eAudioRequestStatus_Success;

			break;
		}
	case eAudioCallbackManagerRequestType_ReportFinishedTriggerInstance:
	case eAudioCallbackManagerRequestType_None:
		{
			result = eAudioRequestStatus_Success;

			break;
		}
	default:
		{
			result = eAudioRequestStatus_FailureInvalidRequest;
			g_audioLogger.Log(eAudioLogType_Warning, "ATL received an unknown AudioCallbackManager request: %u", pRequestDataBase->type);

			break;
		}
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::ProcessAudioObjectRequest(CAudioRequestInternal const& request)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;
	CATLAudioObject* pObject = m_pGlobalAudioObject;

	if (request.audioObjectId != INVALID_AUDIO_OBJECT_ID)
	{
		pObject = m_audioObjectMgr.LookupId(request.audioObjectId);
	}

	if (pObject != nullptr)
	{
		SAudioObjectRequestDataInternalBase const* const pBaseRequestData =
		  static_cast<SAudioObjectRequestDataInternalBase const* const>(request.pData.get());

		switch (pBaseRequestData->type)
		{
		case eAudioObjectRequestType_PrepareTrigger:
			{
				SAudioObjectRequestDataInternal<eAudioObjectRequestType_PrepareTrigger> const* const pRequestData =
				  static_cast<SAudioObjectRequestDataInternal<eAudioObjectRequestType_PrepareTrigger> const* const>(request.pData.get());

				CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, pRequestData->audioTriggerId, nullptr);

				if (pTrigger != nullptr)
				{
					result = PrepUnprepTriggerAsync(pObject, pTrigger, true);
				}
				else
				{
					result = eAudioRequestStatus_FailureInvalidControlId;
				}

				break;
			}
		case eAudioObjectRequestType_UnprepareTrigger:
			{
				SAudioObjectRequestDataInternal<eAudioObjectRequestType_UnprepareTrigger> const* const pRequestData =
				  static_cast<SAudioObjectRequestDataInternal<eAudioObjectRequestType_UnprepareTrigger> const* const>(request.pData.get());

				CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, pRequestData->audioTriggerId, nullptr);

				if (pTrigger != nullptr)
				{
					result = PrepUnprepTriggerAsync(pObject, pTrigger, false);
				}
				else
				{
					result = eAudioRequestStatus_FailureInvalidControlId;
				}

				break;
			}
		case eAudioObjectRequestType_PlayFile:
			{
				SAudioObjectRequestDataInternal<eAudioObjectRequestType_PlayFile> const* const pRequestData =
				  static_cast<SAudioObjectRequestDataInternal<eAudioObjectRequestType_PlayFile> const* const>(request.pData.get());

				if (pRequestData != nullptr && !pRequestData->file.empty())
				{
					result = PlayFile(
					  pObject,
					  pRequestData->file.c_str(),
					  pRequestData->usedAudioTriggerId,
					  pRequestData->bLocalized,
					  request.pOwner,
					  request.pUserData,
					  request.pUserDataOwner);
				}

				break;
			}
		case eAudioObjectRequestType_StopFile:
			{
				SAudioObjectRequestDataInternal<eAudioObjectRequestType_StopFile> const* const pRequestData =
				  static_cast<SAudioObjectRequestDataInternal<eAudioObjectRequestType_StopFile> const* const>(request.pData.get());

				if (pRequestData != nullptr && !pRequestData->file.empty())
				{
					result = StopFile(pObject, pRequestData->file.c_str());
				}

				break;
			}
		case eAudioObjectRequestType_ExecuteTrigger:
			{
				SAudioObjectRequestDataInternal<eAudioObjectRequestType_ExecuteTrigger> const* const pRequestData =
				  static_cast<SAudioObjectRequestDataInternal<eAudioObjectRequestType_ExecuteTrigger> const* const>(request.pData.get());

				CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, pRequestData->audioTriggerId, nullptr);

				if (pTrigger != nullptr)
				{
					result = ActivateTrigger(
					  pObject,
					  pTrigger,
					  pRequestData->timeUntilRemovalInMS,
					  request.pOwner,
					  request.pUserData,
					  request.pUserDataOwner,
					  request.flags);
				}
				else
				{
					result = eAudioRequestStatus_FailureInvalidControlId;
				}

				break;
			}
		case eAudioObjectRequestType_StopTrigger:
			{
				SAudioObjectRequestDataInternal<eAudioObjectRequestType_StopTrigger> const* const pRequestData =
				  static_cast<SAudioObjectRequestDataInternal<eAudioObjectRequestType_StopTrigger> const* const>(request.pData.get());

				CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, pRequestData->audioTriggerId, nullptr);

				if (pTrigger != nullptr)
				{
					result = StopTrigger(pObject, pTrigger);
				}
				else
				{
					result = eAudioRequestStatus_FailureInvalidControlId;
				}

				break;
			}
		case eAudioObjectRequestType_StopAllTriggers:
			{
				StopAllTriggers(pObject);

				result = eAudioRequestStatus_Success;

				break;
			}
		case eAudioObjectRequestType_SetTransformation:
			{
				if (pObject != m_pGlobalAudioObject)
				{
					SAudioObjectRequestDataInternal<eAudioObjectRequestType_SetTransformation> const* const pRequestData =
					  static_cast<SAudioObjectRequestDataInternal<eAudioObjectRequestType_SetTransformation> const* const>(request.pData.get());

					pObject->SetTransformation(pRequestData->transformation);
					result = m_pImpl->Set3DAttributes(pObject->GetImplDataPtr(), pObject->Get3DAttributes());
				}
				else
				{
					g_audioLogger.Log(eAudioLogType_Warning, "ATL received a request to set a position on a global object");
				}

				break;
			}
		case eAudioObjectRequestType_SetRtpcValue:
			{
				result = eAudioRequestStatus_FailureInvalidControlId;

				SAudioObjectRequestDataInternal<eAudioObjectRequestType_SetRtpcValue> const* const pRequestData =
				  static_cast<SAudioObjectRequestDataInternal<eAudioObjectRequestType_SetRtpcValue> const* const>(request.pData.get());

				CATLRtpc const* const pRtpc = stl::find_in_map(m_rtpcs, pRequestData->audioRtpcId, nullptr);

				if (pRtpc != nullptr)
				{
					result = SetRtpc(pObject, pRtpc, pRequestData->value);
				}

				break;
			}
		case eAudioObjectRequestType_SetSwitchState:
			{
				result = eAudioRequestStatus_FailureInvalidControlId;
				SAudioObjectRequestDataInternal<eAudioObjectRequestType_SetSwitchState> const* const pRequestData =
				  static_cast<SAudioObjectRequestDataInternal<eAudioObjectRequestType_SetSwitchState> const* const>(request.pData.get());

				CATLSwitch const* const pSwitch = stl::find_in_map(m_switches, pRequestData->audioSwitchId, nullptr);

				if (pSwitch != nullptr)
				{
					CATLSwitchState const* const pState = stl::find_in_map(pSwitch->audioSwitchStates, pRequestData->audioSwitchStateId, nullptr);

					if (pState != nullptr)
					{
						result = SetSwitchState(pObject, pState);
					}
				}

				break;
			}
		case eAudioObjectRequestType_SetVolume:
			{
				result = eAudioRequestStatus_FailureInvalidControlId;
				//TODO

				break;
			}
		case eAudioObjectRequestType_SetEnvironmentAmount:
			{
				if (pObject != m_pGlobalAudioObject)
				{
					result = eAudioRequestStatus_FailureInvalidControlId;
					SAudioObjectRequestDataInternal<eAudioObjectRequestType_SetEnvironmentAmount> const* const pRequestData =
					  static_cast<SAudioObjectRequestDataInternal<eAudioObjectRequestType_SetEnvironmentAmount> const* const>(request.pData.get());

					CATLAudioEnvironment const* const pEnvironment = stl::find_in_map(m_environments, pRequestData->audioEnvironmentId, nullptr);

					if (pEnvironment != nullptr)
					{
						result = SetEnvironment(pObject, pEnvironment, pRequestData->amount);
					}
				}
				else
				{
					g_audioLogger.Log(eAudioLogType_Warning, "ATL received a request to set an environment on a global object");
				}

				break;
			}
		case eAudioObjectRequestType_ResetEnvironments:
			{
				result = ResetEnvironments(pObject);
				break;
			}
		case eAudioObjectRequestType_ReleaseObject:
			{
				result = eAudioRequestStatus_Failure;

				AudioObjectId const nObjectID = pObject->GetId();

				if (nObjectID != m_globalAudioObjectId)
				{
					if (ReleaseAudioObjectId(nObjectID))
					{
						result = eAudioRequestStatus_Success;
					}
				}
				else
				{
					g_audioLogger.Log(eAudioLogType_Warning, "ATL received a request to release the GlobalAudioObject");
				}

				break;
			}
		case eAudioObjectRequestType_None:
			{
				result = eAudioRequestStatus_Success;

				break;
			}
		default:
			{
				result = eAudioRequestStatus_FailureInvalidRequest;
				g_audioLogger.Log(eAudioLogType_Warning, "ATL received an unknown AudioObject request type: %u", pBaseRequestData->type);

				break;
			}
		}
	}
	else
	{
		g_audioLogger.Log(eAudioLogType_Warning, "ATL received a request to a non-existent AudioObject: %u", request.audioObjectId);
		result = eAudioRequestStatus_FailureInvalidObjectId;
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::ProcessAudioListenertRequest(CATLListenerObject* const pListener, SAudioRequestDataInternal const* const pPassedRequestData)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;
	SAudioListenerRequestDataInternalBase const* const pBaseRequestData =
	  static_cast<SAudioListenerRequestDataInternalBase const* const>(pPassedRequestData);

	switch (pBaseRequestData->type)
	{
	case eAudioListenerRequestType_SetTransformation:
		{
			SAudioListenerRequestDataInternal<eAudioListenerRequestType_SetTransformation> const* const pRequestData =
			  static_cast<SAudioListenerRequestDataInternal<eAudioListenerRequestType_SetTransformation> const* const>(pPassedRequestData);

			pListener->SetTransformation(pRequestData->transformation);
			result = m_pImpl->SetListener3DAttributes(pListener->m_pImplData, pListener->Get3DAttributes());
		}
		break;
	case eAudioListenerRequestType_None:
		result = eAudioRequestStatus_Success;
		break;
	default:
		result = eAudioRequestStatus_FailureInvalidRequest;
		g_audioLogger.Log(eAudioLogType_Warning, "ATL received an unknown AudioListener request type: %u", pBaseRequestData->type);
		break;
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::SetImpl(IAudioImpl* const pImpl)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	if (m_pImpl != nullptr && pImpl != m_pImpl)
	{
		ReleaseImpl();
	}

	m_pImpl = pImpl;

	if (m_pImpl == nullptr)
	{
		g_audioLogger.Log(eAudioLogType_Warning, "nullptr passed to SetImpl, will run with the null implementation");

		POOL_NEW_CREATE(CAudioImpl_null, pAudioImpl_null);
		CRY_ASSERT(pAudioImpl_null != nullptr);
		m_pImpl = pAudioImpl_null;
	}

	result = m_pImpl->Init();

	if (result != eAudioRequestStatus_Success)
	{
		// The impl failed to initialize, allow it to shut down and release then fall back to the null impl.
		g_audioLogger.Log(eAudioLogType_Error, "Failed to set the AudioImpl %s. Will run with the null implementation.", m_pImpl->GetImplementationNameString());

		EAudioRequestStatus result = m_pImpl->ShutDown(); // Shut down the audio middleware.
		CRY_ASSERT(result == eAudioRequestStatus_Success);

		result = m_pImpl->Release(); // Release the engine specific data.
		CRY_ASSERT(result == eAudioRequestStatus_Success);

		POOL_NEW_CREATE(CAudioImpl_null, pAudioImpl_null);
		CRY_ASSERT(pAudioImpl_null != nullptr);
		m_pImpl = pAudioImpl_null;
	}

	IAudioObject* const pGlobalObjectData = m_pImpl->NewGlobalAudioObject(m_globalAudioObjectId);
	m_pGlobalAudioObject->SetImplDataPtr(pGlobalObjectData);

	m_audioObjectMgr.Init(m_pImpl);
	m_audioEventMgr.Init(m_pImpl);
	m_audioStandaloneFileMgr.Init(m_pImpl);
	m_audioListenerMgr.Init(m_pImpl);
	m_xmlProcessor.Init(m_pImpl);
	m_fileCacheMgr.Init(m_pImpl);

	SetImplLanguage();

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	m_implNameString = m_pImpl->GetImplementationNameString();
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

	return result;
}

///////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::ReleaseImpl()
{
	// During audio middleware shutdown we do not allow for any new requests originating from the "dying" audio middleware!
	m_flags |= eAudioInternalStates_AudioMiddlewareShuttingDown;

	m_pImpl->ResetAudioObject(m_pGlobalAudioObject->GetImplDataPtr());
	m_pImpl->DeleteAudioObject(m_pGlobalAudioObject->GetImplDataPtr());
	m_pGlobalAudioObject->SetImplDataPtr(nullptr);

	m_audioStandaloneFileMgr.Release();
	m_audioEventMgr.Release();
	m_audioObjectMgr.Release();
	m_audioListenerMgr.Release();

	m_xmlProcessor.ClearControlsData(eAudioDataScope_All);
	m_xmlProcessor.ClearPreloadsData(eAudioDataScope_All);

	m_fileCacheMgr.Release();

	EAudioRequestStatus result = m_pImpl->ShutDown(); // Shut down the audio middleware.
	CRY_ASSERT(result == eAudioRequestStatus_Success);

	result = m_pImpl->Release(); // Release the engine specific data.
	CRY_ASSERT(result == eAudioRequestStatus_Success);

	m_pImpl = nullptr;
	m_flags &= ~eAudioInternalStates_AudioMiddlewareShuttingDown;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::PlayFile(
  CATLAudioObject* const _pAudioObject,
  char const* const _szFile,
  AudioControlId const _triggerId,
  bool const _bLocalized,
  void* const _pOwner,
  void* const _pUserData,
  void* const _pUserDataOwner)
{
	EAudioRequestStatus status = eAudioRequestStatus_Failure;
	CATLStandaloneFile* const pStandaloneFile = m_audioStandaloneFileMgr.GetStandaloneFile(_szFile);

	if (pStandaloneFile != nullptr)
	{
		SAudioStandaloneFileInfo fileInfo;
		fileInfo.fileId = pStandaloneFile->m_id;
		fileInfo.fileInstanceId = pStandaloneFile->GetId();
		fileInfo.pAudioObject = _pAudioObject->GetImplDataPtr();
		fileInfo.pImplData = pStandaloneFile->m_pImplData;
		fileInfo.szFileName = _szFile;
		fileInfo.bLocalized = _bLocalized;

		if (!m_pDefaultStandaloneFileTrigger)
		{
			ICVar* const pCVar = gEnv->pConsole->GetCVar("s_DefaultStandaloneFilesAudioTrigger");
			string const defaultTriggerName = (pCVar) ? pCVar->GetString() : "";
			m_pDefaultStandaloneFileTrigger = stl::find_in_map(m_triggers, static_cast<AudioControlId const>(AudioStringToId(defaultTriggerName)), nullptr);
		}

		CATLTrigger const* pAudioTrigger = (_triggerId == INVALID_AUDIO_CONTROL_ID) ? m_pDefaultStandaloneFileTrigger : stl::find_in_map(m_triggers, _triggerId, nullptr);

		if (pAudioTrigger != nullptr)
		{
			size_t const numAudioTriggers = pAudioTrigger->m_implPtrs.size();
			if (numAudioTriggers == 1)
			{
				fileInfo.pUsedAudioTrigger = pAudioTrigger->m_implPtrs[0]->m_pImplData;
			}
			else if (numAudioTriggers == 0)
			{
				g_audioLogger.Log(eAudioLogType_Warning, " PlayFile \"%s\": The given AudioTrigger with ID '%u' did not contain any connected ImplementationTrigger", _szFile, _triggerId);
			}
			else
			{
				fileInfo.pUsedAudioTrigger = pAudioTrigger->m_implPtrs[0]->m_pImplData;
				g_audioLogger.Log(eAudioLogType_Warning, " PlayFile \"%s\": The given AudioTrigger with ID '%u' did contain more than one ImplementationTrigger", _szFile, _triggerId);
			}
		}
		else
		{
			g_audioLogger.Log(eAudioLogType_Warning, " PlayFile \"%s\": The given AudioTrigger with ID '%u' did not exist", _szFile, _triggerId);
		}

		EAudioRequestStatus const tempStatus = m_pImpl->PlayFile(&fileInfo);

		if (tempStatus == eAudioRequestStatus_Success || tempStatus == eAudioRequestStatus_Pending)
		{
			if (tempStatus == eAudioRequestStatus_Success)
			{
				pStandaloneFile->m_state = eAudioStandaloneFileState_Playing;
			}
			else if (tempStatus == eAudioRequestStatus_Pending)
			{
				pStandaloneFile->m_state = eAudioStandaloneFileState_Loading;
			}

			pStandaloneFile->m_id = fileInfo.fileId;
			pStandaloneFile->m_audioObjectId = _pAudioObject->GetId();
			pStandaloneFile->m_pImplData = fileInfo.pImplData;

			_pAudioObject->ReportStartedStandaloneFile(pStandaloneFile, _pOwner, _pUserData, _pUserDataOwner);

			// It's a success in both cases.
			status = eAudioRequestStatus_Success;
		}
		else
		{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			g_audioLogger.Log(eAudioLogType_Warning, "PlayFile failed with \"%s\" (InstanceID: %u) on AudioObject \"%s\" (ID: %u)", _szFile, fileInfo.fileInstanceId, m_debugNameStore.LookupAudioObjectName(_pAudioObject->GetId()), _pAudioObject->GetId());
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

			m_audioStandaloneFileMgr.ReleaseStandaloneFile(pStandaloneFile);
		}
	}
	else
	{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		g_audioLogger.Log(eAudioLogType_Warning, "PlayFile failed with \"%s\" on AudioObject \"%s\" (ID: %u)", _szFile, m_debugNameStore.LookupAudioObjectName(_pAudioObject->GetId()), _pAudioObject->GetId());
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

		m_audioStandaloneFileMgr.ReleaseStandaloneFile(pStandaloneFile);
	}

	return status;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::StopFile(
  CATLAudioObject* const pAudioObject,
  char const* const szFile)
{
	EAudioRequestStatus status = eAudioRequestStatus_Failure;
	AudioStandaloneFileId const fileID = static_cast<AudioStandaloneFileId>(AudioStringToId(szFile));
	ObjectStandaloneFileMap const& activeStandaloneFiles = pAudioObject->GetActiveStandaloneFiles();
	ObjectStandaloneFileMap::const_iterator Iter(activeStandaloneFiles.begin());
	ObjectStandaloneFileMap::const_iterator IterEnd(activeStandaloneFiles.end());

	while (Iter != IterEnd)
	{
		CATLStandaloneFile* const pStandaloneFile = m_audioStandaloneFileMgr.LookupId(Iter->first);

		if (pStandaloneFile != nullptr && pStandaloneFile->m_id == fileID)
		{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			if (pStandaloneFile->m_state != eAudioStandaloneFileState_Playing)
			{
				char const* szState = "unknown";

				switch (pStandaloneFile->m_state)
				{
				case eAudioStandaloneFileState_Playing:
					szState = "playing";
					break;
				case eAudioStandaloneFileState_Loading:
					szState = "loading";
					break;
				case eAudioStandaloneFileState_Stopping:
					szState = "stopping";
					break;
				default:
					break;
				}

				g_audioLogger.Log(eAudioLogType_Warning, "Request to stop a standalone audio file that is not playing! State: \"%s\"", szState);
			}
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

			SAudioStandaloneFileInfo fileInfo;
			fileInfo.fileId = fileID;
			fileInfo.fileInstanceId = pStandaloneFile->GetId();
			fileInfo.pAudioObject = pAudioObject->GetImplDataPtr();
			fileInfo.pImplData = pStandaloneFile->m_pImplData;
			fileInfo.szFileName = szFile;
			EAudioRequestStatus const tempStatus = m_pImpl->StopFile(&fileInfo);

			status = eAudioRequestStatus_Success;

			if (tempStatus != eAudioRequestStatus_Pending)
			{
				pAudioObject->ReportFinishedStandaloneFile(pStandaloneFile);
				m_audioStandaloneFileMgr.ReleaseStandaloneFile(pStandaloneFile);

				Iter = activeStandaloneFiles.begin();
				IterEnd = activeStandaloneFiles.end();
				continue;
			}
			else
			{
				pStandaloneFile->m_state = eAudioStandaloneFileState_Stopping;
			}
		}

		++Iter;
	}

	return status;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::PrepUnprepTriggerAsync(
  CATLAudioObject* const pAudioObject,
  CATLTrigger const* const pTrigger,
  bool const bPrepare)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	AudioObjectId const nATLObjectID = pAudioObject->GetId();
	AudioControlId const nATLTriggerID = pTrigger->GetId();

	ObjectTriggerImplStates const& rTriggerImplStates = pAudioObject->GetTriggerImpls();

	for (auto const pTriggerImpl : pTrigger->m_implPtrs)
	{
		AudioEnumFlagsType nTriggerImplFlags = INVALID_AUDIO_ENUM_FLAG_TYPE;
		ObjectTriggerImplStates::const_iterator iPlace = rTriggerImplStates.end();
		if (FindPlaceConst(rTriggerImplStates, pTriggerImpl->m_audioTriggerImplId, iPlace))
		{
			nTriggerImplFlags = iPlace->second.flags;
		}
		EAudioSubsystem const eReceiver = pTriggerImpl->GetReceiver();
		CATLEvent* const pEvent = m_audioEventMgr.GetEvent(eReceiver);
		EAudioRequestStatus ePrepUnprepResult = eAudioRequestStatus_Failure;

		switch (eReceiver)
		{
		case eAudioSubsystem_AudioImpl:
			{
				if (bPrepare)
				{
					if (((nTriggerImplFlags & eAudioTriggerStatus_Prepared) == 0) && ((nTriggerImplFlags & eAudioTriggerStatus_Loading) == 0))
					{
						ePrepUnprepResult = m_pImpl->PrepareTriggerAsync(
						  pAudioObject->GetImplDataPtr(),
						  pTriggerImpl->m_pImplData,
						  pEvent->m_pImplData);
					}
				}
				else
				{
					if (((nTriggerImplFlags & eAudioTriggerStatus_Prepared) != 0) && ((nTriggerImplFlags & eAudioTriggerStatus_Unloading) == 0))
					{
						ePrepUnprepResult = m_pImpl->UnprepareTriggerAsync(
						  pAudioObject->GetImplDataPtr(),
						  pTriggerImpl->m_pImplData,
						  pEvent->m_pImplData);
					}
				}

				if (ePrepUnprepResult == eAudioRequestStatus_Success)
				{
					pEvent->m_audioObjectId = nATLObjectID;
					pEvent->m_pTrigger = pTrigger;
					pEvent->m_audioTriggerImplId = pTriggerImpl->m_audioTriggerImplId;

					pEvent->m_audioEventState = bPrepare ? eAudioEventState_Loading : eAudioEventState_Unloading;
				}

				break;
			}
		case eAudioSubsystem_AudioInternal:
			{
				//TODO: handle this

				break;
			}
		default:
			{
				CRY_ASSERT(false);//unknown ATLRecipient

				break;
			}
		}

		if (ePrepUnprepResult == eAudioRequestStatus_Success)
		{
			pEvent->SetDataScope(pTrigger->GetDataScope());
			pAudioObject->ReportStartedEvent(pEvent);
			result = eAudioRequestStatus_Success;// if at least one event fires, it is a success
		}
		else
		{
			m_audioEventMgr.ReleaseEvent(pEvent);
		}
	}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	if (result != eAudioRequestStatus_Success)
	{
		// No TriggerImpl produced an active event.
		g_audioLogger.Log(eAudioLogType_Warning, "PrepUnprepTriggerAsync failed on AudioObject \"%s\" (ID: %u)", m_debugNameStore.LookupAudioObjectName(nATLObjectID), nATLObjectID);
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::ActivateTrigger(
  CATLAudioObject* const pAudioObject,
  CATLTrigger const* const pTrigger,
  float const timeUntilRemovalMS,
  void* const pOwner /*= nullptr*/,
  void* const pUserData /*= nullptr*/,
  void* const pUserDataOwner /*= nullptr*/,
  AudioEnumFlagsType const flags /*= INVALID_AUDIO_ENUM_FLAG_TYPE*/)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	if (pAudioObject != m_pGlobalAudioObject)
	{
		// If the AudioObject uses Obstruction/Occlusion then set the values before activating the trigger.
		if (pAudioObject->CanRunObstructionOcclusion() && !m_audioObjectMgr.IsActive(pAudioObject))
		{
			pAudioObject->ResetObstructionOcclusion(m_audioListenerMgr.GetDefaultListenerAttributes().transformation);
		}
	}

	AudioControlId const nATLTriggerID = pTrigger->GetId();

	// Sets eATS_STARTING on this TriggerInstance to avoid
	// reporting TriggerFinished while the events are being started.
	pAudioObject->ReportStartingTriggerInstance(m_triggerInstanceIDCounter, nATLTriggerID);

	for (auto const pTriggerImpl : pTrigger->m_implPtrs)
	{
		EAudioSubsystem const eReceiver = pTriggerImpl->GetReceiver();
		CATLEvent* const pEvent = m_audioEventMgr.GetEvent(eReceiver);

		EAudioRequestStatus eActivateResult = eAudioRequestStatus_Failure;

		switch (eReceiver)
		{
		case eAudioSubsystem_AudioImpl:
			{
				eActivateResult = m_pImpl->ActivateTrigger(
				  pAudioObject->GetImplDataPtr(),
				  pTriggerImpl->m_pImplData,
				  pEvent->m_pImplData);

				break;
			}
		case eAudioSubsystem_AudioInternal:
			{
				eActivateResult = ActivateInternalTrigger(
				  pAudioObject,
				  pTriggerImpl->m_pImplData,
				  pEvent->m_pImplData);

				break;
			}
		default:
			{
				CRY_ASSERT(false);//unknown ATLRecipient

				break;
			}
		}

		if (eActivateResult == eAudioRequestStatus_Success || eActivateResult == eAudioRequestStatus_Pending)
		{
			pEvent->m_audioObjectId = pAudioObject->GetId();
			pEvent->m_pTrigger = pTrigger;
			pEvent->m_audioTriggerImplId = pTriggerImpl->m_audioTriggerImplId;
			pEvent->m_audioTriggerInstanceId = m_triggerInstanceIDCounter;
			pEvent->SetDataScope(pTrigger->GetDataScope());

			if (eActivateResult == eAudioRequestStatus_Success)
			{
				pEvent->m_audioEventState = eAudioEventState_Playing;
			}
			else if (eActivateResult == eAudioRequestStatus_Pending)
			{
				pEvent->m_audioEventState = eAudioEventState_Loading;
			}

			pAudioObject->ReportStartedEvent(pEvent);

			// If at least one event fires, it is a success: the trigger has been activated.
			result = eAudioRequestStatus_Success;
		}
		else
		{
			m_audioEventMgr.ReleaseEvent(pEvent);
		}
	}

	// Either removes the eATS_STARTING flag on this trigger instance or removes it if no event was started.
	pAudioObject->ReportStartedTriggerInstance(m_triggerInstanceIDCounter++, pOwner, pUserData, pUserDataOwner, flags);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	if (result != eAudioRequestStatus_Success)
	{
		// No TriggerImpl generated an active event.
		g_audioLogger.Log(eAudioLogType_Warning, "Trigger \"%s\" failed on AudioObject \"%s\" (ID: %u)", m_debugNameStore.LookupAudioTriggerName(nATLTriggerID), m_debugNameStore.LookupAudioObjectName(pAudioObject->GetId()), pAudioObject->GetId());
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::StopTrigger(
  CATLAudioObject* const pAudioObject,
  CATLTrigger const* const pTrigger)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;
	for (auto const pEvent : pAudioObject->GetActiveEvents())
	{
		if ((pEvent != nullptr) && pEvent->IsPlaying() && (pEvent->m_pTrigger == pTrigger))
		{
			switch (pEvent->m_audioSubsystem)
			{
			case eAudioSubsystem_AudioImpl:
				{
					result = m_pImpl->StopEvent(pAudioObject->GetImplDataPtr(), pEvent->m_pImplData);

					break;
				}
			case eAudioSubsystem_AudioInternal:
				{
					result = StopInternalEvent(pAudioObject, pEvent->m_pImplData);

					break;
				}
			default:
				{
					CRY_ASSERT(false);//unknown ATLRecipient

					break;
				}
			}
		}
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::StopAllTriggers(CATLAudioObject* const pAudioObject)
{
	return m_pImpl->StopAllEvents(pAudioObject->GetImplDataPtr());
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::SetSwitchState(
  CATLAudioObject* const pAudioObject,
  CATLSwitchState const* const pState)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	for (auto const pSwitchStateImpl : pState->m_implPtrs)
	{
		EAudioSubsystem const receiver = pSwitchStateImpl->GetReceiver();
		EAudioRequestStatus setStateResult = eAudioRequestStatus_Failure;

		switch (receiver)
		{
		case eAudioSubsystem_AudioImpl:
			{
				setStateResult = m_pImpl->SetSwitchState(pAudioObject->GetImplDataPtr(), pSwitchStateImpl->m_pImplData);

				break;
			}
		case eAudioSubsystem_AudioInternal:
			{
				setStateResult = SetInternalSwitchState(pAudioObject, pSwitchStateImpl->m_pImplData);

				break;
			}
		default:
			{
				CRY_ASSERT(false);//unknown ATLRecipient

				break;
			}
		}

		if (setStateResult == eAudioRequestStatus_Success)
		{
			result = eAudioRequestStatus_Success;// if at least one of the implementations is set successfully, it is a success
		}
	}

	if (result == eAudioRequestStatus_Success)
	{
		pAudioObject->SetSwitchState(pState->GetParentId(), pState->GetId());
	}
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	else
	{
		char const* const sSwitchName = m_debugNameStore.LookupAudioSwitchName(pState->GetParentId());
		char const* const sSwitchStateName = m_debugNameStore.LookupAudioSwitchStateName(pState->GetParentId(), pState->GetId());
		char const* const sAudioObjectName = m_debugNameStore.LookupAudioObjectName(pAudioObject->GetId());
		g_audioLogger.Log(eAudioLogType_Warning, "Failed to set the ATLSwitch \"%s\" to ATLSwitchState \"%s\" on AudioObject \"%s\" (ID: %u)", sSwitchName, sSwitchStateName, sAudioObjectName, pAudioObject->GetId());
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::SetRtpc(
  CATLAudioObject* const pAudioObject,
  CATLRtpc const* const pRtpc,
  float const value)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	for (auto const pRtpcImpl : pRtpc->m_implPtrs)
	{
		EAudioSubsystem const receiver = pRtpcImpl->GetReceiver();
		EAudioRequestStatus setRtpcResult = eAudioRequestStatus_Failure;

		switch (receiver)
		{
		case eAudioSubsystem_AudioImpl:
			{
				setRtpcResult = m_pImpl->SetRtpc(pAudioObject->GetImplDataPtr(), pRtpcImpl->m_pImplData, value);

				break;
			}
		case eAudioSubsystem_AudioInternal:
			{
				setRtpcResult = SetInternalRtpc(pAudioObject, pRtpcImpl->m_pImplData, value);

				break;
			}
		default:
			{
				CRY_ASSERT(false);//unknown ATLRecipient

				break;
			}
		}

		if (setRtpcResult == eAudioRequestStatus_Success)
		{
			result = eAudioRequestStatus_Success;// if at least one of the implementations is set successfully, it is a success
		}
	}

	if (result == eAudioRequestStatus_Success)
	{
		pAudioObject->SetRtpc(pRtpc->GetId(), value);
	}
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	else
	{
		char const* const szRtpcName = m_debugNameStore.LookupAudioRtpcName(pRtpc->GetId());
		char const* const szAudioObjectName = m_debugNameStore.LookupAudioObjectName(pAudioObject->GetId());
		g_audioLogger.Log(eAudioLogType_Warning, "Failed to set the ATLRtpc \"%s\" to %f on AudioObject \"%s\" (ID: %u)", szRtpcName, value, szAudioObjectName, pAudioObject->GetId());
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::SetEnvironment(
  CATLAudioObject* const pAudioObject,
  CATLAudioEnvironment const* const pEnvironment,
  float const amount)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;

	for (auto const pEnvImpl : pEnvironment->m_implPtrs)
	{
		EAudioSubsystem const receiver = pEnvImpl->GetReceiver();
		EAudioRequestStatus setEnvResult = eAudioRequestStatus_Failure;

		switch (receiver)
		{
		case eAudioSubsystem_AudioImpl:
			{
				setEnvResult = m_pImpl->SetEnvironment(pAudioObject->GetImplDataPtr(), pEnvImpl->m_pImplData, amount);

				break;
			}
		case eAudioSubsystem_AudioInternal:
			{
				setEnvResult = SetInternalEnvironment(pAudioObject, pEnvImpl->m_pImplData, amount);

				break;
			}
		default:
			{
				CRY_ASSERT(false);//unknown ATLRecipient

				break;
			}
		}

		if (setEnvResult == eAudioRequestStatus_Success)
		{
			result = eAudioRequestStatus_Success;// if at least one of the implementations is set successfully, it is a success
		}
	}

	if (result == eAudioRequestStatus_Success)
	{
		pAudioObject->SetEnvironmentAmount(pEnvironment->GetId(), amount);
	}
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	else
	{
		char const* const sEnvironmentName = m_debugNameStore.LookupAudioEnvironmentName(pEnvironment->GetId());
		char const* const sAudioObjectName = m_debugNameStore.LookupAudioObjectName(pAudioObject->GetId());
		g_audioLogger.Log(eAudioLogType_Warning, "Failed to set the ATLAudioEnvironment \"%s\" to %f on AudioObject \"%s\" (ID: %u)", sEnvironmentName, amount, sAudioObjectName, pAudioObject->GetId());
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::ResetEnvironments(CATLAudioObject* const pAudioObject)
{
	// Needs to be a copy as SetEnvironment removes from the map that we are iterating over.
	ObjectEnvironmentMap const environments = pAudioObject->GetEnvironments();

	EAudioRequestStatus result = eAudioRequestStatus_Success;

	for (auto const& environmentPair : environments)
	{
		CATLAudioEnvironment const* const pEnvironment = stl::find_in_map(m_environments, environmentPair.first, nullptr);

		if (pEnvironment != nullptr)
		{
			EAudioRequestStatus const eSetEnvResult = SetEnvironment(pAudioObject, pEnvironment, 0.0f);

			if (eSetEnvResult != eAudioRequestStatus_Success)
			{
				// If setting at least one Environment fails, we consider this a failure.
				result = eAudioRequestStatus_Failure;
			}
		}
	}

	if (result == eAudioRequestStatus_Success)
	{
		pAudioObject->ClearEnvironments();
	}
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	else
	{
		AudioObjectId const nObjectID = pAudioObject->GetId();

		g_audioLogger.Log(
		  eAudioLogType_Warning,
		  "Failed to Reset AudioEnvironments on AudioObject \"%s\" (ID: %u)",
		  m_debugNameStore.LookupAudioObjectName(nObjectID),
		  nObjectID);
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::ActivateInternalTrigger(
  CATLAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger,
  IAudioEvent* const pAudioEvent)
{
	//TODO implement
	return eAudioRequestStatus_Failure;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::StopInternalEvent(
  CATLAudioObject* const pAudioObject,
  IAudioEvent const* const pAudioEvent)
{
	//TODO implement
	return eAudioRequestStatus_Failure;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::StopAllInternalEvents(CATLAudioObject* const pAudioObject)
{
	//TODO implement
	return eAudioRequestStatus_Failure;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::SetInternalRtpc(
  CATLAudioObject* const pAudioObject,
  CryAudio::Impl::IAudioRtpc const* const pAudioRtpc,
  float const value)
{
	//TODO implement
	return eAudioRequestStatus_Failure;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::SetInternalSwitchState(
  CATLAudioObject* const pAudioObject,
  IAudioSwitchState const* const pAudioSwitchState)
{
	SATLSwitchStateImplData_internal const* const pInternalStateData = static_cast<SATLSwitchStateImplData_internal const*>(pAudioSwitchState);

	if (pInternalStateData->internalAudioSwitchId == SATLInternalControlIDs::obstructionOcclusionCalcSwitchId)
	{
		if (pAudioObject != m_pGlobalAudioObject)
		{
			if (pInternalStateData->internalAudioSwitchStateId == SATLInternalControlIDs::ignoreStateId)
			{
				pAudioObject->SetObstructionOcclusionCalc(eAudioOcclusionType_Ignore);
				SATLSoundPropagationData propagationData;
				pAudioObject->GetPropagationData(propagationData);
				m_pImpl->SetObstructionOcclusion(pAudioObject->GetImplDataPtr(), propagationData.obstruction, propagationData.occlusion);
			}
			else if (pInternalStateData->internalAudioSwitchStateId == SATLInternalControlIDs::singleRayStateId)
			{
				pAudioObject->SetObstructionOcclusionCalc(eAudioOcclusionType_SingleRay);
			}
			else if (pInternalStateData->internalAudioSwitchStateId == SATLInternalControlIDs::multiRayStateId)
			{
				pAudioObject->SetObstructionOcclusionCalc(eAudioOcclusionType_MultiRay);
			}
			else
			{
				CRY_ASSERT(false);
			}
		}
	}
	else if (pInternalStateData->internalAudioSwitchId == SATLInternalControlIDs::objectDopplerTrackingSwitchId)
	{
		if (pAudioObject != m_pGlobalAudioObject)
		{
			if (pInternalStateData->internalAudioSwitchStateId == SATLInternalControlIDs::onStateId)
			{
				pAudioObject->SetDopplerTracking(true);
			}
			else if (pInternalStateData->internalAudioSwitchStateId == SATLInternalControlIDs::offStateId)
			{
				pAudioObject->SetDopplerTracking(false);
			}
			else
			{
				CRY_ASSERT(true);
			}
		}
	}
	else if (pInternalStateData->internalAudioSwitchId == SATLInternalControlIDs::objectVelocityTrackingSwitchId)
	{
		if (pAudioObject != m_pGlobalAudioObject)
		{
			if (pInternalStateData->internalAudioSwitchStateId == SATLInternalControlIDs::onStateId)
			{
				pAudioObject->SetVelocityTracking(true);
			}
			else if (pInternalStateData->internalAudioSwitchStateId == SATLInternalControlIDs::offStateId)
			{
				pAudioObject->SetVelocityTracking(false);
			}
			else
			{
				CRY_ASSERT(true);
			}
		}
	}

	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::SetInternalEnvironment(
  CATLAudioObject* const pAudioObject,
  CryAudio::Impl::IAudioEnvironment const* const pAudioEnvironment,
  float const amount)
{
	// TODO: implement
	return eAudioRequestStatus_Failure;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioTranslationLayer::RefreshAudioSystem(char const* const szLevelName)
{
	g_audioLogger.Log(eAudioLogType_Warning, "Beginning to refresh the AudioSystem!");

	EAudioRequestStatus result = m_pImpl->StopAllSounds();
	CRY_ASSERT(result == eAudioRequestStatus_Success);

	result = m_fileCacheMgr.UnloadDataByScope(eAudioDataScope_LevelSpecific);
	CRY_ASSERT(result == eAudioRequestStatus_Success);

	result = m_fileCacheMgr.UnloadDataByScope(eAudioDataScope_Global);
	CRY_ASSERT(result == eAudioRequestStatus_Success);

	result = ClearControlsData(eAudioDataScope_All);
	CRY_ASSERT(result == eAudioRequestStatus_Success);

	result = ClearPreloadsData(eAudioDataScope_All);
	CRY_ASSERT(result == eAudioRequestStatus_Success);

	m_pImpl->OnAudioSystemRefresh();

	SetImplLanguage();

	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> configPath(gEnv->pAudioSystem->GetConfigPath());
	result = ParseControlsData(configPath.c_str(), eAudioDataScope_Global);
	CRY_ASSERT(result == eAudioRequestStatus_Success);

	result = ParsePreloadsData(configPath.c_str(), eAudioDataScope_Global);
	CRY_ASSERT(result == eAudioRequestStatus_Success);

	result = m_fileCacheMgr.TryLoadRequest(SATLInternalControlIDs::globalPreloadRequestId, true, true);
	CRY_ASSERT(result == eAudioRequestStatus_Success);

	if (szLevelName != nullptr && szLevelName[0] != '\0')
	{
		configPath += "levels" CRY_NATIVE_PATH_SEPSTR;
		configPath += szLevelName;
		result = ParseControlsData(configPath.c_str(), eAudioDataScope_LevelSpecific);
		CRY_ASSERT(result == eAudioRequestStatus_Success);

		result = ParsePreloadsData(configPath.c_str(), eAudioDataScope_LevelSpecific);
		CRY_ASSERT(result == eAudioRequestStatus_Success);

		AudioPreloadRequestId audioPreloadRequestId = INVALID_AUDIO_PRELOAD_REQUEST_ID;

		if (GetAudioPreloadRequestId(szLevelName, audioPreloadRequestId))
		{
			result = m_fileCacheMgr.TryLoadRequest(audioPreloadRequestId, true, true);
			CRY_ASSERT(result == eAudioRequestStatus_Success);
		}
	}

	g_audioLogger.Log(eAudioLogType_Warning, "Done refreshing the AudioSystem!");

	return eAudioRequestStatus_Success;
}
//////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::SetImplLanguage()
{
	if (ICVar* pCVar = gEnv->pConsole->GetCVar("g_languageAudio"))
	{
		m_pImpl->SetLanguage(pCVar->GetString());
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::OnInputEvent(SInputEvent const& event)
{
	if (event.state == eIS_Changed && event.deviceType == eIDT_Gamepad)
	{
		if (event.keyId == eKI_SYS_ConnectDevice)
		{
			m_pImpl->GamepadConnected(event.deviceUniqueID);
		}
		else if (event.keyId == eKI_SYS_DisconnectDevice)
		{
			m_pImpl->GamepadDisconnected(event.deviceUniqueID);
		}
	}

	// Do not consume event
	return false;
}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
///////////////////////////////////////////////////////////////////////////
bool CAudioTranslationLayer::ReserveAudioObjectId(AudioObjectId& audioObjectId, char const* const szAudioObjectName)
{
	return m_audioObjectMgr.ReserveId(audioObjectId, szAudioObjectName);
}

//////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::DrawAudioSystemDebugInfo()
{
	CRY_PROFILE_FUNCTION(PROFILE_AUDIO);

	if (IRenderAuxGeom* const pAuxGeom = g_audioCVars.m_drawAudioDebug > 0 && gEnv->pRenderer != nullptr ? gEnv->pRenderer->GetIRenderAuxGeom() : nullptr)
	{
		DrawAudioObjectDebugInfo(*pAuxGeom); // needs to be called first so that the rest of the labels are printed
		// on top (Draw2dLabel doesn't provide a way set which labels are printed on top)

		size_t const nBucketAllocatorPoolUsedSize = g_audioMemoryPoolPrimary.GetSmallAllocsSize();
		size_t const nBucketAllocatorPoolAllocation = g_audioMemoryPoolPrimary.GetSmallAllocsCount();
		size_t const nPrimaryPoolSize = g_audioMemoryPoolPrimary.MemSize();
		size_t const nPrimaryPoolFreeSize = g_audioMemoryPoolPrimary.MemFree();
		size_t const nPrimaryPoolUsedSize = nPrimaryPoolSize - nPrimaryPoolFreeSize;
		size_t const nPrimaryPoolAllocation = g_audioMemoryPoolPrimary.FragmentCount();

		float fPosX = 0.0f;
		float fPosY = 4.0f;

		float const fColor[4] = { 1.0f, 1.0f, 1.0f, 0.9f };
		float const fColorRed[4] = { 1.0f, 0.0f, 0.0f, 0.7f };
		float const fColorGreen[4] = { 0.0f, 1.0f, 0.0f, 0.7f };
		float const fColorBlue[4] = { 0.4f, 0.4f, 1.0f, 1.0f };

		pAuxGeom->Draw2dLabel(fPosX, fPosY, 1.6f, fColorBlue, false, "AudioTranslationLayer with %s", m_implNameString.c_str());

		fPosX += 20.0f;
		fPosY += 17.0f;

		pAuxGeom->Draw2dLabel(fPosX, fPosY, 1.35f, fColor, false,
		                      "ATL Memory: Bucket: %d / ? KiB NumAllocs: %d Primary: %.2f / %.2f MiB NumAllocs: %d",
		                      static_cast<int>(nBucketAllocatorPoolUsedSize / 1024),
		                      static_cast<int>(nBucketAllocatorPoolAllocation),
		                      (nPrimaryPoolUsedSize / 1024) / 1024.0f,
		                      (nPrimaryPoolSize / 1024) / 1024.0f,
		                      static_cast<int>(nPrimaryPoolAllocation));

		float const fLineHeight = 13.0f;

		if (m_pImpl != nullptr)
		{
			SAudioImplMemoryInfo memoryInfo;
			m_pImpl->GetMemoryInfo(memoryInfo);

			fPosY += fLineHeight;
			pAuxGeom->Draw2dLabel(fPosX, fPosY, 1.35f, fColor, false,
			                      "Impl Memory: Bucket: %d / ? KiB NumAllocs: %d Primary: %.2f / %.2f MiB NumAllocs: %d Secondary: %.2f / %.2f MiB NumAllocs: %d",
			                      static_cast<int>(memoryInfo.bucketUsedSize / 1024),
			                      static_cast<int>(memoryInfo.bucketAllocations),
			                      (memoryInfo.primaryPoolUsedSize / 1024) / 1024.0f,
			                      (memoryInfo.primaryPoolSize / 1024) / 1024.0f,
			                      static_cast<int>(memoryInfo.primaryPoolAllocations),
			                      (memoryInfo.secondaryPoolUsedSize / 1024) / 1024.0f,
			                      (memoryInfo.secondaryPoolSize / 1024) / 1024.0f,
			                      static_cast<int>(memoryInfo.secondaryPoolAllocations));
		}

		static float const SMOOTHING_ALPHA = 0.2f;
		static float fSyncRays = 0;
		static float fAsyncRays = 0;

		Vec3 const& listenerPosition = m_audioListenerMgr.GetDefaultListenerAttributes().transformation.GetPosition();
		Vec3 const& listenerDirection = m_audioListenerMgr.GetDefaultListenerAttributes().transformation.GetForward();
		float const listenerVelocity = m_audioListenerMgr.GetDefaultListenerVelocity();
		size_t const nNumAudioObjects = m_audioObjectMgr.GetNumAudioObjects();
		size_t const nNumActiveAudioObjects = m_audioObjectMgr.GetNumActiveAudioObjects();
		size_t const nEvents = m_audioEventMgr.GetNumActive();
		size_t const nListeners = m_audioListenerMgr.GetNumActive();
		size_t const nNumEventListeners = m_audioEventListenerMgr.GetNumEventListeners();
		fSyncRays += (CATLAudioObject::CPropagationProcessor::s_totalSyncPhysRays - fSyncRays) * SMOOTHING_ALPHA;
		fAsyncRays += (CATLAudioObject::CPropagationProcessor::s_totalAsyncPhysRays - fAsyncRays) * SMOOTHING_ALPHA * 0.1f;

		bool const bActive = true;
		float const fColorListener[4] =
		{
			bActive ? fColorGreen[0] : fColorRed[0],
			bActive ? fColorGreen[1] : fColorRed[1],
			bActive ? fColorGreen[2] : fColorRed[2],
			1.0f
		};

		float const* fColorNumbers = fColorBlue;

		fPosY += fLineHeight;
		pAuxGeom->Draw2dLabel(fPosX, fPosY, 1.35f, fColorListener, false, "Listener <%d> PosXYZ: %.2f %.2f %.2f FwdXYZ: %.2f %.2f %.2f Velocity: %.2f m/s", 0, listenerPosition.x, listenerPosition.y, listenerPosition.z, listenerDirection.x, listenerDirection.y, listenerDirection.z, listenerVelocity);

		fPosY += fLineHeight;
		pAuxGeom->Draw2dLabel(fPosX, fPosY, 1.35f, fColorNumbers, false,
		                      "Objects: %3" PRISIZE_T "/%3" PRISIZE_T " Events: %3" PRISIZE_T " EventListeners %3" PRISIZE_T " Listeners: %" PRISIZE_T " | SyncRays: %3.1f AsyncRays: %3.1f",
		                      nNumActiveAudioObjects, nNumAudioObjects, nEvents, nNumEventListeners, nListeners, fSyncRays, fAsyncRays);

		fPosY += fLineHeight;
		DrawATLComponentDebugInfo(*pAuxGeom, fPosX, fPosY);

		pAuxGeom->Commit(7);
	}
}

///////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::DrawATLComponentDebugInfo(IRenderAuxGeom& auxGeom, float posX, float const posY)
{
	m_fileCacheMgr.DrawDebugInfo(auxGeom, posX, posY);

	if ((g_audioCVars.m_drawAudioDebug & eADDF_SHOW_ACTIVE_OBJECTS) > 0)
	{
		m_audioObjectMgr.DrawDebugInfo(auxGeom, posX, posY);
		posX += 300.0f;
	}

	if ((g_audioCVars.m_drawAudioDebug & eADDF_SHOW_ACTIVE_EVENTS) > 0)
	{
		m_audioEventMgr.DrawDebugInfo(auxGeom, posX, posY);
		posX += 600.0f;
	}

	if ((g_audioCVars.m_drawAudioDebug & eADDF_SHOW_STANDALONE_FILES) > 0)
	{
		m_audioStandaloneFileMgr.DrawDebugInfo(auxGeom, posX, posY);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::RetriggerAudioControls()
{
	CAudioObjectManager::RegisteredAudioObjectsMap const& registeredAudioObjects = m_audioObjectMgr.GetRegisteredAudioObjects();

	for (auto const& audioObjectPair : registeredAudioObjects)
	{
		CATLAudioObject* const pAudioObject = audioObjectPair.second;

		// First set the audio object's position.
		if (pAudioObject != m_pGlobalAudioObject)
		{
			m_pImpl->Set3DAttributes(pAudioObject->GetImplDataPtr(), pAudioObject->Get3DAttributes());
		}

		// Then update all controls set to the audio object.
		// RTPCs
		ObjectRtpcMap const& rtpcs = pAudioObject->GetRtpcs();

		for (auto const& rtpcPair : rtpcs)
		{
			CATLRtpc const* const pRtpc = stl::find_in_map(m_rtpcs, rtpcPair.first, nullptr);

			if (pRtpc != nullptr)
			{
				for (auto const pRtpcImpl : pRtpc->m_implPtrs)
				{
					EAudioSubsystem const receiver = pRtpcImpl->GetReceiver();

					// We only need to update the implementation.
					if (receiver == eAudioSubsystem_AudioImpl)
					{
						EAudioRequestStatus const setRtpcResult = m_pImpl->SetRtpc(pAudioObject->GetImplDataPtr(), pRtpcImpl->m_pImplData, rtpcPair.second);

						if (setRtpcResult != eAudioRequestStatus_Success)
						{
							g_audioLogger.Log(eAudioLogType_Warning, "RTPC \"%s\" failed during audio middleware switch on AudioObject \"%s\" (ID: %u)", m_debugNameStore.LookupAudioRtpcName(rtpcPair.first), m_debugNameStore.LookupAudioObjectName(pAudioObject->GetId()), pAudioObject->GetId());
						}
					}
				}
			}
		}

		// Switches
		ObjectStateMap const& audioSwitches = pAudioObject->GetSwitchStates();

		for (auto const& switchPair : audioSwitches)
		{
			CATLSwitch const* const pSwitch = stl::find_in_map(m_switches, switchPair.first, nullptr);

			if (pSwitch != nullptr)
			{
				CATLSwitchState const* const pState = stl::find_in_map(pSwitch->audioSwitchStates, switchPair.second, nullptr);

				if (pState != nullptr)
				{
					for (auto const pSwitchStateImpl : pState->m_implPtrs)
					{
						EAudioSubsystem const receiver = pSwitchStateImpl->GetReceiver();

						// We only need to update the implementation.
						if (receiver == eAudioSubsystem_AudioImpl)
						{
							EAudioRequestStatus const setStateResult = m_pImpl->SetSwitchState(pAudioObject->GetImplDataPtr(), pSwitchStateImpl->m_pImplData);

							if (setStateResult != eAudioRequestStatus_Success)
							{
								g_audioLogger.Log(eAudioLogType_Warning, "SwitchStateImpl \"%s\" : \"%s\" failed during audio middleware switch on AudioObject \"%s\" (ID: %u)", m_debugNameStore.LookupAudioSwitchName(switchPair.first), m_debugNameStore.LookupAudioSwitchStateName(switchPair.first, switchPair.second), m_debugNameStore.LookupAudioObjectName(pAudioObject->GetId()), pAudioObject->GetId());
							}
						}
					}
				}
			}
		}

		// Environments
		ObjectEnvironmentMap const& audioEnvironments = pAudioObject->GetEnvironments();

		for (auto const& environmentPair : audioEnvironments)
		{
			CATLAudioEnvironment const* const pEnvironment = stl::find_in_map(m_environments, environmentPair.first, nullptr);

			if (pEnvironment != nullptr)
			{
				for (auto const pEnvImpl : pEnvironment->m_implPtrs)
				{
					EAudioSubsystem const receiver = pEnvImpl->GetReceiver();

					// We only need to update the implementation.
					if (receiver == eAudioSubsystem_AudioImpl)
					{
						EAudioRequestStatus const setEnvResult = m_pImpl->SetEnvironment(pAudioObject->GetImplDataPtr(), pEnvImpl->m_pImplData, environmentPair.second);

						if (setEnvResult != eAudioRequestStatus_Success)
						{
							g_audioLogger.Log(eAudioLogType_Warning, "Environment \"%s\" failed during audio middleware switch on AudioObject \"%s\" (ID: %u)", m_debugNameStore.LookupAudioEnvironmentName(environmentPair.first), m_debugNameStore.LookupAudioObjectName(pAudioObject->GetId()), pAudioObject->GetId());
						}
					}
				}
			}
		}

		// Last re-execute its active triggers and standalone files.
		ObjectTriggerStates& triggerStates = pAudioObject->GetTriggerStates();
		auto it = triggerStates.cbegin();
		auto end = triggerStates.cend();
		while (it != end)
		{
			auto const& triggerState = *it;
			CATLTrigger const* const pTrigger = stl::find_in_map(m_triggers, triggerState.second.audioTriggerId, nullptr);

			if (pTrigger != nullptr)
			{
				if (!pTrigger->m_implPtrs.empty())
				{
					for (auto const pTriggerImpl : pTrigger->m_implPtrs)
					{
						EAudioSubsystem const receiver = pTriggerImpl->GetReceiver();

						// We only need to update the implementation.
						if (receiver == eAudioSubsystem_AudioImpl)
						{
							CATLEvent* const pEvent = m_audioEventMgr.GetEvent(receiver);
							EAudioRequestStatus const activateResult = m_pImpl->ActivateTrigger(
							  pAudioObject->GetImplDataPtr(),
							  pTriggerImpl->m_pImplData,
							  pEvent->m_pImplData);

							if (activateResult == eAudioRequestStatus_Success || activateResult == eAudioRequestStatus_Pending)
							{
								pEvent->m_audioObjectId = pAudioObject->GetId();
								pEvent->m_pTrigger = pTrigger;
								pEvent->m_audioTriggerImplId = pTriggerImpl->m_audioTriggerImplId;
								pEvent->m_audioTriggerInstanceId = triggerState.first;
								pEvent->SetDataScope(pTrigger->GetDataScope());

								if (activateResult == eAudioRequestStatus_Success)
								{
									pEvent->m_audioEventState = eAudioEventState_Playing;
								}
								else if (activateResult == eAudioRequestStatus_Pending)
								{
									pEvent->m_audioEventState = eAudioEventState_Loading;
								}

								pAudioObject->ReportStartedEvent(pEvent);
							}
							else
							{
								g_audioLogger.Log(eAudioLogType_Warning, "TriggerImpl \"%s\" failed during audio middleware switch on AudioObject \"%s\" (ID: %u)", m_debugNameStore.LookupAudioTriggerName(pTrigger->GetId()), m_debugNameStore.LookupAudioObjectName(pAudioObject->GetId()), pAudioObject->GetId());
								m_audioEventMgr.ReleaseEvent(pEvent);
							}
						}
					}
				}
				else
				{
					// The middleware has no connections set up.
					// Stop the event in this case.
					g_audioLogger.Log(eAudioLogType_Warning, "No trigger connections found during audio middleware switch for \"%s\" on \"%s\" (ID: %u)", m_debugNameStore.LookupAudioTriggerName(pTrigger->GetId()), m_debugNameStore.LookupAudioObjectName(pAudioObject->GetId()), pAudioObject->GetId());
				}
			}
			else
			{
				it = triggerStates.erase(it);
				end = triggerStates.cend();
				continue;
			}

			++it;
		}

		ObjectStandaloneFileMap const& activeStandaloneFiles = pAudioObject->GetActiveStandaloneFiles();

		for (auto const& standaloneFilePair : activeStandaloneFiles)
		{
			CATLStandaloneFile* const pStandaloneFile = m_audioStandaloneFileMgr.LookupId(standaloneFilePair.first);

			if (pStandaloneFile != nullptr)
			{
				CRY_ASSERT(pStandaloneFile->m_state == eAudioStandaloneFileState_Playing);

				SAudioStandaloneFileInfo fileInfo;
				fileInfo.fileId = pStandaloneFile->m_id;
				fileInfo.fileInstanceId = pStandaloneFile->GetId();
				fileInfo.pAudioObject = pAudioObject->GetImplDataPtr();
				fileInfo.pImplData = pStandaloneFile->m_pImplData;
				fileInfo.szFileName = m_debugNameStore.LookupAudioStandaloneFileName(pStandaloneFile->m_id);
				EAudioRequestStatus const tempStatus = m_pImpl->PlayFile(&fileInfo);

				if (tempStatus == eAudioRequestStatus_Success || tempStatus == eAudioRequestStatus_Pending)
				{
					if (tempStatus == eAudioRequestStatus_Success)
					{
						pStandaloneFile->m_state = eAudioStandaloneFileState_Playing;
					}
					else if (tempStatus == eAudioRequestStatus_Pending)
					{
						pStandaloneFile->m_state = eAudioStandaloneFileState_Loading;
					}
				}
				else
				{
					g_audioLogger.Log(eAudioLogType_Warning, "PlayFile failed with \"%s\" (InstanceID: %u) on AudioObject \"%s\" (ID: %u)", fileInfo.szFileName, fileInfo.fileInstanceId, m_debugNameStore.LookupAudioObjectName(pAudioObject->GetId()), pAudioObject->GetId());
				}
			}
			else
			{
				g_audioLogger.Log(eAudioLogType_Error, "Retrigger active standalone audio files failed on instance: %u and file: %u as m_audioStandaloneFileMgr.LookupID() returned nullptr!", standaloneFilePair.first, standaloneFilePair.second);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////
void CAudioTranslationLayer::DrawAudioObjectDebugInfo(IRenderAuxGeom& auxGeom)
{
	m_audioObjectMgr.DrawPerObjectDebugInfo(auxGeom, m_audioListenerMgr.GetDefaultListenerAttributes().transformation.GetPosition());
}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

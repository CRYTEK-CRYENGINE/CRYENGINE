// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ATLAudioObject.h"
#include "AudioCVars.h"
#include "AudioEventManager.h"
#include "AudioSystem.h"
#include "AudioStandaloneFileManager.h"
#include "Common/Logger.h"
#include <IAudioImpl.h>
#include <CryString/HashedString.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryMath/Cry_Camera.h>

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	#include <CryRenderer/IRenderAuxGeom.h>
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

namespace CryAudio
{
TriggerInstanceId CryAudio::CATLAudioObject::s_triggerInstanceIdCounter = 1;
CSystem* CryAudio::CATLAudioObject::s_pAudioSystem = nullptr;
CAudioEventManager* CryAudio::CATLAudioObject::s_pEventManager = nullptr;
CAudioStandaloneFileManager* CryAudio::CATLAudioObject::s_pStandaloneFileManager = nullptr;

//////////////////////////////////////////////////////////////////////////
CATLAudioObject::CATLAudioObject()
	: m_pImplData(nullptr)
	, m_maxRadius(0.0f)
	, m_flags(EObjectFlags::InUse)
	, m_previousRelativeVelocity(0.0f)
	, m_previousAbsoluteVelocity(0.0f)
	, m_propagationProcessor(m_attributes.transformation)
	, m_entityId(INVALID_ENTITYID)
	, m_numPendingSyncCallbacks(0)
{}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::Release()
{
	// Do not clear the object's name though!
	m_activeEvents.clear();
	m_triggerImplStates.clear();
	m_activeStandaloneFiles.clear();

	for (auto& triggerStatesPair : m_triggerStates)
	{
		triggerStatesPair.second.numLoadingEvents = 0;
		triggerStatesPair.second.numPlayingEvents = 0;
	}

	m_pImplData = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportStartingTriggerInstance(TriggerInstanceId const audioTriggerInstanceId, ControlId const audioTriggerId)
{
	SAudioTriggerInstanceState& audioTriggerInstanceState = m_triggerStates.emplace(audioTriggerInstanceId, SAudioTriggerInstanceState()).first->second;
	audioTriggerInstanceState.audioTriggerId = audioTriggerId;
	audioTriggerInstanceState.flags |= ETriggerStatus::Starting;
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportStartedTriggerInstance(
  TriggerInstanceId const audioTriggerInstanceId,
  void* const pOwnerOverride,
  void* const pUserData,
  void* const pUserDataOwner,
  ERequestFlags const flags)
{
	ObjectTriggerStates::iterator iter(m_triggerStates.find(audioTriggerInstanceId));

	if (iter != m_triggerStates.end())
	{
		SAudioTriggerInstanceState& audioTriggerInstanceState = iter->second;

		if (audioTriggerInstanceState.numPlayingEvents > 0 || audioTriggerInstanceState.numLoadingEvents > 0)
		{
			audioTriggerInstanceState.pOwnerOverride = pOwnerOverride;
			audioTriggerInstanceState.pUserData = pUserData;
			audioTriggerInstanceState.pUserDataOwner = pUserDataOwner;
			audioTriggerInstanceState.flags &= ~ETriggerStatus::Starting;
			audioTriggerInstanceState.flags |= ETriggerStatus::Playing;

			if ((flags& ERequestFlags::DoneCallbackOnAudioThread) != 0)
			{
				audioTriggerInstanceState.flags |= ETriggerStatus::CallbackOnAudioThread;
			}
			else if ((flags& ERequestFlags::DoneCallbackOnExternalThread) != 0)
			{
				audioTriggerInstanceState.flags |= ETriggerStatus::CallbackOnExternalThread;
			}
		}
		else
		{
			// All of the events have either finished before we got here or never started.
			// So we report this trigger as finished immediately.
			ReportFinishedTriggerInstance(iter);
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Reported a started instance %u that couldn't be found on an object", audioTriggerInstanceId);
	}
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportStartedEvent(CATLEvent* const pEvent)
{
	m_activeEvents.insert(pEvent);
	m_triggerImplStates.insert(std::make_pair(pEvent->m_audioTriggerImplId, SAudioTriggerImplState()));

	// Update the radius where events playing in this audio object are audible
	if (pEvent->m_pTrigger)
	{
		m_maxRadius = std::max(pEvent->m_pTrigger->m_maxRadius, m_maxRadius);
	}

	ObjectTriggerStates::iterator const iter(m_triggerStates.find(pEvent->m_audioTriggerInstanceId));

	if (iter != m_triggerStates.end())
	{
		SAudioTriggerInstanceState& audioTriggerInstanceState = iter->second;

		switch (pEvent->m_state)
		{
		case EEventState::Playing:
			{
				++(audioTriggerInstanceState.numPlayingEvents);

				break;
			}
		case EEventState::PlayingDelayed:
			{
				CRY_ASSERT(audioTriggerInstanceState.numLoadingEvents > 0);
				--(audioTriggerInstanceState.numLoadingEvents);
				++(audioTriggerInstanceState.numPlayingEvents);
				pEvent->m_state = EEventState::Playing;

				break;
			}
		case EEventState::Loading:
			{
				++(audioTriggerInstanceState.numLoadingEvents);

				break;
			}
		case EEventState::Unloading:
			{
				// not handled currently
				break;
			}
		default:
			{
				// unknown event state!
				CRY_ASSERT(false);

				break;
			}
		}
	}
	else
	{
		// must exist!
		CRY_ASSERT(false);
	}
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportFinishedEvent(CATLEvent* const pEvent, bool const bSuccess)
{
	m_activeEvents.erase(pEvent);
	m_triggerImplStates.erase(pEvent->m_audioTriggerImplId);

	// Recalculate the max radius of the audio object.
	m_maxRadius = 0.0f;

	for (auto const pActiveEvent : m_activeEvents)
	{
		if (pActiveEvent->m_pTrigger != nullptr)
		{
			m_maxRadius = std::max(pActiveEvent->m_pTrigger->m_maxRadius, m_maxRadius);
		}
	}

	ObjectTriggerStates::iterator iter(m_triggerStates.begin());

	if (FindPlace(m_triggerStates, pEvent->m_audioTriggerInstanceId, iter))
	{
		switch (pEvent->m_state)
		{
		case EEventState::Playing:
			{
				SAudioTriggerInstanceState& audioTriggerInstanceState = iter->second;
				CRY_ASSERT(audioTriggerInstanceState.numPlayingEvents > 0);

				if (--(audioTriggerInstanceState.numPlayingEvents) == 0 &&
				    audioTriggerInstanceState.numLoadingEvents == 0 &&
				    (audioTriggerInstanceState.flags & ETriggerStatus::Starting) == 0)
				{
					ReportFinishedTriggerInstance(iter);
				}

				break;
			}
		case EEventState::Loading:
			{
				if (bSuccess)
				{
					ReportFinishedLoadingTriggerImpl(pEvent->m_audioTriggerImplId, true);
				}

				break;
			}
		case EEventState::Unloading:
			{
				if (bSuccess)
				{
					ReportFinishedLoadingTriggerImpl(pEvent->m_audioTriggerImplId, false);
				}

				break;
			}
		default:
			{
				// unknown event state!
				CRY_ASSERT(false);

				break;
			}
		}
	}
	else
	{
		if (pEvent->m_pTrigger != nullptr)
		{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			Cry::Audio::Log(ELogType::Warning, "Reported finished event on an inactive trigger %s", pEvent->m_pTrigger->m_name.c_str());
#else
			Cry::Audio::Log(ELogType::Warning, "Reported finished event on an inactive trigger %u", pEvent->m_pTrigger->GetId());
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Reported finished event on a trigger that does not exist anymore");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::GetStartedStandaloneFileRequestData(CATLStandaloneFile* const pStandaloneFile, CAudioRequest& request)
{
	ObjectStandaloneFileMap::const_iterator const iter(m_activeStandaloneFiles.find(pStandaloneFile));

	if (iter != m_activeStandaloneFiles.end())
	{
		SUserDataBase const& audioStandaloneFileData = iter->second;
		request.pOwner = audioStandaloneFileData.pOwnerOverride;
		request.pUserData = audioStandaloneFileData.pUserData;
		request.pUserDataOwner = audioStandaloneFileData.pUserDataOwner;
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportStartedStandaloneFile(
  CATLStandaloneFile* const pStandaloneFile,
  void* const pOwner,
  void* const pUserData,
  void* const pUserDataOwner)
{
	m_activeStandaloneFiles.emplace(pStandaloneFile, SUserDataBase(pOwner, pUserData, pUserDataOwner));
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportFinishedStandaloneFile(CATLStandaloneFile* const pStandaloneFile)
{
	m_activeStandaloneFiles.erase(pStandaloneFile);
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportFinishedLoadingTriggerImpl(TriggerImplId const audioTriggerImplId, bool const bLoad)
{
	if (bLoad)
	{
		m_triggerImplStates[audioTriggerImplId].flags |= ETriggerStatus::Loaded;
	}
	else
	{
		m_triggerImplStates[audioTriggerImplId].flags &= ~ETriggerStatus::Loaded;
	}
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CATLAudioObject::HandleExecuteTrigger(
  CATLTrigger const* const pTrigger,
  void* const pOwner,
  void* const pUserData,
  void* const pUserDataOwner,
  ERequestFlags const flags)
{
	ERequestStatus result = ERequestStatus::Failure;

	// Sets ETriggerStatus::Starting on this TriggerInstance to avoid
	// reporting TriggerFinished while the events are being started.
	ReportStartingTriggerInstance(s_triggerInstanceIdCounter, pTrigger->GetId());

	for (auto const pTriggerImpl : pTrigger->m_implPtrs)
	{
		CATLEvent* const pEvent = s_pEventManager->ConstructAudioEvent();
		ERequestStatus const activateResult = pTriggerImpl->Execute(m_pImplData, pEvent->m_pImplData);

		if (activateResult == ERequestStatus::Success || activateResult == ERequestStatus::Pending)
		{
			pEvent->m_pAudioObject = this;
			pEvent->m_pTrigger = pTrigger;
			pEvent->m_audioTriggerImplId = pTriggerImpl->m_audioTriggerImplId;
			pEvent->m_audioTriggerInstanceId = s_triggerInstanceIdCounter;
			pEvent->SetDataScope(pTrigger->GetDataScope());

			if (activateResult == ERequestStatus::Success)
			{
				pEvent->m_state = EEventState::Playing;
			}
			else if (activateResult == ERequestStatus::Pending)
			{
				pEvent->m_state = EEventState::Loading;
			}

			ReportStartedEvent(pEvent);

			// If at least one event fires, it is a success: the trigger has been activated.
			result = ERequestStatus::Success;
		}
		else
		{
			s_pEventManager->ReleaseEvent(pEvent);

			if (activateResult == ERequestStatus::SuccessDoNotTrack)
			{
				// An event which should not get tracked is a success.
				result = ERequestStatus::Success;
			}
		}
	}

	// Either removes the ETriggerStatus::Starting flag on this trigger instance or removes it if no event was started.
	ReportStartedTriggerInstance(s_triggerInstanceIdCounter++, pOwner, pUserData, pUserDataOwner, flags);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	if (result != ERequestStatus::Success)
	{
		// No TriggerImpl generated an active event.
		Cry::Audio::Log(ELogType::Warning, R"(Trigger "%s" failed on AudioObject "%s")", pTrigger->m_name.c_str(), m_name.c_str());
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	return result;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CATLAudioObject::HandleStopTrigger(CATLTrigger const* const pTrigger)
{
	ERequestStatus result = ERequestStatus::Failure;

	for (auto const pEvent : m_activeEvents)
	{
		if ((pEvent != nullptr) && pEvent->IsPlaying() && (pEvent->m_pTrigger == pTrigger))
		{
			result = pEvent->Stop();
		}
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CATLAudioObject::HandleSetSwitchState(CATLSwitch const* const pSwitch, CATLSwitchState const* const pState)
{
	ERequestStatus result = ERequestStatus::Failure;

	for (auto const pSwitchStateImpl : pState->m_implPtrs)
	{
		result = pSwitchStateImpl->Set(*this);
	}

	if (result == ERequestStatus::Success)
	{
		m_switchStates[pSwitch->GetId()] = pState->GetId();
	}
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, R"(Failed to set the ATLSwitch "%s" to ATLSwitchState "%s" on AudioObject "%s")", pSwitch->m_name.c_str(), pState->m_name.c_str(), m_name.c_str());
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	return result;

}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CATLAudioObject::HandleSetParameter(CParameter const* const pParameter, float const value)
{
	ERequestStatus result = ERequestStatus::Failure;

	for (auto const pParameterImpl : pParameter->m_implPtrs)
	{
		result = pParameterImpl->Set(*this, value);
	}

	if (result == ERequestStatus::Success)
	{
		m_parameters[pParameter->GetId()] = value;
	}
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, R"(Failed to set the Audio Parameter "%s" to %f on Audio Object "%s")", pParameter->m_name.c_str(), value, m_name.c_str());
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	return result;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CATLAudioObject::HandleSetEnvironment(CATLAudioEnvironment const* const pEnvironment, float const amount)
{
	ERequestStatus result = ERequestStatus::Failure;

	for (auto const pEnvImpl : pEnvironment->m_implPtrs)
	{
		if (m_pImplData->SetEnvironment(pEnvImpl->m_pImplData, amount) == ERequestStatus::Success)
		{
			result = ERequestStatus::Success;
		}
	}

	if (result == ERequestStatus::Success)
	{
		if (amount > 0.0f)
		{
			m_environments[pEnvironment->GetId()] = amount;
		}
		else
		{
			m_environments.erase(pEnvironment->GetId());
		}
	}
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, R"(Failed to set the ATLAudioEnvironment "%s" to %f on AudioObject "%s")", pEnvironment->m_name.c_str(), amount, m_name.c_str());
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	return result;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CATLAudioObject::StopAllTriggers()
{
	return m_pImplData->StopAllTriggers();
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetObstructionOcclusion(float const obstruction, float const occlusion)
{
	m_pImplData->SetObstructionOcclusion(obstruction, occlusion);
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CATLAudioObject::LoadTriggerAsync(CATLTrigger const* const pTrigger, bool const bLoad)
{
	ERequestStatus result = ERequestStatus::Failure;
	ControlId const audioTriggerId = pTrigger->GetId();

	for (auto const pTriggerImpl : pTrigger->m_implPtrs)
	{
		ETriggerStatus triggerStatus = ETriggerStatus::None;
		ObjectTriggerImplStates::const_iterator iPlace = m_triggerImplStates.end();

		if (FindPlaceConst(m_triggerImplStates, pTriggerImpl->m_audioTriggerImplId, iPlace))
		{
			triggerStatus = iPlace->second.flags;
		}
		CATLEvent* const pEvent = s_pEventManager->ConstructAudioEvent();
		ERequestStatus prepUnprepResult = ERequestStatus::Failure;

		if (bLoad)
		{
			if (((triggerStatus& ETriggerStatus::Loaded) == 0) && ((triggerStatus& ETriggerStatus::Loading) == 0))
			{
				prepUnprepResult = pTriggerImpl->m_pImplData->LoadAsync(pEvent->m_pImplData);
			}
		}
		else
		{
			if (((triggerStatus& ETriggerStatus::Loaded) != 0) && ((triggerStatus& ETriggerStatus::Unloading) == 0))
			{
				prepUnprepResult = pTriggerImpl->m_pImplData->UnloadAsync(pEvent->m_pImplData);
			}
		}

		if (prepUnprepResult == ERequestStatus::Success)
		{
			pEvent->m_pAudioObject = this;
			pEvent->m_pTrigger = pTrigger;
			pEvent->m_audioTriggerImplId = pTriggerImpl->m_audioTriggerImplId;
			pEvent->m_state = bLoad ? EEventState::Loading : EEventState::Unloading;
		}

		if (prepUnprepResult == ERequestStatus::Success)
		{
			pEvent->SetDataScope(pTrigger->GetDataScope());
			ReportStartedEvent(pEvent);
			result = ERequestStatus::Success;// if at least one event fires, it is a success
		}
		else
		{
			s_pEventManager->ReleaseEvent(pEvent);
		}
	}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	if (result != ERequestStatus::Success)
	{
		// No TriggerImpl produced an active event.
		Cry::Audio::Log(ELogType::Warning, R"(LoadTriggerAsync failed on AudioObject "%s")", m_name.c_str());
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	return result;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CATLAudioObject::HandleResetEnvironments(AudioEnvironmentLookup const& environmentsLookup)
{
	// Needs to be a copy as SetEnvironment removes from the map that we are iterating over.
	ObjectEnvironmentMap const environments = m_environments;
	ERequestStatus result = ERequestStatus::Success;

	for (auto const& environmentPair : environments)
	{
		CATLAudioEnvironment const* const pEnvironment = stl::find_in_map(environmentsLookup, environmentPair.first, nullptr);

		if (pEnvironment != nullptr)
		{
			if (HandleSetEnvironment(pEnvironment, 0.0f) != ERequestStatus::Success)
			{
				// If setting at least one Environment fails, we consider this a failure.
				result = ERequestStatus::Failure;
			}
		}
	}

	if (result == ERequestStatus::Success)
	{
		m_environments.clear();
	}
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, R"(Failed to Reset AudioEnvironments on AudioObject "%s")", m_name.c_str());
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	return result;
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReportFinishedTriggerInstance(ObjectTriggerStates::iterator& iter)
{
	SAudioTriggerInstanceState& audioTriggerInstanceState = iter->second;
	SAudioCallbackManagerRequestData<EAudioCallbackManagerRequestType::ReportFinishedTriggerInstance> requestData(audioTriggerInstanceState.audioTriggerId);
	CAudioRequest request(&requestData);
	request.pObject = this;
	request.pOwner = audioTriggerInstanceState.pOwnerOverride;
	request.pUserData = audioTriggerInstanceState.pUserData;
	request.pUserDataOwner = audioTriggerInstanceState.pUserDataOwner;

	if ((audioTriggerInstanceState.flags & ETriggerStatus::CallbackOnExternalThread) != 0)
	{
		request.flags = ERequestFlags::CallbackOnExternalOrCallingThread;
	}
	else if ((audioTriggerInstanceState.flags & ETriggerStatus::CallbackOnAudioThread) != 0)
	{
		request.flags = ERequestFlags::CallbackOnAudioThread;
	}

	s_pAudioSystem->PushRequest(request);

	if ((audioTriggerInstanceState.flags & ETriggerStatus::Loaded) != 0)
	{
		// If the trigger instance was manually loaded -- keep it
		audioTriggerInstanceState.flags &= ~ETriggerStatus::Playing;
	}
	else
	{
		// If the trigger instance wasn't loaded -- kill it
		m_triggerStates.erase(iter);
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::PushRequest(SAudioRequestData const& requestData, SRequestUserData const& userData)
{
	CAudioRequest const request(userData.flags, this, userData.pOwner, userData.pUserData, userData.pUserDataOwner, &requestData);
	s_pAudioSystem->PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
bool CATLAudioObject::HasActiveData(CATLAudioObject const* const pAudioObject) const
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
		CATLStandaloneFile const* const pStandaloneFile = standaloneFilePair.first;

		if (pStandaloneFile->IsPlaying())
		{
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::Update(
  float const deltaTime,
  float const distanceToListener,
  Vec3 const& listenerPosition,
  Vec3 const& listenerVelocity,
  bool const listenerMoved)
{
	m_propagationProcessor.Update(distanceToListener, listenerPosition, m_flags);

	if (m_propagationProcessor.HasNewOcclusionValues())
	{
		SATLSoundPropagationData propagationData;
		m_propagationProcessor.GetPropagationData(propagationData);
		m_pImplData->SetObstructionOcclusion(propagationData.obstruction, propagationData.occlusion);
	}

	UpdateControls(deltaTime, distanceToListener, listenerPosition, listenerVelocity, listenerMoved);
	m_pImplData->Update();
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ProcessPhysicsRay(CAudioRayInfo* const pAudioRayInfo)
{
	m_propagationProcessor.ProcessPhysicsRay(pAudioRayInfo);
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CATLAudioObject::HandleSetTransformation(CObjectTransformation const& transformation, float const distanceToListener)
{
	float const threshold = distanceToListener * g_cvars.m_positionUpdateThresholdMultiplier;

	if (!m_attributes.transformation.IsEquivalent(transformation, threshold))
	{
		m_attributes.transformation = transformation;
		m_flags |= EObjectFlags::MovingOrDecaying;

		// Immediately propagate the new transformation down to the middleware to prevent executing a trigger before its transformation was set.
		// Calculation of potentially tracked absolute and relative velocities can be safely delayed to next audio frame.
		m_pImplData->Set3DAttributes(m_attributes);
	}

	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::HandleSetOcclusionType(EOcclusionType const calcType, Vec3 const& listenerPosition)
{
	CRY_ASSERT(calcType != EOcclusionType::None);
	m_propagationProcessor.SetOcclusionType(calcType, listenerPosition);
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ReleasePendingRays()
{
	m_propagationProcessor.ReleasePendingRays();
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CATLAudioObject::HandlePlayFile(CATLStandaloneFile* const pFile, void* const pOwner, void* const pUserData, void* const pUserDataOwner)
{
	ERequestStatus status = ERequestStatus::Failure;

	ERequestStatus const tempStatus = m_pImplData->PlayFile(pFile->m_pImplData);

	if (tempStatus == ERequestStatus::Success || tempStatus == ERequestStatus::Pending)
	{
		if (tempStatus == ERequestStatus::Success)
		{
			pFile->m_state = EAudioStandaloneFileState::Playing;
		}
		else if (tempStatus == ERequestStatus::Pending)
		{
			pFile->m_state = EAudioStandaloneFileState::Loading;
		}

		pFile->m_pAudioObject = this;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		pFile->m_pOwner = pOwner;
		pFile->m_pUserData = pUserData;
		pFile->m_pUserDataOwner = pUserDataOwner;
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

		ReportStartedStandaloneFile(pFile, pOwner, pUserData, pUserDataOwner);

		// It's a success in both cases.
		status = ERequestStatus::Success;
	}
	else
	{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		Cry::Audio::Log(ELogType::Warning, R"(PlayFile failed with "%s" on AudioObject "%s")", pFile->m_hashedFilename.GetText().c_str(), m_name.c_str());
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

		s_pStandaloneFileManager->ReleaseStandaloneFile(pFile);
	}

	return status;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CATLAudioObject::HandleStopFile(char const* const szFile)
{
	ERequestStatus status = ERequestStatus::Failure;
	CHashedString const hashedFilename(szFile);
	auto iter = m_activeStandaloneFiles.cbegin();
	auto iterEnd = m_activeStandaloneFiles.cend();

	while (iter != iterEnd)
	{
		CATLStandaloneFile* const pStandaloneFile = iter->first;

		if (pStandaloneFile != nullptr && pStandaloneFile->m_hashedFilename == hashedFilename)
		{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			if (pStandaloneFile->m_state != EAudioStandaloneFileState::Playing)
			{
				char const* szState = "unknown";

				switch (pStandaloneFile->m_state)
				{
				case EAudioStandaloneFileState::Playing:
					szState = "playing";
					break;
				case EAudioStandaloneFileState::Loading:
					szState = "loading";
					break;
				case EAudioStandaloneFileState::Stopping:
					szState = "stopping";
					break;
				default:
					szState = "unknown";
					break;
				}

				Cry::Audio::Log(ELogType::Warning, R"(Request to stop a standalone audio file that is not playing! State: "%s")", szState);
			}
#endif  //INCLUDE_AUDIO_PRODUCTION_CODE

			ERequestStatus const tempStatus = m_pImplData->StopFile(pStandaloneFile->m_pImplData);

			status = ERequestStatus::Success;

			if (tempStatus != ERequestStatus::Pending)
			{
				ReportFinishedStandaloneFile(pStandaloneFile);
				s_pStandaloneFileManager->ReleaseStandaloneFile(pStandaloneFile);

				iter = m_activeStandaloneFiles.begin();
				iterEnd = m_activeStandaloneFiles.end();
				continue;
			}
			else
			{
				pStandaloneFile->m_state = EAudioStandaloneFileState::Stopping;
			}
		}

		++iter;
	}

	return status;
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::Init(char const* const szName, Impl::IObject* const pImplData, Vec3 const& listenerPosition, EntityId entityId)
{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	m_name = szName;
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE

	m_entityId = entityId;
	m_pImplData = pImplData;
	m_propagationProcessor.Init(this, listenerPosition);
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::UpdateControls(
  float const deltaTime,
  float const distanceToListener,
  Vec3 const& listenerPosition,
  Vec3 const& listenerVelocity,
  bool const listenerMoved)
{
	if ((m_flags& EObjectFlags::MovingOrDecaying) != 0)
	{
		Vec3 const deltaPos(m_attributes.transformation.GetPosition() - m_previousAttributes.transformation.GetPosition());

		if (!deltaPos.IsZero())
		{
			m_attributes.velocity = deltaPos / deltaTime;
			m_previousAttributes.transformation.SetPosition(m_attributes.transformation.GetPosition());
		}
		else if (!m_attributes.velocity.IsZero())
		{
			// We did not move last frame, begin exponential decay towards zero.
			float const decay = std::max(1.0f - deltaTime / 0.05f, 0.0f);
			m_attributes.velocity *= decay;

			if (m_attributes.velocity.GetLengthSquared() < FloatEpsilon)
			{
				m_attributes.velocity = ZERO;
				m_flags &= ~EObjectFlags::MovingOrDecaying;
			}
		}

		m_pImplData->Set3DAttributes(m_attributes);

		if ((m_flags& EObjectFlags::TrackAbsoluteVelocity) != 0)
		{
			float const absoluteVelocity = m_attributes.velocity.GetLength();

			if (absoluteVelocity == 0.0f || fabs(absoluteVelocity - m_previousAbsoluteVelocity) > g_cvars.m_velocityTrackingThreshold)
			{
				m_previousAbsoluteVelocity = absoluteVelocity;

				SAudioObjectRequestData<EAudioObjectRequestType::SetParameter> requestData(AbsoluteVelocityParameterId, absoluteVelocity);
				CAudioRequest request(&requestData);
				request.pObject = this;
				s_pAudioSystem->PushRequest(request);
			}
		}

		if ((m_flags& EObjectFlags::TrackRelativeVelocity) != 0)
		{
			// Approaching positive, departing negative value.
			float relativeVelocity = 0.0f;

			if ((m_flags& EObjectFlags::MovingOrDecaying) != 0 && !listenerMoved)
			{
				relativeVelocity = -m_attributes.velocity.Dot((m_attributes.transformation.GetPosition() - listenerPosition).GetNormalized());
			}
			else if ((m_flags& EObjectFlags::MovingOrDecaying) != 0 && listenerMoved)
			{
				Vec3 const relativeVelocityVec(m_attributes.velocity - listenerVelocity);
				relativeVelocity = -relativeVelocityVec.Dot((m_attributes.transformation.GetPosition() - listenerPosition).GetNormalized());
			}

			TryToSetRelativeVelocity(relativeVelocity);
		}
	}
	else if ((m_flags& EObjectFlags::TrackRelativeVelocity) != 0)
	{
		// Approaching positive, departing negative value.
		if (listenerMoved)
		{
			float const relativeVelocity = listenerVelocity.Dot((m_attributes.transformation.GetPosition() - listenerPosition).GetNormalized());
			TryToSetRelativeVelocity(relativeVelocity);
		}
		else if (m_previousRelativeVelocity != 0.0f)
		{
			TryToSetRelativeVelocity(0.0f);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::TryToSetRelativeVelocity(float const relativeVelocity)
{
	if (relativeVelocity == 0.0f || fabs(relativeVelocity - m_previousRelativeVelocity) > g_cvars.m_velocityTrackingThreshold)
	{
		m_previousRelativeVelocity = relativeVelocity;

		SAudioObjectRequestData<EAudioObjectRequestType::SetParameter> requestData(RelativeVelocityParameterId, relativeVelocity);
		CAudioRequest request(&requestData);
		request.pObject = this;
		s_pAudioSystem->PushRequest(request);
	}
}

///////////////////////////////////////////////////////////////////////////
bool CATLAudioObject::CanBeReleased() const
{
	return (m_flags& EObjectFlags::InUse) == 0 &&
	       m_activeEvents.empty() &&
	       m_activeStandaloneFiles.empty() &&
	       !m_propagationProcessor.HasPendingRays() &&
	       m_numPendingSyncCallbacks == 0;
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetFlag(EObjectFlags const flag)
{
	m_flags |= flag;
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::RemoveFlag(EObjectFlags const flag)
{
	m_flags &= ~flag;
}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
float const CATLAudioObject::CStateDebugDrawData::s_minAlpha = 0.5f;
float const CATLAudioObject::CStateDebugDrawData::s_maxAlpha = 1.0f;
int const CATLAudioObject::CStateDebugDrawData::s_maxToMinUpdates = 100;
static char const* const s_szOcclusionTypes[] = { "None", "Ignore", "Adaptive", "Low", "Medium", "High" };

///////////////////////////////////////////////////////////////////////////
CATLAudioObject::CStateDebugDrawData::CStateDebugDrawData(SwitchStateId const audioSwitchState)
	: m_currentState(audioSwitchState)
	, m_currentAlpha(s_maxAlpha)
{
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::CStateDebugDrawData::Update(SwitchStateId const audioSwitchState)
{
	if ((audioSwitchState == m_currentState) && (m_currentAlpha > s_minAlpha))
	{
		m_currentAlpha -= (s_maxAlpha - s_minAlpha) / s_maxToMinUpdates;
	}
	else if (audioSwitchState != m_currentState)
	{
		m_currentState = audioSwitchState;
		m_currentAlpha = s_maxAlpha;
	}
}

using TriggerCountMap = std::map<ControlId const, size_t>;

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::DrawDebugInfo(
  IRenderAuxGeom& auxGeom,
  Vec3 const& listenerPosition,
  AudioTriggerLookup const& triggers,
  AudioParameterLookup const& parameters,
  AudioSwitchLookup const& switches,
  AudioPreloadRequestLookup const& preloadRequests,
  AudioEnvironmentLookup const& environments) const
{
	Vec3 const& position = m_attributes.transformation.GetPosition();
	Vec3 screenPos(ZERO);

	if (IRenderer* const pRenderer = gEnv->pRenderer)
	{
		auto const& camera = GetISystem()->GetViewCamera();
		pRenderer->ProjectToScreen(position.x, position.y, position.z, &screenPos.x, &screenPos.y, &screenPos.z);

		screenPos.x = screenPos.x * 0.01f * camera.GetViewSurfaceX();
		screenPos.y = screenPos.y * 0.01f * camera.GetViewSurfaceZ();
	}
	else
	{
		screenPos.z = -1.0f;
	}

	if ((screenPos.z >= 0.0f) && (screenPos.z <= 1.0f))
	{
		float const distance = position.GetDistance(listenerPosition);

		if ((g_cvars.m_debugDistance <= 0.0f) || ((g_cvars.m_debugDistance > 0.0f) && (distance <= g_cvars.m_debugDistance)))
		{
			float const fontSize = 1.35f;
			float const lineHeight = 14.0f;
			float offsetOnY = 0.0f;

			// Check if text filter is enabled.
			CryFixedStringT<MaxControlNameLength> lowerCaseSearchString(g_cvars.m_pDebugFilter->GetString());
			lowerCaseSearchString.MakeLower();
			bool const bTextFilterDisabled = (lowerCaseSearchString.empty() || (lowerCaseSearchString.compareNoCase("0") == 0));
			bool const bShowSphere = (g_cvars.m_drawAudioDebug & EAudioDebugDrawFilter::ShowSpheres) != 0;
			bool const bShowLabel = (g_cvars.m_drawAudioDebug & EAudioDebugDrawFilter::ShowObjectLabel) != 0;
			bool const bShowTriggers = (g_cvars.m_drawAudioDebug & EAudioDebugDrawFilter::ShowObjectTriggers) != 0;
			bool const bShowStandaloneFiles = (g_cvars.m_drawAudioDebug & EAudioDebugDrawFilter::ShowObjectStandaloneFiles) != 0;
			bool const bShowStates = (g_cvars.m_drawAudioDebug & EAudioDebugDrawFilter::ShowObjectStates) != 0;
			bool const bShowParameters = (g_cvars.m_drawAudioDebug & EAudioDebugDrawFilter::ShowObjectParameters) != 0;
			bool const bShowEnvironments = (g_cvars.m_drawAudioDebug & EAudioDebugDrawFilter::ShowObjectEnvironments) != 0;
			bool const bShowDistance = (g_cvars.m_drawAudioDebug & EAudioDebugDrawFilter::ShowObjectDistance) != 0;
			bool const bShowOcclusionRayLabel = (g_cvars.m_drawAudioDebug & EAudioDebugDrawFilter::ShowOcclusionRayLabels) != 0;
			bool const bFilterAllObjectInfo = (g_cvars.m_drawAudioDebug & EAudioDebugDrawFilter::FilterAllObjectInfo) != 0;

			// Check if object name matches text filter.
			CATLAudioObject* const pAudioObject = const_cast<CATLAudioObject*>(this);
			char const* const szObjectName = pAudioObject->m_name.c_str();
			bool bObjectNameMatchesFilter = false;

			if (bShowLabel || bFilterAllObjectInfo)
			{
				CryFixedStringT<MaxControlNameLength> lowerCaseObjectName(szObjectName);
				lowerCaseObjectName.MakeLower();
				bObjectNameMatchesFilter = (lowerCaseObjectName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos);
			}

			// Check if any trigger matches text filter.
			bool bTriggerMatchesFilter = false;
			std::vector<CryFixedStringT<MaxMiscStringLength>> triggerInfo;

			if ((bShowTriggers && !m_triggerStates.empty()) || bFilterAllObjectInfo)
			{
				TriggerCountMap triggerCounts;

				for (auto const& triggerStatesPair : m_triggerStates)
				{
					++(triggerCounts[triggerStatesPair.second.audioTriggerId]);
				}

				for (auto const& triggerCountsPair : triggerCounts)
				{
					CATLTrigger const* const pTrigger = stl::find_in_map(triggers, triggerCountsPair.first, nullptr);

					if (pTrigger != nullptr)
					{
						char const* const szTriggerName = pTrigger->m_name.c_str();
						CryFixedStringT<MaxControlNameLength> lowerCaseTriggerName(szTriggerName);
						lowerCaseTriggerName.MakeLower();

						if (lowerCaseTriggerName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos)
						{
							bTriggerMatchesFilter = true;
						}

						CryFixedStringT<MaxMiscStringLength> debugText;
						size_t const numInstances = triggerCountsPair.second;

						if (numInstances == 1)
						{
							debugText.Format("%s\n", szTriggerName);
						}
						else
						{
							debugText.Format("%s: %" PRISIZE_T "\n", szTriggerName, numInstances);
						}

						triggerInfo.emplace_back(debugText);
					}
				}
			}

			// Check if any standalone file matches text filter.
			bool bStandaloneFileMatchesFilter = false;
			std::vector<CryFixedStringT<MaxMiscStringLength>> standaloneFileInfo;

			if ((bShowStandaloneFiles && !m_activeStandaloneFiles.empty()) || bFilterAllObjectInfo)
			{
				std::map<CHashedString, size_t> numStandaloneFiles;

				for (auto const& standaloneFilePair : m_activeStandaloneFiles)
				{
					++(numStandaloneFiles[standaloneFilePair.first->m_hashedFilename]);
				}

				for (auto const& numInstancesPair : numStandaloneFiles)
				{
					char const* const szStandaloneFileName = numInstancesPair.first.GetText().c_str();
					CryFixedStringT<MaxControlNameLength> lowerCaseStandaloneFileName(szStandaloneFileName);
					lowerCaseStandaloneFileName.MakeLower();

					if (lowerCaseStandaloneFileName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos)
					{
						bStandaloneFileMatchesFilter = true;
					}

					CryFixedStringT<MaxMiscStringLength> debugText;
					size_t const numInstances = numInstancesPair.second;

					if (numInstances == 1)
					{
						debugText.Format("%s\n", szStandaloneFileName);
					}
					else
					{
						debugText.Format("%s: %" PRISIZE_T "\n", szStandaloneFileName, numInstances);
					}

					standaloneFileInfo.emplace_back(debugText);
				}
			}

			// Check if any state or switch matches text filter.
			bool bStateSwitchMatchesFilter = false;
			std::map<CATLSwitch const* const, CATLSwitchState const* const> switchStateInfo;

			if ((bShowStates && !m_switchStates.empty()) || bFilterAllObjectInfo)
			{
				for (auto const& switchStatePair : m_switchStates)
				{
					CATLSwitch const* const pSwitch = stl::find_in_map(switches, switchStatePair.first, nullptr);

					if (pSwitch != nullptr)
					{
						CATLSwitchState const* const pSwitchState = stl::find_in_map(pSwitch->audioSwitchStates, switchStatePair.second, nullptr);

						if (pSwitchState != nullptr)
						{
							if (!pSwitch->m_name.empty() && !pSwitchState->m_name.empty())
							{
								char const* const szSwitchName = pSwitch->m_name.c_str();
								CryFixedStringT<MaxControlNameLength> lowerCaseSwitchName(szSwitchName);
								lowerCaseSwitchName.MakeLower();
								char const* const szStateName = pSwitchState->m_name.c_str();
								CryFixedStringT<MaxControlNameLength> lowerCaseStateName(szStateName);
								lowerCaseStateName.MakeLower();

								if ((lowerCaseSwitchName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos) ||
								    (lowerCaseStateName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos))
								{
									bStateSwitchMatchesFilter = true;
								}

								switchStateInfo.insert(std::make_pair(pSwitch, pSwitchState));
							}
						}
					}
				}
			}

			// Check if any parameter matches text filter.
			bool bParameterMatchesFilter = false;
			std::map<char const* const, float const> parameterInfo;

			if ((bShowParameters && !m_parameters.empty()) || bFilterAllObjectInfo)
			{
				for (auto const& parameterPair : m_parameters)
				{
					CParameter const* const pParameter = stl::find_in_map(parameters, parameterPair.first, nullptr);

					if (pParameter != nullptr)
					{
						char const* const szParameterName = pParameter->m_name.c_str();
						CryFixedStringT<MaxControlNameLength> lowerCaseParameterName(szParameterName);
						lowerCaseParameterName.MakeLower();

						if (lowerCaseParameterName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos)
						{
							bParameterMatchesFilter = true;
						}

						parameterInfo.insert(std::make_pair(szParameterName, parameterPair.second));
					}
				}
			}

			// Check if any environment matches text filter.
			bool bEnvironmentMatchesFilter = false;
			std::map<char const* const, float const> environmentInfo;

			if ((bShowEnvironments && !m_environments.empty()) || bFilterAllObjectInfo)
			{
				for (auto const& environmentPair : m_environments)
				{
					CATLAudioEnvironment const* const pEnvironment = stl::find_in_map(environments, environmentPair.first, nullptr);

					if (pEnvironment != nullptr)
					{
						char const* const szEnvironmentName = pEnvironment->m_name.c_str();
						CryFixedStringT<MaxControlNameLength> lowerCaseEnvironmentName(szEnvironmentName);
						lowerCaseEnvironmentName.MakeLower();

						if (lowerCaseEnvironmentName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos)
						{
							bEnvironmentMatchesFilter = true;
						}

						environmentInfo.insert(std::make_pair(szEnvironmentName, environmentPair.second));
					}
				}
			}

			// Check if any object info text matches text filter.
			bool const bShowObjectDebugInfo =
			  bTextFilterDisabled ||
			  bObjectNameMatchesFilter ||
			  bTriggerMatchesFilter ||
			  bStandaloneFileMatchesFilter ||
			  bStateSwitchMatchesFilter ||
			  bParameterMatchesFilter ||
			  bEnvironmentMatchesFilter;

			if (bShowObjectDebugInfo)
			{
				if (bShowSphere)
				{
					SAuxGeomRenderFlags const previousRenderFlags = auxGeom.GetRenderFlags();
					SAuxGeomRenderFlags newRenderFlags(e_Def3DPublicRenderflags | e_AlphaBlended);
					newRenderFlags.SetCullMode(e_CullModeNone);
					auxGeom.SetRenderFlags(newRenderFlags);
					float const radius = 0.15f;
					auxGeom.DrawSphere(position, radius, ColorB(255, 1, 1, 255));
					auxGeom.SetRenderFlags(previousRenderFlags);
				}

				if (bShowLabel)
				{
					bool const bHasActiveData = HasActiveData(pAudioObject);
					bool const bDraw = (g_cvars.m_hideInactiveAudioObjects == 0) || ((g_cvars.m_hideInactiveAudioObjects > 0) && bHasActiveData);

					if (bDraw)
					{
						bool const bIsVirtual = (pAudioObject->GetFlags() & EObjectFlags::Virtual) != 0;
						static float const objectActiveColor[4] = { 0.9f, 0.9f, 0.9f, 0.9f };
						static float const objectInactiveColor[4] = { 0.5f, 0.5f, 0.5f, 0.9f };
						static float const objectVirtualColor[4] = { 0.1f, 0.8f, 0.8f, 0.9f };

						auxGeom.Draw2dLabel(
						  screenPos.x,
						  screenPos.y + offsetOnY,
						  fontSize,
						  bIsVirtual ? objectVirtualColor : (bHasActiveData ? objectActiveColor : objectInactiveColor),
						  false,
						  "%s",
						  szObjectName);

						offsetOnY += lineHeight;
					}
				}

				if (bShowTriggers && !triggerInfo.empty())
				{
					float const triggerTextColor[4] = { 0.1f, 0.7f, 0.1f, 0.9f };

					for (auto const& debugText : triggerInfo)
					{
						auxGeom.Draw2dLabel(
						  screenPos.x,
						  screenPos.y + offsetOnY,
						  fontSize,
						  triggerTextColor,
						  false,
						  "%s",
						  debugText.c_str());

						offsetOnY += lineHeight;
					}
				}

				if (bShowStandaloneFiles && !standaloneFileInfo.empty())
				{
					float const standalonFileTextColor[4] = { 0.9f, 0.9f, 0.0f, 0.9f };

					for (auto const& debugText : standaloneFileInfo)
					{
						auxGeom.Draw2dLabel(
						  screenPos.x,
						  screenPos.y + offsetOnY,
						  fontSize,
						  standalonFileTextColor,
						  false,
						  "%s",
						  debugText.c_str());

						offsetOnY += lineHeight;
					}
				}

				if (bShowStates && !switchStateInfo.empty())
				{
					for (auto const& switchStatePair : switchStateInfo)
					{
						auto const pSwitch = switchStatePair.first;
						auto const pSwitchState = switchStatePair.second;

						CStateDebugDrawData& drawData = m_stateDrawInfoMap.emplace(std::piecewise_construct, std::forward_as_tuple(pSwitch->GetId()), std::forward_as_tuple(pSwitchState->GetId())).first->second;
						drawData.Update(pSwitchState->GetId());
						float const switchTextColor[4] = { 0.8f, 0.3f, 0.6f, drawData.m_currentAlpha };

						auxGeom.Draw2dLabel(
						  screenPos.x,
						  screenPos.y + offsetOnY,
						  fontSize,
						  switchTextColor,
						  false,
						  "%s: %s\n",
						  pSwitch->m_name.c_str(),
						  pSwitchState->m_name.c_str());

						offsetOnY += lineHeight;
					}
				}

				if (bShowParameters && !parameterInfo.empty())
				{
					static float const parameterTextColor[4] = { 0.4f, 0.4f, 1.0f, 1.0f };

					for (auto const& parameterPair : parameterInfo)
					{
						auxGeom.Draw2dLabel(
						  screenPos.x,
						  screenPos.y + offsetOnY,
						  fontSize,
						  parameterTextColor,
						  false,
						  "%s: %2.2f\n",
						  parameterPair.first,
						  parameterPair.second);

						offsetOnY += lineHeight;
					}
				}

				if (bShowEnvironments && !environmentInfo.empty())
				{
					static float const environmentTextColor[4] = { 9.0f, 0.5f, 0.0f, 0.7f };

					for (auto const& environmentPair : environmentInfo)
					{
						auxGeom.Draw2dLabel(
						  screenPos.x,
						  screenPos.y + offsetOnY,
						  fontSize,
						  environmentTextColor,
						  false,
						  "%s: %.2f\n",
						  environmentPair.first,
						  environmentPair.second);

						offsetOnY += lineHeight;
					}
				}

				if (bShowDistance)
				{
					static float const distanceTextColor[4] = { 0.9f, 0.9f, 0.9f, 0.9f };

					auxGeom.Draw2dLabel(
					  screenPos.x,
					  screenPos.y + offsetOnY,
					  fontSize,
					  distanceTextColor,
					  false,
					  "Dist: %4.1fm",
					  distance);

					offsetOnY += lineHeight;
				}

				if (bShowOcclusionRayLabel)
				{
					EOcclusionType const occlusionType = m_propagationProcessor.GetOcclusionType();
					SATLSoundPropagationData propagationData;
					m_propagationProcessor.GetPropagationData(propagationData);

					bool const bHasActiveData = HasActiveData(pAudioObject);
					bool const bDraw = (g_cvars.m_hideInactiveAudioObjects == 0) || ((g_cvars.m_hideInactiveAudioObjects > 0) && bHasActiveData);

					if (bDraw)
					{
						CryFixedStringT<MaxMiscStringLength> debugText;

						if (distance < g_cvars.m_occlusionMaxDistance)
						{
							if (occlusionType == EOcclusionType::Adaptive)
							{
								debugText.Format(
								  "%s(%s)",
								  s_szOcclusionTypes[IntegralValue(occlusionType)],
								  s_szOcclusionTypes[IntegralValue(m_propagationProcessor.GetOcclusionTypeWhenAdaptive())]);
							}
							else
							{
								debugText.Format("%s", s_szOcclusionTypes[IntegralValue(occlusionType)]);
							}
						}
						else
						{
							debugText.Format("Ignore (exceeded activity range)");
						}

						bool const bIsVirtual = (pAudioObject->GetFlags() & EObjectFlags::Virtual) != 0;
						float const activeRayLabelColor[4] = { propagationData.occlusion, 1.0f - propagationData.occlusion, 0.0f, 0.9f };
						static float const ignoredRayLabelColor[4] = { 0.5f, 0.5f, 0.5f, 0.9f };
						static float const virtualRayLabelColor[4] = { 0.1f, 0.8f, 0.8f, 0.9f };

						auxGeom.Draw2dLabel(
						  screenPos.x,
						  screenPos.y + offsetOnY,
						  fontSize,
						  ((occlusionType != EOcclusionType::None) && (occlusionType != EOcclusionType::Ignore)) ? (bIsVirtual ? virtualRayLabelColor : activeRayLabelColor) : ignoredRayLabelColor,
						  false,
						  "Occl: %3.2f | Type: %s", // Add obstruction again once the engine supports it.
						  propagationData.occlusion,
						  debugText.c_str());

						offsetOnY += lineHeight;
					}
				}

				m_propagationProcessor.DrawDebugInfo(auxGeom, m_flags, listenerPosition);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ForceImplementationRefresh(
  AudioTriggerLookup const& triggers,
  AudioParameterLookup const& parameters,
  AudioSwitchLookup const& switches,
  AudioEnvironmentLookup const& environments,
  bool const bSet3DAttributes)
{
	if (bSet3DAttributes)
	{
		m_pImplData->Set3DAttributes(m_attributes);
	}

	// Parameters
	ObjectParameterMap const audioParameters = m_parameters;

	for (auto const& parameterPair : audioParameters)
	{
		CParameter const* const pParameter = stl::find_in_map(parameters, parameterPair.first, nullptr);

		if (pParameter != nullptr)
		{
			ERequestStatus const result = HandleSetParameter(pParameter, parameterPair.second);

			if (result != ERequestStatus::Success)
			{
				Cry::Audio::Log(ELogType::Warning, R"(Parameter "%s" failed during audio middleware switch on AudioObject "%s")", pParameter->m_name.c_str(), m_name.c_str());
			}
		}
	}

	// Switches
	ObjectStateMap const audioSwitches = m_switchStates;

	for (auto const& switchPair : audioSwitches)
	{
		CATLSwitch const* const pSwitch = stl::find_in_map(switches, switchPair.first, nullptr);

		if (pSwitch != nullptr)
		{
			CATLSwitchState const* const pState = stl::find_in_map(pSwitch->audioSwitchStates, switchPair.second, nullptr);

			if (pState != nullptr)
			{
				ERequestStatus const result = HandleSetSwitchState(pSwitch, pState);

				if (result != ERequestStatus::Success)
				{
					Cry::Audio::Log(ELogType::Warning, R"(SwitchStateImpl "%s" : "%s" failed during audio middleware switch on AudioObject "%s")", pSwitch->m_name.c_str(), pState->m_name.c_str(), m_name.c_str());
				}
			}
		}
	}

	// Environments
	ObjectEnvironmentMap const audioEnvironments = m_environments;

	for (auto const& environmentPair : audioEnvironments)
	{
		CATLAudioEnvironment const* const pEnvironment = stl::find_in_map(environments, environmentPair.first, nullptr);

		if (pEnvironment != nullptr)
		{
			ERequestStatus const result = HandleSetEnvironment(pEnvironment, environmentPair.second);

			if (result != ERequestStatus::Success)
			{
				Cry::Audio::Log(ELogType::Warning, R"(Environment "%s" failed during audio middleware switch on AudioObject "%s")", pEnvironment->m_name.c_str(), m_name.c_str());
			}
		}
	}

	// Last re-execute its active triggers and standalone files.
	ObjectTriggerStates& triggerStates = m_triggerStates;
	auto it = triggerStates.cbegin();
	auto end = triggerStates.cend();

	while (it != end)
	{
		auto const& triggerState = *it;
		CATLTrigger const* const pTrigger = stl::find_in_map(triggers, triggerState.second.audioTriggerId, nullptr);

		if (pTrigger != nullptr)
		{
			if (!pTrigger->m_implPtrs.empty())
			{
				for (auto const pTriggerImpl : pTrigger->m_implPtrs)
				{

					CATLEvent* const pEvent = s_pEventManager->ConstructAudioEvent();
					ERequestStatus const activateResult = pTriggerImpl->Execute(m_pImplData, pEvent->m_pImplData);

					if (activateResult == ERequestStatus::Success || activateResult == ERequestStatus::Pending)
					{
						pEvent->m_pAudioObject = this;
						pEvent->m_pTrigger = pTrigger;
						pEvent->m_audioTriggerImplId = pTriggerImpl->m_audioTriggerImplId;
						pEvent->m_audioTriggerInstanceId = triggerState.first;
						pEvent->SetDataScope(pTrigger->GetDataScope());

						if (activateResult == ERequestStatus::Success)
						{
							pEvent->m_state = EEventState::Playing;
						}
						else if (activateResult == ERequestStatus::Pending)
						{
							pEvent->m_state = EEventState::Loading;
						}
						ReportStartedEvent(pEvent);
					}
					else
					{
						Cry::Audio::Log(ELogType::Warning, R"(TriggerImpl "%s" failed during audio middleware switch on AudioObject "%s")", pTrigger->m_name.c_str(), m_name.c_str());
						s_pEventManager->ReleaseEvent(pEvent);
					}
				}
			}
			else
			{
				// The middleware has no connections set up.
				// Stop the event in this case.
				Cry::Audio::Log(ELogType::Warning, R"(No trigger connections found during audio middleware switch for "%s" on "%s")", pTrigger->m_name.c_str(), m_name.c_str());
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

	ObjectStandaloneFileMap const& activeStandaloneFiles = m_activeStandaloneFiles;

	for (auto const& standaloneFilePair : activeStandaloneFiles)
	{
		CATLStandaloneFile* const pStandaloneFile = standaloneFilePair.first;

		if (pStandaloneFile != nullptr)
		{
			CRY_ASSERT(pStandaloneFile->m_state == EAudioStandaloneFileState::Playing);

			ERequestStatus const status = HandlePlayFile(pStandaloneFile, pStandaloneFile->m_pOwner, pStandaloneFile->m_pUserData, pStandaloneFile->m_pUserDataOwner);

			if (status == ERequestStatus::Success || status == ERequestStatus::Pending)
			{
				if (status == ERequestStatus::Success)
				{
					pStandaloneFile->m_state = EAudioStandaloneFileState::Playing;
				}
				else if (status == ERequestStatus::Pending)
				{
					pStandaloneFile->m_state = EAudioStandaloneFileState::Loading;
				}
			}
			else
			{
				Cry::Audio::Log(ELogType::Warning, R"(PlayFile failed with "%s" on AudioObject "%s")", pStandaloneFile->m_hashedFilename.GetText().c_str(), m_name.c_str());
			}
		}
		else
		{
			Cry::Audio::Log(ELogType::Error, "Retrigger active standalone audio files failed on instance: %u and file: %u as m_audioStandaloneFileMgr.LookupID() returned nullptr!", standaloneFilePair.first, standaloneFilePair.second);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CATLAudioObject::HandleSetName(char const* const szName)
{
	m_name = szName;
	return m_pImplData->SetName(szName);
}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ExecuteTrigger(ControlId const triggerId, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::ExecuteTrigger> requestData(triggerId);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::StopTrigger(ControlId const triggerId /* = CryAudio::InvalidControlId */, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	if (triggerId != InvalidControlId)
	{
		SAudioObjectRequestData<EAudioObjectRequestType::StopTrigger> requestData(triggerId);
		PushRequest(requestData, userData);
	}
	else
	{
		SAudioObjectRequestData<EAudioObjectRequestType::StopAllTriggers> requestData;
		PushRequest(requestData, userData);
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetTransformation(CObjectTransformation const& transformation, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::SetTransformation> requestData(transformation);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::SetParameter> requestData(parameterId, value);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetSwitchState(ControlId const audioSwitchId, SwitchStateId const audioSwitchStateId, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::SetSwitchState> requestData(audioSwitchId, audioSwitchStateId);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetEnvironment(EnvironmentId const audioEnvironmentId, float const amount, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::SetEnvironment> requestData(audioEnvironmentId, amount);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetCurrentEnvironments(EntityId const entityToIgnore, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::SetCurrentEnvironments> requestData(entityToIgnore);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::ResetEnvironments(SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::ResetEnvironments> requestData;
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetOcclusionType(EOcclusionType const occlusionType, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	if (occlusionType < EOcclusionType::Count)
	{
		SAudioObjectRequestData<EAudioObjectRequestType::SetSwitchState> requestData(OcclusionTypeSwitchId, OcclusionTypeStateIds[IntegralValue(occlusionType)]);
		PushRequest(requestData, userData);
	}
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::PlayFile(SPlayFileInfo const& playFileInfo, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::PlayFile> requestData(playFileInfo.szFile, playFileInfo.usedTriggerForPlayback, playFileInfo.bLocalized);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::StopFile(char const* const szFile, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::StopFile> requestData(szFile);
	PushRequest(requestData, userData);
}

//////////////////////////////////////////////////////////////////////////
void CATLAudioObject::SetName(char const* const szName, SRequestUserData const& userData /* = SAudioRequestUserData::GetEmptyObject() */)
{
	SAudioObjectRequestData<EAudioObjectRequestType::SetName> requestData(szName);
	PushRequest(requestData, userData);
}
} // namespace CryAudio

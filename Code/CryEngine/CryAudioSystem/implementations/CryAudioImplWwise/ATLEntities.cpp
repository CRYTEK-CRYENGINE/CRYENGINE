// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ATLEntities.h"
#include "Common.h"
#include <Logger.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
// AK callbacks
void EndEventCallback(AkCallbackType callbackType, AkCallbackInfo* pCallbackInfo)
{
	if (callbackType == AK_EndOfEvent)
	{
		CEvent* const pAudioEvent = static_cast<CEvent* const>(pCallbackInfo->pCookie);

		if (pAudioEvent != nullptr)
		{
			gEnv->pAudioSystem->ReportFinishedEvent(pAudioEvent->m_atlEvent, true);
		}
	}
}

void PrepareEventCallback(
  AkUniqueID eventId,
  void const* pBankPtr,
  AKRESULT wwiseResult,
  AkMemPoolId memPoolId,
  void* pCookie)
{
	CEvent* const pEvent = static_cast<CEvent* const>(pCookie);

	if (pEvent != nullptr)
	{
		pEvent->m_id = eventId;
		gEnv->pAudioSystem->ReportFinishedEvent(pEvent->m_atlEvent, wwiseResult == AK_Success);
	}
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::Update()
{
	ERequestStatus result = ERequestStatus::Failure;

	if (m_bNeedsToUpdateEnvironments)
	{
		result = PostEnvironmentAmounts();
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::Set3DAttributes(SObject3DAttributes const& attributes)
{
	AkSoundPosition soundPos;
	FillAKObjectPosition(attributes.transformation, soundPos);

	AKRESULT const wwiseResult = AK::SoundEngine::SetPosition(m_id, soundPos);

	if (!IS_WWISE_OK(wwiseResult))
	{
		Cry::Audio::Log(ELogType::Warning, "Wwise - SetPosition failed with AKRESULT: %d", wwiseResult);
		return ERequestStatus::Failure;
	}

	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetEnvironment(IEnvironment const* const pIEnvironment, float const amount)
{
	static float const envEpsilon = 0.0001f;
	ERequestStatus result = ERequestStatus::Failure;
	SEnvironment const* const pEnvironment = static_cast<SEnvironment const* const>(pIEnvironment);

	if (pEnvironment != nullptr)
	{
		switch (pEnvironment->type)
		{
		case EEnvironmentType::AuxBus:
			{
				float const currentAmount = stl::find_in_map(m_environmentImplAmounts, pEnvironment->busId, -1.0f);

				if ((currentAmount == -1.0f) || (fabs(currentAmount - amount) > envEpsilon))
				{
					m_environmentImplAmounts[pEnvironment->busId] = amount;
					m_bNeedsToUpdateEnvironments = true;
				}

				result = ERequestStatus::Success;
				break;
			}
		case EEnvironmentType::Rtpc:
			{
				AkRtpcValue const rtpcValue = static_cast<AkRtpcValue>(pEnvironment->multiplier * amount + pEnvironment->shift);
				AKRESULT const wwiseResult = AK::SoundEngine::SetRTPCValue(pEnvironment->rtpcId, rtpcValue, m_id);

				if (IS_WWISE_OK(wwiseResult))
				{
					result = ERequestStatus::Success;
				}
				else
				{
					Cry::Audio::Log(
					  ELogType::Warning,
					  "Wwise - failed to set the Rtpc %u to value %f on object %u in SetEnvironement()",
					  pEnvironment->rtpcId,
					  rtpcValue,
					  m_id);
				}

				break;
			}
		default:
			{
				CRY_ASSERT(false); //Unknown AudioEnvironmentImplementation type
			}
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Wwise - Invalid EnvironmentData passed to the Wwise implementation of SetEnvironment");
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetParameter(IParameter const* const pIParameter, float const value)
{
	ERequestStatus result = ERequestStatus::Failure;
	SParameter const* const pParameter = static_cast<SParameter const* const>(pIParameter);

	if (pParameter != nullptr)
	{
		AkRtpcValue rtpcValue = static_cast<AkRtpcValue>(pParameter->mult * value + pParameter->shift);

		AKRESULT const wwiseResult = AK::SoundEngine::SetRTPCValue(pParameter->id, rtpcValue, m_id);

		if (IS_WWISE_OK(wwiseResult))
		{
			result = ERequestStatus::Success;
		}
		else
		{
			Cry::Audio::Log(
			  ELogType::Warning,
			  "Wwise - failed to set the Rtpc %" PRISIZE_T " to value %f on object %" PRISIZE_T,
			  pParameter->id,
			  static_cast<AkRtpcValue>(value),
			  m_id);
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Wwise - Invalid RtpcData passed to the Wwise implementation of SetParameter");
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetSwitchState(ISwitchState const* const pISwitchState)
{
	ERequestStatus result = ERequestStatus::Failure;
	SSwitchState const* const pSwitchState = static_cast<SSwitchState const* const>(pISwitchState);

	if (pSwitchState != nullptr)
	{
		switch (pSwitchState->type)
		{
		case ESwitchType::StateGroup:
			{
				AKRESULT const wwiseResult = AK::SoundEngine::SetState(
				  pSwitchState->stateOrSwitchGroupId,
				  pSwitchState->stateOrSwitchId);

				if (IS_WWISE_OK(wwiseResult))
				{
					result = ERequestStatus::Success;
				}
				else
				{
					Cry::Audio::Log(
					  ELogType::Warning,
					  "Wwise failed to set the StateGroup %" PRISIZE_T "to state %" PRISIZE_T,
					  pSwitchState->stateOrSwitchGroupId,
					  pSwitchState->stateOrSwitchId);
				}

				break;
			}
		case ESwitchType::SwitchGroup:
			{
				AKRESULT const wwiseResult = AK::SoundEngine::SetSwitch(
				  pSwitchState->stateOrSwitchGroupId,
				  pSwitchState->stateOrSwitchId,
				  m_id);

				if (IS_WWISE_OK(wwiseResult))
				{
					result = ERequestStatus::Success;
				}
				else
				{
					Cry::Audio::Log(
					  ELogType::Warning,
					  "Wwise - failed to set the SwitchGroup %" PRISIZE_T " to state %" PRISIZE_T " on object %" PRISIZE_T,
					  pSwitchState->stateOrSwitchGroupId,
					  pSwitchState->stateOrSwitchId,
					  m_id);
				}

				break;
			}
		case ESwitchType::Rtpc:
			{
				AKRESULT const wwiseResult = AK::SoundEngine::SetRTPCValue(
				  pSwitchState->stateOrSwitchGroupId,
				  static_cast<AkRtpcValue>(pSwitchState->rtpcValue),
				  m_id);

				if (IS_WWISE_OK(wwiseResult))
				{
					result = ERequestStatus::Success;
				}
				else
				{
					Cry::Audio::Log(
					  ELogType::Warning,
					  "Wwise - failed to set the Rtpc %" PRISIZE_T " to value %f on object %" PRISIZE_T,
					  pSwitchState->stateOrSwitchGroupId,
					  static_cast<AkRtpcValue>(pSwitchState->rtpcValue),
					  m_id);
				}

				break;
			}
		case ESwitchType::None:
			{
				break;
			}
		default:
			{
				Cry::Audio::Log(ELogType::Warning, "Wwise - Unknown ESwitchType: %" PRISIZE_T, pSwitchState->type);

				break;
			}
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Wwise - Invalid SwitchState passed to the Wwise implementation of SetSwitchState");
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetObstructionOcclusion(float const obstruction, float const occlusion)
{
	ERequestStatus result = ERequestStatus::Failure;

	if (g_listenerId != AK_INVALID_GAME_OBJECT)
	{
		AKRESULT const wwiseResult = AK::SoundEngine::SetObjectObstructionAndOcclusion(
		  m_id,
		  g_listenerId,                     // Set obstruction/occlusion for only the default listener for now.
		  static_cast<AkReal32>(occlusion), // The occlusion value is currently used on obstruction as well until a correct obstruction value is calculated.
		  static_cast<AkReal32>(occlusion));

		if (IS_WWISE_OK(wwiseResult))
		{
			result = ERequestStatus::Success;
		}
		else
		{
			Cry::Audio::Log(
			  ELogType::Warning,
			  "Wwise - failed to set Obstruction %f and Occlusion %f on object %" PRISIZE_T,
			  obstruction,
			  occlusion,
			  m_id);
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Wwise - invalid listener Id during SetObjectObstructionAndOcclusion!");
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::ExecuteTrigger(ITrigger const* const pITrigger, IEvent* const pIEvent)
{
	ERequestStatus result = ERequestStatus::Failure;

	CTrigger const* const pTrigger = static_cast<CTrigger const* const>(pITrigger);
	CEvent* const pEvent = static_cast<CEvent* const>(pIEvent);

	if ((pTrigger != nullptr) && (pEvent != nullptr))
	{
		// If the user executes a trigger on the global object we want to post events only to that particular object and not globally!
		AkGameObjectID objectId = g_globalObjectId;

		if (m_id != AK_INVALID_GAME_OBJECT)
		{
			objectId = m_id;
			PostEnvironmentAmounts();
		}

		AkPlayingID const id = AK::SoundEngine::PostEvent(pTrigger->m_id, objectId, AK_EndOfEvent, &EndEventCallback, pEvent);

		if (id != AK_INVALID_PLAYING_ID)
		{
			pEvent->m_state = EEventState::Playing;
			pEvent->m_id = id;
			result = ERequestStatus::Success;
		}
		else
		{
			// if posting an Event failed, try to prepare it, if it isn't prepared already
			Cry::Audio::Log(ELogType::Warning, "Failed to Post Wwise event %" PRISIZE_T, pEvent->m_id);
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Invalid AudioObjectData, ATLTriggerData or EventData passed to the Wwise implementation of ExecuteTrigger.");
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::StopAllTriggers()
{
	// If the user wants to stop all triggers on the global object we want to stop them only on that particular object and not globally!
	AkGameObjectID const objectId = (m_id != AK_INVALID_GAME_OBJECT) ? m_id : g_globalObjectId;
	AK::SoundEngine::StopAll(objectId);
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetName(char const* const szName)
{
#if defined(INCLUDE_WWISE_IMPL_PRODUCTION_CODE)
	StopAllTriggers();

	AKRESULT wwiseResult = AK::SoundEngine::UnregisterGameObj(m_id);
	CRY_ASSERT(wwiseResult == AK_Success);

	wwiseResult = AK::SoundEngine::RegisterGameObj(m_id, szName);
	CRY_ASSERT(wwiseResult == AK_Success);

	return ERequestStatus::SuccessNeedsRefresh;
#else
	return ERequestStatus::Success;
#endif  // INCLUDE_WWISE_IMPL_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::PostEnvironmentAmounts()
{
	ERequestStatus result = ERequestStatus::Failure;
	std::size_t const numEnvironments = m_environmentImplAmounts.size();

	if (numEnvironments > 0)
	{
		AkAuxSendValue auxValues[AK_MAX_AUX_PER_OBJ];
		uint32 auxIndex = 0;

		CObject::EnvironmentImplMap::iterator iEnvPair = m_environmentImplAmounts.begin();
		CObject::EnvironmentImplMap::const_iterator const iEnvStart = m_environmentImplAmounts.begin();
		CObject::EnvironmentImplMap::const_iterator const iEnvEnd = m_environmentImplAmounts.end();

		if (numEnvironments <= AK_MAX_AUX_PER_OBJ)
		{
			for (; iEnvPair != iEnvEnd; ++auxIndex)
			{
				float const amount = iEnvPair->second;
				AkAuxSendValue& auxValue = auxValues[auxIndex];
				auxValue.auxBusID = iEnvPair->first;
				auxValue.fControlValue = amount;
				auxValue.listenerID = g_listenerId;

				// If an amount is zero, we still want to send it to the middleware, but we also want to remove it from the map.
				if (amount == 0.0f)
				{
					m_environmentImplAmounts.erase(iEnvPair++);
				}
				else
				{
					++iEnvPair;
				}
			}
		}
		else
		{
			// sort the environments in order of decreasing amounts and take the first AK_MAX_AUX_PER_OBJ worth
			using TEnvPairSet = std::set<std::pair<AkAuxBusID, float>, SEnvPairCompare>;
			TEnvPairSet cEnvPairs(iEnvStart, iEnvEnd);

			TEnvPairSet::const_iterator iSortedEnvPair = cEnvPairs.begin();
			TEnvPairSet::const_iterator const iSortedEnvEnd = cEnvPairs.end();

			for (; (iSortedEnvPair != iSortedEnvEnd) && (auxIndex < AK_MAX_AUX_PER_OBJ); ++iSortedEnvPair, ++auxIndex)
			{
				AkAuxSendValue& auxValue = auxValues[auxIndex];
				auxValue.auxBusID = iSortedEnvPair->first;
				auxValue.fControlValue = iSortedEnvPair->second;
				auxValue.listenerID = g_listenerId;
			}

			//remove all Environments with 0.0 amounts
			while (iEnvPair != iEnvEnd)
			{
				if (iEnvPair->second == 0.0f)
				{
					m_environmentImplAmounts.erase(iEnvPair++);
				}
				else
				{
					++iEnvPair;
				}
			}
		}

		CRY_ASSERT(auxIndex <= AK_MAX_AUX_PER_OBJ);

		AKRESULT const wwiseResult = AK::SoundEngine::SetGameObjectAuxSendValues(m_id, auxValues, auxIndex);

		if (IS_WWISE_OK(wwiseResult))
		{
			result = ERequestStatus::Success;
		}
		else
		{
			Cry::Audio::Log(ELogType::Warning, "Wwise - SetGameObjectAuxSendValues failed on object %" PRISIZE_T " with AKRESULT: %d", m_id, wwiseResult);
		}
	}

	m_bNeedsToUpdateEnvironments = false;
	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CEvent::Stop()
{
	switch (m_state)
	{
	case EEventState::Playing:
		{
			AK::SoundEngine::StopPlayingID(m_id, 10);
			break;
		}
	default:
		{
			// Stopping an event of this type is not supported!
			CRY_ASSERT(false);
			break;
		}
	}

	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CListener::Set3DAttributes(SObject3DAttributes const& attributes)
{
	ERequestStatus result = ERequestStatus::Success;
	AkListenerPosition listenerPos;
	FillAKListenerPosition(attributes.transformation, listenerPos);

	AKRESULT const wwiseResult = AK::SoundEngine::SetPosition(m_id, listenerPos);

	if (!IS_WWISE_OK(wwiseResult))
	{
		Cry::Audio::Log(ELogType::Warning, "Wwise - CListener::Set3DAttributes failed with AKRESULT: %d", wwiseResult);
		result = ERequestStatus::Failure;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CTrigger::Load() const
{
	return SetLoaded(true);
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CTrigger::Unload() const
{
	return SetLoaded(false);
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CTrigger::LoadAsync(IEvent* const pIEvent) const
{
	return SetLoadedAsync(pIEvent, true);
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CTrigger::UnloadAsync(IEvent* const pIEvent) const
{
	return SetLoadedAsync(pIEvent, false);
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CTrigger::SetLoaded(bool const bLoad) const
{
	ERequestStatus result = ERequestStatus::Failure;
	AkUniqueID id = m_id;
	AKRESULT const wwiseResult = AK::SoundEngine::PrepareEvent(bLoad ? AK::SoundEngine::Preparation_Load : AK::SoundEngine::Preparation_Unload, &id, 1);

	if (IS_WWISE_OK(wwiseResult))
	{
		result = ERequestStatus::Success;
	}
	else
	{
		Cry::Audio::Log(
		  ELogType::Warning,
		  "Wwise - PrepareEvent with %s failed for Wwise event %" PRISIZE_T " with AKRESULT: %d",
		  bLoad ? "Preparation_Load" : "Preparation_Unload",
		  m_id,
		  wwiseResult);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CTrigger::SetLoadedAsync(IEvent* const pIEvent, bool const bLoad) const
{
	ERequestStatus result = ERequestStatus::Failure;

	CEvent* const pEvent = static_cast<CEvent*>(pIEvent);

	if (pEvent != nullptr)
	{
		AkUniqueID id = m_id;
		AKRESULT const wwiseResult = AK::SoundEngine::PrepareEvent(
		  bLoad ? AK::SoundEngine::Preparation_Load : AK::SoundEngine::Preparation_Unload,
		  &id,
		  1,
		  &PrepareEventCallback,
		  pEvent);

		if (IS_WWISE_OK(wwiseResult))
		{
			pEvent->m_id = m_id;
			pEvent->m_state = EEventState::Unloading;
			result = ERequestStatus::Success;
		}
		else
		{
			Cry::Audio::Log(
			  ELogType::Warning,
			  "Wwise - PrepareEvent with %s failed for Wwise event %" PRISIZE_T " with AKRESULT: %d",
			  bLoad ? "Preparation_Load" : "Preparation_Unload",
			  m_id,
			  wwiseResult);
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Error,
		                "Wwise - Invalid IEvent passed to the Wwise implementation of %sTriggerAsync",
		                bLoad ? "Load" : "Unprepare");
	}

	return result;
}
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio

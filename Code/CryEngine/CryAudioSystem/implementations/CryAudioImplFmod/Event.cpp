// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Event.h"
#include "Common.h"
#include "Impl.h"
#include "Object.h"
#include "EventInstance.h"

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
FMOD_RESULT F_CALLBACK EventCallback(FMOD_STUDIO_EVENT_CALLBACK_TYPE type, FMOD_STUDIO_EVENTINSTANCE* event, void* parameters)
{
	auto* const pFmodEventInstance = reinterpret_cast<FMOD::Studio::EventInstance*>(event);

	if (pFmodEventInstance != nullptr)
	{
		CEventInstance* pEventInstance = nullptr;
		FMOD_RESULT const fmodResult = pFmodEventInstance->getUserData(reinterpret_cast<void**>(&pEventInstance));
		CRY_AUDIO_IMPL_FMOD_ASSERT_OK;

		if (pEventInstance != nullptr)
		{
			pEventInstance->SetToBeRemoved();
		}
	}

	return FMOD_OK;
}

//////////////////////////////////////////////////////////////////////////
FMOD_RESULT F_CALLBACK ProgrammerSoundCallback(FMOD_STUDIO_EVENT_CALLBACK_TYPE type, FMOD_STUDIO_EVENTINSTANCE* pEventInst, void* pInOutParameters)
{
	if (pEventInst != nullptr)
	{
		auto const pFmodEventInstance = reinterpret_cast<FMOD::Studio::EventInstance*>(pEventInst);
		CEventInstance* pEventInstance = nullptr;
		FMOD_RESULT fmodResult = pFmodEventInstance->getUserData(reinterpret_cast<void**>(&pEventInstance));
		CRY_AUDIO_IMPL_FMOD_ASSERT_OK;

		if (pEventInstance != nullptr)
		{
			if (type == FMOD_STUDIO_EVENT_CALLBACK_CREATE_PROGRAMMER_SOUND)
			{
				CRY_ASSERT_MESSAGE(pInOutParameters != nullptr, "pInOutParameters is null pointer during %s", __FUNCTION__);
				auto const pInOutProperties = reinterpret_cast<FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES*>(pInOutParameters);
				char const* const szKey = pEventInstance->GetEvent().GetKey().c_str();

				FMOD_STUDIO_SOUND_INFO soundInfo;
				fmodResult = g_pStudioSystem->getSoundInfo(szKey, &soundInfo);
				CRY_AUDIO_IMPL_FMOD_ASSERT_OK;

				FMOD::Sound* pSound = nullptr;
				FMOD_MODE const mode = FMOD_CREATECOMPRESSEDSAMPLE | FMOD_NONBLOCKING | FMOD_3D | soundInfo.mode;
				fmodResult = g_pCoreSystem->createSound(soundInfo.name_or_data, mode, &soundInfo.exinfo, &pSound);
				CRY_AUDIO_IMPL_FMOD_ASSERT_OK;

				pInOutProperties->sound = reinterpret_cast<FMOD_SOUND*>(pSound);
				pInOutProperties->subsoundIndex = soundInfo.subsoundindex;
			}
			else if (type == FMOD_STUDIO_EVENT_CALLBACK_DESTROY_PROGRAMMER_SOUND)
			{
				CRY_ASSERT_MESSAGE(pInOutParameters != nullptr, "pInOutParameters is null pointer during %s", __FUNCTION__);
				auto const pInOutProperties = reinterpret_cast<FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES*>(pInOutParameters);

				auto* pSound = reinterpret_cast<FMOD::Sound*>(pInOutProperties->sound);

				fmodResult = pSound->release();
				CRY_AUDIO_IMPL_FMOD_ASSERT_OK;
			}
			else if ((type == FMOD_STUDIO_EVENT_CALLBACK_START_FAILED) || (type == FMOD_STUDIO_EVENT_CALLBACK_STOPPED))
			{
				CRY_AUDIO_IMPL_FMOD_ASSERT_OK;
				pEventInstance->SetToBeRemoved();
			}
		}
	}

	return FMOD_OK;
}

//////////////////////////////////////////////////////////////////////////
CEvent::~CEvent()
{
	g_eventToParameterIndexes.erase(this);
}

//////////////////////////////////////////////////////////////////////////
ETriggerResult CEvent::Execute(IObject* const pIObject, TriggerInstanceId const triggerInstanceId)
{
	ETriggerResult result = ETriggerResult::Failure;

	auto const pObject = static_cast<CObject*>(pIObject);

	switch (m_actionType)
	{
	case EActionType::Start:
		{
			FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
			CEventInstance* const pEventInstance = g_pImpl->ConstructEventInstance(triggerInstanceId, *this, *pObject);
#else
			CEventInstance* const pEventInstance = g_pImpl->ConstructEventInstance(triggerInstanceId, *this);
#endif        // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

			if (m_pEventDescription == nullptr)
			{
				fmodResult = g_pStudioSystem->getEventByID(&m_guid, &m_pEventDescription);
				CRY_AUDIO_IMPL_FMOD_ASSERT_OK;
			}

			if (m_pEventDescription != nullptr)
			{
				CRY_ASSERT(pEventInstance->GetFmodEventInstance() == nullptr);

				FMOD::Studio::EventInstance* pFmodEventInstance = nullptr;
				fmodResult = m_pEventDescription->createInstance(&pFmodEventInstance);
				CRY_AUDIO_IMPL_FMOD_ASSERT_OK;
				pFmodEventInstance->setListenerMask(pObject->GetListenerMask());
				pEventInstance->SetFmodEventInstance(pFmodEventInstance);

				if ((m_flags& EEventFlags::CheckedParameters) == 0)
				{
					int count = 0;
					m_pEventDescription->getParameterDescriptionCount(&count);

					if (count > 0)
					{
						for (int i = 0; i < count; ++i)
						{
							FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;
							m_pEventDescription->getParameterDescriptionByIndex(i, &parameterDescription);

							if (_stricmp(parameterDescription.name, g_szAbsoluteVelocityParameterName) == 0)
							{
								g_absoluteVelocityParameterInfo.SetId(parameterDescription.id);
								m_flags |= EEventFlags::HasAbsoluteVelocityParameter;
							}
							else if (_stricmp(parameterDescription.name, g_szOcclusionParameterName) == 0)
							{
								g_occlusionParameterInfo.SetId(parameterDescription.id);
								m_flags |= EEventFlags::HasOcclusionParameter;
							}
						}
					}

					m_flags |= EEventFlags::CheckedParameters;
				}

				if ((m_flags& EEventFlags::HasProgrammerSound) != 0)
				{
					fmodResult = pEventInstance->GetFmodEventInstance()->setCallback(ProgrammerSoundCallback);
				}
				else
				{
					fmodResult = pEventInstance->GetFmodEventInstance()->setCallback(EventCallback, FMOD_STUDIO_EVENT_CALLBACK_START_FAILED | FMOD_STUDIO_EVENT_CALLBACK_STOPPED);
				}

				CRY_AUDIO_IMPL_FMOD_ASSERT_OK;
				fmodResult = pEventInstance->GetFmodEventInstance()->setUserData(pEventInstance);
				CRY_AUDIO_IMPL_FMOD_ASSERT_OK;
				fmodResult = pEventInstance->GetFmodEventInstance()->set3DAttributes(&pObject->GetAttributes());
				CRY_AUDIO_IMPL_FMOD_ASSERT_OK;

				pObject->AddEventInstance(pEventInstance);

				if (pObject->SetEventInstance(pEventInstance))
				{
					result = (pEventInstance->GetState() == EEventState::Playing) ? ETriggerResult::Playing : ETriggerResult::Virtual;
				}
				else
				{
					result = ETriggerResult::Pending;
				}
			}

			break;
		}
	case EActionType::Stop:
		{
			pObject->StopEventInstance(m_id);
			result = ETriggerResult::DoNotTrack;

			break;
		}
	case EActionType::Pause: // Intentional fall-through.
	case EActionType::Resume:
		{
			FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

			int const capacity = 32;
			EventInstances const& eventInstances = pObject->GetEventInstances();

			for (auto const pEventInstance : eventInstances)
			{
				if (pEventInstance->GetEvent().GetId() == m_id)
				{
					if (m_pEventDescription == nullptr)
					{
						fmodResult = g_pStudioSystem->getEventByID(&m_guid, &m_pEventDescription);
						CRY_AUDIO_IMPL_FMOD_ASSERT_OK;
					}

					if (m_pEventDescription != nullptr)
					{
						int count = 0;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
						fmodResult = m_pEventDescription->getInstanceCount(&count);
						CRY_AUDIO_IMPL_FMOD_ASSERT_OK;
						CRY_ASSERT_MESSAGE(count < capacity, "Instance count (%d) is higher or equal than array capacity (%d) during %s", count, capacity, __FUNCTION__);
#endif              // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

						FMOD::Studio::EventInstance* eventInstances[capacity];
						fmodResult = m_pEventDescription->getInstanceList(eventInstances, capacity, &count);
						CRY_AUDIO_IMPL_FMOD_ASSERT_OK;

						for (int i = 0; i < count; ++i)
						{
							FMOD::Studio::EventInstance* const pFmodEventInstance = eventInstances[i];

							if (pFmodEventInstance != nullptr)
							{
								fmodResult = pFmodEventInstance->setPaused(m_actionType == EActionType::Pause);
								CRY_AUDIO_IMPL_FMOD_ASSERT_OK;

								pEventInstance->SetPaused(m_actionType == EActionType::Pause);
							}
						}

						result = ETriggerResult::DoNotTrack;
					}
				}
			}

			break;
		}
	default:
		{
			break;
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CEvent::Stop(IObject* const pIObject)
{
	auto const pObject = static_cast<CObject*>(pIObject);
	pObject->StopEventInstance(m_id);
}

//////////////////////////////////////////////////////////////////////////
void CEvent::DecrementNumInstances()
{
	CRY_ASSERT_MESSAGE(m_numInstances > 0, "Number of event instances must be at least 1 during %s", __FUNCTION__);
	--m_numInstances;
}
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio

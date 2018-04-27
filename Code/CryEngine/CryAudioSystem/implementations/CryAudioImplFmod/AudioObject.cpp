// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioObject.h"
#include "AudioEvent.h"
#include "ATLEntities.h"
#include "SharedAudioData.h"
#include <Logger.h>
#include <fmod_common.h>
#include <CryAudio/IAudioSystem.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
extern TriggerToParameterIndexes g_triggerToParameterIndexes;

FMOD::Studio::System* CObjectBase::s_pSystem = nullptr;
FMOD::Studio::System* CListener::s_pSystem = nullptr;
FMOD::System* CStandaloneFileBase::s_pLowLevelSystem = nullptr;

//////////////////////////////////////////////////////////////////////////
FMOD_RESULT F_CALLBACK EventCallback(FMOD_STUDIO_EVENT_CALLBACK_TYPE type, FMOD_STUDIO_EVENTINSTANCE* event, void* parameters)
{
	FMOD::Studio::EventInstance* const pEventInstance = reinterpret_cast<FMOD::Studio::EventInstance*>(event);

	if (pEventInstance != nullptr)
	{
		CEvent* pEvent = nullptr;
		FMOD_RESULT const fmodResult = pEventInstance->getUserData(reinterpret_cast<void**>(&pEvent));
		ASSERT_FMOD_OK;

		if (pEvent != nullptr)
		{
			gEnv->pAudioSystem->ReportFinishedEvent(pEvent->GetATLEvent(), true);
		}
	}

	return FMOD_OK;
}

//////////////////////////////////////////////////////////////////////////
CObjectBase::CObjectBase()
{
	ZeroStruct(m_attributes);
	m_attributes.forward.z = 1.0f;
	m_attributes.up.y = 1.0f;

	// Reserve enough room for events to minimize/prevent runtime allocations.
	m_events.reserve(2);
}

//////////////////////////////////////////////////////////////////////////
CObjectBase::~CObjectBase()
{
	// If the audio object is deleted before its events get
	// cleared we need to remove all references to it
	for (auto const pEvent : m_events)
	{
		pEvent->SetObject(nullptr);
	}

	for (auto const pEvent : m_pendingEvents)
	{
		pEvent->SetObject(nullptr);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectBase::RemoveEvent(CEvent* const pEvent)
{
	if (!stl::find_and_erase(m_events, pEvent))
	{
		if (!stl::find_and_erase(m_pendingEvents, pEvent))
		{
			Cry::Audio::Log(ELogType::Error, "Tried to remove an event from an audio object that does not own that event");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectBase::RemoveFile(CStandaloneFileBase const* const pFile)
{
	if (!stl::find_and_erase(m_files, pFile))
	{
		if (!stl::find_and_erase(m_pendingFiles, pFile))
		{
			Cry::Audio::Log(ELogType::Error, "Tried to remove an audio file from an audio object that is not playing that file");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CObjectBase::SetEvent(CEvent* const pEvent)
{
	bool bSuccess = false;

	// Update the event with all parameter and switch values
	// that are currently set on the audio object before starting it.
	if (pEvent->PrepareForOcclusion())
	{
		m_events.push_back(pEvent);
		FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

		FMOD::Studio::EventInstance* const pEventInstance = pEvent->GetInstance();
		CRY_ASSERT_MESSAGE(pEventInstance != nullptr, "Event instance doesn't exist.");
		CTrigger const* const pTrigger = pEvent->GetTrigger();
		CRY_ASSERT_MESSAGE(pTrigger != nullptr, "Trigger doesn't exist.");

		FMOD::Studio::EventDescription* pEventDescription = nullptr;
		fmodResult = pEventInstance->getDescription(&pEventDescription);
		ASSERT_FMOD_OK;

		if (g_triggerToParameterIndexes.find(pTrigger) != g_triggerToParameterIndexes.end())
		{
			ParameterIdToIndex& parameters = g_triggerToParameterIndexes[pTrigger];

			for (auto const& parameterPair : m_parameters)
			{
				uint32 const parameterId = parameterPair.first->GetId();

				if (parameters.find(parameterId) != parameters.end())
				{
					fmodResult = pEventInstance->setParameterValueByIndex(parameters[parameterId], parameterPair.second);
					ASSERT_FMOD_OK;
				}
				else
				{
					int parameterCount = 0;
					fmodResult = pEventInstance->getParameterCount(&parameterCount);
					ASSERT_FMOD_OK;

					for (int index = 0; index < parameterCount; ++index)
					{
						FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;
						fmodResult = pEventDescription->getParameterByIndex(index, &parameterDescription);
						ASSERT_FMOD_OK;

						if (parameterId == StringToId(parameterDescription.name))
						{
							parameters.emplace(parameterId, index);
							fmodResult = pEventInstance->setParameterValueByIndex(index, parameterPair.second);
							ASSERT_FMOD_OK;
							break;
						}
					}
				}
			}

			for (auto const& switchPair : m_switches)
			{
				CSwitchState const* const pSwitchState = switchPair.second;
				uint32 const parameterId = pSwitchState->GetId();

				if (parameters.find(parameterId) != parameters.end())
				{
					fmodResult = pEventInstance->setParameterValueByIndex(parameters[parameterId], pSwitchState->GetValue());
					ASSERT_FMOD_OK;
				}
				else
				{
					int parameterCount = 0;
					fmodResult = pEventInstance->getParameterCount(&parameterCount);
					ASSERT_FMOD_OK;

					for (int index = 0; index < parameterCount; ++index)
					{
						FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;
						fmodResult = pEventDescription->getParameterByIndex(index, &parameterDescription);
						ASSERT_FMOD_OK;

						if (parameterId == StringToId(parameterDescription.name))
						{
							parameters.emplace(parameterId, index);
							fmodResult = pEventInstance->setParameterValueByIndex(index, pSwitchState->GetValue());
							ASSERT_FMOD_OK;
							break;
						}
					}
				}
			}
		}

		for (auto const& environmentPair : m_environments)
		{
			pEvent->TrySetEnvironment(environmentPair.first, environmentPair.second);
		}

		pEvent->SetObstructionOcclusion(m_obstruction, m_occlusion);

		fmodResult = pEvent->GetInstance()->start();
		ASSERT_FMOD_OK;

		bSuccess = true;
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
void CObjectBase::RemoveParameter(CParameter const* const pParameter)
{
	m_parameters.erase(pParameter);
}

//////////////////////////////////////////////////////////////////////////
void CObjectBase::RemoveSwitch(CSwitchState const* const pSwitch)
{
	m_switches.erase(pSwitch->GetId());
}

//////////////////////////////////////////////////////////////////////////
void CObjectBase::RemoveEnvironment(CEnvironment const* const pEnvironment)
{
	m_environments.erase(pEnvironment);
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObjectBase::Update()
{
	if (!m_pendingEvents.empty())
	{
		auto iter(m_pendingEvents.begin());
		auto iterEnd(m_pendingEvents.cend());

		while (iter != iterEnd)
		{
			if (SetEvent(*iter))
			{
				iter = m_pendingEvents.erase(iter);
				iterEnd = m_pendingEvents.cend();
				continue;
			}

			++iter;
		}
	}

	if (!m_pendingFiles.empty())
	{
		auto iter(m_pendingFiles.begin());
		auto iterEnd(m_pendingFiles.cend());

		while (iter != iterEnd)
		{
			CStandaloneFileBase* pStandaloneFile = *iter;

			if (pStandaloneFile->IsReady())
			{
				m_files.push_back(pStandaloneFile);

				pStandaloneFile->Play(m_attributes);

				iter = m_pendingFiles.erase(iter);
				iterEnd = m_pendingFiles.cend();
				continue;
			}
			++iter;
		}
	}

	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObjectBase::Set3DAttributes(SObject3DAttributes const& attributes)
{
	FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;
	FillFmodObjectPosition(attributes, m_attributes);

	for (auto const pEvent : m_events)
	{
		fmodResult = pEvent->GetInstance()->set3DAttributes(&m_attributes);
		ASSERT_FMOD_OK;
	}

	for (auto const pFile : m_files)
	{
		pFile->Set3DAttributes(m_attributes);
	}

	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObjectBase::ExecuteTrigger(ITrigger const* const pITrigger, IEvent* const pIEvent)
{
	ERequestStatus requestResult = ERequestStatus::Failure;
	CTrigger const* const pTrigger = static_cast<CTrigger const* const>(pITrigger);
	CEvent* const pEvent = static_cast<CEvent*>(pIEvent);

	if ((pTrigger != nullptr) && (pEvent != nullptr))
	{
		pEvent->SetTrigger(pTrigger);
		auto const type = pTrigger->GetEventType();

		switch (type)
		{
		case EEventType::Start:
			{
				FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;
				FMOD::Studio::EventDescription* pEventDescription = pTrigger->GetEventDescription();

				if (pEventDescription == nullptr)
				{
					FMOD_GUID const guid = pTrigger->GetGuid();
					fmodResult = s_pSystem->getEventByID(&guid, &pEventDescription);
					ASSERT_FMOD_OK;
				}

				if (pEventDescription != nullptr)
				{
					CRY_ASSERT(pEvent->GetInstance() == nullptr);

					FMOD::Studio::EventInstance* pInstance = nullptr;
					fmodResult = pEventDescription->createInstance(&pInstance);
					ASSERT_FMOD_OK;
					pEvent->SetInstance(pInstance);
					fmodResult = pEvent->GetInstance()->setCallback(EventCallback, FMOD_STUDIO_EVENT_CALLBACK_START_FAILED | FMOD_STUDIO_EVENT_CALLBACK_STOPPED);
					ASSERT_FMOD_OK;
					fmodResult = pEvent->GetInstance()->setUserData(pEvent);
					ASSERT_FMOD_OK;
					fmodResult = pEvent->GetInstance()->set3DAttributes(&m_attributes);
					ASSERT_FMOD_OK;

					CRY_ASSERT(pEvent->GetId() == InvalidCRC32);
					pEvent->SetId(pTrigger->GetId());
					pEvent->SetObject(this);

					CRY_ASSERT_MESSAGE(std::find(m_pendingEvents.begin(), m_pendingEvents.end(), pEvent) == m_pendingEvents.end(), "Event was already in the pending list");
					m_pendingEvents.push_back(pEvent);
					requestResult = ERequestStatus::Success;
				}
			}
			break;
		case EEventType::Stop:
			{
				StopEvent(pTrigger->GetId());
				requestResult = ERequestStatus::SuccessDoNotTrack;
			}
			break;
		case EEventType::Pause:
		case EEventType::Resume:
			{
				FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;
				FMOD::Studio::EventDescription* pEventDescription = pTrigger->GetEventDescription();

				if (pEventDescription == nullptr)
				{
					FMOD_GUID const guid = pTrigger->GetGuid();
					fmodResult = s_pSystem->getEventByID(&guid, &pEventDescription);
					ASSERT_FMOD_OK;
				}

				if (pEventDescription != nullptr)
				{
					CRY_ASSERT(pEvent->GetInstance() == nullptr);

					bool const shouldPause = (type == EEventType::Pause);
					int const capacity = 32;
					int count = 0;

#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
					fmodResult = pEventDescription->getInstanceCount(&count);
					ASSERT_FMOD_OK;
					CRY_ASSERT_MESSAGE(count < capacity, "Instance count (%d) is higher or equal than array capacity (%d).", count, capacity);
#endif          // INCLUDE_FMOD_IMPL_PRODUCTION_CODE

					FMOD::Studio::EventInstance* eventInstances[capacity];
					fmodResult = pEventDescription->getInstanceList(eventInstances, capacity, &count);
					ASSERT_FMOD_OK;

					for (int i = 0; i < count; ++i)
					{
						auto const pInstance = eventInstances[i];

						if (pInstance != nullptr)
						{
							fmodResult = pInstance->setPaused(shouldPause);
							ASSERT_FMOD_OK;
						}
					}

					requestResult = ERequestStatus::SuccessDoNotTrack;
				}
			}
			break;
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Invalid trigger or event pointers passed to the Fmod implementation of ExecuteTrigger.");
	}

	return requestResult;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObjectBase::StopAllTriggers()
{
	FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

	for (auto const pEvent : m_events)
	{
		fmodResult = pEvent->GetInstance()->stop(FMOD_STUDIO_STOP_IMMEDIATE);
		ASSERT_FMOD_OK;
	}

	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObjectBase::PlayFile(IStandaloneFile* const pIStandaloneFile)
{
	CStandaloneFileBase* const pStandaloneFile = static_cast<CStandaloneFileBase* const>(pIStandaloneFile);

	if (pStandaloneFile != nullptr)
	{
		pStandaloneFile->m_pObject = this;
		pStandaloneFile->StartLoading();

		CRY_ASSERT_MESSAGE(std::find(m_pendingFiles.begin(), m_pendingFiles.end(), pStandaloneFile) == m_pendingFiles.end(), "standalone file was already in the pending standalone files list");
		m_pendingFiles.push_back(pStandaloneFile);

		return ERequestStatus::Success;
	}

	Cry::Audio::Log(ELogType::Error, "Invalid standalone file pointer passed to the Fmod implementation of PlayFile.");
	return ERequestStatus::Failure;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObjectBase::StopFile(IStandaloneFile* const pIStandaloneFile)
{
	CStandaloneFileBase* const pStandaloneFile = static_cast<CStandaloneFileBase* const>(pIStandaloneFile);

	if (pStandaloneFile != nullptr)
	{
		pStandaloneFile->Stop();
		return ERequestStatus::Pending;
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Invalid standalone file pointer passed to the Fmod implementation of StopFile.");
	}

	return ERequestStatus::Failure;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObjectBase::SetName(char const* const szName)
{
	// Fmod does not have the concept of audio objects and with that the debugging of such.
	// Therefore the name is currently not needed here.
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
void CObjectBase::StopEvent(uint32 const id)
{
	FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

	for (auto const pEvent : m_events)
	{
		if (pEvent->GetId() == id)
		{
			fmodResult = pEvent->GetInstance()->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
			ASSERT_FMOD_OK;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CGlobalObject::SetEnvironment(IEnvironment const* const pIEnvironment, float const amount)
{
	ERequestStatus result = ERequestStatus::Failure;
	CEnvironment const* const pEnvironment = static_cast<CEnvironment const* const>(pIEnvironment);

	if (pEnvironment != nullptr)
	{
		for (auto const pObject : m_objects)
		{
			if (pObject != this)
			{
				pObject->SetEnvironment(pEnvironment, amount);
			}
		}

		result = ERequestStatus::Success;
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Invalid Environment pointer passed to the Fmod implementation of SetEnvironment");
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CGlobalObject::SetParameter(IParameter const* const pIParameter, float const value)
{
	ERequestStatus result = ERequestStatus::Failure;
	CParameter const* const pParameter = static_cast<CParameter const* const>(pIParameter);

	if (pParameter != nullptr)
	{
		for (auto const pObject : m_objects)
		{
			if (pObject != this)
			{
				pObject->SetParameter(pParameter, value);
			}
		}
		result = ERequestStatus::Success;
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Invalid parameter pointer passed to the Fmod implementation of SetParameter");
	}

	return result;
}
//////////////////////////////////////////////////////////////////////////
ERequestStatus CGlobalObject::SetSwitchState(ISwitchState const* const pISwitchState)
{
	ERequestStatus result = ERequestStatus::Failure;
	CSwitchState const* const pSwitchState = static_cast<CSwitchState const* const>(pISwitchState);

	if (pSwitchState != nullptr)
	{
		for (auto const pObject : m_objects)
		{
			if (pObject != this)
			{
				pObject->SetSwitchState(pISwitchState);
			}
		}

		result = ERequestStatus::Success;
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Invalid switch pointer passed to the Fmod implementation of SetSwitchState");
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CGlobalObject::SetObstructionOcclusion(float const obstruction, float const occlusion)
{
	Cry::Audio::Log(ELogType::Error, "Trying to set occlusion and obstruction values on the global audio object!");
	return ERequestStatus::Failure;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetEnvironment(IEnvironment const* const pIEnvironment, float const amount)
{
	ERequestStatus result = ERequestStatus::Success;
	CEnvironment const* const pEnvironment = static_cast<CEnvironment const* const>(pIEnvironment);

	if (pEnvironment != nullptr)
	{
		bool bSet = true;
		auto const iter(m_environments.find(pEnvironment));

		if (iter != m_environments.end())
		{
			if (bSet = (fabs(iter->second - amount) > 0.001f))
			{
				iter->second = amount;
			}
		}
		else
		{
			m_environments.emplace(pEnvironment, amount);
		}

		if (bSet)
		{
			for (auto const pEvent : m_events)
			{
				pEvent->TrySetEnvironment(pEnvironment, amount);
			}
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Invalid IEnvironment pointer passed to the Fmod implementation of SetEnvironment");
		result = ERequestStatus::Failure;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetParameter(IParameter const* const pIParameter, float const value)
{
	ERequestStatus result = ERequestStatus::Success;
	CParameter const* const pParameter = static_cast<CParameter const* const>(pIParameter);

	if (pParameter != nullptr)
	{
		EParameterType const type = pParameter->GetType();

		if (type == EParameterType::Parameter)
		{
			uint32 const parameterId = pParameter->GetId();
			FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

			for (auto const pEvent : m_events)
			{
				FMOD::Studio::EventInstance* const pEventInstance = pEvent->GetInstance();
				CRY_ASSERT_MESSAGE(pEventInstance != nullptr, "Event instance doesn't exist.");
				CTrigger const* const pTrigger = pEvent->GetTrigger();
				CRY_ASSERT_MESSAGE(pTrigger != nullptr, "Trigger doesn't exist.");

				FMOD::Studio::EventDescription* pEventDescription = nullptr;
				fmodResult = pEventInstance->getDescription(&pEventDescription);
				ASSERT_FMOD_OK;

				if (g_triggerToParameterIndexes.find(pTrigger) != g_triggerToParameterIndexes.end())
				{
					ParameterIdToIndex& parameters = g_triggerToParameterIndexes[pTrigger];

					if (parameters.find(parameterId) != parameters.end())
					{
						fmodResult = pEventInstance->setParameterValueByIndex(parameters[parameterId], pParameter->GetValueMultiplier() * value + pParameter->GetValueShift());
						ASSERT_FMOD_OK;
					}
					else
					{
						int parameterCount = 0;
						fmodResult = pEventInstance->getParameterCount(&parameterCount);
						ASSERT_FMOD_OK;

						for (int index = 0; index < parameterCount; ++index)
						{
							FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;
							fmodResult = pEventDescription->getParameterByIndex(index, &parameterDescription);
							ASSERT_FMOD_OK;

							if (parameterId == StringToId(parameterDescription.name))
							{
								parameters.emplace(parameterId, index);
								fmodResult = pEventInstance->setParameterValueByIndex(index, pParameter->GetValueMultiplier() * value + pParameter->GetValueShift());
								ASSERT_FMOD_OK;
								break;
							}
						}
					}
				}
				else
				{
					int parameterCount = 0;
					fmodResult = pEventInstance->getParameterCount(&parameterCount);
					ASSERT_FMOD_OK;

					for (int index = 0; index < parameterCount; ++index)
					{
						FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;
						fmodResult = pEventDescription->getParameterByIndex(index, &parameterDescription);
						ASSERT_FMOD_OK;

						if (parameterId == StringToId(parameterDescription.name))
						{
							g_triggerToParameterIndexes[pTrigger].emplace(std::make_pair(parameterId, index));
							fmodResult = pEventInstance->setParameterValueByIndex(index, pParameter->GetValueMultiplier() * value + pParameter->GetValueShift());
							ASSERT_FMOD_OK;
							break;
						}
					}
				}
			}

			auto const iter(m_parameters.find(pParameter));

			if (iter != m_parameters.end())
			{
				iter->second = value;
			}
			else
			{
				m_parameters.emplace(pParameter, value);
			}
		}
		else if (type == EParameterType::VCA)
		{
			auto const pVca = static_cast<CVcaParameter const* const>(pParameter);
			FMOD_RESULT const fmodResult = pVca->GetVca()->setVolume(pVca->GetValueMultiplier() * value + pVca->GetValueShift());
			ASSERT_FMOD_OK;
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Invalid AudioObjectData or ParameterData passed to the Fmod implementation of SetParameter");
		result = ERequestStatus::Failure;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetSwitchState(ISwitchState const* const pISwitchState)
{
	ERequestStatus result = ERequestStatus::Success;
	CSwitchState const* const pSwitchState = static_cast<CSwitchState const* const>(pISwitchState);

	if (pSwitchState != nullptr)
	{
		EStateType const type = pSwitchState->GetType();

		if (type == EStateType::State)
		{
			uint32 const parameterId = pSwitchState->GetId();
			FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

			for (auto const pEvent : m_events)
			{
				FMOD::Studio::EventInstance* const pEventInstance = pEvent->GetInstance();
				CRY_ASSERT_MESSAGE(pEventInstance != nullptr, "Event instance doesn't exist.");
				CTrigger const* const pTrigger = pEvent->GetTrigger();
				CRY_ASSERT_MESSAGE(pTrigger != nullptr, "Trigger doesn't exist.");

				FMOD::Studio::EventDescription* pEventDescription = nullptr;
				fmodResult = pEventInstance->getDescription(&pEventDescription);
				ASSERT_FMOD_OK;

				if (g_triggerToParameterIndexes.find(pTrigger) != g_triggerToParameterIndexes.end())
				{
					ParameterIdToIndex& parameters = g_triggerToParameterIndexes[pTrigger];

					if (parameters.find(parameterId) != parameters.end())
					{
						fmodResult = pEventInstance->setParameterValueByIndex(parameters[parameterId], pSwitchState->GetValue());
						ASSERT_FMOD_OK;
					}
					else
					{
						int parameterCount = 0;
						fmodResult = pEventInstance->getParameterCount(&parameterCount);
						ASSERT_FMOD_OK;

						for (int index = 0; index < parameterCount; ++index)
						{
							FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;
							fmodResult = pEventDescription->getParameterByIndex(index, &parameterDescription);
							ASSERT_FMOD_OK;

							if (parameterId == StringToId(parameterDescription.name))
							{
								parameters.emplace(parameterId, index);
								fmodResult = pEventInstance->setParameterValueByIndex(index, pSwitchState->GetValue());
								ASSERT_FMOD_OK;
								break;
							}
						}
					}
				}
				else
				{
					int parameterCount = 0;
					fmodResult = pEventInstance->getParameterCount(&parameterCount);
					ASSERT_FMOD_OK;

					for (int index = 0; index < parameterCount; ++index)
					{
						FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;
						fmodResult = pEventDescription->getParameterByIndex(index, &parameterDescription);
						ASSERT_FMOD_OK;

						if (parameterId == StringToId(parameterDescription.name))
						{
							g_triggerToParameterIndexes[pTrigger].emplace(std::make_pair(parameterId, index));
							fmodResult = pEventInstance->setParameterValueByIndex(index, pSwitchState->GetValue());
							ASSERT_FMOD_OK;
							break;
						}
					}
				}
			}

			auto const iter(m_switches.find(pSwitchState->GetId()));

			if (iter != m_switches.end())
			{
				iter->second = pSwitchState;
			}
			else
			{
				m_switches.emplace(pSwitchState->GetId(), pSwitchState);
			}
		}
		else if (type == EStateType::VCA)
		{
			auto const pVca = static_cast<CVcaState const* const>(pSwitchState);
			FMOD_RESULT const fmodResult = pVca->GetVca()->setVolume(pVca->GetValue());
			ASSERT_FMOD_OK;
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Invalid switch pointer passed to the Fmod implementation of SetSwitchState");
		result = ERequestStatus::Failure;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetObstructionOcclusion(float const obstruction, float const occlusion)
{
	for (auto const pEvent : m_events)
	{
		pEvent->SetObstructionOcclusion(obstruction, occlusion);
	}

	m_obstruction = obstruction;
	m_occlusion = occlusion;

	return ERequestStatus::Success;
}
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio

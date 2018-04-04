// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "stdafx.h"
#include "AudioObject.h"
#include "AudioEvent.h"
#include "ATLEntities.h"
#include "SharedAudioData.h"
#include <AudioLogger.h>
#include <fmod_common.h>
#include <CryAudio/IAudioSystem.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
extern SwitchToIndexMap g_switchToIndex;
extern ParameterToIndexMap g_parameterToIndex;

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
			g_implLogger.Log(ELogType::Error, "Tried to remove an event from an audio object that does not own that event");
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
			g_implLogger.Log(ELogType::Error, "Tried to remove an audio file from an audio object that is not playing that file");
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

		for (auto const& parameterPair : m_parameters)
		{
			ParameterToIndexMap::iterator const iter(g_parameterToIndex.find(parameterPair.first));

			if (iter != g_parameterToIndex.end())
			{
				if (pEvent->GetEventPathId() == iter->first->GetEventPathId())
				{
					if (iter->second != FMOD_IMPL_INVALID_INDEX)
					{
						FMOD::Studio::ParameterInstance* pParameterInstance = nullptr;
						pEvent->GetInstance()->getParameterByIndex(iter->second, &pParameterInstance);

						if (pParameterInstance != nullptr)
						{
							fmodResult = pParameterInstance->setValue(parameterPair.second);
							ASSERT_FMOD_OK;
						}
					}
					else
					{
						SetParameter(iter->first, parameterPair.second);
					}
				}
			}
			else
			{
				g_implLogger.Log(ELogType::Warning, "Trying to set an unknown Fmod parameter during \"SetEvent\": %s", parameterPair.first->GetName().c_str());
			}
		}

		for (auto const& switchPair : m_switches)
		{
			SwitchToIndexMap::iterator const iter(g_switchToIndex.find(switchPair.second));

			if (iter != g_switchToIndex.end())
			{
				if (pEvent->GetEventPathId() == iter->first->eventPathId)
				{
					if (iter->second != FMOD_IMPL_INVALID_INDEX)
					{
						FMOD::Studio::ParameterInstance* pParameterInstance = nullptr;
						pEvent->GetInstance()->getParameterByIndex(iter->second, &pParameterInstance);

						if (pParameterInstance != nullptr)
						{
							fmodResult = pParameterInstance->setValue(switchPair.second->value);
							ASSERT_FMOD_OK;
						}
					}
					else
					{
						SetSwitchState(switchPair.second);
					}
				}
			}
			else
			{
				g_implLogger.Log(ELogType::Warning, "Trying to set an unknown Fmod switch during \"SetEvent\": %s", switchPair.second->name.c_str());
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
	m_switches.erase(pSwitch->eventPathId);
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
		if (pTrigger->m_eventType == EEventType::Start)
		{
			FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;
			FMOD::Studio::EventDescription* pEventDescription = pTrigger->m_pEventDescription;

			if (pEventDescription == nullptr)
			{
				fmodResult = s_pSystem->getEventByID(&pTrigger->m_guid, &pEventDescription);
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

				CRY_ASSERT(pEvent->GetEventPathId() == InvalidCRC32);
				pEvent->SetEventPathId(pTrigger->m_eventPathId);
				pEvent->SetObject(this);

				CRY_ASSERT_MESSAGE(std::find(m_pendingEvents.begin(), m_pendingEvents.end(), pEvent) == m_pendingEvents.end(), "Event was already in the pending list");
				m_pendingEvents.push_back(pEvent);
				requestResult = ERequestStatus::Success;
			}
		}
		else
		{
			StopEvent(pTrigger->m_eventPathId);

			// Return failure here so the ATL does not keep track of this event.
			requestResult = ERequestStatus::Failure;
		}
	}
	else
	{
		g_implLogger.Log(ELogType::Error, "Invalid trigger or event pointers passed to the Fmod implementation of ExecuteTrigger.");
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

	g_implLogger.Log(ELogType::Error, "Invalid standalone file pointer passed to the Fmod implementation of PlayFile.");
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
		g_implLogger.Log(ELogType::Error, "Invalid standalone file pointer passed to the Fmod implementation of StopFile.");
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
void CObjectBase::StopEvent(uint32 const eventPathId)
{
	FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

	for (auto const pEvent : m_events)
	{
		if (pEvent->GetEventPathId() == eventPathId)
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
		g_implLogger.Log(ELogType::Error, "Invalid Environment pointer passed to the Fmod implementation of SetEnvironment");
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
		g_implLogger.Log(ELogType::Error, "Invalid parameter pointer passed to the Fmod implementation of SetParameter");
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
		g_implLogger.Log(ELogType::Error, "Invalid switch pointer passed to the Fmod implementation of SetSwitchState");
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CGlobalObject::SetObstructionOcclusion(float const obstruction, float const occlusion)
{
	g_implLogger.Log(ELogType::Error, "Trying to set occlusion and obstruction values on the global audio object!");
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
		g_implLogger.Log(ELogType::Error, "Invalid IEnvironment pointer passed to the Fmod implementation of SetEnvironment");
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
		FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

		for (auto const pEvent : m_events)
		{
			if (pEvent->GetEventPathId() == pParameter->GetEventPathId())
			{
				FMOD::Studio::ParameterInstance* pParameterInstance = nullptr;
				ParameterToIndexMap::iterator const iter(g_parameterToIndex.find(pParameter));

				if (iter != g_parameterToIndex.end())
				{
					if (iter->second != FMOD_IMPL_INVALID_INDEX)
					{
						pEvent->GetInstance()->getParameterByIndex(iter->second, &pParameterInstance);

						if (pParameterInstance != nullptr)
						{
							fmodResult = pParameterInstance->setValue(pParameter->GetValueMultiplier() * value + pParameter->GetValueShift());
							ASSERT_FMOD_OK;
						}
						else
						{
							g_implLogger.Log(ELogType::Warning, "Unknown Fmod parameter index (%d) for (%s)", iter->second, pParameter->GetName().c_str());
						}
					}
					else
					{
						int parameterCount = 0;
						fmodResult = pEvent->GetInstance()->getParameterCount(&parameterCount);
						ASSERT_FMOD_OK;

						for (int index = 0; index < parameterCount; ++index)
						{
							fmodResult = pEvent->GetInstance()->getParameterByIndex(index, &pParameterInstance);
							ASSERT_FMOD_OK;

							FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;
							fmodResult = pParameterInstance->getDescription(&parameterDescription);
							ASSERT_FMOD_OK;

							if (pParameter->GetName().compareNoCase(parameterDescription.name) == 0)
							{
								iter->second = index;
								fmodResult = pParameterInstance->setValue(pParameter->GetValueMultiplier() * value + pParameter->GetValueShift());
								ASSERT_FMOD_OK;

								break;
							}
						}
					}
				}
				else
				{
					g_implLogger.Log(ELogType::Warning, "Trying to set an unknown Fmod parameter: %s", pParameter->GetName().c_str());
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
	else
	{
		g_implLogger.Log(ELogType::Error, "Invalid AudioObjectData or RtpcData passed to the Fmod implementation of SetRtpc");
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
		FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

		for (auto const pEvent : m_events)
		{
			if (pEvent->GetEventPathId() == pSwitchState->eventPathId)
			{
				FMOD::Studio::ParameterInstance* pParameterInstance = nullptr;
				SwitchToIndexMap::iterator const iter(g_switchToIndex.find(pSwitchState));

				if (iter != g_switchToIndex.end())
				{
					if (iter->second != FMOD_IMPL_INVALID_INDEX)
					{
						pEvent->GetInstance()->getParameterByIndex(iter->second, &pParameterInstance);

						if (pParameterInstance != nullptr)
						{
							fmodResult = pParameterInstance->setValue(pSwitchState->value);
							ASSERT_FMOD_OK;
						}
					}
					else
					{
						int parameterCount = 0;
						fmodResult = pEvent->GetInstance()->getParameterCount(&parameterCount);
						ASSERT_FMOD_OK;

						for (int index = 0; index < parameterCount; ++index)
						{
							fmodResult = pEvent->GetInstance()->getParameterByIndex(index, &pParameterInstance);
							ASSERT_FMOD_OK;

							FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;
							fmodResult = pParameterInstance->getDescription(&parameterDescription);
							ASSERT_FMOD_OK;

							if (pSwitchState->name.compareNoCase(parameterDescription.name) == 0)
							{
								iter->second = index;
								fmodResult = pParameterInstance->setValue(pSwitchState->value);
								ASSERT_FMOD_OK;

								break;
							}
						}
					}
				}
				else
				{
					g_implLogger.Log(ELogType::Warning, "Trying to set an unknown Fmod switch: %s", pSwitchState->name.c_str());
				}
			}
		}

		auto const iter(m_switches.find(pSwitchState->eventPathId));

		if (iter != m_switches.end())
		{
			iter->second = pSwitchState;
		}
		else
		{
			m_switches.emplace(pSwitchState->eventPathId, pSwitchState);
		}
	}
	else
	{
		g_implLogger.Log(ELogType::Error, "Invalid switch pointer passed to the Fmod implementation of SetSwitchState");
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

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioObject.h"
#include "AudioEvent.h"
#include "AudioTrigger.h"
#include <Logger.h>

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
//////////////////////////////////////////////////////////////////////////
void CObject::StopEvent(uint32 const pathId)
{
	for (auto const pEvent : m_activeEvents)
	{
		if (pEvent->pathId == pathId)
		{
			pEvent->Stop();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::RegisterEvent(CEvent* const pEvent)
{
	m_activeEvents.push_back(pEvent);
}

//////////////////////////////////////////////////////////////////////////
void CObject::UnregisterEvent(CEvent* const pEvent)
{
	m_activeEvents.erase(std::remove(m_activeEvents.begin(), m_activeEvents.end(), pEvent), m_activeEvents.end());
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::Update()
{
	for (auto const pEvent : m_activeEvents)
	{
		pEvent->Update();
	}

	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::Set3DAttributes(SObject3DAttributes const& attributes)
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetEnvironment(IEnvironment const* const pIEnvironment, float const amount)
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetParameter(IParameter const* const pIParameter, float const value)
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetSwitchState(ISwitchState const* const pISwitchState)
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetObstructionOcclusion(float const obstruction, float const occlusion)
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::ExecuteTrigger(ITrigger const* const pITrigger, IEvent* const pIEvent)
{
	ERequestStatus requestResult = ERequestStatus::Failure;
	CTrigger const* const pTrigger = static_cast<CTrigger const* const>(pITrigger);
	CEvent* const pEvent = static_cast<CEvent*>(pIEvent);

	if ((pTrigger != nullptr) && (pEvent != nullptr))
	{
		if (pTrigger->eventType == EEventType::Start)
		{
			requestResult = pEvent->Execute(
			  pTrigger->numLoops,
			  pTrigger->sampleRate,
			  pTrigger->filePath,
			  pTrigger->streamParameters) ? ERequestStatus::Success : ERequestStatus::Failure;

			if (requestResult == ERequestStatus::Success)
			{
				pEvent->pObject = this;
				pEvent->pathId = pTrigger->pathId;
				RegisterEvent(pEvent);
			}
		}
		else
		{
			StopEvent(pTrigger->pathId);

			// Return failure here so the audio system does not keep track of this event.
			requestResult = ERequestStatus::Failure;
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Invalid AudioObjectData, ATLTriggerData or EventData passed to the PortAudio implementation of ExecuteTrigger.");
	}

	return requestResult;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::StopAllTriggers()
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::PlayFile(IStandaloneFile* const pIStandaloneFile)
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::StopFile(IStandaloneFile* const pIStandaloneFile)
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetName(char const* const szName)
{
	// PortAudio does not have the concept of audio objects and with that the debugging of such.
	// Therefore the name is currently not needed here.
	return ERequestStatus::Success;
}
} // namespace PortAudio
} // namespace Impl
} // namespace CryAudio

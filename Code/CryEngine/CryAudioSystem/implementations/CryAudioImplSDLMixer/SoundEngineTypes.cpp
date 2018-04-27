// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SoundEngineTypes.h"
#include "SoundEngine.h"

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
//////////////////////////////////////////////////////////////////////////
ERequestStatus CEvent::Stop()
{
	if (SoundEngine::StopEvent(this))
	{
		return ERequestStatus::Pending;
	}
	return ERequestStatus::Failure;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::Update()
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::Set3DAttributes(SObject3DAttributes const& attributes)
{
	SoundEngine::SetObjectTransformation(this, attributes.transformation);
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
	ERequestStatus result = ERequestStatus::Failure;

	if (pIParameter != nullptr)
	{
		auto const pParameter = static_cast<CParameter const* const>(pIParameter);
		float parameterValue = pParameter->GetMultiplier() * crymath::clamp(value, 0.0f, 1.0f) + pParameter->GetShift();
		parameterValue = crymath::clamp(parameterValue, 0.0f, 1.0f);
		SampleId const sampleId = pParameter->GetSampleId();
		m_volumeMultipliers[sampleId] = parameterValue;
		result = SoundEngine::SetVolume(this, sampleId);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetSwitchState(ISwitchState const* const pISwitchState)
{
	ERequestStatus result = ERequestStatus::Failure;

	if (pISwitchState != nullptr)
	{
		auto const pSwitchState = static_cast<CSwitchState const* const>(pISwitchState);
		float const switchValue = pSwitchState->GetValue();
		SampleId const sampleId = pSwitchState->GetSampleId();
		m_volumeMultipliers[sampleId] = switchValue;
		result = SoundEngine::SetVolume(this, sampleId);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetObstructionOcclusion(float const obstruction, float const occlusion)
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::ExecuteTrigger(ITrigger const* const pITrigger, IEvent* const pIEvent)
{
	if ((pITrigger != nullptr) && (pIEvent != nullptr))
	{
		CTrigger const* const pTrigger = static_cast<CTrigger const* const>(pITrigger);
		CEvent* const pEvent = static_cast<CEvent* const>(pIEvent);
		return SoundEngine::ExecuteEvent(this, pTrigger, pEvent);
	}

	return ERequestStatus::Failure;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::StopAllTriggers()
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::PlayFile(IStandaloneFile* const pIStandaloneFile)
{
	if (SoundEngine::PlayFile(this, static_cast<CStandaloneFile*>(pIStandaloneFile)))
	{
		return ERequestStatus::Success;
	}

	return ERequestStatus::Failure;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::StopFile(IStandaloneFile* const pIStandaloneFile)
{
	if (SoundEngine::StopFile(this, static_cast<CStandaloneFile*>(pIStandaloneFile)))
	{
		return ERequestStatus::Pending;
	}

	return ERequestStatus::Failure;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetName(char const* const szName)
{
	// SDL_mixer does not have the concept of audio objects and with that the debugging of such.
	// Therefore the name is currently not needed here.
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CListener::Set3DAttributes(SObject3DAttributes const& attributes)
{
	SoundEngine::SetListenerPosition(m_id, attributes.transformation);
	return ERequestStatus::Success;
}
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio

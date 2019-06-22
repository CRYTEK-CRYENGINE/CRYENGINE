// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Object.h"
#include "Event.h"
#include "EventInstance.h"
#include "Impl.h"
#include "Listener.h"
#include "VolumeParameter.h"
#include "VolumeState.h"
#include <CryAudio/IAudioSystem.h>

#include <SDL_mixer.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
//////////////////////////////////////////////////////////////////////////
void CObject::Update(float const deltaTime)
{
	if (!m_eventInstances.empty())
	{
		float distance = 0.0f;
		float angle = 0.0f;

		if (m_pListener != nullptr)
		{
			GetDistanceAngleToObject(m_pListener->GetTransformation(), m_transformation, distance, angle);
		}

		auto iter(m_eventInstances.begin());
		auto iterEnd(m_eventInstances.end());

		while (iter != iterEnd)
		{
			CEventInstance const* const pEventInstance = *iter;

			if (pEventInstance->IsToBeRemoved())
			{
				gEnv->pAudioSystem->ReportFinishedTriggerConnectionInstance(pEventInstance->GetTriggerInstanceId(), ETriggerResult::Playing);
				g_pImpl->DestructEventInstance(pEventInstance);

				if (iter != (iterEnd - 1))
				{
					(*iter) = m_eventInstances.back();
				}

				m_eventInstances.pop_back();
				iter = m_eventInstances.begin();
				iterEnd = m_eventInstances.end();
			}
			else
			{
				for (auto const channelIndex : pEventInstance->m_channels)
				{
					SetChannelPosition(pEventInstance->GetEvent(), channelIndex, distance, angle);
				}

				++iter;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::StopAllTriggers()
{
	for (auto const pEventInstance : m_eventInstances)
	{
		pEventInstance->Stop();
	}
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetName(char const* const szName)
{
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	m_name = szName;
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
void CObject::AddListener(IListener* const pIListener)
{
	m_pListener = static_cast<CListener*>(pIListener);
}

//////////////////////////////////////////////////////////////////////////
void CObject::RemoveListener(IListener* const pIListener)
{
	if (m_pListener == static_cast<CListener*>(pIListener))
	{
		m_pListener = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::StopEvent(uint32 const id)
{
	for (auto const pEventInstance : m_eventInstances)
	{
		if (pEventInstance->GetEvent().GetId() == id)
		{
			pEventInstance->Stop();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetVolume(SampleId const sampleId, float const value)
{
	m_volumeMultipliers[sampleId] = value;

	if (!g_isMuted)
	{
		float const volumeMultiplier = GetVolumeMultiplier(this, sampleId);

		for (auto const pEventInstance : m_eventInstances)
		{
			CEvent const& event = pEventInstance->GetEvent();

			if (event.GetSampleId() == sampleId)
			{
				int const mixVolume = GetAbsoluteVolume(event.GetVolume(), volumeMultiplier);

				for (auto const channel : pEventInstance->m_channels)
				{
					Mix_Volume(channel, mixVolume);
				}
			}
		}
	}
}
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio

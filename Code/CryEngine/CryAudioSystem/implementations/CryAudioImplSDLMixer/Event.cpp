// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Event.h"
#include "Common.h"
#include "Impl.h"
#include "Object.h"
#include "EventInstance.h"
#include "Listener.h"

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	#include <Logger.h>
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

#include <SDL_mixer.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
//////////////////////////////////////////////////////////////////////////
ETriggerResult CEvent::Execute(IObject* const pIObject, TriggerInstanceId const triggerInstanceId)
{
	ETriggerResult result = ETriggerResult::Failure;
	auto const pObject = static_cast<CObject*>(pIObject);

	if (m_type == CEvent::EActionType::Start)
	{
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
		CEventInstance* const pEventInstance = g_pImpl->ConstructEventInstance(
			triggerInstanceId,
			*this,
			*pObject);
#else
		CEventInstance* const pEventInstance = g_pImpl->ConstructEventInstance(
			triggerInstanceId,
			*this);
#endif      // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

		// Start playing samples
		Mix_Chunk* pSample = stl::find_in_map(g_sampleData, m_sampleId, nullptr);

		if (pSample == nullptr)
		{
			// Trying to play sample that hasn't been loaded yet, load it in place
			// NOTE: This should be avoided as it can cause lag in audio playback
			string const& samplePath = g_samplePaths[m_sampleId];

			if (LoadSampleImpl(m_sampleId, samplePath))
			{
				pSample = stl::find_in_map(g_sampleData, m_sampleId, nullptr);
			}

			if (pSample == nullptr)
			{
				return ETriggerResult::Failure;
			}
		}

		if (!g_freeChannels.empty())
		{
			int const channelID = g_freeChannels.front();

			if (channelID >= 0)
			{
				g_freeChannels.pop();

				float const volumeMultiplier = GetVolumeMultiplier(pObject, m_sampleId);
				int const mixVolume = GetAbsoluteVolume(m_volume, volumeMultiplier);
				Mix_Volume(channelID, g_isMuted ? 0 : mixVolume);

				int channel = -1;

				if (m_fadeInTime > 0)
				{
					channel = Mix_FadeInChannel(channelID, pSample, m_numLoops, m_fadeInTime);
				}
				else
				{
					channel = Mix_PlayChannel(channelID, pSample, m_numLoops);
				}

				if (channel != -1)
				{
					// Get distance and angle from the listener to the object
					float distance = 0.0f;
					float angle = 0.0f;

					if (pObject->GetListener() != nullptr)
					{
						GetDistanceAngleToObject(pObject->GetListener()->GetTransformation(), pObject->GetTransformation(), distance, angle);
					}

					SetChannelPosition(pEventInstance->GetEvent(), channelID, distance, angle);

					g_channels[channelID].pObject = pObject;
					pEventInstance->m_channels.push_back(channelID);
				}
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
				else
				{
					Cry::Audio::Log(ELogType::Error, "Could not play sample. Error: %s", Mix_GetError());
				}
#endif          // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
			}
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
			else
			{
				Cry::Audio::Log(ELogType::Error, "Could not play sample. Error: %s", Mix_GetError());
			}
#endif        // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
		}
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Error, "Ran out of free audio channels. Are you trying to play more than %d samples?", g_numMixChannels);
		}
#endif      // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

		if (!pEventInstance->m_channels.empty())
		{
			// If any sample was added then add the event to the object
			pObject->m_eventInstances.push_back(pEventInstance);
			result = ETriggerResult::Playing;
		}
	}
	else
	{
		for (auto const pEventInstance : pObject->m_eventInstances)
		{
			if (pEventInstance->GetEvent().GetSampleId() == m_sampleId)
			{
				switch (m_type)
				{
				case CEvent::EActionType::Stop:
					{
						pEventInstance->Stop();
						break;
					}
				case CEvent::EActionType::Pause:
					{
						pEventInstance->Pause();
						break;
					}
				case CEvent::EActionType::Resume:
					{
						pEventInstance->Resume();
						break;
					}
				default:
					{
						break;
					}
				}
			}
		}

		result = ETriggerResult::DoNotTrack;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CEvent::Stop(IObject* const pIObject)
{
	auto const pObject = static_cast<CObject*>(pIObject);
	pObject->StopEvent(m_id);
}

//////////////////////////////////////////////////////////////////////////
void CEvent::DecrementNumInstances()
{
	CRY_ASSERT_MESSAGE(m_numInstances > 0, "Number of event instances must be at least 1 during %s", __FUNCTION__);
	--m_numInstances;
}
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio

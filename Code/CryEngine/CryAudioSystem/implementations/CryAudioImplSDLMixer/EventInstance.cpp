// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EventInstance.h"
#include "Event.h"

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	#include <CrySystem/ITimer.h>
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

#include <SDL_mixer.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
//////////////////////////////////////////////////////////////////////////
void CEventInstance::Stop()
{
	// need to make a copy because the callback
	// registered with Mix_ChannelFinished can edit the list
	ChannelList const channels = m_channels;
	int const fadeOutTime = m_event.GetFadeOutTime();

	for (int const channel : channels)
	{
		if (fadeOutTime == 0)
		{
			Mix_HaltChannel(channel);
		}
		else
		{
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
			if (!m_isFadingOut)
			{
				m_isFadingOut = true;
				m_timeFadeOutStarted = gEnv->pTimer->GetAsyncTime().GetSeconds();
			}
#endif      // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

			Mix_FadeOutChannel(channel, fadeOutTime);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEventInstance::Pause()
{
	// need to make a copy because the callback
	// registered with Mix_ChannelFinished can edit the list
	ChannelList const channels = m_channels;

	for (int const channel : channels)
	{
		Mix_Pause(channel);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEventInstance::Resume()
{
	// need to make a copy because the callback
	// registered with Mix_ChannelFinished can edit the list
	ChannelList channels = m_channels;

	for (int const channel : channels)
	{
		Mix_Resume(channel);
	}
}
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio

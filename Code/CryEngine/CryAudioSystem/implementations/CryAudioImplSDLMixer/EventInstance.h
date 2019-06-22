// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
class CEventInstance final : public CPoolObject<CEventInstance, stl::PSyncNone>
{
public:

	CEventInstance() = delete;
	CEventInstance(CEventInstance const&) = delete;
	CEventInstance(CEventInstance&&) = delete;
	CEventInstance& operator=(CEventInstance const&) = delete;
	CEventInstance& operator=(CEventInstance&&) = delete;

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	explicit CEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event, CObject const& object)
		: m_triggerInstanceId(triggerInstanceId)
		, m_event(event)
		, m_toBeRemoved(false)
		, m_isFadingOut(false)
		, m_timeFadeOutStarted(0.0f)
		, m_object(object)
	{}
#else
	explicit CEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event)
		: m_triggerInstanceId(triggerInstanceId)
		, m_event(event)
		, m_toBeRemoved(false)
	{}
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

	~CEventInstance() = default;

	TriggerInstanceId GetTriggerInstanceId() const { return m_triggerInstanceId; }
	CEvent&           GetEvent() const             { return m_event; }

	void              Stop();
	void              Pause();
	void              Resume();

	bool              IsToBeRemoved() const { return m_toBeRemoved; }
	void              SetToBeRemoved()      { m_toBeRemoved = true; }

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	bool           IsFadingOut() const           { return m_isFadingOut; }
	float          GetTimeFadeOutStarted() const { return m_timeFadeOutStarted; }
	CObject const& GetObject() const             { return m_object; }
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

	ChannelList m_channels;

private:

	TriggerInstanceId const m_triggerInstanceId;
	CEvent&                 m_event;
	bool                    m_toBeRemoved;

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	bool           m_isFadingOut;
	float          m_timeFadeOutStarted;
	CObject const& m_object;
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
};
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio
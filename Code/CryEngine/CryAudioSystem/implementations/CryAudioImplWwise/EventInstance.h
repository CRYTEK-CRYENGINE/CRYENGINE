// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <atomic>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
class CObject;
class CEvent;

enum class EEventInstanceState : EnumFlagsType
{
	None,
	Playing,
	Virtual,
};

class CEventInstance final : public CPoolObject<CEventInstance, stl::PSyncNone>
{
public:

	CEventInstance() = delete;
	CEventInstance(CEventInstance const&) = delete;
	CEventInstance(CEventInstance&&) = delete;
	CEventInstance& operator=(CEventInstance const&) = delete;
	CEventInstance& operator=(CEventInstance&&) = delete;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	CEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event, CObject const& object)
		: m_triggerInstanceId(triggerInstanceId)
		, m_event(event)
		, m_state(EEventInstanceState::None)
		, m_playingId(AK_INVALID_UNIQUE_ID)
		, m_toBeRemoved(false)
		, m_object(object)
	{}
#else
	CEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event)
		: m_triggerInstanceId(triggerInstanceId)
		, m_event(event)
		, m_state(EEventInstanceState::None)
		, m_playingId(AK_INVALID_UNIQUE_ID)
		, m_toBeRemoved(false)
	{}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	~CEventInstance() = default;

	TriggerInstanceId   GetTriggerInstanceId() const             { return m_triggerInstanceId; }
	CEvent&             GetEvent() const                         { return m_event; }
	EEventInstanceState GetState() const                         { return m_state; }

	AkUniqueID          GetPlayingId() const                     { return m_playingId; }
	void                SetPlayingId(AkUniqueID const playingId) { m_playingId = playingId; }

	bool                IsToBeRemoved() const                    { return m_toBeRemoved; }
	void                SetToBeRemoved()                         { m_toBeRemoved = true; }

	void                Stop();
	void                UpdateVirtualState(float const distance);

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	CObject const& GetObject() const { return m_object; }
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

private:

	TriggerInstanceId const m_triggerInstanceId;
	CEvent&                 m_event;
	EEventInstanceState     m_state;
	AkUniqueID              m_playingId;
	std::atomic_bool        m_toBeRemoved;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	CObject const& m_object;
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
};
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio

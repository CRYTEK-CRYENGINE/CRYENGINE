// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>
#include <atomic>

namespace FMOD
{
class ChannelGroup;
class DSP;
} // namespace FMOD

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CObject;
class CEvent;
class CReturn;

enum class EEventState : EnumFlagsType
{
	None,
	Playing,
	Virtual,
	Pending,
};

class CEventInstance final : public CPoolObject<CEventInstance, stl::PSyncNone>
{
public:

	CEventInstance() = delete;
	CEventInstance(CEventInstance const&) = delete;
	CEventInstance(CEventInstance&&) = delete;
	CEventInstance& operator=(CEventInstance const&) = delete;
	CEventInstance& operator=(CEventInstance&&) = delete;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	explicit CEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event, CObject const& object)
		: m_triggerInstanceId(triggerInstanceId)
		, m_event(event)
		, m_state(EEventState::Pending)
		, m_lowpassFrequencyMax(0.0f)
		, m_lowpassFrequencyMin(0.0f)
		, m_pInstance(nullptr)
		, m_pMasterTrack(nullptr)
		, m_pLowpass(nullptr)
		, m_toBeRemoved(false)
		, m_isPaused(false)
		, m_isFadingOut(false)
		, m_object(object)
	{}
#else
	explicit CEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event)
		: m_triggerInstanceId(triggerInstanceId)
		, m_event(event)
		, m_state(EEventState::Pending)
		, m_lowpassFrequencyMax(0.0f)
		, m_lowpassFrequencyMin(0.0f)
		, m_pInstance(nullptr)
		, m_pMasterTrack(nullptr)
		, m_pLowpass(nullptr)
		, m_toBeRemoved(false)
		, m_isPaused(false)
	{}
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	~CEventInstance();

	TriggerInstanceId            GetTriggerInstanceId() const                                       { return m_triggerInstanceId; }
	CEvent&                      GetEvent() const                                                   { return m_event; }
	EEventState                  GetState() const                                                   { return m_state; }

	FMOD::Studio::EventInstance* GetFmodEventInstance() const                                       { return m_pInstance; }
	void                         SetFmodEventInstance(FMOD::Studio::EventInstance* const pInstance) { m_pInstance = pInstance; }

	void                         SetListenermask(int const mask)                                    { m_pInstance->setListenerMask(mask); }

	bool                         PrepareForOcclusion();
	void                         SetOcclusion(float const occlusion);
	void                         SetReturnSend(CReturn const* const pReturn, float const value);
	void                         UpdateVirtualState();
	void                         StopAllowFadeOut();
	void                         StopImmediate();

	void                         SetToBeRemoved()               { m_toBeRemoved = true; }
	bool                         IsToBeRemoved() const          { return m_toBeRemoved; }

	void                         SetPaused(bool const isPaused) { m_isPaused = isPaused; }
	bool                         IsPaused() const               { return m_isPaused; }

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	bool           IsFadingOut() const { return m_isFadingOut; }
	CObject const& GetObject() const   { return m_object; }
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

private:

	TriggerInstanceId const      m_triggerInstanceId;
	CEvent&                      m_event;

	EEventState                  m_state;

	float                        m_lowpassFrequencyMax;
	float                        m_lowpassFrequencyMin;

	FMOD::Studio::EventInstance* m_pInstance;
	FMOD::ChannelGroup*          m_pMasterTrack;
	FMOD::DSP*                   m_pLowpass;

	std::atomic_bool             m_toBeRemoved;
	bool                         m_isPaused;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	bool           m_isFadingOut;
	CObject const& m_object;
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
};
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio

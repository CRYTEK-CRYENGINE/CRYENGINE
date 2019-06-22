// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>
#include <portaudio.h>
#include <atomic>

// Forward declare C struct
struct SNDFILE_tag;
using SNDFILE = struct SNDFILE_tag;

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
enum class EEventInstanceState : EnumFlagsType
{
	None,
	Playing,
	Stopped,
	Done,
	WaitingForDestruction,
};

class CObject;
class CEvent;

class CEventInstance final : public CPoolObject<CEventInstance, stl::PSyncNone>
{
public:

	CEventInstance() = delete;
	CEventInstance(CEventInstance const&) = delete;
	CEventInstance(CEventInstance&&) = delete;
	CEventInstance& operator=(CEventInstance const&) = delete;
	CEventInstance& operator=(CEventInstance&&) = delete;

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	explicit CEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event, CObject const& object)
		: m_triggerInstanceId(triggerInstanceId)
		, m_event(event)
		, m_pSndFile(nullptr)
		, m_pStream(nullptr)
		, m_pData(nullptr)
		, m_numChannels(0)
		, m_remainingLoops(0)
		, m_state(EEventInstanceState::None)
		, m_toBeRemoved(false)
		, m_object(object)
	{}
#else
	explicit CEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event)
		: m_triggerInstanceId(triggerInstanceId)
		, m_event(event)
		, m_pSndFile(nullptr)
		, m_pStream(nullptr)
		, m_pData(nullptr)
		, m_numChannels(0)
		, m_remainingLoops(0)
		, m_state(EEventInstanceState::None)
		, m_toBeRemoved(false)
	{}
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

	~CEventInstance();

	bool Execute(
		int const numLoops,
		double const sampleRate,
		CryFixedStringT<MaxFilePathLength> const& filePath,
		PaStreamParameters const& streamParameters);
	void                Update();
	void                Stop();

	TriggerInstanceId   GetTriggerInstanceId() const              { return m_triggerInstanceId; }
	CEvent&             GetEvent() const                          { return m_event; }
	SNDFILE*            GetSoundFile() const                      { return m_pSndFile; }
	int                 GetNumChannels() const                    { return m_numChannels; }
	void*               GetData() const                           { return m_pData; }
	PaSampleFormat      GetSampleFormat() const                   { return m_sampleFormat; }
	bool                IsToBeRemoved() const                     { return m_toBeRemoved; }

	EEventInstanceState GetState() const                          { return m_state; }
	void                SetState(EEventInstanceState const state) { m_state = state; }

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	CObject const& GetObject() const { return m_object; }
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

private:

	void Reset();

	TriggerInstanceId const          m_triggerInstanceId;
	CEvent&                          m_event;
	SNDFILE*                         m_pSndFile;
	PaStream*                        m_pStream;
	void*                            m_pData;
	int                              m_numChannels;
	int                              m_remainingLoops;
	std::atomic<EEventInstanceState> m_state;
	bool                             m_toBeRemoved;
	PaSampleFormat                   m_sampleFormat;

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	CObject const& m_object;
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE
};
} // namespace PortAudio
} // namespace Impl
} // namespace CryAudio

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EventInstance.h"
#include "Object.h"
#include <CryAudio/IAudioSystem.h>

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	#include <Logger.h>
#endif        // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

#include <portaudio.h>
#include <sndfile.hh>

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
static long unsigned const s_bufferLength = 256;

// Callbacks
//////////////////////////////////////////////////////////////////////////
static int StreamCallback(
	void const* pInputBuffer,
	void* pOutputBuffer,
	long unsigned framesPerBuffer,
	PaStreamCallbackTimeInfo const* pTimeInfo,
	PaStreamCallbackFlags statusFlags,
	void* pUserData)
{
	CRY_ASSERT(framesPerBuffer == s_bufferLength);
	auto const pEventInstance = static_cast<CEventInstance*>(pUserData);

	if (pEventInstance->GetState() == EEventInstanceState::Playing)
	{
		sf_count_t numFramesRead = 0;

		switch (pEventInstance->GetSampleFormat())
		{
		case paInt16:
			{
				auto pStreamData = static_cast<short*>(pOutputBuffer);
				numFramesRead = sf_readf_short(pEventInstance->GetSoundFile(), static_cast<short*>(pEventInstance->GetData()), static_cast<sf_count_t>(s_bufferLength));
				int const numChannels = pEventInstance->GetNumChannels();

				for (sf_count_t i = 0; i < numFramesRead; ++i)
				{
					for (int j = 0; j < numChannels; ++j)
					{
						*pStreamData++ = static_cast<short*>(pEventInstance->GetData())[i * numChannels + j];
					}
				}

				break;
			}
		case paInt32:
			{
				auto pStreamData = static_cast<int*>(pOutputBuffer);
				numFramesRead = sf_readf_int(pEventInstance->GetSoundFile(), static_cast<int*>(pEventInstance->GetData()), static_cast<sf_count_t>(s_bufferLength));
				int const numChannels = pEventInstance->GetNumChannels();

				for (sf_count_t i = 0; i < numFramesRead; ++i)
				{
					for (int j = 0; j < numChannels; ++j)
					{
						*pStreamData++ = static_cast<int*>(pEventInstance->GetData())[i * numChannels + j];
					}
				}

				break;
			}
		case paFloat32:
			{
				auto pStreamData = static_cast<float*>(pOutputBuffer);
				numFramesRead = sf_readf_float(pEventInstance->GetSoundFile(), static_cast<float*>(pEventInstance->GetData()), static_cast<sf_count_t>(s_bufferLength));
				int const numChannels = pEventInstance->GetNumChannels();

				for (sf_count_t i = 0; i < numFramesRead; ++i)
				{
					for (int j = 0; j < numChannels; ++j)
					{
						*pStreamData++ = static_cast<float*>(pEventInstance->GetData())[i * numChannels + j];
					}
				}

				break;
			}
		default:
			{
				break;
			}
		}

		if (numFramesRead != framesPerBuffer)
		{
			pEventInstance->SetState(EEventInstanceState::Done);
		}
	}

	return PaStreamCallbackResult::paContinue;
}

//////////////////////////////////////////////////////////////////////////
CEventInstance::~CEventInstance()
{
	CRY_ASSERT_MESSAGE(m_pStream == nullptr, "PortAudio pStream hasn't been closed  during %s", __FUNCTION__);
	CRY_ASSERT_MESSAGE(m_pSndFile == nullptr, "PortAudio pSndFile hasn't been closed  during %s", __FUNCTION__);
	CRY_ASSERT_MESSAGE(m_pData == nullptr, "PortAudio pData hasn't been freed  during %s", __FUNCTION__);
}

//////////////////////////////////////////////////////////////////////////
bool CEventInstance::Execute(
	int const numLoops,
	double const sampleRate,
	CryFixedStringT<MaxFilePathLength> const& filePath,
	PaStreamParameters const& streamParameters)
{
	CRY_ASSERT_MESSAGE(m_state == EEventInstanceState::None, "PortAudio event is not in None state during %s", __FUNCTION__);
	CRY_ASSERT_MESSAGE(m_pStream == nullptr, "PortAudio pStream is valid during %s", __FUNCTION__);
	CRY_ASSERT_MESSAGE(m_pSndFile == nullptr, "PortAudio pSndFile is valid during %s", __FUNCTION__);
	CRY_ASSERT_MESSAGE(m_pData == nullptr, "PortAudio pData is valid during %s", __FUNCTION__);

	SF_INFO sfInfo;
	ZeroStruct(sfInfo);
	m_pSndFile = sf_open(filePath.c_str(), SFM_READ, &sfInfo);

	if (m_pSndFile != nullptr)
	{
		PaError err = Pa_OpenStream(
			&m_pStream,
			nullptr,
			&streamParameters,
			sampleRate,
			s_bufferLength,
			paClipOff,
			&StreamCallback,
			static_cast<void*>(this));

		if (err == paNoError)
		{
			m_numChannels = streamParameters.channelCount;
			m_remainingLoops = numLoops;
			m_sampleFormat = streamParameters.sampleFormat;
			auto const numEntries = static_cast<size_t>(s_bufferLength * m_numChannels);

			switch (m_sampleFormat)
			{
			case paInt16:
				{
					m_pData = CryModuleMalloc(sizeof(short) * numEntries);
					std::fill(static_cast<short*>(m_pData), static_cast<short*>(m_pData) + numEntries, 0);

					break;
				}
			case paInt32:
				{
					m_pData = CryModuleMalloc(sizeof(int) * numEntries);
					std::fill(static_cast<int*>(m_pData), static_cast<int*>(m_pData) + numEntries, 0);

					break;
				}
			case paFloat32:
				{
					m_pData = CryModuleMalloc(sizeof(float) * numEntries);
					std::fill(static_cast<float*>(m_pData), static_cast<float*>(m_pData) + numEntries, 0.0f);

					break;
				}
			default:
				{
					break;
				}
			}

			err = Pa_StartStream(m_pStream);

			if (err == paNoError)
			{
				m_state = EEventInstanceState::Playing;
			}
			else
			{
#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
				Cry::Audio::Log(ELogType::Error, "StartStream failed: %s", Pa_GetErrorText(err));
#endif        // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

				Reset();

				// This event instance will be immediately destructed so this isn't really necessary but to make things more obvious and to help debugging.
				m_state = EEventInstanceState::WaitingForDestruction;
			}
		}
#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Error, "OpenStream failed: %s", Pa_GetErrorText(err));
		}
#endif        // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE
	}

	return m_state == EEventInstanceState::Playing;
}

//////////////////////////////////////////////////////////////////////////
void CEventInstance::Update()
{
	CRY_ASSERT(m_state != EEventInstanceState::None);

	if (m_state != EEventInstanceState::WaitingForDestruction)
	{
		if (m_state == EEventInstanceState::Stopped)
		{
			CRY_ASSERT_MESSAGE(m_pStream == nullptr, "PortAudio pStream still valid during %s", __FUNCTION__);
			CRY_ASSERT_MESSAGE(m_pSndFile == nullptr, "PortAudio pSndFile still valid during %s", __FUNCTION__);
			CRY_ASSERT_MESSAGE(m_pData == nullptr, "PortAudio pData still valid during %s", __FUNCTION__);
			m_state = EEventInstanceState::WaitingForDestruction;
			m_toBeRemoved = true;
		}
		else if (m_state == EEventInstanceState::Done)
		{
			CRY_ASSERT_MESSAGE(m_pStream != nullptr, "PortAudio pStream not valid during %s", __FUNCTION__);
			CRY_ASSERT_MESSAGE(m_pSndFile != nullptr, "PortAudio pSndFile not valid during %s", __FUNCTION__);
			CRY_ASSERT_MESSAGE(m_pData != nullptr, "PortAudio pData not valid during %s", __FUNCTION__);

			if (m_remainingLoops != 0)
			{
				sf_seek(m_pSndFile, 0, SEEK_SET);

				if (m_remainingLoops > 0)
				{
					--m_remainingLoops;
				}

				m_state = EEventInstanceState::Playing;
			}
			else
			{
				Reset();
				m_state = EEventInstanceState::WaitingForDestruction;
				m_toBeRemoved = true;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEventInstance::Stop()
{
	Reset();
	m_state = EEventInstanceState::Stopped;
}

//////////////////////////////////////////////////////////////////////////
void CEventInstance::Reset()
{
	if (m_pStream != nullptr)
	{
#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
		PaError const err = Pa_AbortStream(m_pStream);

		if (err != paNoError)
		{
			Cry::Audio::Log(ELogType::Error, "Pa_AbortStream failed: %s", Pa_GetErrorText(err));
		}
#else
		Pa_AbortStream(m_pStream);
#endif        // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

		m_pStream = nullptr;
	}

	if (m_pSndFile != nullptr)
	{
#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
		int const result = sf_close(m_pSndFile);

		if (result != 0)
		{
			Cry::Audio::Log(ELogType::Error, "sf_close failed: %n", result);
		}
#else
		sf_close(m_pSndFile);
#endif        // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

		m_pSndFile = nullptr;
	}

	if (m_pData != nullptr)
	{
		CryModuleFree(m_pData);
		m_pData = nullptr;
	}
}
} // namespace PortAudio
} // namespace Impl
} // namespace CryAudio

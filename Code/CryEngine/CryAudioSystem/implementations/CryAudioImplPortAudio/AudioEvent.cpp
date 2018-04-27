// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioEvent.h"
#include "AudioObject.h"
#include <Logger.h>
#include <CryAudio/IAudioSystem.h>
#include <CrySystem/ISystem.h> // needed for gEnv in Release builds
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
	auto const pEvent = static_cast<CEvent*>(pUserData);

	if (pEvent->state == EEventState::Playing)
	{
		sf_count_t numFramesRead = 0;

		switch (pEvent->sampleFormat)
		{
		case paInt16:
			{
				auto pStreamData = static_cast<short*>(pOutputBuffer);
				numFramesRead = sf_readf_short(pEvent->pSndFile, static_cast<short*>(pEvent->pData), static_cast<sf_count_t>(s_bufferLength));

				for (sf_count_t i = 0; i < numFramesRead; ++i)
				{
					for (int j = 0; j < pEvent->numChannels; ++j)
					{
						*pStreamData++ = static_cast<short*>(pEvent->pData)[i * pEvent->numChannels + j];
					}
				}
			}
			break;
		case paInt32:
			{
				auto pStreamData = static_cast<int*>(pOutputBuffer);
				numFramesRead = sf_readf_int(pEvent->pSndFile, static_cast<int*>(pEvent->pData), static_cast<sf_count_t>(s_bufferLength));

				for (sf_count_t i = 0; i < numFramesRead; ++i)
				{
					for (int j = 0; j < pEvent->numChannels; ++j)
					{
						*pStreamData++ = static_cast<int*>(pEvent->pData)[i * pEvent->numChannels + j];
					}
				}
			}
			break;
		case paFloat32:
			{
				auto pStreamData = static_cast<float*>(pOutputBuffer);
				numFramesRead = sf_readf_float(pEvent->pSndFile, static_cast<float*>(pEvent->pData), static_cast<sf_count_t>(s_bufferLength));

				for (sf_count_t i = 0; i < numFramesRead; ++i)
				{
					for (int j = 0; j < pEvent->numChannels; ++j)
					{
						*pStreamData++ = static_cast<float*>(pEvent->pData)[i * pEvent->numChannels + j];
					}
				}
			}
			break;
		}

		if (numFramesRead != framesPerBuffer)
		{
			pEvent->state = EEventState::Done;
		}
	}

	return PaStreamCallbackResult::paContinue;
}

//////////////////////////////////////////////////////////////////////////
CEvent::CEvent(CATLEvent& event_)
	: pSndFile(nullptr)
	, pStream(nullptr)
	, pData(nullptr)
	, pObject(nullptr)
	, numChannels(0)
	, remainingLoops(0)
	, event(event_)
	, state(EEventState::None)
{
}

//////////////////////////////////////////////////////////////////////////
CEvent::~CEvent()
{
	CRY_ASSERT_MESSAGE(pStream == nullptr, "PortAudio pStream hasn't been closed on event destruction.");
	CRY_ASSERT_MESSAGE(pSndFile == nullptr, "PortAudio pSndFile hasn't been closed on event destruction.");
	CRY_ASSERT_MESSAGE(pData == nullptr, "PortAudio pData hasn't been freed on event destruction.");

	if (pObject != nullptr)
	{
		pObject->UnregisterEvent(this);
		pObject = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEvent::Execute(
  int const numLoops,
  double const sampleRate,
  CryFixedStringT<MaxFilePathLength> const& filePath,
  PaStreamParameters const& streamParameters)
{
	CRY_ASSERT_MESSAGE(state == EEventState::None, "PortAudio event is not in None state during Execute.");
	CRY_ASSERT_MESSAGE(pStream == nullptr, "PortAudio pStream is valid during event execution.");
	CRY_ASSERT_MESSAGE(pSndFile == nullptr, "PortAudio pSndFile is valid during event execution.");
	CRY_ASSERT_MESSAGE(pData == nullptr, "PortAudio pData is valid during event execution.");

	SF_INFO sfInfo;
	ZeroStruct(sfInfo);
	pSndFile = sf_open(filePath.c_str(), SFM_READ, &sfInfo);

	if (pSndFile != nullptr)
	{
		PaError err = Pa_OpenStream(
		  &pStream,
		  nullptr,
		  &streamParameters,
		  sampleRate,
		  s_bufferLength,
		  paClipOff,
		  &StreamCallback,
		  static_cast<void*>(this));

		if (err == paNoError)
		{
			numChannels = streamParameters.channelCount;
			remainingLoops = numLoops;
			sampleFormat = streamParameters.sampleFormat;
			auto const numEntries = static_cast<size_t>(s_bufferLength * numChannels);

			switch (sampleFormat)
			{
			case paInt16:
				{
					pData = CryModuleMalloc(sizeof(short) * numEntries);
					std::fill(static_cast<short*>(pData), static_cast<short*>(pData) + numEntries, 0);
				}
				break;
			case paInt32:
				{
					pData = CryModuleMalloc(sizeof(int) * numEntries);
					std::fill(static_cast<int*>(pData), static_cast<int*>(pData) + numEntries, 0);
				}
				break;
			case paFloat32:
				{
					pData = CryModuleMalloc(sizeof(float) * numEntries);
					std::fill(static_cast<float*>(pData), static_cast<float*>(pData) + numEntries, 0.0f);
				}
				break;
			}

			err = Pa_StartStream(pStream);

			if (err == paNoError)
			{
				state = EEventState::Playing;
			}
			else
			{
				Cry::Audio::Log(ELogType::Error, "StartStream failed: %s", Pa_GetErrorText(err));
				Reset();

				// This event instance will be immediately destructed so this isn't really necessary but to make things more obvious and to help debugging.
				state = EEventState::WaitingForDestruction;
			}
		}
		else
		{
			Cry::Audio::Log(ELogType::Error, "OpenStream failed: %s", Pa_GetErrorText(err));
		}
	}

	return state == EEventState::Playing;
}

//////////////////////////////////////////////////////////////////////////
void CEvent::Update()
{
	CRY_ASSERT(state != EEventState::None);

	if (state != EEventState::WaitingForDestruction)
	{
		if (state == EEventState::Stopped)
		{
			CRY_ASSERT_MESSAGE(pStream == nullptr, "PortAudio pStream still valid during event update.");
			CRY_ASSERT_MESSAGE(pSndFile == nullptr, "PortAudio pSndFile still valid during event update.");
			CRY_ASSERT_MESSAGE(pData == nullptr, "PortAudio pData still valid during event update.");
			state = EEventState::WaitingForDestruction;
			gEnv->pAudioSystem->ReportFinishedEvent(event, true);
		}
		else if (state == EEventState::Done)
		{
			CRY_ASSERT_MESSAGE(pStream != nullptr, "PortAudio pStream not valid during event update.");
			CRY_ASSERT_MESSAGE(pSndFile != nullptr, "PortAudio pSndFile not valid during event update.");
			CRY_ASSERT_MESSAGE(pData != nullptr, "PortAudio pData not valid during event update.");

			if (remainingLoops != 0)
			{
				sf_seek(pSndFile, 0, SEEK_SET);

				if (remainingLoops > 0)
				{
					--remainingLoops;
				}

				state = EEventState::Playing;
			}
			else
			{
				Reset();
				state = EEventState::WaitingForDestruction;
				gEnv->pAudioSystem->ReportFinishedEvent(event, true);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CEvent::Stop()
{
	Reset();
	state = EEventState::Stopped;
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
void CEvent::Reset()
{
	if (pStream != nullptr)
	{
		PaError const err = Pa_AbortStream(pStream);

		if (err != paNoError)
		{
			Cry::Audio::Log(ELogType::Error, "Pa_AbortStream failed: %s", Pa_GetErrorText(err));
		}

		pStream = nullptr;
	}

	if (pSndFile != nullptr)
	{
		int const result = sf_close(pSndFile);

		if (result != 0)
		{
			Cry::Audio::Log(ELogType::Error, "sf_close failed: %n", result);
		}

		pSndFile = nullptr;
	}

	if (pData != nullptr)
	{
		CryModuleFree(pData);
		pData = nullptr;
	}
}
} // namespace PortAudio
} // namespace Impl
} // namespace CryAudio

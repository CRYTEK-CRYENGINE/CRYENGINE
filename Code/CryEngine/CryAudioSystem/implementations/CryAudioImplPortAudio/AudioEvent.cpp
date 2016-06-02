// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioEvent.h"
#include <CryAudio/IAudioSystem.h>
#include <portaudio.h>
#include <sndfile.hh>

using namespace CryAudio::Impl::PortAudio;

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
	sf_count_t numFramesRead = 0;
	CAudioEvent* const pAudioEvent = static_cast<CAudioEvent*>(pUserData);

	switch (pAudioEvent->sampleFormat)
	{
	case paInt16:
		{
			short* pStreamData = static_cast<short*>(pOutputBuffer);
			numFramesRead = sf_readf_short(pAudioEvent->pSndFile, static_cast<short*>(pAudioEvent->pData), static_cast<sf_count_t>(s_bufferLength));

			for (sf_count_t i = 0; i < numFramesRead; ++i)
			{
				for (int j = 0; j < pAudioEvent->numChannels; ++j)
				{
					*pStreamData++ = static_cast<short*>(pAudioEvent->pData)[i * pAudioEvent->numChannels + j];
				}
			}
		}
		break;
	case paInt32:
		{
			int* pStreamData = static_cast<int*>(pOutputBuffer);
			numFramesRead = sf_readf_int(pAudioEvent->pSndFile, static_cast<int*>(pAudioEvent->pData), static_cast<sf_count_t>(s_bufferLength));

			for (sf_count_t i = 0; i < numFramesRead; ++i)
			{
				for (int j = 0; j < pAudioEvent->numChannels; ++j)
				{
					*pStreamData++ = static_cast<int*>(pAudioEvent->pData)[i * pAudioEvent->numChannels + j];
				}
			}
		}
		break;
	case paFloat32:
		{
			float* pStreamData = static_cast<float*>(pOutputBuffer);
			numFramesRead = sf_readf_float(pAudioEvent->pSndFile, static_cast<float*>(pAudioEvent->pData), static_cast<sf_count_t>(s_bufferLength));

			for (sf_count_t i = 0; i < numFramesRead; ++i)
			{
				for (int j = 0; j < pAudioEvent->numChannels; ++j)
				{
					*pStreamData++ = static_cast<float*>(pAudioEvent->pData)[i * pAudioEvent->numChannels + j];
				}
			}
		}
		break;
	}

	if (numFramesRead != framesPerBuffer)
	{
		pAudioEvent->bDone = true;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
CAudioEvent::CAudioEvent(AudioEventId const _audioEventId)
	: pSndFile(nullptr)
	, pStream(nullptr)
	, pData(nullptr)
	, pPAAudioObject(nullptr)
	, numChannels(0)
	, remainingLoops(0)
	, audioEventId(_audioEventId)
	, bDone(false)
{
}

//////////////////////////////////////////////////////////////////////////
CAudioEvent::~CAudioEvent()
{
	if (pData != nullptr)
	{
		POOL_FREE(pData);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAudioEvent::Execute(
  int const numLoops,
  double const sampleRate,
  CryFixedStringT<512> const& filePath,
  PaStreamParameters const& streamParameters)
{
	bool bSuccess = false;
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
			long unsigned const numEntries = s_bufferLength * numChannels;

			switch (sampleFormat)
			{
			case paInt16:
				{
					POOL_NEW_CUSTOM(short, pData, numEntries);
					ZeroMemory(pData, sizeof(short) * numEntries);
				}
				break;
			case paInt32:
				{
					POOL_NEW_CUSTOM(int, pData, numEntries);
					ZeroMemory(pData, sizeof(int) * numEntries);
				}
				break;
			case paFloat32:
				{
					POOL_NEW_CUSTOM(float, pData, numEntries);
					ZeroMemory(pData, sizeof(float) * numEntries);
				}
				break;
			}

			err = Pa_StartStream(pStream);

			if (err == paNoError)
			{
				bSuccess = true;
			}
			else
			{
				g_audioImplLogger.Log(eAudioLogType_Error, "StartStream failed: %s", Pa_GetErrorText(err));
			}
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Error, "OpenStream failed: %s", Pa_GetErrorText(err));
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
void CAudioEvent::Stop()
{
	// This should call Reset() on this object.
	SAudioRequest request;
	SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportFinishedEvent> requestData(audioEventId, true);
	request.flags = eAudioRequestFlags_ThreadSafePush;
	request.pData = &requestData;

	gEnv->pAudioSystem->PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////
void CAudioEvent::Reset()
{
	CRY_ASSERT(pStream != nullptr);
	PaError const err = Pa_CloseStream(pStream);

	if (err != paNoError)
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "CloseStream failed: %s", Pa_GetErrorText(err));
	}

	if (pSndFile != nullptr)
	{
		sf_close(pSndFile);
		pSndFile = nullptr;
	}

	pStream = nullptr;
	bDone = false;
}

//////////////////////////////////////////////////////////////////////////
void CAudioEvent::Update()
{
	if (bDone)
	{
		if (remainingLoops != 0 && pStream != nullptr && pSndFile != nullptr)
		{
			sf_seek(pSndFile, 0, SEEK_SET);

			if (remainingLoops > 0)
			{
				--remainingLoops;
			}
		}
		else
		{
			SAudioRequest request;
			SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportFinishedEvent> requestData(audioEventId, true);
			request.flags = eAudioRequestFlags_ThreadSafePush;
			request.pData = &requestData;

			gEnv->pAudioSystem->PushRequest(request);
		}

		bDone = false;
	}
}

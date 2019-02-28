// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)

	#include "VoiceManager.h"
	#include "IVoiceEncoder.h"
	#include "IVoiceDecoder.h"
	#include <CryRenderer/Tarray.h>

	#include <speex.h>
	#include <speex/speex_preprocess.h>

//TODO: move to syncronized CVAR
const int SAMPLE_RATE = 8000;

static std::vector<float> tempBuffer;

class CSpeexEncoder : public IVoiceEncoder
{
public:
	CSpeexEncoder()
	{
		speex_bits_init(&m_bits);
		m_pEncState = speex_encoder_init(&speex_nb_mode);

		m_pPreprocessor = speex_preprocess_state_init(GetFrameSize(), SAMPLE_RATE);

		int tmp = 12000;
		speex_encoder_ctl(m_pEncState, SPEEX_SET_ABR, &tmp);
		/*int tmp=9;
		   speex_encoder_ctl( m_pEncState, SPEEX_SET_QUALITY, &tmp);*/
		tmp = 1;
		speex_encoder_ctl(m_pEncState, SPEEX_SET_VAD, &tmp);
		tmp = 1;
		speex_encoder_ctl(m_pEncState, SPEEX_SET_DTX, &tmp);
		tmp = SAMPLE_RATE;
		speex_encoder_ctl(m_pEncState, SPEEX_SET_SAMPLING_RATE, &tmp);

		tmp = 1;
		speex_preprocess_ctl(m_pPreprocessor, SPEEX_PREPROCESS_SET_VAD, &tmp);
		tmp = 1;
		speex_preprocess_ctl(m_pPreprocessor, SPEEX_PREPROCESS_SET_DENOISE, &tmp);
		tmp = 1;
		speex_preprocess_ctl(m_pPreprocessor, SPEEX_PREPROCESS_SET_AGC, &tmp);
	}

	~CSpeexEncoder()
	{
		speex_bits_destroy(&m_bits);
		speex_encoder_destroy(m_pEncState);
		speex_preprocess_state_destroy(m_pPreprocessor);
	}

	virtual int GetFrameSize()
	{
		int frameSize;
		speex_encoder_ctl(m_pEncState, SPEEX_GET_FRAME_SIZE, &frameSize);
		return frameSize;
	}

	virtual void EncodeFrame(int numSamples, const int16* pSamples, TVoicePacketPtr pkt)
	{
		if (!speex_preprocess(m_pPreprocessor, const_cast<int16*>(pSamples), NULL))
		{
			pkt->SetLength(0);
			return;
		}

		if (tempBuffer.size() < (uint32)numSamples)
			tempBuffer.resize((uint32)numSamples);
		for (int i = 0; i < numSamples; i++)
			tempBuffer[i] = pSamples[i];

		speex_bits_reset(&m_bits);
		bool transmit = (0 != speex_encode(m_pEncState, &tempBuffer[0], &m_bits));
		if (transmit)
		{
			int len = speex_bits_write(&m_bits, (char*)pkt->GetData(), CVoicePacket::MAX_LENGTH);
			pkt->SetLength(len);
		}
		else
			pkt->SetLength(0);
	}

	virtual void Reset()
	{
		//speex_encoder_ctl( m_pEncState, SPEEX_RESET_STATE, 0);
	}

	virtual void Release()
	{
		delete this;
	}

	virtual void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CSpeexEncoder");

		pSizer->Add(*this);
		pSizer->AddObject(m_pPreprocessor, sizeof(SpeexPreprocessState));
	}

private:
	SpeexBits             m_bits;
	void*                 m_pEncState;
	SpeexPreprocessState* m_pPreprocessor;
};

class CSpeexDecoder : public IVoiceDecoder
{
public:
	CSpeexDecoder()
	{
		speex_bits_init(&m_bits);
		m_pDecState = speex_decoder_init(&speex_nb_mode);
		int tmp = SAMPLE_RATE;
		speex_decoder_ctl(m_pDecState, SPEEX_SET_SAMPLING_RATE, &tmp);
	}

	~CSpeexDecoder()
	{
		speex_bits_destroy(&m_bits);
		speex_decoder_destroy(m_pDecState);
	}

	virtual int GetFrameSize()
	{
		int frameSize;
		speex_decoder_ctl(m_pDecState, SPEEX_GET_FRAME_SIZE, &frameSize);
		return frameSize;
	}

	virtual void DecodeFrame(const CVoicePacket& pkt, int numSamples, int16* samples)
	{
		if (tempBuffer.size() < (uint32)numSamples)
			tempBuffer.resize(numSamples);

		speex_bits_read_from(&m_bits, (char*)pkt.GetData(), pkt.GetLength());
		speex_decode(m_pDecState, &m_bits, &tempBuffer[0]);

		for (int i = 0; i < numSamples; i++)
			samples[i] = (short) CLAMP(tempBuffer[i], -32768.0f, 32767.0f);
	}

	virtual void DecodeSkippedFrame(int numSamples, int16* samples)
	{
		if (tempBuffer.size() < (uint32)numSamples)
			tempBuffer.resize(numSamples);

		//speex_bits_read_from( &m_bits, NULL, 0 );
		int ret = speex_decode(m_pDecState, NULL, &tempBuffer[0]);

		NET_ASSERT(!ret && "error decoding stream");

		//NET_ASSERT(speex_bits_remaining(&m_bits)<0 && "error decoding stream - stream is corrupted");

		for (int i = 0; i < numSamples; i++)
			samples[i] = (short) CLAMP(tempBuffer[i], -32768.0f, 32767.0f);
	}

	virtual void Release()
	{
		delete this;
	}

	virtual void Reset()
	{
		//speex_decoder_ctl( m_pDecState, SPEEX_RESET_STATE, 0);
	}

	virtual void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CSpeexDecoder");

		pSizer->Add(*this);
	}

private:
	SpeexBits m_bits;
	void*     m_pDecState;
};

REGISTER_CODEC(speex, CSpeexEncoder, CSpeexDecoder);

#endif

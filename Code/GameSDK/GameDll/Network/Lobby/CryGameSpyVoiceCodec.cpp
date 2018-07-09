// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CryGameSpyVoiceCodec.h"

#if USE_CRYLOBBY_GAMESPY && USE_CRYLOBBY_GAMESPY_VOIP

SCryGameSpyVoiceCodecInfo CCryGameSpyVoiceCodec::m_info;

#if GAMESPY_USE_SPEEX_CODEC

#ifdef _DEBUG
#pragma comment( lib, "libspeex-1.0.5_d.lib" )
#else
#pragma comment( lib, "libspeex-1.0.5.lib" )
#endif // _DEBUG

#define GAMESPY_SPEEX_SAMPLE_RATE (8000)
#define GAMESPY_SPEEX_QUALITY (4)
#define GAMESPY_SPEEX_PERCEPTUAL_ENHANCEMENT (1)

GVCustomCodecInfo CCryGameSpyVoiceCodec::m_speexCodecInfo;

SpeexBits	CCryGameSpyVoiceCodec::m_speexBits;
void*			CCryGameSpyVoiceCodec::m_pspeexEncoderState = NULL;
int16*		CCryGameSpyVoiceCodec::m_pspeexBuffer = NULL;
int				CCryGameSpyVoiceCodec::m_speexSamplesPerFrame = 0;
int				CCryGameSpyVoiceCodec::m_speexEncodedFrameSize = 0;

#endif// GAMESPY_USE_SPEEX_CODEC


void* CCryGameSpyVoiceCodec::Initialise(void)
{
	m_info.m_pCustomCodecInfo = NULL;
	m_info.m_pTerminateCallback = CCryGameSpyVoiceCodec::Terminate;

#if GAMESPY_USE_SPEEX_CODEC
	m_pspeexEncoderState = speex_encoder_init(&speex_nb_mode);
	if (m_pspeexEncoderState != NULL)
	{
		int samplesPerSecond = GAMESPY_SPEEX_SAMPLE_RATE;
		int quality = GAMESPY_SPEEX_QUALITY;

		speex_encoder_ctl(m_pspeexEncoderState, SPEEX_SET_SAMPLING_RATE, &samplesPerSecond);
		speex_encoder_ctl(m_pspeexEncoderState, SPEEX_GET_FRAME_SIZE, &m_speexSamplesPerFrame);
		speex_encoder_ctl(m_pspeexEncoderState, SPEEX_SET_QUALITY, &quality);
		speex_bits_init(&m_speexBits);

		int rate;
		speex_encoder_ctl(m_pspeexEncoderState, SPEEX_GET_BITRATE, &rate);
		int bitsPerFrame = (rate / (GAMESPY_SPEEX_SAMPLE_RATE / m_speexSamplesPerFrame));
		m_speexEncodedFrameSize = (bitsPerFrame / 8);
		if (bitsPerFrame % 8)
		{
			++m_speexEncodedFrameSize;
		}

		m_pspeexBuffer = new int16[m_speexSamplesPerFrame];
		if (m_pspeexBuffer)
		{
			m_speexCodecInfo.m_samplesPerFrame = m_speexSamplesPerFrame;
			m_speexCodecInfo.m_encodedFrameSize = m_speexEncodedFrameSize;
			m_speexCodecInfo.m_newDecoderCallback = CCryGameSpyVoiceCodec::SpeexNewDecoderCallback;
			m_speexCodecInfo.m_freeDecoderCallback = CCryGameSpyVoiceCodec::SpeexFreeDecoderCallback;
			m_speexCodecInfo.m_encodeCallback = CCryGameSpyVoiceCodec::SpeexEncodeCallback;
			m_speexCodecInfo.m_decodeAddCallback = CCryGameSpyVoiceCodec::SpeexDecodeAddCallback;
			m_speexCodecInfo.m_decodeSetCallback = CCryGameSpyVoiceCodec::SpeexDecodeSetCallback;

			m_info.m_pCustomCodecInfo = &m_speexCodecInfo;
		}
	}
#endif// GAMESPY_USE_SPEEX_CODEC

	return &m_info;
}

void CCryGameSpyVoiceCodec::Terminate(void)
{
#if GAMESPY_USE_SPEEX_CODEC
	speex_encoder_destroy(m_pspeexEncoderState);
	m_pspeexEncoderState = NULL;

	speex_bits_destroy(&m_speexBits);

	delete[] m_pspeexBuffer;
	m_pspeexBuffer = NULL;
#endif // GAMESPY_USE_SPEEX_CODEC
}

#if GAMESPY_USE_SPEEX_CODEC
GVBool CCryGameSpyVoiceCodec::SpeexNewDecoderCallback(GVDecoderData* pData)
{
	GVBool done = GVFalse;

	void* pDecoder = speex_decoder_init(&speex_nb_mode);
	if (pDecoder != NULL)
	{
		int perceptualEnhancement = GAMESPY_SPEEX_PERCEPTUAL_ENHANCEMENT;
		speex_decoder_ctl(pDecoder, SPEEX_SET_ENH, &perceptualEnhancement);
		*pData = pDecoder;
		done = GVTrue;
	}

	return done;
}

void CCryGameSpyVoiceCodec::SpeexFreeDecoderCallback(GVDecoderData data)
{
	speex_decoder_destroy(data);
}

void CCryGameSpyVoiceCodec::SpeexEncodeCallback(GVByte* pOut, const GVSample* pIn)
{
	speex_bits_reset(&m_speexBits);

	speex_encode_int(m_pspeexEncoderState, (short*)pIn, &m_speexBits);

	int bytesWritten = speex_bits_write(&m_speexBits, (char*)pOut, m_speexEncodedFrameSize);
	CRY_ASSERT(bytesWritten == m_speexEncodedFrameSize);
}

void CCryGameSpyVoiceCodec::SpeexDecodeAddCallback(GVSample* pOut, const GVByte* pIn, GVDecoderData data)
{
	SpeexDecodeSetCallback(m_pspeexBuffer, pIn, data);

	for (uint32 index = 0; index < m_speexSamplesPerFrame; ++index)
	{
		pOut[index] += m_pspeexBuffer[index];
	}
}

void CCryGameSpyVoiceCodec::SpeexDecodeSetCallback(GVSample* pOut, const GVByte* pIn, GVDecoderData data)
{
	speex_bits_read_from(&m_speexBits, (char*)pIn, m_speexEncodedFrameSize);

	int rcode = speex_decode_int(data, &m_speexBits, pOut);
	CRY_ASSERT(rcode == 0);
}
#endif // GAMESPY_USE_SPEEX_CODEC

#endif // USE_CRYLOBBY_GAMESPY && USE_CRYLOBBY_GAMESPY_VOIP
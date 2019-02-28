// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYGAMESPYVOICECODEC_H__
#define __CRYGAMESPYVOICECODEC_H__

#include <CryLobby/ICryLobby.h>

#if USE_CRYLOBBY_GAMESPY && USE_CRYLOBBY_GAMESPY_VOIP

extern "C" 
{
#include "Gamespy2.11.00/Source/Voice2/gvCodec.h"
};

#define GAMESPY_USE_SPEEX_CODEC				1

#if GAMESPY_USE_SPEEX_CODEC

#include "speex-1.0.5/include/speex.h"

#endif

// windef.h #defines this as GetUserNameA or GetUserNameW but it's a method of ISystem
#undef GetUserName

class CCryGameSpyVoiceCodec
{
public:
	static void*			Initialise(void);
	static void				Terminate(void);

protected:
	static SCryGameSpyVoiceCodecInfo m_info;

#if GAMESPY_USE_SPEEX_CODEC
	static	GVBool		SpeexNewDecoderCallback(GVDecoderData* pData);
	static	void			SpeexFreeDecoderCallback(GVDecoderData data);
	static	void			SpeexEncodeCallback(GVByte* pOut, const GVSample* pIn);
	static	void			SpeexDecodeAddCallback(GVSample* pOut, const GVByte* pIn, GVDecoderData data);
	static	void			SpeexDecodeSetCallback(GVSample* pOut, const GVByte* pIn, GVDecoderData data);

protected:

	static GVCustomCodecInfo m_speexCodecInfo;

	static SpeexBits	m_speexBits;
	static void*			m_pspeexEncoderState;
	static int16*			m_pspeexBuffer;
	static int				m_speexSamplesPerFrame;
	static int				m_speexEncodedFrameSize;

#endif // GAMESPY_USE_SPEEX_CODEC
};

#endif // USE_CRYLOBBY_GAMESPY && USE_CRYLOBBY_GAMESPY_VOIP
#endif // __CRYGAMESPYVOICECODEC_H__
// [EOF]

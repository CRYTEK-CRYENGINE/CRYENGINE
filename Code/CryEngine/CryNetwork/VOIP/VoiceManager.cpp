// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)

	#include "VoiceManager.h"

CAutoRegCodec* CAutoRegCodec::m_root = NULL;

IVoiceEncoder* CVoiceManager::CreateEncoder(const char* name)
{
	const CAutoRegCodec* pCodec = CAutoRegCodec::FindCodec(name);
	if (pCodec)
		return pCodec->CreateEncoder();
	else
		return NULL;
}

IVoiceDecoder* CVoiceManager::CreateDecoder(const char* name)
{
	const CAutoRegCodec* pCodec = CAutoRegCodec::FindCodec(name);
	if (pCodec)
		return pCodec->CreateDecoder();
	else
		return NULL;
}

CAutoRegCodec::CAutoRegCodec(const char* name, VoiceEncoderFactory enc, VoiceDecoderFactory dec) : m_next(m_root), m_name(name), m_encoderFactory(enc), m_decoderFactory(dec)
{
	m_root = this;
}

const CAutoRegCodec* CAutoRegCodec::FindCodec(const char* name)
{
	CAutoRegCodec* pCur = m_root;
	while (pCur && 0 != stricmp(pCur->m_name, name))
		pCur = pCur->m_next;
	return pCur;
}

#endif

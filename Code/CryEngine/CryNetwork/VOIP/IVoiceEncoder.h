// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IVOICEENCODER_H__
#define __IVOICEENCODER_H__

#pragma once

#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)

	#include "VoicePacket.h"

struct IVoiceEncoder
{
	IVoiceEncoder()
	{
		++g_objcnt.voiceEncoder;
	}
	virtual ~IVoiceEncoder()
	{
		--g_objcnt.voiceEncoder;
	}
	virtual int  GetFrameSize() = 0;
	virtual void EncodeFrame(int numSamples, const int16* pSamples, TVoicePacketPtr vp) = 0;
	virtual void Reset() = 0;
	virtual void Release() = 0;
	virtual void GetMemoryStatistics(ICrySizer* pSizer) = 0;
};

struct IVoicePacketSink
{
	virtual ~IVoicePacketSink(){}
	virtual void AddPacket(const TVoicePacketPtr& pkt) = 0;
};

class CVoiceEncodingSession
{
public:
	CVoiceEncodingSession(IVoiceEncoder* pEnc);
	~CVoiceEncodingSession();
	void AddVoiceFragment(int numSamples, const int16* pSamples, IVoicePacketSink* pSink);
	void Reset(IVoicePacketSink* pSink);
	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CVoiceEncodingSession");

		if (!pSizer->Add(*this))
			return;
		pSizer->AddContainer(m_buildFragment);
	}

private:
	IVoiceEncoder*     m_pEnc;
	std::vector<int16> m_buildFragment;
	size_t             m_buildPos;
	uint8              m_seq;
};

#endif
#endif

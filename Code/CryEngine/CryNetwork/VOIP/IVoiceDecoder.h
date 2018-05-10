// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IVOICEDECODER_H__
#define __IVOICEDECODER_H__

#pragma once

#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)

	#include "VoicePacket.h"
	#include <queue>
	#include <CryMemory/STLPoolAllocator.h>

struct IVoiceDecoder
{
	IVoiceDecoder()
	{
		++g_objcnt.voiceDecoder;
	}
	virtual ~IVoiceDecoder()
	{
		--g_objcnt.voiceDecoder;
	}
	virtual int  GetFrameSize() = 0;
	virtual void DecodeFrame(const CVoicePacket& pkt, int frameSize, int16* samples) = 0;
	virtual void DecodeSkippedFrame(int frameSize, int16* samples) = 0;
	virtual void Reset() = 0;
	virtual void Release() = 0;
	virtual void GetMemoryStatistics(ICrySizer* pSizer) = 0;
};

struct SDecodingStats
{
	SDecodingStats() : MaxPendingPackets(0), MinPendingPackets(9999), ZeroSamples(0), SkippedSamples(0)
	{

	}
	uint32 MaxPendingPackets;
	uint32 MinPendingPackets;
	uint32 ZeroSamples;
	uint32 SkippedSamples;
	uint32 IDFirst, IDCounter;
};

class CVoiceDecodingSession
{
public:
	CVoiceDecodingSession(IVoiceDecoder*);
	void   AddPacket(TVoicePacketPtr pkt);
	void   GetSamples(int numSamples, int16* pSamples);
	uint32 GetPendingPackets();
	void   GetStats(SDecodingStats& s);
	void   GetPackets(std::vector<TVoicePacketPtr>&);
	void   Mute(bool mute);
	void   Pause(bool pause);

	void   GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CVoiceDecodingSession");

		if (!pSizer->Add(*this))
			return;
		pSizer->AddContainer(m_packets);
		pSizer->AddObject(&m_packets, m_packets.size() * sizeof(CVoicePacket));
	}
private:
	static const int   BUFFER_SIZE = 8192;

	CryCriticalSection m_lock;

	IVoiceDecoder*     m_pDecoder;
	#if USE_SYSTEM_ALLOCATOR
	typedef std::map<uint32, TVoicePacketPtr, std::less<uint32>>                                                            PacketMap;
	#else
	typedef std::map<uint32, TVoicePacketPtr, std::less<uint32>, stl::STLPoolAllocator<std::pair<const uint32, TVoicePacketPtr>>> PacketMap;
	#endif
	PacketMap      m_packets;
	uint32         m_counter;
	uint32         m_endCounter;
	uint8          m_endSeq;
	bool           m_zeros;
	int            m_minPendingPackets;
	int            m_samplesSinceCorrection;
	int            m_skippedPackets;
	bool           m_mute;
	bool           m_paused;

	int16          m_currentFramePos;
	int16          m_currentFrameLength;
	int16          m_currentFrame[BUFFER_SIZE];

	SDecodingStats m_stats;
};

#endif
#endif

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RECORDINGSYSTEMSERVERFORWARDER_H__
#define __RECORDINGSYSTEMSERVERFORWARDER_H__

#include "RecordingSystemCircularBuffer.h"

#define KILLCAM_FORWARDING_BUFFER_SIZE	(16*1024)

class CServerKillCamForwarder
{
	typedef CCircularBuffer<KILLCAM_FORWARDING_BUFFER_SIZE> BufferType;
	struct SForwardingPacket;

public:
	void Reset();
	void ReceivePacket(IActor *pActor, const CActor::KillCamFPData &packet);
	void Update();
	void GetMemoryUsage(ICrySizer *pSizer) const
	{
		pSizer->AddContainer(m_forwarding);
	}

private:
	void ForwardPacket(SForwardingPacket &forward, CActor::KillCamFPData *pPacket);
	void DropOldestPacket();
	void AddDataAndErase(const void *data, size_t size);
	void GetFirstPacket(BufferType::iterator &first, IActor *pActor, int packetId);
	CActor::KillCamFPData* FindPacket(BufferType::iterator &it, IActor *pActor, int packetId);
	
	struct SForwardingPacket
	{
		SForwardingPacket()
			: pActor(nullptr)
			, m_sent(0)
			, packetID(0)
			, m_numPackets(0)
		{}

		EntityId victim;
		IActor *pActor;
		int packetID;
		int m_sent;
		int m_numPackets;
		CTimeValue m_lastPacketTime;
		BufferType::iterator iterator;
	};

	std::deque<SForwardingPacket> m_forwarding;
	BufferType m_history;
};

#endif // __RECORDINGSYSTEMSERVERFORWARDER_H__
// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RECORDINGSYSTEMCLIENTSENDER_H__
#define __RECORDINGSYSTEMCLIENTSENDER_H__

#include "RecordingSystemCircularBuffer.h"

#define KILLCAM_SEND_BUFFER_SIZE	(8*1024)

class CClientKillCamSender
{
public:
	enum
	{
		KCP_NEWFIRSTPERSON,
		KCP_NEWTHIRDPERSON,
		KCP_FORWARD,
		KCP_MAX
	};

	CClientKillCamSender();
	void Reset();
	void AddKill(IActor *pShooter, EntityId victim, bool bulletTimeKill, bool bToEveryone);
	void Update();

	void GetMemoryUsage(ICrySizer *pSizer) const
	{
		pSizer->AddContainer(m_kills);
		pSizer->AddContainer(m_sentPackets);
	}

private:
	void SendData();
	void AddKillDataToSendQueue(IActor *pShooter, EntityId victim, float from, float to, bool bulletTimeKill, int &fpPacketOffset, int &tpPacketOffset, bool bFinal, bool bToEveryone);
	int AddFirstPersonDataToSendQueue(IActor *pShooter, EntityId victim, float from, float to, int packetOffset, bool bFinal, bool bToEveryone, float& timeoffset);
	size_t AddVictimDataToSendQueue(IActor *pShooter, EntityId victim, float from, float to, bool bulletTimeKill, int packetOffset, bool bFinal, bool bToEveryone, float timeoffset);
	void AddDataToSendQueue(IActor *pShooter, uint8 packetType, int packetId, EntityId victim, void *data, size_t datasize, int packetOffset, bool bFinal, bool bToEveryone);

private:
	struct SSentKillCamPacket
	{
		float from;
		float to;
		int id;
		uint8 numPackets;
	};

	struct SSendingState
	{
		IActor *pShooter;
		EntityId victim;
		uint16 dataOffset;
		uint16 dataSize;
		uint8 packetType;
		uint8 packetID;
		uint8 packetOffset;
		uint8 bFinalPacket:1;
		uint8 bToEveryone:1;
	};

	struct KillQueue 
	{
		KillQueue(IActor *pShooter, EntityId victim, float deathTime, bool bKillHit, int fpPacketOffset, int tpPacketOffset, bool bToEveryone, float timeLeft)
		{
			m_pShooter=pShooter;
			m_victim=victim;
			m_startSendTime=deathTime;
			m_bSendKillHit=bKillHit;
			m_fpPacketOffset=fpPacketOffset;
			m_tpPacketOffset=tpPacketOffset;
			m_bToEveryone=bToEveryone;
			m_timeLeft=timeLeft;
		}
		IActor*	 m_pShooter;
		EntityId m_victim;
		float    m_startSendTime;
		float    m_timeLeft;
		int			 m_fpPacketOffset;
		int			 m_tpPacketOffset;
		bool		 m_bSendKillHit;
		bool		 m_bToEveryone;
	};

	typedef std::deque<SSentKillCamPacket> SentPacketQueue;

	std::deque<KillQueue> m_kills;
	SSendingState m_inflightPacket;
	SentPacketQueue m_sentPackets;
	CCircularBuffer<KILLCAM_SEND_BUFFER_SIZE> m_buffer;
	int m_packetId;
};

#endif // __RECORDINGSYSTEMCLIENTSENDER_H__
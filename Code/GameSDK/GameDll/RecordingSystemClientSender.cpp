// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RecordingSystem.h"
#include "RecordingSystemPackets.h"
#include "Game.h"
#include "GameCVars.h"
#include "Actor.h"
#include "RecordingSystemCircularBuffer.h"
#include "RecordingSystemClientSender.h"

#define DISABLE_FORWARDED_PACKETS

CClientKillCamSender::CClientKillCamSender()
{
	Reset();
};

void CClientKillCamSender::Reset()
{
	m_buffer.Clear();
	m_kills.clear();
	m_sentPackets.clear();
	m_inflightPacket.dataOffset=m_inflightPacket.dataSize=0;
	m_packetId=1;
}

void CClientKillCamSender::AddKill(IActor *pShooter, EntityId victim, bool bulletTimeKill, bool bToEveryone)
{
	float now = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	float length = g_pGameCVars->kc_length;
	if (bulletTimeKill)
	{
		length = g_pGameCVars->kc_skillKillLength;
	}
	// Initially send through all the recorded data before the kill in one chunk.
	int fpPacketOffset=0, tpPacketOffset=0;
	const float duration = (float)__fsel(length-g_pGameCVars->kc_kickInTime,length-g_pGameCVars->kc_kickInTime,0.0f);
	AddKillDataToSendQueue(pShooter, victim, now-duration, now, bulletTimeKill, fpPacketOffset, tpPacketOffset, false, bToEveryone);

	// The rest of the data will be streamed through in chunks as it is recorded.
	m_kills.push_back(KillQueue(pShooter, victim, now, bulletTimeKill, fpPacketOffset, tpPacketOffset, bToEveryone, length-duration));
}

void CClientKillCamSender::Update()
{
	while (!m_kills.empty())
	{
		float now = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		KillQueue &first=m_kills.front();
		const float to = first.m_startSendTime + min( g_pGameCVars->kc_chunkStreamTime, first.m_timeLeft );
		if (now>to)
		{
			first.m_timeLeft -= g_pGameCVars->kc_chunkStreamTime;
			const bool bFinal = first.m_timeLeft <= 0.f;
			AddKillDataToSendQueue( first.m_pShooter, first.m_victim, first.m_startSendTime, to, first.m_bSendKillHit, first.m_fpPacketOffset, first.m_tpPacketOffset, bFinal, first.m_bToEveryone );
			if( bFinal )
			{
				m_kills.pop_front();
			}
			else
			{
				first.m_startSendTime = to;
			}
		}
		else
		{
			break;
		}
	}
	SendData();
}

void CClientKillCamSender::SendData()
{
	if (!m_buffer.Empty())
	{
		const int k_packetDataSize=CActor::KillCamFPData::DATASIZE;
		if (m_inflightPacket.dataOffset>=m_inflightPacket.dataSize)
		{
			m_inflightPacket=*(SSendingState*)m_buffer.GetData(sizeof(SSendingState));
			if (m_inflightPacket.packetType>KCP_MAX)
			{
				CryFatalError("Trying to send an invalid packet");
			}
			if (m_inflightPacket.dataOffset!=0)
			{
				CryFatalError("Picked up a partially sent packet. This should be impossible.");
			}
		}
		size_t size=m_inflightPacket.dataSize-m_inflightPacket.dataOffset;
		bool bFinalPacket=false;
		if (size>k_packetDataSize)
		{
			size=k_packetDataSize;
		}
		else if (m_inflightPacket.bFinalPacket)
		{
			bFinalPacket=true;
		}
		void *pData=m_buffer.GetData(size);
		CActor::KillCamFPData packet(m_inflightPacket.packetType, m_inflightPacket.packetID, (m_inflightPacket.dataOffset/k_packetDataSize)+m_inflightPacket.packetOffset, m_inflightPacket.victim, size, pData, bFinalPacket, m_inflightPacket.bToEveryone);
		m_inflightPacket.pShooter->GetGameObject()->InvokeRMI(CActor::SvKillFPCamData(), packet, eRMI_ToServer);
		if (packet.m_bToEveryone) // Send to self
		{
			CRecordingSystem *pRecordingSystem = g_pGame->GetRecordingSystem();
			pRecordingSystem->ClProcessKillCamData(m_inflightPacket.pShooter, packet);
		}
		m_inflightPacket.dataOffset+= static_cast<uint16> (size);
	}
}

void CClientKillCamSender::AddKillDataToSendQueue(IActor *pShooter, EntityId victim, float from, float to, bool bulletTimeKill, int &fpPacketOffset, int &tpPacketOffset, bool bFinal, bool bToEveryone)
{
	float timeOffset=0;
	fpPacketOffset=AddFirstPersonDataToSendQueue(pShooter, victim, from, to, fpPacketOffset, bFinal, bToEveryone, timeOffset);
	tpPacketOffset=AddVictimDataToSendQueue(pShooter, victim, from+timeOffset, to+timeOffset, bulletTimeKill, tpPacketOffset, bFinal, bToEveryone, timeOffset);
}

int CClientKillCamSender::AddFirstPersonDataToSendQueue(IActor *pShooter, EntityId victim, float from, float to, int packetOffset, bool bFinal, bool bToEveryone, float& timeoffset)
{
#ifndef DISABLE_FORWARDED_PACKETS
	if (!bToEveryone) // Forwarding packets doesn't support it and should never happen
	{
		// Clear out old packets
		for (SentPacketQueue::iterator it=m_sentPackets.begin(), end=m_sentPackets.end(); it!=end;)
		{
			SentPacketQueue::iterator next=it+1;
			if (it->from<from-(max(g_pGameCVars->kc_length, g_pGameCVars->kc_skillKillLength)+g_pGameCVars->kc_resendThreshold))
			{
				m_sentPackets.erase(it);
			}
			it=next;
		}
		// Check if we can resend an old packet
		for (SentPacketQueue::iterator it=m_sentPackets.begin(), end=m_sentPackets.end(); it!=end; ++it)
		{
			SSentKillCamPacket &p=*it;
			if (fabsf(p.from-from)<g_pGameCVars->kc_resendThreshold && fabsf(p.to-to)<g_pGameCVars->kc_resendThreshold)
			{
				timeoffset=p.from-from;
				AddDataToSendQueue(pShooter, KCP_FORWARD, p.id, victim, NULL, 0, p.numPackets, bFinal, false);
				return p.numPackets+packetOffset;
			}
		}
	}
#endif
	// Send new packet
	uint8 *pData;
	CRecordingSystem *crs = g_pGame->GetRecordingSystem();
	size_t datasize=crs->GetFirstPersonDataForTimeRange(&pData, from, to);
	const int k_packetDataSize=CActor::KillCamFPData::DATASIZE;
	int numPackets=(datasize+k_packetDataSize-1)/k_packetDataSize;
	if (datasize)
	{
		SSentKillCamPacket newPacket;
		newPacket.id=m_packetId++;
		if(m_packetId==16)
			m_packetId=1;
		newPacket.from=from;
		newPacket.to=to;
		newPacket.numPackets=numPackets;
		AddDataToSendQueue(pShooter, KCP_NEWFIRSTPERSON, newPacket.id, victim, pData, datasize, packetOffset, bFinal, bToEveryone);
		m_sentPackets.push_back(newPacket);
	}
	return numPackets+packetOffset;
}

size_t CClientKillCamSender::AddVictimDataToSendQueue(IActor *pShooter, EntityId victim, float from, float to, bool bulletTimeKill, int packetOffset, bool bFinal, bool bToEveryone, float timeOffset)
{
	// Will never overlap so always send in full.
	CRecordingSystem *crs = g_pGame->GetRecordingSystem();
	INetContext* pNetContext = gEnv->pGameFramework->GetNetContext();
	SNetObjectID netId;
	uint8 *pData;
	size_t datasize=crs->GetVictimDataForTimeRange(&pData, victim, from, to, bulletTimeKill, (bFinal && timeOffset!=0.0f)?timeOffset:0.0f);
	const int k_packetDataSize=CActor::KillCamFPData::DATASIZE;
	int numPackets=(datasize+k_packetDataSize-1)/k_packetDataSize;
	AddDataToSendQueue(pShooter, KCP_NEWTHIRDPERSON, 0, victim, pData, datasize, packetOffset, bFinal, bToEveryone);
	return numPackets+packetOffset;
}

void CClientKillCamSender::AddDataToSendQueue(IActor *pShooter, uint8 packetType, int packetId, EntityId victim, void *data, size_t datasize, int packetOffset, bool bFinal, bool bToEveryone)
{
	SSendingState header;
	header.packetID= static_cast<uint8>(packetId);
	header.packetType=packetType;
	header.dataSize= static_cast<uint16>(datasize);
	header.dataOffset=0;
	header.victim=victim;
	header.pShooter=pShooter;
	header.packetOffset=packetOffset;
	header.bFinalPacket=bFinal;
	header.bToEveryone=bToEveryone;
	while (!m_buffer.AddData(&header, sizeof(header)))
	{
		CryLog("KillcamSender: Flushing queue\n");
		SendData(); // Flush queue
	}
	while (datasize)
	{
		const int k_packetDataSize=CActor::KillCamFPData::DATASIZE;
		size_t toSend=min((size_t)k_packetDataSize, datasize);
		while (!m_buffer.AddData(data, toSend))
		{
			CryLog("KillcamSender: Flushing queue\n");
			SendData(); // Flush queue
		}
		data=(char*)data+toSend;
		datasize-=toSend;
	}
}

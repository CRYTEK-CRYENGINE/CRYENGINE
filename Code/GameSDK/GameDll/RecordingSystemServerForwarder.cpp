// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RecordingSystem.h"
#include "RecordingSystemPackets.h"
#include "Game.h"
#include "GameCVars.h"
#include "Actor.h"
#include "RecordingSystemCircularBuffer.h"
#include "RecordingSystemClientSender.h"
#include "RecordingSystemServerForwarder.h"

void CServerKillCamForwarder::Reset()
{
	m_forwarding.clear();
	m_history.Clear();
}

void CServerKillCamForwarder::ReceivePacket(IActor *pActor, const CActor::KillCamFPData &packet)
{
	if (packet.m_packetType==CClientKillCamSender::KCP_FORWARD)
	{
		// Add on to the forwarding queue
		SForwardingPacket forward;
		forward.packetID=packet.m_packetId;
		forward.victim=packet.m_victim;
		forward.pActor=pActor;
		forward.m_numPackets=packet.m_numPacket;
		forward.m_lastPacketTime=gEnv->pTimer->GetCurrTime();
		GetFirstPacket(forward.iterator, pActor, packet.m_packetId);
		forward.iterator=m_history.Begin();
		m_forwarding.push_back(forward);
	}
	else
	{
		// Just forward on the to the recipient
		if (packet.m_bToEveryone)
		{
			pActor->GetGameObject()->InvokeRMI(CActor::ClKillFPCamData(), packet, eRMI_ToOtherClients, pActor->GetChannelId());
		}
		else
		{
			//The victim player may no longer be present
			if(IActor *pVictim=g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(packet.m_victim))
				pActor->GetGameObject()->InvokeRMI(CActor::ClKillFPCamData(), packet, eRMI_ToClientChannel, pVictim->GetChannelId());
		}

		if (packet.m_packetType==CClientKillCamSender::KCP_NEWFIRSTPERSON && (!packet.m_bToEveryone || packet.m_bFinalPacket))
		{
			// Store packet into buffer in case we get a forwarding request for it
			// Final packet with the bToEveryone flag gets saved to facilitate detecting packet ID overflow
			AddDataAndErase(&pActor, sizeof(pActor));
			AddDataAndErase(&packet, sizeof(packet));
		}
	}
}

void CServerKillCamForwarder::Update()
{
	CTimeValue now=gEnv->pTimer->GetCurrTime();
	const float k_timeout=2.0f;
	if (!m_forwarding.empty())
	{
		int maxCycles=m_forwarding.size();
		bool bSent=false;
		do 
		{
			SForwardingPacket &forward=m_forwarding.front();
			CActor::KillCamFPData* packet=FindPacket(forward.iterator, forward.pActor, forward.packetID);
			if (packet)
			{
				ForwardPacket(forward, packet);
				bSent=true;
				if (forward.m_sent>=forward.m_numPackets)
					m_forwarding.pop_front();
			}
			else
			{
				if (forward.m_sent!=forward.m_numPackets)
				{
					CryLog("Underflowed forwarding packetID %d from actor %s after sending %d packets\n", forward.packetID, forward.pActor->GetEntity()->GetName(), forward.m_sent);
					if (forward.m_lastPacketTime.GetDifferenceInSeconds(now)>=k_timeout)
					{
						CryLog("Underflowing packet timed out, did the client disconnect? Giving up :(\n");
					}
					else
					{
						// Re-add to the end of the list for trying later (forward packet can arrive before all source packets have arrived)
						m_forwarding.push_back(forward);
					}
				}
				m_forwarding.pop_front();
				maxCycles--;
			}
		} while (!bSent && maxCycles>0);
	}
}

void CServerKillCamForwarder::ForwardPacket(SForwardingPacket &forward, CActor::KillCamFPData *pPacket)
{
	IActor *pVictim=g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(forward.victim);
	forward.pActor->GetGameObject()->InvokeRMI(CActor::ClKillFPCamData(), *pPacket, eRMI_ToClientChannel, pVictim->GetChannelId());
	forward.m_sent++;
	forward.m_lastPacketTime=gEnv->pTimer->GetCurrTime();
}

void CServerKillCamForwarder::DropOldestPacket()
{
	if (m_history.Empty())
	{
		CryFatalError("We're trying to drop a packet to make space for new data but there's nothing left in the buffer\n");
	}
	BufferType::iterator itPacket=m_history.Begin();
	IActor *pActor=*(IActor**)m_history.GetData(sizeof(IActor*));
	CActor::KillCamFPData *pPacket=(CActor::KillCamFPData*)m_history.GetData(sizeof(CActor::KillCamFPData));
	// If packet in queue to be forwarded, send immediately before dropping
	for (std::deque<SForwardingPacket>::iterator it=m_forwarding.begin(), end=m_forwarding.end(); it!=end; ++it)
	{
		SForwardingPacket &forward=*it;
		if (forward.iterator<=itPacket && forward.pActor==pActor && forward.packetID==pPacket->m_packetId)
		{
			CryLog("Server forwarder queue is overflowing! Forcing packet send to free space\n");
			ForwardPacket(forward, pPacket);
		}
	}
}

void CServerKillCamForwarder::AddDataAndErase(const void *data, size_t size)
{
	while (!m_history.AddData(data, size))
	{
		DropOldestPacket();
	}
}

CActor::KillCamFPData* CServerKillCamForwarder::FindPacket(BufferType::iterator &it, IActor *pActor, int packetId)
{
	// Find next packet that matches sending actor/packetId
	while (it.HasData())
	{
		IActor** pPacketActor=(IActor**)it.GetData(sizeof(IActor*));
		CActor::KillCamFPData *packet=(CActor::KillCamFPData*)it.GetData(sizeof(CActor::KillCamFPData));
		if (pActor==*pPacketActor && packet->m_packetId==packetId)
		{
			return packet;
		}
	}
	return NULL;
}

void CServerKillCamForwarder::GetFirstPacket(BufferType::iterator &first, IActor *pActor, int packetId)
{
	// Find first packet for sender actor/packet id taking care to ignore packet ID overflow
	int numPackets=0, foundPackets=0;
	bool bSetFirst=false;
	BufferType::iterator it;
	int middlePacketId=(packetId+(CActor::KillCamFPData::UNIQPACKETIDS/2))%CActor::KillCamFPData::UNIQPACKETIDS;
	first=m_history.Begin();
	it=first;
	while (it.HasData())
	{
		IActor** pPacketActor=(IActor**)it.GetData(sizeof(IActor*));
		CActor::KillCamFPData *packet=(CActor::KillCamFPData*)it.GetData(sizeof(CActor::KillCamFPData));
		if (pActor==*pPacketActor)
		{
			if (packet->m_packetId==middlePacketId) // We've almost certainly overflowed the buffer
			{
				bSetFirst=false;
			}
			else if (packet->m_packetId==packetId)
			{
				if (!bSetFirst)
				{
					first=it;
					bSetFirst=true;
				}
			}
		}
	}
	if (!bSetFirst)
	{
		first=it;
	}
}
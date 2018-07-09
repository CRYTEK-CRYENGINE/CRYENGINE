// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RecordingSystem.h"
#include "RecordingSystemPackets.h"
#include "RecordingSystemCircularBuffer.h"
#include "RecordingSystemClientSender.h"
#include "RecordingSystemServerForwarder.h"
#include "Game.h"
#include "GameCVars.h"
#include "Actor.h"
#include "Utility/CryWatch.h"

void CRecordingSystem::SvProcessKillCamData(IActor *pActor, const CActor::KillCamFPData &packet)
{
	m_forwarder.ReceivePacket(pActor, packet);
}

void CRecordingSystem::ClProcessKillCamData(IActor *pActor, const CActor::KillCamFPData &packet)
{
	// Receiver gets a second to validate the expectations.
	static float timeToValidateUnexpectedData = 1.f;

	// Find the existing StreamData for this packet.
	SKillCamStreamData* pStreamData = m_streamer.GetExpectedStreamData(packet.m_victim);

	// If it arrived before the kill was relayed to the client, then create false expectations of this data and store it in anticipation of validation.
	if(!pStreamData)
	{
		CryLogAlways("Receiving unexpected packet data. This is most likely because the data is arriving before the kill has been requested. Creating false expectations.");
		SKillCamExpectedData falseExpectations(pActor->GetEntityId(), packet.m_victim, packet.m_bToEveryone);
		pStreamData = m_streamer.ExpectStreamData(falseExpectations, false);
		if(!pStreamData)
		{
			CryFatalError("CRecordingSystem::ClProcessKillCamData: Cannot find free slot to add new receive data from: Sender[%d] Victim[%d]", falseExpectations.m_sender, falseExpectations.m_victim);
		}
		PREFAST_ASSUME(pStreamData); //validated above
		pStreamData->SetValidationTimer(timeToValidateUnexpectedData);
	}
	else
	{
		// Update the timer if it's still not been validated and we receive a new packet...
		if(pStreamData->IsValidationTimerSet())
		{
			pStreamData->SetValidationTimer(timeToValidateUnexpectedData);
		}
	}

	if(pStreamData)
	{
		// Process the Packet.
		CRecordingSystem::ProcessKillCamData(pActor, packet, *pStreamData, true);
	}
}

/*static*/ bool CRecordingSystem::ProcessKillCamData(IActor *pActor, const CActor::KillCamFPData &packet, SKillCamStreamData& rStreamData, const bool bFatalOnForwardedPackets)
{
	//TODO: Check if we need some handling as these aren't cleanly divisible
	const int packetSize=CActor::KillCamFPData::DATASIZE;
	const int receiveBufferPackets=sizeof(rStreamData.m_buffer)/packetSize;

	EntityId actorId=pActor->GetEntityId();

	CRY_ASSERT_MESSAGE(actorId==rStreamData.m_expect.m_sender, "Wrong Sender!");
	CRY_ASSERT_MESSAGE(packet.m_victim==rStreamData.m_expect.m_victim, "Wrong Victim!");
	CRY_ASSERT_MESSAGE(packet.m_bToEveryone==rStreamData.m_expect.m_winningKill, "Wrong Winning Kill!");

	int packetNum=packet.m_numPacket;
	if (packet.m_packetType==CClientKillCamSender::KCP_NEWTHIRDPERSON)
	{
		// Grow third person data from the top
		packetNum=receiveBufferPackets-1-packetNum;
		if (packet.m_bFinalPacket)
			rStreamData.m_packetTargetNum[1]=packet.m_numPacket+1;
		rStreamData.m_packetReceived[1]++;
	}
	else if (packet.m_packetType==CClientKillCamSender::KCP_NEWFIRSTPERSON)
	{
		if (packet.m_bFinalPacket)
			rStreamData.m_packetTargetNum[0]=packet.m_numPacket+1;
		rStreamData.m_packetReceived[0]++;
	}
	else if(packet.m_packetType == CClientKillCamSender::KCP_FORWARD)
	{
#ifndef _RELEASE
		if(bFatalOnForwardedPackets)
			CryFatalError("Received a Forwarded packet! We aren't using these at the moment!");
#endif //_RELEASE
		return false;
	}
	else
	{
#ifndef _RELEASE
		CryFatalError("Received a client packet that we can't deal with\n");
#endif //_RELEASE
		return false;
	}

	memcpy(&rStreamData.m_buffer[packetNum*packetSize], packet.m_data, packet.m_size);

	if(	rStreamData.m_packetReceived[0] == rStreamData.m_packetTargetNum[0] && 
			rStreamData.m_packetReceived[1] == rStreamData.m_packetTargetNum[1]) // Fully received a stream
	{
		int totalPackets = rStreamData.m_packetTargetNum[0] + rStreamData.m_packetTargetNum[1];
		assert(totalPackets<=receiveBufferPackets);

		// Reconstruct third person buffer in correct order
		for (int i=0; i<rStreamData.m_packetTargetNum[1]; i++)
		{
			int srcIdx=receiveBufferPackets-1-i;
			int dstIdx=rStreamData.m_packetTargetNum[0]+i;
			memcpy(&rStreamData.m_buffer[dstIdx*packetSize], &rStreamData.m_buffer[srcIdx*packetSize], packetSize);
		}

		rStreamData.m_finalSize = totalPackets*packetSize;

		return true;
	}
	return false;
}

/*static*/ void CRecordingSystem::ExtractCompleteStreamData( SKillCamStreamData& streamData, SPlaybackInstanceData& rPlaybackInstanceData, SFirstPersonDataContainer& rFirstPersonDataContainer )
{
	if(const size_t finalSize = streamData.GetFinalSize())
	{
		SetFirstPersonData(streamData.m_buffer, finalSize, rPlaybackInstanceData, rFirstPersonDataContainer);
		streamData.Reset();
	}
#ifndef _RELEASE
	else
	{
		CryFatalError("Can't extract incomplete Stream Data.");
	}
#endif //_RELEASE
}


void CRecordingSystem::AddKillCamData(IActor *pActor, EntityId victim, bool bulletTimeKill, bool bSendToAll)
{
	m_sender.AddKill(pActor, victim, bulletTimeKill, bSendToAll);
}

void CRecordingSystem::UpdateSendQueues()
{
	m_sender.Update();
	m_forwarder.Update();
}

//////////////////////////////////////////////////////////////////////////

void CKillCamDataStreamer::Update( const float dt )
{
	for(int i=0; i<kMaxSimultaneousStreams; i++)
	{
		if(!m_streams[i].UpdateValidationTimer(dt))
		{
			m_streams[i].Reset();
		}
	}
}

void CKillCamDataStreamer::ClearAllStreamData()
{
	for(int i=0; i<kMaxSimultaneousStreams; i++)
	{
		m_streams[i].Reset();
	}
}

SKillCamStreamData* CKillCamDataStreamer::ExpectStreamData( const SKillCamExpectedData& expect, const bool canUseExistingMatchedData )
{
	SKillCamStreamData* pTargetStreamData = NULL;
	for(int i=0; i<kMaxSimultaneousStreams; i++)
	{
		if(!pTargetStreamData && m_streams[i].IsUnused())
		{
			pTargetStreamData = &m_streams[i];
		}
		else if(canUseExistingMatchedData && m_streams[i].m_expect==expect)
		{
			pTargetStreamData = &m_streams[i];
			// If the data was received before we expected it then the validation timer would have been set, so we validate it now.
			pTargetStreamData->Validate();
			break;
		}
	}
	if(pTargetStreamData)
	{
		pTargetStreamData->m_expect = expect;
	}
	return pTargetStreamData;
}

void CKillCamDataStreamer::ClearStreamData( const EntityId victim )
{
	for(int i=0; i<kMaxSimultaneousStreams; i++)
	{
		if(m_streams[i].m_expect.m_victim==victim)
		{
			m_streams[i].Reset();
		}
	}
}

SKillCamStreamData* CKillCamDataStreamer::GetExpectedStreamData( const EntityId victim )
{
	for(int i=0; i<kMaxSimultaneousStreams; i++)
	{
		if(m_streams[i].m_expect.m_victim==victim)
		{
			return &m_streams[i];
		}
	}
	return NULL;
}

#ifndef _RELEASE
void CKillCamDataStreamer::DebugStreamData() const
{
	CryWatch("Expected Stream Data:");
	for(int i=0; i<kMaxSimultaneousStreams; i++)
	{
		if(m_streams[i].IsUnused())
		{
			CryWatch("  Stream Slot [%d]: Empty", i);
		}
		else
		{
			const SKillCamExpectedData& exp = m_streams[i].m_expect;
			IEntity* pSender = gEnv->pEntitySystem->GetEntity(exp.m_sender);
			IEntity* pVictim = gEnv->pEntitySystem->GetEntity(exp.m_victim);
			CryWatch("  Stream Slot [%d]: Expecting Data. Sender[%s:%d] Victim[%s:%d] WinningKill[%s]", i, pSender?pSender->GetName():"", exp.m_sender, pVictim?pVictim->GetName():"", exp.m_victim, exp.m_winningKill?"True":"False");
		}
	}
}
#endif //_RELEASE

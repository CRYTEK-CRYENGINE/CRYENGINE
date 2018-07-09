// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PerformBreakage.h"
#include "ServerContextView.h"
#include "NetCVars.h"

/*
   ================================================================================================================
   Perform Simple Breakage - Base
   ================================================================================================================

   This set of classes is responsible for sending the RMI declaring a break and
   synchronising the net ids. On the server, each declared product gets allocated
   a net id. These get sent over to the client. When the break is recreated
   this class is called to finish the bind for each spawned entity.
   The order and index of each spawn is the repsonsibility of the game code,
   but is ensured by the break products being deterministic.

 */

CPerformBreakSimpleBase::CPerformBreakSimpleBase()
{
	m_numEntities = 0;
}

CPerformBreakSimpleBase::~CPerformBreakSimpleBase()
{
}

void CPerformBreakSimpleBase::SerialiseNetIds(TSerialize ser)
{
	// Serialise the net-ids for the entities that need to be collected from the breakage
	ser.Value("numEnts", m_numEntities);
	assert(m_numEntities <= MaxEntitiesPerBreak);
	for (int i = 0; i < m_numEntities; i++)
	{
		ser.Value("id", m_entityNetIds[i], 'eid');
		CryLogAlways("brk: # serialise netId: %s", m_entityNetIds[i].GetText());
	}
}

/*
   ================================================================================================================
   Perform Simple Breakage - Server
   ================================================================================================================
 */

/* static */
CPerformBreakSimpleServer* CPerformBreakSimpleServer::GotBreakage(CServerContextView* pView, const SNetIntBreakDescription* pDesc)
{
	// Lock state changes, until this message is acknowledged
	pView->LockStateChanges("BREAK");

	// Queue the breakage message, and make it dependent on the main entity being enabled
	// The dependency is needed for client jopins
	CPerformBreakSimpleServer* pSendable = new CPerformBreakSimpleServer;

	SSendableHandle hdl;
	SSendableHandle waitHandle;
	SNetObjectID dependentNetId = SNetObjectID();
	if (pDesc->breakageEntity && (dependentNetId = pView->ContextState()->GetNetID(pDesc->breakageEntity)))
	{
		pView->Parent()->NetAddSendable(new CWaitForEnabled(dependentNetId, pView), 0, 0, &waitHandle);
		pView->Parent()->NetAddSendable(pSendable, 1, &waitHandle, &hdl);
	}
	else
	{
		pView->Parent()->NetAddSendable(pSendable, 0, NULL, &hdl);
	}

	pSendable->Init(pView, pDesc, hdl, dependentNetId);

	return pSendable;
}

CPerformBreakSimpleServer::CPerformBreakSimpleServer()
	: INetSendable(eMPF_BlocksStateChange, eNRT_ReliableUnordered)
{
}

void CPerformBreakSimpleServer::Init(CServerContextView* pView, const SNetIntBreakDescription* pDesc, SSendableHandle msgHandle, SNetObjectID dependentNetId)
{
	CNetContextState* pContextState = pView->ContextState();
	const int n = pDesc->spawnedObjects.size();
	for (int i = 0; i < n; i++)
	{
		EntityId ent = pDesc->spawnedObjects[i];
		SNetObjectID id = pContextState->GetNetID(ent);
		SContextObjectRef obj = pContextState->GetContextObject(id);
		if (id && obj.main && obj.main->spawnType == eST_Collected)
		{
			// There seems to be no obvious reason for doing this! AllocateObject does it!
			// But the old code did it through a CNetObjectBindLock class
			pContextState->BoundObject(id, "PBRK");

			// Lock the netids so they can't be unbound until this message has been acknowledged
			pView->SendablesDependentOnObjectAdd(id, msgHandle);

			m_entityNetIds[i] = id;
		}
		else
		{
			NetLog("brk: Warning have null netId to send, this shouldn't happen");
			m_entityNetIds[i] = SNetObjectID();
		}
	}

	// Lock main netid so they can't be unbound until this message has been acknowledged
	if (dependentNetId)
		pView->SendablesDependentOnObjectAdd(dependentNetId, msgHandle);

	m_numEntities = n;
	m_pView = pView;
	m_pContextState = pContextState;
	m_pDesc = pDesc;
	m_msgHandle = msgHandle;
	m_dependentNetId = dependentNetId;
}

EMessageSendResult CPerformBreakSimpleServer::Send(INetSender* pSender)
{
	pSender->BeginMessage(CClientContextView::PerformSimpleBreak);
	SerialiseNetIds(pSender->ser);
	m_pDesc->pMessagePayload->SerialiseSimpleBreakage(pSender->ser);
	return eMSR_SentOk;
}

#if ENABLE_PACKET_PREDICTION
SMessageTag CPerformBreakSimpleServer::GetMessageTag(INetSender* pSender, IMessageMapper* mapper)
{
	SMessageTag mTag;

	mTag.messageId = mapper->GetMsgId(CClientContextView::PerformBreak);
	mTag.varying1 = 0;
	mTag.varying2 = 0;

	return mTag;
}
#endif

void CPerformBreakSimpleServer::UpdateState(uint32 nFromSeq, ENetSendableStateUpdate state)
{
	CNetContextState* pContextState = m_pContextState;
	if (!pContextState)
	{
		// This shouldn't happen
		NetLog("brk: Warning pContextState is NULL, bailing out!");
		return;
	}

	if (state == eNSSU_Ack)
	{
		assert(m_numEntities <= MaxEntitiesPerBreak);
		for (int i = 0; i < m_numEntities; i++)
		{
			if (pContextState->GetContextObject(m_entityNetIds[i]).main->userID)
			{
				m_pView->SetSpawnState(m_entityNetIds[i], eSS_Spawned);
			}
			else
			{
				CNetObjectBindLock lock(pContextState, m_entityNetIds[i], "BRK");
				m_pView->SendUnbindMessage(m_entityNetIds[i], false, lock);
			}
		}
	}

	if (state != eNSSU_Requeue)
	{
		// Once the RMI has been acknowledged, remove the state change lock
		m_pView->UnlockStateChanges("BREAK");

		// Remove send dependables so that unbind messages can proceed
		if (m_dependentNetId)
			m_pView->SendablesDependentOnObjectRemove(m_dependentNetId, m_msgHandle);

		assert(m_numEntities <= MaxEntitiesPerBreak);
		for (int i = 0; i < m_numEntities; i++)
		{
			if (m_entityNetIds[i])
			{
				m_pView->SendablesDependentOnObjectRemove(m_entityNetIds[i], m_msgHandle);

				// There seems to be no obvious reason for doing this!
				// But the old code did it through a CNetObjectBindLock class
				pContextState->UnboundObject(m_entityNetIds[i], "PBRK");
			}
		}
	}
}

/*
   ================================================================================================================
   Perform Simple Breakage - Client
   ================================================================================================================
 */

CPerformBreakSimpleClient::CPerformBreakSimpleClient(CClientContextView* pClientView)
{
	++g_objcnt.breakageSimplePlayback;
	m_numEntitiesCollected = 0;
	m_state = k_invalid;
	m_pClientView = pClientView;
}

CPerformBreakSimpleClient::~CPerformBreakSimpleClient()
{
	SCOPED_GLOBAL_LOCK;
	--g_objcnt.breakageSimplePlayback;

	CNetContextState* pContextState = m_pClientView->ContextState();
	if (pContextState && m_state == k_stopped)
	{
		// Only do this if we still have a contextstate - otherwise the game is probably being teared down
		if (m_numEntitiesCollected != m_numEntities)
		{
			// We haven't net-bound all the collected entities.
			// This means that the client spawned the wrong number of entities
			// Inform the server which net ids havent't been bound
			assert(m_numEntities <= MaxEntitiesPerBreak);
			for (int i = 0; i < m_numEntities; i++)
			{
				if (m_entityNetIds[i])
				{
					NetWarning("[brk] uncollected item idx=%d netid=%s", i, m_entityNetIds[i].GetText());
					if (m_pClientView->Parent())
						m_pClientView->Parent()->NetAddSendable(new CSimpleNetMessage<SSimpleObjectIDParams>(SSimpleObjectIDParams(m_entityNetIds[i]), CServerContextView::SkippedCollectedObject), 0, 0, 0);
				}
			}
		}
	}
}

void CPerformBreakSimpleClient::SerialiseNetIds(TSerialize ser)
{
	CPerformBreakSimpleBase::SerialiseNetIds(ser);

	if (CNetContextState* pContextState = m_pClientView->ContextState())
	{
		assert(m_numEntities <= MaxEntitiesPerBreak);
		for (int i = 0; i < m_numEntities; i++)
		{
			if (!pContextState->AllocateObject(0, m_entityNetIds[i], 8, false, eST_Collected, NULL))
			{
				m_pClientView->Parent()->Disconnect(eDC_ContextCorruption, "Failed allocating object for broken product");
				return;
			}
		}
	}
	else
	{
		m_pClientView->Parent()->Disconnect(eDC_ContextCorruption, "Failed allocating object for broken product - ContextState == NULL");
	}
}

void CPerformBreakSimpleClient::BeginBreakage()
{
	m_state = k_started;
}

void CPerformBreakSimpleClient::FinishedBreakage()
{
	m_state = k_stopped;
}

void CPerformBreakSimpleClient::BindSpawnedEntity(EntityId id, int spawnIdx)
{
	// The client created an entity and is binding to a netid
	SCOPED_GLOBAL_LOCK;

	if (spawnIdx == -1)
		spawnIdx = m_numEntitiesCollected;

	assert(m_state == k_started);
	assert(m_numEntities <= MaxEntitiesPerBreak);
	assert(spawnIdx < MaxEntitiesPerBreak);
	if ((unsigned)spawnIdx < (unsigned)m_numEntities)
	{
		NetLog("[brk] collected index %d as Entity: %x", spawnIdx, id);
		m_pClientView->BoundCollectedObject(m_entityNetIds[spawnIdx], id);
		m_entityNetIds[spawnIdx] = SNetObjectID();
		m_numEntitiesCollected++;
	}
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  Client implementation of a context view
   -------------------------------------------------------------------------
   History:
   - 26/07/2004   : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"
#include "ClientContextView.h"
#include "ServerContextView.h"
#include "NetContext.h"
#include <CrySystem/ITimer.h>
#include "Network.h"
#include "BreakagePlayback.h"
#include "PerformBreakage.h"
#include "History/History.h"
#include "VoiceContext.h"
#include "SyncedFileSet.h"

#if ENABLE_DEBUG_KIT
	#pragma warning(disable:4355)
	#include <CryEntitySystem/IEntitySystem.h>
	#include <CryPhysics/IPhysics.h>
#endif

CClientContextView::CClientContextView(CNetChannel* pNetChannel, CNetContext* pNetContext)
#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)
	: m_nVoicePacketHandle()
#endif
{
#if SERVER_FILE_SYNC_MODE
	m_fileSyncPhase = 0;
	m_fileSyncsComplete = 0;
#endif
#if ENABLE_DEBUG_KIT
	m_pNetVis.reset(new CNetVis(this));
#endif

	SetMMM(pNetChannel->GetChannelMMM());

	SContextViewConfiguration config = { 0 };
	config.pChangeStateMsg		= CServerContextView::ChangeState;
	config.pFinishStateMsg		= CServerContextView::FinishState;
	config.pUpdateMsg			= CServerContextView::BeginUpdateObject;
	config.pEndUpdateMsg		= CServerContextView::EndUpdateObject;
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	config.pVoiceDataMsg		= CServerContextView::VoiceData;
#endif
	config.pUpdatePhysicsTime	= CServerContextView::UpdatePhysicsTime;
	config.pPartialUpdate			= static_array<CServerContextView::msgPartialAspect, NumAspects>::value;
	config.pSetAspectProfileMsgs	= {{ 0 }};
	config.pUpdateAspectMsgs		= static_array<CServerContextView::msgUpdateAspect, NumAspects>::value;
	config.pRMIMsgs[eNRT_ReliableOrdered]			= CServerContextView::RMI_ReliableOrdered;
	config.pRMIMsgs[eNRT_ReliableUnordered]			= CServerContextView::RMI_ReliableUnordered;
	config.pRMIMsgs[eNRT_UnreliableOrdered]			= CServerContextView::RMI_UnreliableOrdered;
	config.pRMIMsgs[eNRT_UnreliableUnordered]		= NULL;	
	config.pRMIMsgs[eNRT_UnreliableUnordered + 1]	= CServerContextView::RMI_Attachment;

	Init(pNetChannel, pNetContext, &config);
}

CClientContextView::~CClientContextView()
{
}

void CClientContextView::DefineProtocol(IProtocolBuilder* pBuilder)
{
	DefineExtensionsProtocol(pBuilder);
	pBuilder->AddMessageSink(this,
	                         CServerContextView::GetProtocolDef(),
	                         CClientContextView::GetProtocolDef());
}

void CClientContextView::ChangeContext()
{
	CContextView::ChangeContext();
}

void CClientContextView::CompleteInitialization()
{
	CContextView::CompleteInitialization();
	SetName("Client_" + RESOLVER.ToString(Parent()->GetIP()));
}

void CClientContextView::OnObjectEvent(CNetContextState* pState, SNetObjectEvent* pEvent)
{
	if (pState == ContextState())
	{
		switch (pEvent->event)
		{
		case eNOE_SyncWithGame_End:
#if ENABLE_DEBUG_KIT
			m_pNetVis->Update();
#endif
			break;
		}
	}
	CContextView::OnObjectEvent(pState, pEvent);
}

void CClientContextView::OnWitnessDeclared()
{
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	if (CVoiceContext* pCtx = Context()->GetVoiceContextImpl())
	{
		pCtx->ConfigureCallback(this, eVD_To, GetWitness());
	}
#endif
}

void CClientContextView::SendEstablishedMessage()
{
	// we'd best tell the server
	class CEstablishedContextMessage : public INetMessage
	{
	public:
		CEstablishedContextMessage() : INetMessage(CServerContextView::ClientEstablishedContext)
		{
			++g_objcnt.establishedMsg;
		}
		~CEstablishedContextMessage()
		{
			--g_objcnt.establishedMsg;
		}
		EMessageSendResult WritePayload(TSerialize, uint32, uint32)
		{
			return eMSR_SentOk;
		}
		void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate update)
		{
		}
		size_t GetSize() { return sizeof(*this); }
	};
	Parent()->AddSendable(new CEstablishedContextMessage, 0, NULL, NULL);
}

void CClientContextView::EstablishedContext()
{
	//	if (ContextState())
	//		ContextState()->EstablishedContext(this);

	if (!IsLocal())
		SendEstablishedMessage();
}

void CClientContextView::BindObject(SNetObjectID nID)
{
	//const SContextObject * pObj = ContextState()->GetContextObject(nID);
	SContextObjectRef obj = ContextState()->GetContextObject(nID);
	if (obj.main->spawnType == eST_Collected && !obj.main->userID)
		; // we do the bind at break streaming time in this case
	else
	{
		CContextView::BindObject(nID);
		SetSpawnState(nID, eSS_Enabled);
	}
}

void CClientContextView::UnbindObject(SNetObjectID nID)
{
	CContextView::DoUnbindObject(nID, true);
}

const char* CClientContextView::ValidateMessage(const SNetMessageDef* pMsg, bool bNetworkMsg)
{
	if (!bNetworkMsg && pMsg->reliability == eNRT_UnreliableOrdered)
		return "Cannot send unreliable user messages from client until in game";
	return NULL;
}

bool CClientContextView::EnterState(EContextViewState state)
{
	switch (state)
	{
	case eCVS_Initial:
		{
			ICVar* pNick = gEnv->pConsole->GetCVar("cl_nickname");
			if (pNick && *pNick->GetString())
			{
				CServerContextView::SendSetNicknameWith(SSetNicknameParams(pNick->GetString()), Parent());
			}
		}
		break;
	case eCVS_Begin:
		if (!IsLocal())
		{
#ifdef __WITH_PB__
			Parent()->NetAddSendable(
			  new CSimpleNetMessage<SInitPunkBusterParams>(SInitPunkBusterParams(isPbClEnabled() != 0), CServerContextView::InitPunkBuster),
			  0, NULL, NULL);
#endif
			bool bShouldChangeContext = true;

			if (bShouldChangeContext)
			{
				Context()->ChangeContext();
			}
			TO_GAME(&CClientContextView::ContinueEnterState, this);
			LockStateChanges("ContinueEnterState");
			return true;
		}
		else
			ClearAllState();
		break;
	case eCVS_EstablishContext:
		break;
	case eCVS_ConfigureContext:
		break;
	case eCVS_SpawnEntities:
		break;
	case eCVS_PostSpawnEntities:
		break;
	case eCVS_InGame:
		break;
	}

	if (!CContextView::EnterState(state))
		return false;

	return true;
}

void CClientContextView::ContinueEnterState()
{
	if (!CContextView::EnterState(eCVS_Begin))
		Parent()->Disconnect(eDC_ProtocolError, "Couldn't enter begin state");
	UnlockStateChanges("ContinueEnterState");
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, ChangeState, eNRT_ReliableUnordered, eMPF_StateChange | eMPF_BlocksStateChange)
{
	return SetRemoteState(param.state);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, FinishState, eNRT_ReliableUnordered, eMPF_StateChange | eMPF_BlocksStateChange)
{
	if (FinishRemoteState())
	{
		FinishLocalState();
		return true;
	}
	return false;
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, ForceNextState, eNRT_ReliableOrdered, eMPF_BlocksStateChange)
{
	PushForcedState(param.state, true);
	return true;
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, AuthenticateChallenge, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	CWhirlpoolHash response = param.Hash(Password());
	CServerContextView::SendAuthenticateResponseWith(response, Parent());
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, BeginBindObject, eNRT_ReliableUnordered, eMPF_BlocksStateChange | eMPF_DecodeInSync)
{
	return DoBeginBind(ser, 0);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, BeginBindStaticObject, eNRT_ReliableUnordered, eMPF_BlocksStateChange | eMPF_DecodeInSync)
{
	return DoBeginBind(ser, eBBF_ReadObjectID | eBBF_FlagStatic);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, BeginBindPredictedObject, eNRT_ReliableUnordered, eMPF_BlocksStateChange | eMPF_DecodeInSync)
{
	return DoBeginBind(ser, eBBF_ReadObjectID);
}

bool CClientContextView::DoBeginBind(TSerialize ser, uint32 flags)
{
	if (IsBeforeState(eCVS_SpawnEntities))
	{
		NetWarning("Can't bind before getting to state SpawnEntities");
		return false;
	}

	// note here we check that the current object id DOESN'T exist (normally we'd check if it DOES exist)
	bool alreadyAllocated = ReadCurrentObjectID(ser, true);

	if (!IsLocal())
	{
		EntityId nUserID = 0;
		if (alreadyAllocated)
		{
			NetWarning("Attempt to bind an already allocated object");
			return false;
		}
		if (flags & eBBF_ReadObjectID)
		{
			ser.Value("userID", nUserID);
			ContextState()->SpawnedObject(nUserID);
		}
		else
		{
			nUserID = ContextState()->GetSpawnedObjectId(false);
		}
		Parent()->SetEntityId(nUserID);
		NetworkAspectType nAspectsEnabled;
		NetworkAspectType delegatableMask;
		ser.Value("aspects", nAspectsEnabled);
		ser.Value("delegatableMask", delegatableMask);

#if LOG_ENTITYID_ERRORS
		if (CNetCVars::Get().LogLevel)
			NetLog("BIND OBJECT: netID=%s entityID=%d aspects=%.2x", CurrentObjectID().GetText(), nUserID, nAspectsEnabled);
#endif

		if (ContextState()->AllocateObject(ContextState()->GetSpawnedObjectId(true), CurrentObjectID(), nAspectsEnabled, false, (flags & eBBF_FlagStatic) ? eST_Static : eST_Normal, this))
		{
			ContextState()->GC_BoundObject(ContextState()->GetContextObject(CurrentObjectID()).main->userID);
			ContextState()->SetDelegatableMask(CurrentObjectID(), delegatableMask);
		}
	}
	return true;
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, BeginUnbindObject, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	ContextState()->UnbindObject(param.netID, eUOF_CallGame);
	return true;
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, RemoveStaticObject, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	ContextState()->UnbindStaticObject(param.id);
	return true;
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, UnbindPredictedObject, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	ContextState()->UnbindStaticObject(param.id);
	return true;
}

/*
   NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, InvalidatePredictedSpawn, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
   {
   ContextState()->ForceObjectRemoval(param.netID);
   }
 */

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, BeginUpdateObject, eNRT_ReliableUnordered, eMPF_BlocksStateChange | eMPF_AfterSpawning)
{
#if ENABLE_DEBUG_KIT
	m_startUpdate = GetNetSerializeImplFromSerialize<CNetInputSerializeImpl>(ser)->GetInput().GetBitSize();
#endif
	if (!ReadCurrentObjectID(ser, false))
	{
		Parent()->Disconnect(eDC_ContextCorruption, "BeginUpdateObject");
		return false;
	}
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, EndUpdateObject, eNRT_UnreliableUnordered, 0)
{
#if ENABLE_DEBUG_KIT
	float sz = GetNetSerializeImplFromSerialize<CNetInputSerializeImpl>(ser)->GetInput().GetBitSize() - m_startUpdate;
	if (sz > 0)
		m_pNetVis->AddSample(CurrentObjectID(), 0, sz);
	m_pNetVis->AddSample(CurrentObjectID(), 1, 1);
#endif
	return ClearCurrentObjectID();
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, ReconfigureObject, eNRT_UnreliableUnordered, 0)
{
	bool ok = false;
	SReceiveContext ctx = CreateReceiveContext(ser, 7, nCurSeq, nOldSeq, timeFraction32, &ok);
	if (!ok)
	{
		Parent()->Disconnect(eDC_ContextCorruption, "Failed ReconfigureObject 1");
		return false;
	}
	if (!GetHistory(eH_Configuration)->ReadCurrentValue(ctx, !IgnoringCurrentObject()))
	{
		Parent()->Disconnect(eDC_ContextCorruption, "Failed ReconfigureObject 2");
		return false;
	}
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, RMI_UnreliableOrdered, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return HandleRMI(ser, eNRT_UnreliableUnordered, eMPF_BlocksStateChange);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, RMI_ReliableUnordered, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	return HandleRMI(ser, eNRT_ReliableUnordered, eMPF_BlocksStateChange);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, RMI_ReliableOrdered, eNRT_ReliableOrdered, eMPF_BlocksStateChange)
{
	return HandleRMI(ser, eNRT_ReliableOrdered, eMPF_BlocksStateChange);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, RMI_Attachment, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return HandleRMI(ser, eNRT_NumReliabilityTypes, true);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAuthority, eNRT_UnreliableUnordered, eMPF_BlocksStateChange | eMPF_DecodeInSync)
{
	bool ok = false;
	SReceiveContext ctx = CreateReceiveContext(ser, 7, nCurSeq, nOldSeq, timeFraction32, &ok);
	if (!ok)
		return false;
	return GetHistory(eH_Auth)->ReadCurrentValue(ctx, !IgnoringCurrentObject());
}

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, VoiceData, eNRT_UnreliableUnordered, 0)
{
	SNetObjectID id;
	TVoicePacketPtr pkt = CVoicePacket::Allocate();

	ser.Value("object", id, 'eid');

	pkt->Serialize(ser);

	ReceivedVoice(id);
	if (Context() && Context()->GetVoiceContextImpl())
		Context()->GetVoiceContextImpl()->OnClientVoicePacket(id, pkt);

	return true;
}
#endif

#if ENABLE_SESSION_IDS
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, InitSessionData, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	CSessionID sessionID;
	sessionID.SerializeWith(ser);
	Context()->SetSessionID(sessionID);
	return true;
}
#endif

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CClientContextView, BeginBreakStream, eNRT_ReliableUnordered, 0)
{
	if (param.brk < 0 || param.brk > MAX_BREAK_STREAMS)
	{
		NetWarning("Illegal break id %d", param.brk);
		return false;
	}
	if (m_pBreakOps[param.brk])
	{
		NetWarning("Trying to use a breakage stream for two breaks... illegal [breakid=%d]", param.brk);
		return false;
	}
	m_pBreakOps[param.brk] = new SNetClientBreakDescription();
	return true;
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CClientContextView, DeclareBrokenProduct, eNRT_ReliableUnordered, 0)
{
	if (param.brk < 0 || param.brk > MAX_BREAK_STREAMS)
	{
		NetWarning("Illegal break id %d", param.brk);
		Parent()->Disconnect(eDC_ContextCorruption, "Illegal break id");
		return false;
	}
	if (!m_pBreakOps[param.brk])
	{
		NetWarning("Trying to send break products with no break stream... illegal [breakid=%d, netid=%s]", param.brk, param.id.GetText());
		Parent()->Disconnect(eDC_ContextCorruption, "Trying to send break products with no break stream");
		return false;
	}
	if (!ContextState())
	{
		Parent()->Disconnect(eDC_ContextCorruption, "Failed allocating object for broken product - ContextState == NULL");
		return false;
	}
	if (!ContextState()->AllocateObject(0, param.id, 8, false, eST_Collected, NULL))
	{
		Parent()->Disconnect(eDC_ContextCorruption, "Failed allocating object for broken product");
		return false;
	}
	m_pBreakOps[param.brk]->push_back(param.id);
	return true;
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CClientContextView, PerformBreak, eNRT_ReliableUnordered, 0)
{
	if (param.brk < 0 || param.brk > MAX_BREAK_STREAMS)
	{
		NetWarning("Illegal break id %d", param.brk);
		return false;
	}
	if (!m_pBreakOps[param.brk])
	{
		NetWarning("Trying to send break products with no break stream... illegal [breakid=%d]", param.brk);
		return false;
	}
	if (ContextState())
	{
		_smart_ptr<INetBreakagePlayback> pBrk = new CBreakagePlayback(this, m_pBreakOps[param.brk]);
		ContextState()->GetGameContext()->PlaybackBreakage(param.brk, pBrk);
	}
	m_pBreakOps[param.brk] = 0;
	return true;
}

NET_IMPLEMENT_ATSYNC_MESSAGE(CClientContextView, PerformSimpleBreak, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	if (CNetContextState* pState = ContextState())
	{
		if (IGameContext* pGameContext = pState->GetGameContext())
		{
			CPerformBreakSimpleClient* pClient = new CPerformBreakSimpleClient(this);
			pClient->SerialiseNetIds(ser);

			// Call the game side code, which will call the correct version of SerialiseSimpleBreakage()
			void* userData = pGameContext->ReceiveSimpleBreakage(ser);

			// Need to invoke the playback call from the game thread!
			INetBreakageSimplePlaybackPtr pBreakSimplePlayback(pClient);
			TO_GAME(&CClientContextView::GC_PerformSimpleBreak, this, userData, pBreakSimplePlayback);
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_WARNING, "PerformSimpleBreak FAILED! GameContext was NULL!");
	}
	return true;
}

void CClientContextView::GC_PerformSimpleBreak(void* userData, INetBreakageSimplePlaybackPtr pBreakSimplePlayback)
{
	if (CNetContextState* pState = ContextState())
	{
		if (IGameContext* pGameContext = pState->GetGameContext())
		{
			pGameContext->PlaybackSimpleBreakage(userData, pBreakSimplePlayback);
		}
	}
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, FlushMsgs, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	StartFlushUpdates();
	return true;
}

#if SERVER_FILE_SYNC_MODE
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, BeginSyncFiles, eNRT_ReliableUnordered, 0)
{
	if (m_fileSyncPhase)
		return false;
	ser.Value("syncseq", m_fileSyncPhase, 'flid');
	if (!m_fileSyncPhase)
		return false;
	CryLog("BEGIN FILE SYNC: %d", m_fileSyncPhase);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, BeginSyncFile, eNRT_ReliableUnordered, 0)
{
	if (!m_fileSyncPhase)
		return false;
	uint32 id;
	string filename;
	ser.Value("id", id, 'flid');
	ser.Value("name", filename);
	return Context()->GetSyncedFileSet(true)->BeginUpdateFile(filename.c_str(), id);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, AddFileData, eNRT_ReliableUnordered, 0)
{
	if (!m_fileSyncPhase)
		return false;
	uint32 id;
	uint32 length;
	ser.Value("id", id, 'flid');
	ser.Value("length", length, 'flsz');
	if (length > CSyncedFileSet::MAX_SEND_CHUNK_SIZE)
		return false;
	uint8 buf[CSyncedFileSet::MAX_SEND_CHUNK_SIZE];
	for (int i = 0; i < length; i++)
		ser.Value("data", buf[i], 'ui8');
	return Context()->GetSyncedFileSet(true)->AddFileData(id, buf, length);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, EndSyncFile, eNRT_ReliableUnordered, 0)
{
	if (!m_fileSyncPhase)
		return false;
	uint32 id;
	ser.Value("id", id, 'flid');
	return Context()->GetSyncedFileSet(true)->EndUpdateFile(id);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, AllFilesSynced, eNRT_ReliableUnordered, 0)
{
	if (!m_fileSyncPhase)
		return false;
	uint32 cur;
	ser.Value("syncseq", cur, 'flid');
	if (m_fileSyncPhase != cur)
		return false;
	m_fileSyncPhase = 0;
	m_fileSyncsComplete++;
	return true;
}
#endif // SERVER_FILE_SYNC_MODE

void CClientContextView::BoundCollectedObject(SNetObjectID id, EntityId objId)
{
	FROM_GAME(&CClientContextView::NC_BoundCollectedObject, this, id, objId);
}

void CClientContextView::NC_BoundCollectedObject(SNetObjectID id, EntityId objId)
{
	ASSERT_GLOBAL_LOCK;
	SSimpleObjectIDParams msg(id);
	CServerContextView::SendBoundCollectedObjectWith(msg, Parent());
	ContextState()->RebindObject(id, objId);
}

bool CClientContextView::SetAspectProfileMessage(NetworkAspectID aspectIdx, TSerialize ser, uint32, uint32, uint32)
{
	ASSERT_GLOBAL_LOCK;

	uint8 profile;
	ser.Value("profile", profile);
	//const SContextObject * pObj = ContextState()->GetContextObject( CurrentObjectID() );
	SContextObjectRef obj = ContextState()->GetContextObject(CurrentObjectID());
	if (!obj.main)
	{
		NetLog("Object %s not found for SetAspectProfile", CurrentObjectID().GetText());
		Parent()->Disconnect(eDC_ContextCorruption, "SetAspectProfile - object not found");
		return false;
	}
	if (!ContextState()->DoSetAspectProfile(obj.main->userID, 1 << aspectIdx, profile, false, false, false))
	{
		NetLog("DoSetAspectProfile for object %s failed for SetAspectProfile", CurrentObjectID().GetText());
		Parent()->Disconnect(eDC_ContextCorruption, "SetAspectProfile - context failed");
		return false;
	}
	bool ok = true;
	//	m_objects[CurrentObjectID()].vAspectProfiles[aspect] = profile;
	//	ClearAspects( CurrentObjectID(), 1<<aspect );
	return ok;
}

bool CClientContextView::HasRemoteDef(const SNetMessageDef* pDef)
{
	return CServerContextView::ClassHasDef(pDef);
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CClientContextView, UpdatePhysicsTime, eNRT_UnreliableUnordered, 0)
{
	return SetPhysicsTime(param.tm);
}

void CClientContextView::UnboundObject(SNetObjectID id)
{
	CServerContextView::SendCompletedUnbindObjectWith(id, Parent());
	CContextView::UnboundObject(id);
}

void CClientContextView::AddWaitForFileSyncComplete(IContextEstablisher* pEst, EContextViewState when)
{
#if SERVER_FILE_SYNC_MODE
	if (!IsLocal() && ServerFileSyncEnabled())
		AddWaitValue(pEst, when, &m_fileSyncsComplete, m_fileSyncsComplete + 1, "WaitForFileSync", 5.0f);
#endif // SERVER_FILE_SYNC_MODE
}

#if ENABLE_DEBUG_KIT
void CClientContextView::BeginWorldUpdate(SNetObjectID obj)
{
	ASSERT_GLOBAL_LOCK;
	CNetContextState* pContext = ContextState();
	if (pContext)
	{
		IEntity* pEnt = gEnv->pEntitySystem->GetEntity(pContext->GetContextObject(obj).main->userID);
		if (!pEnt)
			return;
		IPhysicalEntity* pPhys = pEnt->GetPhysics();
		if (!pPhys)
			return;
		pe_status_pos pos;
		pPhys->GetStatus(&pos);
		m_curWorldPos = pos.pos;
	}
}

void CClientContextView::EndWorldUpdate(SNetObjectID obj)
{
	ASSERT_GLOBAL_LOCK;
	CNetContextState* pContext = ContextState();
	if (pContext)
	{
		IEntity* pEnt = gEnv->pEntitySystem->GetEntity(pContext->GetContextObject(obj).main->userID);
		if (!pEnt)
			return;
		IPhysicalEntity* pPhys = pEnt->GetPhysics();
		if (!pPhys)
			return;
		pe_status_pos pos;
		pPhys->GetStatus(&pos);
		m_pNetVis->AddSample(obj, 2, pos.pos.GetDistance(m_curWorldPos));

	#if FULL_ON_SCHEDULING
		Vec3 witnessPos, witnessDir;
		if (GetWitnessDirection(witnessDir) && GetWitnessPosition(witnessPos))
		{
			DEBUGKIT_LOG_SNAPPING(witnessPos, witnessDir, m_curWorldPos, pos.pos, pEnt->GetClass()->GetName());
		}
	#endif
	}
}
#endif

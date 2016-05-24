// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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
	SContextViewConfiguration config = {
		NULL, // FlushMsgs
		CServerContextView::ChangeState,
		NULL, // ForceNextState
		CServerContextView::FinishState,
		CServerContextView::BeginUpdateObject,
		CServerContextView::EndUpdateObject,
		NULL, // ReconfigureObject
		NULL, // SetAuthority
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
		CServerContextView::VoiceData,
#else
		NULL,
#endif
		NULL, // RemoveStaticEntity
		CServerContextView::UpdatePhysicsTime,
		NULL, // BeginSyncFiles;
		NULL, // BeginSyncFile;
		NULL, // AddFileData;
		NULL, // EndSyncFile;
		NULL, // AllFilesSynced;
		// partial update notification messages
		{
			CServerContextView::PartialAspect0,
			CServerContextView::PartialAspect1,
			CServerContextView::PartialAspect2,
			CServerContextView::PartialAspect3,
			CServerContextView::PartialAspect4,
			CServerContextView::PartialAspect5,
			CServerContextView::PartialAspect6,
			CServerContextView::PartialAspect7,
#if NUM_ASPECTS > 8
			CServerContextView::PartialAspect8,
			CServerContextView::PartialAspect9,
			CServerContextView::PartialAspect10,
			CServerContextView::PartialAspect11,
			CServerContextView::PartialAspect12,
			CServerContextView::PartialAspect13,
			CServerContextView::PartialAspect14,
			CServerContextView::PartialAspect15,
#endif//NUM_ASPECTS > 8
#if NUM_ASPECTS > 16
			CServerContextView::PartialAspect16,
			CServerContextView::PartialAspect17,
			CServerContextView::PartialAspect18,
			CServerContextView::PartialAspect19,
			CServerContextView::PartialAspect20,
			CServerContextView::PartialAspect21,
			CServerContextView::PartialAspect22,
			CServerContextView::PartialAspect23,
			CServerContextView::PartialAspect24,
			CServerContextView::PartialAspect25,
			CServerContextView::PartialAspect26,
			CServerContextView::PartialAspect27,
			CServerContextView::PartialAspect28,
			CServerContextView::PartialAspect29,
			CServerContextView::PartialAspect30,
			CServerContextView::PartialAspect31,
#endif//NUM_ASPECTS > 16
		},
		// set aspect profile messages
		{
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
#if NUM_ASPECTS > 8
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
#endif//NUM_ASPECTS > 8
#if NUM_ASPECTS > 16
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
#endif//NUM_ASPECTS > 16
		},
		// update aspect messages
		{
			CServerContextView::UpdateAspect0,
			CServerContextView::UpdateAspect1,
			CServerContextView::UpdateAspect2,
			CServerContextView::UpdateAspect3,
			CServerContextView::UpdateAspect4,
			CServerContextView::UpdateAspect5,
			CServerContextView::UpdateAspect6,
			CServerContextView::UpdateAspect7,
#if NUM_ASPECTS > 8
			CServerContextView::UpdateAspect8,
			CServerContextView::UpdateAspect9,
			CServerContextView::UpdateAspect10,
			CServerContextView::UpdateAspect11,
			CServerContextView::UpdateAspect12,
			CServerContextView::UpdateAspect13,
			CServerContextView::UpdateAspect14,
			CServerContextView::UpdateAspect15,
#endif//NUM_ASPECTS > 8
#if NUM_ASPECTS > 16
			CServerContextView::UpdateAspect16,
			CServerContextView::UpdateAspect17,
			CServerContextView::UpdateAspect18,
			CServerContextView::UpdateAspect19,
			CServerContextView::UpdateAspect20,
			CServerContextView::UpdateAspect21,
			CServerContextView::UpdateAspect22,
			CServerContextView::UpdateAspect23,
			CServerContextView::UpdateAspect24,
			CServerContextView::UpdateAspect25,
			CServerContextView::UpdateAspect26,
			CServerContextView::UpdateAspect27,
			CServerContextView::UpdateAspect28,
			CServerContextView::UpdateAspect29,
			CServerContextView::UpdateAspect30,
			CServerContextView::UpdateAspect31,
#endif//NUM_ASPECTS > 16
		},
#if ENABLE_ASPECT_HASHING
		// hash aspect messages
		{
			CServerContextView::HashAspect0,
			CServerContextView::HashAspect1,
			CServerContextView::HashAspect2,
			CServerContextView::HashAspect3,
			CServerContextView::HashAspect4,
			CServerContextView::HashAspect5,
			CServerContextView::HashAspect6,
			CServerContextView::HashAspect7,
	#if NUM_ASPECTS > 8
			CServerContextView::HashAspect8,
			CServerContextView::HashAspect9,
			CServerContextView::HashAspect10,
			CServerContextView::HashAspect11,
			CServerContextView::HashAspect12,
			CServerContextView::HashAspect13,
			CServerContextView::HashAspect14,
			CServerContextView::HashAspect15,
	#endif//NUM_ASPECTS > 8
	#if NUM_ASPECTS > 16
			CServerContextView::HashAspect16,
			CServerContextView::HashAspect17,
			CServerContextView::HashAspect18,
			CServerContextView::HashAspect19,
			CServerContextView::HashAspect20,
			CServerContextView::HashAspect21,
			CServerContextView::HashAspect22,
			CServerContextView::HashAspect23,
			CServerContextView::HashAspect24,
			CServerContextView::HashAspect25,
			CServerContextView::HashAspect26,
			CServerContextView::HashAspect27,
			CServerContextView::HashAspect28,
			CServerContextView::HashAspect29,
			CServerContextView::HashAspect30,
			CServerContextView::HashAspect31,
	#endif//NUM_ASPECTS > 16
		},
#endif
		// rmi messages
		{
			CServerContextView::RMI_ReliableOrdered,
			CServerContextView::RMI_ReliableUnordered,
			CServerContextView::RMI_UnreliableOrdered,
			NULL,
			// must be last
			CServerContextView::RMI_Attachment,
		},
	};

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
			ContextState()->GC_BoundObject(std::make_pair(ContextState()->GetContextObject(CurrentObjectID()).main->userID, nAspectsEnabled));
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
	SReceiveContext ctx = CreateReceiveContext(ser, 7, nCurSeq, nOldSeq, &ok);
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

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect0, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(0, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect1, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(1, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect2, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(2, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect3, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(3, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect4, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(4, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect5, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(5, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect6, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(6, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect7, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(7, ser, nCurSeq, nOldSeq);
}
#if NUM_ASPECTS > 8
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect8, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(8, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect9, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(9, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect10, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(10, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect11, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(11, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect12, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(12, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect13, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(13, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect14, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(14, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect15, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(15, ser, nCurSeq, nOldSeq);
}
#endif//NUM_ASPECTS > 8
#if NUM_ASPECTS > 16
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect16, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(16, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect17, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(17, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect18, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(18, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect19, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(19, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect20, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(20, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect21, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(21, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect22, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(22, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect23, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(23, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect24, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(24, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect25, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(25, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect26, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(26, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect27, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(27, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect28, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(28, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect29, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(29, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect30, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(30, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, UpdateAspect31, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return UpdateAspect(31, ser, nCurSeq, nOldSeq);
}
#endif//NUM_ASPECTS > 16

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
	SReceiveContext ctx = CreateReceiveContext(ser, 7, nCurSeq, nOldSeq, &ok);
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

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile0, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(0, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile1, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(1, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile2, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(2, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile3, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(3, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile4, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(4, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile5, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(5, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile6, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(6, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile7, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(7, ser);
}
#if NUM_ASPECTS > 8
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile8, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(8, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile9, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(9, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile10, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(10, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile11, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(11, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile12, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(12, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile13, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(13, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile14, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(14, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile15, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(15, ser);
}
#endif//NUM_ASPECTS > 8
#if NUM_ASPECTS > 16
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile16, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(16, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile17, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(17, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile18, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(18, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile19, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(19, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile20, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(20, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile21, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(21, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile22, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(22, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile23, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(23, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile24, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(24, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile25, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(25, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile26, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(26, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile27, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(27, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile28, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(28, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile29, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(29, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile30, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(30, ser);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, SetAspectProfile31, eNRT_UnreliableUnordered, 0)
{
	return SetAspectProfileMessage(31, ser);
}
#endif//NUM_ASPECTS > 16

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

#if ENABLE_ASPECT_HASHING
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect0, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(0, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect1, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(1, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect2, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(2, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect3, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(3, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect4, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(4, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect5, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(5, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect6, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(6, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect7, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(7, ser);
	return true;
}
	#if NUM_ASPECTS > 8
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect8, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(8, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect9, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(9, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect10, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(10, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect11, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(11, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect12, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(12, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect13, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(13, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect14, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(14, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect15, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(15, ser);
	return true;
}
	#endif//NUM_ASPECTS > 8
	#if NUM_ASPECTS > 16
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect16, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(16, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect17, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(17, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect18, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(18, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect19, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(19, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect20, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(20, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect21, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(21, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect22, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(22, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect23, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(23, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect24, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(24, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect25, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(25, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect26, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(26, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect27, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(27, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect28, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(28, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect29, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(29, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect30, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(30, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, HashAspect31, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(31, ser);
	return true;
}
	#endif//NUM_ASPECTS > 16
#endif

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect0, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(0, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect1, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(1, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect2, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(2, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect3, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(3, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect4, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(4, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect5, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(5, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect6, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(6, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect7, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(7, ser);
	return true;
}
#if NUM_ASPECTS > 8
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect8, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(8, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect9, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(9, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect10, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(10, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect11, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(11, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect12, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(12, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect13, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(13, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect14, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(14, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect15, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(15, ser);
	return true;
}
#endif
#if NUM_ASPECTS > 16
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect16, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(16, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect17, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(17, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect18, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(18, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect19, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(19, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect20, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(20, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect21, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(21, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect22, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(22, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect23, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(23, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect24, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(24, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect25, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(25, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect26, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(26, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect27, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(27, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect28, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(28, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect29, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(29, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect30, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(30, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientContextView, PartialAspect31, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(31, ser);
	return true;
}
#endif

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

bool CClientContextView::SetAspectProfileMessage(NetworkAspectID aspectIdx, TSerialize ser)
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

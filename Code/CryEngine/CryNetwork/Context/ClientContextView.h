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
#ifndef __CLIENTCONTEXTVIEW_H__
#define __CLIENTCONTEXTVIEW_H__

#pragma once

#include "ContextView.h"
#include "Authentication.h"
#include "DebugKit/NetVis.h"

struct SDeclareBrokenProduct
{
	SDeclareBrokenProduct(SNetObjectID i = SNetObjectID(), int b = -1) : id(i), brk(b) {}

	SNetObjectID id;
	int          brk;

	void         SerializeWith(TSerialize ser)
	{
		ser.Value("id", id, 'eid');
		ser.Value("brk", brk, 'brId');
	}
};

struct SOnlyBreakId
{
	SOnlyBreakId(int b = -1) : brk(b) {}

	int  brk;

	void SerializeWith(TSerialize ser)
	{
		ser.Value("brk", brk, 'brId');
	}
};

struct SServerPublicAddress
{
	SServerPublicAddress(uint32 i = 0, uint16 p = 0) : ip(i), port(p){}
	uint32 ip;
	uint16 port;
	void   SerializeWith(TSerialize ser)
	{
		ser.Value("ip", ip);
		ser.Value("port", port);
	}
};

#define CCV_NUM_EXTRA_MESSAGES (NUM_ASPECTS * 3)
#define CLIENTVIEW_MIN_NUM_MESSAGES 31

// implements CContextView in a way that acts like a client
class CClientContextView :
	public CNetMessageSinkHelper<CClientContextView, CContextView, CLIENTVIEW_MIN_NUM_MESSAGES + CCV_NUM_EXTRA_MESSAGES>
{
public:
	CClientContextView(CNetChannel*, CNetContext*);
	virtual ~CClientContextView();

	// Statically constructed protocol message defitions for aspect-related messages.
	template <int AspectNum>
	struct msgUpdateAspect { // template-template arg cannot be a function, only a class.
		static SNetMessageDef* fun() { return Helper_AddAspectMessage(
			TrampolineAspect<AspectNum, CContextView, &CClientContextView::UpdateAspect>,
			"CClientContextView:UpdateAspect", AspectNum, eMPF_BlocksStateChange);
		}
	};

	template<int AspectNum>
	struct msgPartialAspect {
		static SNetMessageDef* fun() { return Helper_AddAspectMessage(
			TrampolineAspect<AspectNum, CContextView, &CClientContextView::PartialAspect>,
			"CClientContextView:PartialAspect", AspectNum, eMPF_BlocksStateChange);
		}
	};

	template<int AspectNum>
	struct msgSetAspectProfile {
		static SNetMessageDef* fun() { return Helper_AddAspectMessage(
			TrampolineAspect<AspectNum, CClientContextView, &CClientContextView::SetAspectProfileMessage>,
			"CClientContextView:SetAspectProfile", AspectNum);
		}
	};

#if ENABLE_SESSION_IDS
	NET_DECLARE_IMMEDIATE_MESSAGE(InitSessionData);
#endif
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(AuthenticateChallenge, SAuthenticationSalt);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(ChangeState, SChangeStateMessage);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(ForceNextState, SChangeStateMessage);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(RemoveStaticObject, SRemoveStaticObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(FinishState);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(UpdatePhysicsTime, SPhysicsTime);
	NET_DECLARE_IMMEDIATE_MESSAGE(FlushMsgs);

	NET_DECLARE_IMMEDIATE_MESSAGE(BeginUpdateObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(EndUpdateObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(BeginBindObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(BeginBindPredictedObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(BeginBindStaticObject);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(BeginUnbindObject, SSimpleObjectIDParams);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(UnbindPredictedObject, SRemoveStaticObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(ReconfigureObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_UnreliableOrdered);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_ReliableUnordered);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_ReliableOrdered);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_Attachment);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAuthority);

	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(BeginBreakStream, SOnlyBreakId);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(DeclareBrokenProduct, SDeclareBrokenProduct);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(PerformBreak, SOnlyBreakId);

	NET_DECLARE_ATSYNC_MESSAGE(PerformSimpleBreak);
	void GC_PerformSimpleBreak(void* userData, INetBreakageSimplePlaybackPtr pBreakSimplePlayback);

	NET_DECLARE_IMMEDIATE_MESSAGE(BeginSyncFiles);
	NET_DECLARE_IMMEDIATE_MESSAGE(BeginSyncFile);
	NET_DECLARE_IMMEDIATE_MESSAGE(AddFileData);
	NET_DECLARE_IMMEDIATE_MESSAGE(EndSyncFile);
	NET_DECLARE_IMMEDIATE_MESSAGE(AllFilesSynced);

	NET_DECLARE_IMMEDIATE_MESSAGE(VoiceData);

	//NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(InvalidatePredictedSpawn);

	virtual bool        IsClient() const { return true; }
	virtual void        CompleteInitialization();

	void                AddWaitForFileSyncComplete(IContextEstablisher* pEst, EContextViewState when);

	virtual void        ChangeContext();
	virtual void        BindObject(SNetObjectID nID);
	virtual void        UnbindObject(SNetObjectID nID);
	virtual void        EstablishedContext();
	virtual const char* ValidateMessage(
	  const SNetMessageDef*,
	  bool bNetworkMsg);
	virtual bool HasRemoteDef(const SNetMessageDef* pDef);
	virtual void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CClientContextView");

		pSizer->Add(*this);
		pSizer->AddContainer(m_predictedSpawns);
		CContextView::GetMemoryStatistics(pSizer);

#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)
		pSizer->AddContainer(m_tempPackets);
		for (size_t i = 0; i < m_tempPackets.size(); ++i)
			pSizer->AddObject(m_tempPackets[i].get(), sizeof(CVoicePacket));
#endif
	}

	// INetMessageSink
	void DefineProtocol(IProtocolBuilder* pBuilder);

#if ENABLE_DEBUG_KIT
	virtual TNetChannelID GetLoggingChannelID(TNetChannelID local, TNetChannelID remote) { return -(int)remote; }
	void BeginWorldUpdate(SNetObjectID);
	void EndWorldUpdate(SNetObjectID);
#endif

	void BoundCollectedObject(SNetObjectID id, EntityId objId);
	void NC_BoundCollectedObject(SNetObjectID id, EntityId objId);

	void OnObjectEvent(CNetContextState*, SNetObjectEvent* pEvent);

protected:
	virtual bool EnterState(EContextViewState state);
	virtual void UnboundObject(SNetObjectID id);

private:
	enum EBeginBindFlags
	{
		eBBF_ReadObjectID = BIT(0),
		eBBF_FlagStatic   = BIT(1),
	};

	virtual bool        ShouldInitContext() { return false; }
	virtual const char* DebugString()       { return "CLIENT"; }
	void                SendEstablishedMessage();
	bool                DoBeginBind(TSerialize ser, uint32 flags);
	bool                SetAspectProfileMessage(NetworkAspectID aspectIdx, TSerialize ser, uint32, uint32, uint32);
	void                ContinueEnterState();
	static void                   OnUpdatePredictedSpawn(void* pUsr1, void* pUsr2, ENetSendableStateUpdate);

	SNetClientBreakDescriptionPtr m_pBreakOps[MAX_BREAK_STREAMS];

	virtual uint32 FilterEventMask(uint32 mask, EContextViewState state)
	{
		return mask | (ENABLE_DEBUG_KIT * eNOE_SyncWithGame_End);
	}

	virtual void OnWitnessDeclared();

	std::set<EntityId> m_predictedSpawns;

#if ENABLE_DEBUG_KIT
	std::auto_ptr<CNetVis> m_pNetVis;
	float                  m_startUpdate;
	Vec3                   m_curWorldPos;
#endif

#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)
	SSendableHandle              m_nVoicePacketHandle;
	std::vector<TVoicePacketPtr> m_tempPackets;
#endif

#if SERVER_FILE_SYNC_MODE
	uint32 m_fileSyncPhase;
	uint32 m_fileSyncsComplete;
#endif
};

#endif

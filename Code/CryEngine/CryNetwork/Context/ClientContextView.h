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

#if ENABLE_ASPECT_HASHING
	#define CCV_NUM_EXTRA_MESSAGES (NUM_ASPECTS * 4)
#else
	#define CCV_NUM_EXTRA_MESSAGES (NUM_ASPECTS * 3)
#endif

#define CLIENTVIEW_MIN_NUM_MESSAGES 31

// implements CContextView in a way that acts like a client
class CClientContextView :
	public CNetMessageSinkHelper<CClientContextView, CContextView, CLIENTVIEW_MIN_NUM_MESSAGES + CCV_NUM_EXTRA_MESSAGES>
{
public:
	CClientContextView(CNetChannel*, CNetContext*);
	virtual ~CClientContextView();

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
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect0);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect1);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect2);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect3);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect4);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect5);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect6);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect7);
#if NUM_ASPECTS > 8
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect8);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect9);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect10);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect11);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect12);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect13);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect14);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect15);
#endif
#if NUM_ASPECTS > 16
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect16);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect17);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect18);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect19);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect20);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect21);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect22);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect23);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect24);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect25);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect26);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect27);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect28);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect29);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect30);
	NET_DECLARE_IMMEDIATE_MESSAGE(UpdateAspect31);
#endif
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_UnreliableOrdered);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_ReliableUnordered);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_ReliableOrdered);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_Attachment);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAuthority);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile0);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile1);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile2);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile3);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile4);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile5);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile6);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile7);
#if NUM_ASPECTS > 8
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile8);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile9);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile10);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile11);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile12);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile13);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile14);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile15);
#endif
#if NUM_ASPECTS > 16
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile16);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile17);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile18);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile19);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile20);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile21);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile22);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile23);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile24);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile25);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile26);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile27);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile28);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile29);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile30);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetAspectProfile31);
#endif
#if ENABLE_ASPECT_HASHING
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect0);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect1);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect2);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect3);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect4);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect5);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect6);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect7);
	#if NUM_ASPECTS > 8
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect8);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect9);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect10);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect11);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect12);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect13);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect14);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect15);
	#endif
	#if NUM_ASPECTS > 16
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect16);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect17);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect18);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect19);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect20);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect21);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect22);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect23);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect24);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect25);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect26);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect27);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect28);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect29);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect30);
	NET_DECLARE_IMMEDIATE_MESSAGE(HashAspect31);
	#endif
#endif
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect0);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect1);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect2);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect3);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect4);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect5);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect6);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect7);
#if NUM_ASPECTS > 8
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect8);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect9);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect10);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect11);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect12);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect13);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect14);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect15);
#endif
#if NUM_ASPECTS > 16
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect16);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect17);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect18);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect19);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect20);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect21);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect22);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect23);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect24);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect25);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect26);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect27);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect28);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect29);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect30);
	NET_DECLARE_IMMEDIATE_MESSAGE(PartialAspect31);
#endif

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
	bool                SetAspectProfileMessage(NetworkAspectID aspectIdx, TSerialize ser);
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

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
#ifndef __SERVERCONTEXTVIEW_H__
#define __SERVERCONTEXTVIEW_H__

#include "ContextView.h"
#include "Authentication.h"

class CPerformBreakSimpleServer;

#ifdef __WITH_PB__
struct SInitPunkBusterParams
{
	SInitPunkBusterParams(bool has = false) : clientHasPunkBuster(has) {}
	bool clientHasPunkBuster;
	void SerializeWith(TSerialize ser)
	{
		ser.Value("hasPunkbuster", clientHasPunkBuster);
	}
};
#endif

struct SSetNicknameParams
{
	SSetNicknameParams(){}
	SSetNicknameParams(const char* nick) : m_nick(nick){}
	string m_nick;
	void   SerializeWith(TSerialize ser)
	{
		ser.Value("nick", m_nick);
	}
};

// implement CContextView in a way that acts like a server
typedef CNetMessageSinkHelper<CServerContextView, CContextView, 80 + ((NUM_ASPECTS - 8)*3)> TServerContextViewParent;
class CServerContextView : public TServerContextViewParent
{
	friend class CPerformBreakSimpleServer;
public:
	CServerContextView(CNetChannel*, CNetContext*);
	~CServerContextView();

	virtual void        BindObject(SNetObjectID nID);
	virtual void        UnbindObject(SNetObjectID nID);
	virtual void        ChangeContext();
	virtual void        EstablishedContext();
	virtual const char* ValidateMessage(const SNetMessageDef*, bool bNetworkMsg);
	virtual bool        HasRemoteDef(const SNetMessageDef* pDef);
	virtual bool        IsClient() const { return false; }
	virtual void        ClearAllState();
	virtual void        CompleteInitialization();
	virtual void        OnObjectEvent(CNetContextState*, SNetObjectEvent* pEvent);

	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(AuthenticateResponse, CWhirlpoolHash);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(ChangeState, SChangeStateMessage);
	NET_DECLARE_IMMEDIATE_MESSAGE(FinishState);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(UpdatePhysicsTime, SPhysicsTime);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(CompletedUnbindObject, SSimpleObjectIDParams);

	NET_DECLARE_IMMEDIATE_MESSAGE(ClientEstablishedContext);

	NET_DECLARE_IMMEDIATE_MESSAGE(BeginUpdateObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(EndUpdateObject);
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
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_UnreliableOrdered);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_ReliableUnordered);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_ReliableOrdered);
	NET_DECLARE_IMMEDIATE_MESSAGE(RMI_Attachment);

	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(SetNickname, SSetNicknameParams);
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	NET_DECLARE_IMMEDIATE_MESSAGE(VoiceData);
#endif

	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(BoundCollectedObject, SSimpleObjectIDParams);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(SkippedCollectedObject, SSimpleObjectIDParams);

#ifdef __WITH_PB__
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(InitPunkBuster, SInitPunkBusterParams);
#endif

	virtual bool IsServer() const { return true; }

	// INetMessageSink
	void         DefineProtocol(IProtocolBuilder* pBuilder);

	virtual void GetMemoryStatistics(ICrySizer* pSizer);
	virtual void SendAuthChecks();

	void         AddWaitForFileSyncComplete(IContextEstablisher* pEst, EContextViewState when) {}; // for now, do nothing for the server

#if ENABLE_DEBUG_KIT
	virtual TNetChannelID GetLoggingChannelID(TNetChannelID local, TNetChannelID remote) { return local; }
#endif

protected:
	virtual bool EnterState(EContextViewState state);
	virtual void InitChannelEstablishmentTasks(IContextEstablisher* pEst);

private:
	virtual bool        ShouldInitContext() { return true; }
	virtual const char* DebugString()       { return "SERVER"; }
	virtual void        GotBreakage(const SNetIntBreakDescription* pDesc);
	void                SendUnbindMessage(SNetObjectID netID, bool bFromBind, CNetObjectBindLock lk);
	void                InitSessionIDs();
	void                RemoveStaticEntity(EntityId id);

	void GC_BindObject(SNetObjectID, CNetObjectBindLock lk, CChangeStateLock cslk);

	typedef std::auto_ptr<SAuthenticationSalt> TAuthPtr;

	class CBindObjectMessage;
	class CDeclareBrokenProductMessage;
	class CUnbindObjectMessage;
	class CBeginBreakStream;
	class CPerformBreak;

	class CCET_SyncFiles;

	virtual uint32 FilterEventMask(uint32 mask, EContextViewState state)
	{
		mask |= eNOE_ValidatePrediction;
		if (state >= eCVS_ConfigureContext)
			mask |= eNOE_RemoveStaticEntity;
		return mask;
	}

	virtual void OnWitnessDeclared();

	struct SBreakStreamHandle
	{
		SSendableHandle hdl;
		int             id;
	};
	SBreakStreamHandle m_breakStreamHandles[MAX_BREAK_STREAMS];

	typedef Vec3_tpl<int> BreakSegmentID;
	struct CompareBreakSegmentIDs
	{
		bool operator()(const BreakSegmentID& a, const BreakSegmentID& b) const
		{
			if (a.x < b.x)
				return true;
			else if (a.x > b.x)
				return false;
			else if (a.y < b.y)
				return true;
			else if (a.y > b.y)
				return false;
			else if (a.z < b.z)
				return true;
			else if (a.z > b.z)
				return false;
			return false;
		}
	};
	typedef std::map<BreakSegmentID, SSendableHandle, CompareBreakSegmentIDs, STLMementoAllocator<std::pair<BreakSegmentID, SSendableHandle>>> TBreakSegmentStreams;
	std::auto_ptr<TBreakSegmentStreams> m_pBreakSegmentStreams;

	TAuthPtr                            m_pAuth;

	// locks for establish context
	CChangeStateLock m_lockRemoteMapLoaded;
	CChangeStateLock m_lockLocalMapLoaded;

	typedef std::map<EntityId, EntityId, std::less<EntityId>, STLMementoAllocator<std::pair<EntityId, EntityId>>> TValidatedPredictionMap;
	std::auto_ptr<TValidatedPredictionMap> m_pValidatedPredictions;

#if USE_SYSTEM_ALLOCATOR
	typedef std::map<SNetObjectID, CNetObjectBindLock, std::less<SNetObjectID>>                                                                   TPendingUnbinds;
#else
	typedef std::map<SNetObjectID, CNetObjectBindLock, std::less<SNetObjectID>, STLMementoAllocator<std::pair<SNetObjectID, CNetObjectBindLock>>> TPendingUnbinds;
#endif
	std::auto_ptr<TPendingUnbinds> m_pPendingUnbinds;

#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)
	std::vector<INetChannel*>                             m_pVoiceListeners;
	std::vector<std::pair<SNetObjectID, TVoicePacketPtr>> m_tempPackets;
#endif
	std::vector<SSendableHandle>                          m_dependencyStaging;

#ifdef __WITH_PB__
	bool m_clientHasPunkBuster;
#endif
};

#endif

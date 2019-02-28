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

	// Statically constructed protocol message defitions for aspect-related messages.
	template <int AspectNum>
	struct msgUpdateAspect {
		static SNetMessageDef* fun() { return Helper_AddAspectMessage(
			TrampolineAspect<AspectNum, CContextView, &CServerContextView::UpdateAspect>,
			"CServerContextView:UpdateAspect", AspectNum, eMPF_BlocksStateChange);
		}
	};

	template<int AspectNum>
	struct msgPartialAspect {
		static SNetMessageDef* fun() { return Helper_AddAspectMessage(
			TrampolineAspect<AspectNum, CContextView, &CServerContextView::PartialAspect>,
			"CServerContextView:PartialAspect", AspectNum, eMPF_BlocksStateChange);
		}
	};

	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(AuthenticateResponse, CWhirlpoolHash);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(ChangeState, SChangeStateMessage);
	NET_DECLARE_IMMEDIATE_MESSAGE(FinishState);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(UpdatePhysicsTime, SPhysicsTime);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(CompletedUnbindObject, SSimpleObjectIDParams);

	NET_DECLARE_IMMEDIATE_MESSAGE(ClientEstablishedContext);

	NET_DECLARE_IMMEDIATE_MESSAGE(BeginUpdateObject);
	NET_DECLARE_IMMEDIATE_MESSAGE(EndUpdateObject);

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

	void GC_BindObject( SNetObjectID, CNetObjectBindLock lk, CChangeStateLock cslk, bool levelInit);

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
	typedef std::map<BreakSegmentID, SSendableHandle, CompareBreakSegmentIDs, STLMementoAllocator<std::pair<const BreakSegmentID, SSendableHandle>>> TBreakSegmentStreams;
	std::auto_ptr<TBreakSegmentStreams> m_pBreakSegmentStreams;

	TAuthPtr                            m_pAuth;

	// locks for establish context
	CChangeStateLock m_lockRemoteMapLoaded;
	CChangeStateLock m_lockLocalMapLoaded;

	typedef std::map<EntityId, EntityId, std::less<EntityId>, STLMementoAllocator<std::pair<const EntityId, EntityId>>> TValidatedPredictionMap;
	std::auto_ptr<TValidatedPredictionMap> m_pValidatedPredictions;

#if USE_SYSTEM_ALLOCATOR
	typedef std::map<SNetObjectID, CNetObjectBindLock, std::less<SNetObjectID>>                                                                   TPendingUnbinds;
#else
	typedef std::map<SNetObjectID, CNetObjectBindLock, std::less<SNetObjectID>, STLMementoAllocator<std::pair<const SNetObjectID, CNetObjectBindLock>>> TPendingUnbinds;
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

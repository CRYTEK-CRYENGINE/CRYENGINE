// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  Server implementation of a context view
   -------------------------------------------------------------------------
   History:
   - 26/07/2004   : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"
#include "ServerContextView.h"
#include "ClientContextView.h"
#include "NetContext.h"
#include <CrySystem/ITimer.h>
#include "VoiceContext.h"
#include <CryNetwork/INetworkService.h>
#include "Protocol/NullSendable.h"

#include "CET_Server.h"
#include "UpdateMessage.h"
#include "SyncedFileSet.h"

#include "BreakagePlayback.h"
#include "PerformBreakage.h"

class CServerContextView::CBindObjectMessage : public CUpdateMessage
{
public:
	CBindObjectMessage(SNetObjectID id, INetSendableHookPtr pSpawnHook, CServerContextView* pView, EntityId predicted, CNetObjectBindLock& lk, CChangeStateLock cslk) :
		CUpdateMessage(MakeConfig(id, pView, predicted != 0), eSCF_AssumeEnabled
#if ENABLE_THIN_BINDS
		               | eSCF_UseBindAspectMask
#endif // ENABLE_THIN_BINDS
		               ),
		m_pSpawn(pSpawnHook),
		m_pView(pView),
		m_predicted(predicted),
		m_lk(lk),
		m_cslk(cslk)
	{
#if ENABLE_THIN_BINDS
		m_bindAspectMask = pView->ContextState()->GetBindAspectMask(id);
#endif // ENABLE_THIN_BINDS
	}

#if ENABLE_PACKET_PREDICTION
	virtual SMessageTag GetMessageTag(INetSender* pSender, IMessageMapper* mapper)
	{
		SMessageTag mTag = CUpdateMessage::GetMessageTag(pSender, mapper);

		if (mTag.messageId == 0xFFFFFFFF)
			return mTag;

		mTag.messageId = 0x80000000;    // Special flag to seperate binds (which are generally full or no serialisation from updates which have two varying components)

		return mTag;
	}
#endif

#ifdef _DEBUG
	virtual EMessageSendResult Send(INetSender* pSender)
	{
		EMessageSendResult r = CUpdateMessage::Send(pSender);
		return r;
	}
#endif

	void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate state)
	{
		if (m_pSpawn)
			m_pSpawn->UpdateState(nFromSeq, state);

		if (state == eNSSU_Ack)
		{
			if (!m_pView->ContextState()->GetContextObject(m_netID).main->userID)
				m_pView->SendUnbindMessage(m_netID, false, m_lk);
			else
				m_pView->SetSpawnState(m_netID, eSS_Enabled);
		}

		CUpdateMessage::UpdateState(nFromSeq, state);
	}

private:
	const char* GetBasicDescription() const
	{
		return "Bind";
	}

	bool CheckHook()
	{
		return true;
	}

	EMessageSendResult WriteHook(EWriteHook when, INetSender* pSender)
	{
		switch (when)
		{
		case eWH_BeforeBegin:
			{
				EMessageSendResult res = PreBindChecks();
				if (res != eMSR_SentOk)
					return res;
			}
			if (m_pSpawn)
			{
#if RESERVE_UNBOUND_ENTITIES
				m_pSpawn->SetPartialNetID(m_netID.id);
#endif
				m_pSpawn->Send(pSender);
				SentData();
			}
			break;
		case eWH_DuringBegin:
			{
				CNetContextState* pNetContext = m_pView->ContextState();
				//const SContextObject* obj = pNetContext->GetContextObject(m_netID);
				SContextObjectRef obj = pNetContext->GetContextObject(m_netID);
				if (!obj.main)
					return eMSR_FailedConnection;
				if (!m_pView->IsLocal())
				{
					if (m_predicted)
					{
						pSender->ser.Value("userID", m_predicted);
					}
					else if (obj.main->spawnType == eST_Static)
					{
						EntityId nUserID = obj.main->userID;
						pSender->ser.Value("userID", nUserID);
					}
					NetworkAspectType nAspectsEnabled = obj.xtra->nAspectsEnabled;
					NetworkAspectType delegatableMask = obj.xtra->delegatableMask;
					pSender->ser.Value("aspects", nAspectsEnabled);
					pSender->ser.Value("delegatableMask", delegatableMask);
				}
				SentData();
			}
			break;
		}
		return eMSR_SentOk;
	}

	EMessageSendResult PreBindChecks()
	{
		SContextObjectRef obj = m_pView->ContextState()->GetContextObject(m_netID);
		if (!obj.main || !obj.main->userID)
		{
			// TODO: should try to find out how this can happen and eliminate
			// the possibility
#if ENABLE_DEBUG_KIT
			NetWarning("Last chance: Trying to bind non-existant object: ignoring (netID was %s)", m_netID.GetText());
#endif
			if (obj.main)
				m_pView->UnboundObject(m_netID);
			return eMSR_FailedMessage;
		}

		SSendableHandle after;
		if (obj.main->parent)
		{
			if (m_pView->m_objects.size() <= obj.main->parent.id || !m_pView->IsObjectBound(obj.main->parent))
			{
				//const SContextObject * pParentObj = ContextState()->GetContextObject(obj.main->parent);
				SContextObjectRef parentObj = m_pView->ContextState()->GetContextObject(obj.main->parent);
				if (!parentObj.main || !parentObj.main->userID)
				{
#if ENABLE_DEBUG_KIT
					NetWarning("Last chance: Trying to bind object with non-existing parent: ignoring (name was %s)", obj.main->GetName());
#endif
					m_pView->UnboundObject(m_netID);
					return eMSR_FailedMessage;
				}
			}
		}

		return eMSR_SentOk;
	}

	static SUpdateMessageConfig MakeConfig(SNetObjectID id, CServerContextView* pView, bool predicted)
	{
		SUpdateMessageConfig cfg;
		cfg.m_netID = id;
		if (predicted)
			cfg.m_pStartUpdateDef = CClientContextView::BeginBindPredictedObject;
		else if (pView->ContextState()->GetContextObject(id).main->spawnType != eST_Static)
			cfg.m_pStartUpdateDef = CClientContextView::BeginBindObject;
		else
			cfg.m_pStartUpdateDef = CClientContextView::BeginBindStaticObject;
		cfg.m_pView = pView;
		return cfg;
	}

	INetSendableHookPtr            m_pSpawn;
	EntityId                       m_predicted;
	_smart_ptr<CServerContextView> m_pView;
	CNetObjectBindLock             m_lk;
	CChangeStateLock               m_cslk;
};

class CServerContextView::CDeclareBrokenProductMessage : public INetSendable
{
public:
	CDeclareBrokenProductMessage(SNetObjectID id, int brk, CServerContextView* pView)
		: INetSendable(eMPF_BlocksStateChange, eNRT_ReliableUnordered)
		, m_details(id, brk)
		, m_pView(pView)
		, m_lk(pView->ContextState(), id, "DECLBRK")
		, m_cslk(pView, "DECLBRK")
	{
	}

	virtual size_t GetSize() { return sizeof(*this); }

#if ENABLE_PACKET_PREDICTION
	virtual SMessageTag GetMessageTag(INetSender* pSender, IMessageMapper* mapper)
	{
		SMessageTag mTag;
		mTag.messageId = mapper->GetMsgId(CClientContextView::DeclareBrokenProduct);
		mTag.varying1 = 0;
		mTag.varying2 = 0;
		return mTag;
	}
#endif

	virtual EMessageSendResult Send(INetSender* pSender)
	{
		pSender->BeginMessage(CClientContextView::DeclareBrokenProduct);
		m_details.SerializeWith(pSender->ser);
		return eMSR_SentOk;
	}
	virtual void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate state)
	{
		if (state == eNSSU_Ack)
		{
			if (!m_pView->ContextState()->GetContextObject(m_lk.GetID()).main->userID)
				m_pView->SendUnbindMessage(m_lk.GetID(), false, m_lk);
			else
				m_pView->SetSpawnState(m_lk.GetID(), eSS_Spawned);
		}
	}
	virtual const char* GetDescription()                           { return "NullSendable"; }
	virtual void        GetPositionInfo(SMessagePositionInfo& pos) {}

private:
	SDeclareBrokenProduct          m_details;
	_smart_ptr<CServerContextView> m_pView;
	CNetObjectBindLock             m_lk;
	CChangeStateLock               m_cslk;
};

class CServerContextView::CUnbindObjectMessage : public INetSendable
{
public:
	CUnbindObjectMessage(SNetObjectID id, CServerContextView* pView, CNetObjectBindLock& lock) :
		INetSendable(CClientContextView::BeginBindObject->parallelFlags, CClientContextView::BeginBindObject->reliability),
		m_id(id),
		m_pView(pView),
		m_lock(lock)
	{
	}
	virtual const char* GetDescription()
	{
		if (m_name.empty())
			m_name.Format("Unbind %s", m_id.GetText());
		return m_name.c_str();
	}

	void GetPositionInfo(SMessagePositionInfo& pos)
	{
#if FULL_ON_SCHEDULING
		CNetContextState* pCtx = m_pView->ContextState();
		NET_ASSERT(pCtx);
		//const SContextObject * pObj = pCtx->GetContextObject( pCtx->GetNetID(m_id) );
		SContextObjectRef obj = pCtx->GetContextObject(m_id);
		if (obj.main)
		{
			pos.havePosition = obj.xtra->hasPosition;
			pos.position = obj.xtra->position;
			pos.haveDrawDistance = obj.xtra->hasDrawDistance;
			pos.drawDistance = obj.xtra->drawDistance;
		}
#endif
	}

	virtual size_t GetSize()
	{
		return sizeof(*this);
	}

#if ENABLE_PACKET_PREDICTION
	virtual SMessageTag GetMessageTag(INetSender* pSender, IMessageMapper* mapper)
	{
		SMessageTag mTag;
		mTag.messageId = mapper->GetMsgId(CClientContextView::BeginUnbindObject);
		mTag.varying1 = 0;
		mTag.varying2 = 0;
		return mTag;
	}
#endif

	virtual EMessageSendResult Send(INetSender* pSender)
	{
		pSender->BeginMessage(CClientContextView::BeginUnbindObject);
		SSimpleObjectIDParams(m_id).SerializeWith(pSender->ser);
		return eMSR_SentOk;
	}
	virtual void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate state)
	{
		if (state == eNSSU_Ack)
		{
			m_pView->m_pPendingUnbinds->insert(std::make_pair(m_id, m_lock));
		}
	}

private:
	_smart_ptr<CServerContextView> m_pView;
	SNetObjectID                   m_id;
	CNetObjectBindLock             m_lock;
	string                         m_name;

	// helper for CONTEXTVIEW_BOUND_OBJECT_STRING
	string GetName()
	{
		return m_pView->GetName();
	}
};

class CServerContextView::CPerformBreak : public INetSendable
{
public:
	CPerformBreak(CServerContextView* pView, int id, const SNetIntBreakDescription* pDesc) : INetSendable(eMPF_BlocksStateChange, eNRT_ReliableUnordered), m_id(id), m_pView(pView), m_pDesc(pDesc)
	{
	}

#if ENABLE_PACKET_PREDICTION
	SMessageTag GetMessageTag(INetSender* pSender, IMessageMapper* mapper)
	{
		SMessageTag mTag;
		mTag.messageId = mapper->GetMsgId(CClientContextView::PerformBreak);
		mTag.varying1 = 0;
		mTag.varying2 = 0;
		return mTag;
	}
#endif

	EMessageSendResult Send(INetSender* pSender)
	{
		pSender->BeginMessage(CClientContextView::PerformBreak);
		SOnlyBreakId(m_id).SerializeWith(pSender->ser);
		return eMSR_SentOk;
	}

	void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate state)
	{
		if (state != eNSSU_Requeue)
		{
			m_pView->UnlockStateChanges("BREAK");

			for (DynArray<CNetObjectBindLock>::const_iterator itlk = m_locks.begin(); itlk != m_locks.end(); ++itlk)
			{
				m_pView->SendablesDependentOnObjectRemove(itlk->GetID(), m_handle);
			}
		}
	}

	void GetPositionInfo(SMessagePositionInfo& pos)
	{
		AABB aabb;
		m_pDesc->pMessagePayload->GetAffectedRegion(aabb);
		pos.havePosition = true;
		pos.position = aabb.GetCenter();
	}

	size_t GetSize()
	{
		return sizeof(*this);
	}

	const char* GetDescription()
	{
		return "PerformBreak";
	}

	void SetHandle(SSendableHandle hdl, const DynArray<EntityId>& alsoEnts)
	{
		NET_ASSERT(!m_handle);
		m_handle = hdl;
		AddEntities(alsoEnts);
		AddEntities(m_pDesc->spawnedObjects);
		for (DynArray<CNetObjectBindLock>::const_iterator itlk = m_locks.begin(); itlk != m_locks.end(); ++itlk)
			m_pView->m_pSendables->insert(std::make_pair(itlk->GetID(), m_handle));
	}

private:
	int                            m_id;
	const SNetIntBreakDescription* m_pDesc;
	_smart_ptr<CServerContextView> m_pView;
	SSendableHandle                m_handle;
	DynArray<CNetObjectBindLock>   m_locks;

	void AddEntities(const DynArray<EntityId>& ents)
	{
		for (DynArray<EntityId>::const_iterator it = ents.begin(); it != ents.end(); ++it)
		{
			SNetObjectID objId = m_pView->ContextState()->GetNetID(*it, false);
			if (objId)
				m_locks.push_back(m_pView->ContextState()->LockObject(objId, "PBRK"));
		}
	}
};

class CServerContextView::CBeginBreakStream : public INetSendable
{
public:
	CBeginBreakStream(int id) : INetSendable(eMPF_BlocksStateChange, eNRT_ReliableUnordered), m_id(id)
	{
	}

#if ENABLE_PACKET_PREDICTION
	SMessageTag GetMessageTag(INetSender* pSender, IMessageMapper* mapper)
	{
		SMessageTag mTag;
		mTag.messageId = mapper->GetMsgId(CClientContextView::BeginBreakStream);
		mTag.varying1 = 0;
		mTag.varying2 = 0;
		return mTag;
	}
#endif

	EMessageSendResult Send(INetSender* pSender)
	{
		pSender->BeginMessage(CClientContextView::BeginBreakStream);
		SOnlyBreakId(m_id).SerializeWith(pSender->ser);
		return eMSR_SentOk;
	}

	void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate state)
	{
	}

	void GetPositionInfo(SMessagePositionInfo& pos)
	{
	}

	size_t GetSize()
	{
		return sizeof(*this);
	}

	const char* GetDescription()
	{
		return "BeginBreakStream";
	}

private:
	int m_id;
};

#if SERVER_FILE_SYNC_MODE == 0
	#define IF_SERVER_FILE_SYNC(true_val, false_val) (false_val)
#elif SERVER_FILE_SYNC_MODE == 1
	#define IF_SERVER_FILE_SYNC(true_val, false_val) (true_val)
#else
	#define IF_SERVER_FILE_SYNC(true_val, false_val) (ServerFileSyncEnabled() ? (true_val) : (false_val))
#endif

CServerContextView::CServerContextView(CNetChannel* pChannel, CNetContext* pContext)
{
	SetMMM(pChannel->GetChannelMMM());
	SContextViewConfiguration config = {
		CClientContextView::FlushMsgs,
		CClientContextView::ChangeState,
		CClientContextView::ForceNextState,
		CClientContextView::FinishState,
		CClientContextView::BeginUpdateObject,
		CClientContextView::EndUpdateObject,
		CClientContextView::ReconfigureObject,
		CClientContextView::SetAuthority,
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
		CClientContextView::VoiceData,
#else
		NULL,
#endif
		CClientContextView::RemoveStaticObject,
		CClientContextView::UpdatePhysicsTime,
		IF_SERVER_FILE_SYNC(CClientContextView::BeginSyncFiles,NULL),
		IF_SERVER_FILE_SYNC(CClientContextView::BeginSyncFile, NULL),
		IF_SERVER_FILE_SYNC(CClientContextView::AddFileData,   NULL),
		IF_SERVER_FILE_SYNC(CClientContextView::EndSyncFile,   NULL),
		IF_SERVER_FILE_SYNC(CClientContextView::AllFilesSynced,NULL),
		// partial update notification messages
		{
			CClientContextView::PartialAspect0,
			CClientContextView::PartialAspect1,
			CClientContextView::PartialAspect2,
			CClientContextView::PartialAspect3,
			CClientContextView::PartialAspect4,
			CClientContextView::PartialAspect5,
			CClientContextView::PartialAspect6,
			CClientContextView::PartialAspect7,
#if NUM_ASPECTS > 8
			CClientContextView::PartialAspect8,
			CClientContextView::PartialAspect9,
			CClientContextView::PartialAspect10,
			CClientContextView::PartialAspect11,
			CClientContextView::PartialAspect12,
			CClientContextView::PartialAspect13,
			CClientContextView::PartialAspect14,
			CClientContextView::PartialAspect15,
#endif//NUM_ASPECTS > 8
#if NUM_ASPECTS > 16
			CClientContextView::PartialAspect16,
			CClientContextView::PartialAspect17,
			CClientContextView::PartialAspect18,
			CClientContextView::PartialAspect19,
			CClientContextView::PartialAspect20,
			CClientContextView::PartialAspect21,
			CClientContextView::PartialAspect22,
			CClientContextView::PartialAspect23,
			CClientContextView::PartialAspect24,
			CClientContextView::PartialAspect25,
			CClientContextView::PartialAspect26,
			CClientContextView::PartialAspect27,
			CClientContextView::PartialAspect28,
			CClientContextView::PartialAspect29,
			CClientContextView::PartialAspect30,
			CClientContextView::PartialAspect31,
#endif//NUM_ASPECTS > 16
		},
		// set aspect profile messages
		{
			CClientContextView::SetAspectProfile0,
			CClientContextView::SetAspectProfile1,
			CClientContextView::SetAspectProfile2,
			CClientContextView::SetAspectProfile3,
			CClientContextView::SetAspectProfile4,
			CClientContextView::SetAspectProfile5,
			CClientContextView::SetAspectProfile6,
			CClientContextView::SetAspectProfile7,
#if NUM_ASPECTS > 8
			CClientContextView::SetAspectProfile8,
			CClientContextView::SetAspectProfile9,
			CClientContextView::SetAspectProfile10,
			CClientContextView::SetAspectProfile11,
			CClientContextView::SetAspectProfile12,
			CClientContextView::SetAspectProfile13,
			CClientContextView::SetAspectProfile14,
			CClientContextView::SetAspectProfile15,
#endif//NUM_ASPECTS > 8
#if NUM_ASPECTS > 16
			CClientContextView::SetAspectProfile16,
			CClientContextView::SetAspectProfile17,
			CClientContextView::SetAspectProfile18,
			CClientContextView::SetAspectProfile19,
			CClientContextView::SetAspectProfile20,
			CClientContextView::SetAspectProfile21,
			CClientContextView::SetAspectProfile22,
			CClientContextView::SetAspectProfile23,
			CClientContextView::SetAspectProfile24,
			CClientContextView::SetAspectProfile25,
			CClientContextView::SetAspectProfile26,
			CClientContextView::SetAspectProfile27,
			CClientContextView::SetAspectProfile28,
			CClientContextView::SetAspectProfile29,
			CClientContextView::SetAspectProfile30,
			CClientContextView::SetAspectProfile31,
#endif//NUM_ASPECTS > 16
		},
		// update aspect messages
		{
			CClientContextView::UpdateAspect0,
			CClientContextView::UpdateAspect1,
			CClientContextView::UpdateAspect2,
			CClientContextView::UpdateAspect3,
			CClientContextView::UpdateAspect4,
			CClientContextView::UpdateAspect5,
			CClientContextView::UpdateAspect6,
			CClientContextView::UpdateAspect7,
#if NUM_ASPECTS > 8
			CClientContextView::UpdateAspect8,
			CClientContextView::UpdateAspect9,
			CClientContextView::UpdateAspect10,
			CClientContextView::UpdateAspect11,
			CClientContextView::UpdateAspect12,
			CClientContextView::UpdateAspect13,
			CClientContextView::UpdateAspect14,
			CClientContextView::UpdateAspect15,
#endif//NUM_ASPECTS > 8
#if NUM_ASPECTS > 16
			CClientContextView::UpdateAspect16,
			CClientContextView::UpdateAspect17,
			CClientContextView::UpdateAspect18,
			CClientContextView::UpdateAspect19,
			CClientContextView::UpdateAspect20,
			CClientContextView::UpdateAspect21,
			CClientContextView::UpdateAspect22,
			CClientContextView::UpdateAspect23,
			CClientContextView::UpdateAspect24,
			CClientContextView::UpdateAspect25,
			CClientContextView::UpdateAspect26,
			CClientContextView::UpdateAspect27,
			CClientContextView::UpdateAspect28,
			CClientContextView::UpdateAspect29,
			CClientContextView::UpdateAspect30,
			CClientContextView::UpdateAspect31,
#endif//NUM_ASPECTS > 16
		},
#if ENABLE_ASPECT_HASHING
		// update aspect messages
		{
			CClientContextView::HashAspect0,
			CClientContextView::HashAspect1,
			CClientContextView::HashAspect2,
			CClientContextView::HashAspect3,
			CClientContextView::HashAspect4,
			CClientContextView::HashAspect5,
			CClientContextView::HashAspect6,
			CClientContextView::HashAspect7,
	#if NUM_ASPECTS > 8
			CClientContextView::HashAspect8,
			CClientContextView::HashAspect9,
			CClientContextView::HashAspect10,
			CClientContextView::HashAspect11,
			CClientContextView::HashAspect12,
			CClientContextView::HashAspect13,
			CClientContextView::HashAspect14,
			CClientContextView::HashAspect15,
	#endif//NUM_ASPECTS > 8
	#if NUM_ASPECTS > 16
			CClientContextView::HashAspect16,
			CClientContextView::HashAspect17,
			CClientContextView::HashAspect18,
			CClientContextView::HashAspect19,
			CClientContextView::HashAspect20,
			CClientContextView::HashAspect21,
			CClientContextView::HashAspect22,
			CClientContextView::HashAspect23,
			CClientContextView::HashAspect24,
			CClientContextView::HashAspect25,
			CClientContextView::HashAspect26,
			CClientContextView::HashAspect27,
			CClientContextView::HashAspect28,
			CClientContextView::HashAspect29,
			CClientContextView::HashAspect30,
			CClientContextView::HashAspect31,
	#endif//NUM_ASPECTS > 16
		},
#endif
		// rmi messages
		{
			CClientContextView::RMI_ReliableOrdered,
			CClientContextView::RMI_ReliableUnordered,
			CClientContextView::RMI_UnreliableOrdered,
			NULL,
			// must be last
			CClientContextView::RMI_Attachment,
		}
	};

	MMM_REGION(m_pMMM);

	m_pBreakSegmentStreams.reset(new TBreakSegmentStreams);
	m_pValidatedPredictions.reset(new TValidatedPredictionMap);
	m_pPendingUnbinds.reset(new TPendingUnbinds);

	for (int i = 0; i < MAX_BREAK_STREAMS; i++)
	{
		m_breakStreamHandles[i].hdl = SSendableHandle();
		m_breakStreamHandles[i].id = i;
	}

#ifdef __WITH_PB__
	m_clientHasPunkBuster = false;
#endif

	Init(pChannel, pContext, &config);
}

CServerContextView::~CServerContextView()
{
	MMM_REGION(m_pMMM);
	m_pBreakSegmentStreams.reset();
	m_pValidatedPredictions.reset();
	m_pPendingUnbinds.reset();
}

void CServerContextView::DefineProtocol(IProtocolBuilder* pBuilder)
{
	DefineExtensionsProtocol(pBuilder);
	pBuilder->AddMessageSink(this,
	                         CClientContextView::GetProtocolDef(),
	                         CServerContextView::GetProtocolDef());
}

void CServerContextView::CompleteInitialization()
{
	CContextView::CompleteInitialization();
	SetName("Server_" + RESOLVER.ToString(Parent()->GetIP()));
}

void CServerContextView::GC_BindObject(SNetObjectID netID, CNetObjectBindLock lk, CChangeStateLock cslk)
{
	ASSERT_GLOBAL_LOCK;

	MMM_REGION(m_pMMM);

	if (IsDead())
		return;

	//const SContextObject * pObj = ContextState()->GetContextObject(netID);
	SContextObjectRef obj = ContextState()->GetContextObject(netID);
	if (!obj.main || !obj.main->userID)
	{
		// TODO: should try to find out how this can happen and eliminate
		// the possibility
#if ENABLE_DEBUG_KIT
		NetWarning("Trying to bind non-existant object: ignoring (netID was %s)", netID.GetText());
#endif
		if (obj.main)
			UnboundObject(netID);
		return;
	}

	SSendableHandle after;
	if (obj.main->parent)
	{
		if (m_objects.size() <= obj.main->parent.id || !IsObjectBound(obj.main->parent))
		{
			//const SContextObject * pParentObj = ContextState()->GetContextObject(obj.main->parent);
			SContextObjectRef parentObj = ContextState()->GetContextObject(obj.main->parent);
			if (!parentObj.main || !parentObj.main->userID)
			{
#if ENABLE_DEBUG_KIT
				NetWarning("Trying to bind object with non-existing parent: ignoring (name was %s)", obj.main->GetName());
#endif
				UnboundObject(netID);
				return;
			}
			else
			{
				Parent()->NetAddSendable(new CWaitForEnabled(obj.main->parent, this), 0, NULL, &after);
			}
		}
		if (!IsObjectEnabled(obj.main->parent))
		{
			if (IsLocal())
				Parent()->NetAddSendable(new CWaitForEnabled(obj.main->parent, this), 0, NULL, &after);
			else if (m_objects[obj.main->parent.id].msgHandle)
				after = m_objects[obj.main->parent.id].msgHandle;
			else
				Parent()->NetAddSendable(new CWaitForEnabled(obj.main->parent, this), 0, NULL, &after);
		}
	}

	StrongCaptureRMIs scrmi(Parent());

	bool bStatic = obj.main->spawnType == eST_Static;
	NET_ASSERT(obj.main->spawnType != eST_Collected);
	if (!IsLocal())
	{
		TValidatedPredictionMap::iterator iterPred = m_pValidatedPredictions->find(obj.main->userID);
		EntityId prediction = 0;
		if (iterPred != m_pValidatedPredictions->end())
		{
			prediction = iterPred->second;
			m_pValidatedPredictions->erase(iterPred);
		}

		INetSendableHookPtr pSpawnHook = NULL;
		if (ContextState()->GetGameContext())
			pSpawnHook = ContextState()->GetGameContext()->CreateObjectSpawner(obj.main->userID, Parent());
		if (!pSpawnHook)
		{
#if ENABLE_DEBUG_KIT
			NetWarning("Could not create spawner for object; not binding");
#endif
			UnboundObject(netID);
			return;
		}

#if RESERVE_UNBOUND_ENTITIES
		pSpawnHook->SetPartialNetID(netID.id);
#endif

		if (prediction || bStatic)
			pSpawnHook = 0;
		_smart_ptr<CUpdateMessage> pSend = new CBindObjectMessage(netID, pSpawnHook, this, prediction, lk, cslk);
		uint32 grp = m_objects[netID.id].authority ? obj.xtra->scheduler_owned : obj.xtra->scheduler_normal;
		pSend->SetGroup(grp);
		Parent()->NetAddSendable(&*pSend, 1, &after, &m_objects[netID.id].msgHandle);
		pSend->SetHandle(m_objects[netID.id].msgHandle);
		m_objects[netID.id].activeHandle = m_objects[netID.id].msgHandle;
#if ENABLE_URGENT_RMIS
		// Cache the bind handle so that it can be used as a dependency for unordered RMIs,
		// guaranteeing the object exists.
		// The orderedRMIHandle is also seeded with the bind handle so that the first ordered
		// RMI is dependent on the bind.
		m_objects[netID.id].bindHandle = m_objects[netID.id].orderedRMIHandle = m_objects[netID.id].msgHandle;
#endif // ENABLE_URGENT_RMIS

		if (ContextState()->GetGameContext())
		{
			ContextState()->GetGameContext()->ObjectInitClient(obj.main->userID, Parent());
		}

#if ENABLE_DEBUG_KIT
		if (CVARS.LogLevel)
		{
			char buf1[64], buf2[64], buf3[64];
			NetLog("Sending spawn for object %s. userID=%d netID=%s static=%d [handle=%s after=%s]", obj.main->GetName(), obj.main->userID, netID.GetText(buf1), bStatic, m_objects[netID.id].msgHandle.GetText(buf2), after.GetText(buf3));
		}
#endif
	}
	else
	{
		SetSpawnState(netID, eSS_Enabled);
	}
}

void CServerContextView::OnObjectEvent(CNetContextState* pState, SNetObjectEvent* pEvent)
{
	if (pState == ContextState())
	{
		MMM_REGION(m_pMMM);

		switch (pEvent->event)
		{
		case eNOE_ValidatePrediction:
			m_pValidatedPredictions->insert(std::make_pair(pEvent->predictedEntity, pEvent->predictedReference));
			break;
		case eNOE_RemoveStaticEntity:
			RemoveStaticEntity(pEvent->userID);
			break;
		case eNOE_SyncWithGame_Start:
			if (m_lockLocalMapLoaded.IsLocking() && ContextState()->IsContextEstablished())
				m_lockLocalMapLoaded = CChangeStateLock();
			break;
		}
	}

	CContextView::OnObjectEvent(pState, pEvent);
}

void CServerContextView::ClearAllState()
{
	MMM_REGION(m_pMMM);
	CContextView::ClearAllState();
	m_pValidatedPredictions->clear();
	m_pPendingUnbinds->clear();
}

void CServerContextView::InitSessionIDs()
{
#if ENABLE_SESSION_IDS
	class CSessionIDsMessage : public INetMessage
	{
	public:
		CSessionIDsMessage(CNetContext* pContext) : INetMessage(CClientContextView::InitSessionData), m_sessionID(pContext->GetSessionID())
		{
		}

		EMessageSendResult WritePayload(TSerialize ser, uint32, uint32)
		{
			m_sessionID.SerializeWith(ser);
			return eMSR_SentOk;
		}

		void UpdateState(uint32, ENetSendableStateUpdate)
		{
		}

		virtual size_t GetSize() { return sizeof(*this); }

	private:
		CSessionID m_sessionID;
	};

	Parent()->NetAddSendable(new CSessionIDsMessage(Context()), 0, NULL, NULL);
#endif
}

bool CServerContextView::EnterState(EContextViewState state)
{
	switch (state)
	{
	case eCVS_Initial:
		//ClearAllState();
		InitSessionIDs();
		SendAuthChecks();
		FinishLocalState();
		break;
	case eCVS_Begin:
		FinishLocalState();
#ifdef __WITH_PB__
		m_clientHasPunkBuster = false;
#endif
		break;
	case eCVS_EstablishContext:
		NET_ASSERT(!m_lockLocalMapLoaded.IsLocking());
		NET_ASSERT(!m_lockRemoteMapLoaded.IsLocking());
		if (!IsLocal())
			m_lockRemoteMapLoaded = CChangeStateLock(this, "REMOTE_MAP");
		if (!ContextState()->IsContextEstablished())
			m_lockLocalMapLoaded = CChangeStateLock(this, "LOCAL_MAP");
#ifdef __WITH_PB__
		if (!IsLocal())
		{
			if (const char* failureReason = PbAuthClient(
			      const_cast<char*>(RESOLVER.ToNumericString(Parent()->GetIP()).c_str()),
			      int(m_clientHasPunkBuster != 0),
			      const_cast<char*>(Parent()->GetCDKeyHash().c_str())))
			{
				Parent()->Disconnect(eDC_PunkDetected, failureReason);
				return false;
			}
		}
#endif
		if (m_pAuth.get())
		{
			NetLog("Network password incorrect");
#if 0
			Parent()->Disconnect(eDC_AuthenticationFailed, "Password incorrect");
			return false;
#endif
		}
		FinishLocalState();
		break;
	case eCVS_ConfigureContext:
		FinishLocalState();
		break;
	case eCVS_SpawnEntities:
		FinishLocalState();
		break;
	case eCVS_PostSpawnEntities:
		FinishLocalState();
		break;
	case eCVS_InGame:
		break;
	}

	return CContextView::EnterState(state);
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CServerContextView, ChangeState, eNRT_ReliableUnordered, eMPF_BlocksStateChange | eMPF_StateChange)
{
	return SetRemoteState(param.state);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, FinishState, eNRT_ReliableUnordered, eMPF_BlocksStateChange | eMPF_StateChange)
{
	return FinishRemoteState();
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect0, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(0, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect1, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(1, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect2, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(2, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect3, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(3, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect4, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(4, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect5, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(5, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect6, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(6, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect7, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(7, ser, nCurSeq, nOldSeq);
}
#if NUM_ASPECTS > 8
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect8, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(8, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect9, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(9, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect10, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(10, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect11, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(11, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect12, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(12, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect13, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(13, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect14, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(14, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect15, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(15, ser, nCurSeq, nOldSeq);
}
#endif//NUM_ASPECTS > 8
#if NUM_ASPECTS > 16
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect16, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(16, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect17, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(17, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect18, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(18, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect19, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(19, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect20, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(20, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect21, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(21, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect22, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(22, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect23, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(23, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect24, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(24, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect25, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(25, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect26, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(26, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect27, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(27, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect28, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(28, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect29, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(29, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect30, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(30, ser, nCurSeq, nOldSeq);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, UpdateAspect31, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return UpdateAspect(31, ser, nCurSeq, nOldSeq);
}
#endif//NUM_ASPECTS > 16

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, RMI_UnreliableOrdered, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return HandleRMI(ser, eNRT_UnreliableOrdered, 0);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, RMI_ReliableUnordered, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	return HandleRMI(ser, eNRT_ReliableUnordered, 0);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, RMI_ReliableOrdered, eNRT_ReliableOrdered, eMPF_BlocksStateChange)
{
	return HandleRMI(ser, eNRT_ReliableOrdered, 0);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, RMI_Attachment, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return HandleRMI(ser, eNRT_NumReliabilityTypes, false);
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, BeginUpdateObject, eNRT_ReliableUnordered, eMPF_BlocksStateChange | eMPF_AfterSpawning)
{
	if (!ReadCurrentObjectID(ser, false))
	{
		Parent()->Disconnect(eDC_ContextCorruption, "BeginUpdateObject");
		return false;
	}
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, EndUpdateObject, eNRT_UnreliableOrdered, 0)
{
	return ClearCurrentObjectID();
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CServerContextView, AuthenticateResponse, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	if (gEnv->bMultiplayer)     // We only care about authentication in multiplayer
	{
		if ((m_pAuth.get() == NULL))
		{
			Parent()->Disconnect(eDC_ProtocolError, "Authentication response with no authentication ticket");
			return false;
		}

		if ((param != m_pAuth->Hash(Password())))
		{
			Parent()->Disconnect(eDC_AuthenticationFailed, "Authentication password is wrong");
			m_pAuth.reset();
			return false;
		}
	}

	m_pAuth.reset();
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, ClientEstablishedContext, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	if (!IsInState(eCVS_EstablishContext))
	{
		NetWarning("ClientEstablishedContext received in wrong state!");
		return 0;
	}

	if (!m_lockRemoteMapLoaded.IsLocking())
	{
		NetWarning("Too many established context messages received");
		return false;
	}
	m_lockRemoteMapLoaded = CChangeStateLock();
	return true;
}

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, VoiceData, eNRT_UnreliableOrdered, 0)
{
	SNetObjectID id;
	TVoicePacketPtr pkt = CVoicePacket::Allocate();

	ser.Value("object", id, 'eid');
	pkt->Serialize(ser);
	ReceivedVoice(id);
	if (Context() && Context()->GetVoiceContextImpl())
		Context()->GetVoiceContextImpl()->OnPacketFrom(id, eVD_From, pkt);

	return true;
}
#endif

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CServerContextView, BoundCollectedObject, eNRT_ReliableUnordered, 0)
{
	SetSpawnState(param.netID, eSS_Enabled);
	PolluteObjectAspect(param.netID, 3);
	return true;
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CServerContextView, SkippedCollectedObject, eNRT_ReliableUnordered, 0)
{
#if 0 // keeping around just in case things break spectacularly
	#if ENABLE_DEBUG_KIT
	if (CNetCVars::Get().DisconnectOnUncollectedBreakage)
		Parent()->Disconnect(eDC_ContextCorruption, "Uncollected collected objects");
	else
	#endif
	ContextState()->ForceUnbindObject(param.netID);
#else
	Parent()->Disconnect(eDC_ContextCorruption, "Uncollected collected objects");
#endif

	return true;
}

#if ENABLE_ASPECT_HASHING
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect0, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(0, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect1, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(1, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect2, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(2, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect3, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(3, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect4, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(4, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect5, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(5, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect6, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(6, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect7, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(7, ser);
	return true;
}
	#if NUM_ASPECTS > 8
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect8, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(8, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect9, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(9, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect10, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(10, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect11, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(11, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect12, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(12, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect13, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(13, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect14, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(14, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect15, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(15, ser);
	return true;
}
	#endif//NUM_ASPECTS > 8
	#if NUM_ASPECTS > 16
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect16, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(16, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect17, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(17, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect18, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(18, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect19, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(19, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect20, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(20, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect21, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(21, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect22, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(22, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect23, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(23, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect24, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(24, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect25, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(25, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect26, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(26, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect27, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(27, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect28, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(28, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect29, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(29, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect30, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(30, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, HashAspect31, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	HashAspect(31, ser);
	return true;
}
	#endif//NUM_ASPECTS > 16

#endif

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect0, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(0, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect1, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(1, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect2, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(2, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect3, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(3, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect4, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(4, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect5, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(5, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect6, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(6, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect7, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(7, ser);
	return true;
}
#if NUM_ASPECTS > 8
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect8, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(8, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect9, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(9, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect10, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(10, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect11, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(11, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect12, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(12, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect13, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(13, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect14, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(14, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect15, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(15, ser);
	return true;
}
#endif//NUM_ASPECTS > 8
#if NUM_ASPECTS > 16
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect16, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(16, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect17, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(17, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect18, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(18, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect19, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(19, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect20, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(20, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect21, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(21, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect22, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(22, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect23, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(23, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect24, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(24, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect25, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(25, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect26, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(26, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect27, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(27, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect28, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(28, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect29, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(29, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect30, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(30, ser);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CServerContextView, PartialAspect31, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	PartialAspect(31, ser);
	return true;
}
#endif//NUM_ASPECTS > 16

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CServerContextView, UpdatePhysicsTime, eNRT_UnreliableUnordered, 0)
{
	return SetPhysicsTime(param.tm);
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CServerContextView, CompletedUnbindObject, eNRT_ReliableUnordered, 0)
{
	m_pPendingUnbinds->erase(param.netID);
	return true;
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CServerContextView, SetNickname, eNRT_ReliableUnordered, 0)
{
	Parent()->SetNickname(param.m_nick);
	return true;
}

#ifdef __WITH_PB__
NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CServerContextView, InitPunkBuster, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	m_clientHasPunkBuster = param.clientHasPunkBuster;
	return true;
}
#endif

void CServerContextView::ChangeContext()
{
	if (IsPastOrInState(eCVS_Begin))
		PushForcedState(eCVS_Initial, true);
	if (IsInState(eCVS_InGame))
		FinishLocalState();
	CContextView::ChangeContext();
}

void CServerContextView::EstablishedContext()
{
	m_lockLocalMapLoaded = CChangeStateLock();
}

void CServerContextView::BindObject(SNetObjectID nID)
{
	const SContextObject* pCtxObj = ContextState()->GetContextObject(nID).main;

	if ((pCtxObj == NULL) || (!pCtxObj->userID))
	{
		return;
	}

	CContextView::BindObject(nID);

	TValidatedPredictionMap::iterator itPred = m_pValidatedPredictions->find(pCtxObj->userID);
	if (itPred != m_pValidatedPredictions->end())
	{
		m_objects[nID.id].predictionHandle = itPred->second;
	}

	if (pCtxObj->spawnType == eST_Collected)
		return;

	TO_GAME(&CServerContextView::GC_BindObject, this, nID, ContextState()->LockObject(nID, "SENDBIND"), CChangeStateLock(this, "SENDBIND"));
}

void CServerContextView::UnbindObject(SNetObjectID nID)
{
	CNetObjectBindLock lk = ContextState()->LockObject(nID, "UBND");

	if (CContextView::DoUnbindObject(nID, false))
	{
		SContextViewObject& cvo = m_objects[nID.id];
		if (cvo.predictionHandle && cvo.spawnState == eSS_Unspawned)
		{
			SRemoveStaticObject rso;
			rso.id = cvo.predictionHandle;
			m_pPendingUnbinds->insert(std::make_pair(nID, lk));
			CClientContextView::SendUnbindPredictedObjectWith(rso, Parent());
		}
		else
		{
			SendUnbindMessage(nID, false, lk);
		}
	}
}

const char* CServerContextView::ValidateMessage(const SNetMessageDef* pMsg, bool bNetworkMsg)
{
	if (!bNetworkMsg && pMsg->reliability == eNRT_UnreliableOrdered)
		return "Must send reliable messages until in game";
	return NULL;
}

bool CServerContextView::HasRemoteDef(const SNetMessageDef* pDef)
{
	return CClientContextView::ClassHasDef(pDef);
}

void CServerContextView::GetMemoryStatistics(ICrySizer* pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "CServerContextView");

	MMM_REGION(m_pMMM);
	pSizer->Add(*this);
	CContextView::GetMemoryStatistics(pSizer);
	if (m_pAuth.get())
		pSizer->Add(*m_pAuth);
	pSizer->AddContainer(*m_pBreakSegmentStreams);
	pSizer->AddContainer(*m_pValidatedPredictions);
#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)
	pSizer->AddContainer(m_tempPackets);
	for (size_t i = 0; i < m_tempPackets.size(); ++i)
		pSizer->AddObject(m_tempPackets[i].second.get(), sizeof(CVoicePacket));
	pSizer->AddContainer(m_pVoiceListeners);
#endif
}

void CServerContextView::SendUnbindMessage(SNetObjectID netID, bool bFromBind, CNetObjectBindLock lk)
{
	if (!IsLocal())
	{
		NET_ASSERT(m_dependencyStaging.empty());
		m_dependencyStaging.push_back(m_objects[netID.id].msgHandle);
#if ENABLE_ASPECT_HASHING
		if (Context()->IsMultiplayer())
		{
			for (NetworkAspectID i = 0; i < NumAspects; i++)
				m_dependencyStaging.push_back(m_objectsEx[netID.id].hashMsgHandles[i]);
		}
#endif
		GetSendablesDependentOnObject(netID, m_dependencyStaging);

		Parent()->AddSendable(new CUnbindObjectMessage(netID, this, lk), m_dependencyStaging.size(), m_dependencyStaging.empty() ? NULL : &m_dependencyStaging[0], NULL);
		m_dependencyStaging.resize(0);
	}
}

void CServerContextView::SendAuthChecks()
{
	//if (!Password().empty())
	{
		m_pAuth.reset(new SAuthenticationSalt());
		CClientContextView::SendAuthenticateChallengeWith(*m_pAuth, Parent());
	}
}

class CBreakageSendableSink : public INetSendableSink
{
public:
	CBreakageSendableSink(CServerContextView* pView, SSendableHandle* pHandle) : m_pChannel(pView->Parent()), m_pView(pView), m_pHandle(pHandle) {}

	virtual void NextRequiresEntityEnabled(EntityId id)
	{
		std::map<EntityId, SSendableHandle>::iterator it = m_waitHandles.lower_bound(id);
		if (it == m_waitHandles.end() || it->first != id)
		{
			SNetObjectID netid = m_pView->ContextState()->GetNetID(id);
			if (netid)
			{
				SSendableHandle hdl;
				m_pChannel->NetAddSendable(new CWaitForEnabled(netid, m_pView), 0, 0, &hdl);
				m_waitHandles.insert(it, std::make_pair(id, hdl));
			}
		}
		stl::push_back_unique(m_entities, id);
	}

	virtual void SendMsg(INetSendable* pSnd)
	{
		INetSendablePtr pSendable = CheckNullEntityWrites(pSnd);

		if (m_waitHandles.empty())
			m_pChannel->NetAddSendable(pSendable, 1, m_pHandle, m_pHandle);
		else
		{
			std::vector<SSendableHandle> waits;
			waits.reserve(m_waitHandles.size() + 1);
			waits.push_back(*m_pHandle);
			for (std::map<EntityId, SSendableHandle>::iterator it = m_waitHandles.begin(); it != m_waitHandles.end(); ++it)
				waits.push_back(it->second);
			m_pChannel->NetAddSendable(pSendable, waits.size(), &waits[0], m_pHandle);
			m_waitHandles.clear();
		}
	}

	const DynArray<EntityId>& GetEntities() { return m_entities; }

private:
	CNetChannel*                        m_pChannel;
	CServerContextView*                 m_pView;
	SSendableHandle*                    m_pHandle;

	std::map<EntityId, SSendableHandle> m_waitHandles;
	DynArray<EntityId>                  m_entities;
};

void CServerContextView::GotBreakage(const SNetIntBreakDescription* pDesc)
{
	MMM_REGION(m_pMMM);

	if (IsLocal())
		return;

	const int flags = pDesc->flags;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (flags & eNBF_UseSimpleSend)
	{
		CPerformBreakSimpleServer::GotBreakage(this, pDesc);
		return;
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	BreakSegmentID minSegId, maxSegId;
	SSendableHandle& hdl = m_breakStreamHandles[0].hdl;

	static const float BREAK_SEGMENT_SIZE = 20.0f;

	AABB affected;
	pDesc->pMessagePayload->GetAffectedRegion(affected);
	minSegId = BreakSegmentID(affected.min / BREAK_SEGMENT_SIZE);
	maxSegId = BreakSegmentID(affected.max / BREAK_SEGMENT_SIZE + BreakSegmentID(1));

	std::vector<SSendableHandle, STLMementoAllocator<SSendableHandle>> waitForInitial;
	if (hdl)
		waitForInitial.push_back(hdl);
	for (int i = minSegId.x; i <= maxSegId.x; i++)
		for (int j = minSegId.y; j <= maxSegId.y; j++)
			for (int k = minSegId.z; k <= maxSegId.z; k++)
				if (SSendableHandle bhdl = (*m_pBreakSegmentStreams)[BreakSegmentID(i, j, k)])
					waitForInitial.push_back(bhdl);
	std::sort(waitForInitial.begin(), waitForInitial.end());
	waitForInitial.resize(std::unique(waitForInitial.begin(), waitForInitial.end()) - waitForInitial.begin());

	if (waitForInitial.size() > 0)
	{
		Parent()->NetAddSendable(new CBeginBreakStream(m_breakStreamHandles[0].id), waitForInitial.size(), &waitForInitial[0], &hdl);
	}
	else
	{
		Parent()->NetAddSendable(new CBeginBreakStream(m_breakStreamHandles[0].id), 0, NULL, &hdl);
	}

	if (CNetCVars::Get().breakageSyncEntities)
	{
		for (int i = 0; i < pDesc->spawnedObjects.size(); i++)
		{
			EntityId ent = pDesc->spawnedObjects[i];
			SNetObjectID id = ContextState()->GetNetID(ent);
			//const SContextObject * pObj = ContextState()->GetContextObject(id);
			SContextObjectRef obj = ContextState()->GetContextObject(id);
			if (id && obj.main && obj.main->spawnType == eST_Collected)
			{
				Parent()->NetAddSendable(new CDeclareBrokenProductMessage(id, m_breakStreamHandles[0].id, this), 1, &hdl, &hdl);
			}
			else
			{
				NetLog("[brk] failed to call CDeclareBrokenProductMessage id=%s, main=%p, spawnType=%d", id.GetText(), obj.main, obj.main ? obj.main->spawnType : -1);
			}
		}
	}

	LockStateChanges("BREAK");
	CBreakageSendableSink sink(this, &hdl);
	pDesc->pMessagePayload->AddSendables(&sink, m_breakStreamHandles[0].id);

	_smart_ptr<CPerformBreak> pPB = new CPerformBreak(this, m_breakStreamHandles[0].id, pDesc);
	Parent()->NetAddSendable(&*pPB, 1, &hdl, &hdl);
	pPB->SetHandle(hdl, sink.GetEntities());

	// block radius around breakage
	for (int i = minSegId.x; i <= maxSegId.x; i++)
		for (int j = minSegId.y; j <= maxSegId.y; j++)
			for (int k = minSegId.z; k <= maxSegId.z; k++)
				(*m_pBreakSegmentStreams)[BreakSegmentID(i, j, k)] = hdl;

	// rotate break streams
	SBreakStreamHandle temp = m_breakStreamHandles[0];
	for (int i = 1; i < MAX_BREAK_STREAMS; i++)
		m_breakStreamHandles[i - 1] = m_breakStreamHandles[i];
	m_breakStreamHandles[MAX_BREAK_STREAMS - 1] = temp;
}

#if SERVER_FILE_SYNC_MODE
class CServerContextView::CCET_SyncFiles : public CCET_Base
{
public:
	CCET_SyncFiles(CContextViewPtr pView) : m_pView(pView) {}

	const char* GetName()
	{
		return "SyncFiles";
	}

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		SCOPED_GLOBAL_LOCK;
		m_pView->Context()->GetSyncedFileSet(true)->SendFilesTo(m_pView->Parent());
		return eCETR_Ok;
	}

private:
	CContextViewPtr m_pView;
};
#endif

void CServerContextView::InitChannelEstablishmentTasks(IContextEstablisher* pEst)
{
	if (!IsLocal())
	{
		AddPostSpawnObjects(pEst, eCVS_PostSpawnEntities);
#if SERVER_FILE_SYNC_MODE
		if (ServerFileSyncEnabled())
			pEst->AddTask(eCVS_Begin, new CCET_SyncFiles(this));
#endif
	}
}

void CServerContextView::OnWitnessDeclared()
{
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	if (CVoiceContext* pCtx = Context()->GetVoiceContextImpl())
	{
		pCtx->ConfigureCallback(this, eVD_From, GetWitness());
	}
#endif
}

void CServerContextView::RemoveStaticEntity(EntityId id)
{
	class CRemoveStaticObjectMessage : public INetMessage, private SRemoveStaticObject
	{
	public:
		CRemoveStaticObjectMessage(const SNetMessageDef* pDef, EntityId id) : INetMessage(pDef)
		{
			this->id = id;
		}

		EMessageSendResult WritePayload(TSerialize ser, uint32, uint32)
		{
			this->SerializeWith(ser);
			return eMSR_SentOk;
		}

		void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate)
		{
		}

		size_t GetSize()
		{
			return sizeof(*this);
		}
	};

	SNetObjectID objId = ContextState()->GetNetID(id);
	if (objId)
		if (IsObjectBound(objId))
			return;

	Parent()->NetAddSendable(new CRemoveStaticObjectMessage(CClientContextView::RemoveStaticObject, id), 0, NULL, NULL);
}

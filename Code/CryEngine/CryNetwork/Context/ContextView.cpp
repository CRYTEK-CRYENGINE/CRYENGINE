// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  context views replicate state between contexts
   -------------------------------------------------------------------------
   History:
   - 02/09/2004   12:34 : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"
#include "ContextView.h"
#include "NetContext.h"
#include "Protocol/NetChannel.h"
#include <CrySystem/ITimer.h>
#include <CryEntitySystem/IEntitySystem.h>
#include "Utils.h"
#include "Config.h"

#include "History/History.h"
#include "History/MementoHistory.h"
#include "History/ProfileHistory.h"
#include "History/AuthorityHistory.h"
#include "History/ConfigurationHistory.h"

#include "VoiceContext.h"
#include "Services/ServiceManager.h"
#include "IContextViewExtension.h"

#include "UpdateMessage.h"

static const unsigned StateMessages =
  eNOE_SyncWithGame_Start |
  eNOE_BindObject |
  eNOE_UnbindObject |
  eNOE_UnboundObject |
  eNOE_ObjectAspectChange |
  eNOE_ReconfiguredObject |
  eNOE_BindAspects |
  eNOE_UnbindAspects |
  eNOE_SetAuthority |
  eNOE_SetAspectProfile |
  eNOE_PartialUpdate |
  eNOE_GotBreakage;

static const unsigned ExcludeIfLocal =
  eNOE_ObjectAspectChange |
  eNOE_ReconfiguredObject |
  eNOE_BindAspects |
  eNOE_UnbindAspects |
  eNOE_RemoveStaticEntity |
  eNOE_GotBreakage;

static const unsigned TopLevelMessages =
  //#if ENABLE_DEBUG_KIT
  eNOE_SyncWithGame_Start |
  //#endif
  eNOE_Reset |
  eNOE_ChangeContext |
  eNOE_SendVoicePackets |
  eNOE_RemoveRMIListener |
  eNOE_DebugEvent |
  eNOE_ContextDestroyed;

static const unsigned EstablishedMessages =
  eNOE_EstablishedContext;

static const unsigned ContextViewEvents[] = {
	TopLevelMessages,                                       // eCVS_Initial
	TopLevelMessages,                                       // eCVS_Begin
	TopLevelMessages | EstablishedMessages,                 // eCVS_EstablishContext
	TopLevelMessages | EstablishedMessages,                 // eCVS_ConfigureContext
	TopLevelMessages | EstablishedMessages | StateMessages, // eCVS_SpawnEntities
	TopLevelMessages | EstablishedMessages | StateMessages, // eCVS_PostSpawnEntities
	TopLevelMessages | EstablishedMessages | StateMessages, // eCVS_InGame
};

class CContextView::CRMIMessage_Script : public INetSendable
{
	friend class CContextView::CRMIMessageAllocator;

	CRMIMessage_Script(const SNetMessageDef* pDef, IRMIMessageBodyPtr pBody, CContextViewPtr pView, bool blockStateChange) :
		INetSendable(CalcFlags(pDef->parallelFlags, blockStateChange), pDef->reliability), m_pBody(pBody), m_pView(pView), m_pDef(pDef)
	{
		++g_objcnt.rmiScript;
		CNetContextState* pCtx = pView->ContextState();
		NET_ASSERT(pCtx);
		//const SContextObject * pObj = pCtx->GetContextObject( pCtx->GetNetID(m_pBody->objId) );
		SContextObjectRef obj = pCtx->GetContextObject(pCtx->GetNetID(m_pBody->objId));
		if (obj.main)
			SetGroup(obj.xtra->scheduler_normal);
	}

	~CRMIMessage_Script()
	{
		--g_objcnt.rmiScript;
	}

	void DeleteThis();

public:
	uint32 CalcFlags(uint32 flags, bool blockStateChange)
	{
		if (blockStateChange)
			flags |= eMPF_BlocksStateChange;
		else
			flags &= ~eMPF_BlocksStateChange;
		return flags;
	}

#if ENABLE_PACKET_PREDICTION
	SMessageTag GetMessageTag(INetSender* pSender, IMessageMapper* mapper)
	{
		SMessageTag mTag;

		mTag.messageId = mapper->GetMsgId(m_pDef);
		mTag.varying1 = 0;
		mTag.varying2 = 0;

		return mTag;
	}
#endif

	EMessageSendResult Send(INetSender* pSender)
	{
		EntityId objId = m_pBody->objId;

		SNetObjectID netId = m_pView->ContextState()->GetNetID(objId);
		if (!netId)
			return eMSR_FailedMessage;
		//const SContextObject * pCtxObj = m_pView->ContextState()->GetContextObject(netId);
		SContextObjectRef ctxObj = m_pView->ContextState()->GetContextObject(netId);
		if (!ctxObj.main)
			return eMSR_FailedMessage;
		if (!ctxObj.main->userID)
			return eMSR_FailedMessage;

		uint8 funcId = m_pBody->funcId;

		pSender->BeginMessage(m_pDef);
		pSender->ser.Value("objID", objId, 'eid');
		pSender->ser.Value("funcID", funcId);
		m_pBody->SerializeWith(pSender->ser);
		return eMSR_SentOk;
	}

	void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate update)
	{
		if (update != eNSSU_Requeue)
		{
			NET_ASSERT(m_pBody != NULL);
			if (!m_pBody)
				return;
			NET_ASSERT(m_pView->ContextState());
			if (m_pView->ContextState())
			{
				SNetObjectID netId = m_pView->ContextState()->GetNetID(m_pBody->objId);
				NET_ASSERT(m_msgHandle && "Should not be zero");
				std::pair<TSendablesMap::iterator, TSendablesMap::iterator> range = m_pView->m_pSendables->equal_range(netId);
				for (TSendablesMap::iterator it = range.first; it != range.second; ++it)
				{
					if (it->second == m_msgHandle)
					{
						m_pView->m_pSendables->erase(it);
						break;
					}
				}
			}
		}
	}

	size_t GetSize()
	{
		return sizeof(*this) + m_pBody->GetSize();
	}

	void GetPositionInfo(SMessagePositionInfo& pos)
	{
		CNetContextState* pCtx = m_pView->ContextState();
		NET_ASSERT(pCtx);
		//const SContextObject * pObj = pCtx->GetContextObject( pCtx->GetNetID(m_pBody->objId) );
		SContextObjectRef obj = pCtx->GetContextObject(pos.obj = pCtx->GetNetID(m_pBody->objId));
#if FULL_ON_SCHEDULING
		if (obj.main)
		{
			pos.havePosition = obj.xtra->hasPosition;
			pos.position = obj.xtra->position;
			pos.haveDrawDistance = obj.xtra->hasDrawDistance;
			pos.drawDistance = obj.xtra->drawDistance;
		}
#endif
	}

	void SetMessageHandle(SSendableHandle hndl)
	{
		m_msgHandle = hndl;
	}

	void SetMementoHandle(TMemHdl hdl)
	{
		m_myHdl = hdl;
	}

	TMemHdl GetMementoHandle() const
	{
		return m_myHdl;
	}

	CContextViewPtr GetView()
	{
		return m_pView;
	}

	const char* GetDescription()
	{
		return m_pDef->description;
	}

private:
	IRMIMessageBodyPtr    m_pBody;
	CContextViewPtr       m_pView;
	SSendableHandle       m_msgHandle;
	const SNetMessageDef* m_pDef;
	TMemHdl               m_myHdl;
};

class CContextView::CRMIMessage_UserDef : public INetSendable
{
	friend class CContextView::CRMIMessageAllocator;

	CRMIMessage_UserDef(IRMIMessageBodyPtr pBody, CContextViewPtr pView, bool blockStateChange) :
		INetSendable(CalcFlags(pBody, blockStateChange), pBody->pMessageDef->reliability), m_pBody(pBody), m_pView(pView)
	{
		CNetContextState* pCtx = pView->ContextState();
		NET_ASSERT(pCtx);
		//const SContextObject * pObj = pCtx->GetContextObject( pCtx->GetNetID(m_pBody->objId) );
		SContextObjectRef obj = pCtx->GetContextObject(pCtx->GetNetID(m_pBody->objId));
		if (obj.main)
			SetGroup(obj.xtra->scheduler_normal);
		++g_objcnt.rmiCPP;
	}

	~CRMIMessage_UserDef()
	{
		--g_objcnt.rmiCPP;
	}

	void DeleteThis();

public:
	uint32 CalcFlags(IRMIMessageBody* pBody, bool blockStateChange)
	{
		uint32 flags = pBody->pMessageDef->parallelFlags;
		if (blockStateChange)
			flags |= eMPF_BlocksStateChange;
		else
			flags &= ~eMPF_BlocksStateChange;
		return flags;
	}

#if ENABLE_PACKET_PREDICTION
	SMessageTag GetMessageTag(INetSender* pSender, IMessageMapper* mapper)
	{
		SMessageTag mTag;

		mTag.messageId = mapper->GetMsgId(m_pBody->pMessageDef);
		mTag.varying1 = 0;
		mTag.varying2 = 0;

		return mTag;
	}
#endif

	EMessageSendResult Send(INetSender* pSender)
	{
		bool isAttached = (m_pBody->attachment == eRAT_PreAttach) || (m_pBody->attachment == eRAT_PostAttach);

		if (!isAttached)
		{
			EntityId objId = m_pBody->objId;

			SNetObjectID netId = m_pView->ContextState()->GetNetID(objId);
			if (!netId)
				return eMSR_FailedMessage;
			//const SContextObject * pCtxObj = m_pView->ContextState()->GetContextObject(netId);
			SContextObjectRef ctxObj = m_pView->ContextState()->GetContextObject(netId);
			if (!ctxObj.main)
				return eMSR_FailedMessage;
			if (!ctxObj.main->userID)
				return eMSR_FailedMessage;
			if (!m_pView->IsObjectBound(netId))
				return eMSR_FailedMessage;

			pSender->BeginUpdateMessage(netId);
		}

#if STATS_COLLECTOR
		const char* grp = m_pBody->pMessageDef->description;
		while (const char* grp_2 = strchr(grp, ':'))
			grp = grp_2 + 1;
#endif

		pSender->BeginMessage(m_pBody->pMessageDef);

#if STATS_COLLECTOR
		STATS.BeginGroup(grp);
#endif

		m_pBody->SerializeWith(pSender->ser);

		if (!isAttached)
			pSender->EndUpdateMessage();

#if STATS_COLLECTOR
		STATS.EndGroup();
#endif

		return eMSR_SentOk;
	}

	void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate bAck)
	{
		if (bAck != eNSSU_Requeue)
		{
			NET_ASSERT(m_pBody != NULL);
			if (!m_pBody)
				return;
			//cleanup
			if (m_pView->ContextState())
			{
				SNetObjectID netId = m_pView->ContextState()->GetNetID(m_pBody->objId);
				NET_ASSERT(m_msgHandle && "Should not be zero");
				std::pair<TSendablesMap::iterator, TSendablesMap::iterator> range = m_pView->m_pSendables->equal_range(netId);
				for (TSendablesMap::iterator it = range.first; it != range.second; ++it)
				{
					if (it->second == m_msgHandle)
					{
						m_pView->m_pSendables->erase(it);
						break;
					}
				}
			}
		}
	}

	size_t GetSize()
	{
		return sizeof(*this) + m_pBody->GetSize();
	}

	void GetPositionInfo(SMessagePositionInfo& pos)
	{
		CNetContextState* pCtx = m_pView->ContextState();
		NET_ASSERT(pCtx);
		//const SContextObject * pObj = pCtx->GetContextObject( pCtx->GetNetID(m_pBody->objId) );
		SContextObjectRef obj = pCtx->GetContextObject(pos.obj = pCtx->GetNetID(m_pBody->objId));
#if FULL_ON_SCHEDULING
		if (obj.main)
		{
			pos.havePosition = obj.xtra->hasPosition;
			pos.position = obj.xtra->position;
			pos.haveDrawDistance = obj.xtra->hasDrawDistance;
			pos.drawDistance = obj.xtra->drawDistance;
		}
#endif
	}

	const char* GetDescription()
	{
		return m_pBody->pMessageDef->description;
	}

#if ENABLE_RMI_BENCHMARK || ENABLE_URGENT_RMIS
	virtual IRMIMessageBody* GetRMIMessageBody()
	{
		return m_pBody.get();
	}
#endif

	void SetMessageHandle(SSendableHandle hndl)
	{
		m_msgHandle = hndl;
	}

	void SetMementoHandle(TMemHdl hdl)
	{
		m_myHdl = hdl;
	}

	TMemHdl GetMementoHandle() const
	{
		return m_myHdl;
	}

	CContextViewPtr GetView()
	{
		return m_pView;
	}

private:
	IRMIMessageBodyPtr m_pBody;
	CContextViewPtr    m_pView;
	SSendableHandle    m_msgHandle;
	TMemHdl            m_myHdl;
};

class CContextView::CRMIMessageAllocator
{
public:
	static ILINE CRMIMessage_Script* CreateScript(const SNetMessageDef* pDef, IRMIMessageBodyPtr pBody, CContextViewPtr pView, bool blockStateChange)
	{
		ASSERT_GLOBAL_LOCK;
		TMemHdl hdl = MMM().AllocHdl(sizeof(CRMIMessage_Script));
		CRMIMessage_Script* pRMI = new(MMM().PinHdl(hdl))CRMIMessage_Script(pDef, pBody, pView, blockStateChange);
		pRMI->SetMementoHandle(hdl);
		return pRMI;
	}
	static ILINE CRMIMessage_UserDef* CreateUserDef(IRMIMessageBodyPtr pBody, CContextViewPtr pView, bool blockStateChange)
	{
		ASSERT_GLOBAL_LOCK;
		TMemHdl hdl = MMM().AllocHdl(sizeof(CRMIMessage_Script));
		CRMIMessage_UserDef* pRMI = new(MMM().PinHdl(hdl))CRMIMessage_UserDef(pBody, pView, blockStateChange);
		pRMI->SetMementoHandle(hdl);
		return pRMI;
	}

	template<class T>
	static void Delete(T* p)
	{
		MMM_REGION(&p->GetView()->GetMMM());
		TMemHdl hdl = p->GetMementoHandle();
		p->~T();
		MMM().FreeHdl(hdl);
	}
};

void CContextView::CRMIMessage_Script::DeleteThis()
{
	CRMIMessageAllocator::Delete(this);
}

void CContextView::CRMIMessage_UserDef::DeleteThis()
{
	CRMIMessageAllocator::Delete(this);
}

class CContextView::CNotifyPartialUpdateMessage : public INetSendable
{
public:
	CNotifyPartialUpdateMessage(CContextViewPtr pView, SNetObjectID id, NetworkAspectID aspectIdx, const SNetMessageDef* pDef) :
		INetSendable(pDef->parallelFlags, pDef->reliability),
		m_pDef(pDef), m_pView(pView), m_id(id), m_aspectIdx(aspectIdx)
	{
		SetGroup('pupd');
		++g_objcnt.notifyPartialUpdate;
	}
	~CNotifyPartialUpdateMessage()
	{
		--g_objcnt.notifyPartialUpdate;
	}

	const char* GetDescription()
	{
		if (m_description.empty())
		{
			if (m_pView->ContextState() && m_pView->Context())
			{
				//const SContextObject * pObj = m_pView->ContextState()->GetContextObject(m_id);
				SContextObjectRef obj = m_pView->ContextState()->GetContextObject(m_id);
				if (!obj.main)
					m_description.Format("Partial Update Notification Illegal Object %s:%s", m_id.GetText(), m_pView->Context()->GetAspectName(m_aspectIdx));
				else
					m_description.Format("Partial Update Notification %s:%s", obj.main->GetName(), m_pView->Context()->GetAspectName(m_aspectIdx));
			}
			else
			{
				m_description.Format("Partial Update Notification Illegal Object %s:%u", m_id.GetText(), m_aspectIdx);
			}
		}
		NET_ASSERT(!m_description.empty());
		return m_description.c_str();
	}

	void GetPositionInfo(SMessagePositionInfo& pos)
	{
#if FULL_ON_SCHEDULING
		if (m_pView->IsDead())
			return;
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

#if ENABLE_PACKET_PREDICTION
	SMessageTag GetMessageTag(INetSender* pSender, IMessageMapper* mapper)
	{
		SMessageTag mTag;

		mTag.messageId = mapper->GetMsgId(m_pDef);
		mTag.varying1 = 0;
		mTag.varying2 = 0;

		return mTag;
	}
#endif

	EMessageSendResult Send(INetSender* pSender)
	{
		//const SContextObject * pObj = m_pView->ContextState()->GetContextObject(m_id);
		SContextObjectRef obj = m_pView->ContextState()->GetContextObject(m_id);
		if (!obj.main)
			return eMSR_FailedMessage;
		pSender->BeginMessage(m_pDef);
		pSender->ser.Value("obj", m_id, 'eid');
		return eMSR_SentOk;
	}

	void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate update)
	{
	}

	size_t GetSize()
	{
		return sizeof(*this);
	}

private:
	const SNetMessageDef* m_pDef;
	CContextViewPtr       m_pView;
	SNetObjectID          m_id;
	NetworkAspectID       m_aspectIdx;
	string                m_description;
};

struct CContextView::SExtensionAdder : public IContextViewExtensionAdder
{
	SExtensionAdder(CContextView* pCV) : m_pCV(pCV){}
	void AddExtension(IContextViewExtension* ext)
	{
		NET_ASSERT(ext);
		m_pCV->m_extensions.push_back(ext);
		ext->SetParent(m_pCV);
	}
	CContextView* m_pCV;
};

const char* CContextView::GetStateName(EContextViewState state)
{
#define STATE_NAME(n) case eCVS_ ## n: \
  return # n
	switch (state)
	{
		STATE_NAME(Initial);
		STATE_NAME(Begin);
		STATE_NAME(EstablishContext);
		STATE_NAME(ConfigureContext);
		STATE_NAME(PostSpawnEntities);
		STATE_NAME(SpawnEntities);
		STATE_NAME(InGame);
	}
#undef STATE_NAME
	return "unknown-state";
}

const char* CContextView::GetWaitStateName(EContextViewState state)
{
	static char buffer[eCVS_NUM_STATES][64];
	if (!buffer[state][0])
		cry_sprintf(buffer[state], "WaitFor_%s", GetStateName(state));
	return buffer[state];
}

void CContextView::SChangeStateMessage::SerializeWith(TSerialize ser)
{
	ser.EnumValue("state", state, eCVS_Initial, eCVS_NUM_STATES);
}

//
// CContextView
//

CContextView::CContextView() :
	m_pParent(NULL),
	m_pContext(NULL),
	m_nAttachmentIndex(0),
	m_bDead(false),
	m_currentState(eCVS_Initial),
	m_ignoringCurObject(false),
	m_remotePhysicsTime(0.0f),
	m_flushUpdates(false),
	m_allowVoice(true)
{
#if ENABLE_DEFERRED_RMI_QUEUE
	m_deferredRMI.reserve(DEFERRED_RMI_RESERVE_SIZE);
#endif // ENABLE_DEFERRED_RMI_QUEUE

	for (int i = 0; i < eH_NUM_HISTORIES; i++)
		m_history[i] = 0;
	++g_objcnt.contextView;
}

CContextView::~CContextView()
{
	// it can happen that this is called in response to a flush of the lazy game queue;
	// ensure we are locked in this case
	SCOPED_GLOBAL_LOCK;
	MMM_REGION(m_pMMM);

	ClearAllState();

	m_objectLocks.resize(0);
	m_objects.resize(0);
	m_objectsEx.resize(0);

	m_boundCache = SNetObjectID();
	m_enabledCache = SNetObjectID();

	for (int i = 0; i < eH_NUM_HISTORIES; i++)
	{
		SAFE_DELETE(m_history[i]);
	}

	for (uint32 i = 0; i < m_extensions.size(); i++)
		m_extensions[i]->Release();

	for (int i = 0; i < 2; i++)
		m_pAttachments[i].reset();
	m_pEarlyPartialUpdateIDs.reset();
	m_pSendables.reset();

	--g_objcnt.contextView;
}

void CContextView::Die()
{
	if (m_bDead)
		return;

	ClearAllState();

	if (m_pContext)
	{
		SNetChannelEvent evt;
		evt.event = eNCE_ChannelDestroyed;
		ContextState()->BroadcastChannelEvent(Parent(), &evt);

		ContextState()->ChangeSubscription(this, 0);

		TNetAddressVec ips;
		GetLocalIPs(ips);
		m_pContext->DeregisterLocalIPs(ips);

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
		if (GetWitness() && m_pContext && Context()->GetVoiceContextImpl())
		{
			Context()->GetVoiceContextImpl()->ConfigureCallback(0, eVD_To, GetWitness());
			Context()->GetVoiceContextImpl()->ConfigureCallback(0, eVD_From, GetWitness());
		}
#endif
	}
	m_objectLocks.resize(0);
	m_objects.resize(0);
	m_objectsEx.resize(0);
	m_boundCache = SNetObjectID();
	m_enabledCache = SNetObjectID();
	Parent()->ChangeSubscription(this, 0);

	for (uint32 i = 0; i < m_extensions.size(); i++)
		m_extensions[i]->Die();

	m_bDead = true;

	m_pContextState = NULL;
	m_pContext = NULL;
}

void CContextView::OnObjectEvent(CNetContextState* pState, SNetObjectEvent* pEvent)
{
	MMM_REGION(m_pMMM);

	if (pState == ContextState())
	{
		switch (pEvent->event)
		{
		case eNOE_BindObject:
			BindObject(pEvent->id);
			break;
		case eNOE_UnbindObject:
			UnbindObject(pEvent->id);
			break;
		case eNOE_UnboundObject:
			UnboundObject(pEvent->id);
			break;
		case eNOE_Reset:
			break;
		case eNOE_ObjectAspectChange:
			HandleAspectChanges(pEvent->pChanges);
			break;
		case eNOE_ChangeContext:
			ChangeContext();
			break;
		case eNOE_EstablishedContext:
			EstablishedContext();
			break;
		case eNOE_ReconfiguredObject:
			ReconfiguredObject(pEvent->id);
			break;
		case eNOE_BindAspects:
			BindAspects(pEvent->id, pEvent->aspects);
			break;
		case eNOE_UnbindAspects:
			UnbindAspects(pEvent->id, pEvent->aspects);
			break;
		case eNOE_ContextDestroyed:
			ContextDestroyed();
			break;
		case eNOE_SetAuthority:
			SetAuthority(pEvent->id, pEvent->authority);
			break;
		case eNOE_RemoveRMIListener:
			RemoveRMIListener(pEvent->pRMIListener);
			break;
		case eNOE_SetAspectProfile:
			SetAspectProfile(pEvent->id, pEvent->aspects, pEvent->profile);
			break;
		case eNOE_DebugEvent:
			DebugEvent(pEvent->id, pEvent->dbgEvent);
			break;
		case eNOE_GotBreakage:
			GotBreakage(pEvent->pBreakage);
			break;
		case eNOE_SyncWithGame_Start:
#if ENABLE_DEBUG_KIT
			if (CNetCVars::Get().DebugConnectionState)
				CContextViewStateManager::DumpLockers(Parent()->GetName());
#endif
			break;
		case eNOE_PartialUpdate:
			NotifyPartialUpdate(pEvent->id, pEvent->aspectID);
			break;
		}
	}
}

void CContextView::OnEndpointEvent(const SCTPEndpointEvent& evt)
{
	switch (evt.event)
	{
	case eCEE_NoBlockingMessages:
		OnNoBlockingMessages();
		break;
	}
}

void CContextView::DebugEvent(SNetObjectID id, ENetObjectDebugEvent evt)
{
}

void CContextView::OnChannelEvent(CNetContextState*, INetChannel* pFrom, SNetChannelEvent* pEvent)
{
}

class CContextView::CCET_UnlockStateChanges : public CCET_Base
{
public:
	CCET_UnlockStateChanges(CContextViewPtr pView, const char* name) : m_pView(pView), m_name(name)
	{
		++g_objcnt.cetUnlockStateChanges;
	}

	~CCET_UnlockStateChanges()
	{
		--g_objcnt.cetUnlockStateChanges;
	}

	const char* GetName()
	{
		return m_name;
	}

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		SCOPED_GLOBAL_LOCK;
		m_pView->m_establishmentLock = CChangeStateLock();
		return eCETR_Ok;
	}

private:
	CContextViewPtr m_pView;
	const char*     m_name;
};

class CContextView::CCET_PrimeUpdates : public CCET_Base
{
public:
	CCET_PrimeUpdates(CContextViewPtr pView) : m_pView(pView) {}

	const char* GetName()
	{
		return "PrimeUpdates";
	}

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		SCOPED_GLOBAL_LOCK;
		m_pView->PrimeUpdates();
		return eCETR_Ok;
	}

private:
	CContextViewPtr m_pView;
};

class CContextView::CCET_SafetySleep : public CCET_Base
{
public:
	CCET_SafetySleep(CContextViewPtr pView, uint32 length) : m_pView(pView), m_start(0), m_length(length) {}

	const char* GetName()
	{
		return "SafetySleep";
	}

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		if (!m_start)
		{
			m_start = m_pView->Parent()->GetMostRecentSentSeq();
			return eCETR_Wait;
		}
		else if (m_pView->Parent()->GetMostRecentAckedSeq() < m_start + m_length)
		{
			return eCETR_Wait;
		}
		else
		{
			return eCETR_Ok;
		}
	}

private:
	CContextViewPtr m_pView;
	uint32          m_start;
	const uint32    m_length;
};

class CContextView::CCET_ClearAllState : public CCET_Base
{
public:
	CCET_ClearAllState(CContextViewPtr pView) : m_pView(pView) {}

	const char* GetName()
	{
		return "ClearAllState";
	}

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		SCOPED_GLOBAL_LOCK;
		m_pView->ClearAllState();
		return eCETR_Ok;
	}

private:
	CContextViewPtr m_pView;
};

void CContextView::GC_GetEstablishmentOrder()
{
	ASSERT_PRIMARY_THREAD;
	ASSERT_GLOBAL_LOCK;

	CContextEstablisherPtr pEstablisher = new CContextEstablisher();
	pEstablisher->OnFailDisconnect(Parent());

	if (CNetCVars::Get().SafetySleeps)
	{
		for (int i = eCVS_Begin; i <= eCVS_InGame; i++)
			pEstablisher->AddTask((EContextViewState)i, new CCET_SafetySleep(this, 10 / (i + 1)));
	}
	pEstablisher->AddTask(eCVS_Begin, new CCET_ClearAllState(this));
	InitChannelEstablishmentTasks(pEstablisher);
	// +1 is a small hack to get things working - we should change ChangeContext to return a serial number and arrange to have
	// that passed around in GameContext
#if ENABLE_DEBUG_KIT
	NetLog("Request channel establishment tasks for CTXSERIAL:%d", ContextState()->GetToken());
#endif
	ContextState()->GetGameContext()->InitChannelEstablishmentTasks(pEstablisher, Parent(), ContextState()->GetToken());

	if (!IsLocal())
		pEstablisher->AddTask(eCVS_InGame, new CCET_PrimeUpdates(this));

	for (int i = eCVS_Begin; i <= eCVS_InGame; i++)
		pEstablisher->AddTask((EContextViewState)i, new CCET_UnlockStateChanges(this, GetWaitStateName((EContextViewState)i)));

	ContextState()->RegisterEstablisher(this, pEstablisher);
	ContextState()->SetEstablisherState(this, eCVS_Begin);
	UnlockStateChanges("ESTABLISHMENT_ORDER");
}

bool CContextView::EnterState(EContextViewState state)
{
	if (state == eCVS_Initial)
	{
		ContextState()->ChangeSubscription(this, FilterEventMask(ContextViewEvents[state], state) & m_eventMask);
	}
	else if (state == eCVS_Begin)
	{
		ContextState()->ChangeSubscription(this, 0);
		TO_GAME_LAZY(&CContextView::GC_Lazy_StateSink, this, m_pContextState);
		m_pContextState = Context()->GetCurrentState();
		ContextState()->ChangeSubscription(this, FilterEventMask(ContextViewEvents[state], state) & m_eventMask);

		LockStateChanges("ESTABLISHMENT_ORDER");

		NET_ASSERT(!m_establishmentLock.IsLocking());
		m_establishmentLock = CChangeStateLock(this, GetWaitStateName(eCVS_Begin));
		TO_GAME(&CContextView::GC_GetEstablishmentOrder, this);
	}
	else
	{
		NET_ASSERT(!m_establishmentLock.IsLocking());

		m_establishmentLock = CChangeStateLock(this, GetWaitStateName(state));
		ContextState()->SetEstablisherState(this, state);
		ContextState()->ChangeSubscription(this, FilterEventMask(ContextViewEvents[state], state) & m_eventMask);
	}

	if (state == eCVS_InGame)
	{
		SNetChannelEvent event;
		event.event = eNCE_InGame;
		ContextState()->BroadcastChannelEvent(Parent(), &event);

		SNetObjectEvent noe;
		noe.event = eNOE_InGame;
		ContextState()->Broadcast(&noe);
	}

	Parent()->SetAfterSpawning(state >= eCVS_PostSpawnEntities);

	m_currentState = state;

	bool ok = true;

	for (uint32 i = 0; i < m_extensions.size(); ++i)
	{
		ok &= m_extensions[i]->EnterState(state);
	}

	return ok;
}

void CContextView::ExitState(EContextViewState state)
{
	if (!m_forcedStates.empty())
	{
		state = m_forcedStates.front();
		m_forcedStates.erase(m_forcedStates.begin());
	}
	else if (state == eCVS_InGame)
	{
		state = eCVS_Initial;
	}
	else
	{
		state = EContextViewState(int(state) + 1);
	}
	SetLocalState(state);
}

void CContextView::OnNeedToSendStateInformation(bool urgently)
{
	if (Parent())
	{
		Parent()->OnChangedIdle();
		if (urgently)
			Parent()->ForcePacketSend();
	}
}

void CContextView::OnViewStateDisconnect(const char* message)
{
	if (Parent())
		Parent()->Disconnect(eDC_ProtocolError, message);
}

void CContextView::Init(
  CNetChannel* pParent,
  CNetContext* pContext,
  SContextViewConfiguration* pConfig)
{
	MMM_REGION(m_pMMM);
	for (int i = 0; i < 2; i++)
		m_pAttachments[i].reset(new TAttachmentMap);
	m_pEarlyPartialUpdateIDs.reset(new TEarlyPartialUpdateIDs);
	m_pSendables.reset(new TSendablesMap);

	m_pParent = pParent;
	m_pContext = pContext;

	m_config = *pConfig;
	m_eventMask = ~unsigned(0);

	if (!m_config.pReconfigureMsg)
		m_eventMask &= ~eNOE_ReconfiguredObject;
	if (!m_config.pUpdateMsg)
		m_eventMask &= ~eNOE_ObjectAspectChange;

	m_bLocal = false;

	m_pParent->ChangeSubscription(this, eCEE_NoBlockingMessages);
}

void CContextView::OnChannelDestroyed()
{
	Die();
	m_pParent = 0;
}

void CContextView::SetPassword(const string& password)
{
	m_password = password;
}

void CContextView::HandleAspectChanges(const std::pair<SNetObjectID, SNetObjectAspectChange>* pChanges)
{
	if (IsLocal())
	{
		NetWarning("HandleAspectChanges called on local connection");
		return;
	}

	for (; pChanges->first; ++pChanges)
	{
		if (pChanges->second.aspectsChanged)
			ChangedObject(pChanges->first, 0, pChanges->second.aspectsChanged);
	}
}

void CContextView::CompleteInitialization()
{
	if (gEnv->IsEditor() || (!gEnv->bMultiplayer))
	{
		CContextView* pRemote =
		  m_pContext->GetLocalContext(m_pParent->GetIP());
		m_bLocal = pRemote != NULL;

		if (m_bLocal)
		{
			// note: client context gets set to local here, but gets set to false on the first pass through
			pRemote->m_bLocal = true;
			//		pRemote->m_vAspects.clear();

			m_eventMask &= ~ExcludeIfLocal;
			pRemote->m_eventMask &= ~ExcludeIfLocal;
			if (pRemote->ContextState())
				pRemote->ContextState()->ChangeSubscription(pRemote, pRemote->FilterEventMask(ContextViewEvents[eCVS_Initial], eCVS_Initial) & pRemote->m_eventMask);
		}
	}
	else
	{
		// Multiplayer only: prevents initialization going wrong in potential host
		// migration race conditions where a remote client rejoins before the
		// server's local client
		m_bLocal = m_pParent->IsLocal();
		if (m_bLocal)
		{
			m_eventMask &= ~ExcludeIfLocal;
		}
	}

	m_pContextState = Context()->GetCurrentState();
	ContextState()->ChangeSubscription(this, FilterEventMask(ContextViewEvents[eCVS_Initial], eCVS_Initial) & m_eventMask);

#if ENABLE_SESSION_IDS
	if (!stl::holds_alternative<TLocalNetAddress>(m_pParent->GetIP()))
		m_pContext->EnableSessionDebugging();
#endif

	TNetAddressVec ips;
	GetLocalIPs(ips);
	m_pContext->RegisterLocalIPs(ips, this);

	m_history[eH_AspectData] = new CMementoHistory(this);
	m_history[eH_Profile] = new CProfileHistory(this);
	m_history[eH_Auth] = new CAuthorityHistory(this);
	m_history[eH_Configuration] = new CConfigurationHistory(this);

	if (!Parent()->IsFakeChannel())
	{
		SExtensionAdder adder(this);
		CNetwork::Get()->GetServiceManager()->CreateExtension(IsServer(), &adder);
	}
}

void CContextView::OnNoBlockingMessages()
{
	if (!Parent())
	{
		return;
	}

	if (!Parent()->IsTimeReady())
	{
		return;
	}

	if (ShouldSendFinishLocalState(false))
	{
		class CFinishStateMessage : public INetMessage
		{
		public:
			CFinishStateMessage(
			  CContextView* pView,
			  const SNetMessageDef* pFinishStateMsg) :
				INetMessage(pFinishStateMsg),
				m_pView(pView)
			{
				SetPriorityDelta(16.0f);
				++g_objcnt.finishStateMessage;
			}
			~CFinishStateMessage()
			{
				--g_objcnt.finishStateMessage;
			}
			EMessageSendResult WritePayload(
			  TSerialize serialize,
			  uint32 nCurrentSeq,
			  uint32 nBasisSeq)
			{
				return eMSR_SentOk;
			}
			void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate update)
			{
				if (update == eNSSU_Requeue || m_pView->IsDead())
					return;
				m_pView->AckFinishLocalState(update == eNSSU_Ack);
			}
			virtual size_t GetSize() { return sizeof(*this); }

		private:
			CContextViewPtr m_pView;
		};

#if ENABLE_DEBUG_KIT
		NetLog("Establishment: %s finishes %s", DebugString(), GetStateName(GetLocalState()));
#endif // ENABLE_DEBUG_KIT
		Parent()->NetAddSendable(new CFinishStateMessage(this, m_config.pFinishStateMsg), 0, NULL, NULL);
		SendFinishLocalState();
	}

	EContextViewState sendState;
	if (ShouldSendLocalState(sendState, false))
	{
		class CChangeStateMessage : public INetMessage
		{
		public:
			CChangeStateMessage(
			  CContextView* pView,
			  EContextViewState state,
			  const SNetMessageDef* pChangeStateMsg) :
				INetMessage(pChangeStateMsg),
				m_pView(pView),
				m_state(state)
			{
				SetPriorityDelta(16.0f);
				++g_objcnt.changeStateMessage;
			}
			~CChangeStateMessage()
			{
				--g_objcnt.changeStateMessage;
			}
			EMessageSendResult WritePayload(
			  TSerialize serialize,
			  uint32 nCurrentSeq,
			  uint32 nBasisSeq)
			{
				serialize.EnumValue("state", m_state, eCVS_Initial, eCVS_NUM_STATES);
				return eMSR_SentOk;
			}
			void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate state)
			{
				if (state == eNSSU_Requeue || m_pView->IsDead())
					return;
				if (!m_pView->AckLocalState(state == eNSSU_Ack))
					m_pView->Parent()->Disconnect(eDC_ProtocolError, "Failed to change network state");
			}
			virtual size_t GetSize() { return sizeof(*this); }

		private:
			CContextViewPtr   m_pView;
			EContextViewState m_state;
		};

		//NetLog( "[state] %s sends %s", DebugString(), GetStateName(sendState) );
		Parent()->NetAddSendable(new CChangeStateMessage(this, sendState, m_config.pChangeStateMsg), 0, NULL, NULL);
		SendLocalState();
	}
}

bool CContextView::CheckDependentId(EntityId id, SSendableHandle* hdl, bool allowNotPresent, CNetObjectBindLock* pLk)
{
	bool canSend = true;
	if (id)
	{
		canSend = false;
		SNetObjectID netId = ContextState()->GetNetID(id);
		if (!netId)
		{
			*hdl = SSendableHandle();
			canSend = allowNotPresent; // no such object exists
		}
		else if (netId.id < m_objects.size())
		{
			*pLk = ContextState()->LockObject(netId, allowNotPresent ? "PENDINGRMI_DEP" : "PENDINGRMI_OBJ");
			if (IsObjectEnabled(netId))
			{
				// TODO: optimize
				// currently this code forces any dependent object to be updated, and the rmi to wait for that update to occur
				// probably we only need to do this in the case of attached ordered messages being in the queue for this object
				canSend = true;
				ChangedObject(netId, eCOF_ForcePreGame, 0);
				*hdl = m_objects[netId.id].msgHandle;
			}
			else if (m_objects[netId.id].msgHandle)
			{
				canSend = true;
				*hdl = m_objects[netId.id].msgHandle;
			}
			else
			{
				canSend = Parent()->NetAddSendable(new CWaitForEnabled(netId, this), 0, NULL, hdl);
			}
		}
		else
		{
			*pLk = ContextState()->LockObject(netId, allowNotPresent ? "PENDINGRMI_DEP" : "PENDINGRMI_OBJ");
			canSend = Parent()->NetAddSendable(new CWaitForEnabled(netId, this), 0, NULL, hdl);
		}
	}
	return canSend;
}

bool CContextView::ScheduleAttachment(bool fromChannel, IRMIMessageBodyPtr pMessage, const SAttachmentIndex* pIndex)
{
	MMM_REGION(m_pMMM);

	if (IsDead() || m_pContext->GetCurrentState() != ContextState() || m_flushUpdates)
	{
		return false;
	}

	if (IsBeforeState(eCVS_SpawnEntities))
		return false;

	switch (pMessage->attachment)
	{
	case eRAT_NoAttach:     // Intentional fall-through
	case eRAT_Urgent:       // Intentional fall-through
	case eRAT_Independent:
		{
			CRMIMessage_UserDef* pUD = 0;
			CRMIMessage_Script* pScript = 0;
			INetSendablePtr pSend;
			bool blockStateChange = IsPastOrInState(eCVS_InGame);
			if (pMessage->pMessageDef)
			{
				pUD = CRMIMessageAllocator::CreateUserDef(pMessage, this, blockStateChange);
				pSend = pUD;
			}
			else
			{
				const SNetMessageDef* pDef = GetRMIDef(pMessage->reliability);
				pScript = CRMIMessageAllocator::CreateScript(pDef, pMessage, this, blockStateChange);
				pSend = pScript;
			}
			SSendableHandle depHdl[3];
			SSendableHandle tempMyHdlStorage;
			SSendableHandle* myHdl = &tempMyHdlStorage;
			int cantSend = 0;
			// cantSend is a small bitmask that tracks reasons why a message can't be sent
			// think of it like an array of bools :)
			CNetObjectBindLock lkObj, lkDep;
			cantSend |= !CheckDependentId(pMessage->dependentId, depHdl + 0, true, &lkDep);

			bool doDependencyChecks = true;
#if ENABLE_INDEPENDENT_RMIS
			doDependencyChecks &= (pMessage->attachment != eRAT_Independent);
#endif // ENABLE_INDEPENDENT_RMIS
#if ENABLE_URGENT_RMIS
			doDependencyChecks &= (pMessage->attachment != eRAT_Urgent);
#endif // ENABLE_URGENT_RMIS
			if (doDependencyChecks)
			{
				cantSend |= 2 * !CheckDependentId(pMessage->objId, depHdl + 2, false, &lkObj);
			}
#if ENABLE_URGENT_RMIS || ENABLE_INDEPENDENT_RMIS
			else
			{
				// For urgent and independent RMIs, we still need to ensure the object exists, and if it doesn't make the RMIs depend on it.
				SNetObjectID netId = ContextState()->GetNetID(pMessage->objId);
				if (!netId)
				{
					IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pMessage->objId);
					NetLog("[RMI]: [%s] invoked to [%s] when entity [%s] (id %d) does not exist", (pMessage->pMessageDef != NULL) ? pMessage->pMessageDef->description : "<unknown>", Parent()->GetName(), pEntity ? pEntity->GetName() : "<unknown>", pMessage->objId);
					cantSend |= 2 * !CheckDependentId(pMessage->objId, depHdl + 2, false, &lkObj);
				}
			}
#endif // ENABLE_URGENT_RMIS || ENABLE_INDEPENDENT_RMIS
			cantSend |= 4 * !IsContextCurrent();
			SNetObjectID id = ContextState()->GetNetID(pMessage->objId);
			if (id.id < m_objects.size())
			{
				// Treat all messages as ordered until in game state achieved otherwise
				// you potentially run the risk of having blocking messages always in
				// flight which prevents state changes from happening and clients can't
				// load into game
				if (!IsPastOrInState(eCVS_InGame) || pMessage->reliability == eNRT_ReliableOrdered || pMessage->reliability == eNRT_UnreliableOrdered)
				{
					// Ordered RMIs depend on the previous ordered RMI (first ordered RMI handle is the bind message handle)
					if (gEnv->bServer && (!Parent()->IsLocal()))
					{
						if (m_objects[id.id].orderedRMIHandle == 0)
						{
							IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pMessage->objId);
#if ENABLE_DEFERRED_RMI_QUEUE
							NetLog("[RMI]: Ordered [%s] invoked to [%s] when entity [%s] (id %d) is not bound (bind handle is 0) - %s", (pMessage->pMessageDef != NULL) ? pMessage->pMessageDef->description : "<unknown>", Parent()->GetName(), pEntity ? pEntity->GetName() : "<unknown>", pMessage->objId, fromChannel ? "DEFERRING" : "NOT SENDING");
							if (fromChannel)
							{
								if (pIndex == NULL)
								{
									m_deferredRMI.push_back(pMessage);
								}
								else
								{
									NetLog("[RMI]: Unable to defer - RMI has attachment index");
								}
							}
							return false;
#else
							NetLog("[RMI]: Ordered [%s] invoked to [%s] when entity [%s] (id %d) is not bound (bind handle is 0)", (pMessage->pMessageDef != NULL) ? pMessage->pMessageDef->description : "<unknown>", Parent()->GetName(), pEntity ? pEntity->GetName() : "<unknown>", pMessage->objId);
#endif        // ENABLE_DEFERRED_RMI_QUEUE
						}
					}
					depHdl[1] = *(myHdl = &m_objects[id.id].orderedRMIHandle);
				}
#if ENABLE_URGENT_RMIS || ENABLE_INDEPENDENT_RMIS
				else
				{
					if (gEnv->bServer && (!Parent()->IsLocal()))
					{
						// Only care about server->remote RMIs
						bool dependOnBind = false;
	#if ENABLE_INDEPENDENT_RMIS
						dependOnBind |= (pMessage->attachment == eRAT_Independent);
	#endif    // ENABLE_INDEPENDENT_RMIS
	#if ENABLE_URGENT_RMIS
						dependOnBind |= (pMessage->attachment == eRAT_Urgent);
	#endif    // ENABLE_URGENT_RMIS
						if (dependOnBind)
						{
							// Unordered RMIs (urgent or independent) are dependent on the bind handle only
							if (m_objects[id.id].bindHandle == 0)
							{
								IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pMessage->objId);
								NetLog("[RMI]: Unordered [%s] invoked to [%s] when entity [%s] (id %d) is not bound (bind handle is 0)", (pMessage->pMessageDef != NULL) ? pMessage->pMessageDef->description : "<unknown>", Parent()->GetName(), pEntity ? pEntity->GetName() : "<unknown>", pMessage->objId);
							}
							depHdl[1] = m_objects[id.id].bindHandle;
						}
					}
				}
#endif  // ENABLE_URGENT_RMIS || ENABLE_INDEPENDENT_RMIS
				//if (const SContextObject* pObj = ContextState()->GetContextObject(id))
				SContextObjectRef obj = ContextState()->GetContextObject(id);
				if (obj.main)
				{
					pSend->SetGroup(m_objects[id.id].authority ? obj.xtra->scheduler_owned : obj.xtra->scheduler_normal);
					pSend->SetPulses(obj.xtra->pPulseState);
				}
			}
			if (!cantSend)
			{
				Parent()->NetAddSendable(pSend, 3, depHdl, myHdl);
				if (pMessage->pMessageDef)
				{
					if (pUD)
						pUD->SetMessageHandle(*myHdl);
				}
				else
				{
					if (pScript)
						pScript->SetMessageHandle(*myHdl);
				}
				m_pSendables->insert(std::make_pair(id, *myHdl));
			}
			else
			{
				return false;
			}
		}
		break;

	default: // eRAT_PreAttach || eRAT_PostAttach
		{
			if (pMessage->dependentId)
			{
				NetWarning("Dependent ID specified on an attached message; not supported at this time");
				return false;
			}

			SAttachmentIndex idx;
			if (!pIndex)
			{
				idx.index = m_nAttachmentIndex++;
				idx.id = ContextState()->GetNetID(pMessage->objId);
				if (!idx.id)
				{
					NetWarning("Entity %d is not bound to the network; ignoring attachment %s", pMessage->objId, pMessage->pMessageDef ? pMessage->pMessageDef->description : "<<unknown>>");
					return false;
				}
			}
			else
			{
				idx = *pIndex;
			}

			bool changed = m_pAttachments[pMessage->attachment]->insert(std::make_pair(idx, pMessage)).second;
			NET_ASSERT(changed);

			SNetChannelEvent evt;
			evt.event = eNCE_ScheduleRMI;
			evt.id = ContextState()->GetNetID(pMessage->objId);
			evt.pRMIBody = pMessage;
			ContextState()->BroadcastChannelEvent(Parent(), &evt);

			ChangedObject(evt.id, 0, 0);
		}
	}

	return true;
}

#if ENABLE_DEFERRED_RMI_QUEUE
void CContextView::ProcessDeferredRMIList(void)
{
	// Re-add all the deferred RMIs to the polymorphic queue
	for (TDeferredRMIVec::iterator it = m_deferredRMI.begin(); it != m_deferredRMI.end(); ++it)
	{
		FROM_GAME(&CNetChannel::NC_DispatchRMI, m_pParent, *it);
	}

	m_deferredRMI.clear();
}
#endif // ENABLE_DEFERRED_RMI_QUEUE

void CContextView::DeclareWitness(EntityId id)
{
	if (!ContextState() || !Context())
		return;

	SNetObjectID netID = ContextState()->GetNetID(id);
	if (!netID)
	{
		NetWarning("Trying to declare a witness that is not bound (%.8x)", id);
		return;
	}

	SNetChannelEvent event;
	event.event = eNCE_DeclareWitness;
	event.id = netID;
	ContextState()->BroadcastChannelEvent(Parent(), &event);

	m_witness = netID;

	OnWitnessDeclared();
	for (uint32 i = 0; i < m_extensions.size(); ++i)
		m_extensions[i]->OnWitnessDeclared(id);
}

bool CContextView::ReadCurrentObjectID(TSerialize ser, bool forBind)
{
	if (m_curObjectID)
		NetWarning("Missing end marker for object update (curObjectID = %s)", m_curObjectID.GetText());
	ser.Value("netID", m_curObjectID, 'eid');
	m_ignoringCurObject = false;

	if (forBind && !IsLocal() && !CNetCVars::Get().doingPacketReplay())
		m_curObjectID.salt++;

	//const SContextObject * pObj = ContextState()->GetContextObject(m_curObjectID);
	SContextObjectRef obj = ContextState()->GetContextObject(m_curObjectID);

#if LOG_INCOMING_MESSAGES || LOG_OUTGOING_MESSAGES
	if (CNetCVars::Get().LogNetMessages & 16)
	{
		const char* name = "<<unknown>>";
		if (obj.main)
		{
			IEntity* pEnt = gEnv->pEntitySystem->GetEntity(obj.main->userID);
			if (pEnt)
				name = pEnt->GetName();
		}
		NetLog("Got object ID: %s [%s]", name, m_curObjectID.GetText());
	}
#endif

	if (!forBind && !obj.main)
	{
		NetWarning("Net-object id %s does not exist in net context state", m_curObjectID.GetText());
		return false;
	}
	else if (!forBind && !obj.main->userID)
	{
		m_ignoringCurObject = true;
		Parent()->SetEntityId(0);
		return true;
	}
	else if (forBind)
	{
		return false;
	}
	else
	{
		if (!IsObjectBound(m_curObjectID))
		{
			NetWarning("Net-object id %s does not exist in context view", m_curObjectID.GetText());
			return false;
		}

		Parent()->SetEntityId(obj.main->userID);
		return true;
	}
}

bool CContextView::ClearCurrentObjectID()
{
	if (!m_curObjectID)
	{
		NetWarning("Got end marker for object update, but current object is invalid");
		return false; // NOTE: to prevent against random packet attacks - Lin
	}
	m_curObjectID = SNetObjectID();
#if ENABLE_DEBUG_KIT
	m_acceptedForUpdateObject = SNetObjectID();
#endif
	Parent()->SetEntityId(0);
	return true;
}

void CContextView::ClearAllState()
{
	MMM_REGION(m_pMMM);

	//	m_vAspects.resize(0);
	//	m_scheduler.Clear();
	//	m_authMgr.Reset();
	//	m_enableMgr.Reset();
	CancelUpdates();
	for (int i = 0; i < 2; i++)
	{
		while (!m_pAttachments[i]->empty())
		{
			m_pAttachments[i]->erase(m_pAttachments[i]->begin());
		}
	}

	if (Context() && Context()->IsMultiplayer())
	{
		for (size_t i = 0; i < m_objectsEx.size(); i++)
		{
			SNetObjectID id(static_cast<uint16>(i), m_objects[i].salt);
			if (IsObjectBound(id))
				ClearHistory(id);
		}
	}

	m_curObjectID = SNetObjectID();
	m_ignoringCurObject = false;
	m_witness = SNetObjectID();

	m_objectLocks.resize(0);
	m_objects.resize(0);
	m_objectsEx.resize(0);

	m_pSendables->clear();
	m_flushUpdates = false;
	for (int i = 0; i < eH_NUM_HISTORIES; i++)
	{
		if (m_history[i])
		{
			m_history[i]->Reset();
		}
	}
	m_boundCache = SNetObjectID();
	m_enabledCache = SNetObjectID();
	m_pEarlyPartialUpdateIDs->clear();
}

void CContextView::CancelUpdates()
{
	if (Parent())
	{
		for (uint32 i = 0; i < m_objects.size(); i++)
		{
			Parent()->NetRemoveSendable(m_objects[i].msgHandle);
			if (Context()->IsMultiplayer())
				for (int j = 0; j < NumAspects; j++)
				{
					Parent()->NetRemoveSendable(m_objectsEx[i].notifyPartialUpdateHandle[j]);
				}
		}
		std::vector<SSendableHandle> handles;
		handles.reserve(m_pSendables->size());
		for (TSendablesMap::iterator it = m_pSendables->begin(); it != m_pSendables->end(); ++it)
		{
			handles.push_back(it->second);
		}

		for (size_t i = 0; i < handles.size(); ++i)
			Parent()->NetRemoveSendable(handles[i]);
	}
	m_pSendables->clear();
}

void CContextView::PrimeUpdates()
{
	MMM_REGION(m_pMMM);
	for (uint32 i = 0; i < m_objects.size(); i++)
	{
		SNetObjectID id(i, m_objects[i].salt);
		if (IsObjectEnabled(id))
		{
			UpdateSchedulerState(id);
			ChangedObject(id, 0, NET_ASPECT_ALL);
		}
	}
}

void CContextView::ContextDestroyed()
{
	m_pContext = NULL;
	if (Parent())
		Parent()->Disconnect(eDC_Unknown, "ContextDestroyed");
}

void CContextView::BindObject(SNetObjectID netID)
{
	//const SContextObject * pObj = m_pContext->GetContextObject(netID);
	//SContextObjectRef obj = m_pContext->GetContextObject(netID);

	SNetChannelEvent event;
	event.event = eNCE_BoundObject;
	event.id = netID;
	ContextState()->BroadcastChannelEvent(Parent(), &event);

	if (m_objects.size() <= netID.id)
	{
		m_objects.resize(netID.id + 1);
		m_objectLocks.resize(netID.id + 1);
		if (Context()->IsMultiplayer())
			m_objectsEx.resize(netID.id + 1);
	}
	SContextViewObject& obj = m_objects[netID.id];
	NET_ASSERT(!IsObjectBound(netID));
	NET_ASSERT(obj.spawnState == eSS_Unspawned);
	obj.Reset();
	obj.salt = netID.salt;
	m_objectLocks[netID.id] = ContextState()->LockObject(netID, "CTX");
	if (Context()->IsMultiplayer())
		m_objectsEx[netID.id].Reset();
	m_boundCache = SNetObjectID();
	m_enabledCache = SNetObjectID();
}

void CContextView::SetSpawnState(SNetObjectID id, ESpawnState state)
{
	if (IsObjectBound(id))
	{
		m_enabledCache = SNetObjectID();
		NET_ASSERT(m_objects[id.id].spawnState < state);
		m_objects[id.id].spawnState = state;
		if (state == eSS_Enabled)
		{
			if (!IsLocal())
			{
				for (int i = 0; i < NumAspects; i++)
					PolluteObjectAspect(id, i);
				ChangedObject(id, 0, NET_ASPECT_ALL);
			}
			UpdateSchedulerState(id);
		}
	}
	else
		NetWarning("Trying to enable unbound object %s", id.GetText());
}

void CContextView::UpdateSchedulerState(SNetObjectID id)
{
	TEarlyPartialUpdateIDs::iterator itEPU = m_pEarlyPartialUpdateIDs->find(id);
	if (itEPU != m_pEarlyPartialUpdateIDs->end())
	{
		NetworkAspectType aspects = itEPU->second;
		m_pEarlyPartialUpdateIDs->erase(itEPU);
		CBitIter iter(aspects);
		NetworkAspectID i;
		while (iter.Next(i))
			NotifyPartialUpdate(id, i);
	}

	// TODO: make sure the logic here is correct (not relevant to single player) - Lin
	if (!Context()->IsMultiplayer())
		return;

	//const SContextObject * pObj = m_pContext->GetContextObject(id);
	SContextObjectRef obj = ContextState()->GetContextObject(id);

	SContextViewObject* pVwObj = &m_objects[id.id];
	SContextViewObjectEx* pVwObjEx = &m_objectsEx[id.id];

	// figure what priority class to have
	if (pVwObj->msgHandle)
	{
		INetSendablePtr pSendable = Parent()->NetFindSendable(pVwObj->msgHandle);
		if (pSendable)
		{
			uint32 prioCls = pVwObj->authority ? obj.xtra->scheduler_owned : obj.xtra->scheduler_normal;
			pSendable->SetGroup(prioCls);
		}
	}
}

NetworkAspectType CContextView::GetSentAspects(SNetObjectID id, bool assumeEnabled, EGetSentAspectsAuthority assumeAuthority)
{
	//const SContextObject * pObj = m_pContext->GetContextObject(id);
	SContextObjectRef obj = ContextState()->GetContextObject(id);
	NetworkAspectType aspects;

	if (IsLocal())
		return 0;

	if ((assumeEnabled && IsObjectBound(id)) || IsObjectEnabled(id))
	{
		// relies on the ordering of EGetSentAspectsAuthority
		// authMask is a bitmask lookup table
		uint8 authMask = 4 | m_objects[id.id].authority;
		bool auth = bool((authMask >> assumeAuthority) & 1);
		if (auth)
		{
			aspects = obj.xtra->nAspectsEnabled;
			NetworkAspectType mask = Context()->DelegatableAspects() & obj.xtra->delegatableMask;
			// flip the bits in mask if we're not the object owner (i.e. we're the client)
			mask ^= ((~NetworkAspectType(obj.main->bOwned)) + 1);
			aspects &= mask;
		}
		else if (!obj.main->bOwned)
			aspects = 0;
		else
			aspects = obj.xtra->nAspectsEnabled;
	}
	else
	{
		aspects = 0;
	}

	if (IsServer())
	{
		CNetContextState* pContextState = ContextState();
		if (pContextState && !pContextState->RemoteContextHasAuthority(Parent(), obj.main->userID))
		{
			aspects &= ~Context()->ServerControllerOnlyAspects();
		}
	}

	// Loop through bits and mask off aspects that should not exist on a particular channel due to masking
	ChannelMaskType parentMask = Parent()->GetChannelMask();
	CBitIter iter(aspects);
	NetworkAspectID aspectIdx;
	while (iter.Next(aspectIdx))
	{
		bool channelIsMasked = (parentMask & Context()->CNetContext::GetAspectChannelMask(aspectIdx)) == 0;

		if (channelIsMasked)
		{
			aspects &= ~BIT(aspectIdx);
		}
	}

	return aspects;
}

void CContextView::BroadcastHistoricalEvent(const SHistoricalEvent& event)
{
	for (int i = 0; i < eH_NUM_HISTORIES; i++)
		m_history[i]->HandleEvent(event);
}

bool CContextView::DoUnbindObject(SNetObjectID netID, bool clearSendables)
{
	NET_ASSERT(m_objects.size() > netID.id);
	NET_ASSERT(m_objects[netID.id].salt = netID.salt);
	// we won't send out anything more on this object
	ESpawnState oldSpawnState = m_objects[netID.id].spawnState;
	m_objects[netID.id].spawnState = eSS_Unspawned;

	CNetObjectBindLock tmp;
	tmp.Swap(m_objectLocks[netID.id]);
	m_boundCache = SNetObjectID();
	m_enabledCache = SNetObjectID();

	if (clearSendables)
	{
		Parent()->RemoveSendable(m_objects[netID.id].msgHandle);

		for (int i = 0; i < 2; ++i)
		{
			for (TAttachmentMap::iterator it = m_pAttachments[i]->begin(); it != m_pAttachments[i]->end(); )
			{
				if (it->first.id == netID)
				{
					TAttachmentMap::iterator next = it;
					++next;
					m_pAttachments[i]->erase(it);
					it = next;
				}
				else
				{
					++it;
				}
			}
		}
		std::pair<TSendablesMap::iterator, TSendablesMap::iterator> range = m_pSendables->equal_range(netID);
		for (TSendablesMap::iterator it = range.first; it != range.second; ++it)
		{
			m_pParent->NetRemoveSendable(it->second);
		}
		m_pSendables->erase(range.first, range.second);
	}

	return oldSpawnState > eSS_Unspawned;
}

void CContextView::UnboundObject(SNetObjectID netID)
{
	if (m_objects.size() <= netID.id)
	{
		NetWarning("CContextView::UnboundObject: %s is not bound on %s [size mismatch]", netID.GetText(), Parent()->GetName());
		return;
	}
	if (m_objects[netID.id].salt != netID.salt)
	{
		NetWarning("CContextView::UnboundObject: %s is not bound on %s [salt mismatch]", netID.GetText(), Parent()->GetName());
		return;
	}

	ClearHistory(netID);

	// Ensure the bind lock is removed
	CNetObjectBindLock tmp;
	tmp.Swap(m_objectLocks[netID.id]);

	m_objects[netID.id].Reset();
	if (Context()->IsMultiplayer())
		m_objectsEx[netID.id].Reset();
	m_boundCache = SNetObjectID();
	m_enabledCache = SNetObjectID();
}

void CContextView::ClearHistory(SNetObjectID netID)
{
	if (Context()->IsMultiplayer())
	{
		SSyncContext ctx;
		ctx.pViewObj = &m_objects[netID.id];
		ctx.pViewObjEx = &m_objectsEx[netID.id];
		for (int history = 0; history < eH_NUM_HISTORIES; history++)
		{
			CHistory* pHistory = GetHistory((EHistory) history);
			CBitIter iter(pHistory->indexMask);
			NetworkAspectID aspectIdx;
			while (iter.Next(aspectIdx))
			{
				ctx.index = aspectIdx;
				pHistory->Flush(ctx);
			}
		}
	}
}

bool CContextView::ClearAspects(SNetObjectID netID, NetworkAspectType aspects)
{
	if (!Context()->IsMultiplayer())
		return true;

	//const SContextObject * pObj = ContextState()->GetContextObject( netID );
	SContextObjectRef obj = ContextState()->GetContextObject(netID);

	if (!obj.main)
	{
		NetWarning("Attempt to clear aspects %.2x of unbound object (netID %s)", aspects, netID.GetText());
		return false;
	}

	SSyncContext ctx;
	ctx.objId = netID;
	ctx.ctxObj = ContextState()->GetContextObject(netID);
	NET_ASSERT(m_objects.size() > netID.id);
	NET_ASSERT(m_objects[netID.id].salt = netID.salt);
	ctx.pViewObj = &m_objects[netID.id];
	ctx.pViewObjEx = &m_objectsEx[netID.id];

	aspects &= m_history[eH_AspectData]->indexMask;

	NetworkAspectID i;
	CBitIter itAspects(aspects);
	while (itAspects.Next(i))
	{
		if ((1 << i) & aspects)
		{
			ctx.index = i;
			m_history[eH_AspectData]->Flush(ctx);
		}
	}

	return true;
}

void CContextView::SetAuthority(SNetObjectID id, bool auth)
{
	NET_ASSERT(m_objects.size() > id.id);
	NET_ASSERT(m_objects[id.id].salt = id.salt);

	if (m_objects[id.id].authority != auth)
	{
		m_objects[id.id].authority = auth;
		if (m_config.pSetAuthorityMsg)
		{
			ChangedObject(id, 0, NET_ASPECT_ALL);
		}
		UpdateSchedulerState(id);
	}

	if (m_pContextState)
		m_pContextState->UpdateAuthority(id, auth, IsLocal());
}

void CContextView::GetMemoryStatistics(ICrySizer* pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "CContextView");

	//	pSizer->Add(*this);
	pSizer->AddString(m_password);
	//m_scheduler.GetMemoryStatistics( pSizer );
	if (m_pContextState)
		m_pContextState->GetMemoryStatistics(pSizer);

	for (int i = 0; i < eH_NUM_HISTORIES; i++)
		m_history[i]->GetMemoryStatistics(pSizer);

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CContextView::m_forcedStates");
		pSizer->AddContainer(m_forcedStates);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CContextView::m_mAttachments[0]");
		pSizer->AddContainer(*m_pAttachments[0]);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CContextView::m_mAttachments[1]");
		pSizer->AddContainer(*m_pAttachments[1]);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CContextView::m_mSendables");
		pSizer->AddContainer(*m_pSendables);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CContextView::m_objects");
		pSizer->AddContainer(m_objects);
	}

	pSizer->AddContainer(m_voiceMessageHandles);
}

void CContextView::RemoveRMIListener(IRMIListener* pListener)
{
	for (int i = 0; i < 2; i++)
	{
		for (TAttachmentMap::iterator iter = m_pAttachments[i]->begin(); iter != m_pAttachments[i]->end(); ++iter)
		{
			if (iter->second->pListener == pListener)
				iter->second->pListener = 0;
		}
	}
}

SObjectMemUsage CContextView::GetObjectMemUsage(SNetObjectID id)
{
	SObjectMemUsage out;

	out.used = sizeof(m_objects[id.id]) + sizeof(m_objectLocks[id.id]);
	if (Context()->IsMultiplayer())
	{
		out.used += sizeof(m_objectsEx[id.id]);
	}
	out.required = out.used;
	out.instances = 1;

	return out;
}

void CContextView::PerformRegularCleanup()
{
}

void CContextView::SetAspectProfile(SNetObjectID objectID, NetworkAspectType aspect, uint8 profile)
{
}

void CContextView::GotBreakage(const SNetIntBreakDescription* pDesc)
{
}

void CContextView::PushForcedState(EContextViewState state, bool evenIfLocal)
{
	if (m_config.pForceStateMsg && Parent())
	{
		if (!IsLocal() || evenIfLocal)
		{
			SChangeStateMessage msg;
			msg.state = state;
			Parent()->NetAddSendable(new CSimpleNetMessage<SChangeStateMessage>(msg, m_config.pForceStateMsg), 0, NULL, NULL);
		}
	}
	m_forcedStates.push_back(state);
	m_establishmentLock = CChangeStateLock();
}

CNetChannel* CContextView::Parent() const
{
	return m_pParent;
}

void CContextView::GetLocalIPs(TNetAddressVec& vIPs)
{
	if (Parent())
		Parent()->GetLocalIPs(vIPs);
}

void CContextView::ReconfiguredObject(SNetObjectID netID)
{
	UpdateSchedulerState(netID);
	ChangedObject(netID, 0, NET_ASPECT_ALL);
}

void CContextView::PolluteObjectAspect(SNetObjectID id, NetworkAspectID aspectIdx)
{
	SHistoricalEvent event(eHE_Pollute);
	event.pView = this;
	event.objId = id;
	event.index = aspectIdx;
	event.pViewObj = &m_objects[id.id];
	event.pViewObjEx = &m_objectsEx[id.id];
	BroadcastHistoricalEvent(event);
	ChangedObject(id, 0, BIT(aspectIdx));
}

void CContextView::ChangeContext()
{
	if (m_config.pFlushMsgsMsg && Parent())
	{
		Parent()->AddSendable(new CSimpleNetMessage<SNoParams>(SNoParams(), m_config.pFlushMsgsMsg), 0, NULL, NULL);
	}
	CancelUpdates();
}

void CContextView::DefineExtensionsProtocol(IProtocolBuilder* b)
{
	for (uint32 i = 0; i < m_extensions.size(); ++i)
		m_extensions[i]->DefineProtocol(b);
}

void CContextView::InitChannelEstablishmentTasks(IContextEstablisher* pEst)
{
}

void CContextView::ChangedObject(SNetObjectID id, uint32 flags, NetworkAspectType dirtyAspects)
{
	NET_ASSERT(!(flags & 1)); // legacy thing to make sure nobody is passing true/false anymore

	if (m_objects.size() <= id.id || !IsContextCurrent())
		return;
	if (m_objects[id.id].salt != id.salt)
		return;

	if (!IsObjectBound(id))
	{
#if ENABLE_DEBUG_KIT
		NetLog("CContextView::ChangedObject: unknown object %s on %s", id.GetText(), m_pParent->GetName());
#endif
#if NET_ASSERT_LOGGING
		if (CNetCVars::Get().AssertLogging)
			NET_ASSERT_FAIL("Object bound", __FILE__, __LINE__);
#endif
		return;
	}

	if (Context()->IsMultiplayer())
	{
		if (id.id < m_objectsEx.size())
			m_objectsEx[id.id].dirtyAspects |= dirtyAspects;
	}

	if (!(flags & eCOF_ForcePreGame) && (!IsPastOrInState(eCVS_InGame) || m_flushUpdates || !IsContextCurrent()))
		return;

	if (IsObjectEnabled(id) && !m_objects[id.id].msgHandle)
	{
		SUpdateMessageConfig cfg;
		cfg.m_netID = id;
		cfg.m_pView = this;
		cfg.m_pStartUpdateDef = m_config.pUpdateMsg;
		_smart_ptr<CUpdateMessage> pSend = CRegularUpdateMessage::Create(cfg, eSCF_EnsureEnabled);
		uint32 grp = 0;
#if USE_HIGH_PRIORITY_ASPECT_HACK
		if (dirtyAspects & HIGH_PRIORITY_ASPECT_MASK)
		{
			grp = HIGH_PRIORITY_SCHEDULING_POLICY_GROUP; // hack scheduling policy group for aspect 31
		}
		else
#endif // USE_HIGH_PRIORITY_ASPECT_HACK
		{
			SContextObjectRef obj = ContextState()->GetContextObject(id);
			if (obj.xtra == NULL)
			{
				return;
			}

			grp = m_objects[id.id].authority ? obj.xtra->scheduler_owned : obj.xtra->scheduler_normal;
		}
		pSend->SetGroup(grp);
		if (!(flags & eCOF_NeverSubstitute))
			Parent()->NetSubstituteSendable(&*pSend, 0, NULL, &m_objects[id.id].activeHandle);
		else
			Parent()->NetAddSendable(&*pSend, 0, NULL, &m_objects[id.id].activeHandle);
		pSend->SetHandle(m_objects[id.id].activeHandle);
		m_objects[id.id].msgHandle = m_objects[id.id].activeHandle;
	}
}

bool CContextView::HaveAuthorityOfObject(SNetObjectID id) const
{
	if (m_objects.size() > id.id && IsObjectEnabled(id))
		return m_objects[id.id].authority;
	else
		return false;
}

bool CContextView::UpdateAspect(NetworkAspectID i, TSerialize ser, uint32 nCurSeq, uint32 nOldSeq, uint32 timeFraction32)
{
	if (IsLocal())
	{
		NetWarning("Update aspect called on a local connection");
		return false;
	}

	if (!CurrentObjectID())
	{
		// we received an UpdateAspect message without having received a BeginBindAspect/BeginUpdateObject (setting current object ID)
		// this can happen when the CTPEndpoint message ID is malformed (the clear text arithmetic stream is corrupted somewhere), or
		// someone is trying to attack us with random messages (with all CTPEndpoint level signingKey/encryption/CRC checking passed!)
		NetWarning("Handling UpdateAspect without a valid object ID (possible exploits)");
		return false;
	}

	
#if ENABLE_DEBUG_KIT
	if (CurrentObjectID() != m_acceptedForUpdateObject)
	{
		std::set<uint16>::iterator itCur = m_updatedObjectsThisFrame.find(CurrentObjectID().id);
		if (itCur != m_updatedObjectsThisFrame.end())
		{
			NetWarning("Multiple updates for an object in one packet (netid=%s)", CurrentObjectID().GetText());
			return false;
		}
		m_updatedObjectsThisFrame.insert(CurrentObjectID().id);
		m_acceptedForUpdateObject = CurrentObjectID();
	}
#endif

	// do we have control of this aspect?
	bool haveControl = (GetSentAspects(CurrentObjectID(), false, eGSAA_DefaultAuthority) & (1 << i)) != 0;
	bool ok = false;
	SReceiveContext ctx = CreateReceiveContext(ser, i, nCurSeq, nOldSeq, timeFraction32, &ok);
	if (!ok)
	{
		char msg[256];
		cry_sprintf(msg, "Failed creating sync context for aspect %s on object %s", Context()->GetAspectName(i), ctx.objId.GetText());
		Parent()->Disconnect(eDC_ContextCorruption, msg);
		return false;
	}
	ok = GetHistory(eH_AspectData)->ReadCurrentValue(ctx, !haveControl && !IgnoringCurrentObject());
	if (!ok)
	{
		CNetwork::Get()->BroadcastNetDump(eNDT_ObjectState);
		//
		char msg[256];
		cry_sprintf(msg, "Failed reading current value for aspect %s on object %s", Context()->GetAspectName(i), ctx.objId.GetText());
		Parent()->Disconnect(eDC_ContextCorruption, msg);
		return false;
	}
	return ok;
}

SReceiveContext CContextView::CreateReceiveContext(TSerialize ser, NetworkAspectID index, uint32 nCurSeq, uint32 nOldSeq, uint32 timeFraction32, bool* ok)
{
	SReceiveContext ctx(ser);
	*ok = true;
	ctx.basisSeq = nOldSeq;
	ctx.currentSeq = nCurSeq;
	ctx.timeValue = timeFraction32;
	ctx.flags = 0;
	ctx.index = index;
	ctx.objId = CurrentObjectID();
	*ok &= (ctx.ctxObj = ContextState()->GetContextObject(ctx.objId)).main != 0;
	ctx.pView = this;
	if (*ok)
	{
		ctx.pViewObj = &m_objects[ctx.objId.id];
		ctx.pViewObjEx = &m_objectsEx[ctx.objId.id];
	}
	return ctx;
}

#if FULL_ON_SCHEDULING
bool CContextView::GetWitnessPosition(Vec3& pos)
{
	//const SContextObject * pObj = ContextState()->GetContextObject(m_witness);
	SContextObjectRef obj = ContextState()->GetContextObject(m_witness);
	if (obj.main && obj.xtra->hasPosition)
	{
		pos = obj.xtra->position;
		return true;
	}
	return false;
}

bool CContextView::GetWitnessFov(float& fov)
{
	//const SContextObject * pObj = ContextState()->GetContextObject(m_witness);
	SContextObjectRef obj = ContextState()->GetContextObject(m_witness);
	if (obj.main && obj.xtra->hasFov)
	{
		fov = obj.xtra->fov;
		return true;
	}
	return false;
}

bool CContextView::GetWitnessDirection(Vec3& dir)
{
	//const SContextObject * pObj = ContextState()->GetContextObject(m_witness);
	SContextObjectRef obj = ContextState()->GetContextObject(m_witness);
	if (obj.main && obj.xtra->hasDirection)
	{
		dir = obj.xtra->direction;
		NET_ASSERT(fabsf(dir.GetLength() - 1) < 0.01f);
		return true;
	}
	return false;
}
#endif

EMessageSendResult CContextView::WriteHeader(INetSender* pSender)
{
	pSender->BeginMessage(m_config.pUpdatePhysicsTime);
	SPhysicsTime tm(ContextState()->GetLocalPhysicsTime());
	tm.SerializeWith(pSender->ser);
	return eMSR_SentOk;
}

EMessageSendResult CContextView::WriteFooter(INetSender* pSender)
{
	return eMSR_SentOk;
}

bool CContextView::SetPhysicsTime(CTimeValue tm)
{
#if ENABLE_DEBUG_KIT
	// should be before any object updates
	m_updatedObjectsThisFrame.clear();
#endif

	if (m_remotePhysicsTime > tm)
	{
		NetWarning("Physics time went backwards");
	}
	m_remotePhysicsTime = tm;
	return true;
}

void CContextView::SendablesDependentOnObjectAdd(const SNetObjectID& netId, const SSendableHandle& msgHandle)
{
	m_pSendables->insert(std::make_pair(netId, msgHandle));
}

void CContextView::SendablesDependentOnObjectRemove(const SNetObjectID& netId, const SSendableHandle& msgHandle)
{
	std::pair<TSendablesMap::iterator, TSendablesMap::iterator> range = m_pSendables->equal_range(netId);
	for (TSendablesMap::iterator it = range.first; it != range.second; ++it)
	{
		if (it->second == msgHandle)
		{
			m_pSendables->erase(it);
			break;
		}
	}
}

void CContextView::GetSendablesDependentOnObject(SNetObjectID netId, std::vector<SSendableHandle>& out)
{
	std::pair<TSendablesMap::iterator, TSendablesMap::iterator> range = m_pSendables->equal_range(netId);
	for (TSendablesMap::iterator it = range.first; it != range.second; ++it)
	{
		out.push_back(it->second);
	}
}

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
void CContextView::TransmittedVoice(SNetObjectID netID)
{
	ASSERT_GLOBAL_LOCK;
	if (!Context()->IsMultiplayer())
		return;
	if (netID.id >= m_objects.size())
		return;
	NET_ASSERT(m_objects[netID.id].salt == netID.salt);
	m_objectsEx[netID.id].voiceTransmitTime = g_time;

	if (m_pParent)
		m_pParent->TransmittedVoice();
}

void CContextView::ReceivedVoice(SNetObjectID netID)
{
	ASSERT_GLOBAL_LOCK;
	if (!Context()->IsMultiplayer())
		return;
	if (netID.id >= m_objects.size())
		return;
	NET_ASSERT(m_objects[netID.id].salt == netID.salt);
	m_objectsEx[netID.id].voiceReceiptTime = g_time;
}

CTimeValue CContextView::TimeSinceVoiceReceipt(EntityId id)
{
	ASSERT_GLOBAL_LOCK;
	if (!Context() || !ContextState() || !Context()->IsMultiplayer())
		return 100000.0f;
	SNetObjectID netID = ContextState()->GetNetID(id);
	if (netID.id >= m_objects.size())
		return 100000.0f;
	NET_ASSERT(m_objects[netID.id].salt == netID.salt);
	return g_time - m_objectsEx[netID.id].voiceReceiptTime;
}
#endif

CContextViewObjectLock CContextView::LockObject(SNetObjectID id, const char* why)
{
	return CContextViewObjectLock(this, id, ContextState()->LockObject(id, why));
}

bool CContextView::IsIdle()
{
	bool bState = false;
	if (Context())
	{
		bState = (Context()->GetCurrentState() == ContextState());
	}
	return !HasPendingStateChange(true) && bState;
}

#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)
void CContextView::OnVoicePacket(SNetObjectID object, const TVoicePacketPtr& pPkt)
{
	MMM_REGION(m_pMMM);
	if (IsDead())
		return;
	SSendableHandle& handle = m_voiceMessageHandles[object];
	_smart_ptr<CVoiceMsg> pVM = new CVoiceMsg(object, pPkt, m_config.pVoiceDataMsg, this);
	if (Parent()->NetAddSendable(&*pVM, 1, &handle, &handle))
	{
		pVM->SetHandle(handle);
		TransmittedVoice(object);
	}
}
#endif

bool CContextView::HandleRMI(TSerialize ser, ENetReliabilityType reliability, bool client)
{
	SNetObjectID objID;
	if (reliability == eNRT_NumReliabilityTypes)
		objID = CurrentObjectID();
	else
		ser.Value("objID", objID, 'eid');
	return ContextState()->HandleRMI(objID, client, ser, Parent());
}

void CContextView::NotifyPartialUpdate(SNetObjectID id, NetworkAspectID aspectIdx)
{
	if (!IsObjectEnabled(id) || !Context()->IsMultiplayer() || ContextState() != Context()->GetCurrentState())
		return;
	// technically not correct; should be >= 0; but crysis doesn't use aspect 0
	NET_ASSERT(aspectIdx > 0 && aspectIdx < NUM_ASPECTS);
	if (!IsInState(eCVS_InGame))
	{
		(*m_pEarlyPartialUpdateIDs)[id] |= 1 << aspectIdx;
		return;
	}
	SContextViewObjectEx& objex = m_objectsEx[id.id];
	_smart_ptr<CNotifyPartialUpdateMessage> pMsg = new CNotifyPartialUpdateMessage(this, id, aspectIdx, m_config.pPartialUpdate[aspectIdx]);
	Parent()->NetSubstituteSendable(&*pMsg, 0, NULL, &objex.notifyPartialUpdateHandle[aspectIdx]);
}

bool CContextView::PartialAspect(NetworkAspectID aspectIdx, TSerialize ser, uint32, uint32, uint32)
{
	SNetObjectID id;
	NetLogPacketDebug("PartialAspect %d", aspectIdx);
	ser.Value("obj", id, 'eid');
	if (!Context()->IsMultiplayer() || !IsObjectEnabled(id))
		return true; // original behavior
	m_objectsEx[id.id].partialUpdateReceived[aspectIdx] = g_time;
	m_objectsEx[id.id].partialUpdatesRemaining[aspectIdx] = 5;
	ChangedObject(id, 0, 1 << aspectIdx);
	return true;
}

void CContextView::NetDump(ENetDumpType type, INetDumpLogger& logger)
{
#define LOGHEADER NetLog("-- %sContextView: %s ---------------------------------------", IsServer() ? "Server" : "Client", Parent()->GetName())
}

void CContextView::BindAspects(SNetObjectID obj, NetworkAspectType aspects)
{
}

void CContextView::UnbindAspects(SNetObjectID obj, NetworkAspectType aspects)
{
	if (Context()->IsMultiplayer() && IsObjectBound(obj))
	{
		SHistoricalEvent evt(eHE_UnbindAspect);
		evt.objId = obj;
		evt.pView = this;
		evt.pViewObj = &m_objects[obj.id];
		evt.pViewObjEx = &m_objectsEx[obj.id];
		CBitIter iter(aspects);
		NetworkAspectID aspectIdx;
		while (iter.Next(aspectIdx))
		{
			evt.index = aspectIdx;
			BroadcastHistoricalEvent(evt);
		}
	}
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  context views replicate state between contexts
   -------------------------------------------------------------------------
   History:
   - 02/09/2004   12:34 : Created by Craig Tiller


   NOTE: CUpdateMessage is a friend of this class

*************************************************************************/
#ifndef __CONTEXTVIEW_H__
#define __CONTEXTVIEW_H__

#pragma once

#include <queue>
#include <CryNetwork/ISerialize.h>
#include <CryCore/Containers/MiniQueue.h>
#include "Protocol/NetChannel.h"
#include "NetContext.h"
#include "INetContextListener.h"
#include "ContextViewStateManager.h"
#include "VOIP/VoicePacket.h"
#include "VOIP/IVoicePacketListener.h"
#include "ContextEstablisher.h"
#include "Protocol/ICTPEndpointListener.h"
#include "SyncContext.h"
#include "NetContext.h"
#include "STLMementoAllocator.h"
#include <array>

class CNetContext;
class CNetChannel;
class CHistory;
struct SHistoricalEvent;
struct IContextViewExtension;
class CPerformBreakSimpleServer;

#if CHECKING_BIND_REFCOUNTS
	#define CONTEXTVIEW_BOUND_OBJECT_STRING1(x, p) (x "_" + (p)->GetName()).c_str()
#else
	#define CONTEXTVIEW_BOUND_OBJECT_STRING1(x, p) (x)
#endif

#define CONTEXTVIEW_BOUND_OBJECT_STRING(x) CONTEXTVIEW_BOUND_OBJECT_STRING1(x, this)

static const int MAX_BREAK_STREAMS = 6; // must match with compression policy 'brId'

struct SSimpleObjectIDParams
{
	SSimpleObjectIDParams(SNetObjectID id = SNetObjectID()) : netID(id) {}

	SNetObjectID netID;

	void         SerializeWith(TSerialize ser)
	{
		ser.Value("netID", netID, 'eid');
	}
};

struct SRemoveStaticObject
{
	EntityId id;
	void     SerializeWith(TSerialize ser)
	{
		ser.Value("id", id);
	}
};

struct SPhysicsTime
{
	SPhysicsTime() {}
	SPhysicsTime(CTimeValue t) : tm(t) {}
	CTimeValue tm;
	void       SerializeWith(TSerialize ser)
	{
		ser.Value("tm", tm, 'tPhy');
	}
};

// a context view has a state, so that we can sanely do initialization in
// order
struct SContextViewConfiguration
{
	//SContextViewConfiguration() { memset(this, 0, sizeof(*this)); }
	const SNetMessageDef* pFlushMsgsMsg;
	const SNetMessageDef* pChangeStateMsg;
	const SNetMessageDef* pForceStateMsg;
	const SNetMessageDef* pFinishStateMsg;
	const SNetMessageDef* pUpdateMsg;
	const SNetMessageDef* pEndUpdateMsg;
	const SNetMessageDef* pReconfigureMsg;
	const SNetMessageDef* pSetAuthorityMsg;
	const SNetMessageDef* pVoiceDataMsg;
	const SNetMessageDef* pRemoveStaticEntity;
	const SNetMessageDef* pUpdatePhysicsTime;
	const SNetMessageDef* pBeginSyncFiles;
	const SNetMessageDef* pBeginSyncFile;
	const SNetMessageDef* pAddFileData;
	const SNetMessageDef* pEndSyncFile;
	const SNetMessageDef* pAllFilesSynced;
	std::array<const SNetMessageDef*, NumAspects> pPartialUpdate;
	std::array<const SNetMessageDef*, NumAspects> pSetAspectProfileMsgs;
	std::array<const SNetMessageDef*, NumAspects> pUpdateAspectMsgs;
	const SNetMessageDef* pRMIMsgs[eNRT_NumReliabilityTypes + 1];
};

class CContextView;
typedef _smart_ptr<CContextView> CContextViewPtr;
class CContextViewObjectLock;

enum EChangedObjectFlags
{
	eCOF_ForcePreGame    = 2,
	eCOF_NeverSubstitute = 4,
};

class CContextView :
	public INetMessageSink,
	public INetContextListener,
	public CContextViewStateManager,
	public ICTPEndpointListener
#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)
	, public IVoicePacketListener
#endif
	/*private CDefaultStreamAllocator*/
{
	friend class CUpdateMessage;
	friend class CRegularUpdateMessage;
	friend class CPerformBreakSimpleServer;

	struct SAspect;
	struct SExtensionAdder;

protected:
	CMementoMemoryManagerPtr m_pMMM;

public:
	CContextView();
	virtual ~CContextView();

	void OnChannelDestroyed();

#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)
	// IVoicePacketListener
	virtual void AddRef()
	{
		INetContextListener::AddRef();
	}
	virtual void Release()
	{
		INetContextListener::Release();
	}
	virtual void OnVoicePacket(SNetObjectID object, const TVoicePacketPtr& pPkt);
	// ~IVoicePacketListener
#endif

	virtual bool                           IsClient() const { return false; }
	virtual bool                           IsServer() const { return false; }

	void                                   NetDump(ENetDumpType type, INetDumpLogger& logger);

	ILINE const SContextViewConfiguration& GetConfig() const      { return m_config; }
	CHistory*                              GetHistory(EHistory h) { return m_history[h]; }

	EMessageSendResult                     WriteHeader(INetSender* pSender);
	EMessageSendResult                     WriteFooter(INetSender* pSender);
	CTimeValue                             GetRemotePhysicsTime() const { return m_remotePhysicsTime; }

	virtual bool                           IsIdle();

	void                                   BroadcastHistoricalEvent(const SHistoricalEvent& event);

#if FULL_ON_SCHEDULING
	bool GetWitnessPosition(Vec3& pos);
	bool GetWitnessDirection(Vec3& pos);
	bool GetWitnessFov(float& pos);
#endif

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	void       AllowVoiceTransmission(bool allow) { m_allowVoice = allow; }
	bool       GetAllowVoiceTransmission()        { return m_allowVoice; }
	CTimeValue TimeSinceVoiceReceipt(EntityId id);
#endif

	CContextViewObjectLock LockObject(SNetObjectID id, const char* why);

	bool                   HasWitness(EntityId id)
	{
		return m_witness == ContextState()->GetNetID(id);
	}

	virtual void AddWaitForFileSyncComplete(IContextEstablisher* pEst, EContextViewState when) = 0;

	// set an aspect profile
	void SetAspectProfile(SNetObjectID objectID, NetworkAspectType aspect, uint8 profile);

	// INetContextListener
	virtual void            OnObjectEvent(CNetContextState* pState, SNetObjectEvent* pEvent);
	virtual void            OnChannelEvent(CNetContextState* pState, INetChannel* pFrom, SNetChannelEvent* pEvent);
	virtual SObjectMemUsage GetObjectMemUsage(SNetObjectID id);
	virtual void            PerformRegularCleanup();
	virtual INetChannel*    GetAssociatedChannel() { return m_pParent; }
	virtual void            Die();
	virtual bool            IsDead()               { return m_bDead; }

	string                  GetName()
	{
		return m_name;
	}
	// ~INetContextListener

	// ICTPEndpointListener
	virtual void OnEndpointEvent(const SCTPEndpointEvent& evt);
	// ~ICTPEndpointListener

	EContextViewState GetLocalState() const
	{
		return m_currentState;
	};

	// called by NetChannel::Init() to finish setting things up
	virtual void      CompleteInitialization();
	// the context we are replicating
	CNetContext*      Context() const      { return m_pContext; }
	CNetContextState* ContextState() const { return m_pContextState; }

	// fetch our parent channel
	CNetChannel*                 Parent() const;

	ILINE TUpdateMessageBrackets GetUpdateMessageBrackets()
	{
		return TUpdateMessageBrackets(m_config.pUpdateMsg, m_config.pEndUpdateMsg);
	}

	// get memory usage
	// this function must be called by derived classes
	virtual void GetMemoryStatistics(ICrySizer* pSizer);

	// is a message allowed to be sent in the current state??
	// return NULL if ok, error message if not
	virtual const char* ValidateMessage(
	  const SNetMessageDef*,
	  bool bNetworkMsg) = 0;
	// does our remote protocol have a message
	virtual bool HasRemoteDef(const SNetMessageDef* pDef) = 0;

	// is this a local view? (it communicates with another view that is
	// in the same process and using the same CNetContext)
	bool IsLocal() const { return m_bLocal; }
	// set a password on this view
	void SetPassword(const string& password);

	// schedule an attached message
	bool ScheduleAttachment(bool fromChannel, IRMIMessageBodyPtr pMessage, const SAttachmentIndex* pIndex = NULL);
#if ENABLE_DEFERRED_RMI_QUEUE
	void ProcessDeferredRMIList(void);
#endif // ENABLE_DEFERRED_RMI_QUEUE

	// declare a witness
	void                  DeclareWitness(EntityId id);
	SNetObjectID          GetWitness() { return m_witness; }

	const SNetMessageDef* GetRMIDef(ENetReliabilityType reliability) const
	{
		return m_config.pRMIMsgs[reliability];
	}

#if ENABLE_DEBUG_KIT
	virtual TNetChannelID GetLoggingChannelID(TNetChannelID local, TNetChannelID remote) = 0;
#endif

	void PushForcedState(EContextViewState state, bool evenIfLocal);

	// clear all cached state: context is being changed
	virtual void ClearAllState();

	ILINE bool   IsObjectBound(SNetObjectID id) const
	{
		if (m_boundCache && id == m_boundCache)
			return true;
		return IsObjectBound_Slow(id);
	}
	ILINE bool IsObjectEnabled(SNetObjectID id) const
	{
		if (m_enabledCache && id == m_enabledCache)
			return true;
		return IsObjectEnabled_Slow(id);
	}

	CMementoMemoryManager& GetMMM() { return *m_pMMM; }

protected:
	void Init(
	  CNetChannel* pParent,
	  CNetContext* pContext,
	  SContextViewConfiguration* pConfig);
	void SetMMM(CMementoMemoryManagerPtr pMMM) { m_pMMM = pMMM; }

	// CContextViewStateManager
	virtual bool EnterState(EContextViewState state);
	virtual void ExitState(EContextViewState state);
	virtual void OnNeedToSendStateInformation(bool urgently);
	virtual void OnViewStateDisconnect(const char* message);
	// ~CContextViewStateManager

	static const char* GetStateName(EContextViewState state);
	static const char* GetWaitStateName(EContextViewState state);

	// enable synchronization of an object
	void SetSpawnState(SNetObjectID nID, ESpawnState state);

	// what is our password?
	const string& Password() const { return m_password; }

	// revoke the authority for our remote view to control an object
	void SetAuthority(SNetObjectID, bool);

	bool         ReadCurrentObjectID(TSerialize pSerialize, bool forBind);
	bool         ClearCurrentObjectID();
	SNetObjectID CurrentObjectID() { return m_curObjectID; }
	bool         ClearAspects(SNetObjectID netID, NetworkAspectType aspects);

	// generalized message handler for child classes
	bool HandleRMI(TSerialize ser, ENetReliabilityType reliability, bool client);

	// an object was destroyed
	// derived classes should call DoUnbindObject()
	virtual void UnbindObject(SNetObjectID netID) = 0;
	// an object was created
	// this function must be called by derived classes
	virtual void BindObject(SNetObjectID nID) = 0;

	bool         DoUnbindObject(SNetObjectID netID, bool clearSendables);

	// an object has been reconfigured (aspects changed...)
	void         ReconfiguredObject(SNetObjectID netID);

	void         SetName(const string& name) { NET_ASSERT(m_name.empty()); m_name = name; }

	virtual void UnboundObject(SNetObjectID id);

	virtual void InitChannelEstablishmentTasks(IContextEstablisher* pEst);

	// change context:
	// clear all objects and go through our bootstrap states again
	virtual void ChangeContext();

	void         DefineExtensionsProtocol(IProtocolBuilder*);

	void         PolluteObjectAspect(SNetObjectID netID, NetworkAspectID aspectIdx);
	bool         PartialAspect(NetworkAspectID aspectIdx, TSerialize ser, uint32, uint32, uint32);

private:
	virtual bool        ShouldInitContext() = 0;
	virtual const char* DebugString() = 0;

	void                HandleAspectChanges(const std::pair<SNetObjectID, SNetObjectAspectChange>* pChanges);
	// the context has been established locally
	virtual void        EstablishedContext() = 0;
	// bind an aspect
	void                BindAspects(SNetObjectID obj, NetworkAspectType aspects);
	void                UnbindAspects(SNetObjectID obj, NetworkAspectType aspects);
	// the CNetContext has been destroyed!
	// make sure that we don't reference it anymore,
	// and disconnect the channels
	void ContextDestroyed();

	void UpdateSchedulerState(SNetObjectID);
	void OnNoBlockingMessages();

	// get the local ip addresses of this context view
	void GetLocalIPs(TNetAddressVec& vIPs);

	// is it safe to change local state yet?
	//bool CanChangeLocalState();

	void         RemoveRMIListener(IRMIListener* pListener);

	void         DebugEvent(SNetObjectID id, ENetObjectDebugEvent evt);

	virtual void GotBreakage(const SNetIntBreakDescription* pDesc);

protected:
	enum EGetSentAspectsAuthority
	{
		// order matters... see implementation of GetSentAspects
		eGSAA_DefaultAuthority = 0,
		eGSAA_AssumeNoAuthority,
		eGSAA_AssumeAuthority,
	};

	NetworkAspectType GetSentAspects(SNetObjectID, bool assumeEnabled, EGetSentAspectsAuthority assumeAuthority);
	void              TransmittedVoice(SNetObjectID);
	void              ReceivedVoice(SNetObjectID);

	// parameters for change state
	struct SChangeStateMessage : public ISerializable
	{
		EContextViewState state;
		void SerializeWith(TSerialize ser);
	};

#if !defined(OLD_VOICE_SYSTEM_DEPRECATED)
	class CVoiceMsg : public INetMessage
	{
	public:
		CVoiceMsg(SNetObjectID id, TVoicePacketPtr packet, const SNetMessageDef* pDef, CContextViewPtr pView) :
			INetMessage(pDef), m_id(id), m_packet(packet), m_pView(pView)
		{
			SetGroup('talk');
			++g_objcnt.voiceMsg;
		}
		~CVoiceMsg()
		{
			--g_objcnt.voiceMsg;
		}

		virtual EMessageSendResult WritePayload(
		  TSerialize serialize,
		  uint32 nCurrentSeq,
		  uint32 nBasisSeq)
		{
			serialize.Value("object", m_id, 'eid');
			m_packet->Serialize(serialize);
			return eMSR_SentOk;
		}

		virtual void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate state)
		{
			if (state != eNSSU_Requeue)
			{
				std::pair<TSendablesMap::iterator, TSendablesMap::iterator> range = m_pView->m_pSendables->equal_range(m_id);
				for (TSendablesMap::iterator it = range.first; it != range.second; ++it)
				{
					if (it->second == m_handle)
					{
						m_pView->m_pSendables->erase(it);
						break;
					}
				}
			}
		}

		virtual size_t GetSize() { return sizeof(*this); }

		void           GetPositionInfo(SMessagePositionInfo& pos)
		{
	#if FULL_ON_SCHEDULING
			if (!m_pView || m_pView->IsDead())
				return;
			CNetContextState* pCtx = m_pView->ContextState();
			NET_ASSERT(pCtx);
			const SContextObjectRef obj = pCtx->GetContextObject(m_id);
			if (obj.main && obj.xtra)
			{
				pos.havePosition = obj.xtra->hasPosition;
				pos.position = obj.xtra->position;
				pos.haveDrawDistance = obj.xtra->hasDrawDistance;
				pos.drawDistance = obj.xtra->drawDistance;
			}
	#endif
		}

		void SetHandle(SSendableHandle handle)
		{
			m_handle = handle;
			m_pView->m_pSendables->insert(std::make_pair(m_id, m_handle));
		}

	private:
		SNetObjectID    m_id;
		TVoicePacketPtr m_packet;
		CContextViewPtr m_pView;
		SSendableHandle m_handle;
	};
#endif

	class CNotifyPartialUpdateMessage;

	bool            HaveAuthorityOfObject(SNetObjectID id) const;
	bool            IgnoringCurrentObject() const { return m_ignoringCurObject; }
	bool            UpdateAspect(NetworkAspectID i, TSerialize pSerialize, uint32 nCurSeq, uint32 nOldSeq, uint32 timeFraction32);
	SReceiveContext CreateReceiveContext(TSerialize ser, NetworkAspectID index, uint32 nCurSeq, uint32 nOldSeq, uint32 timeFraction32, bool* ok);
	bool            SetPhysicsTime(CTimeValue tm);
	void            StartFlushUpdates() { m_flushUpdates = true; CancelUpdates(); }

	typedef std::vector<SContextViewObject>   SObjects;
	typedef std::vector<SContextViewObjectEx> SObjectsEx;
	typedef std::vector<CNetObjectBindLock>   CNetObjectBindLocks;
	SObjects            m_objects;
	SObjectsEx          m_objectsEx;
	CNetObjectBindLocks m_objectLocks;

	void ChangedObject(SNetObjectID id, uint32 flags, NetworkAspectType dirtyAspects);

	void SendablesDependentOnObjectAdd(const SNetObjectID& netId, const SSendableHandle& msgHandle);
	void SendablesDependentOnObjectRemove(const SNetObjectID& netId, const SSendableHandle& msgHandle);
	void GetSendablesDependentOnObject(SNetObjectID id, std::vector<SSendableHandle>& out);

private:
	void GC_GetEstablishmentOrder();
	void GC_Lazy_RunEstablisher(EContextEstablishTaskResult (CContextEstablisher::* func)(SContextEstablishState&), EContextViewState state);
	void GC_Lazy_StateSink(_smart_ptr<CNetContextState> ) {}

	CHistory*                           m_history[eH_NUM_HISTORIES];

	CNetChannel*                        m_pParent;
	CNetContext*                        m_pContext;
	_smart_ptr<CNetContextState>        m_pContextState;

	unsigned                            m_eventMask;

	string                              m_password;
	bool                                m_bLocal;
	uint32                              m_nAttachmentIndex;

	bool                                m_bDead;
	EContextViewState                   m_currentState;

	std::vector<EContextViewState>      m_forcedStates;

	bool GetDependentMessageIDForObject(SNetObjectID id, int& handle);
	bool CheckDependentId(EntityId id, SSendableHandle* hdl, bool allowNotBound, CNetObjectBindLock* pLk);
	void CancelUpdates();
	void PrimeUpdates();
	void NotifyPartialUpdate(SNetObjectID id, NetworkAspectID aspectIdx);
	void ClearHistory(SNetObjectID id);
	bool IsContextCurrent()
	{
		return m_pContext->GetCurrentState() == m_pContextState;
	}

	virtual void   OnWitnessDeclared() = 0;

	virtual uint32 FilterEventMask(uint32 mask, EContextViewState state)
	{
		return mask;
	}

	typedef std::map<SAttachmentIndex, IRMIMessageBodyPtr, std::less<SAttachmentIndex>, STLMementoAllocator<std::pair<const SAttachmentIndex, IRMIMessageBodyPtr>>> TAttachmentMap;
	std::auto_ptr<TAttachmentMap> m_pAttachments[2];

#if ENABLE_DEFERRED_RMI_QUEUE
	typedef std::vector<IRMIMessageBodyPtr> TDeferredRMIVec;
	TDeferredRMIVec m_deferredRMI;
#endif // ENABLE_DEFERRED_RMI_QUEUE

protected:
#if USE_SYSTEM_ALLOCATOR
	typedef std::multimap<SNetObjectID, SSendableHandle, std::less<SNetObjectID>>                                                                TSendablesMap;
#else
	typedef std::multimap<SNetObjectID, SSendableHandle, std::less<SNetObjectID>, STLMementoAllocator<std::pair<const SNetObjectID, SSendableHandle>>> TSendablesMap;
#endif
	std::auto_ptr<TSendablesMap> m_pSendables;

private:
	typedef std::vector<IContextViewExtension*> TExtensionsSet;
	TExtensionsSet m_extensions;

	string         m_name;

	// decode state
	SNetObjectID                             m_curObjectID;
	bool                                     m_ignoringCurObject;
	bool                                     m_flushUpdates;
	bool                                     m_allowVoice;

	SNetObjectID                             m_witness;
	VectorMap<SNetObjectID, SSendableHandle> m_voiceMessageHandles;
	typedef std::map<SNetObjectID, NetworkAspectType, std::less<SNetObjectID>, STLMementoAllocator<std::pair<const SNetObjectID, NetworkAspectType>>> TEarlyPartialUpdateIDs;
	std::auto_ptr<TEarlyPartialUpdateIDs>    m_pEarlyPartialUpdateIDs;

	CTimeValue                               m_remotePhysicsTime;

	bool IsObjectBound_Slow(SNetObjectID id) const
	{
		if (m_objectLocks.size() > id.id)
		{
			if (m_objects.size() > id.id && m_objects[id.id].salt != id.salt)
				;
			else if (!m_objectLocks[id.id].Empty())
			{
				m_boundCache = id;
				return true;
			}
		}
		return false;
	}

	bool IsObjectEnabled_Slow(SNetObjectID id) const
	{
		if (m_objects.size() > id.id)
		{
			if (m_objects[id.id].spawnState == eSS_Enabled && id.salt == m_objects[id.id].salt)
			{
				m_enabledCache = id;
				return true;
			}
		}
		return false;
	}

	mutable SNetObjectID m_boundCache;
	mutable SNetObjectID m_enabledCache;

#if ENABLE_DEBUG_KIT
	std::set<uint16> m_updatedObjectsThisFrame;
	SNetObjectID     m_acceptedForUpdateObject;
#endif

	class CRMIMessage_Script;
	class CRMIMessage_UserDef;
	class CRMIMessageAllocator;
	class CCET_PrimeUpdates;
	class CCET_ClearAllState;
	class CCET_SafetySleep;
	class CCET_UnlockStateChanges;

	CChangeStateLock          m_establishmentLock;

	SContextViewConfiguration m_config;

	friend class CContextViewObjectLock;
	void IncObjLock(SNetObjectID id)
	{
		NET_ASSERT(IsObjectBound(id));
		m_objects[id.id].locks++;
		NET_ASSERT(m_objects[id.id].locks);
	}
	void DecObjLock(SNetObjectID id)
	{
		if (!IsObjectBound(id))
			return;
		NET_ASSERT(m_objects[id.id].locks);
		m_objects[id.id].locks--;
	}
};

class CContextViewObjectLock
{
public:
	CContextViewObjectLock() : m_pContext(0) {}
	CContextViewObjectLock(CContextView* pContext, SNetObjectID objId, const CNetObjectBindLock& lk) : m_pContext(pContext), m_objId(objId), m_lk(lk)
	{
		assert((bool)m_objId);
		m_pContext->IncObjLock(m_objId);
	}
	CContextViewObjectLock(const CContextViewObjectLock& lk)
		: m_pContext(lk.m_pContext)
		, m_objId(lk.m_objId)
		, m_lk(lk.m_lk)
	{
		if (m_objId)
			m_pContext->IncObjLock(m_objId);
	}
	~CContextViewObjectLock()
	{
		if (m_objId)
			m_pContext->DecObjLock(m_objId);
	}
	void Swap(CContextViewObjectLock& lk)
	{
		m_pContext.swap(lk.m_pContext);
		std::swap(m_objId, lk.m_objId);
		m_lk.Swap(lk.m_lk);
	}
	CContextViewObjectLock& operator=(CContextViewObjectLock lk)
	{
		Swap(lk);
		return *this;
	}

private:
	_smart_ptr<CContextView> m_pContext;
	SNetObjectID             m_objId;
	CNetObjectBindLock       m_lk;
};

class CWaitForEnabled : public INetSendable
{
public:
	CWaitForEnabled(SNetObjectID obj, _smart_ptr<CContextView> pView) : INetSendable(0, eNRT_ReliableUnordered), m_lk(pView->ContextState()->LockObject(obj, "WaitForEnabled")), m_obj(obj), m_pView(pView)
	{
		SetPriorityDelta(-32);
		++g_objcnt.waitForEnabled;
	}
	~CWaitForEnabled()
	{
		--g_objcnt.waitForEnabled;
	}

	virtual size_t      GetSize()                                                    { return sizeof(*this); }
	virtual void        UpdateState(uint32 nFromSeq, ENetSendableStateUpdate update) {}
	virtual const char* GetDescription()
	{
		if (m_name.empty())
		{
			SContextObjectRef obj = m_pView->ContextState()->GetContextObject(m_obj);
			const char* name = "<<unknown>>";
			if (obj.main)
				name = obj.main->GetName();
			m_name.Format("WaitFor %s (%s) Enabled", name, m_obj.GetText());
		}
		return m_name.c_str();
	}
	virtual void GetPositionInfo(SMessagePositionInfo& pos) {}

#if ENABLE_PACKET_PREDICTION
	SMessageTag GetMessageTag(INetSender* pSender, IMessageMapper* mapper)
	{
		SMessageTag mTag;
		mTag.messageId = 0xFFFFFFFF;
		return mTag;
	}
#endif

	EMessageSendResult Send(INetSender* pSender)
	{
		SContextObjectRef ctxobj = m_pView->ContextState()->GetContextObject(m_obj);
		if (!ctxobj.main || !ctxobj.main->userID)
		{
			NetWarning("CWaitForEnabled: awaited object %s is not bound", m_obj.GetText());
			return eMSR_SentOk;
		}
		/*
		    if (!m_pView->IsObjectBound(m_obj))
		    {
		      NetWarning("CWaitForEnabled: awaited object %s is not bound", m_obj.GetText());
		      return eMSR_SentOk;
		    }
		 */
		if (!m_pView->IsObjectEnabled(m_obj))
		{
			return eMSR_NotReady;
		}
		return eMSR_SentOk;
	}

private:
	CNetObjectBindLock       m_lk;
	SNetObjectID             m_obj;
	string                   m_name;
	_smart_ptr<CContextView> m_pView;
};

// Template helpers for compile-time generation of arrays of aspect-related message definitions.
typedef unsigned char aspect_num;
template<template<int> class Func, aspect_num... values>
struct pack_to_array { static std::array<const SNetMessageDef*, sizeof...(values)> value; };

template<template<int> class Func, aspect_num... values>
std::array<const SNetMessageDef*, sizeof...(values)> pack_to_array<Func, values...>::value
	= {{ Func<values>::fun()... }}; // double braces for Clang

template<template<int> class Func, aspect_num k, aspect_num... values>
struct static_array : static_array<Func, k - 1, k - 1, values...> {};

template<template<int> class Func, aspect_num... values>
struct static_array<Func, 0, values...> : pack_to_array<Func, values...> {};

// Helper function for aspect-related messages, used by both client- and server-
// context views. Addresses of these static trampolines are stored in protocol's
// message definitions and are called on incoming packets. This trampoline 
// redirects the call to the respective instance.
template<int AspectNum, class T, bool (T::*memf)(NetworkAspectID, TSerialize, uint32, uint32, uint32)>
TNetMessageCallbackResult TrampolineAspect(uint32, INetMessageSink *instance,
	TSerialize serialize, uint32 curSeq, uint32 oldSeq, uint32 timeFraction32, EntityId *, INetChannel *)
{
	bool ret = (static_cast<T*>(instance)->*memf)(AspectNum, serialize, curSeq, oldSeq, timeFraction32);
	return TNetMessageCallbackResult(ret, (INetAtSyncItem*)NULL);
}

#endif

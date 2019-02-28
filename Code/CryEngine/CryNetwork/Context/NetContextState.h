// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NETCONTEXTSTATE_H__
#define __NETCONTEXTSTATE_H__

#pragma once

#include <CryNetwork/INetwork.h>
#include "INetContextState.h"
#include "INetContextListener.h"
#include "ContextEstablisher.h"
#include "ChangeList.h"
#include "STLMementoAllocator.h"

class CNetContext;
typedef _smart_ptr<CNetContext> CNetContextPtr;
class CNetChannel;
class CUpdateDisableManager;
class CNetEntityDebug;
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
class CVoiceContext;
#endif
class CPerformBreakSimpleServer;

enum EUnbindObjectFlags
{
	eUOF_CallGame       = BIT(0),
	eUOF_AllowCollected = BIT(2),
};

class CNetContextState : public INetContextState
{
	friend class CPerformBreakSimpleServer;
	CMementoMemoryManagerPtr m_pMMM;

public:
	CNetContextState(CNetContext*, int token, CNetContextState* pPrev);
	~CNetContextState();

	virtual void Die();
	bool         IsDead();

	class CNetObjectBindLock
	{
	public:
		CNetObjectBindLock() : m_pContext(0), m_msg("INVALID") {}
		CNetObjectBindLock(CNetContextState* pContext, SNetObjectID objId, const char* msg) : m_pContext(pContext), m_objId(objId), m_msg(msg)
		{
			assert((bool)m_objId);
			m_pContext->BoundObject(m_objId, m_msg);
		}
		CNetObjectBindLock(const CNetObjectBindLock& lk) : m_pContext(lk.m_pContext), m_objId(lk.m_objId), m_msg(lk.m_msg)
		{
			if (m_objId)
				m_pContext->BoundObject(m_objId, m_msg);
		}
		~CNetObjectBindLock()
		{
			if (m_objId)
				m_pContext->UnboundObject(m_objId, m_msg);
		}
		void Swap(CNetObjectBindLock& lk)
		{
			m_pContext.swap(lk.m_pContext);
			std::swap(m_objId, lk.m_objId);
			std::swap(m_msg, lk.m_msg);
		}
		CNetObjectBindLock& operator=(CNetObjectBindLock lk)
		{
			Swap(lk);
			return *this;
		}
		bool Empty() const
		{
			return !m_objId;
		}
		SNetObjectID GetID() const
		{
			return m_objId;
		}
		void ChangeContextState(CNetContextState* state)
		{
			m_pContext = state;
		}

	private:
		_smart_ptr<CNetContextState> m_pContext;
		SNetObjectID                 m_objId;
		const char*                  m_msg;
	};

	ILINE SContextObjectRef GetContextObject(SNetObjectID nID) const
	{
		if (nID == m_cacheObjectID)
			return m_cacheObjectRef;
		else
			return GetContextObject_Slow(nID);
	}

	bool LockObject(SNetObjectID id, int /*EContextObjectLock*/ lock)
	{
		NET_ASSERT(GetContextObject(id).main);
		uint8 old = (uint8)(m_vObjects[id.id].vLocks[lock]++);
		NET_ASSERT(m_vObjects[id.id].vLocks[lock]);
		return old == 0;
	}
	bool UnlockObject(SNetObjectID id, int /*EContextObjectLock*/ lock)
	{
		NET_ASSERT(GetContextObject(id).main);
		NET_ASSERT(m_vObjects[id.id].vLocks[lock]);
		return --m_vObjects[id.id].vLocks[lock] == 0;
	}
	bool IsLocked(SNetObjectID id, int /*EContextObjectLock*/ lock)
	{
		NET_ASSERT(GetContextObject(id).main);
		return m_vObjects[id.id].vLocks[lock] != 0;
	}
	CNetObjectBindLock LockObject(SNetObjectID, const char* why);

	EntityId GetSpawnedObjectId(bool pull)
	{
		//		NET_ASSERT(m_spawnedObjectId);
		EntityId out = m_spawnedObjectId;
		if (pull)
			m_spawnedObjectId = 0;
		return out;
	}

	typedef const EntityId* RemovedStaticObjectIterator;
	RemovedStaticObjectIterator RemovedStaticObjectBegin() const
	{
		if (m_removedStaticEntities.empty())
			return 0;
		return &*m_removedStaticEntities.begin();
	}
	RemovedStaticObjectIterator RemovedStaticObjectEnd() const
	{
		if (m_removedStaticEntities.empty())
			return 0;
		return &*m_removedStaticEntities.end();
	}

	// warning: for debug code only
	// gather all objects in the map
	void                   GetAllObjects(std::vector<SNetObjectID>& objs);

	int                    GetToken() { return m_token; }
	void                   RebindObject(SNetObjectID netId, EntityId userId);
	void                   ChangeSubscription(INetContextListenerPtr pListener, unsigned events);
	void                   ChangeSubscription(INetContextListenerPtr pListener, INetChannel* pChannel, unsigned events);
	void                   AttachRMILogger(IRMILogger* pLogger);
	void                   DetachRMILogger(IRMILogger* pLogger);
	void                   AddWaitForAllowViews(IContextEstablisher* pEst);
	bool                   DoSetAspectProfile(EntityId id, NetworkAspectType aspectBit, uint8 profile, bool checkOwnership, bool informedGame, bool unlockUpdate);
	void                   BroadcastChannelEvent(INetChannel* pFrom, SNetChannelEvent* pEvent);
	bool                   SendEventToChannelListener(INetChannel* pChannel, SNetObjectEvent* pEvent);
	bool                   IsContextEstablished() const { return m_established; }
	SNetObjectID           GetNetID(EntityId userID, bool ensureNotUnbinding = true);
	EntityId               GetEntityID(SNetObjectID netID);
	CTimeValue             GetLocalPhysicsTime() const { return m_localPhysicsTime; }
	INetContextListenerPtr FindListener(const char* name);
	void                   UpdateAuthority(SNetObjectID id, bool bAuth, bool bLocal);
	bool                   AllocateObject(EntityId userID, SNetObjectID netID, NetworkAspectType aspectBits, bool bOwned, ESpawnType spawnType, INetContextListenerPtr pController);
	bool                   UnbindObject(SNetObjectID id, uint32 flags);
	void                   UnbindStaticObject(EntityId id);
	void                   ForceUnbindObject(SNetObjectID id);
	// reconfigure an objects enabled aspects status
	void                   ReconfigureObject(SNetObjectID netID, NetworkAspectType aspects);
	// toggle some aspects on or off
	void                   TurnAspectsOn(SNetObjectID netID, NetworkAspectType aspects);
	void                   TurnAspectsOff(SNetObjectID netID, NetworkAspectType aspects);
	bool                   HandleRMI(SNetObjectID objID, bool bClient, TSerialize ser, CNetChannel* pNetChannel);
	void                   NotifyGameOfAspectUpdate(SNetObjectID objID, NetworkAspectID aspectIdx, CNetChannel* pChannel, CTimeValue remoteTime);
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	IVoiceGroup*           CreateVoiceGroup();
#endif
	// broadcast an event
	void                   Broadcast(SNetObjectEvent* pEvent);
	CMementoMemoryManager& GetStateMMM() { return *m_pMMM; }
#if ENABLE_THIN_BINDS
	void                   UpdateBindAspectMask(SNetObjectID& netID, NetworkAspectType dirtyAspects);
	NetworkAspectType      GetBindAspectMask(SNetObjectID& netID);
#endif // ENABLE_THIN_BINDS

	void          LogRMI(const char* function, ISerializable* pParams);
	void          LogCppRMI(EntityId id, IRMICppLogger* pLogger);
	void          DeclareAspect(const char* name, NetworkAspectType aspectBit, uint8 aspectFlags);
	void          SetDelegatableMask(EntityId id, NetworkAspectType aspectBits);
	void          SetDelegatableMask(SNetObjectID netID, NetworkAspectType aspectBits);
	void          BindObject(EntityId id, EntityId parentId, NetworkAspectType aspectBits, bool bStatic);
	bool          UnbindObject(EntityId id);
	void          EnableAspects(EntityId id, NetworkAspectType aspectBits, bool enabled);
	void          DelegateAuthority(EntityId id, INetChannel* pControlling);
	void          ChangedAspects(EntityId id, NetworkAspectType aspectBits);
#if FULL_ON_SCHEDULING
	void          ChangedTransform(EntityId id, const Vec3& pos, const Quat& rot, float drawDist);
	void          ChangedFov(EntityId id, float fov);
#endif
	void          EstablishedContext();
	void          DisableAspectsUntilObjectUpdated(EntityId idDisable, NetworkAspectType aspectBits, EntityId idReference, INetChannel* pChannel);
	void          SpawnedObject(EntityId userID);
	bool          IsBound(EntityId userID);
#if RESERVE_UNBOUND_ENTITIES
	EntityId      AddReservedUnboundEntityMapEntry(uint16 partialNetID, EntityId eID);
	EntityId      RemoveReservedUnboundEntityMapEntry(uint16 partialNetID);
	EntityId      GetReservedUnboundEntityMapEntry(uint16 partialNetID);
#endif
	void          RemoveRMIListener(IRMIListener* pListener);
	bool          RemoteContextHasAuthority(INetChannel* pChannel, EntityId id);
	void          SetAspectProfile(EntityId id, NetworkAspectType aspectBit, uint8 profile);
	uint8         GetAspectProfile(EntityId id, NetworkAspectType aspectBit);
	void          SetParentObject(EntityId objId, EntityId parentId);
	void          LogBreak(const SNetBreakDescription& breakage);
	bool          SetSchedulingParams(EntityId objId, uint32 normal, uint32 owned);
	void          PulseObject(EntityId objId, uint32 pulseType);
	void          EnableBackgroundPassthrough(bool enable);
	int           RegisterPredictedSpawn(INetChannel* pChannel, EntityId id);
	void          RegisterValidatedPredictedSpawn(INetChannel* pChannel, int predictionHandle, EntityId id);
	void          SafelyUnbind(EntityId id);
	void          Resaltify(SNetObjectID& id);
	void          GetMemoryStatistics(ICrySizer*);
	void          GetProfilingStatistics(SNetworkProfilingStats* const pProfilingStats);
	void          RequestRemoteUpdate(EntityId id, NetworkAspectType aspects);

	void          RegisterEstablisher(INetContextListenerPtr pListener, CContextEstablisherPtr pEst);
	void          SetEstablisherState(INetContextListenerPtr pListener, EContextViewState state);

	bool          AllInOrPastState(EContextViewState state);
	IGameContext* GetGameContext();

	void          FetchAndPropogateChangesFromGame(bool allowFetch);
	void          PropogateChangesToGame();
	void          PropogateProfileChangesToGame();
	void          PerformRegularCleanup();
	void          DrawDebugScreens();
	void          NetDump(ENetDumpType type);

	void GC_UnboundObject(EntityId);
	void GC_BeginContext(CTimeValue);
	void GC_ControlObject(SNetObjectID, bool, CNetObjectBindLock);
	void GC_BoundObject(const EntityId);
	void GC_SendPostSpawnEntities(CContextViewPtr pView);
	void GC_SetAspectProfile(NetworkAspectType, uint8, SNetObjectID, CNetObjectBindLock);
	void GC_EndContext();
	void GC_SetParentObject(EntityId objId, EntityId parentId);
	void GC_PulseObject(EntityId objId, uint32 pulseType);
	void GC_CompleteUnbind(EntityId id);

	void GC_Lazy_TickEstablishers();

	// queued callbacks from game
	void NC_DelegateAuthority(EntityId, INetChannel*);
	void NC_SetAspectProfile(EntityId userID, NetworkAspectType aspectBit, uint8 profile, bool lockedUpdate);
	void NC_ChangedAspects(EntityId userID, NetworkAspectType aspectBit);
#if FULL_ON_SCHEDULING
	void NC_ChangedTransform(EntityId, Vec3, Quat, float);
	void NC_ChangedFov(EntityId, float);
#endif
	void NC_BroadcastSimpleEvent(ENetObjectEvent event);
	void NC_RequestRemoteUpdate(EntityId, NetworkAspectType);

private:
	// an event subscription
	struct SSubscription
	{
		SSubscription() : events(0), pListener(0) {}
		SSubscription(INetContextListenerPtr p, unsigned e) : events(e), pListener(p) {}
		unsigned               events;
		INetContextListenerPtr pListener;
	};
	typedef std::vector<SSubscription> TSubscriptions;

	// information about a particular disable
	struct SDisableAspectInfo
	{
		SDisableAspectInfo() : id(0), aspectBits(0) {}
		EntityId          id;
		NetworkAspectType aspectBits;
	};

	struct STimedDisableAspectInfo
	{
		CTimeValue finish;
		int        handle;

		bool operator<(const STimedDisableAspectInfo& rhs) const
		{
			return finish > rhs.finish;
		}
	};

	struct SChannelChange
	{
		SNetObjectID            obj;
		NetworkAspectID         aspectIdx;
		_smart_ptr<CNetChannel> pChannel;
		CTimeValue              channelTime;
		CTimeValue              receiveTime;

		ILINE SChannelChange(SNetObjectID obj, NetworkAspectID aspectIdx, const _smart_ptr<CNetChannel>& pChannel, CTimeValue channelTime)
		{
			this->obj = obj;
			this->aspectIdx = aspectIdx;
			this->pChannel = pChannel;
			this->channelTime = channelTime;
			this->receiveTime = g_time;
		}
		ILINE SChannelChange() : aspectIdx(0), pChannel(), channelTime(0.0f) {}

		struct SCompareObjectsThenReceiveTime
		{
			ILINE bool operator()(const SChannelChange& a, const SChannelChange& b) const
			{
				if (a.obj < b.obj)
					return true;
				else if (a.obj > b.obj)
					return false;
				else if (a.aspectIdx < b.aspectIdx)
					return true;
				else if (a.aspectIdx > b.aspectIdx)
					return false;
				else if (a.receiveTime > b.receiveTime) // most recently received change goes first
					return true;
				else if (a.receiveTime < b.receiveTime)
					return false;
				return false;
			}
		};

		struct SCompareObjects
		{
			ILINE bool operator()(const SChannelChange& a, const SChannelChange& b) const
			{
				if (a.obj < b.obj)
					return true;
				else if (a.obj > b.obj)
					return false;
				else if (a.aspectIdx < b.aspectIdx)
					return true;
				else if (a.aspectIdx > b.aspectIdx)
					return false;
				return false;
			}
		};

		struct SCompareChannelThenTimeThenObject
		{
			bool operator()(const SChannelChange& a, const SChannelChange& b) const
			{
				if (a.pChannel < b.pChannel)
					return true;
				else if (a.pChannel > b.pChannel)
					return false;
				else if (a.aspectIdx < b.aspectIdx)
					return true;
				else if (a.aspectIdx > b.aspectIdx)
					return false;
				else if (a.channelTime < b.channelTime)
					return true;
				else if (a.channelTime > b.channelTime)
					return false;
				return false;
			}
		};
	};

	class CPropogateChangesToGameContext;
	class CDebugKit_WorldUpdate;

	struct SChangedProfile
	{
		CNetObjectBindLock obj;
		NetworkAspectType  aspect;
		uint8              profile;
	};

	enum EContextEstabliserPhase
	{
		eCEP_Fresh,
		eCEP_Working,
		eCEP_Dead,
	};
	struct SContextEstablisher
	{
		SContextEstablisher(CContextEstablisherPtr p = CContextEstablisherPtr()) : pEst(p), state(eCVS_Initial), phase(eCEP_Fresh) {}
		CContextEstablisherPtr  pEst;
		EContextViewState       state;
		EContextEstabliserPhase phase;
	};

	typedef std::vector<SContextObject>   TContextObjects;
	typedef std::vector<SContextObjectEx> TContextObjectsEx;
	typedef std::vector<SNetObjectID>     TNetObjectIDs;
	typedef std::vector<SChangedProfile>  TChangedProfiles;
#if RESERVE_UNBOUND_ENTITIES
	typedef VectorMap<uint16, EntityId>   TReservedUnboundEntityMap;
#endif

	SContextObjectRef GetContextObject_Slow(SNetObjectID nID) const;

	void              BroadcastChannelEventTo(const TSubscriptions&, INetChannel* pFrom, SNetChannelEvent* pEvent);
	void              SendBindEventsTo(INetContextListenerPtr pListener, bool alsoAuth);
	void              SendBindAspectsTo(INetContextListenerPtr pListener);
	void              SendSetAspectProfileEventsTo(INetContextListenerPtr pListener);
	void              SendRemoveStaticEntitiesTo(INetContextListenerPtr pListener);
	void              SendBreakageTo(INetContextListenerPtr pListener);
	void              HandleSubscriptionDelta(INetContextListenerPtr pListener, unsigned oldEvents, unsigned newEvents);
	bool              SendEventToListener(INetContextListenerPtr pListener, SNetObjectEvent* pEvent);
	static unsigned   ChangeSubscription(TSubscriptions& subscriptions, INetContextListenerPtr pListener, unsigned events);
	void              MarkObjectChanged(SNetObjectID id, NetworkAspectType aspectsChanged);
	void              HandleAspectChanges(EntityId id, NetworkAspectType aspectsChanged);
	NetworkAspectType UpdateAspectData(SNetObjectID id, NetworkAspectType fetchAspects);
	void              LogChangedProfile(SNetObjectID id, NetworkAspectType aspect, uint8 profile);

	int64 m_netAspectSendTime;
	int   m_lastPacketSendRate;

	// null if we've been retired
	CNetContextPtr m_pContext;
	IGameContext*  m_pGameContext;
	bool           m_multiplayer;
	bool           m_established;
	// in PerformRegularCleanup
	bool           m_bInCleanup;
	int            m_token;

	// have we any context listeners in game
	bool                                   m_bInGame;
	size_t                                 m_cleanupMember;

	TSubscriptions                         m_subscriptions;
	std::map<INetChannel*, TSubscriptions> m_channelSubscriptions;
	std::vector<IRMILogger*>               m_rmiLoggers;
	TSubscriptions                         m_tempSubscriptions;

	TContextObjects                        m_vObjects;
	TContextObjectsEx                      m_vObjectsEx;
	CChangeList<SNetObjectAspectChange>    m_vGameChangedObjects;
	std::vector<SChannelChange>            m_vNetChangeLog;
	std::vector<SChannelChange>            m_vNetChangeLogUnique;
	std::priority_queue<uint16, std::vector<uint16>, std::greater<uint16>> m_freeLowBitObjects;
	std::priority_queue<uint16, std::vector<uint16>, std::greater<uint16>> m_freeMediumBitObjects;
	std::priority_queue<uint16, std::vector<uint16>, std::greater<uint16>> m_freeHighBitObjects;
	TChangedProfiles          m_changedProfiles;
	std::vector<EntityId>     m_removedStaticEntities;
#if RESERVE_UNBOUND_ENTITIES
	TReservedUnboundEntityMap m_ReservedUnboundEntityMap;
#endif

	struct SAspectProfileCacheEntry
	{
		SNetObjectID      id;
		NetworkAspectType aspect;
		uint8             profile;
	};

	typedef std::vector<SAspectProfileCacheEntry> TAspectProfileCache;
	TAspectProfileCache m_aspectProfileCache;

	void CacheAspectProfileChange(SNetObjectID id, NetworkAspectType aspect, uint8 profile);
	bool GetAspectProfileFromCache(SNetObjectID id, NetworkAspectType aspect, uint8& profile);

	mutable SNetObjectID      m_cacheObjectID; // id that's going to be valid (no need to recheck)
	mutable SContextObjectRef m_cacheObjectRef;
	void ClearContextObjectCache() { m_cacheObjectID = SNetObjectID(); m_cacheObjectRef = SContextObjectRef(); }

	void AddToFreeObjects(uint16 id);
	void ClearFreeObjects();

#if USE_SYSTEM_ALLOCATOR
	typedef std::unordered_map<EntityId, SNetObjectID, stl::hash_uint32>                                                                                      TNetIDMap;
#else
	typedef std::unordered_map<EntityId, SNetObjectID, stl::hash_uint32, std::equal_to<uint32>, STLMementoAllocator<std::pair<const EntityId, SNetObjectID>>> TNetIDMap;
#endif
	std::auto_ptr<TNetIDMap> m_pNetIDs;

	CTimeValue               m_localPhysicsTime;

	typedef VectorMap<INetContextListenerPtr, SContextEstablisher, std::less<INetContextListenerPtr>, stl::STLGlobalAllocator<std::pair<const INetContextListenerPtr, SContextEstablisher>>> EstablishersMap;
	EstablishersMap m_allEstablishers;
	EstablishersMap m_currentEstablishers;

	// temp. buffer for changelists
	CChangeList<SNetObjectAspectChange>::Objects m_changeBuffer;

	// temp store for spawned object id
	EntityId m_spawnedObjectId;

	typedef std::list<SNetIntBreakDescription, STLMementoAllocator<SNetIntBreakDescription>> TNetIntBreakDescriptionList;
	std::auto_ptr<TNetIntBreakDescriptionList> m_pLoggedBreakage;

	// called by the view when the object is really destroyed remotely
	// (so we can wait for all objects before resetting)
	// returns true if the object was really unbound
	bool UnboundObject(SNetObjectID nID, const char* reason);
	void BoundObject(SNetObjectID nID, const char* reason)
	{
		LockObject(nID, eCOL_Reference);
		// debug
#if CHECKING_BIND_REFCOUNTS
		m_vObjects[nID].debug_whosLocking[reason]++;
#endif
		//NetLog("REF + %d %s [%d]", nID, reason, m_vObjects[nID].vLocks[eCOL_Reference]);
		// ~debug
	}
	void SendEventTo(SNetObjectEvent* event, INetContextListener* pListener);
};

typedef CNetContextState::CNetObjectBindLock CNetObjectBindLock;

#endif

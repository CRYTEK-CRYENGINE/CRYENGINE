// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  manages state that is shared between a group of channels
               (primarily a set of ISynchedObjects to be replicated)
   -------------------------------------------------------------------------
   History:
   - 02/09/2004   12:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __NETCONTEXT_H__
#define __NETCONTEXT_H__

#pragma once

#include "Config.h"
#include <CryNetwork/INetwork.h>
#include <queue>
#include <memory>
#include "INetContextListener.h"
#include <CryNetwork/NetAddress.h>
#include "INetworkMember.h"
#include "Streams/ArithStream.h"
#include "SessionID.h"
#include "ChangeList.h"
#include "ContextEstablisher.h"
#include "MementoMemoryManager.h"
#include "Protocol/NetChannel.h"
#include "Compression/CompressionManager.h"
#include "NetContextState.h"

class CContextView;
class CServerContextView;
class CClientContextView;
class CNetEntityDebug;
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
class CVoiceContext;
#endif
class CMessageBuffer;
class CNetChannel;
class CNetContext;
class CSyncedFileSet;
class CSyncedFilePak;

// Description:
//     Entry point for all global operations in CryNetwork (operations not on a single channel.)
//     Implemented as a container of multiple CNetContextState's, and simply delegates most operations to that.
//     This allows us to have channels that are still referring to older NetContextState's (for instance... slower channels during a level transition.)
// See Also:
//     CNetContextState, INetContext
class CNetContext :
	public INetworkMember,
	public INetContext,
	public CDefaultStreamAllocator
{
public:
	using INetworkMember::Release;

	CNetContext(IGameContext* pGameContext, uint32 flags);
	~CNetContext();

	// INetContext
	virtual void            DeleteContext();

	virtual void            ActivateDemoRecorder(const char* filename);
	virtual INetChannel*    GetDemoRecorderChannel() const;
	virtual void            ActivateDemoPlayback(const char* filename, INetChannel* pClient, INetChannel* pServer);
	virtual bool            IsDemoPlayback() const  { return m_demoPlayback; }
	virtual bool            IsDemoRecording() const { return m_demoRecording; }
	virtual void            LogRMI(const char* function, ISerializable* pParams);
	virtual void            LogCppRMI(EntityId id, IRMICppLogger* pLogger);
	virtual void            DeclareAspect(const char* name, NetworkAspectType aspectBit, uint8 aspectFlags);
	virtual void            SetDelegatableMask(EntityId id, NetworkAspectType delegateMask);
	virtual void            SetAspectChannelMask(NetworkAspectType aspectBit, ChannelMaskType mask);
	virtual ChannelMaskType GetAspectChannelMask(NetworkAspectID aspectID) { return m_channelMasks[aspectID]; }
	virtual void            BindObject(EntityId id, EntityId parentId, NetworkAspectType aspectBits, bool bStatic);
	virtual bool            UnbindObject(EntityId id);
	virtual void            EnableAspects(EntityId id, NetworkAspectType aspectBits, bool enabled);
	virtual void            DelegateAuthority(EntityId id, INetChannel* pControlling);
	virtual bool            ChangeContext();
	virtual void            ChangedAspects(EntityId id, NetworkAspectType aspectBits);
#if FULL_ON_SCHEDULING
	virtual void            ChangedTransform(EntityId id, const Vec3& pos, const Quat& rot, float drawDist);
	virtual void            ChangedFov(EntityId id, float fov);
#endif
	virtual void            EstablishedContext(int establishToken);
	virtual void            SpawnedObject(EntityId userID);
	virtual bool            IsBound(EntityId userID);
#if RESERVE_UNBOUND_ENTITIES
	virtual EntityId        RemoveReservedUnboundEntityMapEntry(uint16 partialNetID);
#endif
	virtual void            RemoveRMIListener(IRMIListener* pListener);
	virtual bool            RemoteContextHasAuthority(INetChannel* pChannel, EntityId id);
	virtual void            SetAspectProfile(EntityId id, NetworkAspectType aspectBit, uint8 profile);
	virtual uint8           GetAspectProfile(EntityId id, NetworkAspectType aspectBit);
	virtual void            SetParentObject(EntityId objId, EntityId parentId);
	virtual void            LogBreak(const SNetBreakDescription& breakage);
	virtual bool            SetSchedulingParams(EntityId objId, uint32 normal, uint32 owned);
	virtual void            PulseObject(EntityId objId, uint32 pulseType);
	virtual void            EnableBackgroundPassthrough(bool enable);
	virtual int             RegisterPredictedSpawn(INetChannel* pChannel, EntityId id);
	virtual void            RegisterValidatedPredictedSpawn(INetChannel* pChannel, int predictionHandle, EntityId id);
	virtual void            SafelyUnbind(EntityId id);
	virtual void            RequestRemoteUpdate(EntityId id, NetworkAspectType aspects);
	virtual void            RegisterServerControlledFile(const char* filename);
	virtual ICryPak*        GetServerControlledICryPak();
	virtual XmlNodeRef      GetServerControlledXml(const char* filename);
	virtual SNetObjectID    GetNetID(EntityId userID, bool ensureNotUnbinding = true);
	virtual EntityId        GetEntityID(SNetObjectID netID);
	virtual void            Resaltify(SNetObjectID& id);
	// ~INetContext

	// pseudo INetContextListener
	void            Die();
	void            OnObjectEvent(SNetObjectEvent* pEvent);
	void            OnChannelEvent(INetChannel* pFrom, SNetChannelEvent* pEvent);
	string          GetName();
	SObjectMemUsage GetObjectMemUsage(SNetObjectID id) { return SObjectMemUsage(); }
	INetChannel*    GetAssociatedChannel()             { return 0; }
	// ~INetContextListener

	// INetworkMember
	//virtual void Update( CTimeValue blocking );
	virtual void GetMemoryStatistics(ICrySizer* pSizer);
	virtual bool IsDead();
	virtual bool IsSuicidal();
	virtual void NetDump(ENetDumpType type);
	virtual void SyncWithGame(ENetworkGameSync type);
	virtual void PerformRegularCleanup();
	virtual void GetProfilingStatistics(SNetworkProfilingStats* const pProfilingStats);
	// ~INetworkMember

	CNetContextState*    GetCurrentState()   { return m_pState; }
	INetContextListener* GetDemoMode() const { return &(*m_pDemo); }

	//uint16 GetSaltForId( uint16 id );
	virtual NetworkAspectType ServerControllerOnlyAspects() const { return m_serverControllerOnlyAspects; }
	virtual NetworkAspectType DelegatableAspects() const          { return m_delegatableAspects; }
	NetworkAspectType         ServerManagedProfileAspects() const { return m_serverManagedProfileAspects; }
	NetworkAspectType         DeclaredAspects() const             { return m_declaredAspects; }
	NetworkAspectType         TimestampedAspects() const          { return m_timestampedAspects; }
	NetworkAspectType         DisabledCompressionAspects() const  { return m_disabledCompressionAspects; }

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	CVoiceContext* GetVoiceContextImpl() { return m_pVoiceContext; }
#endif

#if ENABLE_SESSION_IDS
	void              SetSessionID(const CSessionID& id);
	const CSessionID& GetSessionID() const
	{
		return m_sessionID;
	}

	void EnableSessionDebugging();
#endif

	// register a context view
	void RegisterLocalIPs(const TNetAddressVec&, CContextView*);
	void DeregisterLocalIPs(const TNetAddressVec&);

	// returns NULL if no such local context associated with an IP address/port
	// in this NetContext
	CContextView* GetLocalContext(const TNetAddress& localAddr) const;

	const char*   GetAspectName(NetworkAspectID aspectIdx)
	{
		NET_ASSERT(aspectIdx < NUM_ASPECTS);
		return m_aspectNames[aspectIdx].c_str();
	}

	IGameContext* GetGameContext()
	{
		return m_pGameContext;
	}

	// reset our state
	void ClearAllState();

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	IVoiceContext* GetVoiceContext();
#endif

	bool IsMultiplayer() { return m_multiplayer; }

#if SERVER_FILE_SYNC_MODE
	CSyncedFileSet* GetSyncedFileSet(bool require);
#endif

private:
	bool m_demoPlayback;
	bool m_demoRecording;

	bool m_multiplayer;

	bool CanDestroyContext();

	string            m_aspectNames[NUM_ASPECTS];
	NetworkAspectType m_serverControllerOnlyAspects;
	NetworkAspectType m_delegatableAspects;
	NetworkAspectType m_regularlyUpdatedAspects;
	NetworkAspectType m_declaredAspects;
	NetworkAspectType m_serverManagedProfileAspects;
	NetworkAspectType m_timestampedAspects;
	NetworkAspectType m_disabledCompressionAspects;

	ChannelMaskType   m_channelMasks[NUM_ASPECTS];

	int               m_ctxSerial;

	// we have a game context associated with us
	IGameContext* m_pGameContext;

	// ip address to context view map; for determining when two views are on the
	// same machine
	std::map<TNetAddress, CContextView*> m_mIPToContext;

	bool                         m_bDead;
	// are we allowed to be dead?
	int                          m_nAllowDead;
	// are we in background passthrough mode
	NetTimerId                   m_backgroundPassthrough;
	static void                  BackgroundPassthrough(NetTimerId, void*, CTimeValue);

	_smart_ptr<CNetContextState> m_pState;

	INetContextListenerPtr       m_pDemo;
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	_smart_ptr<CVoiceContext>    m_pVoiceContext;
#endif
	INetContextListenerPtr       m_pAspectBandwidthDebugger;

#if SERVER_FILE_SYNC_MODE
	std::auto_ptr<CSyncedFileSet> m_pFileSet;
	std::auto_ptr<CSyncedFilePak> m_pFilePak;
#endif

	// this context's session id
#if ENABLE_SESSION_IDS
	CSessionID m_sessionID;
#endif
};

#endif

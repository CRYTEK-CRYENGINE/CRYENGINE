// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __BREAKREPLICATOR_SIMPLE_H__
#define __BREAKREPLICATOR_SIMPLE_H__

#pragma once

#include "BreakReplicator.h"
#include "SerializeBits.h"
#include "ActionGame.h"

#if NET_USE_SIMPLE_BREAKAGE

struct SBreakEvent;
struct IProtocolBuilder;
//class CBitArray;
//class BreakStream;

class CObjectIdentifier
{
public:
	enum ObjType { k_unknown = 0, k_entity, k_static_entity };

public:
	void             Reset();
	void             SerializeWith(CBitArray& array, bool includeObjCenter);
	void             CreateFromPhysicalEntity(const EventPhysMono* pMono);
	IPhysicalEntity* FindPhysicalEntity();
	IPhysicalEntity* GetPhysicsFromHoldingEntity();

public:
	ObjType m_objType;

	Vec3    m_objCenter;
	uint32  m_objHash;

	// When a static is first broken, the remaining pieces get placed into a temporary, holding
	// entity. This points to that entity. But we continue to identify the static object via
	// the pos+cent+vol+hash method, and then resolve the holding entity on the client through
	// the corresponding CObjectIdentifier. That way we dont have to have a net-bound entity, which
	// clogs the internal message queue in the low level network code.
	EntityId m_holdingEntity;
};

/*
   =======================================================================
   Plane BreakStream
   =======================================================================
 */

class BreakStream : public IBreakDescriptionInfo
{
public:
	enum EMode {k_invalid = 0, k_recording, k_playing, k_finished, k_numModes};
	enum EBreakType { k_unknown = 0, k_partBreak, k_planeBreak, k_deformBreak, k_numTypes };

public:
	BreakStream();
	virtual ~BreakStream();
	virtual void Init(EMode mode);

	/* IBreakDescriptionInfo */
	virtual void GetAffectedRegion(AABB& aabb);
	virtual void AddSendables(INetSendableSink* pSink, int32 brkId) {}
	/* IBreakDescriptionInfo */

	virtual void SerialiseSimpleBreakage(TSerialize ser);

	void         SerializeWith_Begin(CBitArray& array, bool includeObjCenter);
	virtual void SerializeWith(CBitArray& array)                                   {}

	virtual bool OnCreatePhysEntityPart(const EventPhysCreateEntityPart* pEvent)   { return pEvent->pEntity == m_pent; }
	virtual bool OnRemovePhysEntityParts(const EventPhysRemoveEntityParts* pEvent) { return pEvent->pEntity == m_pent; }
	virtual bool OnJointBroken(const EventPhysJointBroken* pEvent)                 { return pEvent->pEntity[0] == m_pent; }
	virtual void OnEndFrame();
	virtual void OnSpawnEntity(IEntity* pEntity)                                   {}
	virtual void OnSend(SNetBreakDescription& desc)                                {}
	virtual void Playback()                                                        {}
	virtual bool IsWaitinOnRenderMesh()                                            { return false; }
	virtual bool IsRenderMeshReady()                                               { return true; }

	void         LogHostMigrationInfo();

	bool         CanSendPayload();
	bool         IsIdentifierEqual(const CObjectIdentifier& identifier);

public:

	INetBreakageSimplePlaybackPtr m_collector;  // Low level class used to collect any spawned entities that need to be net bound

	CObjectIdentifier             m_identifier; // Shame to have a full copy here
	IPhysicalEntity*              m_pent;       // The active ent we are recording

	uint16                        m_breakIdx;    // Global break index
	uint16                        m_subBreakIdx; // Incrementing break idx for a particular entity (breaks for one entity should be done in this order)
	uint8                         m_mode;        /* EMode */
	uint8                         m_type;
	uint8                         m_numFramesLeft;        // Count down to being able to send, or finish playback
	uint8                         m_invalidFindCount;     // If the physics entity can't be found, increase this counter
	uint8                         m_waitForDependent;     // How many frames we waited for the dependency subIndex (on clients)
	uint8                         m_logHostMigrationInfo; // On playback, log host migration info for this stream;
};

class PlaneBreak : public BreakStream
{
public:
	PlaneBreak();
	virtual ~PlaneBreak();

	virtual void SerializeWith(CBitArray& array);

	virtual void OnSend(SNetBreakDescription& desc);

	virtual void Playback();

	virtual bool IsWaitinOnRenderMesh();

	virtual bool IsRenderMeshReady();

public:
	SBreakEvent m_be;
	IStatObj*   m_pStatObj;
};

/*
   =====================================================================
   SClientGlassBreak
   =====================================================================
   used to inform the server that the client
   broke the glass
 */

struct SClientGlassBreak
{
	void SerializeWith(TSerialize ser)
	{
		CBitArray array(&ser);
		m_planeBreak.SerializeWith(array);
		if (!array.IsReading())
			array.WriteToSerializer();
	}

	PlaneBreak m_planeBreak;
};

	#define NET_MAX_PENDING_BREAKAGE_STREAMS 1024

class CBreakReplicator : public CNetMessageSinkHelper<CBreakReplicator, INetMessageSink>, public IBreakReplicator, public ISystemEventListener
{
public:
	bool m_bDefineProtocolMode_server;
	void DefineProtocol(IProtocolBuilder* pBuilder);
public:
	CBreakReplicator(CGameContext* pGameCtx);
	virtual ~CBreakReplicator();
	void Reset();

	// ISystemEventListener
	virtual void                              OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);

	bool                                      OnCreatePhysEntityPart(const EventPhysCreateEntityPart* pEvent);
	bool                                      OnRemovePhysEntityParts(const EventPhysRemoveEntityParts* pEvent);
	bool                                      OnRevealEntityPart(const EventPhysRevealEntityPart* pEvent);
	bool                                      OnJointBroken(const EventPhysJointBroken* pEvent);
	void                                      BeginRecordingPlaneBreak(const SBreakEvent& be, EventPhysMono* pMono);
	void                                      BeginRecordingDeforminBreak(const SBreakEvent& be, EventPhysMono* pMono);
	void                                      OnBrokeSomething(const SBreakEvent& be, EventPhysMono* pMono, bool isPlane);

	void                                      OnEndFrame();
	void                                      OnSpawn(IEntity* pEntity, SEntitySpawnParams& params);
	void                                      OnSpawn(IEntity* pEntity, IPhysicalEntity* pPhysEntity, IPhysicalEntity* pSrcPhysEntity);
	void                                      OnRemove(IEntity* pEntity);
	void                                      RemoveEntity(IEntity* pEntity);
	void                                      SendPayloads();
	void                                      PushPlayback(BreakStream* pStream);
	int                                       PlaybackStream(BreakStream* pStream);
	void                                      PlaybackBreakage();
	void*                                     ReceiveSimpleBreakage(TSerialize ser);        // Serialise a received a stream
	void*                                     SerialiseBreakage(TSerialize ser, BreakStream* pStream);
	void                                      PlaybackSimpleBreakage(void* userData, INetBreakageSimplePlaybackPtr pBreakage); // Callback from low levek to push a stream for pending playback
	virtual const EventPhysRemoveEntityParts* GetRemovePartEvents(int& iNumEvents);

	static void                               SerialisePosition(CBitArray& array, Vec3& pos, float range, int numBits);

	int                                       PushBackNewStream(const EventPhysMono* pMono, BreakStream* pStream);
	int                                       GetBreakIndex(const EventPhysMono* pMono, bool add);
	int                                       GetIdentifier(CObjectIdentifier* identifier, const IPhysicalEntity* pent, bool add);

	void                                      OnReuse(IEntity* pEntity, SEntitySpawnParams& params)            {}
	void                                      OnStartFrame()                                                   {}
	void                                      PlaybackBreakage(int breakId, INetBreakagePlaybackPtr pBreakage) { PlaybackBreakage(); }
	void                                      GetMemoryStatistics(ICrySizer* s)                                {}

	static CBreakReplicator*                  Get()                                                            { return m_pThis; }
	static IBreakReplicator*                  GetIBreakReplicator()                                            { return static_cast<IBreakReplicator*>(m_pThis); }
	static void                               RegisterClasses();
	static int                                OnRevealEntityPart(const EventPhys* pEvent);
	static int                                OnJointBroken(const EventPhys* pEvent);
	static int                                OnCreatePhysEntityPart(const EventPhys* pEvent);
	static int                                OnRemovePhysEntityParts(const EventPhys* pEvent);

	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE_WITHOUT_SEND(SvRecvGlassBreak, SClientGlassBreak);

public:

	// A list of streams, indexed by the breakIdx
	DynArray<BreakStream*>                  m_streams;

	DynArray<CObjectIdentifier>             m_identifiers; // We need to keep a list of identifiers on all machines for entity syncing purposes (instead of using CryNetwork)

	int                                     m_activeStartIdx; // The index of the first active stream that is recording/playing

	int                                     m_numPendingStreams;
	BreakStream*                            m_pendingStreams[NET_MAX_PENDING_BREAKAGE_STREAMS]; // For playback, only pop one stream at a time for a physics entity

	std::vector<EventPhysRemoveEntityParts> m_removePartEvents;
	std::vector<EntityId>                   m_entitiesToRemove;

	float                                   m_timeSinceLevelLoaded;

	static CBreakReplicator*                m_pThis;
	static int                              m_accurateWorldPosNumBits;
	static int                              m_inaccurateWorldPosNumBits;
};

#endif // NET_USE_SIMPLE_BREAKAGE

#endif

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __BREAKREPLICATOR_H__
#define __BREAKREPLICATOR_H__

#pragma once

#if NET_USE_SIMPLE_BREAKAGE

	#include "BreakReplicator_Simple.h"

#else // NET_USE_SIMPLE_BREAKAGE

//**=======================================================**
//**                                                       **
//**     OLD BREAKREPLICATOR                               **
//**                                                       **
//**     Deprecated !                                      **
//**                                                       **
//**                                                       **
//**=======================================================**

	#include "ActionGame.h"
	#include <CryNetwork/NetHelpers.h>
	#include "ObjectSelector.h"

	#include "ProceduralBreak.h"
	#include "DeformingBreak.h"
	#include "PlaneBreak.h"
	#include "IBreakReplicatorListener.h"
	#include "IBreakPlaybackStream.h"
	#include "ExplosiveObjectState.h"
	#include "JointBreak.h"
	#include "SimulateRemoveEntityParts.h"
	#include "SimulateCreateEntityPart.h"
	#include "DebugBreakage.h"

class CGameContext;

struct SProceduralBreak;
struct SDeformingBreak;
struct SExplosiveBreak;
struct SExplosiveObjectState;
class CGenericRecordingListener;
class CGenericPlaybackListener;
class CNullListener;
class CProceduralBreakingBaseListener;
class CProceduralBreakingRecordingListener;
class CProceduralBreakingPlaybackListener;
class CDeformingBreak;
class IBreakReplicator_Simple;

struct SSetMagicId
{
	SSetMagicId() : breakId(-1), magicId(-1) {}
	SSetMagicId(int bid, int mid) : breakId(bid), magicId(mid) {}

	int  breakId;
	int  magicId;

	void SerializeWith(TSerialize ser)
	{
		LOGBREAK("SSetMagicId, %s", ser.IsReading() ? "Reading:" : "Writing");
		ser.Value("breakId", breakId, 'brId');
		ser.Value("magicId", magicId);
	}
};

class CBreakReplicator : public CNetMessageSinkHelper<CBreakReplicator, INetMessageSink>, IBreakReplicator
{
public:
	CBreakReplicator(CGameContext* pCtx);
	~CBreakReplicator();
	void Reset();

	bool m_bDefineProtocolMode_server;
	void                                      DefineProtocol(IProtocolBuilder* pBuilder);

	void                                      PlaybackBreakage(int breakId, INetBreakagePlaybackPtr pBreakage);
	void*                                     ReceiveSimpleBreakage(TSerialize ser);
	void                                      PlaybackSimpleBreakage(void* userData, INetBreakageSimplePlaybackPtr pBreakage);

	void                                      OnSpawn(IEntity* pEntity, SEntitySpawnParams& params);
	void                                      OnSpawn(IEntity* pEntity, IPhysicalEntity* pPhysEntity, IPhysicalEntity* pSrcPhysEntity);
	void                                      OnRemove(IEntity* pEntity);
	void                                      OnReuse(IEntity* pEntity, SEntitySpawnParams& params);
	void                                      OnStartFrame();
	void                                      OnEndFrame();
	void                                      OnBrokeSomething(const SBreakEvent& be, EventPhysMono* pMono, bool isPlane);
	void                                      GetMemoryStatistics(ICrySizer* s);

	void                                      BeginEvent(IBreakReplicatorListenerPtr pListener);
	void                                      EndEvent();

	void                                      RemoveEntity(IEntity* pEntity);

	virtual const EventPhysRemoveEntityParts* GetRemovePartEvents(int& iNumEvents);

	void                                      AddListener(IBreakReplicatorListenerPtr pListener, int nFrames);

	int                                       PullOrderId();
	void                                      PushBreak(int orderId, const SNetBreakDescription& desc);
	void                                      PushAbandonment(int orderId);

	static CBreakReplicator*                  Get()                 { return m_pThis; }
	static IBreakReplicator*                  GetIBreakReplicator() { return static_cast<IBreakReplicator*>(m_pThis); }
	static void                               RegisterClasses();

	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(DeformingBreak, SDeformingBreakParams);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(PlaneBreak, SPlaneBreakParams);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(JointBreak, SJointBreakParams);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(SimulateCreateEntityPart, SSimulateCreateEntityPartMessage);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(SimulateRemoveEntityParts, SSimulateRemoveEntityPartsMessage);

	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(DeclareProceduralSpawnRec, SDeclareProceduralSpawnRec);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(DeclareJointBreakRec, SDeclareJointBreakRec);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(DeclareJointBreakParticleRec, SDeclareJointBreakParticleRec);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(DeclareExplosiveObjectState, SDeclareExplosiveObjectState);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(SetMagicId, SSetMagicId);

private:

	struct SListenerInfo
	{
		SListenerInfo() : timeout(-10) {}
		SListenerInfo(IBreakReplicatorListenerPtr p, int t) : timeout(t), pListener(p) {}
		int                         timeout;
		IBreakReplicatorListenerPtr pListener;
	};

	void                        EnterEvent();

	void                        OnUpdateMesh(const EventPhysUpdateMesh* pEvent);
	void                        OnCreatePhysEntityPart(const EventPhysCreateEntityPart* pEvent);
	void                        OnRemovePhysEntityParts(const EventPhysRemoveEntityParts* pEvent);
	void                        OnJointBroken(const EventPhysJointBroken* pEvent);

	static int                  OnUpdateMesh_Begin(const EventPhys* pEvent);
	static int                  OnCreatePhysEntityPart_Begin(const EventPhys* pEvent);
	static int                  OnRemovePhysEntityParts_Begin(const EventPhys* pEvent);
	static int                  OnJointBroken_Begin(const EventPhys* pEvent);

	static int                  OnUpdateMesh_End(const EventPhys* pEvent);
	static int                  OnCreatePhysEntityPart_End(const EventPhys* pEvent);
	static int                  OnRemovePhysEntityParts_End(const EventPhys* pEvent);
	static int                  OnJointBroken_End(const EventPhys* pEvent);

	static int                  OnPostStepEvent(const EventPhys* pEvent);
	void                        OnPostStep(const EventPhysPostStep* pEvent);

	IBreakReplicatorListenerPtr AddProceduralBreakTypeListener(const IProceduralBreakTypePtr& pBT);

	void                        SpinPendingStreams();

	static CBreakReplicator*             m_pThis;
	typedef std::vector<SListenerInfo> ListenerInfos;
	ListenerInfos                        m_listenerInfos;
	ListenerInfos                        m_listenerInfos_temp;
	IBreakReplicatorListenerPtr          m_pActiveListener;
	IBreakReplicatorListenerPtr          m_pGenericUpdateMesh;
	IBreakReplicatorListenerPtr          m_pGenericJointBroken;
	IBreakReplicatorListenerPtr          m_pGenericCreateEntityPart;
	IBreakReplicatorListenerPtr          m_pGenericRemoveEntityParts;
	IBreakReplicatorListenerPtr          m_pNullListener;
	_smart_ptr<CGenericPlaybackListener> m_pGenericPlaybackListener;

	std::vector<IBreakPlaybackStreamPtr> m_playbackMessageHandlers;

	bool                    BeginStream(int idx, const IProceduralBreakTypePtr& p);
	IBreakPlaybackStreamPtr GetStream(int idx);
	IBreakPlaybackStreamPtr PullStream(int idx);
	IBreakPlaybackStreamPtr m_pNullPlayback;

	struct SPendingPlayback
	{
		IBreakPlaybackStreamPtr pStream;
		INetBreakagePlaybackPtr pNetBreakage;
	};
	std::vector<SPendingPlayback>        m_pendingPlayback;
	IBreakReplicatorListenerPtr          m_pPlaybackListener;
	CGameContext*                        m_pContext;

	int                                  m_nextOrderId;
	int                                  m_loggedOrderId;
	std::map<int, SNetBreakDescription*> m_pendingLogs;
	void FlushPendingLogs();

	std::vector<EntityId>                   m_entitiesToRemove;

	std::vector<EventPhysRemoveEntityParts> m_removePartEvents;
};

#endif // NET_USE_SIMPLE_BREAKAGE

#endif

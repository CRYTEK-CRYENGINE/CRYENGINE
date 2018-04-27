// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BreakReplicator.h"
#include "GameContext.h"

#include "NullListener.h"
#include "NullStream.h"
#include "GenericRecordingListener.h"
#include "GenericPlaybackListener.h"
#include "ProceduralBreakingPlaybackListener.h"
#include "ProceduralBreakingRecordingListener.h"
#include "NetworkCVars.h"
#include "ObjectSelector.h"
#include "DebugBreakage.h"

#include "JointBreak.h"
#include "BreakReplicatorGameObject.h"

void CBreakReplicator::RegisterClasses()
{
	// Register the GameObjectExtension
	static bool once = true;
	if (once)
	{
		once = false;
		IEntityClassRegistry::SEntityClassDesc serializerClass;
		serializerClass.sName = "BreakRepGameObject";
		serializerClass.sScriptFile = "";
		serializerClass.flags = ECLF_INVISIBLE;
		static IGameFramework::CGameObjectExtensionCreator<CBreakRepGameObject> creator;
		CCryAction::GetCryAction()->GetIGameObjectSystem()->RegisterExtension(serializerClass.sName, &creator, &serializerClass);
	}
}

#if !NET_USE_SIMPLE_BREAKAGE

//**=======================================================**
//**                                                       **
//**     OLD BREAKREPLICATOR                               **
//**                                                       **
//**     Deprecated !                                      **
//**                                                       **
//**                                                       **
//**=======================================================**

CBreakReplicator* CBreakReplicator::m_pThis = 0;

static const int RECORDING_TIMEOUT = 120;

CBreakReplicator::CBreakReplicator(CGameContext* pCtx)
	: m_bDefineProtocolMode_server(false)
{
	CRY_ASSERT(!m_pThis);
	m_pThis = this;
	m_pContext = pCtx;

	m_nextOrderId = 0;
	m_loggedOrderId = -1;

	m_pActiveListener = 0;
	m_pNullListener = new CNullListener();
	m_pNullPlayback = new CNullStream();
	if (pCtx->HasContextFlag(eGSF_Server))
		m_pGenericJointBroken = m_pGenericCreateEntityPart = m_pGenericUpdateMesh = m_pGenericRemoveEntityParts = new CGenericRecordingListener();
	else
		m_pGenericJointBroken = m_pGenericCreateEntityPart = m_pGenericUpdateMesh = m_pGenericRemoveEntityParts = m_pNullListener;
	if (pCtx->HasContextFlag(eGSF_Client) && !pCtx->HasContextFlag(eGSF_Server))
		m_pGenericPlaybackListener = new CGenericPlaybackListener();

	if (gEnv->pPhysicalWorld)
	{
		gEnv->pPhysicalWorld->AddEventClient(EventPhysCreateEntityPart::id, OnCreatePhysEntityPart_Begin, true, 100000.f);
		gEnv->pPhysicalWorld->AddEventClient(EventPhysRemoveEntityParts::id, OnRemovePhysEntityParts_Begin, true, 100000.f);
		gEnv->pPhysicalWorld->AddEventClient(EventPhysUpdateMesh::id, OnUpdateMesh_Begin, true, 100000.f);
		gEnv->pPhysicalWorld->AddEventClient(EventPhysUpdateMesh::id, OnUpdateMesh_End, true, -100000.f);
		gEnv->pPhysicalWorld->AddEventClient(EventPhysJointBroken::id, OnJointBroken_Begin, true, 100000.f);
		gEnv->pPhysicalWorld->AddEventClient(EventPhysJointBroken::id, OnJointBroken_End, true, -100000.f);
		gEnv->pPhysicalWorld->AddEventClient(EventPhysRemoveEntityParts::id, OnRemovePhysEntityParts_End, true, -100000.f);
		gEnv->pPhysicalWorld->AddEventClient(EventPhysCreateEntityPart::id, OnCreatePhysEntityPart_End, true, -100000.f);

		gEnv->pPhysicalWorld->AddEventClient(EventPhysPostStep::id, OnPostStepEvent, true);
	}

	if (DEBUG_NET_BREAKAGE)
	{
		CNetworkCVars::Get().BreakageLog = 1;
	}

	m_entitiesToRemove.reserve(16);

	m_removePartEvents.reserve(100);
}

CBreakReplicator::~CBreakReplicator()
{
	m_pThis = 0;

	if (gEnv->pPhysicalWorld)
	{
		gEnv->pPhysicalWorld->RemoveEventClient(EventPhysCreateEntityPart::id, OnCreatePhysEntityPart_Begin, true);
		gEnv->pPhysicalWorld->RemoveEventClient(EventPhysCreateEntityPart::id, OnCreatePhysEntityPart_End, true);
		gEnv->pPhysicalWorld->RemoveEventClient(EventPhysRemoveEntityParts::id, OnRemovePhysEntityParts_Begin, true);
		gEnv->pPhysicalWorld->RemoveEventClient(EventPhysRemoveEntityParts::id, OnRemovePhysEntityParts_End, true);
		gEnv->pPhysicalWorld->RemoveEventClient(EventPhysUpdateMesh::id, OnUpdateMesh_Begin, true);
		gEnv->pPhysicalWorld->RemoveEventClient(EventPhysUpdateMesh::id, OnUpdateMesh_End, true);
		gEnv->pPhysicalWorld->RemoveEventClient(EventPhysJointBroken::id, OnJointBroken_Begin, true);
		gEnv->pPhysicalWorld->RemoveEventClient(EventPhysJointBroken::id, OnJointBroken_End, true);

		gEnv->pPhysicalWorld->RemoveEventClient(EventPhysPostStep::id, OnPostStepEvent, true);
	}

	while (!m_pendingLogs.empty())
	{
		delete m_pendingLogs.begin()->second;
		m_pendingLogs.erase(m_pendingLogs.begin());
	}
}

const EventPhysRemoveEntityParts* CBreakReplicator::GetRemovePartEvents(int& iNumEvents)
{
	iNumEvents = m_removePartEvents.size();
	if (iNumEvents)
		return &(m_removePartEvents[0]);
	else
		return NULL;
}

void CBreakReplicator::Reset()
{
}

int CBreakReplicator::PullOrderId()
{
	return m_nextOrderId++;
}

void CBreakReplicator::PushBreak(int orderId, const SNetBreakDescription& desc)
{
	if (orderId == m_loggedOrderId + 1)
	{
		CCryAction::GetCryAction()->GetNetContext()->LogBreak(desc);
		m_loggedOrderId++;
		FlushPendingLogs();
	}
	else
	{
		m_pendingLogs[orderId] = new SNetBreakDescription(desc);
	}
}

void CBreakReplicator::PushAbandonment(int orderId)
{
	if (orderId == m_loggedOrderId + 1)
	{
		m_loggedOrderId++;
		FlushPendingLogs();
	}
	else
	{
		m_pendingLogs[orderId] = 0;
	}
}

void CBreakReplicator::FlushPendingLogs()
{
	while (!m_pendingLogs.empty() && m_pendingLogs.begin()->first == m_loggedOrderId + 1)
	{
		if (m_pendingLogs.begin()->second)
			CCryAction::GetCryAction()->GetNetContext()->LogBreak(*m_pendingLogs.begin()->second);
		m_loggedOrderId++;
		m_pendingLogs.erase(m_pendingLogs.begin());
	}
}

void CBreakReplicator::OnBrokeSomething(const SBreakEvent& be, EventPhysMono* pMono, bool isPlane)
{
	LOGBREAK("CBreakReplicator::OnBrokeSomething(isPlane: %d)", isPlane);

	if (!gEnv->bServer)
		return;

	if (isPlane)
		AddProceduralBreakTypeListener(new CPlaneBreak(be));
	else
		AddProceduralBreakTypeListener(new CDeformingBreak(be));
}

IBreakReplicatorListenerPtr CBreakReplicator::AddProceduralBreakTypeListener(const IProceduralBreakTypePtr& pBT)
{
	for (ListenerInfos::iterator it = m_listenerInfos.begin(); it != m_listenerInfos.end(); ++it)
	{
		if (it->pListener->AttemptAbsorb(pBT))
		{
			if (CNetworkCVars::Get().BreakageLog)
				CryLogAlways("[brk] absorbed into listener %p", (void*)it->pListener);
			return it->pListener;
		}
	}

	CProceduralBreakingBaseListener* pListener = new CProceduralBreakingRecordingListener(pBT);
	if (CNetworkCVars::Get().BreakageLog)
		CryLogAlways("[brk] add listener %p", pListener);
	m_listenerInfos.push_back(SListenerInfo(pListener, RECORDING_TIMEOUT));
	return pListener;
}

void CBreakReplicator::OnSpawn(IEntity* pEntity, SEntitySpawnParams& params)
{
	if (m_pActiveListener)
	{
		if (CNetworkCVars::Get().BreakageLog)
			CryLogAlways("[brk] spawn %s and pass to %s @ %p", pEntity->GetName(), m_pActiveListener->GetName(), &*m_pActiveListener);
		m_pActiveListener->OnSpawn(pEntity, params);
	}
}

void CBreakReplicator::OnSpawn(IEntity* pEntity, IPhysicalEntity* pPhysEntity, IPhysicalEntity* pSrcPhysEntity)
{
}

void CBreakReplicator::OnRemove(IEntity* pEntity)
{
	if (m_pActiveListener)
	{
		if (CNetworkCVars::Get().BreakageLog)
			CryLogAlways("[brk] remove %s and pass to %s @ %p", pEntity->GetName(), m_pActiveListener->GetName(), &*m_pActiveListener);
		m_pActiveListener->OnRemove(pEntity);
	}
}

void CBreakReplicator::RemoveEntity(IEntity* pEntity)
{
	m_entitiesToRemove.push_back(pEntity->GetId());

}

void CBreakReplicator::OnReuse(IEntity* pEntity, SEntitySpawnParams& params)
{
	OnSpawn(pEntity, params);
}

void CBreakReplicator::OnStartFrame()
{
	CRY_ASSERT(!m_pActiveListener);
	for (ListenerInfos::iterator it = m_listenerInfos.begin(); it != m_listenerInfos.end(); ++it)
		it->pListener->OnStartFrame();
}

void CBreakReplicator::OnEndFrame()
{
	CRY_ASSERT(!m_pActiveListener);
	bool hdr = false;
	for (ListenerInfos::iterator it = m_listenerInfos.begin(); it != m_listenerInfos.end(); ++it)
	{
		if (--it->timeout)
		{
			if (it->pListener->OnEndFrame())
			{
				m_listenerInfos_temp.push_back(*it);

				if (CNetworkCVars::Get().BreakageLog)
				{
					if (!hdr)
					{
						CryLogAlways("[brk] end frame");
						hdr = true;
					}
					CryLogAlways("[brk] still active for %d is %s @ %p", it->timeout, it->pListener->GetName(), &*it->pListener);
				}
			}
			else if (CNetworkCVars::Get().BreakageLog)
			{
				CryLogAlways("[brk] %p claims completion", it->pListener.get());
			}
		}
		else
		{
			CryLogAlways("[brk] %p times out", it->pListener.get());
			it->pListener->OnTimeout();
		}
	}
	m_listenerInfos_temp.swap(m_listenerInfos);
	m_listenerInfos_temp.resize(0);

	SpinPendingStreams();

	//================================
	// Process Entities To be Removed
	//================================
	// If the object is bound to the network then it must not be deleted. Instead the game object
	// serialises the state and frees the statobj slot on all machines. This is used when the
	// breakage system is consuming too much memory.
	for (int i = 0; i < (const int)m_entitiesToRemove.size(); i++)
	{
		EntityId entId = m_entitiesToRemove[i];
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entId);
		if (pEntity)
		{
			if ((pEntity->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY)) == 0)
			{
				if (CGameObject* pGameObject = (CGameObject*) pEntity->GetProxy(ENTITY_PROXY_USER))
				{
					if (IGameObjectExtension* pExtension = pGameObject->QueryExtension("BreakRepGameObject"))
					{
						// Net serialise that this entity should free its breakage statobj slot
						static_cast<CBreakRepGameObject*>(pExtension)->m_removed = true;
					}
				}
			}
			else
			{
				// Entities that are not bound to the network can be deleted
				gEnv->pEntitySystem->RemoveEntity(entId);
			}
		}
	}
	m_entitiesToRemove.clear();
}

void CBreakReplicator::BeginEvent(IBreakReplicatorListenerPtr pListener)
{
	CRY_ASSERT(!m_pActiveListener);
	CRY_ASSERT(pListener != NULL);
	m_pActiveListener = pListener;
	if (CNetworkCVars::Get().BreakageLog)
		CryLogAlways("[brk] BeginEvent with listener %s @ %p", pListener->GetName(), &*pListener);
}

void CBreakReplicator::EnterEvent()
{
}

void CBreakReplicator::EndEvent()
{
	if (!m_pActiveListener)
	{
		GameWarning("CBreakReplicator::EndEvent: No active listener during end event, this is wrong (ignored)");
		return;
	}

	CRY_ASSERT(m_pActiveListener != NULL);
	if (CNetworkCVars::Get().BreakageLog)
		CryLogAlways("[brk] EndEvent");
	m_pActiveListener->EndEvent(CCryAction::GetCryAction()->GetGameContext()->GetNetContext());
	m_pActiveListener = 0;
}

ILINE void CBreakReplicator::OnUpdateMesh(const EventPhysUpdateMesh* pEvent)
{
	EnterEvent();
	if (CNetworkCVars::Get().BreakageLog)
		CryLogAlways("[brk] OnUpdateMesh %s", CObjectSelector::GetDescription(pEvent->pEntity).c_str());
	for (ListenerInfos::reverse_iterator iter = m_listenerInfos.rbegin(); iter != m_listenerInfos.rend(); ++iter)
	{
		if (iter->pListener->AcceptUpdateMesh(pEvent))
		{
			BeginEvent(iter->pListener);
			return;
		}
	}
	if (m_pGenericUpdateMesh->AcceptUpdateMesh(pEvent))
		BeginEvent(m_pGenericUpdateMesh);
	else
		BeginEvent(m_pNullListener);
}

ILINE void CBreakReplicator::OnJointBroken(const EventPhysJointBroken* pEvent)
{
	EnterEvent();
	if (CNetworkCVars::Get().BreakageLog)
		CryLogAlways("[brk] OnJointBroken %s", CObjectSelector::GetDescription(pEvent->pEntity[0]).c_str());
	LOGBREAK("OnJointBroken, pent0: %p, pent1: %p, newpent0: %p, newpent1: %p", pEvent->pEntity[0], pEvent->pEntity[1], pEvent->pNewEntity[0], pEvent->pNewEntity[1]);

	for (ListenerInfos::reverse_iterator iter = m_listenerInfos.rbegin(); iter != m_listenerInfos.rend(); ++iter)
	{
		if (iter->pListener->AcceptJointBroken(pEvent))
		{
			BeginEvent(iter->pListener);
			return;
		}
	}

	if (m_pContext->HasContextFlag(eGSF_Server))
	{
		CJointBreak* pJointBreak = new CJointBreak(pEvent->pEntity[0]);
		IBreakReplicatorListenerPtr pListener = AddProceduralBreakTypeListener(pJointBreak);
		if (pListener->AcceptJointBroken(pEvent))
			BeginEvent(m_listenerInfos.back().pListener);
		else
			GameWarning("CBreakReplicator::OnJointBroken: created listener is non-accepting");
	}
	else if (m_pGenericJointBroken->AcceptJointBroken(pEvent))
		BeginEvent(m_pGenericJointBroken);
	else
		BeginEvent(m_pNullListener);
}

ILINE void CBreakReplicator::OnCreatePhysEntityPart(const EventPhysCreateEntityPart* pEvent)
{
	EnterEvent();
	if (CNetworkCVars::Get().BreakageLog)
		CryLogAlways("[brk] OnCreatePhysEntityPart %s%s part[src=%d,new=%d]", CObjectSelector::GetDescription(pEvent->pEntity).c_str(), pEvent->iReason == EventPhysCreateEntityPart::ReasonJointsBroken ? " due to joint break" : "", pEvent->partidSrc, pEvent->partidNew);

	for (ListenerInfos::reverse_iterator iter = m_listenerInfos.rbegin(); iter != m_listenerInfos.rend(); ++iter)
	{
		if (iter->pListener->AcceptCreateEntityPart(pEvent))
		{
			BeginEvent(iter->pListener);
			return;
		}
	}

	if (m_pContext->HasContextFlag(eGSF_Server) && pEvent->iReason == EventPhysCreateEntityPart::ReasonJointsBroken)
	{
		CJointBreak* pJointBreak = new CJointBreak(pEvent->pEntity);
		IBreakReplicatorListenerPtr pListener = AddProceduralBreakTypeListener(pJointBreak);
		if (pListener->AcceptCreateEntityPart(pEvent))
			BeginEvent(m_listenerInfos.back().pListener);
		else
			GameWarning("CBreakReplicator::OnCreatePhysEntityPart: created listener is non-accepting");
	}
	else if (m_pGenericCreateEntityPart->AcceptCreateEntityPart(pEvent))
		BeginEvent(m_pGenericCreateEntityPart);
	else
		BeginEvent(m_pNullListener);
}

static string RemovePartsBitString(const uint32* p, int n)
{
	string x;
	for (int i = 0; i < n; i++)
	{
		for (int j = 0; j < 32; j++)
		{
			if (p[i] & (1 << j))
			{
				int idx = i * 32 + j;
				if (x.empty())
					x += string().Format("%d", idx);
				else
					x += string().Format(", %d", idx);
			}
		}
	}
	return x;
}

ILINE void CBreakReplicator::OnRemovePhysEntityParts(const EventPhysRemoveEntityParts* pEvent)
{
	const int oldSize = m_removePartEvents.size();
	BreakLogAlways("PR: Storing RemovePart event index %d,  'foreign data': 0x%p   iForeignType: %d", oldSize, pEvent->pForeignData, pEvent->iForeignData);
	m_removePartEvents.push_back(*pEvent);

	EnterEvent();
	if (CNetworkCVars::Get().BreakageLog)
		CryLogAlways("[brk] OnRemovePhysEntityParts %s (%s)", CObjectSelector::GetDescription(pEvent->pEntity).c_str(), RemovePartsBitString(pEvent->partIds, sizeof(pEvent->partIds) / sizeof(*pEvent->partIds)).c_str());
	LOGBREAK("OnRemovePhysEntityParts: physEnt=%p", pEvent->pEntity);
	for (ListenerInfos::reverse_iterator iter = m_listenerInfos.rbegin(); iter != m_listenerInfos.rend(); ++iter)
	{
		if (iter->pListener->AcceptRemoveEntityParts(pEvent))
		{
			BeginEvent(iter->pListener);
			return;
		}
	}
	if (m_pGenericRemoveEntityParts->AcceptRemoveEntityParts(pEvent))
		BeginEvent(m_pGenericRemoveEntityParts);
	else
		BeginEvent(m_pNullListener);
}

ILINE void CBreakReplicator::OnPostStep(const EventPhysPostStep* pEvent)
{
	#if BREAK_HIERARCHICAL_TRACKING
	for (ListenerInfos::reverse_iterator iter = m_listenerInfos.rbegin(); iter != m_listenerInfos.rend(); ++iter)
	{
		iter->pListener->OnPostStep();
	}
	#endif
}

int CBreakReplicator::OnPostStepEvent(const EventPhys* pEvent)
{
	m_pThis->OnPostStep(static_cast<const EventPhysPostStep*>(pEvent));
	return 1;
}

int CBreakReplicator::OnJointBroken_Begin(const EventPhys* pEvent)
{
	m_pThis->OnJointBroken(static_cast<const EventPhysJointBroken*>(pEvent));
	return 1;
}

int CBreakReplicator::OnJointBroken_End(const EventPhys* pEvent)
{
	m_pThis->EndEvent();
	return 1;
}

int CBreakReplicator::OnUpdateMesh_Begin(const EventPhys* pEvent)
{
	m_pThis->OnUpdateMesh(static_cast<const EventPhysUpdateMesh*>(pEvent));
	return 1;
}

int CBreakReplicator::OnUpdateMesh_End(const EventPhys* pEvent)
{
	m_pThis->EndEvent();
	return 1;
}

int CBreakReplicator::OnCreatePhysEntityPart_Begin(const EventPhys* pEvent)
{
	m_pThis->OnCreatePhysEntityPart(static_cast<const EventPhysCreateEntityPart*>(pEvent));
	return 1;
}

int CBreakReplicator::OnCreatePhysEntityPart_End(const EventPhys* pEvent)
{
	m_pThis->EndEvent();
	return 1;
}

int CBreakReplicator::OnRemovePhysEntityParts_Begin(const EventPhys* pEvent)
{
	m_pThis->OnRemovePhysEntityParts(static_cast<const EventPhysRemoveEntityParts*>(pEvent));
	return 1;
}

int CBreakReplicator::OnRemovePhysEntityParts_End(const EventPhys* pEvent)
{
	m_pThis->EndEvent();
	return 1;
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CBreakReplicator, DeformingBreak, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	CRY_ASSERT(!CCryAction::GetCryAction()->IsGamePaused());
	return BeginStream(param.breakId, new CDeformingBreak(param.breakEvent));
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CBreakReplicator, PlaneBreak, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	CRY_ASSERT(!CCryAction::GetCryAction()->IsGamePaused());
	return BeginStream(param.breakId, new CPlaneBreak(param.breakEvent));
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CBreakReplicator, JointBreak, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	CRY_ASSERT(!CCryAction::GetCryAction()->IsGamePaused());
	return BeginStream(param.breakId, new CJointBreak());
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CBreakReplicator, DeclareProceduralSpawnRec, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	return GetStream(param.breakId)->GotProceduralSpawnRec(&param);
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CBreakReplicator, DeclareJointBreakRec, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	return GetStream(param.breakId)->GotJointBreakRec(&param);
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CBreakReplicator, DeclareJointBreakParticleRec, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	return GetStream(param.breakId)->GotJointBreakParticleRec(&param);
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CBreakReplicator, DeclareExplosiveObjectState, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	return GetStream(param.breakId)->GotExplosiveObjectState(&param);
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CBreakReplicator, SimulateRemoveEntityParts, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	return false;
	/*
	   CRY_ASSERT(false);
	   IBreakPlaybackStreamPtr pStream = new CGenericPlaybackStream<SSimulateRemoveEntityParts>(param);
	   return BeginStream( param.breakId, pStream );
	 */
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CBreakReplicator, SetMagicId, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	return GetStream(param.breakId)->SetMagicId(param.magicId);
}

void CBreakReplicator::GetMemoryStatistics(ICrySizer* s)
{
}

void CBreakReplicator::DefineProtocol(IProtocolBuilder* pBuilder)
{
	static SNetProtocolDef nullDef = { 0, 0 };

	if (m_bDefineProtocolMode_server)
		pBuilder->AddMessageSink(this, GetProtocolDef(), nullDef);
	else
		pBuilder->AddMessageSink(this, nullDef, GetProtocolDef());
}

bool CBreakReplicator::BeginStream(int idx, const IProceduralBreakTypePtr& pBT)
{
	if (idx < 0)
		return false;
	if (m_playbackMessageHandlers.size() <= idx)
		m_playbackMessageHandlers.resize(idx + 1);
	if (m_playbackMessageHandlers[idx])
		return m_playbackMessageHandlers[idx]->AttemptAbsorb(pBT);
	m_playbackMessageHandlers[idx] = new CProceduralBreakingPlaybackStream(pBT);
	return true;
}

IBreakPlaybackStreamPtr CBreakReplicator::GetStream(int idx)
{
	if (idx < 0 || idx >= m_playbackMessageHandlers.size())
		return m_pNullPlayback;
	if (!m_playbackMessageHandlers[idx])
		return m_pNullPlayback;
	return m_playbackMessageHandlers[idx];
}

IBreakPlaybackStreamPtr CBreakReplicator::PullStream(int idx)
{
	if (idx < 0 || idx >= m_playbackMessageHandlers.size())
		return m_pNullPlayback;
	if (!m_playbackMessageHandlers[idx])
		return m_pNullPlayback;
	IBreakPlaybackStreamPtr pBrk = NULL;
	std::swap(pBrk, m_playbackMessageHandlers[idx]);
	return pBrk;
}

void CBreakReplicator::PlaybackBreakage(int breakId, INetBreakagePlaybackPtr pBreakage)
{
	SPendingPlayback pp;
	pp.pNetBreakage = pBreakage;
	pp.pStream = PullStream(breakId);
	m_pendingPlayback.push_back(pp);
	SpinPendingStreams();
}

void* CBreakReplicator::ReceiveSimpleBreakage(TSerialize ser)
{
	CryFatalError("This code shouldn't be called unless NET_USE_SIMPLE_BREAKAGE is defined");
	return NULL;
}

void CBreakReplicator::PlaybackSimpleBreakage(void* userData, INetBreakageSimplePlaybackPtr pBreakage)
{
	CryFatalError("This code shouldn't be called unless NET_USE_SIMPLE_BREAKAGE is defined");
}

void CBreakReplicator::SpinPendingStreams()
{
	while (true)
	{
		bool stillRunning = false;
		if (m_pPlaybackListener)
		{
			for (ListenerInfos::iterator it = m_listenerInfos.begin(); !stillRunning && it != m_listenerInfos.end(); ++it)
				if (it->pListener == m_pPlaybackListener)
					stillRunning = true;
			if (!stillRunning)
				m_pPlaybackListener = 0;
		}

		if (stillRunning)
			break;
		if (m_pendingPlayback.empty())
			break;

		SPendingPlayback pp = m_pendingPlayback.front();
		m_pendingPlayback.erase(m_pendingPlayback.begin());

		m_pPlaybackListener = pp.pStream->Playback(this, pp.pNetBreakage);
	}
}

void CBreakReplicator::AddListener(IBreakReplicatorListenerPtr pListener, int nFrames)
{
	m_listenerInfos.push_back(SListenerInfo(pListener, nFrames));
}

#endif // !NET_USE_SIMPLE_BREAKAGE

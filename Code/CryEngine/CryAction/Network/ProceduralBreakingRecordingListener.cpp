// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: recording of procedural breaks
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/

#include "StdAfx.h"

#if !NET_USE_SIMPLE_BREAKAGE

	#include "ProceduralBreakingRecordingListener.h"
	#include "GameContext.h"
	#include "BreakReplicator.h"
	#include "NetworkCVars.h"

CProceduralBreakingRecordingListener::CProceduralBreakingRecordingListener(IProceduralBreakTypePtr pBreakType)
	: CProceduralBreakingBaseListener(pBreakType)
	#if BREAK_HIERARCHICAL_TRACKING
	, m_frame(0)
	#else
	, m_gotRemove(false)
	#endif
{
	m_orderId = CBreakReplicator::Get()->PullOrderId();
}

CProceduralBreakingRecordingListener::~CProceduralBreakingRecordingListener()
{
	if (m_orderId >= 0 && CBreakReplicator::Get())
		CBreakReplicator::Get()->PushAbandonment(m_orderId);
}

int CProceduralBreakingRecordingListener::GetEntRef(EntityId id)
{
	if (!id)
	{
		CRY_ASSERT(false);
		return -1; // hrm...
	}
	for (int i = 0; i < m_ents.size(); i++)
		if (m_ents[i] == id)
			return i;
	m_ents.push_back(id);
	return m_ents.size() - 1;
}

bool CProceduralBreakingRecordingListener::AllowComplete()
{
	SProceduralBreakRecordingState state;
	#if BREAK_HIERARCHICAL_TRACKING
	state.gotRemove = m_openIds.empty();
	#else
	state.gotRemove = m_gotRemove;
	#endif
	state.numEmptySteps = GetNumEmptySteps();
	return GetBreakType()->AllowComplete(state);
}

bool CProceduralBreakingRecordingListener::AttemptAbsorb(const IProceduralBreakTypePtr& pBT)
{
	if (GetBreakType()->AttemptAbsorb(pBT))
	{
		GotBreakOp(eNBO_AbsorbStep, -1, -1, 0);
		return true;
	}
	return false;
}

void CProceduralBreakingRecordingListener::GotJointBreak(int idxRef, int id, int epicenter)
{
	// this is an auto created constraint joint to make trees look cooler when they're breaking
	if (id == 1000000)
		return;

	SJointBreakRec brk;
	brk.idxRef = idxRef;
	brk.id = id;
	#if BREAK_HIERARCHICAL_TRACKING
	brk.frame = m_frame;
	#endif
	brk.epicenter = epicenter;
	m_jointBreaks.push_back(brk);
}

void CProceduralBreakingRecordingListener::GotJointBreakParticle(const EventPhysCreateEntityPart* pEvent)
{
	SJointBreakParticleRec rec;
	pe_status_dynamics statusDyn;
	pEvent->pEntNew->GetStatus(&statusDyn);
	rec.vel = statusDyn.v;
	m_gibs.push_back(rec);
}

int CProceduralBreakingRecordingListener::GotBreakOp(ENetBreakOperation op, int idxRef, int partid, EntityId to)
{
	CRY_ASSERT(op >= eNBO_Create && op < eNBO_NUM_OPERATIONS);
	LOGBREAK("GotBreakOp(op:%d, idxRef:%d, partid:%d, entid:%x)", op, idxRef, partid, to);

	#if BREAK_HIERARCHICAL_TRACKING
	STATIC_CHECK(0 == (OPS_ADD_OPEN & OPS_REMOVE_OPEN), AddAndRemoveOpSetsIntersect);
	if (BIT(op) & OPS_ADD_OPEN)
		m_openIds.insert(idxRef);
	else if (BIT(op) & OPS_REMOVE_OPEN)
		m_openIds.erase(idxRef);
	#else
	m_gotRemove |= (op == eNBO_Remove_NoSpawn || op == eNBO_Remove);
	if (m_gotRemove)
		DisallowAdditions();
	#endif

	#if BREAK_HIERARCHICAL_TRACKING
	if (op == eNBO_PostStep && !m_spawnRecs.empty() && m_spawnRecs.back().op == eNBO_PostStep)
		return -1;

	if (BIT(op) & OPS_JOINT_FRAME_COUNTERS)
	{
		m_frame++;
		LOGBREAK("GotBreakOp: m_frame++ -> %d)", m_frame);
		CRY_ASSERT(m_frame > 0);
	}
	#endif

	SProceduralSpawnRec rec;
	rec.op = op;
	rec.partid = partid;
	if (BIT(op) & OPS_CAUSING_ENTS)
	{
		rec.idx = GetEntRef(to);
		if (rec.idx < 0)
			return -1;
	}
	if (BIT(op) & OPS_REFERENCING_ENTS)
		rec.idxRef = idxRef;
	m_spawnRecs.push_back(rec);

	LOGBREAK("GotBreakOp: push spawnRec: %d)", m_frame);

	return rec.idx;
}

void CProceduralBreakingRecordingListener::FinishBreakOp(ENetBreakOperation op, int idxRef, EntityId to, int virtId)
{
}

void CProceduralBreakingRecordingListener::Complete()
{
	_smart_ptr<SProceduralBreak> brk = GetBreakType()->CompleteSend();
	if (!brk)
		return;

	LOGBREAK("Created an SProceduralBreak from a CompleteSend()");

	#if BREAK_HIERARCHICAL_TRACKING
	while (!m_spawnRecs.empty() && m_spawnRecs.back().op == eNBO_PostStep)
		m_spawnRecs.pop_back();
	#endif

	GetBreakType()->PatchRecording(m_spawnRecs, m_jointBreaks);

	char message[256];
	cry_sprintf(message, "recording %p order=%d", this, m_orderId);
	DumpSpawnRecs(m_spawnRecs, m_jointBreaks, message);
	brk->spawnRecs = m_spawnRecs;
	brk->gibs = m_gibs;

	//	std::sort(m_jointBreaks.begin(), m_jointBreaks.end());
	//	m_jointBreaks.resize( std::unique(m_jointBreaks.begin(), m_jointBreaks.end()) - m_jointBreaks.begin() );
	brk->jointBreaks = m_jointBreaks;

	if (CNetworkCVars::Get().BreakageLog)
		brk->magicId = m_orderId;

	SNetBreakDescription desc;
	if (GetBreakType()->SendOnlyOnClientJoin())
	{
		desc.flags = (ENetBreakDescFlags)(desc.flags | eNBF_SendOnlyOnClientJoin);
	}
	desc.nEntities = m_ents.size();
	EntityId blah = 0;
	desc.pEntities = m_ents.empty() ? &blah : &m_ents[0];
	desc.pMessagePayload = &*brk;
	CBreakReplicator::Get()->PushBreak(m_orderId, desc);
	m_orderId = -1;
	//	CCryAction::GetCryAction()->GetGameContext()->GetNetContext()->LogBreak(desc);
}

#endif // !NET_USE_SIMPLE_BREAKAGE

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: plazback of procedural breaks
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/

#include "StdAfx.h"

#if !NET_USE_SIMPLE_BREAKAGE

	#include "ProceduralBreakingPlaybackListener.h"
	#include "BreakReplicator.h"
	#include "NetworkCVars.h"

CProceduralBreakingPlaybackListener::CProceduralBreakingPlaybackListener(_smart_ptr<CProceduralBreakingPlaybackStream> pStream, INetBreakagePlaybackPtr pBreakage)
	: CProceduralBreakingBaseListener(pStream->m_pBreakType)
	, m_pStream(pStream)
	, m_pBreakage(pBreakage)
	#if BREAK_HIERARCHICAL_TRACKING
	, m_playbackFrame(0)
	#endif
{
	m_gibIdx = 0;
}

void CProceduralBreakingPlaybackListener::UpdateJointBreaks()
{
	DynArray<SJointBreakRec>& jbs = m_pStream->m_jointBreaks;

	#if BREAK_HIERARCHICAL_TRACKING
	if (CNetworkCVars::Get().BreakageLog)
		CryLog("[brk] playback frame %d", m_playbackFrame);
	#endif

	// replay breaks
	for (size_t i = 0; i < jbs.size(); ++i)
	{
	#if BREAK_HIERARCHICAL_TRACKING
		if (m_createdObjects.find(jbs[i].idxRef) == m_createdObjects.end())
			continue;
		if (m_playbackFrame != jbs[i].frame)
			continue;
	#endif

		CObjectSelector obj = GetObjectSelector(jbs[i].idxRef);
		const char* desc = CObjectSelector::GetDescription(jbs[i].idxRef).c_str();

	#if BREAK_HIERARCHICAL_TRACKING
		if (CNetworkCVars::Get().BreakageLog)
			CryLog("[brk] break joint %d on ref %d (%s) [reached joint frame %d]", jbs[i].id, jbs[i].idxRef, desc, int(jbs[i].frame));
	#else
		if (CNetworkCVars::Get().BreakageLog)
			CryLog("[brk] break joint %d on ref %d (%s)", jbs[i].id, jbs[i].idxRef, desc);
	#endif

		pe_params_structural_joint pj;
		pj.partidEpicenter = jbs[i].epicenter;
		if (jbs[i].id >= 0)
		{
			pj.id = jbs[i].id;
			pj.bBroken = true;
			pj.bReplaceExisting = true;
		}
		else
		{
			pj.idx = -1;
		}
		bool succeeded = false;
		if (IPhysicalEntity* pEnt = obj.Find())
		{
			if (!pEnt->SetParams(&pj))
				GameWarning("Failed to break joint-id %d on %s", pj.id, desc);
			else
				succeeded = true;
		}
		else
			GameWarning("Unable to find object %s to break joint-id %d on", desc, pj.id);

		if (succeeded)
		{
			jbs.erase(i);
			--i;
		}
	}
}

void CProceduralBreakingPlaybackListener::Start()
{
	char message[256];
	cry_sprintf(message, "begin playback %d", m_pStream->m_magicId);
	DumpSpawnRecs(m_pStream->m_spawnRecs, m_pStream->m_jointBreaks, message);
	#if BREAK_HIERARCHICAL_TRACKING
	DynArray<SJointBreakRec>& jbs = m_pStream->m_jointBreaks;
	for (size_t i = 0; i < jbs.size(); ++i)
		if (jbs[i].idxRef < 0)
			m_createdObjects.insert(jbs[i].idxRef);
	#endif
	UpdateJointBreaks();
	CompleteStepFrom(m_pStream->m_spawnRecs.begin());
}

int CProceduralBreakingPlaybackListener::GotBreakOp(ENetBreakOperation op, int idxRef, int partid, EntityId to)
{
	DynArray<SProceduralSpawnRec>& spawnRecs = m_pStream->m_spawnRecs;
	bool priorAreInvalidated = true;

	for (DynArray<SProceduralSpawnRec>::iterator it = spawnRecs.begin(); it != spawnRecs.end(); ++it)
	{
		bool priorWereInvalidated = priorAreInvalidated;
		priorAreInvalidated &= (it->op == eNBO_NUM_OPERATIONS);
		if (it->op != op)
			continue;
		if ((BIT(op) & OPS_REFERENCING_ENTS))
			if (it->idxRef != idxRef)
				continue;
		if ((BIT(op) & OPS_WITH_PARTIDS))
			if (it->partid != partid)
				continue;
	#if BREAK_HIERARCHICAL_TRACKING
		if (!priorWereInvalidated)
		{
			if (op == eNBO_PostStep)
				break;
			else if (BIT(op) & OPS_JOINT_FRAME_COUNTERS)
				it->op = eNBO_FrameCounterFinished;
		}
		if (it->op == op) // still
		{
			it->op = eNBO_NUM_OPERATIONS; // invalidate
			if (BIT(op) & OPS_JOINT_FRAME_COUNTERS)
				IncrementFrame();
		}
	#else
		it->op = eNBO_NUM_OPERATIONS;
	#endif
		m_pBreakage->SpawnedEntity(it->idx, to);
		if (priorWereInvalidated)
			CompleteStepFrom(it + 1);
		return it->idx;
	}
	#if BREAK_HIERARCHICAL_TRACKING
	if (op != eNBO_PostStep)
	#endif
	GameWarning("CProceduralBreakingPlaybackListener: mismatched spawn event %d:%d[part=%d]->%d", op, idxRef, partid, to);
	return -1;
}

	#if BREAK_HIERARCHICAL_TRACKING
void CProceduralBreakingPlaybackListener::IncrementFrame()
{
	m_playbackFrame++;
	UpdateJointBreaks();
}
	#endif

void CProceduralBreakingPlaybackListener::CompleteStepFrom(DynArray<SProceduralSpawnRec>::iterator it)
{
	DynArray<SProceduralSpawnRec>& spawnRecs = m_pStream->m_spawnRecs;
	while (it != spawnRecs.end())
	{
		switch (it->op)
		{
		case eNBO_AbsorbStep:
			GetBreakType()->AbsorbStep();
			it->op = eNBO_NUM_OPERATIONS;
			break;
	#if BREAK_HIERARCHICAL_TRACKING
		case eNBO_FrameCounterFinished:
			it->op = eNBO_NUM_OPERATIONS;
			IncrementFrame();
			break;
	#endif
		}
		if (it->op == eNBO_NUM_OPERATIONS)
			++it;
		else
			break;
	}
}

void CProceduralBreakingPlaybackListener::FinishBreakOp(ENetBreakOperation op, int idxRef, EntityId to, int virtId)
{
	#if BREAK_HIERARCHICAL_TRACKING
	if (m_createdObjects.insert(virtId).second)
		UpdateJointBreaks();
	#endif
}

void CProceduralBreakingPlaybackListener::GotJointBreakParticle(const EventPhysCreateEntityPart* pEvent)
{
	// This is experimental serialisation of the gib particle velocity
	const unsigned int i = m_gibIdx;
	DynArray<SJointBreakParticleRec>& gibs = m_pStream->m_gibs;
	if (i < (unsigned)gibs.size())
	{
		LOGBREAK("UpdateJointBreaks: applying gib Vel (%f, %f, %f)", XYZ(gibs[i].vel));
		pe_action_set_velocity setVel;
		setVel.v = gibs[i].vel;
		pEvent->pEntNew->Action(&setVel);
	}
	m_gibIdx++;
}

void CProceduralBreakingPlaybackListener::Complete()
{
	char message[256];
	cry_sprintf(message, "end playback %d", m_pStream->m_magicId);
	DumpSpawnRecs(m_pStream->m_spawnRecs, m_pStream->m_jointBreaks, message);
}

bool CProceduralBreakingPlaybackListener::AllowComplete()
{
	if (!m_pStream->m_jointBreaks.empty())
		return false;
	for (size_t i = 0; i < m_pStream->m_spawnRecs.size(); i++)
		if (m_pStream->m_spawnRecs[i].op != eNBO_NUM_OPERATIONS)
			return false;
	return true;
}

CProceduralBreakingPlaybackStream::CProceduralBreakingPlaybackStream(IProceduralBreakTypePtr pBreakType) : m_pBreakType(pBreakType), m_magicId(-1)
{
}

CProceduralBreakingPlaybackStream::~CProceduralBreakingPlaybackStream()
{
}

bool CProceduralBreakingPlaybackStream::SetMagicId(int id)
{
	if (m_magicId >= 0 || id < 0)
		return false;
	m_magicId = id;
	return true;
}

bool CProceduralBreakingPlaybackStream::GotExplosiveObjectState(const SExplosiveObjectState* state)
{
	return m_pBreakType->GotExplosiveObjectState(state);
}

bool CProceduralBreakingPlaybackStream::GotJointBreakRec(const SJointBreakRec* rec)
{
	m_jointBreaks.push_back(*rec);
	return true;
}

bool CProceduralBreakingPlaybackStream::GotJointBreakParticleRec(const SJointBreakParticleRec* rec)
{
	m_gibs.push_back(*rec);
	return true;
}

bool CProceduralBreakingPlaybackStream::GotProceduralSpawnRec(const SProceduralSpawnRec* rec)
{
	CRY_ASSERT(rec->op >= eNBO_Create && rec->op < eNBO_NUM_OPERATIONS);
	m_spawnRecs.push_back(*rec);
	return true;
}

IBreakReplicatorListenerPtr CProceduralBreakingPlaybackStream::Playback(CBreakReplicator* pReplicator, INetBreakagePlaybackPtr pBreak)
{
	bool hasJointBreaks = !m_jointBreaks.empty();
	_smart_ptr<CProceduralBreakingPlaybackListener> pListener = new CProceduralBreakingPlaybackListener(this, pBreak);
	pReplicator->AddListener(&*pListener, CNetworkCVars::Get().BreakTimeoutFrames);
	m_pBreakType->PreparePlayback();
	pListener->Start();
	m_pBreakType->BeginPlayback(hasJointBreaks);
	return &*pListener;
}

bool CProceduralBreakingPlaybackStream::AttemptAbsorb(const IProceduralBreakTypePtr& pBT)
{
	return m_pBreakType->AttemptAbsorb(pBT);
}

#endif // !NET_USE_SIMPLE_BREAKAGE

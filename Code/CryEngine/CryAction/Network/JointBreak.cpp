// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if !NET_USE_SIMPLE_BREAKAGE

	#include "JointBreak.h"
	#include "BreakReplicator.h"
	#include "NetworkCVars.h"

void SJointBreak::AddSendables(INetSendableSink* pSink, int32 brkId)
{
	CBreakReplicator::SendJointBreakWith(SJointBreakParams(brkId), pSink);
	for (size_t i = 0; i < states.size(); i++)
	{
		if (states[i].isEnt)
			pSink->NextRequiresEntityEnabled(states[i].entId);
		CBreakReplicator::SendDeclareExplosiveObjectStateWith(SDeclareExplosiveObjectState(brkId, states[i]), pSink);
	}
	SProceduralBreak::AddProceduralSendables(brkId, pSink);
}

	#if BREAK_HIERARCHICAL_TRACKING
static const uint32 JointBreakPBTFlags = ePBTF_ChainBreaking;
	#else
static const uint32 JointBreakPBTFlags = 0;
	#endif

CJointBreak::CJointBreak() : IProceduralBreakType(JointBreakPBTFlags, 'j')
{
}

CJointBreak::CJointBreak(IPhysicalEntity* pEnt) : IProceduralBreakType(JointBreakPBTFlags, 'j')
{
	SExplosiveObjectState state;
	ExplosiveObjectStateFromPhysicalEntity(state, pEnt);
	m_states.push_back(state);

	pe_status_pos sts;
	pEnt->GetStatus(&sts);
	// TODO: use BBox
	m_rgn = AABB(sts.pos - Vec3(10, 10, 10), sts.pos + Vec3(10, 10, 10));
}

int CJointBreak::GetVirtualId(IPhysicalEntity* pEnt)
{
	int iForeignData = pEnt->GetiForeignData();
	void* pForeignData = pEnt->GetForeignData(iForeignData);
	if (iForeignData == PHYS_FOREIGN_ID_ENTITY && pForeignData)
	{
		EntityId entId = ((IEntity*)pForeignData)->GetId();
		for (size_t i = 0; i < m_states.size(); i++)
		{
			if (m_states[i].isEnt && m_states[i].entId == entId)
				return i + 1;
		}
	}
	else if (iForeignData == PHYS_FOREIGN_ID_STATIC && pForeignData)
	{
		IRenderNode* rn = (IRenderNode*) pForeignData;
		const uint32 hash = CObjectSelector::CalculateHash(rn);
		for (size_t i = 0; i < m_states.size(); i++)
			if (!m_states[i].isEnt && m_states[i].hash == hash)
				return i + 1;
	}
	return 0;
}

CObjectSelector CJointBreak::GetObjectSelector(int idx)
{
	if (idx >= 1 && idx <= m_states.size())
	{
		idx--;
		if (m_states[idx].isEnt)
			return CObjectSelector(m_states[idx].entId);
		else
			return CObjectSelector(m_states[idx].eventPos, m_states[idx].hash);
	}
	return CObjectSelector();
}

bool CJointBreak::GotExplosiveObjectState(const SExplosiveObjectState* pState)
{
	m_states.push_back(*pState);
	return true;
}

void CJointBreak::PreparePlayback()
{
	for (size_t i = 0; i < m_states.size(); i++)
	{
		if (m_states[i].isEnt)
		{
			if (IEntity* pEnt = gEnv->pEntitySystem->GetEntity(m_states[i].entId))
			{
				pEnt->SetPosRotScale(m_states[i].entPos, m_states[i].entRot, m_states[i].entScale);
			}
			else
			{
				GameWarning("[brk] CJointBreak::PreparePlayback unable to find entity %.8x to prepare for playback", m_states[i].entId);
			}
		}
	}
}

bool CJointBreak::AttemptAbsorb(const IProceduralBreakTypePtr& pBT)
{
	if (pBT->type == type)
	{
		// can always absorb another joint break
		CJointBreak* pJB = (CJointBreak*)(IProceduralBreakType*)pBT;
		switch (pJB->m_states.size())
		{
		case 1:
			m_states.push_back(pJB->m_states[0]);
			return true;
		case 0:
			GameWarning("CJointBreak::AttemptAbsorb: nothing to absorb from new break");
			return false;
		default:
			GameWarning("CJointBreak::AttemptAbsorb: too many things to absorb");
			return false;
		}
	}
	return false;
}

void CJointBreak::BeginPlayback(bool hasJointBreaks)
{
	/*
	   if (!hasJointBreaks)
	   {
	    // need to add some threshold to force a joint break of an object that has no joints
	    CObjectSelector sel;
	    if (m_state.isEnt)
	      sel = CObjectSelector(m_state.entId);
	    else
	      sel = CObjectSelector(m_state.objPos, m_state.objCenter, m_state.objVolume);
	    if (CNetworkCVars::Get().BreakageLog)
	      CryLogAlways("[brk] CJointBreak::BeginPlayback: apply impulse to simulate joint break on object %s", sel.GetDescription().c_str());
	    if (IPhysicalEntity * pEnt = sel.Find())
	    {
	      pe_params_structural_joint ppsj;
	      ppsj.idx = -1;
	      ppsj.partidEpicenter = 0;
	      pEnt->SetParams(&ppsj);
	    }
	    else
	    {
	      GameWarning("[brk] CJointBreak::BeginPlayback: couldn't apply fake-impulse since we could not find the object %s", sel.GetDescription().c_str());
	    }
	   }
	 */
}

_smart_ptr<SProceduralBreak> CJointBreak::CompleteSend()
{
	SJointBreak* pjb = new SJointBreak;
	pjb->region = m_rgn;
	pjb->states = m_states;
	return pjb;
}

bool CJointBreak::AllowComplete(const SProceduralBreakRecordingState& state)
{
	return state.gotRemove && state.numEmptySteps > 0;
}

void CJointBreak::PatchRecording(DynArray<SProceduralSpawnRec>& spawnRecs, DynArray<SJointBreakRec>& jointBreaks)
{
	LOGBREAK("PatchRecording");
	std::map<int, int> rootEntitiesAndBreaks;
	for (int i = 0; i < spawnRecs.size(); i++)
		if (BIT(spawnRecs[i].op) & OPS_REFERENCING_ENTS)
			if (spawnRecs[i].idxRef < 0)
			{
				LOGBREAK("  -rootEntitiesAndBreaks[%d]=0", spawnRecs[i].idxRef);
				rootEntitiesAndBreaks[spawnRecs[i].idxRef] = 0;
			}
	for (int i = 0; i < jointBreaks.size(); i++)
		if (jointBreaks[i].idxRef < 0)
		{
			LOGBREAK("  -rootEntitiesAndBreaks[%d]++", jointBreaks[i].idxRef);
			rootEntitiesAndBreaks[jointBreaks[i].idxRef]++;
		}
	for (std::map<int, int>::iterator it = rootEntitiesAndBreaks.begin(); it != rootEntitiesAndBreaks.end(); ++it)
	{
		if (!it->second)
		{
			SJointBreakRec jb;
			jb.idxRef = it->first;
			jb.id = -1;
	#if BREAK_HIERARCHICAL_TRACKING
			jb.frame = 0;
	#endif
			jb.epicenter = 0;
			LOGBREAK("  - pushed SJointBreakRec");
			jointBreaks.push_back(jb);
		}
	}
}

#endif // ! NET_USE_SIMPLE_BREAKAGE

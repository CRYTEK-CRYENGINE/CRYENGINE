// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: deforming breaks
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/

#include "StdAfx.h"
#include "DeformingBreak.h"
#include "BreakReplicator.h"
#include "NetworkCVars.h"
#include "ObjectSelector.h"

void LogDeformPhysicalEntity(const char* from, IPhysicalEntity* pEnt, const Vec3& p, const Vec3& n, float energy)
{
#if DEBUG_NET_BREAKAGE
	if (!pEnt)
		return;

	if (CNetworkCVars::Get().BreakageLog)
	{
		CryLog("[brk] DeformPhysicalEntity on %s @ (%.8f,%.8f,%.8f); n=(%.8f,%.8f,%.8f); energy=%.8f", from, p.x, p.y, p.z, n.x, n.y, n.z, energy);
		CryLog("[brk]    selector is %s", CObjectSelector::GetDescription(pEnt).c_str());
		switch (pEnt->GetiForeignData())
		{
		case PHYS_FOREIGN_ID_STATIC:
			if (IRenderNode* pRN = (IRenderNode*)pEnt->GetForeignData(PHYS_FOREIGN_ID_STATIC))
			{
				CryLog("[brk]    name is %s", pRN->GetName());
				CryLog("[brk]    entity class name is %s", pRN->GetEntityClassName());
				CryLog("[brk]    debug string is %s", pRN->GetDebugString().c_str());
			}
			break;
		case PHYS_FOREIGN_ID_ENTITY:
			if (IEntity* pEntity = (IEntity*)pEnt->GetForeignData(PHYS_FOREIGN_ID_ENTITY))
			{
				CryLog("[brk]    name is %s", pEntity->GetName());
				Matrix34 m = pEntity->GetWorldTM();
				CryLog("[brk]    world tm:");
				for (int i = 0; i < 3; i++)
				{
					Vec4 row = m.GetRow4(i);
					CryLog("[brk]       %+12.8f %+12.8f %+12.8f %+12.8f", row[0], row[1], row[2], row[3]);
				}
			}
			break;
		}
	}
#endif
}

#if !NET_USE_SIMPLE_BREAKAGE

void SDeformingBreakParams::SerializeWith(TSerialize ser)
{
	LOGBREAK("SDeformingBreakParams: %s", ser.IsReading() ? "Reading:" : "Writing");

	ser.Value("breakId", breakId, 'brId');
	bool isEnt = breakEvent.itype == PHYS_FOREIGN_ID_ENTITY;
	ser.Value("isEnt", isEnt, 'bool');
	breakEvent.itype = isEnt ? PHYS_FOREIGN_ID_ENTITY : PHYS_FOREIGN_ID_STATIC;
	if (isEnt)
	{
		ser.Value("ent", breakEvent.idEnt, 'eid');
		ser.Value("pos", breakEvent.pos);
		ser.Value("rot", breakEvent.rot);
		ser.Value("scale", breakEvent.scale);
	}
	else
	{
		ser.Value("eventPos", breakEvent.eventPos);
		ser.Value("hash", breakEvent.hash);
	}
	ser.Value("pt", breakEvent.pt);
	ser.Value("n", breakEvent.n);
	ser.Value("energy", breakEvent.energy);
}

void SDeformingBreak::GetAffectedRegion(AABB& rgn)
{
	rgn.min = breakEvents[0].pt - Vec3(20, 20, 20);
	rgn.max = breakEvents[0].pt + Vec3(20, 20, 20);
}

void SDeformingBreak::AddSendables(INetSendableSink* pSink, int32 brkId)
{
	if (breakEvents[0].itype == PHYS_FOREIGN_ID_ENTITY)
		pSink->NextRequiresEntityEnabled(breakEvents[0].idEnt);
	for (int i = 0; i < breakEvents.size(); ++i)
		CBreakReplicator::SendDeformingBreakWith(SDeformingBreakParams(brkId, breakEvents[i]), pSink);
	AddProceduralSendables(brkId, pSink);
}

CDeformingBreak::CDeformingBreak(const SBreakEvent& be) : IProceduralBreakType(ePBTF_ChainBreaking, 'd'), m_pPhysEnt(0)
{
	m_absorbIdx = 1;
	m_bes.push_back(be);
	m_bes.back().iState = eBES_Generated;
	m_bes.back().time = gEnv->pTimer->GetFrameStartTime().GetSeconds();
}

bool CDeformingBreak::AttemptAbsorb(const IProceduralBreakTypePtr& pBT)
{
	if (pBT->type == type)
	{
		CDeformingBreak* pBrk = (CDeformingBreak*)(IProceduralBreakType*)pBT;
		if (GetObjectSelector(1) == pBrk->GetObjectSelector(1))
		{
			m_bes.push_back(pBrk->m_bes[0]);
			return true;
		}
	}
	return false;
}

void CDeformingBreak::AbsorbStep()
{
	if (m_absorbIdx >= m_bes.size())
	{
		GameWarning("CDeformingBreak::AbsorbStep: too many absorbs for structure (or there was an earlier message and we invalidated m_absorbIdx)");
		return;
	}
	if (!m_pPhysEnt)
	{
		GameWarning("CDeformingBreak::AbsorbStep: attempt to deform a null entity");
		m_absorbIdx = m_bes.size();
		return;
	}
	CRY_ASSERT(m_absorbIdx < m_bes.size());
	if (m_absorbIdx >= m_bes.size())
		return;
	PreparePlaybackForEvent(m_absorbIdx);
	if (!m_pPhysEnt)
	{
		GameWarning("CDeformingBreak::AbsorbStep: attempt to deform a null entity after preparing index %d", m_absorbIdx);
		m_absorbIdx = m_bes.size();
		return;
	}
	LogDeformPhysicalEntity("CLIENT", m_pPhysEnt, m_bes[m_absorbIdx].pt, -m_bes[m_absorbIdx].n, m_bes[m_absorbIdx].energy);
	if (!gEnv->pPhysicalWorld->DeformPhysicalEntity(m_pPhysEnt, m_bes[m_absorbIdx].pt, -m_bes[m_absorbIdx].n, m_bes[m_absorbIdx].energy))
		GameWarning("[brk] DeformPhysicalEntity failed");
	++m_absorbIdx;
}

int CDeformingBreak::GetVirtualId(IPhysicalEntity* pEnt)
{
	int iForeignData = pEnt->GetiForeignData();
	void* pForeignData = pEnt->GetForeignData(iForeignData);
	if (iForeignData == PHYS_FOREIGN_ID_ENTITY && m_bes[0].itype == PHYS_FOREIGN_ID_ENTITY)
	{
		IEntity* pForeignEnt = (IEntity*) pForeignData;
		if (pForeignEnt->GetId() == m_bes[0].idEnt)
			return 1;
	}
	else if (iForeignData == PHYS_FOREIGN_ID_STATIC)
	{
		IRenderNode* rn = (IRenderNode*) pForeignData;
		if (IsOurStatObj(rn))
			return 1;
	}
	return 0;
}

CObjectSelector CDeformingBreak::GetObjectSelector(int idx)
{
	if (idx == 1)
	{
		if (m_bes[0].itype == PHYS_FOREIGN_ID_ENTITY)
			return CObjectSelector(m_bes[0].idEnt);
		else if (m_bes[0].itype == PHYS_FOREIGN_ID_STATIC)
			return CObjectSelector(m_bes[0].eventPos, m_bes[0].hash);
	}
	return CObjectSelector();
}

_smart_ptr<SProceduralBreak> CDeformingBreak::CompleteSend()
{
	SDeformingBreak* pPayload = new SDeformingBreak;
	pPayload->breakEvents = m_bes;
	return pPayload;
}

void CDeformingBreak::PreparePlayback()
{
	PreparePlaybackForEvent(0);
}

void CDeformingBreak::PreparePlaybackForEvent(int event)
{
	m_pPhysEnt = 0;
	if (m_bes[event].itype == PHYS_FOREIGN_ID_ENTITY)
	{
		IEntity* pEnt = gEnv->pEntitySystem->GetEntity(m_bes[event].idEnt);
		if (!pEnt)
		{
			GameWarning("Unable to find entity to perform deforming break on");
			return;
		}
		pEnt->SetPosRotScale(m_bes[event].pos, m_bes[event].rot, m_bes[event].scale);
		m_pPhysEnt = pEnt->GetPhysics();
	}
	else if (m_bes[event].itype == PHYS_FOREIGN_ID_STATIC)
	{
		m_pPhysEnt = CObjectSelector::FindPhysicalEntity(m_bes[event].eventPos, m_bes[event].hash, CObjectSelector_Eps);
		if (!m_pPhysEnt)
			GameWarning("Unable to find static object to perform deforming break on");
	}
}

void CDeformingBreak::BeginPlayback(bool hasJointBreaks)
{
	if (hasJointBreaks && CNetworkCVars::Get().BreakageLog)
		GameWarning("[brk] deforming break has joint breaks attached");
	if (m_pPhysEnt)
	{
		LogDeformPhysicalEntity("CLIENT", m_pPhysEnt, m_bes[0].pt, -m_bes[0].n, m_bes[0].energy);
		if (!gEnv->pPhysicalWorld->DeformPhysicalEntity(m_pPhysEnt, m_bes[0].pt, -m_bes[0].n, m_bes[0].energy))
			GameWarning("[brk] DeformPhysicalEntity failed");
	}
	else
		GameWarning("[brk] No physical entity to deform");
}

bool CDeformingBreak::GotExplosiveObjectState(const SExplosiveObjectState* pState)
{
	return false;
}

bool CDeformingBreak::AllowComplete(const SProceduralBreakRecordingState& state)
{
	return state.numEmptySteps > 3;
}

#endif // !NET_USE_SIMPLE_BREAKAGE

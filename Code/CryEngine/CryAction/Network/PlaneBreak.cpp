// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: Plane breaks
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/

#include "StdAfx.h"

#if !NET_USE_SIMPLE_BREAKAGE

	#include "PlaneBreak.h"
	#include "BreakReplicator.h"
	#include "NetworkCVars.h"
	#include "SerializeBits.h"
	#include "SerializeDirHelper.h"

void LogPlaneBreak(const char* from, const SBreakEvent& evt)
{
	/*
	   if (!pEnt)
	    return;

	   if (CNetworkCVars::Get().BreakageLog)
	   {
	    CryLog("[brk] DeformPhysicalEntity on %s @ (%.8f,%.8f,%.8f); n=(%.8f,%.8f,%.8f); energy=%.8f", from, p.x, p.y, p.z, n.x, n.y, n.z, energy);
	    CObjectSelector sel(pEnt);
	    CryLog("[brk]    selector is %s", sel.GetDescription().c_str());
	    switch (pEnt->GetiForeignData())
	    {
	    case PHYS_FOREIGN_ID_STATIC:
	      if (IRenderNode * pRN = (IRenderNode*)pEnt->GetForeignData(PHYS_FOREIGN_ID_STATIC))
	      {
	        CryLog("[brk]    name is %s", pRN->GetName());
	        CryLog("[brk]    entity class name is %s", pRN->GetEntityClassName());
	        CryLog("[brk]    debug string is %s", pRN->GetDebugString().c_str());
	      }
	      break;
	    case PHYS_FOREIGN_ID_ENTITY:
	      if (IEntity * pEntity = (IEntity*)pEnt->GetForeignData(PHYS_FOREIGN_ID_ENTITY))
	      {
	        CryLog("[brk]    name is %s", pEntity->GetName());
	        Matrix34 m = pEntity->GetWorldTM();
	        CryLog("[brk]    world tm:");
	        for (int i=0; i<3; i++)
	        {
	          Vec4 row = m.GetRow4(i);
	          CryLog("[brk]       %+12.8f %+12.8f %+12.8f %+12.8f", row[0], row[1], row[2], row[3]);
	        }
	      }
	      break;
	    }
	   }
	 */
}

static int s_debugId = 1;

#pragma warning(push)
#pragma warning(disable : 6262)// 32k of stack space of CBitArray
void SPlaneBreakParams::SerializeWith(TSerialize ser)
{
	CBitArray array(&ser);

	bool isEnt = breakEvent.itype == PHYS_FOREIGN_ID_ENTITY;
	array.Serialize(breakId, 0, 6); // Must match with MAX_BREAK_STREAMS!
	array.Serialize(isEnt);

	breakEvent.itype = isEnt ? PHYS_FOREIGN_ID_ENTITY : PHYS_FOREIGN_ID_STATIC;
	if (isEnt)
	{
		array.SerializeEntity(breakEvent.idEnt);
		array.Serialize(breakEvent.pos, 0.f, 1e4f, 20);
		array.Serialize(breakEvent.rot);
	}
	else
	{
		array.Serialize(breakEvent.hash, 0, 0xffffffff);
	}
	array.Serialize(breakEvent.pt, 0.f, 1e4f, 22);

	breakEvent.partid[1] = (breakEvent.partid[1] / PARTID_CGA) << 8 | (breakEvent.partid[1] % PARTID_CGA);

	SerializeDirHelper(array, breakEvent.n, 8, 8);
	array.Serialize(breakEvent.idmat[0], -1, 254);
	array.Serialize(breakEvent.idmat[1], -1, 254);
	array.Serialize(breakEvent.partid[1], 0, 0xffff);
	array.Serialize(breakEvent.mass[0], 1.f, 1000.f, 8);
	SerializeDirVector(array, breakEvent.vloc[0], 20.f, 8, 8, 8);

	breakEvent.partid[1] = (breakEvent.partid[1] & 0xff) + (breakEvent.partid[1] >> 8) * PARTID_CGA;

	// Looks like we dont need these
	breakEvent.partid[0] = 0;
	breakEvent.scale.Set(1.f, 1.f, 1.f);
	breakEvent.radius = 0.f;
	breakEvent.vloc[1].zero();   // Just assume the hitee is at zero velocity
	breakEvent.energy = 0.f;
	breakEvent.penetration = 0.f;
	breakEvent.mass[1] = 0.f;
	breakEvent.seed = 0;

	if (ser.IsWriting()) array.WriteToSerializer();
}
#pragma warning(pop)

void SPlaneBreak::GetAffectedRegion(AABB& rgn)
{
	rgn.min = breakEvents[0].pt - Vec3(20, 20, 20);
	rgn.max = breakEvents[0].pt + Vec3(20, 20, 20);
}

void SPlaneBreak::AddSendables(INetSendableSink* pSink, int32 brkId)
{
	if (breakEvents[0].itype == PHYS_FOREIGN_ID_ENTITY)
		pSink->NextRequiresEntityEnabled(breakEvents[0].idEnt);
	for (int i = 0; i < breakEvents.size(); ++i)
		CBreakReplicator::SendPlaneBreakWith(SPlaneBreakParams(brkId, breakEvents[i]), pSink);
	AddProceduralSendables(brkId, pSink);
}

CPlaneBreak::CPlaneBreak(const SBreakEvent& be) : IProceduralBreakType(ePBTF_ChainBreaking, 'd'), m_pPhysEnt(0)
{
	m_absorbIdx = 1;
	m_bes.push_back(be);
	m_bes.back().iState = eBES_Generated;
	m_bes.back().time = gEnv->pTimer->GetFrameStartTime().GetSeconds();
}

bool CPlaneBreak::AttemptAbsorb(const IProceduralBreakTypePtr& pBT)
{
	if (pBT->type == type)
	{
		CPlaneBreak* pBrk = (CPlaneBreak*)(IProceduralBreakType*)pBT;
		if ((m_bes[0].hash && m_bes[0].hash == pBrk->m_bes[0].hash) ||
		    GetObjectSelector(1) == pBrk->GetObjectSelector(1))
		{
			m_bes.push_back(pBrk->m_bes[0]);
			m_bes.back().iState = eBES_Generated;
			m_bes.back().time = gEnv->pTimer->GetFrameStartTime().GetSeconds();
			return true;
		}
	}
	return false;
}

void CPlaneBreak::AbsorbStep()
{
	if (m_absorbIdx >= m_bes.size())
	{
		GameWarning("CPlaneBreak::AbsorbStep: too many absorbs for structure (or there was an earlier message and we invalidated m_absorbIdx)");
		return;
	}
	if (!m_pPhysEnt)
	{
		GameWarning("CPlaneBreak::AbsorbStep: attempt to deform a null entity");
		m_absorbIdx = m_bes.size();
		return;
	}
	CRY_ASSERT(m_absorbIdx < m_bes.size());
	if (m_absorbIdx >= m_bes.size())
		return;
	PreparePlaybackForEvent(m_absorbIdx);
	if (!m_pPhysEnt)
	{
		GameWarning("CPlaneBreak::AbsorbStep: attempt to deform a null entity after preparing index %d", m_absorbIdx);
		m_absorbIdx = m_bes.size();
		return;
	}
	LogPlaneBreak("CLIENT", m_bes[m_absorbIdx]);
	CActionGame::PerformPlaneBreak(GeneratePhysicsEvent(m_bes[m_absorbIdx]), &m_bes[m_absorbIdx]);

	//	if (!gEnv->pPhysicalWorld->DeformPhysicalEntity(m_pPhysEnt, m_bes[m_absorbIdx].pt, -m_bes[m_absorbIdx].n, m_bes[m_absorbIdx].energy))
	//		GameWarning("[brk] DeformPhysicalEntity failed");

	++m_absorbIdx;
}

int CPlaneBreak::GetVirtualId(IPhysicalEntity* pEnt)
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

CObjectSelector CPlaneBreak::GetObjectSelector(int idx)
{
	if (idx == 1)
	{
		if (m_bes[0].itype == PHYS_FOREIGN_ID_ENTITY)
			return CObjectSelector(m_bes[0].idEnt);
		else if (m_bes[0].itype == PHYS_FOREIGN_ID_STATIC)
			return CObjectSelector(m_bes[0].pt, m_bes[0].hash);
	}
	return CObjectSelector();
}

_smart_ptr<SProceduralBreak> CPlaneBreak::CompleteSend()
{
	SPlaneBreak* pPayload = new SPlaneBreak;
	pPayload->breakEvents = m_bes;
	return pPayload;
}

void CPlaneBreak::PreparePlayback()
{
	PreparePlaybackForEvent(0);
}

void CPlaneBreak::PreparePlaybackForEvent(int event)
{
	m_pPhysEnt = 0;
	if (m_bes[event].itype == PHYS_FOREIGN_ID_ENTITY)
	{
		IEntity* pEnt = gEnv->pEntitySystem->GetEntity(m_bes[event].idEnt);
		if (!pEnt)
		{
			GameWarning("Unable to find entity to perform Plane break on");
			return;
		}
		Vec3 scale = pEnt->GetScale();
		pEnt->SetPosRotScale(m_bes[event].pos, m_bes[event].rot, scale);
		m_pPhysEnt = pEnt->GetPhysics();
	}
	else if (m_bes[event].itype == PHYS_FOREIGN_ID_STATIC)
	{
		const SBreakEvent& be = m_bes[event];
		m_pPhysEnt = CObjectSelector::FindPhysicalEntity(be.pt, be.hash, 0.03f);
		if (!m_pPhysEnt)
			GameWarning("Unable to find static object to perform Plane break on");
	}
}

void CPlaneBreak::BeginPlayback(bool hasJointBreaks)
{
	if (hasJointBreaks && CNetworkCVars::Get().BreakageLog)
		GameWarning("[brk] Plane break has joint breaks attached");
	if (m_pPhysEnt)
	{
		LOGBREAK("PlaneBreak::BeginPlayback");
		CActionGame::PerformPlaneBreak(GeneratePhysicsEvent(m_bes[0]), &m_bes[0]);
	}
	else
		GameWarning("[brk] No physical entity to deform");
}

bool CPlaneBreak::GotExplosiveObjectState(const SExplosiveObjectState* pState)
{
	return false;
}

bool CPlaneBreak::AllowComplete(const SProceduralBreakRecordingState& state)
{
	LOGBREAK("CPlaneBreak AllowComplete %d", state.numEmptySteps > 3);
	return state.numEmptySteps > 3;
}

const EventPhysCollision& CPlaneBreak::GeneratePhysicsEvent(const SBreakEvent& bev)
{
	static EventPhysCollision epc;
	epc.pEntity[0] = 0;
	epc.iForeignData[0] = -1;
	epc.pEntity[1] = 0;
	epc.iForeignData[1] = -1;
	epc.pt = bev.pt;
	epc.n = bev.n;
	epc.vloc[0] = bev.vloc[0];
	epc.vloc[1] = bev.vloc[1];
	epc.mass[0] = bev.mass[0];
	epc.mass[1] = bev.mass[1];
	epc.idmat[0] = bev.idmat[0];
	epc.idmat[1] = bev.idmat[1];
	epc.partid[0] = bev.partid[0];
	epc.partid[1] = bev.partid[1];
	epc.penetration = bev.penetration;
	epc.radius = bev.radius;
	if (m_pPhysEnt)
	{
		epc.pEntity[1] = m_pPhysEnt;
		epc.iForeignData[1] = m_pPhysEnt->GetiForeignData();
		epc.pForeignData[1] = m_pPhysEnt->GetForeignData(epc.iForeignData[1]);
	}
	return epc;
}

#endif // !NET_USE_SIMPLE_BREAKAGE

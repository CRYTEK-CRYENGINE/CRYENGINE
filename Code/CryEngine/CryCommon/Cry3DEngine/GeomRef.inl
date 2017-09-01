// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if !defined(_LIB) || defined(_LAUNCHER)

	#include <CryAnimation/ICryAnimation.h>
	#include <CryMath/GeomQuery.h>

//////////////////////////////////////////////////////////////////////////
// GeomRef implementation

void GeomRef::AddRef() const
{
	if (m_pMeshObj)
		m_pMeshObj->AddRef();
	if (m_pPhysEnt)
		m_pPhysEnt->AddRef();
}

void GeomRef::Release() const
{
	if (m_pMeshObj)
		m_pMeshObj->Release();
	if (m_pPhysEnt)
		m_pPhysEnt->Release();
}

int GeomRef::Set(IEntity* pEntity, int iSlot)
{
	if (!pEntity)
	{
		m_pMeshObj = nullptr;
		m_pPhysEnt = nullptr;
		return -1;
	}

	m_pMeshObj = nullptr;
	int iStart = iSlot < 0 ? 0 : iSlot;
	int iEnd = iSlot < 0 ? pEntity->GetSlotCount() : iSlot + 1;
	iSlot = -1;
	for (int i = iStart; i < iEnd; ++i)
	{
		SEntitySlotInfo slotInfo;
		if (pEntity->GetSlotInfo(i, slotInfo))
		{
			(m_pMeshObj = slotInfo.pCharacter) || (m_pMeshObj = slotInfo.pStatObj);
			if (m_pMeshObj)
			{
				iSlot = i;
				break;
			}
		}
	}

	m_pPhysEnt = pEntity->GetPhysics();
	if (auto pAreaComp = static_cast<IEntityAreaComponent*>(pEntity->GetProxy(ENTITY_PROXY_AREA)))
	{
		m_pArea = pAreaComp->GetArea();
	}

	return iSlot;
}

IPhysicalEntity* GeomRef::GetPhysicalEntity() const
{
	if (m_pPhysEnt)
		return m_pPhysEnt;
	if (m_pMeshObj)
		return m_pMeshObj->GetPhysEntity();
	return 0;
}

IGeometry* GeomRef::GetGeometry() const
{
	if (m_pMeshObj)
	{
		if (phys_geometry* pPhys = m_pMeshObj->GetPhysGeom())
			return pPhys->pGeom;
	}
	return 0;
}

void GeomRef::GetAABB(AABB& bb, EGeomType eAttachType, QuatTS const& tLoc, bool bCentered) const
{
	bb.Reset();
	if (eAttachType == GeomType_Physics)
	{
		if (IPhysicalEntity* pPhysEnt = GetPhysicalEntity())
		{
			pe_status_pos pos;
			if (pPhysEnt->GetStatus(&pos))
			{
				// Box is already axis-aligned, but not offset.
				bb = AABB(pos.pos + pos.BBox[0], pos.pos + pos.BBox[1]);
				return;
			}
		}
	}
	if (m_pMeshObj)
	{
		bb = m_pMeshObj->GetAABB();
		if (bCentered)
			bb.Move(-bb.GetCenter());
		bb.SetTransformedAABB(Matrix34(tLoc), bb);
	}
	else if (m_pArea)
		bb = m_pArea->GetAABB();
}

float GeomRef::GetExtent(EGeomType eAttachType, EGeomForm eForm) const
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (eAttachType == GeomType_Physics)
	{
		if (IPhysicalEntity* pPhysEnt = GetPhysicalEntity())
		{
			// status_extent query caches extent in geo.
			pe_status_extent se;
			se.eForm = eForm;
			pPhysEnt->GetStatus(&se);
			return se.extent;
		}
		if (IGeometry* pGeom = GetGeometry())
			return pGeom->GetExtent(eForm);
	}

	// Use render objects if requested, or if no physics objects
	if (m_pMeshObj)
		return m_pMeshObj->GetExtent(eForm);
	else if (m_pArea)
		return m_pArea->GetExtent(eForm);

	return 0.f;
}

void GeomRef::GetRandomPos(PosNorm& ran, CRndGen seed, EGeomType eAttachType, EGeomForm eForm, QuatTS const& tWorld, bool bCentered) const
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (eAttachType == GeomType_Physics)
	{
		if (IPhysicalEntity* pPhysEnt = GetPhysicalEntity())
		{
			pe_status_random sr;
			sr.eForm = eForm;
			sr.seed = seed;
			m_pPhysEnt->GetStatus(&sr);
			ran = sr.ran;
			return;
		}
		if (IGeometry* pGeom = GetGeometry())
		{
			pGeom->GetRandomPos(ran, seed, eForm);
			if (bCentered)
				ran.vPos -= pGeom->GetCenter();
			ran <<= tWorld;
			return;
		}
	}

	// Use render objects if requested, or if no physics objects
	if (m_pMeshObj)
	{
		m_pMeshObj->GetRandomPos(ran, seed, eForm);
		if (bCentered)
			ran.vPos -= m_pMeshObj->GetAABB().GetCenter();
		ran <<= tWorld;
		return;
	}
	else if (m_pArea)
		return m_pArea->GetRandomPos(ran, seed, eForm);

	ran.zero();
}

#endif

// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include <CryMath/Cry_Geo.h>

struct IMeshObj;
struct IPhysicalEntity;
struct IGeometry;

//////////////////////////////////////////////////////////////////////////
//! Reference to one visual or physical geometry, for particle attachment.

struct GeomRef
{
	IMeshObj*        m_pMeshObj;    //!< Render object attachment.
	IPhysicalEntity* m_pPhysEnt;    //!< Physics object attachment.

	GeomRef()
	{
		m_pMeshObj = 0;
		m_pPhysEnt = 0;
	}

	void Set(IMeshObj* pMesh)
	{
		m_pMeshObj = pMesh;
	}
	void Set(IPhysicalEntity* pPhys)
	{
		m_pPhysEnt = pPhys;
	}
	int  Set(IEntity* pEntity, int iSlot = -1);

	void AddRef() const;
	void Release() const;

	operator bool() const
	{
		return m_pMeshObj || m_pPhysEnt;
	}
	bool operator==(const GeomRef& other) const
	{
		return m_pMeshObj == other.m_pMeshObj && m_pPhysEnt == other.m_pPhysEnt;
	}
	bool operator!=(const GeomRef& other) const
	{
		return m_pMeshObj != other.m_pMeshObj || m_pPhysEnt != other.m_pPhysEnt;
	}

	IMeshObj* GetMesh() const
	{
		return m_pMeshObj;
	}
	IPhysicalEntity* GetPhysicalEntity() const;
	IGeometry*       GetGeometry() const;

	void             GetAABB(AABB& bb, EGeomType eAttachType, QuatTS const& tLoc, bool bCentered = false) const;
	float            GetExtent(EGeomType eAttachType, EGeomForm eForm) const;
	void             GetRandomPos(PosNorm& ran, CRndGen seed, EGeomType eAttachType, EGeomForm eForm, QuatTS const& tWorld, bool bCentered = false) const;
};

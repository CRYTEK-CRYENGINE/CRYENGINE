// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  helper for refering to either entities or statobj physical entities
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"
#include "ObjectSelector.h"
#include "DebugBreakage.h"

CObjectSelector::CObjectSelector()
	: m_selType(eST_Null)
	, m_eventPos(ZERO)
	, m_hash(0)
	, m_drawDistance(0.0f)
{
}

void CObjectSelector::GetPositionInfo(SMessagePositionInfo& pos)
{
	pos.havePosition = !m_eventPos.IsZero();
	pos.position = m_eventPos;
	pos.haveDrawDistance = m_drawDistance > 0;
	pos.drawDistance = m_drawDistance;
}

IPhysicalEntity* CObjectSelector::Find() const
{
	if (m_selType == eST_Entity)
	{
		if (IEntity* pEnt = gEnv->pEntitySystem->GetEntity(m_entity))
			return pEnt->GetPhysics();
	}
	else if (m_selType == eST_StatObj)
	{
		if (m_hash)
			return CObjectSelector::FindPhysicalEntity(m_eventPos, m_hash, CObjectSelector_Eps);
	}
	return NULL;
}

/* static */
string CObjectSelector::GetDescription(EntityId entId)
{
	return "<defunct>";
}

string CObjectSelector::GetDescription(IPhysicalEntity* pent)
{
	return "<defunct>";
}
//  m_drawDistance = 0;
//  m_hash = 0;
//  if (!pEnt)
//  {
//    m_selType = eST_Null;
//    m_eventPos = ZERO;
//  }
//  else if (pEnt->GetiForeignData() == PHYS_FOREIGN_ID_STATIC)
//  {
//    IRenderNode * pNode = (IRenderNode*) pEnt->GetForeignData(PHYS_FOREIGN_ID_STATIC);
//    IStatObj * pStatObj = pNode->GetEntityStatObj();
//    phys_geometry * pPhysGeom = pStatObj? pStatObj->GetPhysGeom() : 0;
//    m_eventPos = pNode->GetBBox().GetCenter();
//    m_selType = eST_StatObj;
//    m_drawDistance = pNode->GetMaxViewDist();
//    m_hash=CalculateHash(pNode);
//  }
//  else if (pEnt->GetiForeignData() == PHYS_FOREIGN_ID_ENTITY)
//  {
//    IEntity * pEntity = (IEntity*)pEnt->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
//    if (pEntity)
//    {
//      m_selType = eST_Entity;
//      m_entity = pEntity->GetId();
//      AABB aabb;
//      pEntity->GetWorldBounds(aabb);
//      m_eventPos = aabb.GetCenter();
//      FillDrawDistance(pEntity);
//    }
//    else
//    {
//      CRY_ASSERT(false);
//      m_selType = eST_Null;
//      m_eventPos = ZERO;
//    }
//  }
//  else
//  {
//    CRY_ASSERT(false);
//    m_selType = eST_Null;
//    m_eventPos = ZERO;
//  }
// switch (m_selType)
// {
// case eST_Null:
//  return "null-selector";
// case eST_Entity:
//  if (IEntity * pEnt = gEnv->pEntitySystem->GetEntity(m_entity))
//    return string().Format("entity[%.8x:%s]", m_entity, pEnt->GetName());
//  else
//    return string().Format("unbound-entity[%.8x]", m_entity);
// case eST_StatObj:
//
//  Matrix34A matrix;
//  pNode->GetEntityStatObj(0, 0, &matrix);
//  Vec3 p = matrix.GetColumn3();
//  return string().Format("statobj[ name=%s, ename=%s, pos=%f, %f, %f, hash=0x%x ]", name, ename, p.x, p.y, p.z, hash);

// default:
//  CRY_ASSERT(false);
//  return "invalid-selector";
// }

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
#ifndef __OBJECTSELECTOR_H__
#define __OBJECTSELECTOR_H__

#pragma once

#include "DebugBreakage.h"

static const float CObjectSelector_Eps = 1.0f;

class CObjectSelector
{
public:
	CObjectSelector();
	CObjectSelector(const Vec3& eventpos, uint32 hash = 0)
	{
		m_selType = eST_StatObj;
		m_eventPos = eventpos;
		m_drawDistance = 0;
		m_hash = hash;
	}
	CObjectSelector(EntityId ent)
	{
		if (ent)
		{
			m_selType = eST_Entity;
			m_entity = ent;
			m_drawDistance = 0;
		}
		else
		{
			m_selType = eST_Null;
			m_eventPos = ZERO;
		}
		m_hash = 0;
	}
	CObjectSelector(IEntity* pEnt)
	{
		m_drawDistance = 0;
		if (!pEnt)
		{
			m_selType = eST_Null;
			m_eventPos = ZERO;
		}
		else
		{
			m_selType = eST_Entity;
			m_entity = pEnt->GetId();
			FillDrawDistance(pEnt);
		}
	}
	void             GetPositionInfo(SMessagePositionInfo& pos);
	IPhysicalEntity* Find() const;

	static string    GetDescription(IPhysicalEntity* pent);
	static string    GetDescription(EntityId entId);

	bool             operator==(const CObjectSelector& sel) const
	{
		if (m_selType == eST_Entity)
		{
			return m_entity == sel.m_entity;
		}
		else if (m_selType == eST_StatObj)
		{
			return m_hash == sel.m_hash;
		}
		else
		{
			return false;
		}
	}

	static uint32 CalculateHash(const char* str, uint32 hashIn /*=0*/)
	{
		uint32 hash = hashIn;
		const uint8* b = (const uint8*)str;
		for (; *b; b++)
		{
			hash = hash ^ ((hash << 5) + (hash >> 2) + (*b));
		}
		return hash;
	}
	static uint32 CalculateHash(const int* buf, int len, uint32 hashIn /*=0*/)
	{
		uint32 hash = hashIn;
		for (int i = 0; i < len; i++, buf++)
		{
			hash = hash ^ ((hash << 5) + (hash >> 2) + (*buf));
		}
		return hash;
	}
	static uint32 CalculateHash(IRenderNode* pNode, bool debug = false)
	{
		(void)debug;
		if (pNode)
		{
			uint32 hash;
			const char* name = pNode->GetName();
			const char* ename = pNode->GetEntityClassName();
			Matrix34A matrix;
			pNode->GetEntityStatObj(0, &matrix);
			hash = CalculateHash(name, 0);
			hash = CalculateHash(ename, hash);
			hash = CalculateHash(alias_cast<int*>(&matrix), sizeof(Matrix34A) / sizeof(int), hash);
#if DEBUG_NET_BREAKAGE
			if (debug)
			{
				Vec3 p = matrix.GetColumn3();
				LOGBREAK("name=%s, ename=%s, pos=%f, %f, %f, hash=0x%x", name, ename, p.x, p.y, p.z, hash);
			}
#endif
			return hash;
		}
		return 0;
	}

	static IPhysicalEntity* FindPhysicalEntity(const Vec3& centre, uint32 objHash, float eps)
	{
		IPhysicalEntity** pents;
		IRenderNode* pRenderNode;
		const Vec3 epsV = Vec3(eps);
		int j = gEnv->pPhysicalWorld->GetEntitiesInBox(centre - epsV, centre + epsV, pents, ent_static);
		for (--j; j >= 0; j--)
		{
			if (pRenderNode = (IRenderNode*)pents[j]->GetForeignData(PHYS_FOREIGN_ID_STATIC))
			{
				if (objHash == CObjectSelector::CalculateHash(pRenderNode, DEBUG_NET_BREAKAGE))
				{
					IPhysicalEntity* pent = pRenderNode->GetPhysics();
					return pent;
				}
			}
		}
		return NULL;
	}

private:
	enum ESelType
	{
		eST_Null, // must be first
		eST_Entity,
		eST_StatObj,
		eST_NUM_TYPES // must be last
	};
	ESelType m_selType;

	EntityId m_entity;
	float    m_drawDistance;

	// Static Objects
	Vec3   m_eventPos;
	uint32 m_hash;

	void FillDrawDistance(IEntity* pEnt)
	{
		if (IEntityRender* pRP = pEnt->GetRenderInterface())
			if (IRenderNode* pRN = pRP->GetRenderNode())
				m_drawDistance = pRN->GetMaxViewDist();
	}
};

#endif

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RulerPathAgent.h"
#include "Ruler.h"
#include "RulerPoint.h"
#include <CryAISystem/IAIObjectManager.h>
#include <CryAISystem/IAIActor.h>
//////////////////////////////////////////////////////////////////////////
CRulerPathAgent::CRulerPathAgent()
	: m_bPathQueued(false)
	, m_bLastPathSuccess(false)
	, m_fLastPathDist(0.0f)
	, m_vStartPoint(ZERO)
	, m_agentType(0)
{
	// TODO: m_AgentMovementAbility need new values?
}

//////////////////////////////////////////////////////////////////////////
CRulerPathAgent::~CRulerPathAgent()
{

}

//////////////////////////////////////////////////////////////////////////
void CRulerPathAgent::Render(IRenderer* pRenderer, const ColorF& colour)
{
	// Draw path
	if (!m_bPathQueued && m_bLastPathSuccess)
	{
		if (!m_path.empty())
		{
			IRenderAuxGeom* pAuxGeom = pRenderer->GetIRenderAuxGeom();
			CRY_ASSERT(pAuxGeom);

			PATHPOINTVECTOR::const_iterator li, linext;
			li = m_path.begin();
			linext = li;
			++linext;
			while (linext != m_path.end())
			{
				Vec3 p0 = li->vPos;
				Vec3 p1 = linext->vPos;

				// bump z up a bit for each subsequent path
				p0.z += 0.3f * m_agentType;
				p1.z += 0.3f * m_agentType;

				pAuxGeom->DrawLine(p0, colour, p1, colour);
				pAuxGeom->DrawSphere(p1, 0.05f, colour);
				li = linext;
				++linext;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRulerPathAgent::SetType(uint32 type, const string& name)
{
	CRY_ASSERT(type < 255); // currently 8 bits available for agent type
	m_agentType = type;
	m_agentName = name;
	m_AgentMovementAbility.pathfindingProperties.navCapMask.SetLnmCaps(type);
}

//////////////////////////////////////////////////////////////////////////
bool CRulerPathAgent::RequestPath(const CRulerPoint& startPoint, const CRulerPoint& endPoint)
{
	bool bResult = false;

	if (startPoint.IsEmpty() || endPoint.IsEmpty())
	{
		CRY_ASSERT_MESSAGE(false, "CRuler::RequestPath but the path points aren't set!");
		return false;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CRulerPathAgent::ClearLastRequest()
{
	m_bLastPathSuccess = false;
	m_fLastPathDist = 0.0f;
	m_path.clear();
	m_bPathQueued = false;
}

//////////////////////////////////////////////////////////////////////////
IEntity* CRulerPathAgent::GetPathAgentEntity() const
{
	// For smart objects to work in the pathfinder, they require an entity to compare against the potential
	//	smart object user classes. For now, try to find an entity of the right agent type, and use that
	//	as a dummy. This appears to be safe, since CAISystem::GetSmartObjectLinkCostFactor() doesn't modify the entity.
	//	Also, the ruler won't be active during gameplay.

	// Potential improvement: create a temporary entity and use that instead?

	IAIObjectManager* pAIObjMgr = gEnv->pAISystem->GetAIObjectManager();
	AutoAIObjectIter it = pAIObjMgr->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_ACTOR);
	for (; it->GetObject(); it->Next())
	{
		IAIActor* pAIActor = it->GetObject()->CastToIAIActor();
		if (pAIActor && pAIActor->GetMovementAbility().pathfindingProperties.navCapMask.GetLnmCaps() == m_agentType)
		{
			return it->GetObject()->GetEntity();
		}
	}

	// Failed to find a suitable entity
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
const char* CRulerPathAgent::GetPathAgentName() const
{
	return "EditorRulerPathAgent";
}

//////////////////////////////////////////////////////////////////////////
unsigned short CRulerPathAgent::GetPathAgentType() const
{
	return AIOBJECT_DUMMY; // TODO Parameterize?
}

//////////////////////////////////////////////////////////////////////////
float CRulerPathAgent::GetPathAgentPassRadius() const
{
	return 0.3f; // TODO Parameterize?
}

//////////////////////////////////////////////////////////////////////////
Vec3 CRulerPathAgent::GetPathAgentPos() const
{
	return m_vStartPoint;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CRulerPathAgent::GetPathAgentVelocity() const
{
	return Vec3Constants<float>::fVec3_Zero; // TODO ??
}

//////////////////////////////////////////////////////////////////////////
const AgentMovementAbility& CRulerPathAgent::GetPathAgentMovementAbility() const
{
	return m_AgentMovementAbility;
}


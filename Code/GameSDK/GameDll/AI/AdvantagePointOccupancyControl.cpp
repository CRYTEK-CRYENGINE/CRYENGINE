// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AdvantagePointOccupancyControl.h"

#if defined(CRYAISYSTEM_DEBUG)
#include <CryRenderer/IRenderAuxGeom.h>
#endif


#if defined(CRYAISYSTEM_DEBUG)

int CAdvantagePointOccupancyControl::ai_AdvantagePointOccupancyDebug = 0;

#endif


CAdvantagePointOccupancyControl::CAdvantagePointOccupancyControl()
{
#if defined(CRYAISYSTEM_DEBUG)
	REGISTER_CVAR(ai_AdvantagePointOccupancyDebug, 0, 0, "Debug advantage point occupancy control manager: 0 = disabled, 1 = enabled. Yellow = reserved for 1 entity, orange = reserved for multiple entities.");
#endif
}


CAdvantagePointOccupancyControl::~CAdvantagePointOccupancyControl()
{
#if defined(CRYAISYSTEM_DEBUG)
	if (gEnv->pConsole)
	{
		gEnv->pConsole->UnregisterVariable("ai_AdvantagePointOccupancyDebug", true);
	}
#endif
}


void CAdvantagePointOccupancyControl::Reset()
{
	m_occupiedAdvantagePoints.clear();
}

void CAdvantagePointOccupancyControl::OccupyAdvantagePoint( EntityId entityId, const Vec3& position )
{
	std::pair<OccupiedAdvantagePoints::iterator, bool> ret = m_occupiedAdvantagePoints.insert(std::make_pair(entityId, position));
	if(ret.second == false)
	{
		ret.first->second = position;
	}
}

void CAdvantagePointOccupancyControl::ReleaseAdvantagePoint( EntityId entityId )
{
	m_occupiedAdvantagePoints.erase(entityId);
}

bool CAdvantagePointOccupancyControl::IsAdvantagePointOccupied( const Vec3& position ) const
{
	OccupiedAdvantagePoints::const_iterator it = m_occupiedAdvantagePoints.begin();
	OccupiedAdvantagePoints::const_iterator itEnd = m_occupiedAdvantagePoints.end();

	while(it != itEnd)
	{
		if(MatchAdvantagePointPosition(position, (*it).second))
			return true;
		else
			++it;
	}
	return false;
}

bool CAdvantagePointOccupancyControl::IsAdvantagePointOccupied(const Vec3& position, const EntityId ignoreEntityId) const
{
	OccupiedAdvantagePoints::const_iterator it = m_occupiedAdvantagePoints.begin();
	OccupiedAdvantagePoints::const_iterator itEnd = m_occupiedAdvantagePoints.end();

	while(it != itEnd)
	{
		if ( ((*it).first != ignoreEntityId) && MatchAdvantagePointPosition(position, (*it).second))
			return true;
		else
			++it;
	}
	return false;
}


void CAdvantagePointOccupancyControl::Update()
{
#if defined(CRYAISYSTEM_DEBUG)
	if (ai_AdvantagePointOccupancyDebug != 0)
	{
		DebugDraw();
	}
#endif
}


bool CAdvantagePointOccupancyControl::MatchAdvantagePointPosition( const Vec3& position, const Vec3& AdvantagePoint ) const
{
	return ( (AdvantagePoint - position).len2() < 0.25f * 0.25f );
}


#if defined(CRYAISYSTEM_DEBUG)

void CAdvantagePointOccupancyControl::DebugDraw() const
{
	IRenderAuxGeom* debugRenderer = gEnv->pRenderer->GetIRenderAuxGeom();
	IF_UNLIKELY (debugRenderer == NULL)
	{
		return;
	}

	OccupiedAdvantagePoints::const_iterator iter;
	OccupiedAdvantagePoints::const_iterator endIter = m_occupiedAdvantagePoints.end();
	for (iter = m_occupiedAdvantagePoints.begin() ; iter != endIter ; ++iter)
	{
		DebugDrawAdvantagePoint(debugRenderer, iter->first, iter->second);
	}
}


void CAdvantagePointOccupancyControl::DebugDrawAdvantagePoint(IRenderAuxGeom* debugRenderer, const EntityId entityID, const Vec3& occupiedPos) const
{
	static const ColorB singleOccupationDrawColor = Col_Yellow;
	static const ColorB multipleOccupationsDrawColor = Col_Orange;
	
	ColorB drawColor;
	if (IsAdvantagePointOccupied(occupiedPos, entityID))
	{
		drawColor = multipleOccupationsDrawColor;
	}
	else
	{
		drawColor = singleOccupationDrawColor;
	}

	debugRenderer->DrawSphere(occupiedPos, 0.6f, drawColor);

	const IEntity* entity = gEnv->pEntitySystem->GetEntity(entityID);
	if (entity != NULL)
	{
		const Vec3& entityPos = entity->GetWorldPos();
		const Vec3 tempPos = entityPos + Vec3(0.0f, 0.0f, 2.0f);
		debugRenderer->DrawLine(entityPos, drawColor, tempPos, drawColor);
		debugRenderer->DrawLine(tempPos, drawColor, occupiedPos, drawColor);
	}
}


#endif
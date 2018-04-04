// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/********************************************************************
   -------------------------------------------------------------------------
   File name:   HideSpot.cpp
   $Id$
   Description: Hidespot-related structures

   -------------------------------------------------------------------------
   History:

 *********************************************************************/

#include "StdAfx.h"
#include "HideSpot.h"

SHideSpot::SHideSpot() : pNavNode(0), pNavNodes(0), pAnchorObject(0), pObstacle(0), entityId(0)
{
}

SHideSpot::SHideSpot(SHideSpotInfo::EHideSpotType type, const Vec3& pos, const Vec3& dir)
	: info(type, pos, dir), pNavNode(0), pNavNodes(0), pAnchorObject(0), pObstacle(0), entityId(0)
{
}

bool SHideSpot::IsSecondary() const
{
	switch (info.type)
	{
	case SHideSpotInfo::eHST_TRIANGULAR:
		return pObstacle && !pObstacle->IsCollidable();
	case SHideSpotInfo::eHST_WAYPOINT:
		return pNavNode && (pNavNode->navType == IAISystem::NAV_WAYPOINT_HUMAN) &&
		       (pNavNode->GetWaypointNavData()->type == WNT_HIDESECONDARY);
	case SHideSpotInfo::eHST_ANCHOR:
		return pAnchorObject && (pAnchorObject->GetType() == AIANCHOR_COMBAT_HIDESPOT_SECONDARY);
	}

	return false;
}

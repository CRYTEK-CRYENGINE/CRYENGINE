// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/********************************************************************
   -------------------------------------------------------------------------
   File name:   HideSpot.h
   $Id$
   Description: Hidespot-related structures

   -------------------------------------------------------------------------
   History:

 *********************************************************************/

#ifndef _HIDESPOT_H_
#define _HIDESPOT_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include "SmartObjects.h"

// Description:
//	 Structure that contains critical hidespot information.
struct SHideSpotInfo
{
	enum EHideSpotType
	{
		eHST_TRIANGULAR,
		eHST_WAYPOINT,
		eHST_ANCHOR,
		eHST_SMARTOBJECT,
		eHST_VOLUME,
		eHST_DYNAMIC,
		eHST_INVALID,
	};

	EHideSpotType type;
	Vec3          pos;
	Vec3          dir;

	SHideSpotInfo() : type(eHST_INVALID), pos(ZERO), dir(ZERO) {}
	SHideSpotInfo(EHideSpotType type, const Vec3& pos, const Vec3& dir) : type(type), pos(pos), dir(dir) {}
};

struct SHideSpot
{
	SHideSpot();
	SHideSpot(SHideSpotInfo::EHideSpotType type, const Vec3& pos, const Vec3& dir);

	bool IsSecondary() const;

	//////////////////////////////////////////////////////////////////////////

	SHideSpotInfo info;

	// optional parameters - can be used with multiple hide spot types
	const GraphNode*                     pNavNode;
	const std::vector<const GraphNode*>* pNavNodes;
	EntityId                             entityId; // The entity id of the source object for dynamic hidepoints.

	// parameters used only with one specific hide spot type
	const ObstacleData* pObstacle;      // triangular
	CQueryEvent         SOQueryEvent;   // smart objects
	const CAIObject*    pAnchorObject;  // anchors
};

typedef std::multimap<float, SHideSpot> MultimapRangeHideSpots;

#endif  // #ifndef _HIDESPOT_H_

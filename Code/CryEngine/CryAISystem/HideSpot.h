// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

enum EObstacleFlags
{
	OBSTACLE_COLLIDABLE = BIT(0),
	OBSTACLE_HIDEABLE = BIT(1),
};

struct ObstacleData
{
	Vec3 vPos;
	Vec3 vDir;
	/// this radius is approximate - it is estimated during link generation. if -ve it means
	/// that it shouldn't be used (i.e. object is significantly non-circular)
	float       fApproxRadius;
	uint8       flags;
	uint8       approxHeight; // height in 4.4 fixed point format.

	inline void SetCollidable(bool state) { state ? (flags |= OBSTACLE_COLLIDABLE) : (flags &= ~OBSTACLE_COLLIDABLE); }
	inline void SetHideable(bool state) { state ? (flags |= OBSTACLE_HIDEABLE) : (flags &= ~OBSTACLE_HIDEABLE); }
	inline bool IsCollidable() const { return (flags & OBSTACLE_COLLIDABLE) != 0; }
	inline bool IsHideable() const { return (flags & OBSTACLE_HIDEABLE) != 0; }

	/// Sets the approximate height and does the necessary conversion.
	inline void  SetApproxHeight(float h) { approxHeight = (uint8)(clamp_tpl(h * (1 << 4), 0.0f, 255.0f)); }
	/// Returns the approximate height and does the necessary conversion.
	inline float GetApproxHeight() const { return (float)approxHeight / (float)(1 << 4); }
	/// Gets the navigation node that this obstacle is attached to (and caches the result). The result should be
	/// safe in game, but might be invalid in editor if the navigation gets modified whilst AI/physics is running
	/// so I advise against dereferencing the result (unless you validate it first).
	/// The calculation assumes that this obstacle is only attached to triangular nodes
	const std::vector<const GraphNode*>& GetNavNodes() const;
	void ClearNavNodes() { navNodes.clear(); }
	// mechanisms for setting the nav nodes on a temporary obstacle object
	void SetNavNodes(const std::vector<const GraphNode*>& nodes);
	void AddNavNode(const GraphNode* pNode);

	/// Note - this is used during triangulation where we can't have two objects
	/// sitting on top of each other - even if they have different directions etc
	bool operator==(const ObstacleData& other) const { return ((fabs(vPos.x - other.vPos.x) < 0.001f) && (fabs(vPos.y - other.vPos.y) < 0.001f)); }

	ObstacleData(const Vec3& pos = Vec3(0, 0, 0), const Vec3& dir = Vec3(0, 0, 0))
		: vPos(pos)
		, vDir(dir)
		, fApproxRadius(-1.0f)
		, flags(OBSTACLE_COLLIDABLE)
		, approxHeight(0)
		, needToEvaluateNavNodes(false)
	{}

	void Serialize(TSerialize ser);

private:
	// pointer to the triangular navigation nodes that contains us (result of GetEnclosing).
	mutable std::vector<const GraphNode*> navNodes;
	mutable bool                          needToEvaluateNavNodes;
};

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
	EntityId                             entityId; // The entity id of the source object for dynamic hidepoints.

	// parameters used only with one specific hide spot type
	const ObstacleData* pObstacle;      // triangular
	CQueryEvent         SOQueryEvent;   // smart objects
	const CAIObject*    pAnchorObject;  // anchors
};

typedef std::multimap<float, SHideSpot> MultimapRangeHideSpots;

#endif  // #ifndef _HIDESPOT_H_

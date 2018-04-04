// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef NAVREGION_H
#define NAVREGION_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Graph.h"
#include "NavPath.h"
#include <CryNetwork/SerializeFwd.h>

struct AgentMovementAbility;

/// Enforce a standard interface on the different types of navigation region
class CNavRegion
{
public:
	CNavRegion() {}
	virtual ~CNavRegion() {}

	/// Beautify the path passed in, returning the result in outPath. This code
	/// should assume that this is the whole path (at some point paths will transition
	/// between regions, but then beautification will be hidden by the caller). start and end
	/// directions should just be taken as hints.
	virtual void BeautifyPath(
	  const VectorConstNodeIndices& inPath, TPathPoints& outPath,
	  const Vec3& startPos, const Vec3& startDir,
	  const Vec3& endPos, const Vec3& endDir,
	  float radius,
	  const AgentMovementAbility& movementAbility,
	  const NavigationBlockers& navigationBlockers) = 0;

	/// As BeautifyPath, but this should return the "raw", unbeautified path
	virtual void UglifyPath(const VectorConstNodeIndices& inPath, TPathPoints& outPath,
	                        const Vec3& startPos, const Vec3& startDir,
	                        const Vec3& endPos, const Vec3& endDir) = 0;

	/// Should return the graph node containing pos, using pStart as a hint as to
	/// where to start looking. If no node can be found then do an approximate search
	/// by extending the internal area/volume representation by range, and return the
	/// closest position that is valid for the returned graph node. If returnSuspect
	/// then a node may be returned even if it may not be reachable.
	/// If there are a number of nodes that could be returned, only nodes that have at least one
	/// link with radius > passRadius will be returned. Range determines the search range for
	/// the enclosing node: (range < 0) indicates a default search radius will be used
	/// determined by the navigation type. (range >= 0) will be taken into account
	/// depending on the navigation type.
	virtual unsigned GetEnclosing(const Vec3& pos, float passRadius = 0.0f, unsigned startIndex = 0,
	                              float range = -1.0f, Vec3* closestValid = 0, bool returnSuspect = false, const char* requesterName = "", bool omitWalkabilityTest = false) = 0;

	/// Should restore its original state
	virtual void Clear() = 0;

	/// Gets called on enter/exit etc from game mode - i.e. opportunity to clear/reset any dynamic updating
	/// data.
	virtual void Reset(IAISystem::EResetReason reason) {};

	/// Serialise the _modifications_ since load-time
	virtual void Serialize(TSerialize ser) = 0;

	/// Attempt a straight path up to a maximum cost of maxCost between start and end,
	/// putting the result in pathOut. The estimated cost of the path is also returned,
	/// the estimate being made using the heuristic. Returning < 0 or an empty pathOut
	/// means no path could be found
	virtual float AttemptStraightPath(TPathPoints& pathOut,
	                                  CAStarSolver* m_pAStarSolver,
	                                  const CHeuristic* pHeuristic,
	                                  const Vec3& start, const Vec3& end,
	                                  unsigned startHintIndex,
	                                  float maxCost,
	                                  const NavigationBlockers& navigationBlockers,
	                                  bool returnPartialPath)
	{
		return -1.0f;
	}

	/// Indicate if it's possible to go in a straight line from from to to according to the properties of the nav region.
	/// The navigation blockers should be considered impassable.
	virtual bool CheckPassability(const Vec3& from, const Vec3& to, float radius, const NavigationBlockers& navigationBlockers, IAISystem::tNavCapMask) const
	{
		return true;
	}

	/// Populates points with the path points needed to go from startPos to endPos, both of which fall within pNode and returns
	/// true if possible. Default implementation uses CheckPassability
	virtual bool GetSingleNodePath(const GraphNode* pNode, const Vec3& startPos, const Vec3& endPos, float radius,
	                               const NavigationBlockers& navigationBlockers, std::vector<PathPointDescriptor>& points,
	                               IAISystem::tNavCapMask navCaps) const
	{
		points.push_back(PathPointDescriptor(pNode->navType, startPos));
		if (!CheckPassability(startPos, endPos, radius, navigationBlockers, navCaps))
			points.push_back(PathPointDescriptor(pNode->navType, pNode->GetPos()));
		points.push_back(PathPointDescriptor(pNode->navType, endPos));
		return true;
	}

	/// Should return a position "near" to curPos that is suitable as a location to teleport
	/// from curPos to.
	/// if successful returns true and teleportPos, if not returns false and
	/// teleportPos is set to ZERO
	/// requesterName is just for debug
	virtual bool GetTeleportPosition(const Vec3& curPos, Vec3& teleportPos, const char* requesterName) { return false; }

	/// Allow for some periodic updating
	virtual void Update(class CCalculationStopper& stopper) {}

	/// Gets called when a node has been created (i.e. the region may choose to connect
	/// the node somewhere automatically)
	virtual void NodeCreated(unsigned nodeIndex) {}

	/// Called just after a node has been moved
	virtual void NodeMoved(unsigned nodeIndex) {}

	/// This gets called just _before_ the node is going to be deleted. Don't delete/destroy it
	/// during this call!!!
	virtual void NodeAboutToBeDeleted(GraphNode* pNode) {}

	/// Will get called when the mission has finished loaded (so all nav regions etc are created)
	virtual void OnMissionLoaded() {};

	/// Should return the memory usage in bytes
	virtual size_t MemStats() = 0;
};

#endif

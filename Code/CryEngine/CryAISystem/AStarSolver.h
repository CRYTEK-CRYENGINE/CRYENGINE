// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef ASTARSOLVER_H
#define ASTARSOLVER_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryAISystem/IAgent.h>
#include "AStarOpenList.h"
#include <CryNetwork/SerializeFwd.h>
#include "AILog.h"
#include <CryAISystem/IPathfinder.h> // For NavigationBlockers

class CGraph;

enum EAStarResult
{
	ASTAR_NOPATH,
	ASTAR_STILLSOLVING,
	ASTAR_PATHFOUND,
};

class CHeuristic
{
public:
	CHeuristic() {};
	virtual ~CHeuristic() {};

	/// Estimate the cost from one node to another - this estimate must be a minimum
	/// bound on the cost in order for AStar to work correctly (though it may run faster
	/// if the estimate more a best-guess than a lower-bound)
	virtual float EstimateCost(const AStarSearchNode& node0, const AStarSearchNode& node1) const = 0;

	/// Calculate the cost from one node to another via a specific link. This calculation
	/// should include anything specific like terrain costs, stealth vs speed etc
	/// A return value < 0 means that this link is absolutely impassable.
	virtual float CalculateCost(const CGraph* graph, const AStarSearchNode& node0, unsigned linkIndex0,
	                            const AStarSearchNode& node1, const NavigationBlockers& navigationBlockers) const = 0;

	/// Can be overridden to bypass expensive calculations in CalculateCost
	virtual bool IsLinkPassable(const CGraph* graph,
	                            const AStarSearchNode& node0, unsigned linkIndex0,
	                            const AStarSearchNode& node1,
	                            const NavigationBlockers& navigationBlockers) const
	{
		return CalculateCost(graph, node0, linkIndex0, node1, navigationBlockers) >= 0.0f;
	}

	virtual const struct PathfindingHeuristicProperties* GetProperties() const { return 0; }

};

class CCalculationStopper;

//====================================================================
// CAStarSolver
// Implements A*, supporting splitting the solving over multiple calls.
//====================================================================
class CAStarSolver
{
public:
	CAStarSolver(CGraphNodeManager& nodeManager);
	~CAStarSolver();

	/// Setup the path search. A warning will be generated if a path is already in progress - if so
	/// that path will be aborted and the new one started.
	/// Also pass in a collection of navigation blockers - this collection will be copied (to make
	/// sure it doesn't change as the search proceeds)
	EAStarResult SetupAStar(
	  CCalculationStopper& stopper,
	  const CGraph* pGraph,
	  const CHeuristic* pHeuristic,
	  unsigned startIndex, unsigned endIndex,
	  const NavigationBlockers& navigationBlockers,
	  bool bDebugDrawOpenList);
	/// Continue the path search - all pointers passed in to SetupAStar must still be valid
	EAStarResult ContinueAStar(CCalculationStopper& stopper);

	/// Cancel any existing path request
	void AbortAStar();

	/// returns the current heuristic
	ILINE const CHeuristic* GetHeuristic() const
	{
		return m_request.m_pHeuristic;
	}

	typedef std::vector<unsigned> tPathNodes;

	/// Returns the path - only valid once pathfinding has returned ASTAR_PATHFOUND, and before
	/// the next path is requested
	ILINE const tPathNodes& GetPathNodes()
	{
		return m_pathNodes;
	}

	/// If the last result was ASTAR_NOPATH then this calculates and returns a partial path that ends
	/// up as close as possible (according to the heuristic) to the requested destination
	const tPathNodes& CalculateAndGetPartialPath();

	/// returns the memory usage in bytes
	size_t                 MemStats();

	ILINE AStarSearchNode* GetAStarNode(unsigned index)
	{
		return m_AStarNodeManager.GetAStarNode(index);
	}

	ILINE const AStarSearchNodeVector& GetTaggedNodesVector()
	{
		return m_AStarNodeManager.GetTaggedNodesVector();
	}

private:
	/// If continue/start found a path then this will actually ratify it - again all pointers
	/// must still be valid
	EAStarResult WalkBack(const CCalculationStopper& stopper);

	void         ASTARStep(const AStarSearchNode* pEnd);

	/// Everything we need to know to continue/recreate this request
	struct CRequest
	{
		// Cached state - i.e. state set up when a path is requested and maintained across
		// subsequent calls to ContinueAStar

		/// Pointer to the graph currently being used. If 0 means we're not currently solving.
		const CGraph*     m_pGraph;
		/// A* heuristic
		const CHeuristic* m_pHeuristic;
		/// Pointer to the start graph node
		unsigned          m_startIndex;

		/// Pointer to the end graph node
		unsigned m_endIndex;
	};

	/// Our request
	CRequest m_request;

	/// Final path we store - can be picked up using GetPathNodes()
	tPathNodes m_pathNodes;

	/// current node we're working on
	AStarSearchNode* m_pathfinderCurrent;

	/// Dynamic Node management including: The open list (nodes that still need investigating). Rather than using a closed
	/// list, "closed" nodes will be tagged but not on the open list
	CAStarNodeListManager m_AStarNodeManager;

	/// Searching for the path, or walking back to identify it after it's found?
	bool m_bWalkingBack;

	/// Debug draw open list
	bool m_bDebugDrawOpenList;

	/// collection of blockers used to dynamically modify the links
	NavigationBlockers m_navigationBlockers;

	static int         s_instances;

	// Prevent copying which will break the instance count
	CAStarSolver(const CAStarSolver&);
	CAStarSolver& operator=(const CAStarSolver&);
};

#endif

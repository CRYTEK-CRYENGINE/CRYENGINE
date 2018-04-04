// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#if !defined(__CUSTOMNAVREGION_H__)
#define __CUSTOMNAVREGION_H__

#include "NavRegion.h"

class CGraph;

/// Handles all graph operations that relate to the smart objects aspect
class CCustomNavRegion : public CNavRegion
{
	CGraph* m_pGraph;

public:
	CCustomNavRegion(CGraph* pGraph) : m_pGraph(pGraph)
	{}

	virtual ~CCustomNavRegion() {}

	/// inherited
	virtual void BeautifyPath(
	  const VectorConstNodeIndices& inPath, TPathPoints& outPath,
	  const Vec3& startPos, const Vec3& startDir,
	  const Vec3& endPos, const Vec3& endDir,
	  float radius,
	  const AgentMovementAbility& movementAbility,
	  const NavigationBlockers& navigationBlockers)
	{
		UglifyPath(inPath, outPath, startPos, startDir, endPos, endDir);
	}

	/// inherited
	virtual void UglifyPath(
	  const VectorConstNodeIndices& inPath, TPathPoints& outPath,
	  const Vec3& startPos, const Vec3& startDir,
	  const Vec3& endPos, const Vec3& endDir);
	/// inherited
	virtual unsigned GetEnclosing(const Vec3& pos, float passRadius = 0.0f, unsigned startIndex = 0,
	                              float range = -1.0f, Vec3* closestValid = 0, bool returnSuspect = false, const char* requesterName = "", bool omitWalkabilityTest = false)
	{
		return 0;
	}

	/// Serialise the _modifications_ since load-time
	virtual void Serialize(TSerialize ser)
	{}

	/// inherited
	virtual void Clear();

	/// inherited
	virtual size_t MemStats();

	float          GetCustomLinkCostFactor(const GraphNode* nodes[2], const PathfindingHeuristicProperties& pathFindProperties) const;

	uint32         CreateCustomNode(const Vec3& pos, void* customData, uint16 customId, SCustomNavData::CostFunction pCostFunction, bool linkWithEnclosing = true);
	void           RemoveNode(uint32 nodeIndex);
	void           LinkCustomNodes(uint32 node1Index, uint32 node2Index, float radius1To2, float radius2To1);
};

#endif // #if !defined(__CUSTOMNAVREGION_H__)

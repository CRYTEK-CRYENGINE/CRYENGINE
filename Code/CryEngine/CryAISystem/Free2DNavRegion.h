// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef FREE2DNAVREGION_H
#define FREE2DNAVREGION_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include "NavRegion.h"

class CFree2DNavRegion : public CNavRegion
{
public:
	CFree2DNavRegion(CGraph* pGraph);
	virtual ~CFree2DNavRegion();

	/// inherited
	virtual void BeautifyPath(
	  const VectorConstNodeIndices& inPath, TPathPoints& outPath,
	  const Vec3& startPos, const Vec3& startDir,
	  const Vec3& endPos, const Vec3& endDir,
	  float radius,
	  const AgentMovementAbility& movementAbility,
	  const NavigationBlockers& navigationBlockers);

	/// inherited
	virtual void UglifyPath(const VectorConstNodeIndices& inPath, TPathPoints& outPath,
	                        const Vec3& startPos, const Vec3& startDir,
	                        const Vec3& endPos, const Vec3& endDir);

	/// inherited
	virtual unsigned GetEnclosing(const Vec3& pos, float passRadius = 0.0f, unsigned startIndex = 0,
	                              float range = -1.0f, Vec3* closestValid = 0, bool returnSuspect = false, const char* requesterName = "", bool omitWalkabilityTest = false);

	/// inherited
	virtual void Clear();

	/// inherited
	virtual void Serialize(TSerialize ser) {}

	/// inherited
	virtual bool CheckPassability(const Vec3& from, const Vec3& to, float radius, const NavigationBlockers& navigationBlockers, IAISystem::tNavCapMask) const;

	/// inherited
	virtual bool GetSingleNodePath(const GraphNode* pNode, const Vec3& startPos, const Vec3& endPos, float radius,
	                               const NavigationBlockers& navigationBlockers, std::vector<PathPointDescriptor>& points, IAISystem::tNavCapMask) const;

	/// inherited
	virtual size_t MemStats();

private:
	GraphNode* m_pDummyNode;
	unsigned   m_dummyNodeIndex;
};

#endif

// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "Free2DNavRegion.h"
#include "CAISystem.h"
#include "AILog.h"

//===================================================================
// CFree2DNavRegion
//===================================================================
CFree2DNavRegion::CFree2DNavRegion(CGraph* pGraph)
{
	m_pDummyNode = 0;
	m_dummyNodeIndex = 0;
}
//===================================================================
// ~CFree2DNavRegion
//===================================================================
CFree2DNavRegion::~CFree2DNavRegion()
{
}

//===================================================================
// BeautifyPath
//===================================================================
void CFree2DNavRegion::BeautifyPath(const VectorConstNodeIndices& inPath, TPathPoints& outPath,
                                    const Vec3& startPos, const Vec3& startDir,
                                    const Vec3& endPos, const Vec3& endDir,
                                    float radius,
                                    const AgentMovementAbility& movementAbility,
                                    const NavigationBlockers& navigationBlockers)
{
	AIWarning("CFree2DNavRegion::BeautifyPath should never be called");
}

//===================================================================
// UglifyPath
//===================================================================
void CFree2DNavRegion::UglifyPath(const VectorConstNodeIndices& inPath, TPathPoints& outPath,
                                  const Vec3& startPos, const Vec3& startDir,
                                  const Vec3& endPos, const Vec3& endDir)
{
	AIWarning("CFree2DNavRegion::UglifyPath should never be called");
}

//===================================================================
// GetEnclosing
//===================================================================
unsigned CFree2DNavRegion::GetEnclosing(const Vec3& pos, float passRadius, unsigned startIndex,
                                        float /*range*/, Vec3* closestValid, bool returnSuspect, const char* requesterName, bool omitWalkabilityTest)
{
	/*
	   const SpecialArea *sa = GetAISystem()->GetSpecialArea(pos, SpecialArea::TYPE_FREE_2D);
	   if (sa)
	    return 0;
	   int nBuildingID = sa->nBuildingID;
	   if (nBuildingID < 0)
	    return 0;
	 */
	if (!m_pDummyNode)
	{
		m_dummyNodeIndex = gAIEnv.pGraph->CreateNewNode(IAISystem::NAV_FREE_2D, ZERO);
		m_pDummyNode = gAIEnv.pGraph->GetNode(m_dummyNodeIndex);
	}

	return m_dummyNodeIndex;
}

//===================================================================
// Clear
//===================================================================
void CFree2DNavRegion::Clear()
{
	if (m_pDummyNode)
	{
		gAIEnv.pGraph->Disconnect(m_dummyNodeIndex);
		m_pDummyNode = 0;
		m_dummyNodeIndex = 0;
	}
}

//===================================================================
// MemStats
//===================================================================
size_t CFree2DNavRegion::MemStats()
{
	size_t size = sizeof(*this);
	return size;
}

//===================================================================
// CheckPassability
//===================================================================
bool CFree2DNavRegion::CheckPassability(const Vec3& from, const Vec3& to,
                                        float radius, const NavigationBlockers& navigationBlockers, IAISystem::tNavCapMask) const
{
	return false;
}

//===================================================================
// ExpandShape
//===================================================================
static void ShrinkShape(const ListPositions& shapeIn, ListPositions& shapeOut, float radius)
{
	// push the points in. The shape is guaranteed to be wound anti-clockwise
	shapeOut.clear();
	for (ListPositions::const_iterator it = shapeIn.begin(); it != shapeIn.end(); ++it)
	{
		ListPositions::const_iterator itNext = it;
		if (++itNext == shapeIn.end())
			itNext = shapeIn.begin();
		ListPositions::const_iterator itNextNext = itNext;
		if (++itNextNext == shapeIn.end())
			itNextNext = shapeIn.begin();

		Vec3 pos = *it;
		Vec3 posNext = *itNext;
		Vec3 posNextNext = *itNextNext;
		pos.z = posNext.z = posNextNext.z = 0.0f;

		Vec3 segDirPrev = (posNext - pos).GetNormalizedSafe();
		Vec3 segDirNext = (posNextNext - posNext).GetNormalizedSafe();

		Vec3 normalInPrev(-segDirPrev.y, segDirPrev.x, 0.0f);
		Vec3 normalInNext(-segDirNext.y, segDirNext.x, 0.0f);
		Vec3 normalAv = (normalInPrev + normalInNext).GetNormalizedSafe();

		Vec3 cross = segDirPrev.Cross(segDirNext);
		// convex means the point is convex from the point of view of inside the shape
		bool convex = cross.z < 0.0f;

		static float radiusScale = 0.5f;
		if (convex)
		{
			Vec3 newPtPrev = posNext + normalInPrev * (radius * radiusScale);
			newPtPrev.z = it->z;
			Vec3 newPtMid = posNext + normalAv * (radius * radiusScale);
			newPtMid.z = itNext->z;
			Vec3 newPtNext = posNext + normalInNext * (radius * radiusScale);
			newPtNext.z = itNextNext->z;
			shapeOut.push_back(newPtPrev);
			shapeOut.push_back(newPtMid);
			shapeOut.push_back(newPtNext);
		}
		else
		{
			float dot = segDirPrev.Dot(segDirNext);
			float extraRadiusScale = sqrtf(2.0f / (1.0f + dot));
			Vec3 newPtMid = posNext + normalAv * (radius * radiusScale * extraRadiusScale);
			newPtMid.z = itNext->z;
			shapeOut.push_back(newPtMid);
		}

	}
}

//===================================================================
// GetSingleNodePath
// Danny todo This "works" but doesn't generate ideal paths because it hugs
// a single wall - it can't cross over to hug the other wall. Should
// be OK for most sensible areas though. Also there is a danger that
// the path can get close to the area edge, but I don't know what can
// be done to stop that. Also, the shape shrinking could/should
// be precalculated.
//===================================================================
bool CFree2DNavRegion::GetSingleNodePath(const GraphNode* pNode,
                                         const Vec3& startPos, const Vec3& endPos, float radius,
                                         const NavigationBlockers& navigationBlockers,
                                         std::vector<PathPointDescriptor>& points,
                                         IAISystem::tNavCapMask) const
{
	return false;
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   CryGame Source File.
   ---------------------------------------------------------------------
   File name:   SmartPathFollower.cpp
   Version:     v1.00
   Description: PathFollower implementaion based on tracking the
   furthest reachable position on the path.

   ---------------------------------------------------------------------
   History:
   - 16 Feb 2010  : Will Wilson: Created.

 *********************************************************************/

#include "StdAfx.h"

#include "SmartPathFollower.h"
#include "CAISystem.h"
#include "AILog.h"
#include "NavPath.h"
#include "DebugDrawContext.h"
#include <CryGame/IGameFramework.h>

#include "Navigation/NavigationSystem/NavigationSystem.h"
#include <CryAISystem/NavigationSystem/INavigationQuery.h>

//#pragma optimize("", off)
//#pragma inline_depth(0)

/*static*/ const float InterpolatedPath::k_defaultClosestFindSegmentToleranceZ = 0.5f;

float InterpolatedPath::FindNextSegmentIndex(size_t startIndex) const
{
	float addition = .0f;
	if (!m_points.empty())
	{
		TPoints::const_iterator iter(m_points.begin());

		// Move index to start of search
		std::advance(iter, startIndex);
		float previousDistance = iter->distance;
		if (++iter != m_points.end())
		{
			addition = 1.0f;
		}
	}
	return startIndex + addition;
}

// Search for a segment index based on the distance along the path, startIndex is optional to allow search optimization
float InterpolatedPath::FindSegmentIndexAtDistance(float requestedDistance, size_t startIndex) const
{
	// Default to returning the last index (or -1 if empty)
	float result = static_cast<float>(m_points.size()) - 1.0f;

	// Clamp to limits
	requestedDistance = clamp_tpl(requestedDistance, 0.0f, m_totalDistance);

	if (!m_points.empty())
	{
		TPoints::const_iterator iter(m_points.begin());

		// Move index to start of search
		std::advance(iter, startIndex);

		float prevDistance = iter->distance;
		AIAssert(prevDistance <= requestedDistance + 0.01f);

		for (; iter != m_points.end(); ++iter)
		{
			const SPathControlPoint2& point = *iter;
			const float pointDistance = point.distance;

			// If waypoint at requested distance
			if (pointDistance >= requestedDistance)
			{
				float end = static_cast<float>(std::distance(m_points.begin(), iter));
				float start = (iter != m_points.begin()) ? end - 1.0f : 0.0f;

				float distanceWithinSegment = requestedDistance - prevDistance;
				float segmentDistance = pointDistance - prevDistance;
				AIAssert(distanceWithinSegment <= segmentDistance);

				float delta = (segmentDistance > 0.001f) ? distanceWithinSegment / segmentDistance : 0.0f;

				AIAssert(delta >= 0.0f && delta <= 1.0f);

				result = Lerp(start, end, delta);
				break;
			}

			prevDistance = pointDistance;
		}
	}

	return result;
}

/*
   struct TestPathFollower
   {
   TestPathFollower()
   {
   InterpolatedPath p;

   p.push_back( SPathControlPoint2( IAISystem::NAV_UNSET, Vec3(784.01086, 764.79883, 72.698608), 0 ) );
   p.push_back( SPathControlPoint2( IAISystem::NAV_UNSET, Vec3(783.87500, 764.75000, 72.750000), 0 ) );

   const float r = p.FindClosestSegmentIndex( Vec3(786.97339, 751.88739, 72.055885), 0, 5 );
   const float s = p.FindClosestSegmentIndex( Vec3(786.97339, 751.88739, 72.600000), 0, 5 );
   }
   } g__;
 */

// Returns the segment index of the closest point on the path (fractional part provides exact position if required)
float InterpolatedPath::FindClosestSegmentIndex(const Vec3& testPoint, const float startDistance,
                                                const float endDistance, const float toleranceZ /* = 0.5f */, const bool bUse2D /* = true */) const
{
	AIAssert(startDistance <= endDistance);

	float bestIndex = -1.0f;

	const size_t pointCount = m_points.size();
	if (pointCount != 0)
	{
		// If the point is this close or closer, consider it on the segment and stop searching
		static const float MIN_DISTANCE_TO_SEGMENT = 0.01f;

		float bestDistSq = std::numeric_limits<float>::max();

		const float startIndex = FindSegmentIndexAtDistance(startDistance);
		const size_t startIndexInt = static_cast<size_t>(startIndex);
		const float endIndex = FindSegmentIndexAtDistance(endDistance, startIndexInt);
		const size_t endIndexInt = static_cast<size_t>(ceil(endIndex));

		const Vec3 startPos(GetPositionAtSegmentIndex(startIndex));
		const Vec3 endPos(GetPositionAtSegmentIndex(endIndex));

		// Move index to start of search
		TPoints::const_iterator iter(m_points.begin());
		std::advance(iter, startIndexInt);
		// If we have more than one point, we skip the [startPos,startPos] segment as the check will be included in [startPos,secondPos]
		const bool evaluatingSinglePoint = (startIndex == endIndex);
		std::advance(iter, (evaluatingSinglePoint) ? 0 : 1);

		const TPoints::const_iterator finalIter(m_points.begin() + endIndexInt + 1);

		float segmentStartIndex = startIndex;
		Vec3 segmentStartPos(startPos);

		const float testZ = testPoint.z;

		// TODO: Do the search backwards so that we make sure that, if wierd stuff goes on and we're facing a worst possible scenario, we end up with the
		// that is furthest away from the start.
		for (; iter != finalIter; ++iter)
		{
			const SPathControlPoint2& point = *iter;

			const size_t segmentEndIndexInt = std::distance(m_points.begin(), iter);
			const float segmentEndIndex = min(static_cast<float>(segmentEndIndexInt), endIndex);
			const Vec3 segmentEndPos = (segmentEndIndexInt != endIndexInt) ? point.pos : endPos;

			const Lineseg segment(segmentStartPos, segmentEndPos);

			// Check segment at the same approx. height as the test point
			if (toleranceZ == FLT_MAX ||
			    ((testZ >= min(segment.start.z, segment.end.z) - toleranceZ) &&
			     (testZ <= max(segment.start.z, segment.end.z) + toleranceZ)))
			{
				float segmentDelta;
				const float distToSegmentSq =
				  bUse2D
				  ? Distance::Point_Lineseg2DSq(testPoint, segment, segmentDelta)
				  : Distance::Point_LinesegSq(testPoint, segment, segmentDelta);

				// If this is a new best
				if (distToSegmentSq < bestDistSq)
				{
					bestDistSq = distToSegmentSq;

					// Calculate length of the segment (may not be 1 at start and end)
					const float segmentLength = segmentEndIndex - segmentStartIndex;
					// Calculate the segment index by incorporating the delta returned by the segment test
					bestIndex = segmentStartIndex + (segmentDelta * segmentLength);

					// Early out if point on line - it can't get any closer
					if (bestDistSq <= square(MIN_DISTANCE_TO_SEGMENT))
						break;
				}
			}

			segmentStartPos = segmentEndPos;
			segmentStartIndex = segmentEndIndex;
		}
	}

	return bestIndex;
}

/// True if the path section between the start and endIndex deviates from the line by no more than maxDeviation.
bool InterpolatedPath::IsParrallelTo(const Lineseg& line, float startIndex, float endIndex, float maxDeviation) const
{
	AIAssert(startIndex <= endIndex);

	// Move along path between start point (inclusive) and end point (exclusive) testing each control point
	for (float index = startIndex; index < endIndex; index = floorf(index) + 1.0f)
	{
		Vec3 testPos(GetPositionAtSegmentIndex(index));

		float delta;
		if (Distance::Point_Lineseg2DSq(testPos, line, delta) > maxDeviation)
			return false;
	}

	// Finally test the end position
	Vec3 endPos(GetPositionAtSegmentIndex(endIndex));

	float delta;
	return (Distance::Point_Lineseg2DSq(endPos, line, delta) <= maxDeviation);
}

Vec3 InterpolatedPath::GetPositionAtSegmentIndex(float index) const
{
	AIAssert(!m_points.empty());

	// Bound index to minimum
	index = max(index, 0.0f);

	float delta = fmodf(index, 1.0f);

	size_t startIndex = static_cast<size_t>(index);
	AIAssert(startIndex < m_points.size());
	if (startIndex >= m_points.size())
		startIndex = m_points.size() - 1;
	size_t endIndex = startIndex + 1;

	Vec3 startPos(m_points[startIndex].pos);
	Vec3 endPos((endIndex < m_points.size()) ? m_points[endIndex].pos : startPos);

	return Lerp(startPos, endPos, delta);
}

float InterpolatedPath::GetDistanceAtSegmentIndex(float index) const
{
	AIAssert(!m_points.empty());

	// Bound index to minimum
	index = max(index, 0.0f);

	float delta = fmodf(index, 1.0f);

	size_t startIndex = static_cast<size_t>(index);
	AIAssert(startIndex < m_points.size());
	if (startIndex >= m_points.size())
		startIndex = m_points.size() - 1;
	size_t endIndex = startIndex + 1;

	float startDist = m_points[startIndex].distance;
	float endDist = (endIndex < m_points.size()) ? m_points[endIndex].distance : startDist;

	return Lerp(startDist, endDist, delta);
}

void InterpolatedPath::GetLineSegment(float startIndex, float endIndex, Lineseg& segment) const
{
	segment.start = GetPositionAtSegmentIndex(startIndex);
	segment.end = GetPositionAtSegmentIndex(endIndex);
}

// Returns the index of the first navigation type transition after index or end of path
size_t InterpolatedPath::FindNextNavTypeSectionIndexAfter(size_t index) const
{
	// Default is to return the last valid index (or ~0 if empty)
	size_t changeIndex = m_points.size() - 1;

	// If index safely on path
	if (index < changeIndex)
	{
		TPoints::const_iterator iter(m_points.begin() + index);
		const IAISystem::ENavigationType prevNavType = iter->navType;

		while (++iter != m_points.end())
		{
			// If the nav type changes
			if (iter->navType != prevNavType)
			{
				changeIndex = std::distance<TPoints::const_iterator>(m_points.begin(), iter);
				break;
			}
		}
	}

	return changeIndex;
}

// Returns the next index on the path that deviates from a straight line by the deviation specified.
float InterpolatedPath::FindNextInflectionIndex(float startIndex, float maxDeviation /*= 0.1f*/, const bool bUse2D /*= true*/) const
{
	// Start at start pos and generate ray based on next segment start point
	Lineseg testLine;
	testLine.start = GetPositionAtSegmentIndex(startIndex);
	testLine.end = testLine.start;

	const float maxDeviationSq = square(maxDeviation);

	float searchStartIndex = ceilf(startIndex);
	float searchStartDist = GetDistanceAtSegmentIndex(searchStartIndex);
	float searchEndDist = min(searchStartDist + 3.0f, m_totalDistance);

	const float distIncrement = 0.1f;
	float lastSafeIndex = searchStartIndex;
	size_t maxSteps = 64;

	for (float dist = searchStartDist; maxSteps && (dist < searchEndDist); dist += distIncrement, --maxSteps)
	{
		// Update line
		const float index = FindSegmentIndexAtDistance(dist, static_cast<size_t>(startIndex));
		testLine.end = GetPositionAtSegmentIndex(index);

		const size_t subIndexEnd = static_cast<size_t>(ceilf(index));

		// Test deviation against test range of the path
		for (size_t subIndex = static_cast<size_t>(searchStartIndex) + 1; subIndex < subIndexEnd; ++subIndex)
		{
			float delta;
			float deviationSq =
			  bUse2D
			  ? Distance::Point_Lineseg2DSq(m_points[subIndex].pos, testLine, delta)
			  : Distance::Point_LinesegSq(m_points[subIndex].pos, testLine, delta);
			if (deviationSq > maxDeviationSq)
			{
				// This point is no longer safe, use the last safe index
				return lastSafeIndex;
			}
		}

		lastSafeIndex = index;
	}

	return lastSafeIndex;
}

// Shortens the path to the specified endIndex
void InterpolatedPath::ShortenToIndex(float endIndex)
{
	// If cut is actually on the path
	if (endIndex >= 0.0f && endIndex < m_points.size())
	{
		// Create the cut index based on the next integer index after the endIndex
		size_t cutIndex = static_cast<size_t>(endIndex) + 1;

		float delta = fmodf(endIndex, 1.0f);

		// Create a new point based on the original
		SPathControlPoint2 newEndPoint(m_points[static_cast<size_t>(endIndex)]);
		newEndPoint.pos = GetPositionAtSegmentIndex(endIndex);
		newEndPoint.distance = GetDistanceAtSegmentIndex(endIndex);
		// NOTE: New end point copies other values from the point immediately before it. Safe assumption?

		// Erase removed section of the path
		m_points.erase(m_points.begin() + cutIndex, m_points.end());

		m_points.push_back(newEndPoint);
	}
}

void InterpolatedPath::push_back(const SPathControlPoint2& point)
{
	float distance = 0.0f;

	if (!m_points.empty())
		distance = m_points.back().pos.GetDistance(point.pos);

	m_points.push_back(point);

	float cumulativeDistance = m_totalDistance + distance;
	m_points.back().distance = cumulativeDistance;

	m_totalDistance = cumulativeDistance;
}

//===================================================================
// CSmartPathFollower
//===================================================================
CSmartPathFollower::CSmartPathFollower(const PathFollowerParams& params, const IPathObstacles& pathObstacles)
	: m_params(params),
	m_pathVersion(-2),
	m_followTargetIndex(),
	m_inflectionIndex(),
	m_pNavPath(),
	m_pathObstacles(pathObstacles),
	m_curPos(ZERO),
	m_followNavType(IAISystem::NAV_UNSET),
	m_lookAheadEnclosingAABB(AABB::RESET),
	m_allowCuttingCorners(true)
	//  m_safePathOptimal(false),
	//  m_reachTestCount()
{
	Reset();
}

//===================================================================
// CSmartPathFollower
//===================================================================
CSmartPathFollower::~CSmartPathFollower()
{
}
/*
   // Attempts to find a reachable target between startIndex (exclusive) & endIndex (inclusive).
   // Successful forward searches return the last reachable target found advancing up the path.
   // Successful reverse searches return the first reachable target found reversing back down the path.
   bool CSmartPathFollower::FindReachableTarget(float startIndex, float endIndex, float& reachableIndex) const
   {
   CRY_PROFILE_FUNCTION(PROFILE_AI);

   // Default fail value
   reachableIndex = -1.0f;

   float startDistance = m_path.GetDistanceAtSegmentIndex(startIndex);
   float endDistance = m_path.GetDistanceAtSegmentIndex(endIndex);

   // This can be negative if searching backwards along path
   float lengthToTest = endDistance - startDistance;
   float lengthToTestAbs = fabsf(lengthToTest);

   // Use this as the starting offset distance
   const float minTestDist = 0.4f;      // TODO: Parameterize

   // Direction of search determines the termination logic
   const bool lookingAhead = (lengthToTest >= 0.0f);

   // This is used to ease calculation of path distance without worrying about direction of search
   const float dirMultiplier = (lookingAhead) ? 1.0f : -1.0f;

   float advanceDist = minTestDist;
   // Move along the path starting at minTestDist
   for (float testOffsetDist = minTestDist; testOffsetDist < lengthToTestAbs; testOffsetDist += advanceDist)
   {
   float testDist = startDistance + (testOffsetDist * dirMultiplier);
   float testIndex = m_path.FindSegmentIndexAtDistance(testDist);			// TODO: Optimize, needs range look-up support

   if (CanReachTarget(m_curPos, testIndex))
   {
    reachableIndex = testIndex;

    // If looking behind, return the first reachable index found
    if (!lookingAhead)
    {
    //m_safePathOptimal = true;
    return true;
    }
   }
   else	// Can't reach target at this index
   {
    // If looking ahead & there is a previous valid index
    if (lookingAhead && reachableIndex >= 0.0f)
    {
    // Indicate the safe path is now optimal (continue using it until)
    //m_safePathOptimal = true;
    return true;
    }
    // Otherwise, keep looking (expensive but copes with weird situations better)
   }

   // Increase the advance distance
   advanceDist *= 1.5f;
   }

   // Finally, always test the endIndex (to support start/end of path)
   if (CanReachTarget(m_curPos, endIndex))
   {
   reachableIndex = endIndex;
   }

   return (reachableIndex >= 0.0f);
   }*/

// Attempts to find a reachable target between startIndex (exclusive) & endIndex (inclusive).
// Successful forward searches return the last reachable target found advancing up the path.
// Successful reverse searches return the first reachable target found reversing back down the path.
bool CSmartPathFollower::FindReachableTarget(float startIndex, float endIndex, float& reachableIndex) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	reachableIndex = -1.0f;

	if (startIndex < endIndex)
	{
		float nextIndex = 0.0f;
		float dist = endIndex - startIndex;

		// early out for reachable ends
		if (CanReachTarget(endIndex))
		{
			reachableIndex = endIndex;

			return true;
		}

		// early out if we can even reach anything
		//if (!CanReachTarget(startIndex + 0.35f))
		//{
		//return false;
		//}

		// now search with a decreasing step size
		float stepSize = 4.0f;

		do
		{
			nextIndex = reachableIndex >= 0.0f ? reachableIndex : floor_tpl(startIndex + stepSize);
			if (dist >= stepSize)
				CanReachTargetStep(stepSize, endIndex, nextIndex, reachableIndex);

			if (reachableIndex >= endIndex)
				return true;

		}
		while ((stepSize *= 0.5f) >= 0.5f);

		if (reachableIndex >= 0.0f)
		{
			if (gAIEnv.CVars.DrawPathFollower > 1)
				GetAISystem()->AddDebugSphere(m_path.GetPositionAtSegmentIndex(nextIndex), 0.25f, 255, 0, 0, 5.0f);
		}

		if (reachableIndex >= endIndex)
			return true;
	}
	else // Search backwards along the path incrementally
	{
		assert(startIndex >= endIndex);

		const float StartingAdvanceDistance = 0.5f;

		float advance = StartingAdvanceDistance;
		float start = m_path.GetDistanceAtSegmentIndex(startIndex);
		float end = m_path.GetDistanceAtSegmentIndex(endIndex);

		do
		{
			start -= advance;
			if (start < end)
				start = end;

			float test = m_path.FindSegmentIndexAtDistance(start);

			if (CanReachTarget(test))
			{
				reachableIndex = test;
				break;
			}

			advance = min(1.5f, advance * 1.5f);
		}
		while (start > end);

	}

	return reachableIndex >= 0.0f;
}

bool CSmartPathFollower::CanReachTargetStep(float step, float endIndex, float nextIndex, float& reachableIndex) const
{
	if (nextIndex > endIndex)
		nextIndex = endIndex;

	while (CanReachTarget(nextIndex))
	{
		if (gAIEnv.CVars.DrawPathFollower > 1)
			GetAISystem()->AddDebugSphere(m_path.GetPositionAtSegmentIndex(nextIndex), 0.25f, 0, 255, 0, 5.0f);

		reachableIndex = nextIndex;

		if (nextIndex >= endIndex)
			break;

		nextIndex = min(nextIndex + step, endIndex);
	}

	return reachableIndex >= 0.0f;
}

// True if the test position can be reached by the agent.
bool CSmartPathFollower::CanReachTarget(float testIndex) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const Vec3 startPos(m_curPos);

	// Assume safe until proved otherwise
	bool safeLineToTarget = false;

	//////////////////////////////////////////////////////////////////////////
	/// Navigation Mesh Handling

	assert(testIndex >= 0.0f);
	if (testIndex < 0.0f)
		return false;

	const SPathControlPoint2& segStartPoint = m_path[static_cast<size_t>(testIndex)];
	Vec3 testPos(m_path.GetPositionAtSegmentIndex(testIndex));

	{
		if (NavigationMeshID meshID = m_pNavPath->GetMeshID())
		{
			const Vec3 raiseUp(0.0f, 0.0f, 0.2f);
			const Vec3 raisedStartPos = startPos + raiseUp;
			const Vec3 raisedTestPos = testPos + raiseUp;

			const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(meshID);
			const MNM::CNavMesh& navMesh = mesh.navMesh;

			const MNM::CNavMesh::SGridParams& gridParams = navMesh.GetGridParams();

			const MNM::real_t horizontalRange(5.0f);
			const MNM::real_t verticalRange(2.0f);

			MNM::vector3_t startLocationInMeshCoordinates(raisedStartPos - gridParams.origin);
			MNM::vector3_t endLocationInMeshCoordinates(raisedTestPos - gridParams.origin);

			MNM::TriangleID triangleStartID = navMesh.GetTriangleAt(startLocationInMeshCoordinates, verticalRange, verticalRange, m_params.pQueryFilter);
			if (!triangleStartID)
			{
				MNM::vector3_t closestStartLocation, triangleCenter;
				triangleStartID = navMesh.GetClosestTriangle(startLocationInMeshCoordinates, verticalRange, horizontalRange, m_params.pQueryFilter, nullptr, &closestStartLocation);
				navMesh.PushPointInsideTriangle(triangleStartID, closestStartLocation, MNM::real_t(.05f));
				startLocationInMeshCoordinates = closestStartLocation;
			}

			MNM::TriangleID triangleEndID = navMesh.GetTriangleAt(endLocationInMeshCoordinates, verticalRange, verticalRange, m_params.pQueryFilter);
			if (!triangleEndID)
			{
				// Couldn't find a triangle for the end position. Pick the closest one.
				MNM::vector3_t closestEndLocation;
				triangleEndID = navMesh.GetClosestTriangle(endLocationInMeshCoordinates, verticalRange, horizontalRange, m_params.pQueryFilter, nullptr, &closestEndLocation);
				navMesh.PushPointInsideTriangle(triangleEndID, closestEndLocation, MNM::real_t(.05f));
				endLocationInMeshCoordinates = closestEndLocation;
			}

			if (!triangleStartID || !triangleEndID)
				return false;

			MNM::CNavMesh::RayCastRequest<512> wayRequest;

			if (navMesh.RayCast(startLocationInMeshCoordinates, triangleStartID, endLocationInMeshCoordinates, triangleEndID, wayRequest, m_params.pQueryFilter))
				return false;

			//Check against obstacles...
			if (m_pathObstacles.IsPathIntersectingObstacles(m_pNavPath->GetMeshID(), raisedStartPos, raisedTestPos, m_params.passRadius))
				return false;

			return true;
		}
	}

	return false;
}

//===================================================================
// DistancePointPoint
//===================================================================
float CSmartPathFollower::DistancePointPoint(const Vec3 pt1, const Vec3 pt2) const
{
	return m_params.use2D ? Distance::Point_Point2D(pt1, pt2) : Distance::Point_Point(pt1, pt2);
}

//===================================================================
// DistancePointPointSq
//===================================================================
float CSmartPathFollower::DistancePointPointSq(const Vec3 pt1, const Vec3 pt2) const
{
	return m_params.use2D ? Distance::Point_Point2DSq(pt1, pt2) : Distance::Point_PointSq(pt1, pt2);
}

void CSmartPathFollower::Reset()
{
	// NOTE: m_params is left unaltered
	m_pathVersion = -2;

	m_validatedStartPos.zero();
	m_followTargetIndex = 0.0f;
	m_followNavType = IAISystem::NAV_UNSET;
	m_inflectionIndex = 0.0f;

	stl::free_container(m_path);
}

//===================================================================
// AttachToPath
//===================================================================
void CSmartPathFollower::AttachToPath(INavPath* pNavPath)
{
	Reset();
	m_pNavPath = pNavPath;
}

//===================================================================
// ProcessPath
//===================================================================
void CSmartPathFollower::ProcessPath()
{
	AIAssert(m_pNavPath);
	m_pathVersion = m_pNavPath->GetVersion();

	// Reset cached data
	m_path.clear();
	m_followTargetIndex = 0.0f;
	m_followNavType = IAISystem::NAV_UNSET;
	m_inflectionIndex = 0.0f;

	const CNavPath* pNavPath = static_cast<CNavPath*>(m_pNavPath);
	const TPathPoints& pathPts = pNavPath->GetPath();

	if (pathPts.size() < 2)
		return;

	const TPathPoints::const_iterator itEnd = pathPts.end();

	SPathControlPoint2 pathPoint;
	pathPoint.distance = 0.0f;

	// For each existing path segment
	for (TPathPoints::const_iterator it = pathPts.begin(); it != itEnd; ++it)
	{
		// Distance used to look ahead and behind (magic number)
		const PathPointDescriptor& ppd = *it;

		pathPoint.navType = ppd.navType;
		pathPoint.pos = ppd.vPos;

		m_path.push_back(pathPoint);
	}

	// Ensure end point is at ground level (very often it isn't)
	if (m_params.snapEndPointToGround)
	{
		SPathControlPoint2& endPoint = m_path.back();

		// FIXME: GetFloorPos() is failing in some cases
		//    Vec3 floorPos;
		//    if (GetFloorPos(floorPos,							// Found floor position (if successful)
		//                    endPoint.pos + Vec3(0.0f, 0.0f, 0.5f),					// Existing end position
		//                    0.0f,									// Up distance (already included)
		//                    2.0f,									// End below floor position
		//                    m_params.passRadius,	// Test radius (avoid falling through cracks)
		//                    AICE_STATIC))					// Only consider static obstacles
		//    {
		//      endPoint.pos = floorPos;
		//    }
		//    else
		//    {
		//      // Can't find the floor!!
		//      IPersistantDebug* pDebug = gEnv->pGameFramework->GetIPersistantDebug();
		//      pDebug->AddSphere(endPoint.pos, 0.6f, ColorF(1.0f, 0, 0), 5.0f);
		//      //AIAssert(false);
		//    }

		// Snap to previous point height - assuming it is safe
		if (m_path.size() > 1)
		{
			const SPathControlPoint2& prevPoint = m_path[m_path.size() - 2];
			endPoint.pos.z = min(prevPoint.pos.z + 0.1f, endPoint.pos.z);
		}
	}

	// Now cut the path short if we should stop before the end
	if (m_params.endDistance > 0.0f)
	{
		float newDistance = m_path.TotalDistance() - m_params.endDistance;
		float endIndex = m_path.FindSegmentIndexAtDistance(newDistance);
		m_path.ShortenToIndex(endIndex);
	}

	AIAssert(m_path.size() >= 2);
}

// Attempts to advance the follow target along the path as far as possible while ensuring the follow
// target remains reachable. Returns true if the follow target is reachable, false otherwise.
bool CSmartPathFollower::Update(PathFollowResult& result, const Vec3& curPos, const Vec3& curVel, float dt)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool targetReachable = true;
	//m_reachTestCount = 0;

	CAISystem* pAISystem = GetAISystem();

	// If path has changed
	const bool bPathHasChanged = (m_pathVersion != m_pNavPath->GetVersion());
	if (bPathHasChanged)
	{
		// Duplicate, fix end height and optionally shorten path
		ProcessPath();
	}

	// Set result defaults (ensure no undefined data is passed back to the caller)
	{
		// This is used to vaguely indicate if the FT has reached path end and so has the agent
		result.reachedEnd = false;

		// Don't generate predicted states (obsolete)
		if (result.predictedStates)
			result.predictedStates->resize(0);

		result.followTargetPos = curPos;
		result.inflectionPoint = curPos;

		result.velocityOut.zero();
	}

	// Cope with empty path (should not occur but best to be safe)
	if (m_path.empty() || m_path.TotalDistance() < FLT_EPSILON)
	{
		// Indicate we've reached the "end" of path, either because it's empty or has a total distance of 0.
		result.reachedEnd = true;
		return true;
	}

	m_curPos = curPos;

	// Record the current navigation type for the follow target (allows detection of transitions)
	if (m_followNavType == IAISystem::NAV_UNSET)
	{
		size_t segmentIndex = static_cast<size_t>(m_followTargetIndex);
		m_followNavType = (segmentIndex < m_path.size()) ? m_path[segmentIndex].navType : IAISystem::NAV_UNSET;
	}

	Vec3 followTargetPos = m_path.GetPositionAtSegmentIndex(m_followTargetIndex);
	Vec3 inflectionPoint = m_path.GetPositionAtSegmentIndex(m_inflectionIndex);

	// TODO: Optimize case where very little deviation from path (check max distances of intervening points to line)

	bool recalculateTarget = false;
	bool onSafeLine = false;

	// fraction of the current path segment that needs to be covered before target is recalculated
	const float recalculationFraction = 0.25f;

	// If safe path not optimal or progress has been made along the path
	if (/*!m_safePathOptimal ||*/ m_followTargetIndex > 0.0f)
	{
		// Generate the safe line previously calculated
		Lineseg safeLine(m_validatedStartPos, followTargetPos);

		float delta;
		const float distToSafeLineSq =
		  m_params.use2D
		  ? Distance::Point_Lineseg2DSq(curPos, safeLine, delta)
		  : Distance::Point_LinesegSq(curPos, safeLine, delta);

		onSafeLine = distToSafeLineSq < sqr(0.15f);   // TODO: Parameterize & perhaps scale by proximity to target?

		// Are we still effectively on the safe line?
		if (onSafeLine)
		{
			// Have we moved significantly along safe line?
			if (m_allowCuttingCorners && (delta > recalculationFraction))
			{
				// Is the more path left to advance the FT?
				if (m_followTargetIndex < m_path.size() - 1.001f)
					recalculateTarget = true;
			}
		}
		else  // Deviated too far from safe line
		{
			recalculateTarget = true;
		}
	}
	else  // No target yet calculated (or attempting to get to start)
	{
		recalculateTarget = true;
	}

	const bool isInsideObstacles = m_pathObstacles.IsPointInsideObstacles(m_curPos);
	bool isAllowedToShortcut;

	if (gAIEnv.CVars.SmartPathFollower_useAdvancedPathShortcutting == 0)
	{
		isAllowedToShortcut = isInsideObstacles ? false : m_params.isAllowedToShortcut;
	}
	else
	{
		if (isInsideObstacles || !m_params.isAllowedToShortcut)
		{
			isAllowedToShortcut = false;
		}
		else
		{
			isAllowedToShortcut = true; // will change below

			// inspect the path segments from our current position up to the look-ahead position:
			// if any of them is close to an obstacle, then the path may NOT be shortcut

			float indexAtCurrentPos;
			float indexAtLookAheadPos;

			if (m_followTargetIndex > 0.0f)
			{
				indexAtCurrentPos = m_followTargetIndex;
			}
			else
			{
				// we're at the start of the path
				const float startDist = 0.0f;
				const float endDist = 5.0f;       // 5m tolerance
				const float toleranceZ = FLT_MAX; // using a FLT_MAX z-tolerance in case we're currently traversing a SmartObject that will make us end up at a "much higher" (or "much lower") position than we started at the time of the traveral
				indexAtCurrentPos = m_path.FindClosestSegmentIndex(curPos, startDist, endDist, toleranceZ, m_params.use2D);
			}

			const float currentDistance = m_path.GetDistanceAtSegmentIndex(indexAtCurrentPos);
			indexAtLookAheadPos = m_path.FindSegmentIndexAtDistance(currentDistance + gAIEnv.CVars.SmartPathFollower_LookAheadDistance);

			for (float index = indexAtCurrentPos; index <= indexAtLookAheadPos; index += 1.0f)
			{
				float indexAtStartOfSegment = floorf(index);
				float indexAtEndOfSegment = indexAtStartOfSegment + 1.0f;

				// make sure the index at the end of the segment doesn't go beyond the end of the path
				indexAtEndOfSegment = std::min(indexAtEndOfSegment, static_cast<float>(m_path.size()) - 1.0f);

				Lineseg lineseg;
				m_path.GetLineSegment(indexAtStartOfSegment, indexAtEndOfSegment, lineseg);

				const float maxDistanceToObstaclesToConsiderTooClose = 0.5f;
				if (m_pathObstacles.IsLineSegmentIntersectingObstaclesOrCloseToThem(lineseg, maxDistanceToObstaclesToConsiderTooClose))
				{
#ifndef _RELEASE
					if (gAIEnv.CVars.SmartPathFollower_useAdvancedPathShortcutting_debug != 0)
					{
						gEnv->pGameFramework->GetIPersistantDebug()->Begin("SmartPathFollower_useAdvancedPathShortcutting_debug", false);
						gEnv->pGameFramework->GetIPersistantDebug()->AddLine(lineseg.start + Vec3(0.0, 0.0f, 1.5f), lineseg.end + Vec3(0.0f, 0.0f, 1.5f), ColorF(1.0f, 0.0f, 0.0f), 1.0f);
					}
#endif
					isAllowedToShortcut = false;
					break;
				}

#ifndef _RELEASE
				if (gAIEnv.CVars.SmartPathFollower_useAdvancedPathShortcutting_debug != 0)
				{
					gEnv->pGameFramework->GetIPersistantDebug()->Begin("SmartPathFollower_useAdvancedPathShortcutting_debug", false);
					gEnv->pGameFramework->GetIPersistantDebug()->AddLine(lineseg.start + Vec3(0.0, 0.0f, 1.5f), lineseg.end + Vec3(0.0f, 0.0f, 1.5f), ColorF(0.0f, 1.0f, 0.0f), 1.0f);
				}
#endif
			}
		}
	}

	if (recalculateTarget && isAllowedToShortcut)
	{
		float currentIndex;
		float lookAheadIndex;

		const float LookAheadDistance = gAIEnv.CVars.SmartPathFollower_LookAheadDistance;

		// Generate a look-ahead range based on the current FT index.
		if (m_followTargetIndex > 0.0f)
		{
			currentIndex = m_followTargetIndex;

			const float currentDistance = m_path.GetDistanceAtSegmentIndex(m_followTargetIndex);
			lookAheadIndex = m_path.FindSegmentIndexAtDistance(currentDistance + LookAheadDistance);
		}
		else
		{
			const float startDist = 0.0f;
			const float endDist = 5.0f;       // We're at the start of the path, so we should be within 5m
			const float toleranceZ = FLT_MAX; // using a FLT_MAX z-tolerance in case we're currently traversing a SmartObject that will make us end up at a "much higher" (or "much lower") position than we started at the time of the traveral

			currentIndex = m_path.FindClosestSegmentIndex(curPos, startDist, endDist, toleranceZ, m_params.use2D);

			const float currentDistance = m_path.GetDistanceAtSegmentIndex(currentIndex);
			lookAheadIndex = m_path.FindSegmentIndexAtDistance(currentDistance + LookAheadDistance);
		}

		float newTargetIndex = -1.0f;

		// query all entities in this path as well as their bounding boxes from physics
		m_lookAheadEnclosingAABB.Reset();
		m_lookAheadEnclosingAABB.Add(m_curPos);
		m_lookAheadEnclosingAABB.Add(m_path.GetPositionAtSegmentIndex(lookAheadIndex));

		for (float current = currentIndex, end = lookAheadIndex; current <= end; current += 1.0f)
			m_lookAheadEnclosingAABB.Add(m_path.GetPositionAtSegmentIndex(current));
		m_lookAheadEnclosingAABB.Expand(Vec3(m_params.passRadius + 0.005f, m_params.passRadius + 0.005f, 0.5f));

		// 1. Search forward to find the next corner from current position.
		if (!FindReachableTarget(currentIndex, lookAheadIndex, newTargetIndex))
		{
			// 2. Try original target.
			if (onSafeLine || CanReachTarget(m_followTargetIndex))
			{
				newTargetIndex = m_followTargetIndex;
			}
			else  // Old target inaccessible
			{
				// 3. Find closest point on path before previous FT - use it to bound look-behind search
				// NOTE: Should be safe even if path loops and gives inaccessible closest as search is done backwards.
				float lookBehindIndex = 0.0f;

				// 4. Search backwards from previous FT - start small and increase until beyond closet point on path.
				if (!FindReachableTarget(m_followTargetIndex, lookBehindIndex, newTargetIndex))
				{
					// 5. Admit defeat and inform caller. The caller probably needs to regenerate path.
					targetReachable = false;

#ifndef _RELEASE
					if (gAIEnv.CVars.DrawPathFollower > 0)
					{
						CDebugDrawContext dc;
						dc->Draw3dLabel(m_curPos, 1.6f, "Failed PathFollower!");
					}
#endif
				}
			}
		}

		// If valid target was found
		if (newTargetIndex >= 0.0f)
		{
			m_validatedStartPos = curPos;

			// If the index has actually changed (avoids recalculating inflection point)
			if (m_followTargetIndex != newTargetIndex)
			{
				// Update the target
				m_followTargetIndex = newTargetIndex;
				followTargetPos = m_path.GetPositionAtSegmentIndex(newTargetIndex);

				// Update inflection point
				m_inflectionIndex = m_path.FindNextInflectionIndex(newTargetIndex, m_params.pathRadius * 0.5f, m_params.use2D);   // TODO: Parameterize max deviation
				inflectionPoint = m_path.GetPositionAtSegmentIndex(m_inflectionIndex);

				// Update the cached follow target navigation-type
				size_t segmentIndex = static_cast<size_t>(m_followTargetIndex);
				m_followNavType = m_path[segmentIndex].navType;
			}
		}
	}

	if (recalculateTarget && !isAllowedToShortcut)
	{
		// If we need to stick to follow a path we don't need to try to cut
		// and find the shortest way to reach the final target position.
		const float predictionTime = GetPredictionTimeForMovingAlongPath(isInsideObstacles, curVel.GetLengthSquared2D());
		const float kIncreasedZToleranceForFindingClosestSegment = FLT_MAX; // We basically don't care about the z tolerance

		float currentIndex = m_path.FindClosestSegmentIndex(m_curPos, .0f, m_path.TotalDistance(), kIncreasedZToleranceForFindingClosestSegment, m_params.use2D);
		if (currentIndex < 0.0f)
		{
			assert(currentIndex >= 0.0f);
			return false;
		}

		if (m_followTargetIndex == .0f)
		{
			m_followTargetIndex = currentIndex;
			const float kMinDistanceSqToFirstClosestPointInPath = sqr(0.7f);
			const Vec3 pathStartPos = m_path.GetPositionAtSegmentIndex(currentIndex);
			const float distanceSqToClosestPointOfPath = DistancePointPointSq(pathStartPos, m_curPos);
			// Update follow target
			if (distanceSqToClosestPointOfPath < kMinDistanceSqToFirstClosestPointInPath)
			{
				m_followTargetIndex = m_path.FindNextSegmentIndex(static_cast<size_t>(m_followTargetIndex));
			}

			followTargetPos = m_path.GetPositionAtSegmentIndex(m_followTargetIndex);

			// Update inflection point/index
			m_inflectionIndex = m_path.FindNextSegmentIndex(static_cast<size_t>(m_followTargetIndex));
			inflectionPoint = m_path.GetPositionAtSegmentIndex(m_inflectionIndex);
		}
		else
		{
			const float currentDistance = m_path.GetDistanceAtSegmentIndex(currentIndex);
			const Vec3 localNextPos = curVel * predictionTime;
			const float kMinLookAheadDistanceAllowed = 1.0f;
			const float lookAheadDistance = max(kMinLookAheadDistanceAllowed, localNextPos.len());
			const float lookAheadIndex = m_path.FindSegmentIndexAtDistance(currentDistance + lookAheadDistance);
			const float nextFollowIndex = m_path.FindNextSegmentIndex(static_cast<size_t>(lookAheadIndex));

			if (m_followTargetIndex < nextFollowIndex)
			{
				m_followTargetIndex = nextFollowIndex;
				followTargetPos = m_path.GetPositionAtSegmentIndex(lookAheadIndex);

				m_inflectionIndex = m_path.FindNextSegmentIndex(static_cast<size_t>(nextFollowIndex));
				inflectionPoint = m_path.GetPositionAtSegmentIndex(m_inflectionIndex);
			}
		}
		// Update the starting position
		m_validatedStartPos = m_curPos;
	}

	// Generate results
	{
		// Pass the absolute data (will eventually replace the old velocity based data below)
		result.followTargetPos = followTargetPos;
		result.inflectionPoint = inflectionPoint;

		// TODO: The following is deprecated. Passing motion requests using velocity only is imprecise,
		// unstable with frame time (due to interactions between animation and physics) and requires
		// hacks and magic numbers to make it work semi-reliably.
		if (bool allowMovement = true)
		{
			Vec3 velocity = followTargetPos - curPos;
			if (m_params.use2D)
				velocity.z = 0.0f;
			velocity.NormalizeSafe();
			float distToEnd = GetDistToEnd(&curPos);

			float speed = m_params.normalSpeed;

			// See if target has reached end of path
			const float MinDistanceToEnd = m_params.endAccuracy;

			if (m_params.stopAtEnd)
			{
				if (distToEnd < MinDistanceToEnd)
				{
					result.reachedEnd = true;
					speed = 0.0f;
				}
				else
				{
					float slowDownDist = 1.2f;
					const float decelerationMultiplier = m_params.isVehicle ? gAIEnv.CVars.SmartPathFollower_decelerationVehicle : gAIEnv.CVars.SmartPathFollower_decelerationHuman;
					speed = min(speed, decelerationMultiplier * distToEnd / slowDownDist);
				}
			}
			else
			{
				// This is used to vaguely indicate if the FT has reached path end and so has the agent
				const float MaxTimeStep = 0.5f;
				result.reachedEnd = distToEnd < max(MinDistanceToEnd, speed * min(dt, MaxTimeStep));
			}

			// TODO: Stability might be improved by detecting and reducing velocities pointing backwards along the path.

			// TODO: Curvature speed control

			// limit speed & acceleration/deceleration
			if (bPathHasChanged)
			{
				// take over component of curVel along new movement direction
				// (keep current speed if continuing in the same direction,
				// slow down otherwise; going to zero when moving at right or higher angles)
				Vec3 velDir = curVel;
				Vec3 moveDir = (followTargetPos - curPos);
				if (m_params.use2D)
				{
					velDir.z = 0.0f;
					moveDir.z = 0.0f;
				}

				float curSpeed = velDir.NormalizeSafe();
				moveDir.NormalizeSafe();

				float dot = velDir.Dot(moveDir);
				Limit(dot, 0.0f, 1.0f);

				m_lastOutputSpeed = curSpeed * dot;
			}
			Limit(m_lastOutputSpeed, m_params.minSpeed, m_params.maxSpeed);
			float maxOutputSpeed = min(m_lastOutputSpeed + dt * m_params.maxAccel, m_params.maxSpeed);
			float minOutputSpeed = m_params.stopAtEnd ? 0.0f : max(m_lastOutputSpeed - dt * m_params.maxDecel, m_params.minSpeed);

			Limit(speed, minOutputSpeed, maxOutputSpeed);
			m_lastOutputSpeed = speed;

			// Integrate speed
			velocity *= speed;

			assert(velocity.len() <= (m_params.maxSpeed + 0.001f));

			result.velocityOut = velocity;
		}
	}

	AIAssert(result.velocityOut.IsValid());

#ifndef _RELEASE
	if (gAIEnv.CVars.DrawPathFollower > 0)
	{
		// Draw path
		Draw();

		Vec3 up(0.0f, 0.0f, 0.2f);

		// Draw the safe line & follow target
		CDebugDrawContext dc;
		ColorB debugColor = recalculateTarget ? ColorB(255, 0, 0) : ColorB(0, 255, 0);

		if (result.reachedEnd)
		{
			debugColor.g = result.velocityOut.IsZeroFast() ? 128 : 0;
			debugColor.b = 255;
		}

		dc->DrawSphere(followTargetPos + up, 0.2f, debugColor);
		dc->DrawCapsuleOutline(m_validatedStartPos + up, followTargetPos + up, m_params.passRadius, debugColor);

		// Draw inflection point
		dc->DrawSphere(inflectionPoint + up, 0.2f, ColorB(0, 0, 255));
		dc->DrawCapsuleOutline(followTargetPos + up, inflectionPoint + up, m_params.passRadius, ColorB(0, 0, 255));

		dc->DrawArrow(m_curPos + up, result.velocityOut, 0.2f, ColorB(230, 200, 180));

		//s_passibilityCheckMaxCount = max(s_passibilityCheckMaxCount, m_reachTestCount);
		//dc->Draw3dLabel(m_curPos + up, 1.5f, "%d/%d (%d)", m_reachTestCount, s_passibilityCheckMaxCount, s_cheapTestSuccess);
	}
#endif      // !defined _RELEASE

	return targetReachable;
}
//===================================================================
// GetPredictionTimeForMovingAlongPath
//===================================================================
float CSmartPathFollower::GetPredictionTimeForMovingAlongPath(const bool isInsideObstacles,
                                                              const float currentSpeedSq)
{
	float predictionTime = gAIEnv.CVars.SmartPathFollower_LookAheadPredictionTimeForMovingAlongPathWalk;
	if (isInsideObstacles)
	{
		// Heuristic time to look ahead on the path when the agent moves inside the shape
		// of dynamic obstacles. We try to don't look ahead too much to stick more to the path
		predictionTime = 0.2f;
	}
	else
	{
		const float minSpeedToBeConsideredRunningOrSprintingSq = sqr(2.0f);
		if (currentSpeedSq > minSpeedToBeConsideredRunningOrSprintingSq)
		{
			predictionTime = gAIEnv.CVars.SmartPathFollower_LookAheadPredictionTimeForMovingAlongPathRunAndSprint;
		}
	}

	return predictionTime;
}

//===================================================================
// GetDistToEnd
//===================================================================
float CSmartPathFollower::GetDistToEnd(const Vec3* pCurPos) const
{
	float distanceToEnd = 0.0f;

	if (!m_path.empty())
	{
		distanceToEnd = m_path.TotalDistance() - m_path.GetDistanceAtSegmentIndex(m_followTargetIndex);
		if (pCurPos)
		{
			Vec3 followTargetPos(m_path.GetPositionAtSegmentIndex(m_followTargetIndex));
			distanceToEnd += DistancePointPoint(*pCurPos, followTargetPos);
		}
	}

	return distanceToEnd;
}

//===================================================================
// Serialize
//===================================================================
void CSmartPathFollower::Serialize(TSerialize ser)
{
	ser.Value("m_params", m_params);

	ser.Value("m_followTargetIndex", m_followTargetIndex);
	ser.Value("m_inflectionIndex", m_inflectionIndex);
	ser.Value("m_validatedStartPos", m_validatedStartPos);
	ser.Value("m_allowCuttingCorners", m_allowCuttingCorners);

	if (ser.IsReading())
	{
		// NOTE: It's assumed the path will be rebuilt as we don't serialize the path version.
		// If we're reading, we need to rebuild the cached path by creating an "invalid" path version.
		m_pathVersion = -2;
	}
}

//===================================================================
// Draw
//===================================================================
void CSmartPathFollower::Draw(const Vec3& drawOffset) const
{
	CDebugDrawContext dc;
	size_t pathSize = m_path.size();
	for (size_t i = 0; i < pathSize; ++i)
	{
		Vec3 prevControlPoint(m_path.GetPositionAtSegmentIndex(static_cast<float>(i) - 1.0f));
		Vec3 thisControlPoint(m_path.GetPositionAtSegmentIndex(static_cast<float>(i)));

		dc->DrawLine(prevControlPoint, ColorB(0, 0, 0), thisControlPoint, ColorB(0, 0, 0));
		dc->DrawSphere(thisControlPoint, 0.05f, ColorB(0, 0, 0));
		dc->Draw3dLabel(thisControlPoint, 1.5f, "%" PRISIZE_T, i);
	}
}

//===================================================================
// GetDistToSmartObject
//===================================================================
float CSmartPathFollower::GetDistToSmartObject() const
{
	return GetDistToNavType(IAISystem::NAV_SMARTOBJECT);
}

float CSmartPathFollower::GetDistToNavType(IAISystem::ENavigationType navType) const
{
	// NOTE: This function is used by the Trace Op to detect movement through straight SO's (those that define no actions).
	// It's used to preclude path regeneration once the SO is within 1m (magic number).
	// This whole thing is poorly formed & ideally needs to be replaced by FT waiting to ensure accurate positioning.

	// TODO: Replace this *hack* with wait system.

	if (!m_path.empty())
	{
		size_t nPts = m_path.size();

		// If at end of path
		if (m_followTargetIndex + 1 >= nPts)
		{
			if (navType == m_path.back().navType)
				return Distance::Point_Point(m_path.back().pos, m_curPos);

			return std::numeric_limits<float>::max();
		}

		// FIXME: Stupid and expensive (but the original behavior)
		const float closestIndex = m_path.FindClosestSegmentIndex(
		  m_curPos,
		  0.0f,
		  m_path.GetDistanceAtSegmentIndex(m_followTargetIndex),
		  InterpolatedPath::k_defaultClosestFindSegmentToleranceZ,
		  m_params.use2D);

		if (closestIndex < 0.0f)
			return std::numeric_limits<float>::max();

		const size_t closestIndexInt = static_cast<size_t>(closestIndex);

		// Work out segment index for agent position
		float curDist = DistancePointPoint(m_curPos, m_path[closestIndexInt].pos);
		float totalDist = 0.0f;
		if (closestIndexInt + 1 < nPts)
		{
			totalDist = DistancePointPoint(m_path[closestIndexInt].pos, m_path[closestIndexInt + 1].pos);

			float curFraction = (totalDist > 0.0f) ? (curDist / totalDist) : 0.0f;

			// If current segment is of the selected nav type and equal customID.
			if ((m_path[closestIndexInt].navType == navType) &&
			    (m_path[closestIndexInt + 1].navType == navType) &&
			    (m_path[closestIndexInt].customId == m_path[closestIndexInt + 1].customId))
			{
				// If over half-way through segment - we're there! Otherwise not...
				return (curFraction < 0.5f) ? 0.0f : std::numeric_limits<float>::max();
			}
		}
		else if ((closestIndexInt + 1 == nPts) && (m_path[closestIndexInt].navType == navType))
		{
			return DistancePointPoint(m_curPos, m_path[closestIndexInt].pos);
		}
		else
			return std::numeric_limits<float>::max();

		// Go through all segments looking for navType
		float dist = 0.0f;
		Vec3 lastPos = m_curPos;
		for (unsigned int i = closestIndexInt + 1; i < nPts; ++i)
		{
			dist += DistancePointPoint(lastPos, m_path[i].pos);
			if (i + 1 < nPts)
			{
				if ((m_path[i].navType == navType) &&
				    (m_path[i + 1].navType == navType) &&
				    (m_path[i].customId == m_path[i + 1].customId))
					return dist;
			}
			else
			{
				if (m_path[i].navType == navType)
					return dist;
			}
			lastPos = m_path[i].pos;
		}
	}

	// Not found
	return std::numeric_limits<float>::max();
}

//===================================================================
// GetDistToCustomNav
//===================================================================
float CSmartPathFollower::GetDistToCustomNav(const InterpolatedPath& controlPoints, uint32 curLASegmentIndex, const Vec3& curLAPos) const
{
	size_t nPts = controlPoints.size();

	float dist = std::numeric_limits<float>::max();

	if (curLASegmentIndex + 1 < nPts)
	{

		dist = 0.0f;

		if (controlPoints[curLASegmentIndex].navType != IAISystem::NAV_CUSTOM_NAVIGATION ||
		    controlPoints[curLASegmentIndex + 1].navType != IAISystem::NAV_CUSTOM_NAVIGATION)
		{
			Vec3 lastPos = curLAPos;

			for (uint32 i = curLASegmentIndex + 1; i < nPts; ++i)
			{
				dist += DistancePointPoint(lastPos, m_path[i].pos);
				if (i + 1 < nPts)
				{
					if (controlPoints[i].navType == IAISystem::NAV_CUSTOM_NAVIGATION &&
					    controlPoints[i + 1].navType == IAISystem::NAV_CUSTOM_NAVIGATION)
					{
						break;
					}
				}
				else
				{
					if (controlPoints[i].navType == IAISystem::NAV_CUSTOM_NAVIGATION)
					{
						break;
					}
				}

				lastPos = controlPoints[i].pos;
			}
		}
	}

	return dist;
}

//===================================================================
// GetPathPointAhead
//===================================================================
Vec3 CSmartPathFollower::GetPathPointAhead(float requestedDist, float& actualDist) const
{
	Vec3 followTargetPos(m_path.GetPositionAtSegmentIndex(m_followTargetIndex));

	float posDist = m_curPos.GetDistance(followTargetPos);
	if (requestedDist <= posDist)
	{
		actualDist = requestedDist;
		return Lerp(m_curPos, followTargetPos, (posDist > 0.0f) ? (requestedDist / posDist) : 1.0f);
	}

	// Remove the safe line distance
	requestedDist -= posDist;

	float ftDist = m_path.GetDistanceAtSegmentIndex(m_followTargetIndex);

	// Find the end distance along the path
	float endDist = ftDist + requestedDist;
	if (endDist > m_path.TotalDistance())
		endDist = m_path.TotalDistance();

	// Actual distance ahead = (distance along line) + distance from FT to agent position
	actualDist = (endDist - ftDist) + posDist;

	float endIndex = m_path.FindSegmentIndexAtDistance(endDist);

	return m_path.GetPositionAtSegmentIndex(endIndex);
}

//===================================================================
// CheckWalkability
//===================================================================
bool CSmartPathFollower::CheckWalkability(const Vec2* path, const size_t length) const
{
	// assumes m_curPos is set and valid (which is only the case when following a path)
	if (!m_path.empty())
	{
		if (NavigationMeshID meshID = m_pNavPath->GetMeshID())
		{
			const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(meshID);
			const MNM::CNavMesh& navMesh = mesh.navMesh;

			const Vec3 raiseUp(0.0f, 0.0f, 0.2f);
			const MNM::real_t verticalRange(2.0f);

			Vec3 startLoc = m_curPos + raiseUp;

			MNM::vector3_t mnmStartLoc = MNM::vector3_t(MNM::real_t(startLoc.x), MNM::real_t(startLoc.y), MNM::real_t(startLoc.z));
			MNM::TriangleID triStart = navMesh.GetTriangleAt(mnmStartLoc, verticalRange, verticalRange, m_params.pQueryFilter);
			IF_UNLIKELY (!triStart)
				return false;

			float currentZ = startLoc.z;
			for (size_t i = 0; i < length; ++i)
			{
				const Vec2& endLoc2D = path[i];
				const Vec3 endLoc(endLoc2D.x, endLoc2D.y, currentZ);

				const MNM::vector3_t mnmEndLoc = MNM::vector3_t(MNM::real_t(endLoc.x), MNM::real_t(endLoc.y), MNM::real_t(endLoc.z));

				const MNM::TriangleID triEnd = navMesh.GetTriangleAt(mnmEndLoc, verticalRange, verticalRange, m_params.pQueryFilter);

				if (!triEnd)
					return false;

				MNM::CNavMesh::RayCastRequest<512> raycastRequest;

				if (navMesh.RayCast(mnmStartLoc, triStart, mnmEndLoc, triEnd, raycastRequest, m_params.pQueryFilter) != MNM::CNavMesh::eRayCastResult_NoHit)
					return false;

				if (m_pathObstacles.IsPathIntersectingObstacles(m_pNavPath->GetMeshID(), startLoc, endLoc, m_params.passRadius))
					return false;

				MNM::vector3_t v0, v1, v2;
				const bool success = mesh.navMesh.GetVertices(triEnd, v0, v1, v2);
				CRY_ASSERT(success);
				const MNM::vector3_t closest = MNM::ClosestPtPointTriangle(mnmEndLoc, v0, v1, v2);
				currentZ = closest.GetVec3().z;

				startLoc = endLoc;
				mnmStartLoc = mnmEndLoc;
				triStart = triEnd;
			}
		}

		return true;
	}

	return false;
}

//===================================================================
// GetAllowCuttingCorners
//===================================================================
bool CSmartPathFollower::GetAllowCuttingCorners() const
{
	return m_allowCuttingCorners;
}

//===================================================================
// SetAllowCuttingCorners
//===================================================================
void CSmartPathFollower::SetAllowCuttingCorners(const bool allowCuttingCorners)
{
	m_allowCuttingCorners = allowCuttingCorners;
}

//===================================================================
// IsRemainingPathOverlappingWithNavMeshTileBounds
//===================================================================
bool CSmartPathFollower::IsRemainingPathOverlappingWithNavMeshTileBounds(const NavigationMeshID affectedMeshID, const MNM::TileID affectedTileID) const
{
	AIAssert(!m_path.empty());
	AIAssert(m_pNavPath->GetMeshID() == affectedMeshID);

	AABB remainingPathAABB;

	// add our current position to the path's AABB (our position is very likely somewhere between 2 path points)
	const float indexOfCurrentPos = m_path.FindClosestSegmentIndex(m_curPos, 0.0f, FLT_MAX, FLT_MAX, m_params.use2D);
	remainingPathAABB.min = remainingPathAABB.max = m_path.GetPositionAtSegmentIndex(indexOfCurrentPos);

	// add all succeeding points to the path's AABB
	for (size_t i = static_cast<size_t>(indexOfCurrentPos) + 1, n = m_path.size(); i < n; ++i)
	{
		remainingPathAABB.Add(m_path[i].pos);
	}

	// extend the path's AABB a bit as the path is usually very close to the ground and might therefore slightly slip under the tile's AABB
	remainingPathAABB.Expand(Vec3(0.0f, 0.0f, 0.5f));

	AABB tileAABB;
	gAIEnv.pNavigationSystem->GetTileBoundsForMesh(affectedMeshID, affectedTileID, tileAABB);

	return remainingPathAABB.IsIntersectBox(tileAABB);
}

//===================================================================
// IsRemainingPathTraversableOnNavMesh
//===================================================================
bool CSmartPathFollower::IsRemainingPathTraversableOnNavMesh() const
{
	AIAssert(!m_path.empty());

	if (const NavigationMeshID meshIDUsedByPath = m_pNavPath->GetMeshID())
	{
		const MNM::CNavMesh& navMeshUsedByPath = gAIEnv.pNavigationSystem->GetMesh(meshIDUsedByPath).navMesh;

		float index1 = m_path.FindClosestSegmentIndex(m_curPos, 0.0f, FLT_MAX, FLT_MAX, m_params.use2D);
		float index2 = m_path.FindNextSegmentIndex(static_cast<size_t>(index1));

		// - perform raycasts along all segments of the remaining path
		// - as soon as a raycast fails we know that the segment can no longer be used to move along
		for (size_t i = static_cast<size_t>(index1) + 1; i < m_path.size(); i++)
		{
			const Vec3 segmentPos1 = m_path.GetPositionAtSegmentIndex(index1);
			const Vec3 segmentPos2 = m_path.GetPositionAtSegmentIndex(index2);

			const MNM::real_t verticalRange(2.0f);

			const MNM::vector3_t mnmStartLoc = MNM::vector3_t(segmentPos1);
			const MNM::vector3_t mnmEndLoc = MNM::vector3_t(segmentPos2);
			const MNM::TriangleID triStart = navMeshUsedByPath.GetTriangleAt(mnmStartLoc, verticalRange, verticalRange, m_params.pQueryFilter);
			const MNM::TriangleID triEnd = navMeshUsedByPath.GetTriangleAt(mnmEndLoc, verticalRange, verticalRange, m_params.pQueryFilter);

			if (!triStart || !triEnd)
			{
				return false;
			}

			if (triStart)
			{
				MNM::CNavMesh::RayCastRequest<512> raycastRequest;
				MNM::CNavMesh::ERayCastResult raycastResult = navMeshUsedByPath.RayCast(mnmStartLoc, triStart, mnmEndLoc, triEnd, raycastRequest, m_params.pQueryFilter);

				if (raycastResult != MNM::CNavMesh::eRayCastResult_NoHit)
				{
					return false;
				}
			}

			index1 = index2;
			index2 = m_path.FindNextSegmentIndex(i);
		}
	}
	return true;
}

//===================================================================
// IsRemainingPathAffectedByNavMeshChange
//===================================================================
bool CSmartPathFollower::IsRemainingPathAffectedByNavMeshChange(const NavigationMeshID affectedMeshID, const MNM::TileID affectedTileID, bool bAnnotationChange, bool bDataChange) const
{
	const NavigationMeshID meshIDUsedByPath = m_pNavPath->GetMeshID();
	if (meshIDUsedByPath != affectedMeshID)
		return false;

	if (m_pNavPath->Empty())
		return false;
	
	if (IsRemainingPathOverlappingWithNavMeshTileBounds(affectedMeshID, affectedTileID))
	{
		if (bAnnotationChange)
		{
			if (IsRemainingPathAffectedByFilterChange(m_params.pQueryFilter))
			{
				return true;
			}
		}
		if (bDataChange)
		{
			if (!IsRemainingPathTraversableOnNavMesh())
			{
				return true;
			}
		}
	}
	return false;
}

bool CSmartPathFollower::IsRemainingPathAffectedByFilterChange(const INavMeshQueryFilter* pFilter) const
{
	float indexFloat = m_path.FindClosestSegmentIndex(m_curPos, 0.0f, FLT_MAX, FLT_MAX, m_params.use2D);
	size_t index = static_cast<size_t>(indexFloat) + 1;

	if (!m_pNavPath->CanPassFilter(index, pFilter))
		return true;

	return false;
}
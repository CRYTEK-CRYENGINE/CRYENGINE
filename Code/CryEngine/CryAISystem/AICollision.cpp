// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AICollision.h"
#include "CAISystem.h"
#include "DebugDrawContext.h"

static EAICollisionEntities aiCollisionEntitiesTable[] =
{
	AICE_STATIC,
	AICE_ALL,
	AICE_ALL_SOFT,
	AICE_DYNAMIC,
	AICE_STATIC_EXCEPT_TERRAIN,
	AICE_ALL_EXCEPT_TERRAIN,
	AICE_ALL_INLUDING_LIVING
};

IPhysicalEntity* g_AIEntitiesInBoxPreAlloc[GetPhysicalEntitiesInBoxMaxResultCount];

// For automatic cleanup of memory allocated by physics
// We could have a static buffer also, at the cost of a little complication - physics memory is still needed if static buffer too small
void PhysicalEntityListAutoPtr::operator=(IPhysicalEntity** pList)
{
	if (m_pList)
		gEnv->pPhysicalWorld->GetPhysUtils()->DeletePointer(m_pList);
	m_pList = pList;
}

//====================================================================
// IntersectSweptSphere
// hitPos is optional - may be faster if 0
//====================================================================
bool IntersectSweptSphere(Vec3* hitPos, float& hitDist, const Lineseg& lineseg, float radius, EAICollisionEntities aiCollisionEntities, IPhysicalEntity** pSkipEnts, int nSkipEnts, int geomFlagsAny)
{
	primitives::sphere spherePrim;
	spherePrim.center = lineseg.start;
	spherePrim.r = radius;

	Vec3 dir = lineseg.end - lineseg.start;

	geom_contact* pContact = 0;
	geom_contact** ppContact = hitPos ? &pContact : 0;
	int geomFlagsAll = 0;

	float d = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(spherePrim.type, &spherePrim, dir,
	                                                           aiCollisionEntities, ppContact,
	                                                           geomFlagsAll, geomFlagsAny, 0, 0, 0, pSkipEnts, nSkipEnts);

	if (d > 0.0f)
	{
		hitDist = d;
		if (pContact && hitPos)
			*hitPos = pContact->pt;
		return true;
	}
	else
	{
		return false;
	}
}

//====================================================================
// IntersectSweptSphere
//====================================================================
bool IntersectSweptSphere(Vec3* hitPos, float& hitDist, const Lineseg& lineseg, float radius, const std::vector<IPhysicalEntity*>& entities)
{
	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;
	primitives::sphere spherePrim;
	spherePrim.center = lineseg.start;
	spherePrim.r = radius;

	Vec3 dir = lineseg.end - lineseg.start;

	ray_hit hit;
	unsigned nEntities = entities.size();
	hitDist = std::numeric_limits<float>::max();
	for (unsigned iEntity = 0; iEntity < nEntities; ++iEntity)
	{
		IPhysicalEntity* pEntity = entities[iEntity];
		if (pPhysics->CollideEntityWithBeam(pEntity, lineseg.start, dir, radius, &hit))
		{
			if (hit.dist < hitDist)
			{
				if (hitPos)
					*hitPos = hit.pt;
				hitDist = hit.dist;
			}
		}
	}
	return hitDist < std::numeric_limits<float>::max();
}

//====================================================================
// OverlapCylinder
//====================================================================
bool OverlapCylinder(const Lineseg& lineseg, float radius, const std::vector<IPhysicalEntity*>& entities)
{
	intersection_params ip;
	ip.bStopAtFirstTri = true;
	ip.bNoAreaContacts = true;
	ip.bNoIntersection = 1;
	ip.bNoBorder = true;

	primitives::cylinder cylinderPrim;
	cylinderPrim.center = 0.5f * (lineseg.start + lineseg.end);
	cylinderPrim.axis = lineseg.end - lineseg.start;
	cylinderPrim.hh = 0.5f * cylinderPrim.axis.NormalizeSafe(Vec3Constants<float>::fVec3_OneZ);
	cylinderPrim.r = radius;

	ray_hit hit;
	unsigned nEntities = entities.size();
	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;
	const Vec3 vZero(ZERO);
	for (unsigned iEntity = 0; iEntity < nEntities; ++iEntity)
	{
		IPhysicalEntity* pEntity = entities[iEntity];
		if (pPhysics->CollideEntityWithPrimitive(pEntity, cylinderPrim.type, &cylinderPrim, vZero, &hit, &ip))
			return true;
	}
	return false;
}

//====================================================================
// FindFloor
//====================================================================
bool FindFloor(const Vec3& position, Vec3& floor)
{
	const Vec3 dir = Vec3(0.0f, 0.0f, -(WalkabilityFloorDownDist + WalkabilityFloorUpDist));
	const Vec3 start = position + Vec3(0, 0, WalkabilityFloorUpDist);

	const RayCastResult& result = gAIEnv.pRayCaster->Cast(RayCastRequest(start, dir, AICE_ALL,
		rwi_stop_at_pierceable | rwi_colltype_any(geom_colltype_player)));

	if (!result || (result[0].dist < 0.0f))
	{
		return false;
	}

	floor = Vec3(start.x, start.y, start.z - result[0].dist);
	return true;
}

//===================================================================
// IsLeft: tests if a point is Left|On|Right of an infinite line.
//    Input:  three points P0, P1, and P2
//    Return: >0 for P2 left of the line through P0 and P1
//            =0 for P2 on the line
//            <0 for P2 right of the line
//===================================================================
inline float IsLeft(Vec3 P0, Vec3 P1, const Vec3& P2)
{
	bool swap = false;
	if (P0.x < P1.x)
		swap = true;
	else if (P0.x == P1.x && P0.y < P1.y)
		swap = true;

	if (swap)
		std::swap(P0, P1);

	float res = (P1.x - P0.x) * (P2.y - P0.y) - (P2.x - P0.x) * (P1.y - P0.y);
	const float tol = 0.0000f;
	if (res > tol || res < -tol)
		return swap ? -res : res;
	else
		return 0.0f;
}

inline bool ptEqual(const Vec3& lhs, const Vec3& rhs)
{
	const float tol = 0.01f;
	return (fabs(lhs.x - rhs.x) < tol) && (fabs(lhs.y - rhs.y) < tol);
}

#if 0
struct SPointSorter
{
	SPointSorter(const Vec3& pt) : pt(pt) {}
	bool operator()(const Vec3& lhs, const Vec3& rhs)
	{
		float isLeft = IsLeft(pt, lhs, rhs);
		if (isLeft > 0.0f)
			return true;
		else if (isLeft < 0.0f)
			return false;
		else
			return (lhs - pt).GetLengthSquared2D() < (rhs - pt).GetLengthSquared2D();
	}
	const Vec3 pt;
};

//===================================================================
// ConvexHull2D
// Implements Graham's scan
//===================================================================
void ConvexHull2DGraham(std::vector<Vec3>& ptsOut, const std::vector<Vec3>& ptsIn)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	const unsigned nPtsIn = ptsIn.size();
	if (nPtsIn < 3)
	{
		ptsOut = ptsIn;
		return;
	}
	unsigned iBotRight = 0;
	for (unsigned iPt = 1; iPt < nPtsIn; ++iPt)
	{
		if (ptsIn[iPt].y < ptsIn[iBotRight].y)
			iBotRight = iPt;
		else if (ptsIn[iPt].y == ptsIn[iBotRight].y && ptsIn[iPt].x < ptsIn[iBotRight].x)
			iBotRight = iPt;
	}

	static std::vector<Vec3> ptsSorted; // avoid memory allocation
	ptsSorted.assign(ptsIn.begin(), ptsIn.end());

	std::swap(ptsSorted[0], ptsSorted[iBotRight]);
	{
		CRY_PROFILE_REGION(PROFILE_AI, "SORT Graham");
		std::sort(ptsSorted.begin() + 1, ptsSorted.end(), SPointSorter(ptsSorted[0]));
	}
	ptsSorted.erase(std::unique(ptsSorted.begin(), ptsSorted.end(), ptEqual), ptsSorted.end());

	const unsigned nPtsSorted = ptsSorted.size();
	if (nPtsSorted < 3)
	{
		ptsOut = ptsSorted;
		return;
	}

	ptsOut.resize(0);
	ptsOut.push_back(ptsSorted[0]);
	ptsOut.push_back(ptsSorted[1]);
	unsigned int i = 2;
	while (i < nPtsSorted)
	{
		if (ptsOut.size() <= 1)
		{
			AIWarning("Badness in ConvexHull2D");
			AILogComment("i = %d ptsIn = ", i);
			for (unsigned j = 0; j < ptsIn.size(); ++j)
				AILogComment("%6.3f, %6.3f, %6.3f", ptsIn[j].x, ptsIn[j].y, ptsIn[j].z);
			AILogComment("ptsSorted = ");
			for (unsigned j = 0; j < ptsSorted.size(); ++j)
				AILogComment("%6.3f, %6.3f, %6.3f", ptsSorted[j].x, ptsSorted[j].y, ptsSorted[j].z);
			ptsOut.resize(0);
			return;
		}
		const Vec3& pt1 = ptsOut[ptsOut.size() - 1];
		const Vec3& pt2 = ptsOut[ptsOut.size() - 2];
		const Vec3& p = ptsSorted[i];
		float isLeft = IsLeft(pt2, pt1, p);
		if (isLeft > 0.0f)
		{
			ptsOut.push_back(p);
			++i;
		}
		else if (isLeft < 0.0f)
		{
			ptsOut.pop_back();
		}
		else
		{
			ptsOut.pop_back();
			ptsOut.push_back(p);
			++i;
		}
	}
}
#endif

inline float IsLeftAndrew(const Vec3& p0, const Vec3& p1, const Vec3& p2)
{
	return (p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y);
}

inline bool PointSorterAndrew(const Vec3& lhs, const Vec3& rhs)
{
	if (lhs.x < rhs.x) return true;
	if (lhs.x > rhs.x) return false;
	return lhs.y < rhs.y;
}

//===================================================================
// ConvexHull2D
// Implements Andrew's algorithm
//
// Copyright 2001, softSurfer (www.softsurfer.com)
// This code may be freely used and modified for any purpose
// providing that this copyright notice is included with it.
// SoftSurfer makes no warranty for this code, and cannot be held
// liable for any real or imagined damage resulting from its use.
// Users of this code must verify correctness for their application.
//===================================================================
static std::vector<Vec3> ConvexHull2DAndrewTemp;

void ConvexHull2DAndrew(std::vector<Vec3>& ptsOut, const std::vector<Vec3>& ptsIn)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	const int n = (int)ptsIn.size();
	if (n < 3)
	{
		ptsOut = ptsIn;
		return;
	}

	std::vector<Vec3>& P = ConvexHull2DAndrewTemp;
	P = ptsIn;

	{
		CRY_PROFILE_REGION(PROFILE_AI, "SORT Andrew");
		std::sort(P.begin(), P.end(), PointSorterAndrew);
	}

	// the output array ptsOut[] will be used as the stack
	int i;

	ptsOut.clear();
	ptsOut.reserve(P.size());

	// Get the indices of points with min x-coord and min|max y-coord
	int minmin = 0, minmax;
	float xmin = P[0].x;
	for (i = 1; i < n; i++)
		if (P[i].x != xmin)
			break;

	minmax = i - 1;
	if (minmax == n - 1)
	{
		// degenerate case: all x-coords == xmin
		ptsOut.push_back(P[minmin]);
		if (P[minmax].y != P[minmin].y) // a nontrivial segment
			ptsOut.push_back(P[minmax]);
		ptsOut.push_back(P[minmin]);           // add polygon endpoint
		return;
	}

	// Get the indices of points with max x-coord and min|max y-coord
	int maxmin, maxmax = n - 1;
	float xmax = P[n - 1].x;
	for (i = n - 2; i >= 0; i--)
		if (P[i].x != xmax) break;
	maxmin = i + 1;

	// Compute the lower hull on the stack H
	ptsOut.push_back(P[minmin]);      // push minmin point onto stack
	i = minmax;
	while (++i <= maxmin)
	{
		// the lower line joins P[minmin] with P[maxmin]
		if (IsLeftAndrew(P[minmin], P[maxmin], P[i]) >= 0 && i < maxmin)
			continue;          // ignore P[i] above or on the lower line

		while ((int)ptsOut.size() > 1) // there are at least 2 points on the stack
		{
			// test if P[i] is left of the line at the stack top
			if (IsLeftAndrew(ptsOut[ptsOut.size() - 2], ptsOut.back(), P[i]) > 0)
				break;         // P[i] is a new hull vertex
			else
				ptsOut.pop_back(); // pop top point off stack
		}
		ptsOut.push_back(P[i]);       // push P[i] onto stack
	}

	// Next, compute the upper hull on the stack H above the bottom hull
	if (maxmax != maxmin)             // if distinct xmax points
		ptsOut.push_back(P[maxmax]);    // push maxmax point onto stack
	int bot = (int)ptsOut.size() - 1; // the bottom point of the upper hull stack
	i = maxmin;
	while (--i >= minmax)
	{
		// the upper line joins P[maxmax] with P[minmax]
		if (IsLeftAndrew(P[maxmax], P[minmax], P[i]) >= 0 && i > minmax)
			continue;          // ignore P[i] below or on the upper line

		while ((int)ptsOut.size() > bot + 1)    // at least 2 points on the upper stack
		{
			// test if P[i] is left of the line at the stack top
			if (IsLeftAndrew(ptsOut[ptsOut.size() - 2], ptsOut.back(), P[i]) > 0)
				break;         // P[i] is a new hull vertex
			else
				ptsOut.pop_back();         // pop top po2int off stack
		}
		ptsOut.push_back(P[i]);       // push P[i] onto stack
	}
	if (minmax != minmin)
		ptsOut.push_back(P[minmin]);  // push joining endpoint onto stack
	if (!ptsOut.empty() && ptEqual(ptsOut.front(), ptsOut.back()))
		ptsOut.pop_back();
}

//===================================================================
// GetFloorRectangleFromOrientedBox
//===================================================================
void GetFloorRectangleFromOrientedBox(const Matrix34& tm, const AABB& box, SAIRect3& rect)
{
	Matrix33 tmRot(tm);
	tmRot.Transpose();

	Vec3 corners[8];
	SetAABBCornerPoints(box, corners);

	rect.center = tm.GetTranslation();

	rect.axisu = tm.GetColumn1();
	rect.axisu.z = 0;
	rect.axisu.Normalize();
	rect.axisv.Set(rect.axisu.y, -rect.axisu.x, 0);

	Vec3 tu = tmRot.TransformVector(rect.axisu);
	Vec3 tv = tmRot.TransformVector(rect.axisv);

	rect.min.x = FLT_MAX;
	rect.min.y = FLT_MAX;
	rect.max.x = -FLT_MAX;
	rect.max.y = -FLT_MAX;

	for (unsigned i = 0; i < 8; ++i)
	{
		float du = tu.Dot(corners[i]);
		float dv = tv.Dot(corners[i]);
		rect.min.x = min(rect.min.x, du);
		rect.max.x = max(rect.max.x, du);
		rect.min.y = min(rect.min.y, dv);
		rect.max.y = max(rect.max.y, dv);
	}
}

void CleanupAICollision()
{
	stl::free_container(ConvexHull2DAndrewTemp);
}

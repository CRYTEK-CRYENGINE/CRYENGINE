// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   PathObstacles.cpp
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - ?

 *********************************************************************/

#include "StdAfx.h"
#include "PathObstacles.h"
#include "AICollision.h"
#include "DebugDrawContext.h"
#include "PipeUser.h"
#include "NavPath.h"
#include "AIHash.h"

#include <Cry3DEngine/I3DEngine.h>

#include "Navigation/MNM/MNM.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"

#include <numeric>

#if defined(GetObject)
	#undef GetObject
#endif

static float criticalTopAlt = 0.5f;
static float criticalBaseAlt = 2.0f;

static std::vector<Vec3> s_pts;
static std::vector<int> s_addedObstacleIndices;

TPathObstacles CPathObstacles::s_pathObstacles;
CPathObstacles::TCachedDynamicObstacleFlags CPathObstacles::s_dynamicObstacleFlags;

// NOTE Jun 4, 2007: <pvl> the comparison operators are used just for debugging ATM.
inline bool operator==(const SPathObstacleCircle2D& lhs, const SPathObstacleCircle2D& rhs)
{
	return lhs.center == rhs.center && lhs.radius == rhs.radius;
}

inline bool operator==(const SPathObstacleShape2D& lhs, const SPathObstacleShape2D& rhs)
{
	return IsEquivalent(lhs.aabb, rhs.aabb, 0.0f) && lhs.pts == rhs.pts;
}

bool operator==(const CPathObstacle& lhs, const CPathObstacle& rhs)
{
	if (lhs.GetType() != rhs.GetType())
		return false;

	CPathObstacle::EPathObstacleType type = lhs.GetType();

	switch (type)
	{
	case CPathObstacle::ePOT_Circle2D:
		return lhs.GetCircle2D() == rhs.GetCircle2D();
		break;
	case CPathObstacle::ePOT_Shape2D:
		return lhs.GetShape2D() == rhs.GetShape2D();
		break;
	default:
		AIAssert(0 && "Unhandled obstacle type in operator==(CPathObstacle, CPathObstacle)");
		break;
	}
	return false;
}

inline bool operator==(const TPathObstacles& lhs, const TPathObstacles& rhs)
{
	if (lhs.size() != rhs.size())
		return false;

	const int numElems = lhs.size();

	for (int i = 0; i < numElems; ++i)
		if (lhs[i] != rhs[i])
			return false;

	return true;
}

// NOTE Jun 4, 2007: <pvl> for consistency checking while debugging
unsigned int SPathObstacleShape2D::GetHash() const
{
	unsigned int hash = 0;
	TVectorOfVectors::const_iterator it = pts.begin();
	TVectorOfVectors::const_iterator end = pts.end();
	for (; it != end; ++it)
		hash += HashFromVec3(*it, 0.0f, 1.0f);
	return hash;
}

unsigned int SPathObstacleCircle2D::GetHash() const
{
	return HashFromVec3(center, 0.0f, 1.0f) + HashFromFloat(radius, 0.0f, 1.0f);
}

unsigned int CPathObstacle::GetHash() const
{
	unsigned int hash = HashFromFloat(float (m_type), 0.0f, 1.0f);

	switch (m_type)
	{
	case CPathObstacle::ePOT_Circle2D:
		return hash + GetCircle2D().GetHash();
		break;
	case CPathObstacle::ePOT_Shape2D:
		return hash + GetShape2D().GetHash();
		break;
	default:
		AIAssert(0 && "Unhandled obstacle type in CPathObstacle::GetHash()");
		break;
	}
	return 0;
}
/*
   static unsigned int GetHash (const TPathObstacles & obstacles)
   {
   unsigned int hash = 0;

   TPathObstacles::const_iterator it = obstacles.begin ();
   TPathObstacles::const_iterator end = obstacles.end ();
   for ( ; it != end; ++it)
    hash += (*it)->GetHash ();
   return hash;
   }
 */
static void ClearObstacles(TPathObstacles& obstacles)
{
	obstacles.clear();
}

static void DeepCopyObstacles(TPathObstacles& dst, const TPathObstacles& src)
{
	for (uint32 i = 0; i < src.size(); ++i)
	{
		dst.push_back(new CPathObstacle(*src[i].get()));
	}
}

static bool ObstacleDrawingIsOnForActor(const CAIActor* pAIActor)
{
	if (gAIEnv.CVars.DebugDraw > 0)
	{
		const char* pathName = gAIEnv.CVars.DrawPathAdjustment;
		if (*pathName && (!strcmp(pathName, "all") || (pAIActor && !strcmp(pAIActor->GetName(), pathName))))
			return true;
	}

	return false;
}

//===================================================================
// CPathObstacle
//===================================================================
CPathObstacle::CPathObstacle(EPathObstacleType type) : m_type(type)
{
	switch (m_type)
	{
	case ePOT_Circle2D:
		m_pData = new SPathObstacleCircle2D;
		break;
	case ePOT_Shape2D:
		m_pData = new SPathObstacleShape2D;
		break;
	case ePOT_Unset:
		m_pData = 0;
		break;
	default:
		m_pData = 0;
		AIError("CPathObstacle: Unhandled type: %d", type);
		break;
	}
}

//===================================================================
// CPathObstacle
//===================================================================
CPathObstacle::~CPathObstacle()
{
	Free();
}

void CPathObstacle::Free()
{
	if (m_pData)
	{
		switch (m_type)
		{
		case ePOT_Circle2D:
			delete &GetCircle2D();
			break;
		case ePOT_Shape2D:
			delete &GetShape2D();
			break;
		default:
			AIError("~CPathObstacle Unhandled type: %d", m_type);
			break;
		}
		m_type = ePOT_Unset;
		m_pData = 0;
	}
}

//===================================================================
// operator=
//===================================================================
CPathObstacle& CPathObstacle::operator=(const CPathObstacle& other)
{
	if (this == &other)
		return *this;
	Free();
	m_type = other.GetType();
	switch (m_type)
	{
	case ePOT_Circle2D:
		m_pData = new SPathObstacleCircle2D;
		*((SPathObstacleCircle2D*) m_pData) = other.GetCircle2D();
		break;
	case ePOT_Shape2D:
		m_pData = new SPathObstacleShape2D;
		*((SPathObstacleShape2D*) m_pData) = other.GetShape2D();
		break;
	default:
		AIError("CPathObstacle::operator= Unhandled type: %d", m_type);
		return *this;
	}
	return *this;
}

//===================================================================
// CPathObstacle
//===================================================================
CPathObstacle::CPathObstacle(const CPathObstacle& other)
{
	m_type = ePOT_Unset;
	m_pData = 0;
	*this = other;
}

//===================================================================
// SCachedObstacle
//===================================================================
//#define CHECK_CACHED_OBST_CONSISTENCY
struct SCachedObstacle
{
	void Reset()
	{
		entity = 0;
		entityHash = 0;
		extraRadius = 0.0f;
		shapes.clear();
#ifdef CHECK_CACHED_OBST_CONSISTENCY
		shapesHash = 0;
#endif
		debugBoxes.clear();
		stl::free_container(shapes);
	}

	IPhysicalEntity*                       entity;
	unsigned                               entityHash;
	float                                  extraRadius;
	TPathObstacles                         shapes;
#ifdef CHECK_CACHED_OBST_CONSISTENCY
	unsigned                               shapesHash;
#endif
	std::vector<CPathObstacles::SDebugBox> debugBoxes;
};

/// In a suit_demonstration level 32 results in about 80% cache hits
int CPathObstacles::s_obstacleCacheSize = 32;
CPathObstacles::TCachedObstacles CPathObstacles::s_cachedObstacles;

//===================================================================
// GetOrClearCachedObstacle
//===================================================================
SCachedObstacle* CPathObstacles::GetOrClearCachedObstacle(IPhysicalEntity* entity, float extraRadius)
{
	if (s_obstacleCacheSize == 0)
		return 0;

	unsigned entityHash = GetHashFromEntities(&entity, 1);

	const TCachedObstacles::reverse_iterator itEnd = s_cachedObstacles.rend();
	const TCachedObstacles::reverse_iterator itBegin = s_cachedObstacles.rbegin();
	for (TCachedObstacles::reverse_iterator it = s_cachedObstacles.rbegin(); it != itEnd; ++it)
	{
		SCachedObstacle& cachedObstacle = **it;
		if (cachedObstacle.entity != entity || cachedObstacle.extraRadius != extraRadius)
			continue;
		if (cachedObstacle.entityHash == entityHash)
		{
			s_cachedObstacles.erase(it.base() - 1);
			s_cachedObstacles.push_back(&cachedObstacle);
#ifdef CHECK_CACHED_OBST_CONSISTENCY
			if (cachedObstacle.shapesHash != GetHash(cachedObstacle.shapes))
				AIError("corrupted obstacle!");
#endif
			return &cachedObstacle;
		}
		delete *(it.base() - 1);
		s_cachedObstacles.erase(it.base() - 1);
		return 0;
	}
	return 0;
}

//===================================================================
// AddCachedObstacle
//===================================================================
void CPathObstacles::AddCachedObstacle(struct SCachedObstacle* obstacle)
{
	if (s_cachedObstacles.size() == s_obstacleCacheSize)
	{
		delete s_cachedObstacles.front();
		s_cachedObstacles.erase(s_cachedObstacles.begin());
	}
	if (s_obstacleCacheSize > 0)
		s_cachedObstacles.push_back(obstacle);
}

//===================================================================
// PopOldestCachedObstacle
//===================================================================
struct SCachedObstacle* CPathObstacles::GetNewCachedObstacle()
{
	if (s_obstacleCacheSize == 0)
		return 0;
	if ((int)s_cachedObstacles.size() < s_obstacleCacheSize)
		return new SCachedObstacle();
	SCachedObstacle* obs = s_cachedObstacles.front();
	s_cachedObstacles.erase(s_cachedObstacles.begin());
	obs->Reset();
	return obs;
}

//===================================================================
// CombineObstacleShape2DPair
//===================================================================
static bool CombineObstacleShape2DPair(SPathObstacleShape2D& shape1, SPathObstacleShape2D& shape2)
{
	if (!Overlap::Polygon_Polygon2D<TVectorOfVectors>(shape1.pts, shape2.pts, &shape1.aabb, &shape2.aabb))
		return false;

	const float boundingBox1z = shape1.aabb.GetCenter().z;
	const float boundingBox2z = shape2.aabb.GetCenter().z;
	const float maxZDifferenceToCombineObstacles = 1.5f;
	const bool shapesShouldBeCombined = abs(boundingBox1z - boundingBox2z) < maxZDifferenceToCombineObstacles;
	if (shapesShouldBeCombined)
	{
		shape2.pts.insert(shape2.pts.end(), shape1.pts.begin(), shape1.pts.end());
		ConvexHull2D(shape1.pts, shape2.pts);
		shape1.CalcAABB();
		return true;
	}
	return false;
}

//===================================================================
// CombineObstaclePair
// If possible adds the second obstacle (which may become unuseable) to
// the first and returns true.
// If not possible nothing gets modified and returns false
//===================================================================
static bool CombineObstaclePair(CPathObstaclePtr ob1, CPathObstaclePtr ob2)
{
	if (ob1->GetType() != ob2->GetType())
		return false;

	if (ob1->GetType() == CPathObstacle::ePOT_Shape2D)
		return CombineObstacleShape2DPair(ob1->GetShape2D(), ob2->GetShape2D());

	return false;
}

//===================================================================
// CombineObstacles
//===================================================================
static void CombineObstacles(TPathObstacles& combinedObstacles, const TPathObstacles& obstacles)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	combinedObstacles = obstacles;

	for (TPathObstacles::iterator it = combinedObstacles.begin(); it != combinedObstacles.end(); ++it)
	{
		if ((*it)->GetType() == CPathObstacle::ePOT_Shape2D)
			(*it)->GetShape2D().CalcAABB();
	}

StartAgain:
	for (TPathObstacles::iterator it = combinedObstacles.begin(); it != combinedObstacles.end(); ++it)
	{
		for (TPathObstacles::iterator itOther = it + 1; itOther != combinedObstacles.end(); ++itOther)
		{
			if (CombineObstaclePair(*it, *itOther))
			{
				itOther->swap(combinedObstacles.back());
				combinedObstacles.pop_back();
				goto StartAgain;
			}
		}
	}
}

//===================================================================
// SimplifyObstacle
//===================================================================
static void SimplifyObstacle(CPathObstaclePtr ob)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (ob->GetType() == CPathObstacle::ePOT_Circle2D)
	{
		static const float diagScale = 1.0f / sqrtf(2.0f);
		const SPathObstacleCircle2D& circle = ob->GetCircle2D();
		CPathObstacle newOb(CPathObstacle::ePOT_Shape2D);
		SPathObstacleShape2D& shape2D = newOb.GetShape2D();
		shape2D.pts.push_back(circle.center + diagScale * Vec3(-circle.radius, -circle.radius, 0.0f));
		shape2D.pts.push_back(circle.center + Vec3(0.0f, -circle.radius, 0.0f));
		shape2D.pts.push_back(circle.center + diagScale * Vec3(circle.radius, -circle.radius, 0.0f));
		shape2D.pts.push_back(circle.center + Vec3(circle.radius, 0.0f, 0.0f));
		shape2D.pts.push_back(circle.center + diagScale * Vec3(circle.radius, circle.radius, 0.0f));
		shape2D.pts.push_back(circle.center + Vec3(0.0f, circle.radius, 0.0f));
		shape2D.pts.push_back(circle.center + diagScale * Vec3(-circle.radius, circle.radius, 0.0f));
		shape2D.pts.push_back(circle.center + Vec3(-circle.radius, 0.0f, 0.0f));
		*ob = newOb;
	}
}

//===================================================================
// SimplifyObstacles
//===================================================================
static void SimplifyObstacles(TPathObstacles& obstacles)
{
	for (TPathObstacles::iterator it = obstacles.begin(); it != obstacles.end(); ++it)
		SimplifyObstacle(*it);
}

//===================================================================
// AddEntityBoxesToObstacles
//===================================================================

namespace PathObstaclesPredicates
{
float ReturnZAccumulatedValue(const float& accumulatedValue, const Vec3& position)
{
	return accumulatedValue + position.z;
}

struct AssignZValueToVector
{
	AssignZValueToVector(const float zValue)
		: m_zValue(zValue)
	{
	}

	void operator()(Vec3& position)
	{
		position.z = m_zValue;
	}

	const float m_zValue;
};
}

bool CPathObstacles::AddEntityBoxesToObstacles(IPhysicalEntity* entity, TPathObstacles& obstacles,
                                               float extraRadius, float terrainZ, const bool debug) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	static int cacheHits = 0;
	static int cacheMisses = 0;

	static int numPerOutput = 100;
	if (cacheHits + cacheMisses > numPerOutput)
	{
		AILogComment("PathObstacles: cache hits = %d misses = %d", cacheHits, cacheMisses);
		cacheMisses = cacheHits = 0;
	}

	SCachedObstacle* cachedObstacle = GetOrClearCachedObstacle(entity, extraRadius);
	if (cachedObstacle)
	{
#ifdef PATHOBSTACLES_DEBUG
		if (debug)
			m_debugPathAdjustmentBoxes.insert(m_debugPathAdjustmentBoxes.end(), cachedObstacle->debugBoxes.begin(), cachedObstacle->debugBoxes.end());
#endif
		obstacles.insert(obstacles.end(), cachedObstacle->shapes.begin(), cachedObstacle->shapes.end());
		++cacheHits;
		return true;
	}
	++cacheMisses;

	// put the obstacles into a temporary and then combine/copy at the end
	ClearObstacles(s_pathObstacles);

	s_pts.reserve(256);
	s_pts.clear();

	{
		pe_status_nparts statusNParts;
		int nParts = entity->GetStatus(&statusNParts);

		pe_status_pos statusPos;
		if (!entity->GetStatus(&statusPos))
			return false;

		pe_params_part paramsPart;
		for (statusPos.ipart = 0, paramsPart.ipart = 0; statusPos.ipart < nParts; ++statusPos.ipart, ++paramsPart.ipart)
		{
			if (!entity->GetParams(&paramsPart))
				continue;

			if (!(paramsPart.flagsAND & geom_colltype_player))
				continue;

			if (!entity->GetStatus(&statusPos))
				continue;

			if (!statusPos.pGeomProxy)
				continue;

			primitives::box box;
			statusPos.pGeomProxy->GetBBox(&box);

			Vec3 center = box.center * statusPos.scale;
			Vec3 size = box.size * statusPos.scale;
			size += Vec3(extraRadius);

			Vec3 worldCenter = statusPos.pos + statusPos.q * center;
			Matrix33 orientationTM = Matrix33(statusPos.q) * box.Basis.GetTransposed();

			if ((worldCenter.z <= terrainZ + criticalBaseAlt) || (worldCenter.z >= terrainZ + criticalTopAlt))
				worldCenter.z = terrainZ + 0.1f;

			s_pts.push_back(worldCenter + orientationTM * Vec3(size.x, size.y, size.z));
			s_pts.push_back(worldCenter + orientationTM * Vec3(size.x, size.y, -size.z));
			s_pts.push_back(worldCenter + orientationTM * Vec3(size.x, -size.y, size.z));
			s_pts.push_back(worldCenter + orientationTM * Vec3(size.x, -size.y, -size.z));
			s_pts.push_back(worldCenter + orientationTM * Vec3(-size.x, size.y, size.z));
			s_pts.push_back(worldCenter + orientationTM * Vec3(-size.x, size.y, -size.z));
			s_pts.push_back(worldCenter + orientationTM * Vec3(-size.x, -size.y, size.z));
			s_pts.push_back(worldCenter + orientationTM * Vec3(-size.x, -size.y, -size.z));
		}
	}

	// compute the height of all points
	float pointsMinZ = std::numeric_limits<float>::max();
	float pointsMaxZ = std::numeric_limits<float>::min();
	for (std::vector<Vec3>::const_iterator it = s_pts.begin(); it != s_pts.end(); ++it)
	{
		pointsMinZ = std::min(pointsMinZ, it->z);
		pointsMaxZ = std::max(pointsMaxZ, it->z);
	}

	s_pathObstacles.push_back(new CPathObstacle(CPathObstacle::ePOT_Shape2D));
	s_pathObstacles.back()->GetShape2D().minZ = pointsMinZ;
	s_pathObstacles.back()->GetShape2D().maxZ = pointsMaxZ;
	ConvexHull2D(s_pathObstacles.back()->GetShape2D().pts, s_pts);

	std::vector<Vec3>& points = s_pathObstacles.back()->GetShape2D().pts;

	unsigned int pointAmount = points.size();
	if (pointAmount > 0)
	{
		std::for_each(points.begin(), points.end(), PathObstaclesPredicates::AssignZValueToVector(terrainZ));
	}

	AIAssert(!s_pathObstacles.empty());

	obstacles.insert(obstacles.end(), s_pathObstacles.begin(), s_pathObstacles.end());

	// update cache
	cachedObstacle = GetNewCachedObstacle();
	if (cachedObstacle)
	{
		cachedObstacle->entity = entity;
		cachedObstacle->entityHash = GetHashFromEntities(&entity, 1);
		cachedObstacle->extraRadius = extraRadius;

		cachedObstacle->shapes.insert(cachedObstacle->shapes.end(), s_pathObstacles.begin(), s_pathObstacles.end());
#ifdef CHECK_CACHED_OBST_CONSISTENCY
		cachedObstacle->shapesHash = GetHash(cachedObstacle->shapes);
#endif

#ifdef PATHOBSTACLES_DEBUG
		if (debug)
		{
			uint32 origDebugBoxesSize = m_debugPathAdjustmentBoxes.size();
			cachedObstacle->debugBoxes.insert(cachedObstacle->debugBoxes.end(), m_debugPathAdjustmentBoxes.begin() + origDebugBoxesSize, m_debugPathAdjustmentBoxes.end());
		}
#endif

		AddCachedObstacle(cachedObstacle);
	}
	// NOTE Jun 3, 2007: <pvl> mostly for debugging - so that s_pathObstacles
	// doesn't confuse reference counts.  Shouldn't cost more than clearing an
	// already cleared container.  Can be removed if necessary.
	ClearObstacles(s_pathObstacles);

	return true;
}

//===================================================================
// SSphereSorter
// Arbitrary sort order - but ignores z.
//===================================================================
struct SSphereSorter
{
	bool operator()(const Sphere& lhs, const Sphere& rhs) const
	{
		if (lhs.center.x < rhs.center.x)
			return true;
		else if (lhs.center.x > rhs.center.x)
			return false;
		else if (lhs.center.y < rhs.center.y)
			return true;
		else if (lhs.center.y > rhs.center.y)
			return false;
		else if (lhs.radius < rhs.radius)
			return true;
		else if (lhs.radius > rhs.radius)
			return false;
		else
			return false;
	}
};

//===================================================================
// SSphereEq
//===================================================================
struct SSphereEq
{
	bool operator()(const Sphere& lhs, const Sphere& rhs) const
	{
		return IsEquivalent(lhs.center, rhs.center, 0.1f) && fabs(lhs.radius - rhs.radius) < 0.1f;
	}
};

//===================================================================
// IsInNavigationMesh
//===================================================================
bool IsInNavigationMesh(const NavigationMeshID meshID, const Vec3& point, const float verticalRangeMeters, const float horizontalRangeMeters)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	return gAIEnv.pNavigationSystem->IsLocationInMesh(meshID, point);
}

//===================================================================
// GetPathObstacles_AIObject
//===================================================================
void CPathObstacles::GetPathObstacles_AIObject(CAIObject* pObject, SPathObstaclesInfo& pathObstaclesInfo, float maximumDistanceToPath) const
{
	assert(pObject);

	// Special case - Ignore actors who are inside vehicles
	CAIActor* pActor = pObject->CastToCAIActor();
	IAIActorProxy* pActorProxy = (pActor ? pActor->GetProxy() : NULL);
	if (!pActorProxy || pActorProxy->GetLinkedVehicleEntityId() == 0)
	{
		CAIActor::ENavInteraction navInteraction = pathObstaclesInfo.pAIActor ? CAIActor::GetNavInteraction(pathObstaclesInfo.pAIActor, pObject) : CAIActor::NI_IGNORE;
		if (navInteraction == CAIActor::NI_STEER)
		{
			static const float maxSpeedSq = square(0.3f);
			if (pObject->GetVelocity().GetLengthSquared() <= maxSpeedSq)
			{
				const NavigationMeshID meshID = pathObstaclesInfo.pNavPath->GetMeshID();
				const bool usingMNM = (meshID != NavigationMeshID(0));
				const Vec3 objectPos = pObject->GetPhysicsPos();

				const bool considerObject = IsInNavigationMesh(meshID, objectPos, 2.0f, pathObstaclesInfo.minAvRadius + 0.5f);
				if (considerObject)
				{
					Vec3 pathPos(ZERO);
					float distAlongPath = 0.0f;
					const float distToPath = pathObstaclesInfo.pNavPath->GetDistToPath(pathPos, distAlongPath, objectPos, pathObstaclesInfo.maxDistToCheckAhead, true);
					if (distToPath >= 0.0f && distToPath <= maximumDistanceToPath && distToPath <= pathObstaclesInfo.maxPathDeviation)
					{
						//FIXME: Don't allocate at run-time for something like this...
						CPathObstaclePtr pPathObstacle = new CPathObstacle(CPathObstacle::ePOT_Circle2D);
						SPathObstacleCircle2D& pathObstacleCircle2D = pPathObstacle->GetCircle2D();
						pathObstacleCircle2D.center = objectPos;
						pathObstacleCircle2D.radius = pathObstaclesInfo.minAvRadius + 0.5f;

						pathObstaclesInfo.dynamicObstacleSpheres.push_back(Sphere(pathObstacleCircle2D.center, pathObstacleCircle2D.radius));

						pathObstaclesInfo.foundObjectsAABB.Add(objectPos, pathObstaclesInfo.minAvRadius + pObject->GetRadius());
						pathObstaclesInfo.outObstacles.push_back(pPathObstacle);
					}
				}
			}
		}
	}
}

//===================================================================
// GetPathObstacles_Vehicle
//===================================================================
void CPathObstacles::GetPathObstacles_Vehicle(CAIObject* pObject, SPathObstaclesInfo& pathObstaclesInfo) const
{
	assert(pObject);

	IPhysicalEntity* pPhysicalEntity = pObject->GetPhysics();
	if (pPhysicalEntity)
	{
		bool bIgnore = false;

		// Ignore all vehicles if we don't care about them, per our avoidance ability, so they are ignored later! (See below)
		if ((pathObstaclesInfo.movementAbility.avoidanceAbilities & eAvoidance_Vehicles) != eAvoidance_Vehicles)
		{
			bIgnore = true;
		}
		else
		{
			static const float maxSpeedSq = square(0.3f);
			CAIActor::ENavInteraction navInteraction = pathObstaclesInfo.pAIActor ? CAIActor::GetNavInteraction(pathObstaclesInfo.pAIActor, pObject) : CAIActor::NI_IGNORE;
			if (navInteraction != CAIActor::NI_STEER || pObject->GetVelocity().GetLengthSquared() > maxSpeedSq)
			{
				bIgnore = true;
			}
		}

		// Ignoring it means we treat is as if it was checked, so physical entity check will skip it later.
		// Not ignoring means we want to use its physical check, so we get an accurate hull shape for it.
		stl::push_back_unique(bIgnore ? pathObstaclesInfo.checkedPhysicsEntities : pathObstaclesInfo.queuedPhysicsEntities, pPhysicalEntity);
	}
}

//===================================================================
// GetPathObstacles_PhysicalEntity
//===================================================================
void CPathObstacles::GetPathObstacles_PhysicalEntity(IPhysicalEntity* pPhysicalEntity,
                                                     SPathObstaclesInfo& pathObstaclesInfo, bool bIsPushable, float fCullShapeScale,
                                                     const CNavPath& navPath) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	assert(pPhysicalEntity);

	if (pPhysicalEntity && !stl::find(pathObstaclesInfo.checkedPhysicsEntities, pPhysicalEntity))
	{
		pe_params_bbox params_bbox;
		pPhysicalEntity->GetParams(&params_bbox);

		const Vec3& bboxMin = params_bbox.BBox[0];
		const Vec3& bboxMax = params_bbox.BBox[1];

		const Vec3 testPosition = 0.5f * (bboxMin + bboxMax);

		// If using MNM and the object was of the type considered for the mesh regeneration
		// then skip it from the obstacle calculation
		bool isConsideredInMNMGeneration = NavigationSystemUtils::IsDynamicObjectPartOfTheMNMGenerationProcess(pPhysicalEntity);
		if (isConsideredInMNMGeneration)
			return;

		const NavigationMeshID meshID = pathObstaclesInfo.pNavPath->GetMeshID();

		bool usedAsDynamicObstacle = IsPhysicalEntityUsedAsDynamicObstacle(pPhysicalEntity);
		const bool considerObject = usedAsDynamicObstacle && IsInNavigationMesh(meshID, testPosition, (bboxMax.z - bboxMin.z) * 0.5f, (bboxMax.x - bboxMin.x) * 0.5f);

		if (considerObject)
		{
			const float boxRadius = fCullShapeScale * 0.5f * Distance::Point_Point(params_bbox.BBox[0], params_bbox.BBox[1]);
			const float downwardsCheckDistance = (params_bbox.BBox[1].z - params_bbox.BBox[0].z) + 0.1f;  // allow checking further 10cm into the ground, just to be on the safe side

			Vec3 closestPointOnMesh(ZERO);
			if (!gAIEnv.pNavigationSystem->GetClosestMeshLocation(meshID, testPosition, downwardsCheckDistance, 2 * boxRadius, nullptr, &closestPointOnMesh, nullptr))
				return;

			const float groundZ = closestPointOnMesh.z; // p3DEngine->GetTerrainEleva|tion(testPosition.x, testPosition.y);
			const float altTop = params_bbox.BBox[1].z - groundZ;
			const float altBase = params_bbox.BBox[0].z - groundZ;

			if (altTop >= criticalTopAlt && altBase <= criticalBaseAlt)
			{

				Vec3 pathPos(ZERO);
				float distAlongPath = 0.0f;
				const float distToPath = pathObstaclesInfo.pNavPath->GetDistToPath(pathPos, distAlongPath, testPosition, pathObstaclesInfo.maxDistToCheckAhead, true);
				if (distToPath >= 0.0f && distToPath <= pathObstaclesInfo.maxPathDeviation)
				{
					const bool obstacleIsSmall = boxRadius < gAIEnv.CVars.ObstacleSizeThreshold;
					const int actorType = pathObstaclesInfo.pAIActor ? pathObstaclesInfo.pAIActor->GetType() : AIOBJECT_ACTOR;

					float extraRadius = (actorType == AIOBJECT_VEHICLE && obstacleIsSmall ? gAIEnv.CVars.ExtraVehicleAvoidanceRadiusSmall : pathObstaclesInfo.minAvRadius);
					if (bIsPushable && pathObstaclesInfo.movementAbility.pushableObstacleWeakAvoidance)
					{
						// Partial avoidance - scale down radius
						extraRadius = pathObstaclesInfo.movementAbility.pushableObstacleAvoidanceRadius;
					}
					const bool debug = ObstacleDrawingIsOnForActor(pathObstaclesInfo.pAIActor);
					if (AddEntityBoxesToObstacles(pPhysicalEntity, pathObstaclesInfo.outObstacles, extraRadius, groundZ, debug))
					{
						pathObstaclesInfo.foundObjectsAABB.Add(testPosition, pathObstaclesInfo.minAvRadius + boxRadius);
						pathObstaclesInfo.dynamicObstacleSpheres.push_back(Sphere(testPosition, boxRadius));
					}
				}
			}
		}
	}
}

//===================================================================
// GetPathObstacles_DamageRegion
//===================================================================
void CPathObstacles::GetPathObstacles_DamageRegion(const Sphere& damageRegionSphere, SPathObstaclesInfo& pathObstaclesInfo) const
{
	Vec3 pathPos(ZERO);
	float distAlongPath = 0.0f;
	float distToPath = pathObstaclesInfo.pNavPath->GetDistToPath(pathPos, distAlongPath, damageRegionSphere.center, pathObstaclesInfo.maxDistToCheckAhead, true);
	if (distToPath >= 0.0f && distToPath <= pathObstaclesInfo.maxPathDeviation)
	{
		static float safetyRadius = 2.0f;
		const float actualRadius = safetyRadius + damageRegionSphere.radius * 1.5f; // game code actually uses AABB

		//FIXME: Don't allocate at run-time for something like this...
		CPathObstaclePtr pPathObstacle = new CPathObstacle(CPathObstacle::ePOT_Circle2D);
		SPathObstacleCircle2D& pathObstacleCircle2D = pPathObstacle->GetCircle2D();
		pathObstacleCircle2D.center = damageRegionSphere.center;
		pathObstacleCircle2D.radius = actualRadius;

		pathObstaclesInfo.dynamicObstacleSpheres.push_back(Sphere(damageRegionSphere.center, damageRegionSphere.radius * 1.5f));

		pathObstaclesInfo.foundObjectsAABB.Add(damageRegionSphere.center, actualRadius);
		pathObstaclesInfo.outObstacles.push_back(pPathObstacle);
	}
}

//===================================================================
// IsObstaclePushable
//===================================================================
bool CPathObstacles::IsObstaclePushable(const CAIActor* pAIActor, IPhysicalEntity* pEntity, SPathObstaclesInfo& pathObstaclesInfo, float& outCullShapeScale) const
{
	assert(pAIActor);
	assert(pEntity);

	outCullShapeScale = 1.0f;

	pe_params_flags params_flags;
	pEntity->GetParams(&params_flags);
	bool bIsPushable = ((params_flags.flags & pef_pushable_by_players) == pef_pushable_by_players);

	// Check mass to see if object needs to be fully avoided
	if (bIsPushable)
	{
		pe_status_dynamics status_dynamics;
		pEntity->GetStatus(&status_dynamics);
		const float fMass = status_dynamics.mass;

		const float fPushableMassMin = pathObstaclesInfo.movementAbility.pushableObstacleMassMin;
		const float fPushableMassMax = pathObstaclesInfo.movementAbility.pushableObstacleMassMax;

		if (fMass < fPushableMassMin)
		{
			// bIsPushable stays true
			assert(bIsPushable);
		}
		else if (fMass > fPushableMassMax)
		{
			// Too heavy, so fully avoid it
			outCullShapeScale = 1.0f;
			bIsPushable = false;
		}
		else
		{
			// Somewhere in-between; scale the cull shape to try to avoid it
			const float fPushableMassRange = fPushableMassMax - fPushableMassMin;
			outCullShapeScale = (fPushableMassRange > FLT_EPSILON ? (fMass - fPushableMassMin) * __fres(fPushableMassRange) : 1.0f);
			bIsPushable = false;
		}
	}

	return bIsPushable;
}

//===================================================================
// IsPhysicalEntityUsedAsDynamicObstacle
//===================================================================

bool CPathObstacles::IsPhysicalEntityUsedAsDynamicObstacle(IPhysicalEntity* pPhysicalEntity) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(pPhysicalEntity);
	bool usedAsDynamicObstacle = true;
	IEntity* pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pPhysicalEntity);
	if (pEntity)
	{
		const EntityId entityId = pEntity->GetId();
		TCachedDynamicObstacleFlags::const_iterator it = s_dynamicObstacleFlags.find(entityId);
		if (it != s_dynamicObstacleFlags.end())
		{
			usedAsDynamicObstacle = it->second;
		}
		else
		{
			IScriptTable* pScriptTable = pEntity->GetScriptTable();
			SmartScriptTable pPropertiesTable;
			if (pScriptTable && pScriptTable->GetValue("Properties", pPropertiesTable))
			{
				SmartScriptTable pAITable;
				if (pPropertiesTable->GetValue("AI", pAITable))
					pAITable->GetValue("bUsedAsDynamicObstacle", usedAsDynamicObstacle);
			}

			if (s_dynamicObstacleFlags.size() >= s_maxDynamicObstacleFlags)
			{
				AIWarning("s_dynamicObstacleFlags grows more than %d elements. It will be now cleared.", s_maxDynamicObstacleFlags);
				TCachedDynamicObstacleFlags emptyMap;
				s_dynamicObstacleFlags.swap(emptyMap);
			}

			s_dynamicObstacleFlags[entityId] = usedAsDynamicObstacle;
		}
	}

	return usedAsDynamicObstacle;
}

//===================================================================
// GetPathObstacles
//===================================================================
void CPathObstacles::GetPathObstacles(TPathObstacles& obstacles, const AgentMovementAbility& movementAbility,
                                      const CNavPath* pNavPath, const CAIActor* pAIActor)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	AIAssert(pNavPath);

	ClearObstacles(obstacles);

	if (gAIEnv.CVars.AdjustPathsAroundDynamicObstacles == 0)
		return;

	if (pNavPath->Empty())
		return;

	const bool usingMNM = (pNavPath->GetMeshID() != NavigationMeshID(0));
	const IAISystem::ENavigationType navTypeFilter = usingMNM ? (IAISystem::ENavigationType)(IAISystem::NAV_FLIGHT | IAISystem::NAV_VOLUME) : (IAISystem::ENavigationType)(IAISystem::NAV_FLIGHT | IAISystem::NAV_VOLUME | IAISystem::NAV_UNSET);
	const IAISystem::ENavigationType navType = pNavPath->GetPath().front().navType;

	if (navType & navTypeFilter)
		return;

	// The information relating to this path obstacles calculation
	static const float maxPathDeviation = 15.0f;
	SPathObstaclesInfo pathObstaclesInfo(obstacles, movementAbility, pNavPath, pAIActor, maxPathDeviation);

#ifdef PATHOBSTACLES_DEBUG
	const bool bDebug = ObstacleDrawingIsOnForActor(pAIActor);
	if (bDebug)
		m_debugPathAdjustmentBoxes.resize(0);
#endif

	const float minActorAvRadius = gAIEnv.CVars.MinActorDynamicObstacleAvoidanceRadius;
	const float minVehicleAvRadius = gAIEnv.CVars.ExtraVehicleAvoidanceRadiusBig;

	const int actorType = pAIActor ? pAIActor->GetType() : AIOBJECT_ACTOR;
	pathObstaclesInfo.minAvRadius = 0.125f + max((actorType != AIOBJECT_ACTOR ? minVehicleAvRadius : minActorAvRadius), movementAbility.pathRadius);

	static const float maxSpeedScale = 1.0f;
	pathObstaclesInfo.maxSpeed = movementAbility.movementSpeeds.GetRange(AgentMovementSpeeds::AMS_COMBAT, AgentMovementSpeeds::AMU_RUN).max;

	pathObstaclesInfo.maxDistToCheckAhead = movementAbility.pathRegenIntervalDuringTrace > .0f ?
	                                        maxSpeedScale * pathObstaclesInfo.maxSpeed * movementAbility.pathRegenIntervalDuringTrace :
	                                        pNavPath->GetMaxDistanceFromStart();

	const CPipeUser* pipeUser = pAIActor ? pAIActor->CastToCPipeUser() : NULL;
	if (pipeUser && pipeUser->ShouldConsiderActorsAsPathObstacles())
	{
		//// Check actors
		if ((movementAbility.avoidanceAbilities & eAvoidance_Actors))
		{
			ActorLookUp& lookUp = *gAIEnv.pActorLookUp;
			size_t activeActorCount = lookUp.GetActiveCount();

			for (size_t actorIndex = 0; actorIndex < activeActorCount; ++actorIndex)
			{
				CAIObject* pObject = lookUp.GetActor<CAIObject>(actorIndex);
				assert(pObject);

				if ((pObject != pAIActor) && (pObject->GetType() == AIOBJECT_ACTOR))
				{
					const float maximumActorDistanceToPath = 4.0f;
					GetPathObstacles_AIObject(pObject, pathObstaclesInfo, maximumActorDistanceToPath);
				}
			}
		}
	}

	// Check vehicles (which are also physical entities)
	{
		AutoAIObjectIter pVehicleIter(gAIEnv.pAIObjectManager->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_VEHICLE));
		while (IAIObject* pObject = pVehicleIter->GetObject())
		{
			CAIObject* pCObject = (CAIObject*)pObject;
			assert(pCObject && pCObject->GetType() == AIOBJECT_VEHICLE);

			if (pCObject && pCObject != pAIActor)
				GetPathObstacles_Vehicle(pCObject, pathObstaclesInfo);

			pVehicleIter->Next();
		}
	}

	// Check dynamic physical entities
	{
		AABB obstacleAABB = pNavPath->GetAABB(pathObstaclesInfo.maxDistToCheckAhead);
		obstacleAABB.min -= Vec3(maxPathDeviation, maxPathDeviation, maxPathDeviation);
		obstacleAABB.max += Vec3(maxPathDeviation, maxPathDeviation, maxPathDeviation);

		PhysicalEntityListAutoPtr pEntities;
		const uint32 nEntityCount = GetEntitiesFromAABB(pEntities, obstacleAABB, AICE_DYNAMIC);
		for (uint32 nEntity = 0; nEntity < nEntityCount; ++nEntity)
		{
			IPhysicalEntity* pPhysicalEntity = pEntities[nEntity];
			assert(pPhysicalEntity && pAIActor);

			IPhysicalEntity* pSelfEntity = pAIActor ? pAIActor->GetPhysics(true) : NULL;
			if (pSelfEntity && pSelfEntity == pPhysicalEntity)
				continue;

			if (pPhysicalEntity)
			{
				float fCullShapeScale = 1.0f;
				const bool bIsPushable = IsObstaclePushable(pAIActor, pPhysicalEntity, pathObstaclesInfo, fCullShapeScale);

				if ((bIsPushable && (movementAbility.avoidanceAbilities & eAvoidance_PushableObstacle) == eAvoidance_PushableObstacle) ||
				    (!bIsPushable && (movementAbility.avoidanceAbilities & eAvoidance_StaticObstacle) == eAvoidance_StaticObstacle) ||
				    stl::find(pathObstaclesInfo.queuedPhysicsEntities, pPhysicalEntity))
				{
					GetPathObstacles_PhysicalEntity(pPhysicalEntity, pathObstaclesInfo, bIsPushable, fCullShapeScale, *pNavPath);
				}
			}
		}
	}

	// Check damage regions
	if (movementAbility.avoidanceAbilities & eAvoidance_DamageRegion)
	{
		CAISystem* pAISystem = GetAISystem();
		const CAISystem::TDamageRegions& damageRegions = pAISystem->GetDamageRegions();
		if (!damageRegions.empty())
		{
			CAISystem::TDamageRegions::const_iterator itDamageRegion = damageRegions.begin();
			CAISystem::TDamageRegions::const_iterator itDamageRegionEnd = damageRegions.end();
			for (; itDamageRegion != itDamageRegionEnd; ++itDamageRegion)
			{
				const Sphere& damageRegionSphere = itDamageRegion->second;
				GetPathObstacles_DamageRegion(damageRegionSphere, pathObstaclesInfo);
			}
		}
	}
}

//===================================================================
// CPathObstacles
//===================================================================
CPathObstacles::CPathObstacles()
	: m_lastCalculateTime(0.0f), m_lastCalculatePos(ZERO), m_lastCalculatePathVersion(-1)
{
}

//===================================================================
// CPathObstacles
//===================================================================
CPathObstacles::~CPathObstacles()
{
}

//===================================================================
// CalculateObstaclesAroundActor
//===================================================================
void CPathObstacles::CalculateObstaclesAroundActor(const CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	IF_UNLIKELY (!pPipeUser)
		return;

	const float criticalDist = 2.0f;
	const int64 criticalTimeMs = 2000;

	CTimeValue now(GetAISystem()->GetFrameStartTime());
	int64 deltaTimeMs = (now - m_lastCalculateTime).GetMilliSecondsAsInt64();

	Vec3 pipeUserPos = pPipeUser->GetPos();

	if ((deltaTimeMs < criticalTimeMs)
	    && (pPipeUser->m_Path.GetVersion() == m_lastCalculatePathVersion)
	    && pipeUserPos.IsEquivalent(m_lastCalculatePos, criticalDist))
	{
		return;
	}

	m_lastCalculatePos = pipeUserPos;
	m_lastCalculateTime = now;

	m_lastCalculatePathVersion = pPipeUser->m_Path.GetVersion();

	ClearObstacles(m_combinedObstacles);
	ClearObstacles(m_simplifiedObstacles);

	GetPathObstacles(m_simplifiedObstacles, pPipeUser->m_movementAbility, &pPipeUser->m_Path, pPipeUser);

	SimplifyObstacles(m_simplifiedObstacles);

	// NOTE Mai 24, 2007: <pvl> CombineObstacles() messes up its second argument
	// so if we're going to debug draw simplified obstacles later, we need to
	// keep m_simplifiedObstacles intact by passing a copy to CombineObstacles().
	// TPathObstacles is a container of smart pointers so we need to perform
	// a proper deep copy.
	// UPDATE Jun 4, 2007: <pvl> in fact, both arguments of CombineObstacleShape2DPair()
	// can get changed (the first one gets the convex hull of both and the second one
	// contains all the points).  So if any obstacle in m_simplifiedObstacles
	// comes from the cache and overlaps with anything else, it will be corrupted.
	// So let's just perform a deep copy here every time.
	TPathObstacles tempSimplifiedObstacles;
	tempSimplifiedObstacles.reserve(m_simplifiedObstacles.size());

	DeepCopyObstacles(tempSimplifiedObstacles, m_simplifiedObstacles);
	CombineObstacles(m_combinedObstacles, tempSimplifiedObstacles);
}

//===================================================================
// CalculateObstaclesAroundLocation
//===================================================================
void CPathObstacles::CalculateObstaclesAroundLocation(const Vec3& location, const AgentMovementAbility& movementAbility, const CNavPath* pNavPath)
{
	const float criticalDist = 2.0f;
	const int64 criticalTimeMs = 2000;

	CTimeValue now(GetAISystem()->GetFrameStartTime());
	int64 deltaTimeMs = (now - m_lastCalculateTime).GetMilliSecondsAsInt64();

	// Danny todo: this is/would be a nice optimisation to make - however when finding hidespots
	// we get here before a path is calculated, so the path obstacle list is invalid (since it
	// depends on the path).
	if (deltaTimeMs < criticalTimeMs && pNavPath->GetVersion() == m_lastCalculatePathVersion &&
	    location.IsEquivalent(m_lastCalculatePos, criticalDist))
		return;

	m_lastCalculatePos = location;
	m_lastCalculateTime = now;
	m_lastCalculatePathVersion = pNavPath->GetVersion();

	ClearObstacles(m_combinedObstacles);
	ClearObstacles(m_simplifiedObstacles);

	GetPathObstacles(m_simplifiedObstacles, movementAbility, pNavPath, 0);
	SimplifyObstacles(m_simplifiedObstacles);

	// NOTE Mai 24, 2007: <pvl> CombineObstacles() messes up its second argument
	// so if we're going to debug draw simplified obstacles later, we need to
	// keep m_simplifiedObstacles intact by passing a copy to CombineObstacles().
	// TPathObstacles is a container of smart pointers so we need to perform
	// a proper deep copy.
	// UPDATE Jun 4, 2007: <pvl> in fact, both arguments of CombineObstacleShape2DPair()
	// can get changed (the first one gets the convex hull of both and the second one
	// contains all the points).  So if any obstacle in m_simplifiedObstacles
	// comes from the cache and overlaps with anything else, it will be corrupted.
	// So let's just perform a deep copy here every time.

	TPathObstacles tempSimplifiedObstacles;
	tempSimplifiedObstacles.reserve(m_simplifiedObstacles.size());

	DeepCopyObstacles(tempSimplifiedObstacles, m_simplifiedObstacles);
	// ATTN Jun 1, 2007: <pvl> costly!  Please remind me to remove this if I forget somehow! :)
	//AIAssert (tempSimplifiedObstacles == m_simplifiedObstacles);
	CombineObstacles(m_combinedObstacles, tempSimplifiedObstacles);

}

//===================================================================
// IsPointInsideObstacles
//===================================================================
bool CPathObstacles::IsPointInsideObstacles(const Vec3& pt) const
{
	for (TPathObstacles::const_iterator it = m_combinedObstacles.begin(); it != m_combinedObstacles.end(); ++it)
	{
		const CPathObstacle& ob = **it;
		if (ob.GetType() != CPathObstacle::ePOT_Shape2D)
			continue;

		const SPathObstacleShape2D& shape2D = ob.GetShape2D();
		if (shape2D.pts.empty())
			continue;

		if (Overlap::Point_Polygon2D(pt, shape2D.pts, &shape2D.aabb))
			return true;
	}
	return false;
}

//===================================================================
// IsLineSegmentIntersectingObstaclesOrCloseToThem
//===================================================================
bool CPathObstacles::IsLineSegmentIntersectingObstaclesOrCloseToThem(const Lineseg& linesegToTest, float maxDistanceToConsiderClose) const
{
	float maxDistanceToConsiderCloseSqr = square(maxDistanceToConsiderClose);

	for (TPathObstacles::const_iterator it = m_combinedObstacles.begin(); it != m_combinedObstacles.end(); ++it)
	{
		const CPathObstacle& ob = **it;
		if (ob.GetType() != CPathObstacle::ePOT_Shape2D)
			continue;

		const SPathObstacleShape2D& shape2d = ob.GetShape2D();
		if (shape2d.pts.empty())
			continue;

		if (Overlap::Lineseg_Polygon2D(linesegToTest, shape2d.pts, &shape2d.aabb))
			return true;

		TVectorOfVectors::const_iterator itEnd = shape2d.pts.end();
		TVectorOfVectors::const_iterator it1 = shape2d.pts.begin();
		TVectorOfVectors::const_iterator it2 = it1;
		++it2;

		while (it2 != itEnd)
		{
			if (Distance::Lineseg_Lineseg2DSq<float>(linesegToTest, Lineseg(*it1, *it2)) <= maxDistanceToConsiderCloseSqr)
				return true;
			++it1;
			++it2;
		}
	}
	return false;
}

//===================================================================
// IsPathIntersectingObstacles
//===================================================================
bool CPathObstacles::IsPathIntersectingObstacles(const NavigationMeshID meshID, const Vec3& start,
                                                 const Vec3& end, float radius) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool bResult = false;

	const Lineseg lineseg(start, end);
	const float radiusSq = radius * radius;
	for (TPathObstacles::const_iterator it = m_combinedObstacles.begin(); it != m_combinedObstacles.end(); ++it)
	{
		const CPathObstacle& ob = **it;
		if (ob.GetType() != CPathObstacle::ePOT_Shape2D)
			continue;

		//TODO: Investigate - can we make sure no empty shapes are stored in the container, to avoid these checks?
		const SPathObstacleShape2D& shape2D = ob.GetShape2D();
		if (shape2D.pts.empty())
			continue;

		Vec3 vIntersectionPoint(ZERO);
		if (Overlap::Lineseg_AABB2D(lineseg, shape2D.aabb) &&
		    Intersect::Lineseg_Polygon2D(lineseg, shape2D.pts, vIntersectionPoint)
		    )
		{
			// Francesco: since the obstacle could be on a different layer than where the player is walking
			// we need to check if the position on the ground of the intersection with the polygon is at than approximate
			// ground height as the obstacle itself
			Vec3 groundPointOnMesh(vIntersectionPoint);
			gAIEnv.pNavigationSystem->GetClosestMeshLocation(meshID, vIntersectionPoint, 1.5f, 0.5f, nullptr, &groundPointOnMesh, nullptr);
			const float obstacleZ = shape2D.aabb.min.z;
			const float intersectionPointZ = groundPointOnMesh.z;
			const float maxZDifference = 0.5f;
			const bool isObstacleOnTheSameLayerAsThePathSegment = abs(intersectionPointZ - obstacleZ) < maxZDifference;
			if (!isObstacleOnTheSameLayerAsThePathSegment)
				continue;

			// Ignore if shape is within our pass radius or the intersection is close enough to the end
			bool bValid = true;
			if (radius > FLT_EPSILON)
			{
				const float fDistanceToEnd = end.GetSquaredDistance2D(vIntersectionPoint);
				bValid = (fDistanceToEnd > radiusSq || !Overlap::Sphere_AABB2D(Sphere(start, radius), shape2D.aabb));
			}

			if (bValid)
			{
				bResult = true;
				break;
			}
		}
	}

	return bResult;
}

//===================================================================
// GetPointOutsideObstacles
//===================================================================
Vec3 CPathObstacles::GetPointOutsideObstacles(const Vec3& pt, float extraDist) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	Vec3 newPos = pt;

	for (TPathObstacles::const_iterator it = m_combinedObstacles.begin(); it != m_combinedObstacles.end(); ++it)
	{
		const CPathObstacle& ob = **it;
		if (ob.GetType() != CPathObstacle::ePOT_Shape2D)
			continue;

		const SPathObstacleShape2D& shape2D = ob.GetShape2D();
		if (shape2D.pts.empty())
			continue;

		if (Overlap::Point_Polygon2D(newPos, shape2D.pts, &shape2D.aabb))
		{
			(void) Distance::Point_Polygon2DSq(newPos, shape2D.pts, newPos);
			newPos.z = pt.z;
			Vec3 polyMidPoint = std::accumulate(shape2D.pts.begin(), shape2D.pts.end(), Vec3(ZERO)) / (float)shape2D.pts.size();
			Vec3 dir = (newPos - polyMidPoint);
			dir.z = 0.0f;
			dir.NormalizeSafe();
			newPos += extraDist * dir;
		}
	}
	return newPos;
}

//===================================================================
// Reset
//===================================================================
void CPathObstacles::Reset()
{
#ifdef PATHOBSTACLES_DEBUG
	m_debugPathAdjustmentBoxes.clear();
#endif
	ClearObstacles(m_simplifiedObstacles);
	ClearObstacles(m_combinedObstacles);
}

void CPathObstacles::ResetOfStaticData()
{
	while (!s_cachedObstacles.empty())
	{
		delete s_cachedObstacles.back();
		s_cachedObstacles.pop_back();
	}
	stl::free_container(s_cachedObstacles);
	stl::free_container(s_pathObstacles);
	stl::free_container(s_dynamicObstacleFlags);
	stl::free_container(s_pts);
	stl::free_container(s_addedObstacleIndices);
}

//===================================================================
// DebugDraw
//===================================================================
void CPathObstacles::DebugDraw() const
{
#ifdef PATHOBSTACLES_DEBUG
	CDebugDrawContext dc;

	for (unsigned i = 0; i < m_debugPathAdjustmentBoxes.size(); ++i)
	{
		const SDebugBox& box = m_debugPathAdjustmentBoxes[i];
		Matrix34 mat34;
		mat34.SetRotation33(Matrix33(box.q));
		mat34.SetTranslation(box.pos);
		dc->DrawOBB(box.obb, mat34, false, ColorB(0, 1, 0), eBBD_Extremes_Color_Encoded);
		dc->DrawOBB(box.obb, mat34, false, ColorB(0, 1, 0), eBBD_Extremes_Color_Encoded);
	}

	if (gAIEnv.CVars.DebugDraw != 0)
	{
		for (TPathObstacles::const_iterator it = m_simplifiedObstacles.begin(); it != m_simplifiedObstacles.end(); ++it)
		{
			const CPathObstacle& obstacle = **it;
			if (obstacle.GetType() == CPathObstacle::ePOT_Shape2D && !obstacle.GetShape2D().pts.empty())
			{
				const TVectorOfVectors& pts = obstacle.GetShape2D().pts;
				for (unsigned i = 0; i < pts.size(); ++i)
				{
					unsigned j = (i + 1) % pts.size();
					Vec3 v1 = pts[i];
					Vec3 v2 = pts[j];
					dc->DrawLine(v1, Col_Red, v2, Col_Red);
				}
			}
		}
		for (TPathObstacles::const_iterator it = m_combinedObstacles.begin(); it != m_combinedObstacles.end(); ++it)
		{
			ColorF col(1, 1, 1);
			const CPathObstacle& obstacle = **it;
			if (obstacle.GetType() == CPathObstacle::ePOT_Shape2D && !obstacle.GetShape2D().pts.empty())
			{
				const TVectorOfVectors& pts = obstacle.GetShape2D().pts;
				for (unsigned i = 0; i < pts.size(); ++i)
				{
					unsigned j = (i + 1) % pts.size();
					Vec3 v1 = pts[i];
					Vec3 v2 = pts[j];
					dc->DrawLine(v1, Col_White, v2, Col_White);

					// draw 2 more horizontal lines: each at the min. and max. height of the obstacle
					v1.z = obstacle.GetShape2D().minZ;
					v2.z = obstacle.GetShape2D().minZ;
					dc->DrawLine(v1, Col_LightGray, v2, Col_LightGray);
					v1.z = obstacle.GetShape2D().maxZ;
					v2.z = obstacle.GetShape2D().maxZ;
					dc->DrawLine(v1, Col_LightGray, v2, Col_LightGray);

					// draw a vertical line to connect the min. and max. height
					v1.z = obstacle.GetShape2D().minZ;
					v2 = v1;
					v2.z = obstacle.GetShape2D().maxZ;
					dc->DrawLine(v1, Col_LightGray, v2, Col_LightGray);
				}
			}
		}
	}
#endif
}

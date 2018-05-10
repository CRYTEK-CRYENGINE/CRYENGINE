// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   PathObstacles.h
   $Id$
   Description: interface for the CGraph class.

   -------------------------------------------------------------------------
   History:
   -

 *********************************************************************/

#ifndef PATHOBSTACLE_H
#define PATHOBSTACLE_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include "AILog.h"
#include <vector>
#include <set>

#ifdef CRYAISYSTEM_DEBUG
	#define PATHOBSTACLES_DEBUG
#endif

#include <CryAISystem/IPathfinder.h>

class CAIObject;

typedef std::vector<Vec3> TVectorOfVectors;

//===================================================================
// SPathObstacleShape2D
//===================================================================
struct SPathObstacleShape2D
{
	SPathObstacleShape2D() { aabb.Reset(); minZ = maxZ = 0.0f; }
	void         CalcAABB() { if (pts.empty()) aabb.Reset(); else aabb = AABB(&pts[0], pts.size()); }
	/// Debugging consistency check.
	unsigned int GetHash() const;

	TVectorOfVectors pts;
	AABB             aabb;
	float            minZ, maxZ; // backup of the height of all points in the shape before their AABB gets flattened; it's necessary to keep track of the height for CNavPath::AdjustPathAroundObstacleShape2D() to work reliably with "large" obstacles on slopes
};

// NOTE Jun 1, 2007: <pvl> used just for debugging ATM
bool operator==(const SPathObstacleShape2D& lhs, const SPathObstacleShape2D& rhs);

//===================================================================
// SPathObstacleCircle2D
//===================================================================
struct SPathObstacleCircle2D
{
	/// Debugging consistency check.
	unsigned int GetHash() const;

	Vec3         center;
	float        radius;
};

// NOTE Jun 1, 2007: <pvl> used just for debugging ATM
bool operator==(const SPathObstacleCircle2D& lhs, const SPathObstacleCircle2D& rhs);

//===================================================================
// CPathObstacle
//===================================================================
class CPathObstacle : public _reference_target_t
{
public:
	enum EPathObstacleType
	{
		ePOT_Unset,
		ePOT_Circle2D,
		ePOT_Shape2D,
		ePOT_Sphere3D
	};

	CPathObstacle(EPathObstacleType type = ePOT_Unset);
	~CPathObstacle();
	void           Free();
	CPathObstacle& operator=(const CPathObstacle& other);
	CPathObstacle(const CPathObstacle& other);

	EPathObstacleType            GetType() const     { return m_type; }
	SPathObstacleShape2D&        GetShape2D()        { AIAssert(m_type == ePOT_Shape2D); return *((SPathObstacleShape2D*) m_pData); }
	const SPathObstacleShape2D&  GetShape2D() const  { AIAssert(m_type == ePOT_Shape2D); return *((SPathObstacleShape2D*) m_pData); }
	SPathObstacleCircle2D&       GetCircle2D()       { AIAssert(m_type == ePOT_Circle2D); return *((SPathObstacleCircle2D*) m_pData); }
	const SPathObstacleCircle2D& GetCircle2D() const { AIAssert(m_type == ePOT_Circle2D); return *((SPathObstacleCircle2D*) m_pData); }

	/// Debugging consistency check.
	unsigned int GetHash() const;

private:
	EPathObstacleType m_type;
	void*             m_pData;
};

// NOTE Jun 1, 2007: <pvl> used just for debugging ATM
//bool operator== (const CPathObstacle & lhs, const CPathObstacle & rhs);

typedef _smart_ptr<CPathObstacle>     CPathObstaclePtr;

typedef std::vector<CPathObstaclePtr> TPathObstacles;

inline bool operator==(const TPathObstacles& lhs, const TPathObstacles& rhs);

class CNavPath;

//===================================================================
// CPathObstacles
//===================================================================
class CPathObstacles : public IPathObstacles
{
public:
	CPathObstacles();
	~CPathObstacles();

	/// collect all the obstacles around the actor. If this is called in quick succession
	/// when the actor hasn't moved then the old result will be used
	void CalculateObstaclesAroundActor(const CPipeUser* pPipeUser);
	void CalculateObstaclesAroundLocation(const Vec3& location, const AgentMovementAbility& movementAbility, const CNavPath* pNavPath);

	/// Gets the result
	const TPathObstacles& GetCombinedObstacles() const { return m_combinedObstacles; }

	/// Indicates if pt is inside the obstacles
	virtual bool IsPointInsideObstacles(const Vec3& pt) const override;

	/// Considers all ePOT_Shape2D obstacles and checks for whether given line segment is intersecting any of them or is at least close to them.
	virtual bool IsLineSegmentIntersectingObstaclesOrCloseToThem(const Lineseg& linesegToTest, float maxDistanceToConsiderClose) const override;

	/// Indicates if path crosses through obstacles
	virtual bool IsPathIntersectingObstacles(const NavigationMeshID meshID, const Vec3& start, const Vec3& end, float radius) const override;

	/// If pt is inside the combined obstacles this returns a new pt approx extraDist
	/// outside the combined obstacles (but beware it may be pushed into another obstacle!). Otherwise
	/// just returns pt.
	Vec3        GetPointOutsideObstacles(const Vec3& pt, float extraDist) const;

	void        Reset();

	static void ResetOfStaticData();

private:
	typedef std::vector<IPhysicalEntity*> TCheckedPhysicalEntities;
	typedef std::vector<Sphere>           TDynamicObstacleSpheres;
	struct SPathObstaclesInfo
	{
		float                       minAvRadius;
		float                       maxSpeed;
		float                       maxDistToCheckAhead;
		float                       maxPathDeviation;

		const CNavPath*             pNavPath;
		const CAIActor*             pAIActor;
		const AgentMovementAbility& movementAbility;

		AABB                        foundObjectsAABB;

		TPathObstacles&             outObstacles;
		TCheckedPhysicalEntities    queuedPhysicsEntities;
		TCheckedPhysicalEntities    checkedPhysicsEntities;
		TDynamicObstacleSpheres     dynamicObstacleSpheres;

		SPathObstaclesInfo(TPathObstacles& _obstacles, const AgentMovementAbility& _movementAbility,
		                   const CNavPath* _pNavPath, const CAIActor* _pAIActor, float _maxPathDeviation)
			: minAvRadius(0.0f)
			, maxSpeed(0.0f)
			, maxDistToCheckAhead(0.0f)
			, maxPathDeviation(_maxPathDeviation)
			, pNavPath(_pNavPath)
			, pAIActor(_pAIActor)
			, movementAbility(_movementAbility)
			, foundObjectsAABB(AABB::RESET)
			, outObstacles(_obstacles)
		{
		}
	};

	void GetPathObstacles(TPathObstacles& obstacles, const AgentMovementAbility& movementAbility, const CNavPath* pNavPath, const CAIActor* pAIActor);
	void GetPathObstacles_AIObject(CAIObject* pObject, SPathObstaclesInfo& pathObstaclesInfo, float maximumDistanceToPath) const;
	void GetPathObstacles_Vehicle(CAIObject* pObject, SPathObstaclesInfo& pathObstaclesInfo) const;
	void GetPathObstacles_PhysicalEntity(IPhysicalEntity* pEntity, SPathObstaclesInfo& pathObstaclesInfo, bool bIsPushable, float fCullShapeScale, const CNavPath& navPath) const;
	void GetPathObstacles_DamageRegion(const Sphere& damageRegionSphere, SPathObstaclesInfo& pathObstaclesInfo) const;

	/// Returns if the given obstacle entity is pushable by this actor
	/// outCullShapeScale - The scale [0,1] to apply to the cull shape for this entity
	bool IsObstaclePushable(const CAIActor* pAIActor, IPhysicalEntity* pEntity, SPathObstaclesInfo& pathObstaclesInfo, float& outCullShapeScale) const;

	/// Returns true if the object has the property bUsedAsDynamicObstacle set as true, false otherwise
	bool IsPhysicalEntityUsedAsDynamicObstacle(IPhysicalEntity* pPhysicalEntity) const;

	/// These are only used during path adjustment. However, if we were told to store debug then they won't
	/// be cleared afterwards, and can get drawn using DrawPathAdjustmentShapes
	TPathObstacles m_combinedObstacles;
	TPathObstacles m_simplifiedObstacles;

	/// time that CalculateObstaclesAroundActor was last called (and a cached result wasn't used)
	CTimeValue m_lastCalculateTime;
	/// Our position when CalculateObstaclesAroundActor was last called (and a cached result wasn't used)
	Vec3       m_lastCalculatePos;
	/// version number of the path when last called (since the result depends on the path)
	int        m_lastCalculatePathVersion;

	/// size of the cache of obstacle shapes
	static int s_obstacleCacheSize;
	typedef std::vector<struct SCachedObstacle*> TCachedObstacles;
	/// cached obstacle shapes - so that the most recently used is at the back.
	static TCachedObstacles s_cachedObstacles;

	static TPathObstacles   s_pathObstacles;

	typedef std::map<EntityId, bool> TCachedDynamicObstacleFlags;
	static TCachedDynamicObstacleFlags s_dynamicObstacleFlags;
	static const int                   s_maxDynamicObstacleFlags = 256;

	/// If there is a cached record for entity/extraRadius and its position matches the entity position then it will
	/// be returned, and the cache reordered.
	/// If the cached entity is found and its position doesn't match the cache will be removed and 0 returned
	/// If the cached entity isn't found 0 will be returned.
	static struct SCachedObstacle* GetOrClearCachedObstacle(IPhysicalEntity* entity, float extraRadius);
	/// pushes a obstacle into the cache, and takes ownership of it. obstacle should have been created
	/// on the heap
	static void                    AddCachedObstacle(struct SCachedObstacle* obstacle);
	static struct SCachedObstacle* GetNewCachedObstacle();
public:
	// debug for path adjustment
	bool AddEntityBoxesToObstacles(IPhysicalEntity* entity, TPathObstacles& obstacles, float extraRadius, float terrainZ, const bool debug) const;
	void DebugDraw() const;
	struct SDebugBox
	{
		OBB  obb;
		Quat q;
		Vec3 pos;
	};
#ifdef PATHOBSTACLES_DEBUG
	mutable std::vector<SDebugBox> m_debugPathAdjustmentBoxes;
#endif
};

#endif

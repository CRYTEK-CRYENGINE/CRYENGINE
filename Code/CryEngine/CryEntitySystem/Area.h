// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AreaManager.h"
#include "EntitySystem.h"
#include <CryMath/GeomQuery.h>

#define INVALID_AREA_GROUP_ID -1

namespace Cry
{
namespace AreaManager
{
enum class EAreaState
{
	None,
	Initialized       = BIT(0),
	Hidden            = BIT(1),
	ObstructRoof      = BIT(2),
	ObstructFloor     = BIT(3),
	EntityIdsResolved = BIT(4),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EAreaState);
} // namespace AreaManager
} // namespace Cry

// Position type of a position compared relative to the given area
enum EAreaPosType
{
	AREA_POS_TYPE_2DINSIDE_ZABOVE,   // Above an area
	AREA_POS_TYPE_2DINSIDE_ZINSIDE,  // Inside an area
	AREA_POS_TYPE_2DINSIDE_ZBELOW,   // Below an area
	AREA_POS_TYPE_2DOUTSIDE_ZABOVE,  // Next to an area but above it
	AREA_POS_TYPE_2DOUTSIDE_ZINSIDE, // Next to an area but inside its min and max Z
	AREA_POS_TYPE_2DOUTSIDE_ZBELOW,  // Next to an area but below it

	AREA_POS_TYPE_COUNT
};

enum ECachedAreaData
{
	eCachedAreaData_None                           = 0x00000000,
	eCachedAreaData_PosTypeValid                   = BIT(0),
	eCachedAreaData_PosOnHullValid                 = BIT(1),
	eCachedAreaData_DistWithinSqValid              = BIT(2),
	eCachedAreaData_DistWithinSqValidNotObstructed = BIT(3),
	eCachedAreaData_DistNearSqValid                = BIT(4),
	eCachedAreaData_DistNearSqValidNotObstructed   = BIT(5),
	eCachedAreaData_DistNearSqValidHeightIgnored   = BIT(6),
	eCachedAreaData_PointWithinValid               = BIT(7),
	eCachedAreaData_PointWithinValidHeightIgnored  = BIT(8),
};

class CAreaSolid;

class CArea final : public IArea
{

public:

	using a2DPoint = Vec2;

	struct a2DBBox
	{
		bool PointOutBBox2D(const a2DPoint& point) const
		{
			return (point.x < min.x || point.x > max.x || point.y < min.y || point.y > max.y);
		}

		bool PointOutBBox2DVertical(const a2DPoint& point) const
		{
			return (point.y <= min.y || point.y > max.y || point.x > max.x);
		}

		bool BBoxOutBBox2D(const a2DBBox& box) const
		{
			return (box.max.x < min.x || box.min.x > max.x || box.max.y < min.y || box.min.y > max.y);
		}

		ILINE bool Overlaps2D(const AABB& bb) const
		{
			if (min.x > bb.max.x) return false;
			if (min.y > bb.max.y) return false;
			if (max.x < bb.min.x) return false;
			if (max.y < bb.min.y) return false;
			return true; //the aabb's overlap
		}

		a2DPoint min; // 2D BBox min
		a2DPoint max; // 2D BBox max
	};

	struct a2DSegment
	{
		a2DSegment()
			: isHorizontal(false),
			bObstructSound(false),
			k(0.0f),
			b(0.0f){}

		bool IntersectsXPos(const a2DPoint& point) const
		{
			if (k != 0.0f)
			{
				return (point.x < (point.y - b) / k);
			}
			else
			{
				return (point.y - b) > 0.0f;
			}
		}

		bool IntersectsXPosVertical(const a2DPoint& point) const
		{
			if (k != 0.0f)
			{
				return false;
			}
			else
			{
				return (point.x <= b);
			}
		}

		a2DPoint const GetStart() const
		{
			return a2DPoint(bbox.min.x, (float)__fsel(k, bbox.min.y, bbox.max.y));
		}

		a2DPoint const GetEnd() const
		{
			return a2DPoint(bbox.max.x, (float)__fsel(k, bbox.max.y, bbox.min.y));
		}

		bool    bObstructSound; // Does it obstruct sounds?
		bool    isHorizontal;   // horizontal flag
		float   k, b;           // line parameters y=kx+b
		a2DBBox bbox;           // segment's BBox
		Vec2    normal;         // 2D outward facing normal
	};

	struct SBoxHolder
	{
		a2DBBox box;
		CArea*  area;
	};

	using TAreaBoxes = std::vector<SBoxHolder>;

	static const TAreaBoxes& GetBoxHolders();

	//////////////////////////////////////////////////////////////////////////
	explicit CArea(CAreaManager* pManager);

	//IArea
	virtual size_t         GetEntityAmount() const override                  { return m_entityIds.size(); }
	virtual const EntityId GetEntityByIdx(size_t const index) const override { return m_entityIds[index]; }
	virtual int GetGroup() const override    { return m_areaGroupId; }
	virtual int GetPriority() const override { return m_priority; }
	virtual int GetID() const override       { return m_areaId; }
	virtual AABB GetAABB() const override;
	virtual float GetExtent(EGeomForm eForm) override;
	virtual void GetRandomPoints(Array<PosNorm> points, CRndGen seed, EGeomForm eForm) const override;
	virtual bool IsPointInside(Vec3 const& pointToTest) const override;
	//~IArea

	void     Release();
	void     SetID(const int id)              { m_areaId = id; }
	void     SetEntityID(const EntityId id)   { m_entityId = id; }
	EntityId GetEntityID() const              { return m_entityId; }
	void     SetGroup(const int id)           { m_areaGroupId = id; }
	void     SetPriority(const int nPriority) { m_priority = nPriority; }

	// Description:
	//    Sets sound obstruction depending on area type
	void            SetSoundObstructionOnAreaFace(size_t const index, bool const bObstructs);

	void            SetAreaType(EEntityAreaType type);
	EEntityAreaType GetAreaType() const { return m_areaType; }

	//////////////////////////////////////////////////////////////////////////
	// These functions also switch area type.
	//////////////////////////////////////////////////////////////////////////
	void SetPoints(Vec3 const* const pPoints, bool const* const pSoundObstructionSegments, size_t const numLocalPoints, bool const bClosed);
	void SetBox(const Vec3& min, const Vec3& max, const Matrix34& tm);
	void SetSphere(const Vec3& vCenter, float fRadius);
	void BeginSettingSolid(const Matrix34& worldTM);
	void AddConvexHullToSolid(const Vec3* verticesOfConvexHull, bool bObstruction, int numberOfVertices);
	void EndSettingSolid();
	//////////////////////////////////////////////////////////////////////////

	void                    MovePoints(Vec3 const* const pPoints, size_t const numLocalPoints);

	void                    SetMatrix(const Matrix34& tm);
	void                    GetMatrix(Matrix34& tm) const;

	void                    GetBox(Vec3& min, Vec3& max) const             { min = m_boxMin; max = m_boxMax; }
	void                    GetWorldBox(Vec3& rMin, Vec3& rMax) const;
	void                    GetSphere(Vec3& vCenter, float& fRadius) const { vCenter = m_sphereCenter; fRadius = m_sphereRadius; }

	void                    SetHeight(float fHeight)                       { m_height = fHeight; }
	float                   GetHeight() const                              { return m_height; }

	void                    AddEntity(const EntityId entId);
	void                    AddEntity(const EntityGUID entGuid);
	void                    AddEntities(const EntityIdVector& entIDs);
	const EntityIdVector*   GetEntities() const     { return &m_entityIds; }
	const EntityGuidVector* GetEntitiesGuid() const { return &m_entityGuids; }

	void                    RemoveEntity(EntityId const entId);
	void                    RemoveEntity(EntityGUID const entGuid);
	void                    RemoveEntities();
	void                    ResolveEntityIds();
	void                    ReleaseCachedAreaData();

	void                    SetProximity(float prx) { m_proximity = prx; }
	float                   GetProximity()          { return m_proximity; }

	float                   GetFadeDistance();
	float                   GetEnvironmentFadeDistance();
	float                   GetGreatestFadeDistance();

	float                   GetInnerFadeDistance() const               { return m_innerFadeDistance; }
	void                    SetInnerFadeDistance(float const distance) { m_innerFadeDistance = distance; }

	// Invalidations
	void InvalidateCachedAreaData(EntityId const nEntityID);

	// Calculations
	//	Methods can have set the bCacheResult parameter which indicates, if set to true,
	//	that data will be cached and available through the GetCached... methods.
	//	If bCacheResult is set to false no data is cached and just the result is returned,
	//	this allows for calculating positions or distances without trashing previously cached results.
	EAreaPosType CalcPosType(EntityId const nEntityID, Vec3 const& rPos, bool const bCacheResult = true);
	float        CalcPointWithinDist(EntityId const nEntityID, Vec3 const& point3d, bool const bIgnoreSoundObstruction = true, bool const bCacheResult = true);
	bool         CalcPointWithin(EntityId const nEntityID, Vec3 const& point3d, bool const bIgnoreHeight = false, bool const bCacheResult = true);
	bool         CalcPointWithinNonCached(Vec3 const& point3d, bool const bIgnoreHeight) const;
	float        CalcDistToPoint(a2DPoint const& point) const;

	// Squared-distance returned works only if point32 is not within the area
	float CalcPointNearDistSq(EntityId const nEntityID, Vec3 const& Point3d, Vec3& OnHull3d, bool const bIgnoreSoundObstruction = true, bool const bCacheResult = true);
	float CalcPointNearDistSq(EntityId const nEntityID, Vec3 const& Point3d, bool const bIgnoreHeight = false, bool const bIgnoreSoundObstruction = true, bool const bCacheResult = true);
	float ClosestPointOnHullDistSq(EntityId const nEntityID, Vec3 const& Point3d, Vec3& OnHull3d, bool const bIgnoreSoundObstruction = true, bool const bCacheResult = true);

	// Get cached data
	float        GetCachedPointWithinDistSq(EntityId const nEntityID) const;
	bool         GetCachedPointWithin(EntityId const nEntityID) const;
	EAreaPosType GetCachedPointPosTypeWithin(EntityId const nEntityID) const;
	float        GetCachedPointNearDistSq(EntityId const nEntityID) const;
	Vec3 const&  GetCachedPointOnHull(EntityId const nEntityID) const;

	// Events
	void AddCachedEvent(const SEntityEvent& pNewEvent);
	void ClearCachedEventsFor(EntityId const nEntityID);
	void ClearCachedEvents();
	void SendCachedEventsFor(EntityId const nEntityID);

	void SendEvent(SEntityEvent& newEvent, bool bClearCachedEvents = true);

	// Inside
	void EnterArea(EntityId const entityId);
	void LeaveArea(EntityId const entityId);
	void ExclusiveUpdateAreaInside(
	  EntityId const entityId,
	  EntityId const higherAreaId,
	  float const fade);
	void ExclusiveUpdateAreaNear(
	  EntityId const entityId,
	  EntityId const higherAreaId,
	  float const distance,
	  Vec3 const& position);
	float CalculateFade(const Vec3& pos3D);

	// Near
	void EnterNearArea(
	  EntityId const entityId,
	  Vec3 const& closestPointToArea,
	  float const distance);
	void LeaveNearArea(EntityId const entityId);

	// Far
	void   OnAddedToAreaCache(EntityId const entityId);
	void   OnRemovedFromAreaCache(EntityId const entityId);

	void   Draw(size_t const idx);

	size_t MemStat();

	void   GetBBox(Vec2& vMin, Vec2& vMax) const;
	void   GetSolidBoundBox(AABB& outBoundBox) const;

#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
	size_t            GetCacheEntityCount() const { return m_mapEntityCachedAreaData.size(); }
	char const* const GetAreaEntityName() const;
#endif // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE

	void                         GetMemoryUsage(ICrySizer* pSizer) const;

	Cry::AreaManager::EAreaState GetState() const                                      { return m_state; }
	void                         AddState(Cry::AreaManager::EAreaState const state)    { m_state |= state; }
	void                         RemoveState(Cry::AreaManager::EAreaState const state) { m_state &= ~state; }

private:

	struct SCachedAreaData
	{
		SCachedAreaData()
			: eFlags(eCachedAreaData_None),
			ePosType(AREA_POS_TYPE_COUNT),
			oPos(ZERO),
			fDistanceWithinSq(FLT_MAX),
			fDistanceNearSq(FLT_MAX),
			bPointWithin(false){}

		ECachedAreaData eFlags;
		EAreaPosType    ePosType;
		Vec3            oPos;
		float           fDistanceWithinSq;
		float           fDistanceNearSq;
		bool            bPointWithin;
	};

	virtual ~CArea() override;

	void           AddSegment(const a2DPoint& p0, const a2DPoint& p1, bool const nObstructSound);
	void           UpdateSegment(a2DSegment& segment, a2DPoint const& p0, a2DPoint const& p1);
	void           CalcBBox();
	const a2DBBox& GetBBox() const;
	a2DBBox&       GetBBox();
	void           ClearPoints();
	void           CalcClosestPointToObstructedShape(EntityId const nEntityID, Vec3& rv3ClosestPos, float& rfClosestDistSq, Vec3 const& rv3SourcePos);
	void           CalcClosestPointToObstructedBox(Vec3& rv3ClosestPos, float& rfClosestDistSq, Vec3 const& rv3SourcePos) const;
	void           CalcClosestPointToSolid(Vec3 const& rv3SourcePos, bool bIgnoreSoundObstruction, float& rfClosestDistSq, Vec3* rv3ClosestPos) const;
	void           ReleaseAreaData();

private:

	CAreaManager* const m_pAreaManager;
	float               m_proximity;
	float               m_fadeDistance;
	float               m_environmentFadeDistance;
	float               m_greatestFadeDistance;
	float               m_innerFadeDistance;

	// attached entities IDs list
	EntityIdVector   m_entityIds;
	EntityGuidVector m_entityGuids;

	using CachedEvents = std::vector<SEntityEvent>;
	CachedEvents             m_cachedEvents;

	int                      m_areaId;
	int                      m_areaGroupId;
	EntityId                 m_entityId;
	int                      m_priority;

	Matrix34                 m_worldTM;
	Matrix34                 m_invMatrix;

	EEntityAreaType          m_areaType;
	typedef VectorMap<EntityId, SCachedAreaData> TEntityCachedAreaDataMap;
	TEntityCachedAreaDataMap m_mapEntityCachedAreaData;
	Vec3 const               m_nullVec;

	// for shape areas ----------------------------------------------------------------------
	// area's bbox
	a2DBBox m_areaBBox;
	size_t  m_bbox_holder;
	// the area segments
	std::vector<Vec3> m_areaPoints;
	using AreaSegments = std::vector<a2DSegment*>;
	AreaSegments m_areaSegments;
	CGeomExtents m_extents;
	float m_area = 0;
	bool m_bClosed = true;
	std::vector<int> m_triIndices;

	size_t PrevPoint(size_t i) const { return i > 0 ? i - 1 : m_areaPoints.size() - 1; }
	size_t NextPoint(size_t i) const { return i < m_areaPoints.size() - 1 ? i + 1 : 0; }

	// for sector areas ----------------------------------------------------------------------
	//	int	m_Building;
	//	int	m_Sector;
	//	IVisArea *m_Sector;

	// for box areas ----------------------------------------------------------------------
	Vec3         m_boxMin;
	Vec3         m_boxMax;
	bool         m_bAllObstructed : 1;
	unsigned int m_numObstructed;

	struct SBoxSideSoundObstruction
	{
		bool bObstructed : 1;
	} m_boxSideObstruction[6];

	struct SBoxSide
	{
		SBoxSide(Vec3 const& _minValues, Vec3 const& _maxValues)
			: minValues(_minValues),
			maxValues(_maxValues){}
		Vec3 const minValues;
		Vec3 const maxValues;
	};

	// for sphere areas ----------------------------------------------------------------------
	Vec3  m_sphereCenter;
	float m_sphereRadius;
	float m_sphereRadius2;

	// for solid areas -----------------------------------------------------------------------
	_smart_ptr<CAreaSolid> m_pAreaSolid;

	//  area vertical origin - the lowest point of area
	float m_origin;
	//  area height (vertical size). If (m_height<=0) - not used, only 2D check is done. Otherwise
	//  additional check for Z to be in [m_origin, m_origin + m_height] range is done
	float m_height;

	Cry::AreaManager::EAreaState m_state = Cry::AreaManager::EAreaState::None;
};

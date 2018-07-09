// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MNM_MESH_GRID_H
#define __MNM_MESH_GRID_H

#pragma once

#include "MNM.h"
#include "Tile.h"
#include "MNMProfiler.h"
#include "Islands.h"

#include <CryMath/SimpleHashLookUp.h>
#include <CryCore/Containers/VectorMap.h>

#include <CryAISystem/NavigationSystem/MNMNavMesh.h>
#include <CryAISystem/NavigationSystem/INavigationQuery.h>

class OffMeshNavigationManager;

namespace MNM
{
struct OffMeshNavigation;

///////////////////////////////////////////////////////////////////////

// Heuristics for dangers
enum EWeightCalculationType
{
	eWCT_None = 0,
	eWCT_Range,             // It returns 1 as a weight for the cost if the location to evaluate
	                        //  is in the specific range from the threat, 0 otherwise
	eWCT_InverseDistance,   // It returns the inverse of the distance as a weight for the cost
	eWCT_Direction,         // It returns a value between [0,1] as a weight for the cost in relation of
	                        // how the location to evaluate is in the threat direction. The range is not taken into
	                        // account with this weight calculation type
	eWCT_Last,
};

template<EWeightCalculationType WeightType>
struct DangerWeightCalculation
{
	MNM::real_t CalculateWeight(const Vec3& locationToEval, const Vec3& startingLocation, const Vec3& dangerPosition, const float rangeSq) const
	{
		return MNM::real_t(.0f);
	}
};

template<>
struct DangerWeightCalculation<eWCT_InverseDistance>
{
	MNM::real_t CalculateWeight(const Vec3& locationToEval, const Vec3& startingLocation, const Vec3& dangerPosition, const float rangeSq) const
	{
		// TODO: This is currently not used, but if we see we need it, then we should think to change
		// the euclidean distance calculation with a faster approximation (Like the one used in the FindWay function)
		const float distance = (dangerPosition - locationToEval).len();
		bool isInRange = rangeSq > .0f ? sqr(distance) > rangeSq : true;
		return isInRange ? MNM::real_t(1 / distance) : MNM::real_t(.0f);
	}
};

template<>
struct DangerWeightCalculation<eWCT_Range>
{
	MNM::real_t CalculateWeight(const Vec3& locationToEval, const Vec3& startingLocation, const Vec3& dangerPosition, const float rangeSq) const
	{
		const Vec3 dangerToLocationDir = locationToEval - dangerPosition;
		const float weight = static_cast<float>(__fsel(dangerToLocationDir.len2() - rangeSq, 0.0f, 1.0f));
		return MNM::real_t(weight);
	}
};

template<>
struct DangerWeightCalculation<eWCT_Direction>
{
	real_t CalculateWeight(const Vec3& locationToEval, const Vec3& startingLocation, const Vec3& dangerPosition, const float rangeSq) const
	{
		Vec3 startLocationToNewLocation = (locationToEval - startingLocation);
		startLocationToNewLocation.NormalizeSafe();
		Vec3 startLocationToDangerPosition = (dangerPosition - startingLocation);
		startLocationToDangerPosition.NormalizeSafe();
		float dotProduct = startLocationToNewLocation.dot(startLocationToDangerPosition);
		return max(real_t(.0f), real_t(dotProduct));
	}
};

struct DangerArea
{
	virtual ~DangerArea() {}
	virtual real_t      GetDangerHeuristicCost(const Vec3& locationToEval, const Vec3& startingLocation) const = 0;
	virtual const Vec3& GetLocation() const = 0;
};
DECLARE_SHARED_POINTERS(DangerArea)

template<EWeightCalculationType WeightType>
struct DangerAreaT : public DangerArea
{
	DangerAreaT()
		: location(ZERO)
		, effectRangeSq(.0f)
		, cost(0)
		, weightCalculator()
	{}

	DangerAreaT(const Vec3& _location, const float _effectRange, const uint8 _cost)
		: location(_location)
		, effectRangeSq(sqr(_effectRange))
		, cost(_cost)
		, weightCalculator()
	{}

	virtual real_t GetDangerHeuristicCost(const Vec3& locationToEval, const Vec3& startingLocation) const
	{
		return real_t(cost) * weightCalculator.CalculateWeight(locationToEval, startingLocation, location, effectRangeSq);
	}

	virtual const Vec3& GetLocation() const { return location; }

private:
	const Vec3                                location;      // Position of the danger
	const float                               effectRangeSq; // If zero the effect is on the whole level
	const unsigned int                        cost;          // Absolute cost associated with the Danger represented by the DangerAreaT
	const DangerWeightCalculation<WeightType> weightCalculator;
};

enum { max_danger_amount = 5, };

typedef CryFixedArray<MNM::DangerAreaConstPtr, max_danger_amount> DangerousAreasList;

///////////////////////////////////////////////////////////////////////

class CNavMesh : public MNM::INavMesh
{
	friend class CIslands;

public:
	enum { x_bits = 11, };   // must add up to a 32 bit - the "tileName"
	enum { y_bits = 11, };
	enum { z_bits = 10, };

	enum { max_x = (1 << x_bits) - 1, };
	enum { max_y = (1 << y_bits) - 1, };
	enum { max_z = (1 << z_bits) - 1, };

	struct SGridParams
	{
		SGridParams()
			: origin(ZERO)
			, tileSize(16)
			, tileCount(1024)
			, voxelSize(0.1f)
		{
		}

		Vec3   origin;
		Vec3i  tileSize;
		Vec3   voxelSize;
		uint32 tileCount;
	};

	enum { SideCount = 14, };

	enum EPredictionType
	{
		ePredictionType_TriangleCenter = 0,
		ePredictionType_Advanced,
		ePredictionType_Latest,
	};

	struct WayQueryRequest
	{
		WayQueryRequest(EntityId _requesterEntityId, TriangleID _from, const vector3_t& _fromLocation, TriangleID _to,
		                const vector3_t& _toLocation, const OffMeshNavigation& _offMeshNavigation, const OffMeshNavigationManager& _offMeshNavigationManager,
		                const DangerousAreasList& dangerousAreas, const INavMeshQueryFilter* pFilter, const MNMCustomPathCostComputerSharedPtr& _pCustomPathCostComputer)
			: m_from(_from)
			, m_to(_to)
			, m_fromLocation(_fromLocation)
			, m_toLocation(_toLocation)
			, m_offMeshNavigation(_offMeshNavigation)
			, m_offMeshNavigationManager(_offMeshNavigationManager)
			, m_dangerousAreas(dangerousAreas)
			, m_pCustomPathCostComputer(_pCustomPathCostComputer)
			, m_requesterEntityId(_requesterEntityId)
			, m_pFilter(pFilter)
		{}

		virtual ~WayQueryRequest(){}

		virtual bool                    CanUseOffMeshLink(const OffMeshLinkID linkID, float* costMultiplier) const;
		bool                            IsPointValidForAgent(const Vec3& pos, uint32 flags) const;

		ILINE const TriangleID          From() const                 { return m_from; }
		ILINE const TriangleID          To() const                   { return m_to; }
		ILINE const OffMeshNavigation&  GetOffMeshNavigation() const { return m_offMeshNavigation; }
		ILINE const DangerousAreasList& GetDangersInfos()            { return m_dangerousAreas; }
		ILINE const vector3_t&          GetFromLocation() const      { return m_fromLocation; };
		ILINE const vector3_t&          GetToLocation() const        { return m_toLocation; };
		ILINE const MNMCustomPathCostComputerSharedPtr& GetCustomPathCostComputer() const { return m_pCustomPathCostComputer; }  // might be nullptr (which is totally OK)
		ILINE const INavMeshQueryFilter* GetFilter() const        { return m_pFilter; }

	private:
		const TriangleID                m_from;
		const TriangleID                m_to;
		const vector3_t                 m_fromLocation;
		const vector3_t                 m_toLocation;
		const OffMeshNavigation&        m_offMeshNavigation;
		const OffMeshNavigationManager& m_offMeshNavigationManager;
		DangerousAreasList              m_dangerousAreas;
		MNMCustomPathCostComputerSharedPtr m_pCustomPathCostComputer;
		const INavMeshQueryFilter*   m_pFilter;
	protected:
		const EntityId                  m_requesterEntityId;
	};

	struct WayQueryWorkingSet
	{
		WayQueryWorkingSet()
		{
			nextLinkedTriangles.reserve(32);
		}
		typedef std::vector<WayTriangleData> TNextLinkedTriangles;

		void Reset()
		{
			aStarOpenList.Reset();
			nextLinkedTriangles.clear();
			nextLinkedTriangles.reserve(32);
		}

		TNextLinkedTriangles nextLinkedTriangles;
		AStarOpenList        aStarOpenList;
	};

	struct WayQueryResult
	{
		WayQueryResult(const size_t wayMaxSize = 512)
			: m_wayMaxSize(wayMaxSize)
			, m_waySize(0)
		{
			Reset();
		}

		WayQueryResult(WayQueryResult&&) = default;
		WayQueryResult(const WayQueryResult&) = delete;
		~WayQueryResult() = default;

		void Reset()
		{
			m_pWayTriData.reset(new WayTriangleData[m_wayMaxSize]);
			m_waySize = 0;
		}

		ILINE WayTriangleData* GetWayData() const               { return m_pWayTriData.get(); }
		ILINE size_t           GetWaySize() const               { return m_waySize; }
		ILINE size_t           GetWayMaxSize() const            { return m_wayMaxSize; }

		ILINE void             SetWaySize(const size_t waySize) { m_waySize = waySize; }

		ILINE void             Clear()                          { m_waySize = 0; }

		WayQueryResult& operator=(WayQueryResult&&) = default;
		WayQueryResult& operator=(const WayQueryResult&) = delete;

	private:

		std::unique_ptr<WayTriangleData[]> m_pWayTriData;
		size_t                             m_wayMaxSize;
		size_t                             m_waySize;
	};

	enum EWayQueryResult
	{
		eWQR_Continuing = 0,
		eWQR_Done,
	};

	CNavMesh();
	~CNavMesh();

	void                 Init(const SGridParams& params);

	static inline size_t ComputeTileName(size_t x, size_t y, size_t z)
	{
		return (x & ((1 << x_bits) - 1)) |
		       ((y & ((1 << y_bits) - 1)) << x_bits) |
		       ((z & ((1 << z_bits) - 1)) << (x_bits + y_bits));
	}

	static inline void ComputeTileXYZ(size_t tileName, size_t& x, size_t& y, size_t& z)
	{
		x = tileName & ((1 << x_bits) - 1);
		y = (tileName >> x_bits) & ((1 << y_bits) - 1);
		z = (tileName >> (x_bits + y_bits)) & ((1 << z_bits) - 1);
	}

	size_t     GetTriangles(aabb_t aabb, TriangleID* triangles, size_t maxTriCount, const INavMeshQueryFilter* pFilter, float minIslandArea = 0.f) const;
	TriangleID GetTriangleAt(const vector3_t& location, const real_t verticalDownwardRange, const real_t verticalUpwardRange, const INavMeshQueryFilter* pFilter, float minIslandArea = 0.f) const;
	TriangleID GetClosestTriangle(const vector3_t& location, real_t vrange, real_t hrange, const INavMeshQueryFilter* pFilter, real_t* distance = nullptr, vector3_t* closest = nullptr, float minIslandArea = 0.f) const;
	TriangleID GetClosestTriangle(const vector3_t& location, const aabb_t& aroundLocationAABB, const INavMeshQueryFilter* pFilter, real_t* distance = nullptr, vector3_t* closest = nullptr, float minIslandArea = 0.f) const;

	bool IsTriangleAcceptableForLocation(const vector3_t& location, TriangleID triangleID) const;

	bool GetVertices(TriangleID triangleID, vector3_t& v0, vector3_t& v1, vector3_t& v2) const;
	bool GetVertices(TriangleID triangleID, vector3_t* verts) const;
	// Sets a bit in linkedEdges for each edge on this triangle that has a link.
	// Bit 0: v0->v1, Bit 1: v1->v2, Bit 2: v2->v0
	bool          GetLinkedEdges(TriangleID triangleID, size_t& linkedEdges) const;
	bool          GetTriangle(TriangleID triangleID, Tile::STriangle& triangle) const;

	size_t        GetMeshBorders(const aabb_t& aabb, const INavMeshQueryFilter* pFilter, Vec3* pBorders, size_t maxBorderCount, float minIslandArea = 0.f) const;

	bool          PushPointInsideTriangle(const TriangleID triangleID, vector3_t& location, real_t amount) const;


	inline size_t GetTriangleCount() const
	{
		return m_triangleCount;
	}

	void                      AddOffMeshLinkToTile(const TileID tileID, const TriangleID triangleID, const uint16 offMeshIndex);
	void                      UpdateOffMeshLinkForTile(const TileID tileID, const TriangleID triangleID, const uint16 offMeshIndex);
	void                      RemoveOffMeshLinkFromTile(const TileID tileID, const TriangleID triangleID);

	CNavMesh::EWayQueryResult FindWay(WayQueryRequest& inputRequest, WayQueryWorkingSet& workingSet, WayQueryResult& result) const;

	real_t                    CalculateHeuristicCostForDangers(const vector3_t& locationToEval, const vector3_t& startingLocation, const Vec3& meshOrigin, const DangerousAreasList& dangersInfos) const;
	real_t                    CalculateHeuristicCostForCustomRules(const vector3_t& locationComingFrom, const vector3_t& locationGoingTo, const Vec3& meshOrigin, const IMNMCustomPathCostComputer* pCustomPathCostComputer) const;
	void                      PullString(const vector3_t& from, const TriangleID fromTriID, const vector3_t& to, const TriangleID toTriID, vector3_t& middlePoint) const;

	struct RayHit
	{
		TriangleID triangleID;
		real_t     distance;
		size_t     edge;
	};

	enum ERayCastResult
	{
		eRayCastResult_NoHit = 0,
		eRayCastResult_Hit,
		eRayCastResult_RayTooLong,
		eRayCastResult_Unacceptable,
		eRayCastResult_InvalidStart,
		eRayCastResult_InvalidEnd,
	};

	/*
	 ********************************************************************************************
	   RayCastRequestBase holds the actual request information needed to perform a RayCast request.
	   It needs to be constructed through the RayCastRequest that assures the presence of the way.
	 ********************************************************************************************
	 */

	struct RaycastRequestBase
	{
	protected:
		// This class can't be instantiated directly.
		// cppcheck-suppress uninitMemberVar
		RaycastRequestBase(const size_t _maxWayTriCount)
			: maxWayTriCount(_maxWayTriCount)
			, way(NULL)
			, wayTriCount(0)
			, result(ERayCastResult::eRayCastResult_NoHit)
		{}

	public:
		RayHit         hit;
		TriangleID*    way;
		size_t         wayTriCount;
		const size_t   maxWayTriCount;
		ERayCastResult result;
	};

	template<size_t MaximumNumberOfTrianglesInWay>
	struct RayCastRequest : public RaycastRequestBase
	{
		RayCastRequest()
			: RaycastRequestBase(MaximumNumberOfTrianglesInWay)
		{
			way = &(wayArray[0]);
			wayTriCount = 0;
		}
	private:
		TriangleID wayArray[MaximumNumberOfTrianglesInWay];
	};

	// ********************************************************************************************

	ERayCastResult RayCast(const vector3_t& from, TriangleID fromTri, const vector3_t& to, TriangleID toTri,
	                       RaycastRequestBase& wayRequest, const INavMeshQueryFilter* filter) const;

	ERayCastResult RayCast_v1(const vector3_t& from, TriangleID fromTri, const vector3_t& to, TriangleID toTri, RaycastRequestBase& wayRequest) const;
	ERayCastResult RayCast_v2(const vector3_t& from, TriangleID fromTriangleID, const vector3_t& to, TriangleID toTriangleID, RaycastRequestBase& wayRequest) const;
	
	template<typename TFilter>
	ERayCastResult RayCast_v3(const vector3_t& from, TriangleID fromTriangleID, const vector3_t& to, const TFilter& filter, RaycastRequestBase& wayRequest) const;

	TileID         SetTile(size_t x, size_t y, size_t z, STile& tile);
	void           ClearTile(TileID tileID, bool clearNetwork = true);

	void           CreateNetwork();
	void           ConnectToNetwork(TileID tileID);

	inline bool    Empty() const
	{
		return m_tiles.GetTileCount() == 0;
	}

	inline size_t GetTileCount() const
	{
		return m_tiles.GetTileCount();
	}

	TileID GetTileID(size_t x, size_t y, size_t z) const;
	const STile&    GetTile(TileID) const;
	STile&          GetTile(TileID);
	const vector3_t GetTileContainerCoordinates(TileID) const;

	void                      Swap(CNavMesh& other);

	inline const SGridParams& GetGridParams() const
	{
		return m_params;
	}

	inline void OffsetOrigin(const Vec3& offset)
	{
		m_params.origin += offset;
	}

	void Draw(size_t drawFlags, const ITriangleColorSelector& colorSelector, TileID excludeID = 0) const;

	bool CalculateMidEdge(const TriangleID triangleID1, const TriangleID triangleID2, Vec3& result) const;

	enum ProfilerTimers
	{
		NetworkConstruction = 0,
	};

	enum ProfilerMemoryUsers
	{
		TriangleMemory = 0,
		VertexMemory,
		BVTreeMemory,
		LinkMemory,
		GridMemory,
	};

	enum ProfilerStats
	{
		TileCount = 0,
		TriangleCount,
		VertexCount,
		BVTreeNodeCount,
		LinkCount,
	};

	typedef MNMProfiler<ProfilerMemoryUsers, ProfilerTimers, ProfilerStats> ProfilerType;
	const ProfilerType& GetProfiler() const;

#if MNM_USE_EXPORT_INFORMATION
	enum EAccessibilityRequestValue
	{
		eARNotAccessible = 0,
		eARAccessible    = 1
	};

	struct AccessibilityRequest
	{
		AccessibilityRequest(TriangleID _fromTriangleId, const OffMeshNavigation& _offMeshNavigation)
			: fromTriangle(_fromTriangleId)
			, offMeshNavigation(_offMeshNavigation)
		{}
		const TriangleID         fromTriangle;
		const OffMeshNavigation& offMeshNavigation;
	};

	void ResetAccessibility(uint8 accessible);
	void ComputeAccessibility(const AccessibilityRequest& inputRequest);
#endif

	CIslands& GetIslands() { return m_islands; }
	const CIslands& GetIslands() const { return m_islands; }

	TileID GetNeighbourTileID(size_t x, size_t y, size_t z, size_t side) const;
	void SetTrianglesAnnotation(const MNM::TriangleID* pTrianglesArray, const size_t trianglesCount, const MNM::AreaAnnotation areaAnnotation, std::vector<TileID>& affectedTiles);
	bool SnapPosition(const vector3_t& position, const aabb_t& aroundPositionAABB, const SSnapToNavMeshRulesInfo& snappingRules, const INavMeshQueryFilter* pFilter, vector3_t& snappedPosition, MNM::TriangleID* pTriangleId) const;

	// MNM::INavMesh
	virtual void       GetMeshParams(NavMesh::SParams& outParams) const override;
	virtual TileID     FindTileIDByTileGridCoord(const vector3_t& tileGridCoord) const override;
	virtual size_t     QueryTriangles(const aabb_t& queryAabbWorld, INavMeshQueryFilter* pOptionalFilter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const override;
	virtual TriangleID FindClosestTriangle(const vector3_t& queryPosWorld, const TriangleID* pCandidateTriangles, const size_t candidateTrianglesCount, vector3_t* pOutClosestPosWorld, float* pOutClosestDistanceSq) const override;
	virtual bool       GetTileData(const TileID tileId, Tile::STileData& outTileData) const override;
	virtual const AreaAnnotation* GetTriangleAnnotation(TriangleID triangleID) const override;
	virtual bool CanTrianglePassFilter(const TriangleID triangleID, const INavMeshQueryFilter& filter) const override;
	// ~MNM::INavMesh

private:

	template<typename TFilter>
	CNavMesh::EWayQueryResult FindWayInternal(WayQueryRequest& inputRequest, WayQueryWorkingSet& workingSet, const TFilter& filter, WayQueryResult& result) const;

	void    PredictNextTriangleEntryPosition(const TriangleID bestNodeTriangleID, const vector3_t& startPosition, const TriangleID nextTriangleID, const unsigned int vertexIndex, const vector3_t& finalLocation, vector3_t& outPosition) const;

	//! Function provides next triangle edge through with the ray is leaving the triangle and returns whether the ray ends in the triangle or not.
	//! intersectingEdgeIndex is set to InvalidEdgeIndex if the ray ends in the triangle or there was no intersection found.
	bool FindNextIntersectingTriangleEdge(const vector3_t& rayStartPos, const vector3_t& rayEndPos, const vector2_t pVertices[3], real_t& rayIntersectionParam, uint16& intersectingEdgeIndex) const;

	//! Returns id of the neighbour triangle corresponding to the edge index of the current triangle or InvalidTriangleID if the edge is on navmesh boundaries
	template<typename TFilter>
	TriangleID StepOverEdgeToNeighbourTriangle(const vector3_t& rayStart, const vector3_t& rayEnd, const TileID currentTileID, const TriangleID currentTriangleID, const uint16 edgeIndex, const TFilter& filter) const;

	//! Filter for QueryTriangles, which accepts all triangles.
	//! Note, that the signature of PassFilter() function is same as INavMeshQueryFilter::PassFilter(), but it's not virtual.
	//! This way, templated implementation functions can avoid unnecessary checks and virtual calls.
	struct SAcceptAllQueryTrianglesFilter
	{
		inline bool PassFilter(const Tile::STriangle&) const { return true; }
		inline float GetCostMultiplier(const MNM::Tile::STriangle& triangle) const { return 1.0f; }
		inline MNM::real_t GetCost(const MNM::vector3_t& fromPos, const MNM::vector3_t& toPos,
			const MNM::Tile::STriangle& currentTriangle, const MNM::Tile::STriangle& nextTriangle,
			const MNM::TriangleID currentTriangleId, const MNM::TriangleID nextTriangleId) const
		{
			return (fromPos - toPos).lenNoOverflow();
		}
	};

	struct SMinIslandAreaQueryTrianglesFilter;

	template<typename TFilter>
	size_t QueryTrianglesWithFilterInternal(const aabb_t& queryAabbWorld, TFilter& filter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const;

	template<typename TFilter>
	size_t     QueryTileTriangles(const TileID tileID, const vector3_t& tileOrigin, const aabb_t& queryAabbWorld, TFilter& filter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const;
	template<typename TFilter>
	size_t     QueryTileTrianglesLinear(const TileID tileID, const STile& tile, const aabb_t& queryAabbTile, TFilter& filter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const;
	template<typename TFilter>
	size_t     QueryTileTrianglesBV(const TileID tileID, const STile& tile, const aabb_t& queryAabbTile, TFilter& filter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const;

	TriangleID FindClosestTriangleInternal(const vector3_t& queryPosWorld, const TriangleID* pCandidateTriangles, const size_t candidateTrianglesCount, vector3_t* pOutClosestPosWorld, real_t::unsigned_overflow_type* pOutClosestDistanceSq) const;

	size_t     GetTrianglesBordersNoFilter(const TriangleID* triangleIDs, const size_t triangleCount, Vec3* pBorders, const size_t maxBorderCount) const;
	size_t     GetTrianglesBordersWithFilter(const TriangleID* triangleIDs, const size_t triangleCount, const INavMeshQueryFilter& filter, Vec3* pBorders, const size_t maxBorderCount) const;

protected:
	void ComputeAdjacency(size_t x, size_t y, size_t z, const real_t& toleranceSq, STile& tile);
	void ReComputeAdjacency(size_t x, size_t y, size_t z, const real_t& toleranceSq, STile& tile,
	                        size_t side, size_t tx, size_t ty, size_t tz, TileID targetID);

	bool           IsLocationInTriangle(const vector3_t& location, const TriangleID triangleID) const;
	typedef VectorMap<TriangleID, TriangleID> RaycastCameFromMap;
	ERayCastResult ConstructRaycastResult(const ERayCastResult returnResult, const RayHit& rayHit, const TriangleID lastTriangleID, const RaycastCameFromMap& comeFromMap, RaycastRequestBase& raycastRequest) const;

	struct TileContainer
	{
		TileContainer()
			: x(0)
			, y(0)
			, z(0)
		{
		}

		uint32 x: x_bits;
		uint32 y: y_bits;
		uint32 z: z_bits;

		STile  tile;
	};

	// - manages memory used for TileContainer instances in one big block
	// - kind of an optimized std::vector<TileContainer>
	// - free TileContainers are treated as a list of free indexes
	// - the free list and a counter of used elements specify a "high-water" mark such that the free list needs not grow unnecessarily
	class TileContainerArray
	{
	public:
		TileContainerArray();

		// Custom copy-construction that bypasses the m_tiles[] pointer and its data from the rhs argument.
		// Objects of this class are currently only copy-constructed in CNavMesh's copy-ctor.
		// When the assert() in our ctor's body fails, a copy-construction obviously occured in a different situation, and we will have to rethink about the use-cases.
		TileContainerArray(const TileContainerArray& rhs);

		~TileContainerArray();

		// called only once (by CNavMesh::Init())
		void Init(size_t initialTileContainerCount);

		// - allocates a new TileContainer (or re-uses an unused one)
		// - the returned TileID-1 can then be used as index for operator[]
		// - never returns a TileID of value 0
		const TileID AllocateTileContainer();

		// - marks the TileContainer associated with given TileID as unused
		// - calling this function more than once in a row with the same TileID will fail an assert()
		void         FreeTileContainer(const TileID tileID);

		const size_t GetTileCount() const;

		// - access a TileContainer by index
		// - in most cases, you will have a TileID at hand, and must then pass in TileID-1
		TileContainer&       operator[](size_t index);
		const TileContainer& operator[](size_t index) const;

		void                 Swap(TileContainerArray& other);
		void                 UpdateProfiler(ProfilerType& profiler) const;

#if DEBUG_MNM_DATA_CONSISTENCY_ENABLED
		void BreakOnInvalidTriangle(const TriangleID triangleID) const;
#else
		void BreakOnInvalidTriangle(const TriangleID triangleID) const {}
#endif

	private:
		// Assignment not supported (we'd have to think about potential use-cases first as it's not inherently clear what should happen with the m_tiles pointer)
		// (VC++ 2012 does not yet support defaulted and deleted functions, so we still need to declare it in the private scope and omit the implementation)
		TileContainerArray& operator=(const TileContainerArray&);

		void                Grow(size_t amount);

	private:
		TileContainer* m_tiles;
		size_t         m_tileCount;
		size_t         m_tileCapacity;

		typedef std::vector<size_t> FreeIndexes;    // dynamically-resizing array of indexes pointing to unused elements in m_tiles[]
		FreeIndexes m_freeIndexes;
	};

	inline Tile::STriangle& GetTriangleUnsafe(const TileID tileID, const uint16 triangleIdx) const
	{
		const TileContainer& container = m_tiles[tileID - 1];

		CRY_ASSERT(triangleIdx < container.tile.triangleCount);
		return container.tile.triangles[triangleIdx];
	}

	TileContainerArray m_tiles;
	size_t             m_triangleCount;

	// replace with something more efficient will speed up path-finding, ray-casting and
	// connectivity computation
	typedef std::map<uint32, uint32> TileMap;
	TileMap             m_tileMap;

	CIslands            m_islands;

	SGridParams                          m_params;
	ProfilerType                         m_profiler;

	static const real_t                  kMinPullingThreshold;
	static const real_t                  kMaxPullingThreshold;
	static const real_t                  kAdjecencyCalculationToleranceSq;
};
}

#endif  // #ifndef __MNM_MESH_GRID_H

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/NavigationSystem/INavMeshQuery.h>
#include <CryAISystem/NavigationSystem/INavMeshQueryManager.h>
#include <CryAISystem/NavigationSystem/INavMeshQueryProcessing.h>
#include <CryAISystem/NavigationSystem/INavMeshQueryFilter.h>
#include <CryAISystem/NavigationSystem/NavMeshQueryFilterDefault.h>

#include "Tile.h"

#ifdef NAV_MESH_QUERY_DEBUG
#include "NavMeshQueryDebug.h"
#endif // NAV_MESH_QUERY_DEBUG

namespace MNM
{

namespace DefaultQueryFilters
{
	//! Global filter for QueryTriangles that is used when no user filter is provided.
	//! Note, that the signature of PassFilter(), GetCostMultiplier() and GetCost() functions is same as in INavMeshQueryFilter, but it's not virtual.
	//! This way, templated implementation functions can avoid unnecessary checks and virtual calls.
	struct SQueryTrianglesFilterGlobal
	{
		SQueryTrianglesFilterGlobal()
			: includeFlags(-1)
			, excludeFlags(0)
		{}
		
		inline bool PassFilter(const Tile::STriangle& triangle) const
		{
			return (triangle.areaAnnotation.GetFlags() & includeFlags) && !(triangle.areaAnnotation.GetFlags() & excludeFlags);
		}
		inline bool PassFilter(const MNM::AreaAnnotation annotation) const
		{
			return (annotation.GetFlags() & includeFlags) && !(annotation.GetFlags() & excludeFlags);
		}
		inline float GetCostMultiplier(const MNM::Tile::STriangle& triangle) const { return 1.0f; }
		inline float GetCostMultiplier(const MNM::AreaAnnotation annotation) const { return 1.0f; }

		MNM::AreaAnnotation::value_type includeFlags;
		MNM::AreaAnnotation::value_type excludeFlags;
	};
	
	// Use this one for functions that expect a templated filter type
	extern SQueryTrianglesFilterGlobal g_globalFilter;

	// Use this global filter for functions that expect a INavMeshQueryFilter type
	extern SNavMeshQueryFilterDefault g_globalFilterVirtual;

	// Use this 'accept all' for functions that expect a INavMeshQueryFilter type
	extern SAcceptAllQueryTrianglesFilter g_acceptAllFilterVirtual;
}

SERIALIZATION_ENUM_BEGIN_NESTED2(INavMeshQuery, SNavMeshQueryConfig, EOverlappingMode, "EOverlappingMode")
SERIALIZATION_ENUM(INavMeshQuery::SNavMeshQueryConfig::EOverlappingMode::BoundingBox_Partial, "BoundingBox_Partial", "Bounding Box Partial")
SERIALIZATION_ENUM(INavMeshQuery::SNavMeshQueryConfig::EOverlappingMode::BoundingBox_Full, "BoundingBox_Full", "Bounding Box Full")
SERIALIZATION_ENUM(INavMeshQuery::SNavMeshQueryConfig::EOverlappingMode::Triangle_Partial, "Triangle_Partial", "Triangle Partial")
SERIALIZATION_ENUM(INavMeshQuery::SNavMeshQueryConfig::EOverlappingMode::Triangle_Full, "Triangle_Full", "Triangle Full")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED2(INavMeshQuery, SNavMeshQueryConfigBatch, EActionOnNavMeshChange, "EActionOnNavMeshChange")
SERIALIZATION_ENUM(INavMeshQuery::SNavMeshQueryConfigBatch::EActionOnNavMeshChange::Ignore, "Ignore", "Ignore")
SERIALIZATION_ENUM(INavMeshQuery::SNavMeshQueryConfigBatch::EActionOnNavMeshChange::InvalidateAlways, "InvalidateAlways", "Ignore Always")
SERIALIZATION_ENUM(INavMeshQuery::SNavMeshQueryConfigBatch::EActionOnNavMeshChange::InvalidateIfProccesed, "InvalidateIfProccesed", "Invalidate if Processed")
SERIALIZATION_ENUM_END()


class CNavMeshQueryManager;

//! CNavMeshQuery base implementation for INavMeshQuery
//! Can't be instantiated. It is used as a base class for CNavMeshQueryInstant and CNavMeshQueryBatch
//! Holds common data and behavior along with some utility functions
class CNavMeshQuery : public INavMeshQuery
{
protected:
	//! CNavMeshTriangleIterator allows iteration over NavMesh triangles inside an AABB
	//! Internally it iterates over triangles within a tile and over tiles (Bounding Volumes or linear)
	class CTriangleIterator
	{
	public:
		struct STriangleInfo
		{
			const TileID              tileId;
			const TriangleID          triangleId;
			const uint16              triangleIdx;
			const Tile::STriangle     triangle;

			//! Builds an invalid TriangleInfo
			STriangleInfo()
				: tileId()
				, triangleId()
				, triangleIdx(0)
				, triangle()
			{
				// Empty
			}

			STriangleInfo(const TileID tileId_, const TriangleID triangleId_, const uint16 triangleIdx_, const Tile::STriangle& triangle_)
				: tileId(tileId_)
				, triangleId(triangleId_)
				, triangleIdx(triangleIdx_)
				, triangle(triangle_)
			{
				// Empty
			}

			bool IsValid() const
			{
				static const STriangleInfo invalidTriangleInfo = STriangleInfo();
				return tileId != invalidTriangleInfo.tileId && triangleId != invalidTriangleInfo.triangleId;
			}
		};

		CTriangleIterator();
		CTriangleIterator(const NavigationMeshID meshId, const aabb_t& queryAabb);
		CTriangleIterator(const CTriangleIterator& other) = default;
		CTriangleIterator& operator=(const CTriangleIterator&) = default;
		~CTriangleIterator() = default;


		NavigationMeshID       GetMeshId() const;
		const CNavMesh*        GetNavMesh() const;

		size_t                 GetMinTileX() const;
		size_t                 GetMinTileY() const;
		size_t                 GetMinTileZ() const;

		size_t                 GetMaxTileX() const;
		size_t                 GetMaxTileY() const;
		size_t                 GetMaxTileZ() const;

		//! Returns true if there's a non-visited triangle inside the AABB
		bool                   HasNext() const;

		//! Updates the stored NavMesh pointer from the stored mesh id.
		//! Should be called first at the beginning of every frame that we want to use the iterator
		//! This way we ensure that NavMesh pointer is still valid before performing any operation
		//! /return True if it was successfully updated
		bool                   UpdateNavMeshPointer();

		//! Moves the iterator to the next triangle in the NavMesh
		//! /return True if the iterator sucessfully moved to the next triangle (hasNext() was true)
		bool                   Next();

		//! Get the TriangleInfo of the triangle currently pointed by the iterator
		STriangleInfo           GetTriangle() const;

	private:
		bool                   IsEnabledBoundingVolumes() const;

		bool                   NextTile();

		bool                   NextTriangleLinear();
		STriangleInfo           GetTriangleLinear() const;

		bool                   NextTriangleBoundingVolumes();
		STriangleInfo           GetTriangleBoundingVolumes() const;

		NavigationMeshID       m_meshId;
		const CNavMesh*        m_pNavMesh;

		// Min/Max members could be const but initialization in the ctor would be less efficient
		size_t                 m_minTileX;
		size_t                 m_minTileY;
		size_t                 m_minTileZ;

		size_t                 m_maxTileX;
		size_t                 m_maxTileY;
		size_t                 m_maxTileZ;

		TileID                 m_tileId;
		size_t                 m_tileX;
		size_t                 m_tileY;
		size_t                 m_tileZ;
		uint16                 m_triangleIdxTile;

		bool                   m_hasNext;
	};

public:
	virtual NavMeshQueryId              GetId() const override final;
	virtual const SNavMeshQueryConfig&  GetQueryConfig() const override final;
	virtual INavMeshQuery::EQueryStatus GetStatus() const override final;
	virtual bool                        IsValid() const override final;
	virtual bool                        IsDone() const override final;
	virtual bool                        IsRunning() const override final;

#ifdef NAV_MESH_QUERY_DEBUG
	virtual const INavMeshQueryDebug&   GetQueryDebug() const override final;
	virtual bool                        operator< (const INavMeshQuery & other) const override final;
#endif // NAV_MESH_QUERY_DEBUG

	virtual INavMeshQuery::EQueryStatus Run(INavMeshQueryProcessing& queryProcessing, const size_t processingBatchSize = 32) override final;

protected:
	CNavMeshQuery(const NavMeshQueryId queryId, const SNavMeshQueryConfig& queryConfig);

	CNavMeshQuery(const CNavMeshQuery&) = default;
	CNavMeshQuery(CNavMeshQuery&&) = default;
	~CNavMeshQuery() = default;

	void                                SetStatus(const INavMeshQuery::EQueryStatus status);

	virtual INavMeshQuery::EQueryStatus QueryTiles(INavMeshQueryProcessing& queryProcessing, const size_t processingBatchSize) = 0;

#ifdef NAV_MESH_QUERY_DEBUG
	void                                DebugQueryResult(const CTimeValue& timeAtStart);
	void                                DebugDrawQueryConfig() const;
	void                                DebugDrawQueryBatch(const MNM::INavMeshQueryDebug::SBatchData& queryBatch) const;
	void                                DebugDrawQueryInvalidation(const MNM::INavMeshQueryDebug::SInvalidationData& invalidationData) const;
#endif // NAV_MESH_QUERY_DEBUG

	aabb_t                              CaculateQueryAabbInTileSpace(TileID tileId) const;
	bool                                TriangleIntersectsQueryAabb(const aabb_t& queryAabbInTileSpace, const CTriangleIterator::STriangleInfo& triangleInfo) const;

private:
	bool                                Initialize();

protected:
	CTriangleIterator                   m_triangleIterator;
#ifdef NAV_MESH_QUERY_DEBUG
	size_t                              m_batchCount;
	CNavMeshQueryDebug                  m_queryDebug;
	TriangleIDArray                 m_trianglesDebug;
#endif // NAV_MESH_QUERY_DEBUG

private:
	const NavMeshQueryId                m_queryId;
	const SNavMeshQueryConfig&          m_queryConfig;
	EQueryStatus                        m_status;
};

//! CNavMeshQueryInstant is used to perform queries that will run only once during one frame.
class CNavMeshQueryInstant final : public CNavMeshQuery
{
	typedef CNavMeshQuery BaseClass;

	friend class CNavMeshQueryManager;
private:
	CNavMeshQueryInstant(const NavMeshQueryId queryId, const INavMeshQuery::SNavMeshQueryConfigInstant& queryConfig);

	CNavMeshQueryInstant(const CNavMeshQueryInstant&) = default;
	CNavMeshQueryInstant(CNavMeshQueryInstant&&) = default;
	~CNavMeshQueryInstant() = default;

	virtual INavMeshQuery::EQueryStatus QueryTiles(INavMeshQueryProcessing& queryProcessing, const size_t processingBatchSize) override;

	template <typename TFilter>
	INavMeshQuery::EQueryStatus         QueryTilesInternal(INavMeshQueryProcessing& queryProcessing, const TFilter& filter, const size_t processingBatchSize);

	const SNavMeshQueryConfigInstant    m_queryConfig;
};

//! CNavMeshQueryBatch is used to perform queries that will run in batches.
//! As opposed to CNavMeshQueryInstant, these queries can run multiple times during one or multiple frames.
//! During multiple frame execution, NavMesh may get regenerated, possibly affecting the query and/or invalidating it.
//! The query keeps an internal state in order to be able to run/stop/resume execution.
//! This allows us to execute queries in batches and during multiple frames.
class CNavMeshQueryBatch final : public CNavMeshQuery
{
	typedef CNavMeshQuery BaseClass;
	typedef std::unordered_set<TileID> TilesID;

	friend class CNavMeshQueryManager;

	struct RuntimeData
	{
		NavigationAgentTypeID navAgentTypeId;
		TilesID               processedTilesId;
	};

private:
	CNavMeshQueryBatch(const NavMeshQueryId queryId, const INavMeshQuery::SNavMeshQueryConfigBatch& queryConfig);
	~CNavMeshQueryBatch();

	CNavMeshQueryBatch(const CNavMeshQueryBatch&) = default;
	CNavMeshQueryBatch(CNavMeshQueryBatch&&) = default;

	void                                InitializeRuntimeData();

	virtual INavMeshQuery::EQueryStatus QueryTiles(INavMeshQueryProcessing& queryProcessing, const size_t processingBatchSize) override;
	template <typename TFilter>
	INavMeshQuery::EQueryStatus         QueryTilesInternal(INavMeshQueryProcessing& queryProcessing, const TFilter& filter, const size_t processingBatchSize);

	void                                RegisterNavMeshCallbacks();
	void                                DeregisterNavMeshCallbacks();

	void                                OnNavMeshChanged(NavigationAgentTypeID navAgentId, NavigationMeshID navMeshId, TileID tileId);
	void                                OnNavMeshAnnotationChanged(NavigationAgentTypeID navAgentId, NavigationMeshID navMeshId, TileID tileId);

	bool                                IsTileProcessed(const TileID tileId) const;
	void                                SetTileProcessed(const TileID tileId);

#ifdef NAV_MESH_QUERY_DEBUG
	void                                DebugInvalidation(const TileID tileId, const INavMeshQueryDebug::EInvalidationType invalidationType);
#endif //NAV_MESH_QUERY_DEBUG

	const SNavMeshQueryConfigBatch      m_queryConfig;
	RuntimeData                         m_runtimeData;
};
} // namespace MNM

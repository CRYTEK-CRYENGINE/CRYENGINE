// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef __MNM_TILE_GENERATOR_H
#define __MNM_TILE_GENERATOR_H

#pragma once

#include "MNM.h"
#include "MNMProfiler.h"
#include "CompactSpanGrid.h"
#include "Tile.h"
#include "BoundingVolume.h"
#include "HashComputer.h"

#include <CryAISystem/NavigationSystem/MNMTileGenerator.h>

#include <CryMath/SimpleHashLookUp.h>

#include <CryCore/Containers/CryListenerSet.h>

#if DEBUG_MNM_ENABLED
	#define DEBUG_MNM_GATHER_NONWALKABLE_REASONS       (1)
	#define DEBUG_MNM_GATHER_EXTRA_CONTOUR_VERTEX_INFO (1)
#endif // DEBUG_MNM_ENABLED

struct IExternalNavigationMeshProvider;

namespace MNM
{

struct STileGeneratorExtensionsContainer
{
	typedef VectorMap<TileGeneratorExtensionID, MNM::TileGenerator::IExtension*> TileGeneratorExtensionPtrsMap;

	TileGeneratorExtensionPtrsMap extensions;
	TileGeneratorExtensionID      idCounter;
	mutable CryReadModifyLock     extensionsLock;
};

struct SSpanCoord
{
	struct Hash
	{
		size_t operator()(const SSpanCoord& coord) const { return std::hash<size_t>()(coord.spanAbsIdx); }
	};

	SSpanCoord(size_t x, size_t y, size_t s, size_t spanAbsIdx_)
		: cellX(x)
		, cellY(y)
		, spanIdx(s)
		, spanAbsIdx(spanAbsIdx_)
	{}

	bool operator==(const SSpanCoord& other) const
	{
		return (spanAbsIdx == other.spanAbsIdx) && (cellX == other.cellX) && (cellY == other.cellY) && (spanIdx == other.spanIdx);
	}

	size_t cellX;
	size_t cellY;
	size_t spanIdx;
	size_t spanAbsIdx;
};

class CTileGenerator
{
public:
	enum
	{
		MaxTileSizeX = 18,
		MaxTileSizeY = 18,
		MaxTileSizeZ = 18,
	};

	struct Params
	{
		Params()
			: origin(ZERO)
			, sizeX(8)
			, sizeY(8)
			, sizeZ(8)
			, voxelSize(0.1f)
			, climbableInclineGradient(0.0f)
			, climbableStepRatio(0.0f)
			, flags(0)
			, blurAmount(0)
			, minWalkableArea(16)
			, boundary(nullptr)
			, exclusions(nullptr)
			, exclusionCount(0)
			, hashValue(0)
			, pTileGeneratorExtensions(nullptr)
		{}

		enum Flags
		{
			NoBorder    = 1 << 0,
			NoErosion   = 1 << 1,
			NoHashTest  = 1 << 2,
			BuildBVTree = 1 << 3,
			DebugInfo   = 1 << 7,
		};

		Vec3   origin;
		Vec3   voxelSize;
		float  climbableInclineGradient;
		float  climbableStepRatio;

		uint16 flags;
		uint16 minWalkableArea;
		uint16 exclusionCount;

		uint8  blurAmount;
		uint8  sizeX;
		uint8  sizeY;
		uint8  sizeZ;

		struct AgentSettings
		{
			AgentSettings()
				: radius(4)
				, height(18)
				, climbableHeight(4)
				, maxWaterDepth(8)
				, callback()
			{}

			uint32                       radius          : 8;
			uint32                       height          : 8;
			uint32                       climbableHeight : 8;
			uint32                       maxWaterDepth   : 8;

			NavigationMeshEntityCallback callback;
		}                                        agent;

		const BoundingVolume*                    boundary;
		const BoundingVolume*                    exclusions;
		uint32                                   hashValue;

		const STileGeneratorExtensionsContainer* pTileGeneratorExtensions;
		NavigationAgentTypeID                    navAgentTypeId;
	};

	enum ProfilerTimers
	{
		Voxelization = 0,
		Filter,
		DistanceTransform,
		Blur,
		ContourExtraction,
		Simplification,
		Triangulation,
		BVTreeConstruction,
	};

	enum ProfilerMemoryUsers
	{
		DynamicSpanGridMemory = 0,
		CompactSpanGridMemory,
		SegmentationMemory,
		RegionMemory,
		PolygonMemory,
		TriangulationMemory,
		VertexMemory,
		TriangleMemory,
		BVTreeConstructionMemory,
		BVTreeMemory,
	};

	enum ProfilerStats
	{
		VoxelizationTriCount = 0,
		RegionCount,
		PolygonCount,
		TriangleCount,
		VertexCount,
		BVTreeNodeCount,
	};

	enum class EDrawMode
	{
		DrawNone = 0,
		DrawRawInputGeometry,
		DrawRawVoxels,
		DrawFlaggedVoxels,
		DrawFilteredVoxels,
		DrawDistanceTransform,
		DrawPainting,
		DrawSegmentation,
		DrawTracers,
		DrawContourVertices,
		DrawNumberedContourVertices,
		DrawSimplifiedContours,
		DrawTriangulation,
		DrawBVTree,
		LastDrawMode,
	};

	bool Generate(const Params& params, STile& tile, uint32* hashValue);
	void Draw(const EDrawMode mode, const bool bDrawAdditionalInfo) const;

	typedef MNMProfiler<ProfilerMemoryUsers, ProfilerTimers, ProfilerStats> ProfilerType;
	const ProfilerType& GetProfiler() const;

	enum SpanFlags
	{
		NotWalkable  = BIT(0),
		TileBoundary = BIT(1),
	};

	enum Labels
	{
		FirstInvalidLabel = BIT(12) - 1,
		NoLabel           = FirstInvalidLabel,
		ExternalContour   = BIT(12),
		InternalContour   = BIT(13),
		BorderLabelH      = BIT(14),
		BorderLabelV      = BIT(15),
	};

	enum Paint
	{
		NoPaint = 0,
		BadPaint,
		OkPaintStart,
	};

protected:

	typedef std::vector<uint16> SpanExtraInfo;

	struct ContourVertex
	{
		ContourVertex()
#if DEBUG_MNM_GATHER_EXTRA_CONTOUR_VERTEX_INFO
			: debugInfoIndex(~0)
#endif
		{}

		ContourVertex(uint16 _x, uint16 _y, uint16 _z)
			: x(_x)
			, y(_y)
			, z(_z)
			, flags(0)
#if DEBUG_MNM_GATHER_EXTRA_CONTOUR_VERTEX_INFO
			, debugInfoIndex(~0)
#endif
		{}

		enum Flags
		{
			TileBoundary  = BIT(0),
			TileBoundaryV = BIT(1),
			Unremovable   = BIT(2),
			TileSideA     = BIT(4),
			TileSideB     = BIT(5),
			TileSideC     = BIT(6),
			TileSideD     = BIT(7),
			TileSides     = (TileSideA | TileSideB | TileSideC | TileSideD),
		};

		bool operator<(const ContourVertex& other) const
		{
			if (x == other.x)
				return y < other.y;
			else
				return x < other.x;
		}

		bool operator==(const ContourVertex& other) const
		{
			return (x == other.x) && (y == other.y) && (z == other.z);
		}

		uint16 x;
		uint16 y;
		uint16 z;
		uint16 flags;
#if DEBUG_MNM_GATHER_EXTRA_CONTOUR_VERTEX_INFO
		size_t debugInfoIndex;
#endif
	};

	typedef std::vector<ContourVertex> Contour;
	typedef std::vector<Contour>       Contours;

	struct PolygonVertex
	{
		enum Flags
		{
			Reflex = BIT(0),
			Ear    = BIT(1),
		};

		PolygonVertex(uint16 _x, uint16 _y, uint16 _z)
			: x(_x)
			, y(_y)
			, z(_z)
			, flags(0)
		{}

		uint16 x;
		uint16 y;
		uint16 z;
		uint16 flags;

		bool operator<(const ContourVertex& other) const
		{
			if (x == other.x)
				return y < other.y;
			else
				return x < other.x;
		}

		bool operator==(const ContourVertex& other) const
		{
			return (x == other.x) && (y == other.y) && (z == other.z);
		}
	};

	typedef std::vector<PolygonVertex> PolygonContour;

	struct Hole
	{
		PolygonContour verts;
		Vec2i          center;
		int            rad;
	};

	typedef std::vector<Hole>          PolygonHoles;
	typedef std::vector<PolygonVertex> VoxelContour;

	struct Polygon
	{
		PolygonContour contour;
		PolygonHoles   holes;
	};

	struct Region
	{
		Region()
			: spanCount(0)
			, flags(0)
		{}

		enum Flags
		{
			TileBoundary  = BIT(0),
			TileBoundaryV = BIT(1),
		};

		Contour  contour;
		Contours holes;

		size_t   spanCount;
		size_t   flags;

		void     swap(Region& other)
		{
			std::swap(spanCount, other.spanCount);

			contour.swap(other.contour);
			holes.swap(other.holes);
		}
	};

	class CGeneratedMesh : public TileGenerator::IMesh
	{
	public:
		typedef uint64                                               TileVertexKey;
		typedef simple_hash_lookup<TileVertexKey, Tile::VertexIndex> TileVertexIndexLookUp;
		typedef std::vector<Tile::STriangle>                         Triangles;
		typedef std::vector<Tile::Vertex>                            Vertices;

	public:
		void Clear();
		void SetTileAabb(const AABB& tileAabbWorld) { m_tileAabb = tileAabbWorld; }

		bool IsEmpty() const                        { return m_triangles.empty(); }
		void CopyIntoTile(STile& tile) const;

		// Functions for Triangulate()
		void             Reserve(size_t trianglesCount, size_t verticesCount);
		size_t           InsertVertex(const Tile::Vertex& vertex);
		Tile::STriangle& InsertTriangle();

		// TileGenerator::IMesh
		virtual bool AddTrianglesWorld(const Triangle* pTriangles, const size_t count, const Tile::STriangle::EFlags flags) override;
		// ~TileGenerator::IMesh

		void                 AddStatsToProfiler(ProfilerType& profiler) const;

		const Triangles&     GetTriangles() const { return m_triangles; }
		const Vertices&      GetVertices() const  { return m_vertices; }

		void                 ResetHashSeed(uint32 hashSeed);
		uint32               CompleteAndGetHashValue();

		static TileVertexKey GetKeyFromTileVertex(const Tile::Vertex& vtx);

	private:
		enum { k_maxTriangleCount = 1024 };

		Triangles             m_triangles;
		Vertices              m_vertices;
		TileVertexIndexLookUp m_vertexIndexLookUp;
		AABB                  m_tileAabb;
		HashComputer          m_hashComputer;
	};

	// Call this when reusing an existing TileGenerator for a second job.
	// Clears all the data but leaves the allocated memory.
	void                 Clear();

	static size_t        BorderSizeH(const Params& params);
	static size_t        BorderSizeV(const Params& params);

	inline static size_t Top(const Params& params)
	{
		// TODO pavloi 2016.03.10: is 0.1f added to deal with some numerical issues?
		return (size_t)(params.sizeZ / params.voxelSize.z + 0.10f);
	}

	inline bool IsBorderCell(size_t x, size_t y) const
	{
		const size_t border = BorderSizeH(m_params);
		const size_t width = m_spanGrid.GetWidth();
		const size_t height = m_spanGrid.GetHeight();

		return (x < border) || (x >= width - border) || (y < border) || (y >= height - border);
	}

	inline bool IsBoundaryCell_Static(size_t x, size_t y, const size_t border, const size_t width, const size_t height) const
	{
		return (((x == border) || (x == width - border - 1)) && (y >= border) && (y <= height - border - 1))
		       || (((y == border) || (y == height - border - 1)) && (x >= border) && (x <= width - border - 1));
	}

	inline bool IsBoundaryCell(size_t x, size_t y) const
	{
		const size_t border = BorderSizeH(m_params);
		const size_t width = m_spanGrid.GetWidth();
		const size_t height = m_spanGrid.GetHeight();

		return (((x == border) || (x == width - border - 1)) && (y >= border) && (y <= height - border - 1))
		       || (((y == border) || (y == height - border - 1)) && (x >= border) && (x <= width - border - 1));
	}

	inline bool IsBoundaryVertex(size_t x, size_t y) const
	{
		const size_t border = BorderSizeH(m_params);
		const size_t width = m_spanGrid.GetWidth();
		const size_t height = m_spanGrid.GetHeight();

		return (((x == border) || (x == width - border)) && (y >= border) && (y <= height - border))
		       || (((y == border) || (y == height - border)) && (x >= border) && (x <= width - border));
	}

	inline bool IsCornerVertex(size_t x, size_t y) const
	{
		const size_t border = BorderSizeH(m_params);
		const size_t width = m_spanGrid.GetWidth();
		const size_t height = m_spanGrid.GetHeight();

		return (((x == border) || (x == width - border)) && ((y == border) || (y == height - border)));
	}

	inline bool IsBoundaryVertexV(size_t z) const
	{
		const size_t borderV = BorderSizeV(m_params);

		return (z == borderV) || (z == Top(m_params) + borderV);
	}

	size_t VoxelizeVolume(const AABB& volume, uint32 hashValueSeed = 0, uint32* hashValue = 0);

	// Filter walkable and helpers
	struct SFilterWalkableParams;
	struct SSpanClearance;
	struct SNonWalkableNeighbourReason;

	bool        GenerateFromVoxelizedVolume(const AABB& aabb, const bool fullyContained);

	void        FilterWalkable(const AABB& aabb, bool fullyContained = true);
	static bool FilterWalkable_CheckSpanBackface(const CompactSpanGrid::Span& span);
	static bool FilterWalkable_CheckSpanWaterDepth(const CompactSpanGrid::Span& span, const Params& params);
	static bool FilterWalkable_CheckNeighbours(const SSpanCoord& spanCoord, const SSpanClearance& spanClearance, const SFilterWalkableParams& filterParams, SNonWalkableNeighbourReason* pOutReason);
	void        FilterWalkable_CheckBoundaries(const AABB& aabb, const size_t gridWidth, const size_t gridHeight);

	void        ComputeDistanceTransform();
	void        BlurDistanceTransform();

	void        PaintBorder(uint16* data, size_t borderH, size_t borderV);

	struct NeighbourInfoRequirements
	{
		NeighbourInfoRequirements()
			: paint(NoPaint)
			, notPaint(NoPaint)
		{}
		uint16 paint, notPaint;
	};

	struct NeighbourInfo
	{
		NeighbourInfo(const Vec3i& pos)
			: pos(pos.x, pos.y)
			, top(pos.z)
			, index(~0ul)
			, label(CTileGenerator::NoLabel)
			, paint(NoPaint)
			, isValid(false)
		{}
		NeighbourInfo(const Vec2i& xy, const size_t z)
			: pos(xy)
			, top(z)
			, index(~0ul)
			, label(CTileGenerator::NoLabel)
			, paint(NoPaint)
			, isValid(false)
		{}
		const Vec2i pos;
		size_t      top;
		size_t      index;
		uint16      label;
		uint16      paint;
		bool        isValid;
		ILINE bool  Check(const NeighbourInfoRequirements& req) const
		{
			return isValid &&
			       (req.paint == NoPaint || req.paint == paint) &&
			       (req.notPaint == NoPaint || req.notPaint != paint);
		}
	};

	uint16 GetPaintVal(size_t x, size_t y, size_t z, size_t index, size_t borderH, size_t borderV, size_t erosion);
	void   AssessNeighbour(NeighbourInfo& info, size_t erosion, size_t climbableVoxelCount);

	enum TracerDir
	{
		N,    //  0,+1
		E,    // +1, 0
		S,    //  0,-1
		W,    // -1, 0
		TOTAL_DIR
	};

	struct Tracer
	{
		Vec3i  pos;
		int    dir;
		size_t indexIn;
		size_t indexOut;
		bool   bPinchPoint;

		ILINE bool  operator==(const Tracer& rhs) const { return pos == rhs.pos && dir == rhs.dir; }
		ILINE bool  operator!=(const Tracer& rhs) const { return pos != rhs.pos || dir != rhs.dir; }
		ILINE void  SetPos(const NeighbourInfo& info)   { pos = info.pos; pos.z = info.top; }
		ILINE void  TurnRight()                         { dir = ((dir + 1) & 3); }
		ILINE void  TurnLeft()                          { dir = ((dir + 3) & 3); }
		ILINE Vec2i GetDir() const                      { return Vec2i((dir & 1) * sgn(2 - dir), ((dir + 1) & 1) * sgn(1 - dir)); }
		ILINE Vec3i GetFront() const                    { return pos + GetDir(); }
		ILINE Vec3i GetLeft() const                     { return pos + (GetDir().rot90ccw()); }
		ILINE Vec3i GetFrontLeft() const                { const Vec2i dv(GetDir()); return pos + dv + (dv.rot90ccw()); }
	};

	struct TracerPath
	{
		TracerPath() : turns(0) {}
		typedef std::vector<Tracer> Tracers;
		Tracers steps;
		int     turns;
	};

	void   TraceContour(CTileGenerator::TracerPath& path, const Tracer& start, size_t erosion, size_t climbableVoxelCount, const NeighbourInfoRequirements& contourReq);
	int    LabelTracerPath(const CTileGenerator::TracerPath& path, size_t climbableVoxelCount, Region& region, Contour& contour, const uint16 internalLabel, const uint16 internalLabelFlags, const uint16 externalLabel);

	void   TidyUpContourEnd(Contour& contour);
	size_t ExtractContours();
	void   CalcPaintValues();

	void   FilterBadRegions(size_t minSpanCount);

	bool   SimplifyContour(const Contour& contour, const real_t& tolerance2DSq, const real_t& tolerance3DSq,
	                       PolygonContour& poly);
	void   SimplifyContours();

	typedef simple_hash_lookup<uint32, uint16> VertexIndexLookUp;
	void       MergeHole(PolygonContour& contour, const size_t contourVertex, const PolygonContour& hole, const size_t holeVertex, const int distSqr) const;
	size_t     Triangulate(PolygonContour& contour, const size_t agentHeight, const size_t borderH, const size_t borderV,
	                       VertexIndexLookUp& lookUp);
	size_t     Triangulate();
	void       BuildBVTree();

	ILINE void CacheTracerPath(const TracerPath& path)
	{
#ifndef _RELEASE
		if (m_params.flags & Params::DebugInfo)
		{
			// TODO pavloi 2016.03.15: m_tracerPaths is not accounted in memory profiler
			m_tracerPaths.push_back(path);
		}
#endif      //_RELEASE
	}

	struct SurroundingSpanInfo
	{
		SurroundingSpanInfo(uint16 _label, size_t _index, size_t _flags = 0)
			: flags(_flags)
			, label(_label)
			, index(_index)
		{}
		size_t flags;
		size_t index;
		uint16 label;
	};

	enum NeighbourClassification
	{
		UW = 0,   // not walkable
		NB = 1,   // walkable, not border
		WB = 2,   // walkable, border
	};

	struct SContourVertexDebugInfo;

	inline NeighbourClassification ClassifyNeighbour(const SurroundingSpanInfo& neighbour,
	                                                 size_t erosion, size_t borderFlag) const
	{
		if (((neighbour.flags & NotWalkable) != 0) || (m_distances[neighbour.index] < erosion))
			return UW;
		return (neighbour.label & borderFlag) ? WB : NB;
	}

	bool GatherSurroundingInfo(const Vec2i& vertex, const Vec2i& direction, const uint16 top,
	                           const uint16 climbableVoxelCount, size_t& height, SurroundingSpanInfo& left, SurroundingSpanInfo& front,
	                           SurroundingSpanInfo& frontLeft) const;
	void        DetermineContourVertex(const Vec2i& vertex, const Vec2i& direction, uint16 top,
	                                   uint16 climbableVoxelCount, ContourVertex& contourVertex, const bool bInternalContour, SContourVertexDebugInfo* pDebugInfo) const;
	inline bool ContourVertexRemovable(const ContourVertex& contourVertex) const
	{
		return ((contourVertex.flags & ContourVertex::TileBoundary) == 0)
		       && ((contourVertex.flags & ContourVertex::Unremovable) == 0);
	}

	void   AddContourVertex(const ContourVertex& vertex, Region& region, Contour& contour) const;

	size_t InsertUniqueVertex(VertexIndexLookUp& lookUp, size_t x, size_t y, size_t z);

	void   DrawNavTriangulation() const;
	void   DrawTracers(const Vec3& origin) const;
	void   DrawSimplifiedContours(const Vec3& origin) const;
	void   DrawSegmentation(const Vec3& origin, const bool bDrawAdditionalInfo) const;

	Params         m_params;
	ProfilerType   m_profiler;

	CGeneratedMesh m_mesh;

	typedef std::vector<Tile::SBVNode> BVTree;
	BVTree          m_bvtree;

	CompactSpanGrid m_spanGrid;

	SpanExtraInfo   m_distances;
	SpanExtraInfo   m_labels;
	SpanExtraInfo   m_paint;

	typedef std::vector<Region> Regions;
	Regions m_regions;

	typedef std::vector<Polygon> Polygons;
	Polygons m_polygons;

	typedef std::vector<TracerPath> TracerPaths;
	TracerPaths     m_tracerPaths;

	CompactSpanGrid m_spanGridRaw;
	CompactSpanGrid m_spanGridFlagged;

#if DEBUG_MNM_ENABLED
	std::vector<Triangle> m_debugRawGeometry;
	AABB                  m_debugVoxelizedVolume;
#endif // DEBUG_MNM_ENABLED

#if DEBUG_MNM_GATHER_NONWALKABLE_REASONS

	struct SNonWalkableNeighbourReason
	{
		SNonWalkableNeighbourReason()
			: szReason(nullptr)
		{}

		const char* szReason;
	};

	struct SNonWalkableReason
	{
		SNonWalkableReason(const char* szReason_ = nullptr)
			: szReason(szReason_)
			, bIsNeighbourReason(false)
			, neighbourReason()
		{}

		SNonWalkableReason(const char* szReason_, const SNonWalkableNeighbourReason& neighbourReason_)
			: szReason(szReason_)
			, bIsNeighbourReason(true)
			, neighbourReason(neighbourReason_)
		{}

		const char*                 szReason;
		bool                        bIsNeighbourReason;
		SNonWalkableNeighbourReason neighbourReason;
	};

	typedef std::unordered_map<SSpanCoord, SNonWalkableReason, SSpanCoord::Hash> NonWalkableSpanReasonMap;

	friend class CNonWalkableInfoPrinter;

	NonWalkableSpanReasonMap m_debugNonWalkableReasonMap;

#endif // DEBUG_MNM_GATHER_NONWALKABLE_REASONS

	inline void DebugAddNonWalkableReason(const SSpanCoord& spanCoord, const char* szReason)
	{
#if DEBUG_MNM_GATHER_NONWALKABLE_REASONS
		IF_UNLIKELY (m_params.flags & Params::DebugInfo)
		{
			m_debugNonWalkableReasonMap[spanCoord] = SNonWalkableReason(szReason);
		}
#endif // DEBUG_MNM_GATHER_NONWALKABLE_REASONS
	}

	friend class CContourRenderer;
#if DEBUG_MNM_GATHER_EXTRA_CONTOUR_VERTEX_INFO
	struct SContourVertexDebugInfo
	{
		SContourVertexDebugInfo()
			: unremovableReason()
			, tracer()
			, tracerIndex(-1)
			, lclass(UW)
			, flclass(UW)
			, fclass(UW)
			, walkableBit(0)
			, borderBitH(0)
			, borderBitV(0)
			, internalBorderV(false)
		{}

		CryFixedStringT<64>     unremovableReason;
		Tracer                  tracer;
		int                     tracerIndex;
		NeighbourClassification lclass;
		NeighbourClassification flclass;
		NeighbourClassification fclass;
		uint8                   walkableBit;
		uint8                   borderBitH;
		uint8                   borderBitV;
		bool                    internalBorderV;
	};

	typedef std::vector<SContourVertexDebugInfo> ContourVertexDebugInfos;
	ContourVertexDebugInfos m_debugContourVertexDebugInfos;
	mutable Contour         m_debugDiscardedVertices;
#endif // DEBUG_MNM_GATHER_EXTRA_CONTOUR_VERTEX_INFO

	void DebugAddContourVertexUnremovableReason(SContourVertexDebugInfo* pDebugInfo, const char* szUnremovableReason) const
	{
#if DEBUG_MNM_GATHER_EXTRA_CONTOUR_VERTEX_INFO
		IF_UNLIKELY (pDebugInfo)
		{
			if (!pDebugInfo->unremovableReason.empty())
			{
				pDebugInfo->unremovableReason += '\n';
			}
			pDebugInfo->unremovableReason += szUnremovableReason;
		}
#endif
	}
};
}

#endif  // #ifndef __MNM_TILE_GENERATOR_H

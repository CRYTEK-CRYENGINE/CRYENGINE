// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MNM_TILE_GENERATOR_H
#define __MNM_TILE_GENERATOR_H

#pragma once

#include "MNM.h"
#include "MNMProfiler.h"
#include "CompactSpanGrid.h"
#include "Tile.h"
#include "BoundingVolume.h"

#include <CryMath/SimpleHashLookUp.h>

namespace MNM
{
class TileGenerator
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
			, boundary(0)
			, exclusions(0)
			, exclusionCount(0)
			, hashValue(0)
		{
		}

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
			{
			}

			uint32                       radius          : 8;
			uint32                       height          : 8;
			uint32                       climbableHeight : 8;
			uint32                       maxWaterDepth   : 8;

			NavigationMeshEntityCallback callback;
		}                     agent;

		const BoundingVolume* boundary;
		const BoundingVolume* exclusions;
		uint32                hashValue;
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

	enum DrawMode
	{
		DrawNone = 0,
		DrawRawVoxels,
		DrawFlaggedVoxels,
		DrawDistanceTransform,
		DrawSegmentation,
		DrawContourVertices,
		DrawNumberedContourVertices,
		DrawTracers,
		DrawSimplifiedContours,
		DrawTriangulation,
		DrawBVTree,
		LastDrawMode,
	};

	bool Generate(const Params& params, Tile& tile, uint32* hashValue);
	void Draw(DrawMode mode) const;

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
		{
		}

		ContourVertex(uint16 _x, uint16 _y, uint16 _z)
			: x(_x)
			, y(_y)
			, z(_z)
			, flags(0)
		{
		}

		uint16 x;
		uint16 y;
		uint16 z;
		uint16 flags;

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
		{
		}

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
		{
		}

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

	// Call this when reusing an existing TileGenerator for a second job.
	// Clears all the data but leaves the allocated memory.
	void          Clear();

	inline size_t BorderSizeH() const
	{
		return (m_params.flags & Params::NoBorder) ? 0 : ((m_params.agent.radius & ~1) + 2);
	}

	inline size_t BorderSizeV() const
	{
		return (m_params.flags & Params::NoBorder) ? 0 : ((m_params.agent.radius & ~1) + 2);
	}

	inline bool IsBorderCell(size_t x, size_t y) const
	{
		const size_t border = BorderSizeH();
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
		const size_t border = BorderSizeH();
		const size_t width = m_spanGrid.GetWidth();
		const size_t height = m_spanGrid.GetHeight();

		return (((x == border) || (x == width - border - 1)) && (y >= border) && (y <= height - border - 1))
		       || (((y == border) || (y == height - border - 1)) && (x >= border) && (x <= width - border - 1));
	}

	inline bool IsBoundaryVertex(size_t x, size_t y) const
	{
		const size_t border = BorderSizeH();
		const size_t width = m_spanGrid.GetWidth();
		const size_t height = m_spanGrid.GetHeight();

		return (((x == border) || (x == width - border)) && (y >= border) && (y <= height - border))
		       || (((y == border) || (y == height - border)) && (x >= border) && (x <= width - border));
	}

	inline bool IsCornerVertex(size_t x, size_t y) const
	{
		const size_t border = BorderSizeH();
		const size_t width = m_spanGrid.GetWidth();
		const size_t height = m_spanGrid.GetHeight();

		return (((x == border) || (x == width - border)) && ((y == border) || (y == height - border)));
	}

	inline bool IsBoundaryVertexV(size_t z) const
	{
		const size_t borderV = BorderSizeV();

		return (z == borderV) || (z == m_top + borderV);
	}

	size_t VoxelizeVolume(const AABB& volume, uint32 hashValueSeed = 0, uint32* hashValue = 0);
	void   FilterWalkable(const AABB& aabb, bool fullyContained = true);
	void   ComputeDistanceTransform();
	void   BlurDistanceTransform();

	void   PaintBorder(uint16* data, size_t borderH, size_t borderV);

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
			, label(TileGenerator::NoLabel)
			, paint(NoPaint)
			, isValid(false)
		{}
		NeighbourInfo(const Vec2i& xy, const size_t z)
			: pos(xy)
			, top(z)
			, index(~0ul)
			, label(TileGenerator::NoLabel)
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

	void   TraceContour(TileGenerator::TracerPath& path, const Tracer& start, size_t erosion, size_t climbableVoxelCount, const NeighbourInfoRequirements& contourReq);
	int    LabelTracerPath(const TileGenerator::TracerPath& path, size_t climbableVoxelCount, Region& region, Contour& contour, const uint16 internalLabel, const uint16 internalLabelFlags, const uint16 externalLabel);

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
			m_tracerPaths.push_back(path);
		}
#endif //_RELEASE
	}

	struct SurroundingSpanInfo
	{
		SurroundingSpanInfo(uint16 _label, size_t _index, size_t _flags = 0)
			: flags(_flags)
			, label(_label)
			, index(_index)
		{
		}
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
	                                   uint16 climbableVoxelCount, ContourVertex& contourVertex) const;
	inline bool ContourVertexRemovable(const ContourVertex& contourVertex) const
	{
		return ((contourVertex.flags & ContourVertex::TileBoundary) == 0)
		       && ((contourVertex.flags & ContourVertex::Unremovable) == 0);
	}

	void   AddContourVertex(const ContourVertex& vertex, Region& region, Contour& contour) const;

	size_t InsertUniqueVertex(VertexIndexLookUp& lookUp, size_t x, size_t y, size_t z);

	Params       m_params;
	ProfilerType m_profiler;
	size_t       m_top;

	typedef std::vector<Tile::Triangle> Triangles;
	Triangles m_triangles;

	typedef std::vector<Tile::Vertex> Vertices;
	Vertices m_vertices;

	typedef std::vector<Tile::BVNode> BVTree;
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

	CompactSpanGrid m_spanGridFlagged;
};
}

#endif  // #ifndef __MNM_TILE_GENERATOR_H

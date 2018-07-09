// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #TODO : Would it be possible to support different depth, width and height resolutions? Alternatively we could just create an equivalent CQuadtreePlotter class.

#pragma once
namespace Cry
{
	namespace SensorSystem
	{
		struct SOctreeParams
		{
			inline SOctreeParams()
				: depth(0)
				, bounds(ZERO)
			{}

			inline SOctreeParams(uint32 _depth, const AABB& _bounds)
				: depth(_depth)
				, bounds(_bounds)
			{}

			uint32 depth;
			AABB   bounds;
		};

		struct SOctreeCell
		{
			inline SOctreeCell()
				: depth(0)
				, idx(s_invalidIdx)
				, bounds(ZERO)
			{}

			inline SOctreeCell(uint32 _depth, uint32 _idx, const AABB& _bounds)
				: depth(_depth)
				, idx(_idx)
				, bounds(_bounds)
			{}

			uint32 depth;
			uint32 idx;
			AABB   bounds;

			static const uint32 s_invalidIdx = 0xffffffff;
		};

		class COctreePlotter
		{
		public:

			COctreePlotter(const SOctreeParams& params);

			void                                   SetParams(const SOctreeParams& params);
			const SOctreeParams&                   GetParams() const;

			uint32                                 GetCellCount() const;
			uint32                                 GetParentCell(uint32 cellIdx) const;

			bool                                   GetCell(SOctreeCell& cell, uint32 cellIdx) const;
			bool                                   GetContainingCell(SOctreeCell& cell, const Vec3& pos) const;
			bool                                   GetContainingCell(SOctreeCell& cell, const AABB& bounds) const;

			template<typename VISITOR> inline void VisitCells(VISITOR& visitor) const;
			template<typename VISITOR> inline void VisitContainingCells(VISITOR& visitor, const AABB& bounds) const;
			template<typename VISITOR> inline void VisitOverlappingCells(VISITOR& visitor, const AABB& bounds) const;

		private:

			uint32 Resolution(uint32 depth) const;
			uint32 FirstCellIdx(uint32 depth) const;
			uint32 CellCount(uint32 depth) const;

			void   Cell(SOctreeCell& cell, uint32 depth, uint32 idx) const;
			bool   Cell(SOctreeCell& cell, uint32 depth, const Vec3& pos) const;

			void ExpandCellIdx(uint32 idx, uint32 resolution, uint32(&xyz)[3]) const;
			Vec3 ExpandCellIdx(uint32 idx, uint32 resolution) const;
			Vec3 Hash(const Vec3& size, const Vec3& pos) const;

			AABB CellBounds(const Vec3& size, const Vec3& hash) const;

		private:

			SOctreeParams m_params;

			static const uint32 ms_minDepth = 1;
			static const uint32 ms_maxDepth = 10;
			static const uint32 ms_maxCellCount = 0x09249249; // N.B. In binary this value is 1001001001001001001001001001. We shift it down to calculate the index of the first cell at a given depth (see FirstCellIdx()).
		};
	}
}
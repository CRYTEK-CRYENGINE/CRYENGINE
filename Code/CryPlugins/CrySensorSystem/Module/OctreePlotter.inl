// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "OctreePlotter.h"

namespace Cry
{
	namespace SensorSystem
	{
		inline COctreePlotter::COctreePlotter(const SOctreeParams& params)
		{
			SetParams(params);
		}

		inline void COctreePlotter::SetParams(const SOctreeParams& params)
		{
			m_params.depth = clamp_tpl(params.depth, ms_minDepth, ms_maxDepth);
			m_params.bounds = params.bounds;
		}

		inline const SOctreeParams& COctreePlotter::GetParams() const
		{
			return m_params;
		}

		inline uint32 COctreePlotter::GetCellCount() const
		{
			return FirstCellIdx(m_params.depth + 1);
		}

		inline uint32 COctreePlotter::GetParentCell(uint32 cellIdx) const
		{
			if (cellIdx > 0)
			{
				for (uint32 depth = 2, maxDepth = m_params.depth; depth <= maxDepth; ++depth)
				{
					if (cellIdx < FirstCellIdx(depth + 1))
					{
						uint32 xyz[3];
						ExpandCellIdx(cellIdx - FirstCellIdx(depth), Resolution(depth), xyz);
						xyz[0] /= 2;
						xyz[1] /= 2;
						xyz[2] /= 2;

						const uint32 resolution = Resolution(depth - 1);
						return FirstCellIdx(depth - 1) + xyz[0] + (xyz[1] * resolution) + (xyz[2] * resolution * resolution);
					}
				}
			}
			return SOctreeCell::s_invalidIdx;
		}

		inline bool COctreePlotter::GetCell(SOctreeCell& cell, uint32 cellIdx) const
		{
			for (uint32 depth = 1, maxDepth = m_params.depth; depth <= maxDepth; ++depth)
			{
				if (cellIdx < FirstCellIdx(depth + 1))
				{
					Cell(cell, depth, cellIdx - FirstCellIdx(depth));
					return true;
				}
			}
			return false;
		}

		inline bool COctreePlotter::GetContainingCell(SOctreeCell& cell, const Vec3& pos) const
		{
			return Cell(cell, m_params.depth, pos);
		}

		inline bool COctreePlotter::GetContainingCell(SOctreeCell& cell, const AABB& bounds) const
		{
			if (m_params.bounds.ContainsBox(bounds))
			{
				Cell(cell, 1, 0);

				const Vec3 pos = bounds.GetCenter();
				for (uint32 depth = 2, maxDepth = m_params.depth; depth <= maxDepth; ++depth)
				{
					SOctreeCell childCell;
					Cell(childCell, depth, pos);
					if (!childCell.bounds.ContainsBox(bounds))
					{
						break;
					}

					cell = childCell;
				}
				return true;
			}
			return false;
		}

		template<typename VISITOR> inline void COctreePlotter::VisitCells(VISITOR& visitor) const
		{
			for (uint32 depth = 1, maxDepth = m_params.depth; depth <= maxDepth; ++depth)
			{
				for (uint32 cellIdx = 0, cellCount = CellCount(depth); cellIdx < cellCount; ++cellIdx)
				{
					SOctreeCell cell;
					Cell(cell, depth, cellIdx);
					visitor(cell);
				}
			}
		}

		template<typename VISITOR> inline void COctreePlotter::VisitContainingCells(VISITOR& visitor, const AABB& bounds) const
		{
			const Vec3 pos = bounds.GetCenter();
			for (uint32 depth = 1, maxDepth = m_params.depth; depth <= maxDepth; ++depth)
			{
				SOctreeCell cell;
				Cell(cell, depth, pos);
				if (cell.bounds.ContainsBox(bounds))
				{
					visitor(cell);
				}
				else
				{
					break;
				}
			}
		}

		template<typename VISITOR> inline void COctreePlotter::VisitOverlappingCells(VISITOR& visitor, const AABB& bounds) const
		{
			if (m_params.bounds.IsIntersectBox(bounds))
			{
				SOctreeCell cell;
				Cell(cell, 1, 0);
				visitor(cell);

				for (uint32 depth = 2, maxDepth = m_params.depth; depth <= maxDepth; ++depth)
				{
					const uint32 resolution = Resolution(depth);
					const Vec3 size = (m_params.bounds.max - m_params.bounds.min) / static_cast<float>(resolution);
					const AABB hash(Hash(size, bounds.min), Hash(size, bounds.max));

					const float minHash = 0.0f;
					const float maxHash = static_cast<float>(resolution - 1);

					const uint32 begin[3] =
					{
						static_cast<uint32>(clamp_tpl(hash.min.x, minHash, maxHash)),
						static_cast<uint32>(clamp_tpl(hash.min.y, minHash, maxHash)),
						static_cast<uint32>(clamp_tpl(hash.min.z, minHash, maxHash))
					};

					const uint32 end[3] =
					{
						static_cast<uint32>(clamp_tpl(hash.max.x, minHash, maxHash)),
						static_cast<uint32>(clamp_tpl(hash.max.y, minHash, maxHash)),
						static_cast<uint32>(clamp_tpl(hash.max.z, minHash, maxHash))
					};

					for (uint32 z = begin[2]; z <= end[2]; ++z)
					{
						for (uint32 y = begin[1]; y <= end[1]; ++y)
						{
							for (uint32 x = begin[0]; x <= end[0]; ++x)
							{
								SOctreeCell cell;
								Cell(cell, depth, x + (y * resolution) + (z * resolution * resolution));
								visitor(cell);
							}
						}
					}
				}
			}
		}

		inline uint32 COctreePlotter::Resolution(uint32 depth) const
		{
			return 1 << (depth - 1);
		}

		inline uint32 COctreePlotter::FirstCellIdx(uint32 depth) const
		{
			return ms_maxCellCount & ((1 << ((depth - 1) * 3)) - 1);
		}

		inline uint32 COctreePlotter::CellCount(uint32 depth) const
		{
			return 1 << ((depth - 1) * 3);
		}

		inline void COctreePlotter::Cell(SOctreeCell& cell, uint32 depth, uint32 idx) const
		{
			const uint32 resolution = Resolution(depth);
			const Vec3 size = (m_params.bounds.max - m_params.bounds.min) / static_cast<float>(resolution);
			const Vec3 hash = ExpandCellIdx(idx, resolution);

			cell.depth = depth;
			cell.idx = FirstCellIdx(depth) + idx;
			cell.bounds = CellBounds(size, hash);
		}

		inline bool COctreePlotter::Cell(SOctreeCell& cell, uint32 depth, const Vec3& pos) const
		{
			const float resolution = static_cast<float>(Resolution(depth));
			const Vec3 size = (m_params.bounds.max - m_params.bounds.min) / resolution;
			const Vec3 hash = Hash(size, pos);

			if ((hash.x < 0.0f) || (hash.x >= resolution) || (hash.y < 0.0f) || (hash.y >= resolution) || (hash.z < 0.0f) || (hash.z >= resolution))
			{
				return false;
			}

			cell.depth = depth;
			cell.idx = FirstCellIdx(depth) + static_cast<uint32>(hash.x + (hash.y * resolution) + (hash.z * resolution * resolution));
			cell.bounds = CellBounds(size, hash);
			return true;
		}

		inline void COctreePlotter::ExpandCellIdx(uint32 idx, uint32 resolution, uint32(&xyz)[3]) const
		{
			xyz[2] = idx / (resolution * resolution);

			idx -= xyz[2] * resolution * resolution;

			xyz[1] = idx / resolution;

			idx -= xyz[1] * resolution;

			xyz[0] = idx;
		}

		inline Vec3 COctreePlotter::ExpandCellIdx(uint32 idx, uint32 resolution) const
		{
			uint32 xyz[3];
			ExpandCellIdx(idx, resolution, xyz);
			return Vec3(static_cast<float>(xyz[0]), static_cast<float>(xyz[1]), static_cast<float>(xyz[2]));
		}

		inline Vec3 COctreePlotter::Hash(const Vec3& size, const Vec3& pos) const
		{
			Vec3 hash;
			hash.x = floor_tpl((pos.x - m_params.bounds.min.x) / size.x);
			hash.y = floor_tpl((pos.y - m_params.bounds.min.y) / size.y);
			hash.z = floor_tpl((pos.z - m_params.bounds.min.z) / size.z);
			return hash;
		}

		inline AABB COctreePlotter::CellBounds(const Vec3& size, const Vec3& hash) const
		{
			AABB bounds;
			bounds.min.x = m_params.bounds.min.x + (size.x * hash.x);
			bounds.min.y = m_params.bounds.min.y + (size.y * hash.y);
			bounds.min.z = m_params.bounds.min.z + (size.z * hash.z);
			bounds.max = bounds.min + size;
			return bounds;
		}
	}
}
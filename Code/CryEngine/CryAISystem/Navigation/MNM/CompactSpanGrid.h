// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MNM_COMPACT_SPAN_GRID_H
#define __MNM_COMPACT_SPAN_GRID_H

#pragma once

namespace MNM
{
class DynamicSpanGrid;
class CompactSpanGrid
{
public:
	struct Cell
	{
		enum
		{
			MaxSpanCount = 255,
		};

		inline Cell()
			: index(0)
			, count(0)
		{
		}

		inline Cell(uint32 _index, uint32 _count)
			: index(_index)
			, count(_count)
		{
		}

		inline operator bool() const
		{
			return count != 0;
		}

		uint32 index : 24;
		uint32 count : 8;
	};

	struct Span
	{
		inline Span()
			: backface(0)
			, flags(0)
			, bottom(0)
			, height(0)
			, depth(0)
		{
		}

		inline Span(uint16 _bottom, uint16 _height, uint16 _depth, bool _backface)
			: backface(_backface ? 1 : 0)
			, flags(0)
			, bottom(_bottom)
			, height(_height)
			, depth(_depth)
		{
			assert(height > 0);
		}

		uint32 backface : 1;
		uint32 flags    : 3;
		uint32 bottom   : 9;
		uint32 height   : 9;
		uint32 depth    : 10;   // at surface
	};

	inline CompactSpanGrid()
		: m_width(0)
		, m_height(0)
	{
	}

	inline size_t GetCellCount() const
	{
		return m_cells.size();
	}

	inline size_t GetSpanCount() const
	{
		return m_spans.size();
	}

	inline size_t GetWidth() const
	{
		return m_width;
	}

	inline size_t GetHeight() const
	{
		return m_height;
	}

	inline size_t GetMemoryUsage() const
	{
		return sizeof(*this) + m_cells.capacity() * sizeof(Cell) + m_spans.capacity() * sizeof(Span);
	}

	inline const Cell operator[](size_t i) const
	{
		if (i < m_cells.size())
			return m_cells[i];

		return Cell(0, 0);
	}

	inline const Cell GetCell(size_t x, size_t y) const
	{
		if ((x < m_width) && (y < m_height))
			return m_cells[y * m_width + x];

		return Cell(0, 0);
	}

	inline Span& GetSpan(size_t i)
	{
		return m_spans[i];
	}

	inline const Span& GetSpan(size_t i) const
	{
		return m_spans[i];
	}

	inline bool GetSpanAt(size_t x, size_t y, size_t top, size_t tolerance, size_t& outSpan) const
	{
		if ((x < m_width) && (y < m_height))
		{
			if (const Cell cell = m_cells[y * m_width + x])
			{
				size_t count = cell.count;
				size_t index = cell.index;

				for (size_t s = 0; s < count; ++s)
				{
					const Span& span = m_spans[index + s];
					const size_t otop = span.bottom + span.height;

					const int dtop = top - otop;

					if ((size_t)abs(dtop) <= tolerance)
					{
						outSpan = index + s;

						return true;
					}
				}
			}
		}

		return false;
	}

	inline bool GetSpanAt(size_t cellOffset, size_t top, size_t tolerance, size_t& outSpan) const
	{
		if (const Cell cell = m_cells[cellOffset])
		{
			size_t count = cell.count;
			size_t index = cell.index;

			for (size_t s = 0; s < count; ++s)
			{
				const Span& span = m_spans[index + s];
				const size_t otop = span.bottom + span.height;

				const int dtop = top - otop;

				if ((size_t)abs(dtop) <= tolerance)
				{
					outSpan = index + s;

					return true;
				}
			}
		}

		return false;
	}

	void Swap(CompactSpanGrid& other);
	void BuildFrom(const DynamicSpanGrid& dynGrid);
	void CompactExcluding(const CompactSpanGrid& spanGrid, size_t flags, size_t newSpanCount);
	void Clear();

private:
	size_t m_width;
	size_t m_height;

	typedef std::vector<Cell> Cells;
	Cells m_cells;

	typedef std::vector<Span> Spans;
	Spans m_spans;
};
}

#endif  // #ifndef __MNM_COMPACT_SPAN_GRID_H

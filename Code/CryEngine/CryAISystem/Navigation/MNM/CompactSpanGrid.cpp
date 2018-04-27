// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CompactSpanGrid.h"
#include "DynamicSpanGrid.h"

namespace MNM
{
void CompactSpanGrid::Swap(CompactSpanGrid& other)
{
	std::swap(m_width, other.m_width);
	std::swap(m_height, other.m_height);

	m_cells.swap(other.m_cells);
	m_spans.swap(other.m_spans);
}

void CompactSpanGrid::Clear()
{
	m_width = 0;
	m_height = 0;
	m_cells.clear();
	m_spans.clear();
}

void CompactSpanGrid::BuildFrom(const DynamicSpanGrid& dynGrid)
{
	const size_t cellCount = dynGrid.GetWidth() * dynGrid.GetHeight();
	const size_t spanCount = dynGrid.GetCount();

	m_width = dynGrid.GetWidth();
	m_height = dynGrid.GetHeight();

	m_cells.resize(cellCount);
	m_spans.resize(spanCount);

	size_t spanIndex = 0;

	for (size_t i = 0; i < cellCount; ++i)
	{
		if (const DynamicSpanGrid::Element* span = dynGrid[i])
		{
			const size_t index = spanIndex;
			m_cells[i].index = index;
			m_spans[spanIndex++] = Span(span->bottom, span->top - span->bottom, span->depth, span->backface);

			for (span = span->next; span; span = span->next)
				m_spans[spanIndex++] = Span(span->bottom, span->top - span->bottom, span->depth, span->backface);

			const size_t count = spanIndex - index;

			assert(count <= Cell::MaxSpanCount);

			m_cells[i].count = count & 0xff;
		}
	}
}

void CompactSpanGrid::CompactExcluding(const CompactSpanGrid& spanGrid, size_t flags, size_t newSpanCount)
{
	const size_t cellCount = spanGrid.GetWidth() * spanGrid.GetHeight();

	m_width = spanGrid.GetWidth();
	m_height = spanGrid.GetHeight();

	m_cells.resize(cellCount);
	m_spans.resize(newSpanCount);

	size_t spanIndex = 0;

	for (size_t i = 0; i < cellCount; ++i)
	{
		if (const Cell cell = spanGrid[i])
		{
			size_t ncount = 0;
			const size_t nindex = spanIndex;

			const size_t count = cell.count;
			const size_t index = cell.index;

			for (size_t s = 0; s < count; ++s)
			{
				const Span& span = spanGrid.GetSpan(index + s);

				if (span.flags & flags)
					continue;

				++ncount;
				m_spans[spanIndex++] = span;
			}

			m_cells[i] = Cell(ncount ? nindex : 0, ncount);
		}
	}
}
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DynamicSpanGrid.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

namespace MNM
{
DynamicSpanGrid::DynamicSpanGrid()
	: m_width(0)
	, m_height(0)
	, m_count(0)
{
}

DynamicSpanGrid::DynamicSpanGrid(size_t width, size_t height)
	: m_width(width)
	, m_height(height)
	, m_count(0)
{
	m_grid.resize(width * height, 0);
}

DynamicSpanGrid::DynamicSpanGrid(const DynamicSpanGrid& other)
	: m_width(other.m_width)
	, m_height(other.m_height)
	, m_count(other.m_count)
{
	const size_t gridSize = m_width * m_height;

	m_grid.resize(gridSize, 0);

	for (size_t i = 0; i < gridSize; ++i)
	{
		if (other.m_grid[i])
		{
			const Element* ospan = other.m_grid[i];

			Element* span = Construct(*ospan);
			m_grid[i] = span;

			for (ospan = ospan->next; ospan; ospan = ospan->next)
			{
				Element* next = Construct(*ospan);

				span->next = next;
				span = next;
			}
		}
	}
}

void DynamicSpanGrid::Swap(DynamicSpanGrid& other)
{
	m_grid.swap(other.m_grid);
	m_alloc.swap(other.m_alloc);

	std::swap(m_width, other.m_width);
	std::swap(m_height, other.m_height);
	std::swap(m_count, other.m_count);
}

void DynamicSpanGrid::Reset(size_t width, size_t height)
{
	m_width = width;
	m_height = height;
	m_count = 0;

	Grid().swap(m_grid);
	m_grid.resize(width * height, 0);

	Allocator().swap(m_alloc);
}

DynamicSpanGrid::Element* DynamicSpanGrid::operator[](size_t i)
{
	return m_grid[i];
}

const DynamicSpanGrid::Element* DynamicSpanGrid::operator[](size_t i) const
{
	return m_grid[i];
}

DynamicSpanGrid::Element* DynamicSpanGrid::GetSpan(size_t x, size_t y)
{
	return m_grid[y * m_width + x];
}

const DynamicSpanGrid::Element* DynamicSpanGrid::GetSpan(size_t x, size_t y) const
{
	return m_grid[y * m_width + x];
}

size_t DynamicSpanGrid::GetWidth() const
{
	return m_width;
}

size_t DynamicSpanGrid::GetHeight() const
{
	return m_height;
}

size_t DynamicSpanGrid::GetCount() const
{
	return m_count;
}

size_t DynamicSpanGrid::GetMemoryUsage() const
{
	return sizeof(*this) + (m_grid.capacity() * sizeof(Grid::value_type)) + m_alloc.get_total_memory();
}

void DynamicSpanGrid::AddVoxel(size_t x, size_t y, size_t z, bool backface)
{
	const size_t offset = y * m_width + x;
	Element* curr = m_grid[offset];
	Element* last = 0;

	const uint16 z0 = static_cast<uint16>(z);
	const uint16 z1 = static_cast<uint16>(z + 1);

	if (!curr)
	{
		++m_count;
		m_grid[offset] = Construct(Element(z0, z1, backface));

		return;
	}
	while (curr)
	{
		const uint16 bottom = curr->bottom;
		const uint16 top = curr->top;

		/*			if (z0 < top)
		      {
		        if (z0 < bottom) // add new or append at bottom
		        {
		          if (z1 == bottom) // append at bottom
		          {
		          }
		          else // add new
		          {
		          }
		        }
		        else if (z0 == bottom) // append at bottom
		        {
		          if (curr->backface != backface) // split
		          {
		          }
		          else // do nothing - merged
		          {
		          }
		        }
		      }
		      else
		      {
		      }
		 */
		/*
		 */

		if ((z0 >= bottom) && (z1 <= top))
		{
			if ((backface == curr->backface) || backface)
				return;

			if (z0 == bottom)
			{
				if (z1 == top)
				{
					curr->backface = 0;

					TryMergeNext(curr, z1, backface);
					TryMergePrev(curr, last, z0, backface);
				}
				else
				{
					curr->bottom = z1;

					++m_count;
					Element* newSpan = Construct(Element(z0, z1, backface));
					newSpan->next = curr;

					if (last)
					{
						last->next = newSpan;

						TryMergePrev(newSpan, last, z0, backface);
					}
					else
						m_grid[offset] = newSpan;
				}

				return;
			}
			else if (z1 == top)   // insert at top
			{
				curr->top = z0;

				++m_count;
				Element* newSpan = Construct(Element(z0, z1, backface));
				newSpan->next = curr->next;
				curr->next = newSpan;

				TryMergeNext(newSpan, z1, backface);
			}
			else   // insert in the middle
			{
				curr->top = z0;

				++m_count;
				Element* newSpanTop = Construct(Element(z1, top, curr->backface));
				newSpanTop->next = curr->next;

				++m_count;
				Element* newSpanMiddle = Construct(Element(z0, z1, backface));
				newSpanMiddle->next = newSpanTop;
				curr->next = newSpanMiddle;
			}

			return;
		}
		else if (z0 == top)
		{
			if (!curr->next || (curr->next->bottom > z0))
			{
				if (backface == curr->backface)
					curr->top = z1;
				else
				{
					++m_count;
					Element* newSpan = Construct(Element(z0, z1, backface));
					newSpan->next = curr->next;
					curr->next = newSpan;
				}

				TryMergeNext(curr, z1, backface);

				return;
			}
		}
		else if (z1 == bottom)
		{
			if (backface == curr->backface)
				curr->bottom = z0;
			else
			{
				++m_count;
				Element* newSpan = Construct(Element(z0, z1, backface));
				newSpan->next = curr;

				if (last)
					last->next = newSpan;
				else
					m_grid[offset] = newSpan;
			}

			TryMergePrev(curr, last, z0, backface);

			return;
		}
		else if (z1 < bottom)
		{
			++m_count;
			Element* newSpan = Construct(Element(z0, z1, backface));

			newSpan->next = curr;

			if (last)
				last->next = newSpan;
			else
				m_grid[offset] = newSpan;

			return;
		}

		last = curr;
		curr = curr->next;
	}

	if (!curr)
	{
		++m_count;
		Element* newSpan = Construct(Element(z0, z1, backface));

		last->next = newSpan;
	}
}
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MNM_DYNAMIC_SPAN_GRID_H
#define __MNM_DYNAMIC_SPAN_GRID_H

#pragma once

#include <CryMemory/FixedAllocator.h>

namespace MNM
{
class DynamicSpanGrid
{
public:
	struct Element
	{
		inline Element()
			: bottom(0)
			, top(0)
			, next(0)
		{
		}

		inline Element(uint16 _bottom, uint16 _top, bool _backface)
			: backface(_backface ? 1 : 0)
			, bottom(_bottom)
			, top(_top)
			, depth(0)
			, next(0)
		{
		}

		enum { MaxWaterDepth = (1 << 10) - 1, };

		uint32   backface : 1;
		uint32   flags    : 3; // unused at this point
		uint32   bottom   : 9;
		uint32   top      : 9;
		uint32   depth    : 10;
		Element* next;
	};

	DynamicSpanGrid();
	DynamicSpanGrid(size_t width, size_t height);
	DynamicSpanGrid(const DynamicSpanGrid& other);

	void           Swap(DynamicSpanGrid& other);

	void           Reset(size_t width, size_t height);
	Element*       operator[](size_t i);
	const Element* operator[](size_t i) const;

	Element*       GetSpan(size_t x, size_t y);
	const Element* GetSpan(size_t x, size_t y) const;
	size_t         GetWidth() const;
	size_t         GetHeight() const;
	size_t         GetCount() const;
	size_t         GetMemoryUsage() const;

	void           AddVoxel(size_t x, size_t y, size_t z, bool backface = false);
private:
	inline void    TryMergeNext(Element* span, uint16 top, bool backface)
	{
		if (span->next && (span->next->bottom == top) && (span->next->backface == backface))
		{
			span->top = span->next->top;

			Element* next = span->next;
			span->next = span->next->next;

			--m_count;
			Destruct(next);
		}
	}

	inline void TryMergePrev(Element* span, Element* prev, uint16 bottom, bool backface)
	{
		if (prev && (prev->top == bottom) && (prev->backface == backface))
		{
			prev->next = span->next;
			prev->top = span->top;

			--m_count;
			Destruct(span);
		}
	}

	inline Element* Construct(const Element& span)
	{
		return new(m_alloc.alloc())Element(span);
	}

	inline void Destruct(Element* span)
	{
		if (span)
		{
			span->~Element();
			m_alloc.dealloc(span);
		}
	}

	size_t m_width;
	size_t m_height;
	size_t m_count;

	typedef std::vector<Element*> Grid;
	Grid m_grid;

	typedef TypeFixedAllocator<Element, 256* 1024> Allocator;
	Allocator m_alloc;
};
}

#endif  // #ifndef __MNM_DYNAMIC_SPAN_GRID_H

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Scott Peter
// Containers that use their own heap for allocation.
//---------------------------------------------------------------------------

#pragma once

#ifndef HEAP_CONTAINER_H
	#define HEAP_CONTAINER_H

	#include <CryMemory/PoolAllocator.h>

//---------------------------------------------------------------------------
template<class T, typename L = stl::PSyncMultiThread>
struct HeapQueue : public L
{
	typedef typename L::Lock Lock;

	HeapQueue()
	{
		reset();
	}

	T* push_back()
	{
		Lock lock(*this);
		return push_back(m_Allocator.New());
	}

	template<class I>
	T* push_back(I const& init)
	{
		Lock lock(*this);
		Node* pNode = (Node*)m_Allocator.Allocate();
		new(static_cast<T*>(pNode))T(init);
		return push_back(pNode);
	}

	template<class I, class J>
	T* push_back(I const& i, J const& j)
	{
		Lock lock(*this);
		Node* pNode = (Node*)m_Allocator.Allocate();
		new(static_cast<T*>(pNode))T(i, j);
		return push_back(pNode);
	}

	T* pop_front()
	{
		Lock lock(*this);

		// Quick check, before locking.
		if (empty())
			return 0;

		Node* pNode = *m_ppHead;
		if (pNode)
		{
			m_ppHead = &(*m_ppHead)->pNext;
			m_nQueued--;
			validate();
		}
		return pNode;
	}

	void clear()
	{
		Lock lock(*this);

		validate();

		// Destruct all elements.
		size_t nCheckAlloc = 0;
		while (m_pList)
		{
			nCheckAlloc++;
			Node* pNext = m_pList->pNext;
			m_pList->~Node();
			m_pList = pNext;
		}
		assert(nCheckAlloc == m_nAlloc);

		// Empty queue structure.
		reset();

		// Free pool memory all at once.
		m_Allocator.FreeMemory(false);
	}

	size_t size() const
	{
		return m_nQueued;
	}

	bool empty() const
	{
		return m_nQueued == 0;
	}

	size_t allocated_memory() const
	{
		// Amortise allocated mem over all list instances.
		Lock lock(*this);
		return m_Allocator.GetTotalMemory().nAlloc;
	}

	//! Additional lock against storage deletion.
	L ClearLock;

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_Allocator);
	}
protected:

	struct Node : T
	{
		Node* pNext;
	};

	Node*  m_pList;                       //!< First (allocated) node in list.
	Node** m_ppHead;                      //!< Points to pointer to front of queue, for popping.
	Node** m_ppTail;                      //!< Points to pointer at end of list, and of queue, for pushing.
	size_t m_nAlloc, m_nQueued;

	void validate()
	{
		assert(m_nQueued <= m_nAlloc);
		assert(m_ppHead);
		assert(m_ppTail);
		assert(!*m_ppTail);
		assert((m_nQueued == 0) == !*m_ppHead);
		assert((m_nQueued == 0) == (m_ppHead == m_ppTail));
		assert((m_nAlloc == 0) == (m_ppTail == &m_pList));
		assert((m_nAlloc == 0) == !m_pList);
	}

	void reset()
	{
		m_pList = 0;
		m_ppHead = m_ppTail = &m_pList;
		m_nAlloc = m_nQueued = 0;
		validate();
	}

	Node* push_back(Node* pNode)
	{
		pNode->pNext = 0;

		*m_ppTail = pNode;
		m_ppTail = &pNode->pNext;
		m_nAlloc++;
		m_nQueued++;

		validate();

		return pNode;
	}

	//! Allocate all elements from an exclusive pool.
	//! Any locking is performed by the queue, no further locking needed in allocator.
	stl::TPoolAllocator<Node, stl::PSyncNone> m_Allocator;
};

//---------------------------------------------------------------------------
template<class T, class C = std::less<T>, typename L = stl::PSyncNone>
struct HeapPriorityQueue : public HeapQueue<T, L>
{
	// Hand-holding for brain-dead template compiler.
	typedef HeapQueue<T, L>      super;
	typedef typename super::Node Node;
	using super::empty;
	using super::validate;
	using super::m_ppHead;
	using super::m_ppTail;
	using super::m_nQueued;

public:

	typedef typename super::Lock Lock;

	//! Pop the "largest" element, using class C.
	T* pop_largest()
	{
		Lock lock(*this);

		if (!empty())
		{
			C comp;

			// Find highest-valued item.
			// To do: improve linear search! Use priority queue.
			Node** ppTop = m_ppHead;
			for (Node** ppNode = &(*m_ppHead)->pNext; *ppNode; ppNode = &(*ppNode)->pNext)
			{
				if (comp(**ppTop, **ppNode))
					ppTop = ppNode;
			}
			Node* pTop = *ppTop;

			// Move link to head.
			if (ppTop != m_ppHead)
			{
				if (!pTop->pNext)
				{
					// End of list.
					m_ppTail = ppTop;
				}
				*ppTop = pTop->pNext;
				pTop->pNext = *m_ppHead;
				*m_ppHead = pTop;
			}

			// Pop head.
			m_ppHead = &pTop->pNext;
			m_nQueued--;

			validate();
			return pTop;
		}
		else
		{
			return NULL;
		}
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		HeapQueue<T, L>::GetMemoryUsage(pSizer);
	}
};

#endif

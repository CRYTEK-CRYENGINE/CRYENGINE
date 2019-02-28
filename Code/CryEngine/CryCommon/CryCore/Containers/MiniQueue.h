// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  a small memory overhead, fixed size, efficient, iterable
               queue class (used for CContextView::SObjectClone)
   -------------------------------------------------------------------------
   History:
   - 02/09/2004   12:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __MINIQUEUE_H__
#define __MINIQUEUE_H__

#pragma once

//! This class implements a very small queue of plain-old-data.
template<typename T, uint8 N>
struct MiniQueue
{
public:
	MiniQueue() : m_nValues(0), m_nOffset(0) {}

	MiniQueue(const MiniQueue& mq)
	{
		m_nValues = mq.m_nValues;
		m_nOffset = 0;
		for (int i = 0; i < m_nValues; i++)
		{
			m_vValues[i] = mq[i];
		}
	}

	bool Empty() const
	{
		return m_nValues == 0;
	}
	bool Full() const
	{
		return m_nValues == N;
	}
	uint8 Size() const
	{
		return m_nValues;
	}
	uint8 Capacity() const
	{
		return N;
	}
	ILINE void Push(T nValue)
	{
		assert(!Full());
		m_vValues[(m_nOffset + m_nValues) % N] = nValue;
		m_nValues++;
	}
	//! Push, but if the queue is full, then pop first.
	void CyclePush(T nValue)
	{
		if (Full())
			Pop();
		Push(nValue);
	}
	void PushFront(T nValue)
	{
		assert(!Full());
		m_nOffset = (m_nOffset + (N - 1)) % N;
		m_vValues[m_nOffset] = nValue;
		m_nValues++;
	}
	T Front() const
	{
		assert(!Empty());
		return m_vValues[m_nOffset];
	}
	T& Front()
	{
		assert(!Empty());
		return m_vValues[m_nOffset];
	}
	T& operator[](int i)
	{
		return m_vValues[(m_nOffset + i) % N];
	}
	T operator[](int i) const
	{
		return m_vValues[(m_nOffset + i) % N];
	}
	T Back() const
	{
		assert(!Empty());
		return m_vValues[(m_nOffset + m_nValues - 1) % N];
	}
	T& Back()
	{
		assert(!Empty());
		return m_vValues[(m_nOffset + m_nValues - 1) % N];
	}
	void Pop()
	{
		assert(!Empty());
		m_nOffset = (m_nOffset + 1) % N;
		m_nValues--;
	}
	void PopBack()
	{
		assert(!Empty());
		m_nValues--;
	}
	void Clear()
	{
		m_nOffset = m_nValues = 0;
	}

	struct SIterator
	{
	public:
		SIterator() {}
		SIterator(T* pValues, uint8 nOffset) : m_pValues(pValues), m_nOffset(nOffset) {}
		T& operator*()
		{
			return m_pValues[m_nOffset % N];
		}
		T* operator->()
		{
			return &m_pValues[m_nOffset % N];
		}
		SIterator& operator++()
		{
			m_nOffset++;
			return *this;
		}
		SIterator operator++(int)
		{
			SIterator itor = *this;
			++m_nOffset;
			return itor;
		}
		SIterator& operator--()
		{
			m_nOffset--;
			return *this;
		}
		SIterator& operator+=(uint8 delta)
		{
			m_nOffset += delta;
			return *this;
		}
		SIterator& operator-=(uint8 delta)
		{
			m_nOffset -= delta;
			return *this;
		}

		friend bool operator!=(const SIterator& a, const SIterator& b)
		{
			assert(a.m_pValues == b.m_pValues);
			return a.m_nOffset != b.m_nOffset;
		}
		friend bool operator==(const SIterator& a, const SIterator& b)
		{
			assert(a.m_pValues == b.m_pValues);
			return a.m_nOffset == b.m_nOffset;
		}

		friend int operator-(const SIterator& a, const SIterator& b)
		{
			assert(a.m_pValues == b.m_pValues);
			int diff = int(a.m_nOffset) - int(b.m_nOffset);
			return diff;
		}

		uint8 Offset() { return m_nOffset; }

	private:
		T*    m_pValues;
		uint8 m_nOffset;
	};

	SIterator Begin()  { return SIterator(m_vValues, m_nOffset); }
	SIterator End()    { return SIterator(m_vValues, m_nOffset + m_nValues); }

	SIterator RBegin() { return SIterator(m_vValues, m_nOffset + m_nValues - 1); }
	SIterator REnd()   { return SIterator(m_vValues, m_nOffset - 1); }

	struct SConstIterator
	{
	public:
		SConstIterator() {}
		SConstIterator(const T* pValues, uint8 nOffset) : m_pValues(pValues), m_nOffset(nOffset) {}
		const T& operator*()
		{
			return m_pValues[m_nOffset % N];
		}
		const T* operator->()
		{
			return &m_pValues[m_nOffset % N];
		}
		SConstIterator& operator++()
		{
			m_nOffset++;
			return *this;
		}
		SConstIterator& operator--()
		{
			m_nOffset--;
			return *this;
		}
		SConstIterator& operator+=(uint8 delta)
		{
			m_nOffset += delta;
			return *this;
		}
		SConstIterator& operator-=(uint8 delta)
		{
			m_nOffset -= delta;
			return *this;
		}
		friend bool operator!=(const SConstIterator& a, const SConstIterator& b)
		{
			assert(a.m_pValues == b.m_pValues);
			return a.m_nOffset != b.m_nOffset;
		}
		friend bool operator==(const SConstIterator& a, const SConstIterator& b)
		{
			assert(a.m_pValues == b.m_pValues);
			return a.m_nOffset == b.m_nOffset;
		}

		friend int operator-(const SConstIterator& a, const SConstIterator& b)
		{
			assert(a.m_pValues == b.m_pValues);
			int diff = int(a.m_nOffset) - int(b.m_nOffset);
			return diff;
		}

		uint8 Offset() { return m_nOffset; }

	private:
		const T* m_pValues;
		uint8    m_nOffset;
	};

	SConstIterator Begin() const  { return SConstIterator(m_vValues, m_nOffset); }
	SConstIterator End() const    { return SConstIterator(m_vValues, m_nOffset + m_nValues); }

	SConstIterator RBegin() const { return SConstIterator(m_vValues, m_nOffset + m_nValues - 1); }
	SConstIterator REnd() const   { return SConstIterator(m_vValues, m_nOffset - 1); }

	void           Erase(SIterator where)
	{
		assert(where.Offset() >= m_nOffset);
		assert(where.Offset() < m_nOffset + m_nValues);
		for (size_t offset = where.Offset(); offset < (size_t)(m_nOffset + m_nValues - 1); offset++)
		{
			m_vValues[offset % N] = m_vValues[(offset + 1) % N];
		}
		m_nValues--;
	}

	void Erase(SIterator first, SIterator last)
	{
		int num = last - first;
		if (num == 0)
			return;
		assert(num > 0);
		assert(num <= Size());
		assert(first.Offset() >= m_nOffset);
		assert(first.Offset() < m_nOffset + m_nValues);
		assert(last.Offset() >= m_nOffset);
		assert(last.Offset() <= m_nOffset + m_nValues);
		for (int offset = (int)first.Offset(); offset < m_nOffset + m_nValues - 1; offset++)
		{
			m_vValues[offset % N] = m_vValues[(offset + num) % N];
		}
		m_nValues -= num;
	}

private:
	uint8 m_nValues;
	uint8 m_nOffset;
	T     m_vValues[N];
};

#endif

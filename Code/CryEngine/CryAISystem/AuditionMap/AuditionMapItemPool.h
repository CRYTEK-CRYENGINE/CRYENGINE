// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Perception
{
namespace AuditionHelpers
{

template<class TYPE, int SORT_AFTER = 32> class CItemPool
{
public:
	typedef typename std::vector<TYPE>::size_type size_type;

	size_type Allocate()
	{
		size_type index;
		if (!m_freeIndices.empty())
		{
			index = m_freeIndices.back();
			m_freeIndices.pop_back();
		}
		else
		{
			index = m_itemsArray.size();
			m_itemsArray.resize(index + 1);
		}
		m_allocatedIndices.push_back(index);

		CRY_ASSERT(index >= 0 && index < m_itemsArray.size());
		m_itemsArray[index] = TYPE();
		return index;
	}

	void Release(size_type index)
	{
		CRY_ASSERT(index >= 0 && index < m_itemsArray.size());

		if (!stl::find_and_erase(m_allocatedIndices, index))
		{
			CRY_ASSERT(false);
			return;
		}

		m_freeIndices.push_back(index);

		--m_releasesBeforeSortingCounter;
		if (m_releasesBeforeSortingCounter <= 0)
		{
			// Sort descendantly so that the lowest indices can be popped of the end
			// again. This will make us re-use lowest indices faster which means
			// that newly 'allocated' stimuli will remain clustered at the start of
			// the memory buffers (more or less).
			std::sort(m_freeIndices.begin(), m_freeIndices.end(), std::greater<size_type>());

			// Sort ascendantly so that we run through all the allocated items
			// we will access them in sequential order in memory.
			std::sort(m_allocatedIndices.begin(), m_allocatedIndices.end());

			m_releasesBeforeSortingCounter = SORT_AFTER;
		}
	}

	void Clear()
	{
		m_itemsArray.clear();
		m_freeIndices.clear();
		m_allocatedIndices.clear();
		m_releasesBeforeSortingCounter = SORT_AFTER;
	}

	size_type Size() const
	{
		return m_allocatedIndices.size();
	}
	size_type GetIndex(size_t idx)
	{
		return m_allocatedIndices[idx];
	}

	TYPE& operator[](size_type index)
	{
		CRY_ASSERT(index >= 0 && index < m_itemsArray.size());
		return m_itemsArray[index];
	}

	const TYPE& operator[](size_type index) const
	{
		CRY_ASSERT(index >= 0 && index < m_itemsArray.size());
		return m_itemsArray[index];
	}

private:
	std::vector<TYPE>      m_itemsArray;
	std::vector<size_type> m_freeIndices;
	std::vector<size_type> m_allocatedIndices;
	int                    m_releasesBeforeSortingCounter = SORT_AFTER;
};

template<class TYPE, int SORT_AFTER = 32> class CItemPoolRefCount : public CItemPool<TYPE, SORT_AFTER>
{
public:
	typedef CItemPool<TYPE, SORT_AFTER> base;
	typedef typename base::size_type    size_type;

	void AddRef(size_type index)
	{
		base::operator[](index).AddRef();
	}
	void DecRef(size_type index)
	{
		TYPE& item = base::operator[](index);
		CRY_ASSERT(item.GetRefCount() > 0);

		item.DecRef();
		if (item.GetRefCount() == 0)
		{
			Release(index);
		}
	}
	void Release(size_type index)
	{
		CRY_ASSERT(base::operator[](index).GetRefCount() == 0);
		base::Release(index);
	}
};

class ItemCounted
{
public:
	ItemCounted()
		: m_refCount(0)
	{}

	size_t GetRefCount() { return m_refCount; }
	void   AddRef()      { ++m_refCount; }
	void   DecRef()      { --m_refCount; }

private:
	size_t m_refCount;
};

}   // AuditionHelpers

} // Perception

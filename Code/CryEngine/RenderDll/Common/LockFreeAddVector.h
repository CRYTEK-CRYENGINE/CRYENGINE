// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// Only thread safe in certain practical conditions, where number of simultaneous threads is not too big, in number of 10,20 or so.
// Implementation is not efficient on the initial grow, and will often cause critical section locks between threads.
// It is assumed same pre-grown vector will be used again later, in this case growing will not be required and it will behave mostly lockfree
// Should only be used for primitive types, without constructors and destructors.
template<class T>
struct lockfree_add_vector
{
	enum { safety_preallocated_items = 128 }; // if array size grows to the capacity - safety_preallocated_items array is locked and resized

	lockfree_add_vector()
		: m_lastIndex(0)
		, m_new_capacity(0)
		, m_capacity(0)
		, m_pCurrentBuffer(0)
		, m_safe_capacity(0)
	{
		//static_assert(std::is_trivially_copyable<T>::value, "Item type must be a Trivial Copyabale type.");
		reserve(safety_preallocated_items * 2);
	}

	void push_back(const T& value) { lockfree_push_back(value);  }

	size_t lockfree_push_back(const T& value)
	{
		//CryAutoLock<CryCriticalSectionNonRecursive> lock_temp(m_temp_lock);

		int newIndex = CryInterlockedIncrement(&m_lastIndex);
		int lastCapacity = m_capacity;

		// When new index gets close to the end of the capacity meaning vector will soon need to be reallocated, we start really locking access
		if (newIndex < m_safe_capacity)
		{
			// Fast case when we filling far away from end of the array capacity.
			m_pCurrentBuffer[newIndex - 1] = value;

			assert(m_capacity == lastCapacity); // Sanity check to be catch not thread safe access
		}
		else
		{
			// When new index gets close to the end of the capacity meaning vector will soon need to be reallocated, we start really locking access between threads
			CryAutoLock<CryCriticalSectionNonRecursive> lock(m_resize_lock);

			if (newIndex < m_capacity)
			{
				m_pCurrentBuffer[newIndex - 1] = value;
			}
			else
			{
				// By that time all threads should be only coming this path, and not a fast track.
				m_new_capacity = m_capacity + m_capacity / 2 + safety_preallocated_items;
				MemoryBarrier();

				m_freeList.push_back(m_pMemoryBuffer); // Keep memory allocated to make sure potential non thread safe write is not corrupting memory
				m_pMemoryBuffer = std::make_shared<SMemoryBuffer>(m_new_capacity, m_pMemoryBuffer->pBuffer, m_pMemoryBuffer->size);
				m_pCurrentBuffer = m_pMemoryBuffer->pBuffer;

				MemoryBarrier(); // Make sure that resize first happen before we change capacity guards
				m_capacity = m_new_capacity;
				m_safe_capacity = m_capacity - safety_preallocated_items;

				m_pCurrentBuffer[newIndex - 1] = value;
			}
		}
		return (size_t)(newIndex-1);
	}

	void reserve(size_t sz)
	{
		if (sz > m_capacity)
		{
			m_capacity = sz;
			m_new_capacity = m_capacity;
			m_safe_capacity = m_capacity - safety_preallocated_items;

			if (m_pMemoryBuffer)
				m_pMemoryBuffer = std::make_shared<SMemoryBuffer>(m_capacity, m_pMemoryBuffer->pBuffer, m_pMemoryBuffer->size);
			else
				m_pMemoryBuffer = std::make_shared<SMemoryBuffer>(m_capacity, nullptr, 0);
			m_pCurrentBuffer = m_pMemoryBuffer->pBuffer;
		}
		m_freeList.clear();
	}

	// Borrows name from CThreadSafeWorkerContainer
	// Must be called only after all threads finished pushing items to the the vector.
	void CoalesceMemory()
	{
		// If size of the array is within safety_preallocated_items from capacity, grow array even more, so that on next use we not run into the same gray zone.
		if (m_lastIndex >= m_safe_capacity)
		{
			reserve(m_capacity + m_capacity / 2 + safety_preallocated_items);
		}
		m_freeList.clear();
	}
	void Init()                          { CoalesceMemory(); };
	void SetNoneWorkerThreadID(uint64 t) {};

	//////////////////////////////////////////////////////////////////////////
	// std::vector like interface.
	//////////////////////////////////////////////////////////////////////////
	size_t   size() const                   { return m_lastIndex; }

	T*       begin()                        { return m_pCurrentBuffer; }
	const T* begin() const                  { return m_pCurrentBuffer; }
	T*       end()                          { return m_pCurrentBuffer + m_lastIndex; }
	const T* end() const                    { return m_pCurrentBuffer + m_lastIndex; }

	T&       operator[](size_t index)       { return m_pCurrentBuffer[index]; };
	const T& operator[](size_t index) const { return m_pCurrentBuffer[index]; };

	void     clear()                        { m_lastIndex = 0; }
	bool     empty() const                  { return m_lastIndex == 0; }

private:
	lockfree_add_vector(const lockfree_add_vector<T>& other) {}

	struct SMemoryBuffer
	{
		T*     pBuffer;
		size_t size;
		SMemoryBuffer(size_t sz, T* pPrevBuffer, size_t prevSize)
		{
			assert(sz > prevSize); // capacity can only grow
			const int align = 128;
			size_t alignedSize = (sz * sizeof(T) + (align - 1)) & ~(align - 1);
			pBuffer = (T*)CryModuleMemalign(alignedSize, align);
			size = sz;
			if (pPrevBuffer)
				cryMemcpy(pBuffer, pPrevBuffer, prevSize * sizeof(T));
			// Fill empty memory with 0
			memset(pBuffer + prevSize, 0, (sz - prevSize) * sizeof(T));
		}
		~SMemoryBuffer()
		{
			CryModuleMemalignFree(pBuffer);
		}
	};
	typedef std::shared_ptr<SMemoryBuffer> SMemoryBufferPtr;
private:
	volatile int                   m_capacity;
	volatile int                   m_safe_capacity;
	volatile int                   m_lastIndex;
	volatile int                   m_new_capacity;
	T*                             m_pCurrentBuffer;

	SMemoryBufferPtr               m_pMemoryBuffer;
	std::vector<SMemoryBufferPtr>  m_freeList;

	CryCriticalSectionNonRecursive m_resize_lock;

	CryCriticalSectionNonRecursive m_temp_lock;
};

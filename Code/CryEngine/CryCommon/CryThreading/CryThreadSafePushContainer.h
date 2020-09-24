// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// Description:
// This specialized container provides a dynamically grow-able multi-producer container.
// This is NOT a multi-producer <-> single-consumer. Consumer access can only happen when NO producer is accessing the container.
// It is optimized for high-frequency pushing from multiple-producers
// It does have some important constrains though.
// Single Consumer Access : The user has to make sure that no producer is modifying the queue.
// Multi  Producer Access : The user has to make sure that the consumer is currently not reading from the container.
//
// Requirement:
// - Concurrent push from multiple-producers
// - Iteration of container by single-consumer
// - Producers and consumer need not concurrently operate on the same vector 
// - Push returns index that remains valid until the container is cleared/reset
//
// Detail:
// The container provides a virtual chunk of memory which is divided into strides.
// On push a thread will grab a stride (contended) and fill it up on consecutive push operations (uncontested) until it runs out and needs to fetch a new stride (contended).
// If the mapped part of the memory chunk runs out of space, it is extended into the initially reserved region.
// Once all producers have finished pushing into the container the consumer can prepare access the container.
//
// Limitations:
// - Over its lifetime a container<T> type instance can only hosts s_nMaxThreads threads.
//   Usually this should not be a limitation. As the container<T> is usually used by < 10 threads.
//   It becomes a limitation if new threads are spawned and destroyed constantly and access a container<T> type. Then m_workers runs out of pre-allocated worker objects.
// - We can place elements into consecutive memory locations by reserving a fixed range of virtual memory at construction time. 
//   The maximumCapacity passed into the constructor defines this limit on the total size of the container.
//   Use a generous safety margin, as exceeding this limit impacts performance.
//   (The default is 128MB.)
// - To reduce management overhead, invalid elements are marked by writing a special 'tombstone value' to their memory location.
//   Elements with this value are then skipped when iterating over the container.
//   However, this also means that pushed elements containing the tombstone cannot be distinguished from invalid entries and are skipped, too.
//   An assert is triggered, when such an element is pushed into the container.

#include <CryThreading/CryAtomics.h>
#include <CryThreading/CryThread.h>
#include <CrySystem/ISystem.h>
#include <CryMemory/CryMemoryManager.h>
#include <CryMemory/VirtualMemory.h>
#include <CryMemory/CrySizer.h>

namespace CryMT
{
	template<class T>
	class CThreadSafePushContainer : private CVirtualMemory
	{
		static constexpr int32 s_nMaxThreads = 30;
		static constexpr int32 s_tombstone = 0xfbfbfbfb;
		static_assert(sizeof(T) >= sizeof(s_tombstone), "Size of T must be at least sizeof(int32) due to internal s_tombstone value");
		
	public:
		struct CRY_ALIGN(64) SWorker // avoid false sharing. One Worker object per cache-line
		{
			threadID m_threadId;
			T* m_pData;
			uint32 m_relativeContainerIndexOffset;
			uint16 m_indexIntoStride;
			char m_padding[64 - sizeof(threadID) - sizeof(T*) - sizeof(uint32) - sizeof(uint16)];
		};

		class iterator
		{
		public:
			using iterator_category = std::forward_iterator_tag;
			using value_type = T;
			using difference_type = std::ptrdiff_t;
			using pointer = T*;
			using reference = T&;

			//////////////////////////////////////////////////////////////////////////
			iterator(T* ptr, const CThreadSafePushContainer<T>& baseContainer)
				: m_ptr(ptr)
				, m_pBegin(baseContainer.beginPtr())
				, m_pEnd(baseContainer.endPtr())
				, m_pContainer(baseContainer)
			{}

			//////////////////////////////////////////////////////////////////////////
			iterator(const iterator& x) = default;

			//////////////////////////////////////////////////////////////////////////
			bool operator==(const iterator& x) const
			{
				return m_ptr == x.m_ptr;
			}

			//////////////////////////////////////////////////////////////////////////
			bool operator!=(const iterator& x) const
			{
				return m_ptr != x.m_ptr;
			}

			//////////////////////////////////////////////////////////////////////////
			T& operator*() const
			{
				return *m_ptr;
			}

			////////////////////////////////////////////////////////////////////////////
			T* operator->() const
			{
				return m_ptr;
			}

			//////////////////////////////////////////////////////////////////////////
			iterator& operator++()
			{
				do
				{
					++m_ptr;
					// must check against end first, so we don't access out of bounds memory!
				} while (m_ptr < m_pEnd && *reinterpret_cast<int32*>(m_ptr) == s_tombstone);

				// we've reached the end, so might have to continue with the next container
				if (m_ptr >= m_pEnd && m_pContainer.m_pFallbackContainer != nullptr)
				{
					new (this) iterator(m_pContainer.m_pFallbackContainer->beginPtr(), *m_pContainer.m_pFallbackContainer);
				}

				return *this;
			}

			//////////////////////////////////////////////////////////////////////////
			iterator operator++(int)
			{
				iterator tmp = *this;
				++*this;
				return tmp;
			}

			//////////////////////////////////////////////////////////////////////////
			iterator& operator--()
			{
				do
				{
					--m_ptr;
					// must check against begin first, so we don't access out of bounds memory!
				} while (m_pBegin <= m_ptr && *reinterpret_cast<int32*>(m_ptr) == s_tombstone);

				if (m_ptr < m_pBegin && m_pContainer.m_pParentContainer != nullptr)
				{
					new (this) iterator(m_pContainer.m_pParentContainer->endPtr() - 1, *m_pContainer.m_pParentContainer);
					if (*reinterpret_cast<int32*>(m_ptr) == s_tombstone)
					{
						--(*this);
					}
				}

				return *this;
			}

			//////////////////////////////////////////////////////////////////////////
			iterator operator--(int)
			{
				iterator tmp = *this;
				--*this;
				return tmp;
			}

		private:
			T* m_ptr;
			T* m_pBegin;
			T* m_pEnd;
			const CThreadSafePushContainer<T>& m_pContainer;
		};

	public:
		CThreadSafePushContainer(uint32 elementsPerStride = 64, size_t maximumCapacity = 128 << 20 /*MB*/);
		~CThreadSafePushContainer();

		T&       operator[](uint32 n);
		const T& operator[](uint32 n) const;

		void push_back(const T& rObj);
		void push_back(const T& rObj, uint32& index);

		void push_back(T&& rObj);
		void push_back(T&& rObj, uint32& index);

		template<typename... Args>
		T*   push_back_new(Args&&... args);

		template<typename... Args>
		T*   push_back_new(uint32& index, Args&&... args);

		// Clear container (size does not change)
		void clear();
		bool empty() const;

		// Clear container and unmap all used memory
		void reset_container();

		size_t capacity() const;

		iterator begin() const;
		iterator end() const;

		void GetMemoryUsage(ICrySizer*) const;

	private:
		CThreadSafePushContainer(CThreadSafePushContainer<T>* parent);

		// Disable copy/assignment
		CThreadSafePushContainer(const CThreadSafePushContainer &rOther) = delete;
		CThreadSafePushContainer& operator=(const CThreadSafePushContainer& rOther) = delete;

		T* beginPtr() const;
		T* endPtr() const;
		size_t ownCapacity() const;

		T* push_back_impl(uint32& index);
		// check that a pushed object is not a tombstone
		void PostInsertTombstoneCheck(uint32 index) const;

		SWorker* GetWorker();
		void GetNextDataBlock(SWorker* pWorker);

		bool IsTombEntry(uint32 index) const;

		template<typename... Args>
		void DefaultConstructElement(void* pElement, Args&&... args);
		void DestroyElements(T* pElements, uint32 numElements);
		
	private:
		//! pointer to the first reserved page
		T*    m_pAllocationBase;
		//! pointer to first memory position not yet used for allocations
		T*    m_pAllocationTail;
		//! pointer to the first unmapped page
		void* m_pMappedEnd;
		//! number of reserved bytes
		const size_t m_reservedMemorySize;

		const int m_numElementsPerStride;
		SWorker m_workers[s_nMaxThreads];

		// we don't want to crash, when the container runs full, so we use a follow-up instead
		CryCriticalSection           m_nextContainerLock;
		CThreadSafePushContainer<T>* m_pParentContainer;
		CThreadSafePushContainer<T>* m_pFallbackContainer;
	};

	///////////////////////////////////////////////////////////////////////////////
	template<typename T>
	inline CThreadSafePushContainer<T>::CThreadSafePushContainer(uint32 elementsPerStride, size_t maxCapacity)
		: CVirtualMemory(), m_reservedMemorySize(Align(maxCapacity, GetSystemPageSize()))
		, m_numElementsPerStride(elementsPerStride), m_pParentContainer(nullptr), m_pFallbackContainer(nullptr)
	{
		CRY_ASSERT(alignof(T) <= GetSystemPageSize());
		m_pAllocationBase = (T*)ReserveAddressRange(m_reservedMemorySize, "CThreadSafePushContainer");
		if (m_pAllocationBase == nullptr)
			CryFatalError("Unable to reserve enough virtual memory! (%" PRISIZE_T "MB requested)", maxCapacity >> 20);
		m_pAllocationTail = m_pAllocationBase;
		m_pMappedEnd = m_pAllocationBase;
		memset(m_workers, 0, sizeof(m_workers));
		reset_container();
	}

	template<typename T>
	inline CThreadSafePushContainer<T>::CThreadSafePushContainer(CThreadSafePushContainer<T>* parent)
		: CThreadSafePushContainer(parent->m_numElementsPerStride, parent->m_reservedMemorySize)
	{
		m_pParentContainer = parent;
	}

	template<typename T>
	inline CThreadSafePushContainer<T>::~CThreadSafePushContainer()
	{
		clear();
		UnreserveAddressRange(m_pAllocationBase, m_reservedMemorySize);
		SAFE_DELETE(m_pFallbackContainer);
	}

	///////////////////////////////////////////////////////////////////////////////
	template<typename T>
	inline T& CThreadSafePushContainer<T>::operator[](uint32 n)
	{
		CRY_ASSERT(!IsTombEntry(n), "Accessing element at index '%d', but it is marked as invalid.", n);

		if (ownCapacity() <= n && CRY_VERIFY(m_pFallbackContainer != nullptr, "CThreadSafeAddContainer out of bound access detected"))
		{
			return (*m_pFallbackContainer)[n - ownCapacity()];
		}
		return beginPtr()[n];
	}

	///////////////////////////////////////////////////////////////////////////////
	template<typename T>
	inline const T& CThreadSafePushContainer<T>::operator[](uint32 n) const
	{
		return const_cast<const T&>(const_cast<CThreadSafePushContainer<T>*>(this)->operator[](n));
	}

	///////////////////////////////////////////////////////////////////////////////
	template<typename T>
	inline void CThreadSafePushContainer<T>::push_back(const T& rObj)
	{
		uint32 index = ~0;
		T* pObj = push_back_impl(index);
		::new(pObj) T(rObj);
		PostInsertTombstoneCheck(index);
	}

	///////////////////////////////////////////////////////////////////////////////
	template<typename T>
	inline void CThreadSafePushContainer<T>::push_back(const T& rObj, uint32& index)
	{
		T* pObj = push_back_impl(index);
		::new(pObj) T(rObj);
		PostInsertTombstoneCheck(index);
	}

	///////////////////////////////////////////////////////////////////////////////
	template<typename T>
	inline void CThreadSafePushContainer<T>::push_back(T&& rObj)
	{
		uint32 index = ~0;
		T* pObj = push_back_impl(index);
		::new(pObj) T(std::move(rObj));
		PostInsertTombstoneCheck(index);
	}

	///////////////////////////////////////////////////////////////////////////////
	template<typename T>
	inline void CThreadSafePushContainer<T>::push_back(T&& rObj, uint32& index)
	{
		T* pObj = push_back_impl(index);
		::new(pObj) T(std::move(rObj));
		PostInsertTombstoneCheck(index);
	}

	///////////////////////////////////////////////////////////////////////////////
	template<typename T>
	template<typename... Args>
	inline T* CThreadSafePushContainer<T>::push_back_new(Args&&... args)
	{
		uint32 index = ~0;
		T* pObj = push_back_impl(index);
		DefaultConstructElement(pObj, std::forward<Args>(args)...);
		PostInsertTombstoneCheck(index);
		return pObj;
	}

	///////////////////////////////////////////////////////////////////////////////
	template<typename T>
	template<typename... Args>
	inline T* CThreadSafePushContainer<T>::push_back_new(uint32& index, Args&&... args)
	{
		T* pObj = push_back_impl(index);
		DefaultConstructElement(pObj, std::forward<Args>(args)...);
		PostInsertTombstoneCheck(index);
		return pObj;
	}

	///////////////////////////////////////////////////////////////////////////////
	template<typename T>
	inline T* CThreadSafePushContainer<T>::push_back_impl(uint32& index)
	{
		SWorker* pWorker = GetWorker();
		IF(pWorker->m_pData == nullptr, false)
		{
			GetNextDataBlock(pWorker);
		}

		// Get Data pointer
		T* pData = &pWorker->m_pData[pWorker->m_indexIntoStride];

		// Set index position
		index = pWorker->m_relativeContainerIndexOffset + pWorker->m_indexIntoStride;

		// Check for last free element used and reset if so
		IF(++pWorker->m_indexIntoStride == m_numElementsPerStride, false)
		{
			*pWorker = {};
		}
		return pData;
	}

	template<typename T>
	void CThreadSafePushContainer<T>::PostInsertTombstoneCheck(uint32 index) const
	{
		CRY_ASSERT(!IsTombEntry(index), "The inserted element contains the container's tombstone value and will therefore be skipped during iteration.");
	}

	template<typename T>
	inline bool CThreadSafePushContainer<T>::IsTombEntry(uint32 index) const
	{
		if (index < ownCapacity())
			return *reinterpret_cast<const int32*>(beginPtr() + index) == s_tombstone;
		else if (m_pFallbackContainer)
			return m_pFallbackContainer->IsTombEntry(index - ownCapacity());
		else
			return false;
	}

	//////////////////////////////////////////////////////////////////////////
	template<class T>
	void CThreadSafePushContainer<T>::clear()
	{
		// remove data and reset buffer
		DestroyElements(beginPtr(), ownCapacity());
		m_pAllocationTail = m_pAllocationBase;
		// could unmap part of the address space to account for shrinking buffers

		// the next container is only a fail-safe and should not be needed regularly
		SAFE_DELETE(m_pFallbackContainer);
		
		// Reset workers
		memset(m_workers, 0, sizeof(m_workers));
	}

	//////////////////////////////////////////////////////////////////////////
	template<class T>
	void CThreadSafePushContainer<T>::reset_container()
	{
		clear();
		UnmapPages(m_pAllocationBase, (char*)m_pMappedEnd - (char*)m_pAllocationBase);
		m_pMappedEnd = m_pAllocationBase;
	}

	//////////////////////////////////////////////////////////////////////////
	template<class T>
	bool CThreadSafePushContainer<T>::empty() const
	{
		return capacity() == 0;
	}

	//////////////////////////////////////////////////////////////////////////
	template<class T>
	size_t CThreadSafePushContainer<T>::capacity() const
	{
		return ownCapacity() + (m_pFallbackContainer != nullptr ? m_pFallbackContainer->capacity() : 0);
	}

	template<class T>
	size_t CThreadSafePushContainer<T>::ownCapacity() const
	{
		return m_pAllocationTail - m_pAllocationBase;
	}

	//////////////////////////////////////////////////////////////////////////
	template<class T>
	T* CThreadSafePushContainer<T>::beginPtr() const
	{
		return m_pAllocationBase;
	}

	template<class T>
	T* CThreadSafePushContainer<T>::endPtr() const
	{
		return m_pAllocationTail;
	}

	//////////////////////////////////////////////////////////////////////////
	template<class T>
	typename CThreadSafePushContainer<T>::iterator CThreadSafePushContainer<T>::begin() const
	{
		CRY_ASSERT(m_pParentContainer == nullptr, "Fallback containers should not be accessible from the outside");
		auto iter = iterator(beginPtr(), *this);
		//Find first valid element
		if (!empty() && IsTombEntry(0))
			++iter;
		return iter;
	}

	//////////////////////////////////////////////////////////////////////////
	template<class T>
	typename CThreadSafePushContainer<T>::iterator CThreadSafePushContainer<T>::end() const
	{
		// Ensure we replicate common iterator behavior
		// For empty container begin() == end()
		// For the for-loop iterator, iter == iterEnd
		if (m_pFallbackContainer)
			return m_pFallbackContainer->end();
		else
			return iterator(endPtr(), *this);
	}

	///////////////////////////////////////////////////////////////////////////////
	template<typename T>
	inline typename CThreadSafePushContainer<T>::SWorker* CThreadSafePushContainer<T>::GetWorker()
	{
		thread_local int tls_slotId = ~0;
		threadID threadId = CryGetCurrentThreadId();

		// Optimization: We assume that push_back will happen very often in a short time on the same container
		// Hence we cache the slotId.
		// Note: The TLS is a global value. All CThreadSafePushContainer<T> will share the same TLS. Hence there could be a value from another container cached.
		// Since we store a fixed size of workers we can never be out of bound. It can  be possible that the slotId references to the an id in another container. Hence we need to check for the matching thread id.
		if (tls_slotId != ~0)
		{
			SWorker& worker = m_workers[tls_slotId];

			// Check cached slot id
			if (worker.m_threadId == threadId)
			{
				return &worker;
			}
		}

		// Find worker for threadId
		for (SWorker& worker : m_workers)
		{
			if (worker.m_threadId == threadId)
			{
				tls_slotId = int(&worker - m_workers);
				return &worker;
			}
		}

		// Check if the cached TLS slotId is free in this container
		// If two container share the same slotId there is higher chance the slotId cache will hit
		if (tls_slotId != ~0)
		{
			SWorker& worker = m_workers[tls_slotId];
			{
				if (CryInterlockedCompareExchange64((int64*)&worker.m_threadId, threadId, 0) == 0)
				{
					return &worker;
				}
			}
		}

		// Assign worker slot to a thread
		for (SWorker& worker : m_workers)
		{
			if (worker.m_threadId == 0 && CryInterlockedCompareExchange64((int64*)&worker.m_threadId, threadId, 0) == 0)
			{
				tls_slotId = int(&worker - m_workers);
				return &worker;
			}
		}

		CryFatalError("CThreadSafeAddContainer: s_nMaxThreads limit reached.");
		return nullptr;
	}
	
	//////////////////////////////////////////////////////////////////////////
	template<class T>
	inline void CThreadSafePushContainer<T>::GetNextDataBlock(typename CThreadSafePushContainer<T>::SWorker* pWorker)
	{
		while (true)
		{
			T* const pOldTail = m_pAllocationTail;
			T* const pNewTail = pOldTail + m_numElementsPerStride;
			if (pNewTail <= m_pMappedEnd)
			{
				if (CryInterlockedCompareExchangePointer(reinterpret_cast<void**>(&m_pAllocationTail), pNewTail, pOldTail) == pOldTail)
				{
					pWorker->m_relativeContainerIndexOffset = uint32(pOldTail - m_pAllocationBase);
					pWorker->m_pData = pOldTail;
					pWorker->m_indexIntoStride = 0;
					memset(pWorker->m_pData, s_tombstone, sizeof(T) * m_numElementsPerStride);
					return;
				}
				continue;
			}
			else
			{
				char* const pCurrentMappedEnd = (char*)m_pMappedEnd;
				const size_t pageGrowth = Align(sizeof(T) * m_numElementsPerStride, GetSystemPageSize());

				if (pCurrentMappedEnd + pageGrowth > (char*)m_pAllocationBase + m_reservedMemorySize)
				{
					if (m_pFallbackContainer == nullptr)
					{
						AUTO_LOCK(m_nextContainerLock);
						if (m_pFallbackContainer == nullptr) // might be set while waiting for the lock
						{
							if (gEnv && !gEnv->IsEditor()) // don't spam in editor, but should be adapted for game
							{
								CryWarning(EValidatorModule::VALIDATOR_MODULE_SYSTEM, EValidatorSeverity::VALIDATOR_WARNING,
									"The ThreadSafePushContainer has surpassed its maximum capacity! This will impact performance!");
							}
							m_pFallbackContainer = new CThreadSafePushContainer<T>(this);
						}
					}
					CRY_ASSERT(m_pFallbackContainer != nullptr);

					m_pFallbackContainer->GetNextDataBlock(pWorker);
					pWorker->m_relativeContainerIndexOffset += ownCapacity();
					return;
				}

				MapPages(pCurrentMappedEnd, pageGrowth);
				// we either update successfully, or someone else has already mapped further
				CRY_VERIFY(CryInterlockedCompareExchangePointer(&m_pMappedEnd, pCurrentMappedEnd + pageGrowth, pCurrentMappedEnd) >= pCurrentMappedEnd);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	template<class T>
	inline void CThreadSafePushContainer<T>::GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObjectSize(this);
		pSizer->AddObject(m_pAllocationBase, (char*)m_pMappedEnd - (char*)m_pAllocationBase);
		if (m_pFallbackContainer != nullptr)
			pSizer->AddObject(m_pFallbackContainer);
	}

	//////////////////////////////////////////////////////////////////////////
	template<class T>
	void CThreadSafePushContainer<T>::DestroyElements(T* pElements, uint32 numElements)
	{
		if (!std::is_pod<T>::value)
		{
			for (uint32 i = 0; i < numElements; ++i)
			{
				if (*reinterpret_cast<int32*>(&pElements[i]) != s_tombstone)
				{
					pElements[i].~T();
				}
			}
		}
		if (numElements)
			memset(pElements, s_tombstone, numElements * sizeof(T));
	}

	//////////////////////////////////////////////////////////////////////////
	template<class T>
	template<typename... Args>
	void CThreadSafePushContainer<T>::DefaultConstructElement(void* pElement, Args&&... args)
	{
		if (!std::is_pod<T>::value)
		{
			::new(pElement) T(std::forward<Args>(args)...);
		}
		else
		{
			memset(pElement, 0, sizeof(T));
		}
	}
} // CryMT

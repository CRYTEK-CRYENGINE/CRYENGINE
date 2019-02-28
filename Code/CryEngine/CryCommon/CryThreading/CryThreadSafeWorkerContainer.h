// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   Description:
   Specialized Container for Renderer data with the following properties:
   - Created during the 3DEngine Update, consumed in the renderer in the following frame
   - This Container is very restricted and likely not optimal for other situations
   -------------------------------------------------------------------------
   History:
   - 13:06:2013: Created by Nico Moss
*************************************************************************/

#ifndef _CRY_THREAD_SAFE_WORKER_CONTAINER_H_
#define _CRY_THREAD_SAFE_WORKER_CONTAINER_H_

#include <CryCore/Platform/platform.h>
#include <vector>

//! BE CAREFULL WHEN USING THIS CONTAINER!
//!
//! --- Properties: ---.
//! - Stores data local to worker thread to avoid thread-safety semantics.
//! - Allows for a single non-worker thread to be tracked which is stored in m_workers[0].
//!     Hence: As m_workers[0] is shared between all none-worker threads, ensure that only one additional none-worker thread may access this container e.g. MainThread.
//! - Coalesce memory to obtain a continues memory block.
//! - Coalesce memory to for faster element access to a continues memory block.
//!
//! --- Restrictions:---.
//! - The workers own the memory structure.
//! - The coalesced memory stores a copy of the workers used memory.
//!     Hence: Be careful when altering data within the coalesced memory.
//!     If the templated element is a pointer type than altering the memory pointed to, is not be an issue.
//!     If the templated element is of type class or struct than ensure that data changes are done on the worker local data and not on the coalesced memory. Use worker encoded indices to do so.
template<class T>
class CThreadSafeWorkerContainer
{
public:
	struct SDefaultNoOpFunctor
	{
		ILINE void operator()(T* pData) const {}
	};

public:
	CThreadSafeWorkerContainer();
	~CThreadSafeWorkerContainer();

	void Init();
	void SetNoneWorkerThreadID(threadID nThreadId) { m_foreignWorkerId = nThreadId; }

	//! Safe access of elements for calling thread via operator[].
	uint32 ConvertToEncodedWorkerId_threadlocal(uint32 nIndex) const;

	uint32 GetNumWorkers() const;
	uint32 GetWorkerId_threadlocal() const;

	//! \note Be aware that these values can potentially change if some objects are added in parallel.
	size_t size() const;
	bool   empty() const;
	size_t capacity() const;

	size_t size_threadlocal() const;
	bool   empty_threadlocal() const;
	size_t capacity_threadlocal() const;

	//! \note Be aware that this operator is more expensive if the memory was not coalesced before.
	T&       operator[](size_t n);
	const T& operator[](size_t n) const;

	T*       push_back_new();
	T*       push_back_new(size_t& nIndex);

	void     push_back(const T& rObj);
	void     push_back(const T& rObj, size_t& nIndex);

	//! \note These functions are changing the size of the continuous memory block and thus are *not* thread-safe.
	void                                        clear();
	template<class OnElementDeleteFunctor> void clear(const OnElementDeleteFunctor& rFunctor = CThreadSafeWorkerContainer<T>::SDefaultNoOpFunctor());
	void                                        erase(const T& rObj);
	void                                        resize(size_t n);
	void                                        reserve(size_t n);

	//  *not* thread-safe functions
	void PrefillContainer(T* pElement, size_t numElements);
	void CoalesceMemory();

	void GetMemoryUsage(ICrySizer* pSizer) const;

private:

	class CRY_ALIGN(128) SWorker
	{
	public:
		SWorker() : m_dataSize(0) {}

		uint32 m_dataSize;
		std::vector<T> m_data;
	};

	T*   push_back_impl(size_t& nIndex);
	void ReserverCoalescedMemory(size_t n);

	threadID m_foreignWorkerId; //!< Id of the non-job-manager-worker thread that's also allowed to use this container.

	SWorker* m_workers;
	uint32   m_nNumWorkers;

	uint32   m_coalescedArrCapacity;
	T*       m_coalescedArr;
	bool     m_isCoalesced;
};

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline CThreadSafeWorkerContainer<T>::CThreadSafeWorkerContainer()
	: m_nNumWorkers(0)
	, m_workers(0)
	, m_coalescedArrCapacity(0)
	, m_coalescedArr(0)
	, m_isCoalesced(false)
{
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline CThreadSafeWorkerContainer<T>::~CThreadSafeWorkerContainer()
{
	clear();
	CryAlignedDeleteArray(m_workers, m_nNumWorkers);
	m_workers = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::Init()
{
	m_nNumWorkers = gEnv->GetJobManager()->GetNumWorkerThreads() + 1;
	m_workers = CryAlignedNewArray<CThreadSafeWorkerContainer<T>::SWorker>(m_nNumWorkers);

	m_foreignWorkerId = THREADID_NULL;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline size_t CThreadSafeWorkerContainer<T >::size() const
{
	uint32 totalSize = 0;
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		totalSize += m_workers[i].m_dataSize;
	}
	return totalSize;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline bool CThreadSafeWorkerContainer<T >::empty() const
{
	return size() == 0;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline size_t CThreadSafeWorkerContainer<T >::capacity() const
{
	uint32 totalCapacity = 0;
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		totalCapacity += m_workers[i].m_data.capacity();
	}
	return totalCapacity;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline size_t CThreadSafeWorkerContainer<T >::size_threadlocal() const
{
	const uint32 nWorkerThreadId = GetWorkerId_threadlocal();
	return m_workers[nWorkerThreadId].m_dataSize;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline bool CThreadSafeWorkerContainer<T >::empty_threadlocal() const
{
	const uint32 nWorkerThreadId = GetWorkerId_threadlocal();
	return m_workers[nWorkerThreadId].m_data.empty();
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline size_t CThreadSafeWorkerContainer<T >::capacity_threadlocal() const
{
	const uint32 nWorkerThreadId = GetWorkerId_threadlocal();
	return m_workers[nWorkerThreadId].m_data.capacity();
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline T& CThreadSafeWorkerContainer<T >::operator[](size_t n)
{
	const uint32 nHasWorkerEncodedIndex = (n & 0x80000000) >> 31;

	IF ((m_isCoalesced && !nHasWorkerEncodedIndex), 1)
	{
		return m_coalescedArr[n];
	}
	else
	{
		const uint32 nWorkerThreadId = (n & 0x7F00007F) >> 24; // Mask bit 24-30  (0 is starting bit)
		const uint32 nOffset = (n & ~0xFF000000);              // Mask out top 8 bits

		// Encoded offset into worker local array
		if (nHasWorkerEncodedIndex)
		{
			return m_workers[nWorkerThreadId].m_data[nOffset];
		}
		else // None-coalesced and none worker encoded offset
		{
			uint32 nTotalOffset = nOffset;
			for (int i = 0; i < m_nNumWorkers; ++i)
			{
				SWorker& worker = m_workers[i];

				if (nTotalOffset < worker.m_dataSize)
					return worker.m_data[nTotalOffset];
				else
					nTotalOffset -= worker.m_dataSize;
			}

			// Out of bound access detected!
			CRY_ASSERT_MESSAGE(false, "CThreadSafeWorkerContainer::operator[] - Out of bounds access");
			__debugbreak();
			return m_coalescedArr[0];
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline const T& CThreadSafeWorkerContainer<T >::operator[](size_t n) const
{
	return const_cast<const T&>(const_cast<CThreadSafeWorkerContainer<T>*>(this)->operator[](n));
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline T* CThreadSafeWorkerContainer<T >::push_back_new()
{
	size_t unused = ~0;
	return push_back_impl(unused);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline T* CThreadSafeWorkerContainer<T >::push_back_new(size_t& nIndex)
{
	return push_back_impl(nIndex);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::push_back(const T& rObj)
{
	size_t nUnused = ~0;
	T* pObj = push_back_impl(nUnused);
	*pObj = rObj;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::push_back(const T& rObj, size_t& nIndex)
{
	T* pObj = push_back_impl(nIndex);
	*pObj = rObj;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::clear()
{
	clear(SDefaultNoOpFunctor());
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
template<class OnElementDeleteFunctor>
inline void CThreadSafeWorkerContainer<T >::clear(const OnElementDeleteFunctor& rFunctor)
{
	// Reset worker data
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		// Delete elements
		uint32 nSize = m_workers[i].m_data.size();
		for (int j = 0; j < nSize; ++j)
		{
			// Call on element delete functor
			// Note: Default functor will do nothing with the element
			rFunctor(&m_workers[i].m_data[j]);
		}

		stl::free_container(m_workers[i].m_data);
		m_workers[i].m_dataSize = 0;
	}

	// Reset container data
	if (m_coalescedArr)
		CryModuleMemalignFree(m_coalescedArr);

	m_coalescedArr = 0;
	m_coalescedArrCapacity = 0;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::erase(const T& rObj)
{
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		typename std::vector<T>::iterator iter = m_workers[i].m_data.begin();
		typename std::vector<T>::iterator iterEnd = m_workers[i].m_data.end();

		for (; iter != iterEnd; ++iter)
		{
			if (rObj == *iter)
			{
				m_workers[i].m_data.erase(iter);
				--m_workers[i].m_dataSize;
				m_isCoalesced = false;
				return;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::resize(size_t n)
{
	CoalesceMemory();

	uint32 nSizePerWorker = n / m_nNumWorkers;
	uint32 nExcessSize = n % m_nNumWorkers;

	// Resize workers evenly
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		uint32 nWorkerSize = nSizePerWorker + nExcessSize;

		if (nWorkerSize > m_workers[i].m_data.size())
			m_workers[i].m_data.resize(nWorkerSize);

		m_workers[i].m_dataSize = nWorkerSize;
		nExcessSize = 0; // First worker creates excess items
	}

	ReserverCoalescedMemory(n);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::reserve(size_t n)
{
	CoalesceMemory();

	uint32 nSizePerWorker = n / m_nNumWorkers;
	uint32 nExcessSize = n % m_nNumWorkers;

	// Resize workers evenly
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		uint32 nWorkerSize = nSizePerWorker + nExcessSize;

		if (nWorkerSize > m_workers[i].m_data.size())
			m_workers[i].m_data.resize(nWorkerSize);

		nExcessSize = 0; // First worker creates excess items
	}

	ReserverCoalescedMemory(n);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::PrefillContainer(T* pElement, size_t numElements)
{
	reserve(numElements);

	uint32 nOffset = 0;
	uint32 nNumItemPerWorker = numElements / m_nNumWorkers;
	uint32 nNumExcessItems = numElements % m_nNumWorkers;

	// Store items evenly in workers
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		uint32 nNumItems = nNumItemPerWorker + nNumExcessItems;
		for (int j = 0; j < nNumItems; ++j)
		{
			m_workers[i].m_data[j] = pElement[nOffset + j];
		}

		m_workers[i].m_dataSize = nNumItems;
		nOffset += nNumItems;
		nNumExcessItems = 0; // First worker stores excess items
	}

	m_isCoalesced = false;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::CoalesceMemory()
{
	if (m_isCoalesced)
		return;

	static CryCriticalSectionNonRecursive s_accessLock;
	CryAutoLock<CryCriticalSectionNonRecursive> lock(s_accessLock);

	// Ensure enough memory exists
	uint32 minSizeNeeded = 0;
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		minSizeNeeded += m_workers[i].m_dataSize;
	}

	IF (minSizeNeeded >= m_coalescedArrCapacity, 0)
		ReserverCoalescedMemory(minSizeNeeded + (minSizeNeeded / 4));

	// Copy data to coalesced array
	uint32 nOffest = 0;
	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		SWorker& rWorker = m_workers[i];
		if (rWorker.m_dataSize == 0)
			continue;
		memcpy(m_coalescedArr + nOffest, &rWorker.m_data[0], sizeof(T) * rWorker.m_dataSize);
		nOffest += rWorker.m_dataSize;
	}

	m_isCoalesced = true;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
uint32 CThreadSafeWorkerContainer<T >::ConvertToEncodedWorkerId_threadlocal(uint32 nIndex) const
{
	const uint32 nThreadId = GetWorkerId_threadlocal();
	assert(nIndex < m_workers[nThreadId].m_dataSize);
	return (uint32)((1 << 31) | (nThreadId << 24) | nIndex);
}

//////////////////////////////////////////////////////////////////////////
template<typename T>
uint32 CThreadSafeWorkerContainer<T >::GetNumWorkers() const
{
	return m_nNumWorkers;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_coalescedArr, m_coalescedArrCapacity * sizeof(T));

	for (int i = 0; i < m_nNumWorkers; ++i)
	{
		pSizer->AddContainer(m_workers[i].m_data);
	}
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeWorkerContainer<T >::ReserverCoalescedMemory(size_t n)
{
	if (n <= m_coalescedArrCapacity)
		return;

	T* arrOldData = m_coalescedArr;
	m_coalescedArr = reinterpret_cast<T*>(CryModuleMemalign(n * sizeof(T), alignof(T)));
	memcpy(m_coalescedArr, arrOldData, m_coalescedArrCapacity * sizeof(T));
	if (arrOldData)
		CryModuleMemalignFree(arrOldData);
	m_coalescedArrCapacity = n;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline T* CThreadSafeWorkerContainer<T >::push_back_impl(size_t& nIndex)
{
	// Avoid writing to thread share resource and take hit of 'if statement to avoid false-sharing between threads
	IF (m_isCoalesced, 0)
		m_isCoalesced = false;

	// Get worker id
	const uint32 nWorkerThreadId = GetWorkerId_threadlocal();

	SWorker& activeWorker = m_workers[nWorkerThreadId];

	// Ensure enough space
	if (activeWorker.m_dataSize >= activeWorker.m_data.size())
		activeWorker.m_data.resize(activeWorker.m_data.size() + (activeWorker.m_data.size() / 2) + 1);

	// Encode worker local offset into index and return
	T* retItem = &activeWorker.m_data[activeWorker.m_dataSize];
	nIndex = (size_t)((1 << 31) | (nWorkerThreadId << 24) | activeWorker.m_dataSize);
	++activeWorker.m_dataSize;
	return retItem;
}

template<typename T>
uint32 CThreadSafeWorkerContainer<T >::GetWorkerId_threadlocal() const
{
	const uint32 nWorkerId = JobManager::GetWorkerThreadId();

	// Check if none-worker is valid
	if (nWorkerId == ~0)
	{
		if (m_foreignWorkerId != CryGetCurrentThreadId())
			CryFatalError("Trying to access CThreadSafeWorkerContainer from an unspecified none-worker thread. Current specified none-worker threadId with access rights: %" PRI_THREADID, m_foreignWorkerId);
	}

	// None-worker has id of ~0 ... add +1 to shift to 0. Worker0 will use slot 1 etc.
	return nWorkerId + 1;
}

#endif // _CRY_THREAD_SAFE_WORKER_CONTAINER_H_

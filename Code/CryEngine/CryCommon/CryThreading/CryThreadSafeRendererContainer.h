// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   Description:
   Specialized Container for Renderer data with the following proberties:
   - Created during the 3DEngine Update, comsumed in the renderer in the following frame
   - This Container is very restricted and likely not optimal for other situations
   -------------------------------------------------------------------------
   History:
   - 21:02:2012: Created by Christopher Bolte
*************************************************************************/
#ifndef _CRY_THREAD_SAFE_RENDERER_CONTAINER_H_
#define _CRY_THREAD_SAFE_RENDERER_CONTAINER_H_

//! This container is specialized for data which is generated in the 3DEngine and consumed by the renderer
//! in the following frame due to multithreaded rendering. To be useable by Jobs as well as other Threads
//! some very specific desing choices were taken:
//! First of the underlying continuous memory block is only resized during a call to 'CoalesceMemory'
//! to prevent freeing a memory block which could be used by another thread.
//! If new memory is requiered, a page of 4 KB is allocated and used as a temp storage till the next
//! call to 'CoalesceMemory' which then copies all page memory into one continuous block.
//! Also all threading relevant functions are implemented LockLess to prevent lock contention and make
//! this container useable from Jobs.
//!
//! Right now, the main usage pattern of this container is by the RenderThread, who calls at the beginning
//! of its frame 'CoalesceMemory', since then we can be sure that the 3DEngine has finished creating it's elements.
//!
//! Since the main purpose of this container is multi-threading adding of elements, a slight change was done to the
//! push_back interface compared to std::vector:
//!     All implemented push_back variants can return a pointer into the storage (safe since no memory is freed during adding)
//!     and a index for this elements. This is done since calling operator[] could be expensive when called before 'CoalesceMemory'.
//!
//! Only push_back(_new) are safe to be called from any filling thread.

template<typename T>
class CRY_ALIGN(128) CThreadSafeRendererContainer
{
public:
	CThreadSafeRendererContainer();
	~CThreadSafeRendererContainer();

	//NOTE: be aware that these valus can potentially change if some objects are added in parallel
	size_t size() const;
	size_t empty() const;
	size_t capacity() const;

	//NOTE: be aware that this operator can be more expensive if the memory was not coalesced  before
	T&       operator[](size_t n);
	const T& operator[](size_t n) const;

	void*    push_back_new();
	void*    push_back_new(size_t& nIndex);

	void push_back(const T &);
	void push_back(const T &, size_t & nIndex);
	void push_back(T &&);
	void push_back(T &&, size_t & nIndex);

	// NOTE: These functions are changing the size of the continuous memory block and thus are *not* thread-safe
	void clear();
	void resize(size_t n);
	void reserve(size_t n);

	void CoalesceMemory();

	void GetMemoryUsage(ICrySizer*) const;

	// disable copy/assignment
	CThreadSafeRendererContainer(const CThreadSafeRendererContainer &rOther);
	CThreadSafeRendererContainer& operator=(const CThreadSafeRendererContainer& rOther);

private:

	//! Struct to represent a memory chunk.
	//! Used in fallback allocations during 'Fill' phase.
	class CMemoryPage
	{
	public:
		//! Size of a page to allocate, the CMemoryPage is just the header.
		//! The actual object data is stored in the 4KB chunk right
		//! after the header (while keeping the requiered alignment and so on).
		enum { nMemoryPageSize = 4096 };

		CMemoryPage();

		// allocation functions
		static CMemoryPage* AllocateNewPage();
		bool                TryAllocateElement(size_t& nIndex, void*& pObj);

		// access to the elements
		T& GetElement(size_t n);
		T* GetData() const;

		// information about the page (NOTE: not thread-safe in all combinations)
		size_t Size() const;
		size_t Capacity() const;

		CMemoryPage* m_pNext;           //!< Pointer to next entry in single-linked list of CMemoryPages.

	private:
		LONG m_nSize;                 //!< Number of elements currently in the page.
		LONG m_nCapacity;             //!< Number of elements which could fit into the page.
		T*   m_arrData;               //!< Element memory, from the same memory chunk right after the CMemoryPage class.
	};

	/////////////////////////////////////
	//! Private functions which do the lock-less updating
	void* push_back_impl(size_t& nIndex);
	bool try_append_to_continuous_memory(size_t & nIndex, void* & pObj);

	T&          GetMemoryPageElement(size_t n);

	static void DefaultConstructElements(void* pElements, size_t nElements)
	{
		for (size_t i = 0; i < nElements; ++i)
		{
			pElements = (::new(pElements) T()) + 1;
		}
	}

	static void DestroyElements(T* pElements, size_t nElements)
	{
		for (size_t i = 0; i < nElements; ++i)
		{
			pElements[i].~T();
		}
	}

	static void MoveElements(T* pDestination, T* pSource, size_t nElements)
	{
		for (size_t i = 0; i < nElements; ++i)
		{
			::new(static_cast<void*>(pDestination + i))T(std::move(pSource[i]));
			pSource[i].~T();
		}
	}

	/////////////////////////////////////
	// Private Member Variables.
	T* m_arrData;                         //!< Storage for the continuous memory part, during coalescing resized to hold all page memory.
	LONG m_nCapacity;                     //!< Avaible Memory in continuous memory part, if exhausted during 'Fill' phase, pages as temp memory chunks are allocated.

	CMemoryPage* m_pMemoryPages;          //!< Single linked list of memory chunks, used for fallback allocations during 'Fill' phase (to prevent changing the continuous memory block during 'Fill'.

	LONG m_nSize;                         //!< Number of elements currently in the container, can be larger than m_nCapacity due the nonContinuousPages.

	bool m_bElementAccessSafe;            //!< Boolean to indicate if we are currently doing a 'CoalasceMemory' step, during which some operations are now allowed.

};

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline CThreadSafeRendererContainer<T>::CThreadSafeRendererContainer() :
	m_arrData(NULL),
	m_nCapacity(0),
	m_pMemoryPages(NULL),
	m_nSize(0),
	m_bElementAccessSafe(true)
{
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline CThreadSafeRendererContainer<T>::~CThreadSafeRendererContainer()
{
	clear();
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline size_t CThreadSafeRendererContainer<T >::size() const
{
	return *const_cast<volatile LONG*>(&m_nSize);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline size_t CThreadSafeRendererContainer<T >::empty() const
{
	return *const_cast<volatile LONG*>(&m_nSize) == 0;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline size_t CThreadSafeRendererContainer<T >::capacity() const
{
	// capacity of continuous memory block
	LONG nCapacity = m_nCapacity;

	// add capacity of all memory pages
	CMemoryPage* pCurrentMemoryPage = m_pMemoryPages;
	while (pCurrentMemoryPage)
	{
		nCapacity += pCurrentMemoryPage->Capacity();
		pCurrentMemoryPage = pCurrentMemoryPage->m_pNext;
	}

	return nCapacity;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline T& CThreadSafeRendererContainer<T >::operator[](size_t n)
{
	assert(m_bElementAccessSafe);
	T* pRet = NULL;

	assert((LONG)n < m_nSize);

	if ((LONG)n < m_nCapacity)
	{
		pRet = &m_arrData[n];
	}
	else
	{
		pRet = &GetMemoryPageElement(n);
	}
	return *pRet;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline const T& CThreadSafeRendererContainer<T >::operator[](size_t n) const
{
	return const_cast<const T&>(const_cast<CThreadSafeRendererContainer<T>*>(this)->operator[](n));
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void* CThreadSafeRendererContainer<T >::push_back_new()
{
	assert(m_bElementAccessSafe);
	size_t nUnused = ~0;
	return push_back_impl(nUnused);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void* CThreadSafeRendererContainer<T >::push_back_new(size_t& nIndex)
{
	assert(m_bElementAccessSafe);
	return push_back_impl(nIndex);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeRendererContainer<T >::push_back(const T& rObj)
{
	assert(m_bElementAccessSafe);
	size_t nUnused = ~0;
	void* pObj = push_back_impl(nUnused);
	::new(pObj) T(rObj);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeRendererContainer<T >::push_back(const T& rObj, size_t& nIndex)
{
	assert(m_bElementAccessSafe);
	void* pObj = push_back_impl(nIndex);
	::new(pObj) T(rObj);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeRendererContainer<T >::push_back(T&& rObj)
{
	assert(m_bElementAccessSafe);
	size_t nUnused = ~0;
	void* pObj = push_back_impl(nUnused);
	::new(pObj) T(std::move(rObj));
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeRendererContainer<T >::push_back(T&& rObj, size_t& nIndex)
{
	assert(m_bElementAccessSafe);
	void* pObj = push_back_impl(nIndex);
	::new(pObj) T(std::move(rObj));
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeRendererContainer<T >::clear()
{
	assert(m_bElementAccessSafe);
	size_t remainingSize = m_nSize;

	// free non-continuous pages if we have some
	CMemoryPage* pCurrentMemoryPage = m_pMemoryPages;
	while (pCurrentMemoryPage)
	{
		CMemoryPage* pOldPage = pCurrentMemoryPage;
		pCurrentMemoryPage = pCurrentMemoryPage->m_pNext;
		remainingSize -= pOldPage->Size();
		DestroyElements(pOldPage->GetData(), pOldPage->Size());
		free(pOldPage);
	}
	m_pMemoryPages = NULL;

	// free continuous part
	DestroyElements(m_arrData, remainingSize);
	CryModuleMemalignFree(m_arrData);
	m_arrData = NULL;

	m_nSize = 0;
	m_nCapacity = 0;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeRendererContainer<T >::resize(size_t n)
{
	assert(m_bElementAccessSafe);
	CoalesceMemory();
	size_t nOldSize = m_nSize;
	m_nSize = n;

	if ((LONG)n <= m_nCapacity)
		return;

	T* arrOldData = m_arrData;
	m_arrData = reinterpret_cast<T*>(CryModuleMemalign(n * sizeof(T), alignof(T)));
	MoveElements(m_arrData, arrOldData, nOldSize);
	DefaultConstructElements(m_arrData + nOldSize, n - nOldSize);
	CryModuleMemalignFree(arrOldData);

	m_nCapacity = n;

}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void CThreadSafeRendererContainer<T >::reserve(size_t n)
{
	assert(m_bElementAccessSafe);
	CoalesceMemory();
	if ((LONG)n <= m_nCapacity)
		return;

	T* arrOldData = m_arrData;
	m_arrData = reinterpret_cast<T*>(CryModuleMemalign(n * sizeof(T), alignof(T)));
	MoveElements(m_arrData, arrOldData, m_nSize);
	CryModuleMemalignFree(arrOldData);

	m_nCapacity = n;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline bool CThreadSafeRendererContainer<T >::try_append_to_continuous_memory(size_t& nIndex, void*& pObj)
{
	assert(m_bElementAccessSafe);
	LONG nSize = ~0;
	LONG nCapacity = ~0;
	do
	{
		// read volatile the new size
		nSize = *const_cast<volatile LONG*>(&m_nSize);
		nCapacity = *const_cast<volatile LONG*>(&m_nCapacity);

		if (nSize >= nCapacity)
			return false;
	}
	while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&m_nSize), nSize + 1, nSize) != nSize);
	nIndex = nSize;
	pObj = &m_arrData[nSize];

	return true;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void* CThreadSafeRendererContainer<T >::push_back_impl(size_t& nIndex)
{
	assert(m_bElementAccessSafe);
	void* pObj = NULL;

	// non atomic check to see if there is space in the continuous array
	if (try_append_to_continuous_memory(nIndex, pObj))
		return pObj;

	// exhausted continuous memory, falling back to page allocation
	for (;; )
	{
		assert(m_bElementAccessSafe);
		size_t nPageBaseIndex = 0;

		// traverse the page list till the first page with free memory
		CMemoryPage* pCurrentMemoryPage = m_pMemoryPages;
		while (pCurrentMemoryPage)
		{
			size_t nAvaibleElements = pCurrentMemoryPage->Capacity() - pCurrentMemoryPage->Size();
			if (nAvaibleElements)
				break;

			// no memory in this page, go to the next one
			nPageBaseIndex += pCurrentMemoryPage->Capacity();
			pCurrentMemoryPage = pCurrentMemoryPage->m_pNext;
		}

		// try to allocate a element on this page
		if (pCurrentMemoryPage && pCurrentMemoryPage->TryAllocateElement(nIndex, pObj))
		{
			// update global elements counter
			CryInterlockedIncrement(alias_cast<volatile int*>(&m_nSize));

			// adjust in-page-index to global index
			nIndex += nPageBaseIndex + m_nCapacity;
			return pObj;
		}
		else
		{
			// all pages are empty, allocate and link a new one
			CMemoryPage* pNewPage = CMemoryPage::AllocateNewPage();

			void* volatile* ppLastMemoryPageAddress = NULL;
			do
			{
				// find place to link in page
				CMemoryPage* pLastMemoryPage = m_pMemoryPages;
				ppLastMemoryPageAddress = alias_cast<void* volatile*>(&m_pMemoryPages);

				while (pLastMemoryPage)
				{
					ppLastMemoryPageAddress = alias_cast<void* volatile*>(&(pLastMemoryPage->m_pNext));
					pLastMemoryPage = pLastMemoryPage->m_pNext;
				}

			}
			while (CryInterlockedCompareExchangePointer(ppLastMemoryPageAddress, pNewPage, NULL) != NULL);
		}

	}
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline T& CThreadSafeRendererContainer<T >::GetMemoryPageElement(size_t n)
{
	assert(m_bElementAccessSafe);
	size_t nFirstListIndex = m_nCapacity;
	CMemoryPage* pCurrentMemoryPage = m_pMemoryPages;

	size_t nPageCapacity = pCurrentMemoryPage->Capacity();
	while (n >= (nFirstListIndex + nPageCapacity))
	{
		// this is threadsafe because we assume that if we want to get element 'n'
		// the clientcode did already fill the container up to element 'n'
		// thus up to 'n', m_pNonContinuousList will have valid pages
		// NOTE: This is not safe when trying to read a element behind the valid
		// range (same as std::vector)
		nFirstListIndex += nPageCapacity;
		pCurrentMemoryPage = pCurrentMemoryPage->m_pNext;

		// update page capacity, since it can differe due alignment
		nPageCapacity = pCurrentMemoryPage->Capacity();
	}

	return pCurrentMemoryPage->GetElement(n - nFirstListIndex);
}

//! When not not in the 'Fill' phase, it is safe to colace all page entries into one continuous memory block.
template<typename T>
inline void CThreadSafeRendererContainer<T >::CoalesceMemory()
{
	assert(m_bElementAccessSafe);
	if (m_pMemoryPages == NULL)
		return; // nothing to do

	// mark state as not accessable
	m_bElementAccessSafe = false;

	size_t nOldSize = m_nSize;

	// compute required memory
	size_t nRequiredElements = 0;
	{
		CMemoryPage* pCurrentMemoryPage = m_pMemoryPages;
		while (pCurrentMemoryPage)
		{
			nRequiredElements += pCurrentMemoryPage->Size();
			pCurrentMemoryPage = pCurrentMemoryPage->m_pNext;
		}
	}

	T* arrOldData = m_arrData;
	m_arrData = reinterpret_cast<T*>(CryModuleMemalign((m_nCapacity + nRequiredElements) * sizeof(T), alignof(T)));
	size_t nContinuousElements = nOldSize - nRequiredElements;
	MoveElements(m_arrData, arrOldData, nContinuousElements);
	CryModuleMemalignFree(arrOldData);

	// copy page data into continuous memory block
	size_t nBeginToFillIndex = nContinuousElements;
	CMemoryPage* pCurrentMemoryPage = m_pMemoryPages;
	while (pCurrentMemoryPage)
	{
		// copy data
		MoveElements(m_arrData + nBeginToFillIndex, pCurrentMemoryPage->GetData(), pCurrentMemoryPage->Size());
		nBeginToFillIndex += pCurrentMemoryPage->Size();

		// free page
		CMemoryPage* pOldPage = pCurrentMemoryPage;
		pCurrentMemoryPage = pCurrentMemoryPage->m_pNext;
		free(pOldPage);
	}
	assert(nBeginToFillIndex == m_nSize);

	m_pMemoryPages = NULL;
	m_nCapacity += nRequiredElements;

	// the container can be used again
	m_bElementAccessSafe = true;
}

//! Collect information about used memory.
template<typename T>
void CThreadSafeRendererContainer<T >::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_arrData, m_nCapacity * sizeof(T));

	CMemoryPage* pCurrentMemoryPage = m_pMemoryPages;
	while (pCurrentMemoryPage)
	{
		pSizer->AddObject(pCurrentMemoryPage, CMemoryPage::nMemoryPageSize);
		pCurrentMemoryPage = pCurrentMemoryPage->m_pNext;
	}
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline CThreadSafeRendererContainer<T>::CMemoryPage::CMemoryPage() :
	m_pNext(NULL),
	m_nSize(0)
{
	// compute offset for actual data
	size_t nObjectAlignment = alignof(T);
	UINT_PTR nMemoryBlockBegin = alias_cast<UINT_PTR>(this);
	UINT_PTR nMemoryBlockEnd = alias_cast<UINT_PTR>(this) + nMemoryPageSize;

	nMemoryBlockBegin += sizeof(CMemoryPage);
	nMemoryBlockBegin = (nMemoryBlockBegin + nObjectAlignment - 1) & ~(nObjectAlignment - 1);

	// compute number of avaible elements
	assert((nMemoryBlockEnd - nMemoryBlockBegin) > 0);
	m_nCapacity = (LONG)((nMemoryBlockEnd - nMemoryBlockBegin) / sizeof(T));

	// store pointer to store data to
	m_arrData = alias_cast<T*>(nMemoryBlockBegin);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline typename CThreadSafeRendererContainer<T>::CMemoryPage * CThreadSafeRendererContainer<T>::CMemoryPage::AllocateNewPage()
{
	void* pNewPageMemoryChunk = malloc(nMemoryPageSize);
	assert(pNewPageMemoryChunk != NULL);

	memset(pNewPageMemoryChunk, 0, nMemoryPageSize);
	CMemoryPage* pNewPage = new(pNewPageMemoryChunk) CMemoryPage();
	return pNewPage;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline bool CThreadSafeRendererContainer<T>::CMemoryPage::TryAllocateElement(size_t & nIndex, void* & pObj)
{
	LONG nSize = ~0;
	LONG nCapacity = ~0;
	do
	{
		// read volatile the new size
		nSize = *const_cast<volatile LONG*>(&m_nSize);
		nCapacity = *const_cast<volatile LONG*>(&m_nCapacity);
		// stop trying if this page is full
		if (nSize >= nCapacity)
			return false;
	}
	while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&m_nSize), nSize + 1, nSize) != nSize);

	//Note: this is the index in the page and it is adjusted in the calling context
	nIndex = nSize;
	pObj = &m_arrData[nSize];

	return true;

}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline T &CThreadSafeRendererContainer<T>::CMemoryPage::GetElement(size_t n)
{
	assert((LONG)n < m_nSize);
	assert(m_nSize <= m_nCapacity);
	return m_arrData[n];
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline T * CThreadSafeRendererContainer<T>::CMemoryPage::GetData() const
{
	return m_arrData;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline size_t CThreadSafeRendererContainer<T>::CMemoryPage::Size() const
{
	return m_nSize;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline size_t CThreadSafeRendererContainer<T>::CMemoryPage::Capacity() const
{
	return m_nCapacity;
}

#endif // _CRY_THREAD_SAFE_RENDERER_CONTAINER_H_

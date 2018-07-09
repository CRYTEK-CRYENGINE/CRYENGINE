// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Michael Kopietz
// Modified: -
//
//---------------------------------------------------------------------------

#ifndef __CCRYPOOLALLOCATOR__
#define __CCRYPOOLALLOCATOR__

namespace NCryPoolAlloc
{

template<class TPool, class TItem>
class CFirstFit : public TPool
{
public:
	ILINE CFirstFit()
	{
	}

	template<class T>
	ILINE T Allocate(size_t Size, size_t Align = 1)
	{
		//fastpath?
		if (TPool::m_pEmpty && TPool::m_pEmpty->Available(Size, Align))
		{
			TItem* pItem = TPool::Split(TPool::m_pEmpty, Size, Align);
			if (!pItem)
				return 0;
			pItem->InUse(Align);
			TPool::AllocatedMemory(pItem->MemSize());

			//not fully occupied empty space?
			TPool::m_pEmpty = pItem != TPool::m_pEmpty ? TPool::m_pEmpty : 0;
			return TPool::Handle(pItem);
		}

		TItem* pBestItem;
		for (pBestItem = TPool::m_Items.First(); pBestItem; pBestItem = pBestItem->Next())
		{
			if (pBestItem->Available(Size, Align))                        // && (!pBestItem || pItem->MemSize()<pBestItem->MemSize()))
				break;
		}
		if (!pBestItem)
			return 0;                           //out of mem

		TItem* pItem = TPool::Split(pBestItem, Size, Align);
		if (!pItem)                           //no free node
			return 0;
		pItem->InUse(Align);
		TPool::AllocatedMemory(pItem->MemSize());

		//not fully occupied empty space?
		TPool::m_pEmpty = pItem != pBestItem ? pBestItem : 0;
		return TPool::Handle(pItem);
	}
	template<class T>
	ILINE bool Free(T Handle, bool ForceBoundsCheck = false)
	{
		return Handle ? TPool::Free(Handle, ForceBoundsCheck) : false;
	}

};

template<class TPool, class TItem>
class CWorstFit : public TPool
{
public:
	ILINE CWorstFit()
	{
	}

	template<class T>
	ILINE T Allocate(size_t Size, size_t Align = 1)
	{
		TItem* pBestItem = 0;
		for (TItem* pItem = TPool::m_Items.First(); pItem; pItem = pItem->Next())
		{
			if (pItem->IsFree() && (!pBestItem || pItem->MemSize() > pBestItem->MemSize()))
				pBestItem = pItem;
		}
		if (!pBestItem || !pBestItem->Available(Size, Align))
			return 0;                           //out of mem

		TItem* pItem = Split(pBestItem, Size, Align);
		if (!pItem)                           //no free node
			return 0;
		pItem->InUse(Align);
		AllocatedMemory(pItem->MemSize());
		return Handle(pItem);
	}

};

template<class TPool, class TItem>
class CBestFit : public TPool
{
public:
	ILINE CBestFit()
	{
	}

	template<class T>
	ILINE T Allocate(size_t Size, size_t Align = 1)
	{
		TItem* pBestItem = 0;
		for (TItem* pItem = TPool::m_Items.First(); pItem; pItem = pItem->Next())
		{
			if ((!pBestItem || pItem->MemSize() < pBestItem->MemSize()) && pItem->Available(Size, Align))
			{
				if (pItem->MemSize() == Size)
				{
					pItem->InUse(Align);
					AllocatedMemory(pItem->MemSize());
					return (T)Handle(pItem);
				}
				pBestItem = pItem;
			}
		}
		if (!pBestItem)
			return 0;                           //out of mem

		TItem* pItem = Split(pBestItem, Size, Align);
		if (!pItem)                           //no free node
			return 0;
		pItem->InUse(Align);
		AllocatedMemory(pItem->MemSize());
		return (T)Handle(pItem);
	}
};

template<class TAllocator>
class CReallocator : public TAllocator
{
public:

	template<class T>
	ILINE bool Reallocate(T* pData, size_t Size, size_t Alignment)
	{
		//special cases
		if (!Size)                          //just free?
		{
			TAllocator::Free(*pData);
			*pData = 0;
			return true;
		}

		if (!*pData)                          //just alloc?
		{
			*pData = TAllocator::template Allocate<T>(Size, Alignment);
			return *pData != 0;
		}

		//same size, nothing to do at all?
		if (TAllocator::Item(*pData)->MemSize() == Size)
			return true;

		if (TAllocator::ReSize(pData, Size))
			return true;

		T pNewData = TAllocator::template Allocate<T>(Size, Alignment);
		if (!pNewData)
			return false;
		memcpy(TAllocator::template Resolve<uint8*>(pNewData),
		       TAllocator::template Resolve<uint8*>(*pData), min(TAllocator::Item(*pData)->MemSize(), Size));
		TAllocator::template Free(*pData);
		*pData = pNewData;
		return true;
	}

};

}

#endif

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Michael Kopietz
// Modified: -
//
//---------------------------------------------------------------------------

#ifndef __CCRYPOOLCONTAINER__
#define __CCRYPOOLCONTAINER__

namespace NCryPoolAlloc
{

template<size_t TElementCount, class TElement>
class CPool : public CMemoryStatic<TElementCount* sizeof(TElement)>
{
	class CPoolNode;
	class CPoolNode : public CListItem<CPoolNode>
	{
	};
	CList<CPoolNode> m_List;
public:
	ILINE CPool()
	{
		CPoolNode* pPrev = 0;
		CPoolNode* pNode = 0;
		for (size_t a = 1; a < TElementCount; a++)                    //skip first element as it would be counted as zero ptr
		{
			uint8* pData = &CMemoryStatic<TElementCount* sizeof(TElement)>::Data()[a * sizeof(TElement)];
			pNode = reinterpret_cast<CPoolNode*>(pData);
			pNode->Prev(pPrev);
			if (pPrev)
				pPrev->Next(pNode);
			else
				m_List.First(pNode);
			pPrev = pNode;
			//																m_List.AddLast(pNode);
		}
		if (pPrev)
		{
			pPrev->Next(0);
			m_List.Last(pPrev);
		}
	}
	ILINE uint8* Allocate(size_t Size, size_t Align = 1)
	{
		CPoolNode* pNode = m_List.PopFirst();
		return reinterpret_cast<uint8*>(pNode);
	}

	template<class T>
	ILINE void Free(T* pData)
	{
		if (pData)
		{
			CPoolNode* pNode = reinterpret_cast<CPoolNode*>(pData);
			m_List.AddLast(pNode);
		}
	}

	ILINE TElement& operator[](uint32 Idx)
	{
		uint8* pData = &CMemoryStatic<TElementCount* sizeof(TElement)>::Data()[Idx * sizeof(TElement)];
		return *reinterpret_cast<TElement*>(pData);
	}

	ILINE const TElement& operator[](uint32 Idx) const
	{
		const uint8* pData = &CMemoryStatic<TElementCount* sizeof(TElement)>::Data()[Idx * sizeof(TElement)];
		return *reinterpret_cast<const TElement*>(pData);
	}
};

template<class TMemory, bool BoundsCheck = false>
class CInPlace : public TMemory
{
protected:
	CList<CListItemInPlace> m_Items;
	size_t                  m_Allocated;
	CListItemInPlace*       m_pEmpty;

	ILINE void AllocatedMemory(size_t S)
	{
		m_Allocated += S + sizeof(CListItemInPlace);
	}
	ILINE void FreedMemory(size_t S)
	{
		m_Allocated -= S + sizeof(CListItemInPlace);
	}
	ILINE void Stack(CListItemInPlace* pItem)
	{
	}
public:
	ILINE CInPlace() :
		m_Allocated(0)
	{
	}

	ILINE void InitMem(const size_t S = 0, uint8* pData = 0)
	{
		TMemory::InitMem(S, pData);
		if (!TMemory::MemSize())
			return;
		pData = TMemory::Data();
		CListItemInPlace* pFirst = reinterpret_cast<CListItemInPlace*>(pData);
		CListItemInPlace* pFree = pFirst + 1;
		CListItemInPlace* pLast = reinterpret_cast<CListItemInPlace*>(pData + TMemory::MemSize()) - 1;
		m_Items.~CList<CListItemInPlace>();
		new(&m_Items)CList<CListItemInPlace>();
		m_Items.AddLast(pFirst);
		m_Items.AddLast(pFree);
		m_Items.AddLast(pLast);

		pFirst->InUse(0);                               //static first item
		pFree->Free();
		pLast->InUse(0);                                //static last item
		m_pEmpty = pFree;
		m_Allocated = 0;
	}

	ILINE size_t FragmentCount() const
	{
		return m_Items.Count();
	}

	ILINE CListItemInPlace* Split(CListItemInPlace* pItem, size_t Size, size_t Align)
	{
		size_t Offset = reinterpret_cast<size_t>(pItem->Data());
		Offset += pItem->MemSize();                       //ptr to end
		Offset -= Size;                                   //minus size
		Size += Offset & (Align - 1);                     //adjust size to fit required alignment
		Offset -= Offset & (Align - 1);
		size_t TSize = sizeof(CListItemInPlace);
		Offset -= TSize;                              //header

		if (Offset <= reinterpret_cast<size_t>(pItem + 1))                         //not enough space for splitting?
			return pItem;

		CListItemInPlace* pItemNext = reinterpret_cast<CListItemInPlace*>(Offset);

		const size_t Offset2 = reinterpret_cast<size_t>(pItemNext->Data());
		CPA_ASSERT(!(Offset2 & (Align - 1)));
		m_Items.AddBehind(pItemNext, pItem);
		//pItemNext->Prev(pItem);
		//pItemNext->Next(pItem->Next());

		//																	if(pItem->Next())
		//																		pItem->Next()->Prev(pItemNext);

		//																	pItem->Next(pItemNext);
		pItemNext->Free();
		return pItemNext;
	}

	ILINE void Merge(CListItemInPlace* pItem)
	{
		//merge with next if possible
		CListItemInPlace* pItemNext = pItem->Next();
		if (pItemNext->IsFree())
		{
			if (m_pEmpty == pItemNext)
				m_pEmpty = pItem;
			m_Items.Remove(pItemNext);
			//pItem->Next(pItemNext->Next());
			//pItem->Next()->Prev(pItem);
		}
		//merge with prev if possible
		CListItemInPlace* pItemPrev = pItem->Prev();
		if (pItemPrev->IsFree())
		{
			if (m_pEmpty == pItem)
				m_pEmpty = pItemPrev;
			m_Items.Remove(pItem);
			//pItemPrev->Next(pItem->Next());
			//pItem->Next()->Prev(pItemPrev);
			pItem = pItemPrev;
		}
	}
	template<class T>
	ILINE T Resolve(void* rItem)  const
	{
		return reinterpret_cast<T>(rItem);
	}

	template<class T>
	ILINE size_t Size(const T* pData)  const
	{
		const CListItemInPlace* pItem = Item(pData);
		return pItem->MemSize();
	}

	bool InBounds(const void* pData, const bool Check)  const
	{
		return !Check || (
		  reinterpret_cast<size_t>(pData) >= reinterpret_cast<size_t>(TMemory::Data()) &&
		  reinterpret_cast<size_t>(pData) < reinterpret_cast<size_t>(TMemory::Data()) + TMemory::MemSize());
	}

	template<class T>
	ILINE bool Free(T* pData, bool ForceBoundsCheck = false)
	{
		if (pData && InBounds(pData, BoundsCheck | ForceBoundsCheck))
		{
			CListItemInPlace* pItem = Item(pData);
			FreedMemory(pItem->MemSize());
			pItem->Free();
			Merge(pItem);
			return true;
		}
		return false;
	}

	ILINE bool   Beat()          { return false; }     //dummy beat in case no defragmentator is wraping
	ILINE size_t MemFree() const { return TMemory::MemSize() - m_Allocated; }
	ILINE size_t MemSize() const { return TMemory::MemSize(); }

	ILINE uint8* Handle(CListItemInPlace* pItem) const
	{
		return pItem->Data();
	}
	template<class T>
	ILINE CListItemInPlace* Item(T* pData)
	{
		return reinterpret_cast<CListItemInPlace*>(pData) - 1;
	}
	template<class T>
	ILINE const CListItemInPlace* Item(const T* pData)  const
	{
		return reinterpret_cast<const CListItemInPlace*>(pData) - 1;
	}
	ILINE static bool Defragmentable() { return false; }

	template<class T>
	ILINE bool ReSize(T* pData, size_t SizeNew)
	{
		//special cases
		CListItemInPlace* pItem = Item(*pData);
		const size_t SizeOld = pItem->MemSize();

		//reduction
		if (SizeOld > SizeNew)
		{
			if (pItem->Next()->IsFree())
			{
				CListItemInPlace* pNextNext = pItem->Next()->Next();
				size_t Offset = reinterpret_cast<size_t>(pItem->Data());
				Offset += SizeNew;                                    //Offset to next
				CListItemInPlace* pItemNext = reinterpret_cast<CListItemInPlace*>(Offset);
				pItem->Next(pItemNext);
				pNextNext->Prev(pItemNext);
				pItemNext->Prev(pItem);
				pItemNext->Next(pNextNext);
				pItemNext->Free();
				return true;
			}

			if (SizeOld - SizeNew <= sizeof(CListItemInPlace))
				return true;                                //header is bigger than the amount of freed memory

			//split
			size_t Offset = reinterpret_cast<size_t>(pItem->Data());
			Offset += SizeNew;                                    //Offset to next
			CListItemInPlace* pItemNext = reinterpret_cast<CListItemInPlace*>(Offset);
			m_Items.AddBehind(pItemNext, pItem);
			pItemNext->Free();
			return true;
		}

		//SizeOld<SizeNew grow
		CListItemInPlace* pNext = pItem->Next();
		CListItemInPlace* pNextNext = pNext->Next();
		const size_t SizeNext = pNext->IsFree() ? pNext->MemSize() + sizeof(CListItemInPlace) : 0;
		if (SizeNew <= SizeNext + SizeOld)
		{
			if (SizeNew + sizeof(CListItemInPlace) + 1 < SizeNext + SizeOld)
			{
				size_t Offset = reinterpret_cast<size_t>(pItem->Data());
				Offset += SizeNew;                                    //Offset to next
				CListItemInPlace* pItemNext = reinterpret_cast<CListItemInPlace*>(Offset);
				pItem->Next(pItemNext);
				pNextNext->Prev(pItemNext);
				pItemNext->Prev(pItem);
				pItemNext->Next(pNextNext);
				pItemNext->Free();
			}
			else
			{
				pItem->Next(pNextNext);
				pNextNext->Prev(pItem);
			}
			return true;
		}
		return false;                               //no further in-place realloc possible
	}

};

template<class TMemory, size_t TNodeCount, bool BoundsCheck = false>
class CReferenced : public TMemory
{
	typedef CPool<TNodeCount, CListItemReference> tdNodePool;

protected:
	tdNodePool                m_NodePool;
	CList<CListItemReference> m_Items;
	size_t                    m_Allocated;
	CListItemReference*       m_pEmpty;

	ILINE void AllocatedMemory(size_t S)
	{
		m_Allocated += S;
	}
	ILINE void FreedMemory(size_t S)
	{
		m_Allocated -= S;
	}
	ILINE void Stack(CListItemReference* pItem)
	{
		m_Items.Validate(pItem);
		CListItemReference* pItem2 = 0;
		CListItemReference* pNext = pItem->Next();
		uint8* pData = pItem->Data(pNext->Align());
		if (pData != pItem->Data())                                 //needs splitting 'cause of alignment?
		{
			pItem2 = reinterpret_cast<CListItemReference*>(m_NodePool.Allocate(1, 1));
			if (!pItem2)                                //no free node found for splitting?
				return;                                   //failed to stack -> return
		}

		memmove(pData, pNext->Data(), pNext->MemSize());

		if (pItem2)                                   //was not aligned?
		{
			//then keep the current ITem
			const size_t SizeItem = pItem->MemSize();
			const size_t SizeNext = pNext->MemSize();
			m_Items.AddBehind(pItem2, pNext);
			pItem2->Data(pData + SizeNext);
			pNext->Data(pData);
			pItem2->MemSize(pItem2->Next()->Data() - pItem2->Data());
			pNext->MemSize(SizeNext);
			pItem->MemSize(pNext->Data() - pItem->Data());
			m_Items.Validate(pItem);
			m_Items.Validate(pItem2);
			m_Items.Validate(pNext);
		}
		else
		{
			const size_t SizeItem = pItem->MemSize();
			const size_t SizeNext = pNext->MemSize();
			m_Items.Remove(pItem);
			m_Items.AddBehind(pItem, pNext);
			pItem->Data(pNext->Data());
			pNext->Data(pData);
			pNext->MemSize(SizeItem);
			pItem->MemSize(SizeNext);
			m_Items.Validate(pItem);
			m_Items.Validate(pNext);
		}

	}
public:
	ILINE CReferenced() :
		m_Allocated(0)
	{
	}

	ILINE void InitMem(const size_t S = 0, uint8* pData = 0)
	{
		TMemory::InitMem(S, pData);
		if (!TMemory::MemSize())
			return;
		pData = TMemory::Data();
		CListItemReference* pItem = reinterpret_cast<CListItemReference*>(m_NodePool.Allocate(1, 1));
		CListItemReference* pLast = reinterpret_cast<CListItemReference*>(m_NodePool.Allocate(1, 1));
		m_Items.AddFirst(pItem);
		m_Items.AddLast(pLast);
		pLast->Init(pData + TMemory::MemSize(), 0, pItem, 0);
		pLast->InUse(0);
		pItem->Init(pData, TMemory::MemSize(), 0, pLast);
		pItem->Free();
		m_pEmpty = pItem;
		m_Allocated = 0;

	}

	ILINE size_t FragmentCount() const
	{
		return m_Items.Count();
	}

	ILINE CListItemReference* Split(CListItemReference* pItem, size_t Size, size_t Align)
	{
		size_t Offset = reinterpret_cast<size_t>(pItem->Data());
		if (!(Offset & (Align - 1)))                              //perfectly aligned?
		{
			if (pItem->MemSize() != Size)                                 //not perfectly fitting?
			{
				//then split
				CListItemReference* pItemPrev = reinterpret_cast<CListItemReference*>(m_NodePool.Allocate(1, 1));
				if (!pItemPrev)
					return 0;
				const size_t OrgSize = pItem->MemSize();
				m_Items.AddBefore(pItemPrev, pItem);
				pItemPrev->Data(pItem->Data());
				pItem->Data(pItem->Data() + Size);
				pItem->MemSize(OrgSize - Size);
				pItemPrev->MemSize(Size);
				pItem = pItemPrev;
			}
			return pItem;
		}

		//not aligned to block start
		//then lets try to align to block end
		Offset += pItem->MemSize();                           //ptr to end
		Offset -= Size;                                       //minus size
		if (!(Offset & (Align - 1)))                          //perfectly aligned?
		{
			CListItemReference* pItemPrev = reinterpret_cast<CListItemReference*>(m_NodePool.Allocate(1, 1));
			if (!pItemPrev)
				return 0;
			const size_t OrgSize = pItem->MemSize();
			m_Items.AddBefore(pItemPrev, pItem);
			pItemPrev->Data(pItem->Data());
			pItem->Data(reinterpret_cast<uint8*>(Offset));
			pItemPrev->MemSize(OrgSize - Size);
			pItem->MemSize(Size);
			pItemPrev->Free();
			return pItem;
		}
		//last resort, fragment it into 3 parts

		//Size	+=Offset&(Align-1);	//adjust size to fit required alignment
		Offset -= Offset & (Align - 1);

		CListItemReference* pItemPrev = reinterpret_cast<CListItemReference*>(m_NodePool.Allocate(1, 1));
		CListItemReference* pItemNext = reinterpret_cast<CListItemReference*>(m_NodePool.Allocate(1, 1));
		if (!pItemPrev || !pItemNext)
			return 0;
		const size_t OrgSize = pItem->MemSize();

		m_Items.AddBefore(pItemPrev, pItem);
		m_Items.AddBehind(pItemNext, pItem);

		pItemPrev->Data(pItem->Data());
		pItem->Data(reinterpret_cast<uint8*>(Offset));
		pItemNext->Data(pItem->Data() + Size);
		pItemPrev->MemSize(pItem->Data() - pItemPrev->Data());
		pItemNext->MemSize(OrgSize - pItemPrev->MemSize() - Size);
		pItem->MemSize(Size);

		pItemPrev->Free();
		pItemNext->Free();
		return pItem;
	}

	ILINE void Merge(CListItemReference* pItem)
	{
		m_Items.Validate(pItem);

		//merge with next if possible
		CListItemReference* pItemNext = pItem->Next();
		if (pItemNext && pItemNext->IsFree())
		{
			if (m_pEmpty == pItemNext)
				m_pEmpty = pItem;
			const size_t OrgSize = pItem->MemSize();
			const size_t NextSize = pItemNext->MemSize();
			m_Items.Remove(pItemNext);
			pItem->MemSize(OrgSize + NextSize);
			m_NodePool.Free(pItemNext);
		}
		//merge with prev if possible
		CListItemReference* pItemPrev = pItem->Prev();
		if (pItemPrev && pItemPrev->IsFree())
		{
			if (m_pEmpty == pItem)
				m_pEmpty = pItemPrev;
			const size_t OrgSize = pItem->MemSize();
			const size_t PrevSize = pItemPrev->MemSize();
			m_Items.Remove(pItem);
			pItemPrev->MemSize(PrevSize + OrgSize);
			m_NodePool.Free(pItem);
		}
	}

	template<class T>
	ILINE T Resolve(const uint32 ID)
	{
		CPA_ASSERT(ID);                                   //0 is invalid
		return reinterpret_cast<T>(Item(ID)->Data());
	}

	ILINE uint32 AddressToHandle(void* pData)
	{
		for (CListItemReference* pItem = m_Items.First(); pItem; pItem = pItem->Next())
		{
			if (pItem->Data() == pData)
				return Handle(pItem);
		}
		return 0;
	}

	template<class T>
	ILINE size_t Size(T ID)  const
	{
		CPA_ASSERT(ID);                                   //0 is invalid
		return Item(ID)->MemSize();
	}
	template<class T>
	bool InBounds(T ID, const bool Check) const
	{
		//boundscheck doesn't work for Referenced containers
		return true;
	}

	template<class T>
	ILINE bool Free(T ID, bool ForceBoundsCheck = false)
	{
		IF (!ID, false)
			return true;
		IF (!InBounds(ID, BoundsCheck | ForceBoundsCheck), false)
			return false;

		CListItemReference* pItem = Item(ID);
		FreedMemory(pItem->MemSize());
		pItem->Free();
		Merge(pItem);
		return true;
	}

	ILINE bool   Beat()          { return false; }           //!< Dummy beat in case no defragmentator is wrapping.

	ILINE size_t MemFree() const { return TMemory::MemSize() - m_Allocated; }

	ILINE size_t MemSize() const { return TMemory::MemSize(); }

	ILINE uint32 Handle(CListItemReference* pItem) const
	{
		return static_cast<uint32>(pItem - &m_NodePool[0]);
	}
	ILINE CListItemReference* Item(uint32 ID)
	{
		return &m_NodePool[ID];
	}
	ILINE const CListItemReference* Item(uint32 ID) const
	{
		return &m_NodePool[ID];
	}
	ILINE static bool Defragmentable() { return true; }

	template<class T>
	ILINE bool ReSize(T* pData, size_t SizeNew)
	{
		CListItemReference* pItem = Item(*pData);
		const size_t SizeOld = pItem->MemSize();

		//reduction
		if (SizeOld > SizeNew)
		{
			if (pItem->Next()->IsFree())
			{
				CListItemReference* pNext = pItem->Next();
				const size_t NextSize = pNext->MemSize();
				pNext->Data(pNext->Data() + SizeNew - SizeOld);
				pNext->MemSize(NextSize - SizeNew + SizeOld);
				pItem->MemSize(SizeNew);
				return true;
			}

			//split
			CListItemReference* pItemNext = reinterpret_cast<CListItemReference*>(m_NodePool.Allocate(1, 1));
			m_Items.AddBehind(pItemNext, pItem);
			pItemNext->Data(pItem->Data() + SizeNew);
			pItem->MemSize(SizeNew);
			pItemNext->MemSize(SizeOld - SizeNew);
			pItemNext->Free();
			return true;
		}

		//SizeOld<SizeNew grow
		CListItemReference* pNext = pItem->Next();
		const size_t SizeNext = pNext->IsFree() ? pNext->MemSize() : 0;
		if (SizeNew <= SizeNext + SizeOld)
		{
			if (SizeNew == SizeNext + SizeOld)
			{
				m_Items.Remove(pNext);
				m_NodePool.Free(pNext);
			}
			else
			{
				pNext->Data(pNext->Data() + SizeNew - SizeOld);
				pNext->MemSize(SizeNext - SizeNew + SizeOld);
			}
			pItem->MemSize(SizeNew);
			return true;
		}
		return false;                                   //no further in-place realloc possible
	}

};

}

#endif

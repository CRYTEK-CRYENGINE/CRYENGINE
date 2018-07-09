// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Michael Kopietz
// Modified: -
//
//---------------------------------------------------------------------------
#ifndef __CCRYPOOLLIST__
#define __CCRYPOOLLIST__

namespace NCryPoolAlloc
{

class CListItemInPlace;
class CListItemReference;

template<typename TItem>
class CListItem
{
	TItem* m_pPrev;
	TItem* m_pNext;
public:

	ILINE TItem*       Prev()             { return m_pPrev; }
	ILINE TItem*       Next()             { return m_pNext; }
	ILINE const TItem* Prev() const       { return m_pPrev; }
	ILINE const TItem* Next() const       { return m_pNext; }
	ILINE void         Prev(TItem* pPrev) { m_pPrev = pPrev; }
	ILINE void         Next(TItem* pNext) { m_pNext = pNext; }

	//! Used for debugging.
	void Validate();
};

template<typename TItem>
class CListItemFlagged : public CListItem<TItem>
{
	enum
	{
		ELIF_INUSE  = (1 << 0),
		ELIF_LOCKED = (1 << 1),
	};
	uint32 m_Flags : 8;
	uint32 m_Align : 24;
public:
	ILINE CListItemFlagged() :
		m_Flags(0)
	{
	}
	ILINE bool   IsFree() const  { return (m_Flags & ELIF_INUSE) != ELIF_INUSE; }
	ILINE void   Free()          { m_Flags &= ~ELIF_INUSE; }
	ILINE void   InUse(uint32 A) { m_Flags |= ELIF_INUSE; m_Align = A; }
	ILINE bool   Locked() const  { return ELIF_LOCKED == (m_Flags & ELIF_LOCKED); }
	ILINE void   Lock()          { m_Flags |= ELIF_LOCKED; }
	ILINE void   Unlock()        { m_Flags &= ~ELIF_LOCKED; }
	ILINE uint32 Align() const   { return m_Align; }
};

class CListItemInPlace : public CListItemFlagged<CListItemInPlace>
{
public:
	ILINE void Init(uint8* pData, size_t Size, CListItemInPlace* pPrev, CListItemInPlace* pNext)
	{
		Prev(pPrev);
		Next(pNext);
		CPA_ASSERT(Size == MemSize());
	}

	ILINE bool Available(size_t Size, size_t Align) const
	{
		size_t Offset = reinterpret_cast<size_t>(Data());
		if (Offset & (Align - 1))                        //not aligned?
			Size += sizeof(CListItemInPlace) + Align - 1;  //then an intermedian node needs to fit
		return Size <= MemSize() && IsFree();
	}
	ILINE uint8*       Data()       { return reinterpret_cast<uint8*>(this) + sizeof(CListItemInPlace); }
	ILINE const uint8* Data() const { return reinterpret_cast<const uint8*>(this) + sizeof(CListItemInPlace); }
	ILINE size_t       MemSize() const
	{
		const uint8* pNext = reinterpret_cast<const uint8*>(Next());
		const uint8* pThis = reinterpret_cast<const uint8*>(this);
		const size_t ESize = sizeof(CListItemInPlace);
		size_t Delta = pNext - pThis;
		Delta -= ESize;
		return Delta;
	}
};

class CListItemReference : public CListItemFlagged<CListItemReference>
{
	uint8* m_pData;
	//	size_t							m_Size;
public:
	ILINE void Init(uint8* pData, size_t Size, CListItemReference* pPrev, CListItemReference* pNext)
	{
		Data(pData);
		Prev(pPrev);
		Next(pNext);
		MemSize(Size);
	}
	ILINE bool Available(size_t Size, size_t Align) const
	{
		size_t Offset = reinterpret_cast<size_t>(Data());
		if ((Offset & (Align - 1)))
			Size += Align - (Offset & (Align - 1));
		return Size <= MemSize() && IsFree();
	}
	ILINE void   Data(uint8* pData) { m_pData = pData; }
	ILINE uint8* Data(size_t Align)
	{
		Align--;
		size_t Offset = reinterpret_cast<size_t>(m_pData);
		Offset = (Offset + Align) & ~Align;
		return reinterpret_cast<uint8*>(Offset);
	}
	ILINE uint8*       Data()               { return m_pData; }
	ILINE const uint8* Data() const         { return m_pData; }
	ILINE void         MemSize(size_t Size) {}
	ILINE size_t       MemSize() const
	{
		const size_t T = reinterpret_cast<size_t>(Data());
		const size_t N = Next() ? reinterpret_cast<size_t>(Next()->Data()) : T;
		return N - T;
	}
	//ILINE	void					MemSize(size_t Size){m_Size=Size;}
	//ILINE	size_t				MemSize()const{return m_Size;}
};

template<class TItem, bool VALIDATE = false>
class CList
{
	TItem* m_pFirst;
	TItem* m_pLast;
	size_t m_Count;
public:
	ILINE CList() :
		m_pFirst(0),
		m_pLast(0),
		m_Count(0)
	{
	}

	ILINE void   First(TItem* pItem) { m_pFirst = pItem; }
	ILINE TItem* First()             { return m_pFirst; }
	ILINE void   Last(TItem* pItem)  { m_pLast = pItem; }
	ILINE TItem* Last()              { return m_pLast; }
	ILINE bool   Empty() const       { return m_pFirst == 0; }

	ILINE TItem* PopFirst()
	{
		Validate();

		if (!m_pFirst)
			return 0;

		TItem* pRet = m_pFirst;

		m_pFirst = m_pFirst->Next();

		if (m_pFirst)                       //if any element exists
			m_pFirst->Prev(0);                //set prev ptr of this element to 0
		else
			m_pLast = 0;                      //set ptr to last element to 0 if ptr to first is zero as well

		Validate();
		m_Count--;
		return pRet;
	}

	ILINE TItem* PopLast()
	{
		Validate();

		if (!m_pLast)
			return 0;

		TItem* pRet = m_pLast;

		m_pLast = m_pLast->Prev();

		if (m_pLast)                      //if any element exists
			m_pLast->Next(0);               //set prev ptr of this element to 0
		else
			m_pFirst = 0;                   //set ptr to last element to 0 if ptr to first is zero as well

		Validate();
		m_Count--;
		return pRet;
	}

	ILINE void AddFirst(TItem* pItem)
	{
		CPA_ASSERT(pItem);              //ERROR AddFirst got 0 pointer

		Validate();

		pItem->Prev(0);
		pItem->Next(m_pFirst);
		if (!m_pFirst)
			m_pLast = pItem;
		else
			m_pFirst->Prev(pItem);
		m_pFirst = pItem;

		m_Count++;
		Validate();
	}

	ILINE void AddLast(TItem* pItem)
	{
		CPA_ASSERT(pItem);              //ERROR AddLast got 0 pointer

		Validate();

		pItem->Prev(m_pLast);
		pItem->Next(0);
		if (!m_pLast)
			m_pFirst = pItem;
		else
			m_pLast->Next(pItem);
		m_pLast = pItem;

		m_Count++;
		Validate();
	}
	ILINE void AddBefore(TItem* pItem, TItem* pItemSuccessor)
	{
		CPA_ASSERT(pItem);
		CPA_ASSERT(pItemSuccessor);

		Validate();

		pItem->Next(pItemSuccessor);
		pItem->Prev(pItemSuccessor->Prev());
		pItemSuccessor->Prev(pItem);

		if (pItemSuccessor == m_pFirst)
			m_pFirst = pItem;
		else
			pItem->Prev()->Next(pItem);

		m_Count++;
		Validate();
	}
	ILINE void AddBehind(TItem* pItem, TItem* pItemPredecessor)
	{
		CPA_ASSERT(pItem);
		CPA_ASSERT(pItemPredecessor);

		Validate();

		pItem->Next(pItemPredecessor->Next());
		pItem->Prev(pItemPredecessor);
		pItemPredecessor->Next(pItem);

		if (pItemPredecessor == m_pLast)
			m_pLast = pItem;
		else
			pItem->Next()->Prev(pItem);

		m_Count++;
		Validate();
	}

	ILINE void Remove(TItem* pItem)
	{
		CPA_ASSERT(pItem);              //ERROR releasing empty item

		if (pItem == m_pFirst)
		{
			PopFirst();
			return;
		}
		if (pItem == m_pLast)
		{
			PopLast();
			return;
		}

		Validate(pItem);

		pItem->Prev()->Next(pItem->Next());
		pItem->Next()->Prev(pItem->Prev());

		m_Count--;
		Validate();
	}

	//! Used for debugging.
	ILINE void Validate(TItem* pReferenceItem = 0)
	{
		if (!VALIDATE)
			return;

		//one-sided empty?
		CPA_ASSERT((!First() && !Last()) || (First() && Last()));              //ERROR validating item-list, just one end is 0

		// endles linking?
		TItem* pPrev = 0;
		TItem* pItem = First();
		while (pItem)
		{
			if (pReferenceItem == pItem)
				pReferenceItem = 0;
			CPA_ASSERT(pPrev == pItem->Prev());            //ERROR validating item-list, endless linking NULL
			pPrev = pItem;
			pItem = pItem->Next();
		}

		CPA_ASSERT(pPrev == Last());            //ERROR validating item-list, broken list, does not end at specified Last item
		CPA_ASSERT(!pReferenceItem);            //ERROR reference item not found in the item-list
	}
	ILINE size_t Count() const { return m_Count; }
};
}

#endif

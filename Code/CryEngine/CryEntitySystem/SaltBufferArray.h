// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "SaltHandle.h"           // CSaltHandle<>

#define SALT_DEFAULT_SIZE (64 * 1024)

// use one instance of this class in your object manager
// quite efficient implementation, avoids use of pointer to save memory and reduce cache misses
template<class TSalt = unsigned short, class TIndex = unsigned short, unsigned int TSize = SALT_DEFAULT_SIZE - 3>
class CSaltBufferArray
{
public:
	// constructor
	CSaltBufferArray()
	{
		assert((TIndex)TSize != (TIndex) - 1);      // we need one index to get an end marker
		assert((TIndex)TSize != (TIndex) - 2);      // we need another index to mark valid elements

		Reset();
	}

	// reset (don't use old handles after calling reset)
	void Reset()
	{
		// build freelist

		/*
		   // front to back
		   TIndex i;
		   for(i=0;i<TSize-1;++i)
		   {
		   m_Buffer[i].m_Salt=0;						//
		   m_Buffer[i].m_NextIndex=i+1;
		   }
		   m_Buffer[i].m_Salt=0;
		   m_Buffer[i].m_NextIndex=(TIndex)-1;			// end marker
		   m_maxUsed=1;
		   m_FreeListStartIndex=1;
		 */

		// back to front
		TIndex i;
		for (i = TSize - 1; i > 1; --i)
		{
			m_Buffer[i].m_Salt = 0;           //
			m_Buffer[i].m_NextIndex = i - 1;
		}
		assert(i == 1);
		m_Buffer[1].m_Salt = 0;
		m_Buffer[1].m_NextIndex = (TIndex) - 1;     // end marker
		m_maxUsed = TSize - 1;
		m_FreeListStartIndex = TSize - 1;

		// 0 is not used because it's nil
		m_Buffer[0].m_Salt = ~0;
		m_Buffer[0].m_NextIndex = (TIndex) - 2;
	}

	// max index that is allowed for TIndex (max entity count at a time)
	static constexpr TIndex GetTSize()
	{
		return TSize;
	}

	//!
	bool IsFull() const
	{
		return m_FreeListStartIndex == (TIndex) - 1;
	}

	// O(n) n=FreeList Size
	// useful for serialization (Reset should be called before inserting the first known element)
	// Arguments:
	//   Handle - must be not nil
	void InsertKnownHandle(const CSaltHandle<TSalt, TIndex>& Handle)
	{
		assert((bool)Handle);   // must be not nil

		if (!IsUsed(Handle.GetIndex()))
			RemoveFromFreeList(Handle.GetIndex());

		if (Handle.GetIndex() > m_maxUsed)
			m_maxUsed = Handle.GetIndex();

		SSaltBufferElement& rElement = m_Buffer[Handle.GetIndex()];

		rElement.m_Salt = Handle.GetSalt();
		rElement.m_NextIndex = (TIndex) - 2;      // mark used
	}

	// O(1) = fast
	// Returns:
	//   nil if there was not enough space, valid SaltHandle otherwise
	CSaltHandle<TSalt, TIndex> InsertDynamic()
	{
		if (m_FreeListStartIndex == (TIndex) - 1)
			return CSaltHandle<TSalt, TIndex>();   // buffer is full

		// update bounds
		if (m_FreeListStartIndex > m_maxUsed)
			m_maxUsed = m_FreeListStartIndex;

		CSaltHandle<TSalt, TIndex> ret(m_Buffer[m_FreeListStartIndex].m_Salt, m_FreeListStartIndex);

		SSaltBufferElement& rElement = m_Buffer[m_FreeListStartIndex];

		m_FreeListStartIndex = rElement.m_NextIndex;
		rElement.m_NextIndex = (TIndex) - 2;  // mark used

		assert(IsUsed(ret.GetIndex()));   // Index was not used, Insert() wasn't called or Remove() called twice

		return ret;
	}

	/*
	   // to support save games compatible with patched levels (patched levels might use more EntityIDs and save game might conflict with dynamic ones)
	   // this function need to called once in the game after the level was loaded
	   // can be called multiple times but runns in O(n) n=TSize
	   void RebuildBackwardFreeList()
	   {
	   TIndex *pPrev = &m_FreeListStartIndex;

	   TIndex i;
	   for(i=TSize-1;i>0;--i)			// skip 0 as it's used for nil
	   {
	    if(m_Buffer[i].m_NextIndex != (TIndex)-2)
	    {
	 * pPrev=i;
	      pPrev = &(m_Buffer[i].m_NextIndex);
	    }
	   }
	 * pPrev = (TIndex)-1;		// end marker
	   }
	 */

	// O(n) = slow
	// Returns:
	//   nil if there was not enough space, valid SaltHandle otherwise
	CSaltHandle<TSalt, TIndex> InsertStatic()
	{
		if (m_FreeListStartIndex == (TIndex) - 1)
			return CSaltHandle<TSalt, TIndex>();   // buffer is full

		// find last available index O(n)
		TIndex LastFreeIndex = m_FreeListStartIndex;
		TIndex* pPrevIndex = &m_FreeListStartIndex;

		for (;; )
		{
			SSaltBufferElement& rCurrElement = m_Buffer[LastFreeIndex];

			if (rCurrElement.m_NextIndex == (TIndex) - 1)
				break;

			pPrevIndex = &(rCurrElement.m_NextIndex);
			LastFreeIndex = rCurrElement.m_NextIndex;
		}

		// remove from end
		*pPrevIndex = (TIndex) - 1;

		// update bounds (actually with introduction of InsertStatic/Dynamic() the m_maxUsed becomes useless)
		if (LastFreeIndex > m_maxUsed)
			m_maxUsed = LastFreeIndex;

		CSaltHandle<TSalt, TIndex> ret(m_Buffer[LastFreeIndex].m_Salt, LastFreeIndex);

		SSaltBufferElement& rElement = m_Buffer[LastFreeIndex];

		rElement.m_NextIndex = (TIndex) - 2;  // mark used

		assert(IsUsed(ret.GetIndex()));   // Index was not used, Insert() wasn't called or Remove() called twice

		return ret;
	}

	// O(1) - don't call for invalid handles and don't remove objects twice
	void Remove(const CSaltHandle<TSalt, TIndex>& Handle)
	{
		assert((bool)Handle);     // must be not nil

		TIndex Index = Handle.GetIndex();

		assert(IsUsed(Index));    // Index was not used, Insert() wasn't called or Remove() called twice

		TSalt& rSalt = m_Buffer[Index].m_Salt;
		TSalt oldSalt = rSalt;

		assert(Handle.GetSalt() == oldSalt);

		rSalt++;

		assert(rSalt > oldSalt);    // if this fails a wraparound occured (thats severe) todo: check in non debug as well
		(void)oldSalt;

		m_Buffer[Index].m_NextIndex = m_FreeListStartIndex;

		m_FreeListStartIndex = Index;
	}

	// for pure debugging purpose
	void Debug()
	{
		if (m_FreeListStartIndex == (TIndex) - 1)
			printf("Debug (max size:%d, no free element): ", TSize);
		else
			printf("Debug (max size:%d, free index: %d): ", TSize, m_FreeListStartIndex);

		for (TIndex i = 0; i < TSize; ++i)
		{
			if (m_Buffer[i].m_NextIndex == (TIndex) - 1)
				printf("%d.%d ", (int)i, (int)m_Buffer[i].m_Salt);
			else if (m_Buffer[i].m_NextIndex == (TIndex) - 2)
				printf("%d.%d* ", (int)i, (int)m_Buffer[i].m_Salt);
			else
				printf("%d.%d->%d ", (int)i, (int)m_Buffer[i].m_Salt, (int)m_Buffer[i].m_NextIndex);
		}

		printf("\n");
	}

	// O(1)
	// Returns:
	//   true=handle is referenceing to a valid object, false=handle is not or referencing to an object that was removed
	bool IsValid(const CSaltHandle<TSalt, TIndex>& rHandle) const
	{
		if (!rHandle)
		{
			assert(!"Invalid Handle, please have caller test and avoid call on invalid handle");
			return false;
		}

		if (rHandle.GetIndex() > TSize)
		{
			assert(0);
			return false;
		}

		return m_Buffer[rHandle.GetIndex()].m_Salt == rHandle.GetSalt();
	}

	// O(1) - useful for iterating the used elements, use together with GetMaxUsed()
	bool IsUsed(const TIndex Index) const
	{
		return m_Buffer[Index].m_NextIndex == (TIndex) - 2;     // is marked used?
	}

	// useful for iterating the used elements, use together with IsUsed()
	TIndex GetMaxUsed() const
	{
		return m_maxUsed;
	}

private: // ----------------------------------------------------------------------------------------

	// O(n) n=FreeList Size
	// Returns:
	//   Index must be part of the FreeList
	void RemoveFromFreeList(const TIndex Index)
	{
		if (m_FreeListStartIndex == Index)     // first index
		{
			m_FreeListStartIndex = m_Buffer[Index].m_NextIndex;
		}
		else                                // not the first index
		{
			TIndex old = m_FreeListStartIndex;
			TIndex it = m_Buffer[old].m_NextIndex;

			for (;; )
			{
				TIndex next = m_Buffer[it].m_NextIndex;

				if (it == Index)
				{
					m_Buffer[old].m_NextIndex = next;
					break;
				}

				assert(next != (TIndex) - 1);         // end index, would mean the element was not in the list

				old = it;
				it = next;
			}
		}

		m_Buffer[Index].m_NextIndex = (TIndex) - 2; // mark used
	}

	// ------------------------------------------------------------------------------------------------

	struct SSaltBufferElement
	{
		TSalt  m_Salt;                                  //!< 0.. is counting up on every remove, should never wrap around
		TIndex m_NextIndex;                             //!< linked list of free or used elements, (TIndex)-1 is used as end marker and for not valid elements, -2 is used for used elements
	};

	SSaltBufferElement m_Buffer[TSize];               //!< freelist and salt buffer elements in one, [0] is not used
	TIndex             m_FreeListStartIndex;          //!< (TIndex)-1 if empty, index in m_Buffer otherwise
	TIndex             m_maxUsed;                     //!< to enable fast iteration through the used elements - is constantly growing execpt when calling Reset()
};
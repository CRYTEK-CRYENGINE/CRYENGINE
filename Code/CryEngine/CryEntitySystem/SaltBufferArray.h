// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SaltHandle.h"           // CSaltHandle

// use one instance of this class in your object manager
// quite efficient implementation, avoids use of pointer to save memory and reduce cache misses
class CSaltBufferArray
{
public:
	static constexpr EntityIndex MaxSize = std::numeric_limits<EntityIndex>::max() - 2;
	static constexpr EntityIndex UsedMarker = MaxSize + 1;
	static constexpr EntityIndex EndMarker = MaxSize + 2;

	// constructor
	CSaltBufferArray()
	{
		Reset();
	}

	// reset (don't use old handles after calling reset)
	void Reset()
	{
		// build freelist

		/*
		   // front to back
		   EntityIndex i;
		   for(i=0;i<MaxSize-1;++i)
		   {
		   m_Buffer[i].m_Salt=0;						//
		   m_Buffer[i].m_NextIndex=i+1;
		   }
		   m_Buffer[i].m_Salt=0;
		   m_Buffer[i].m_NextIndex=EndMarker
		   m_maxUsed=1;
		   m_FreeListStartIndex=1;
		 */

		// back to front
		EntityIndex i;
		for (i = MaxSize - 1; i > 1; --i)
		{
			m_Buffer[i].m_Salt = 0;           //
			m_Buffer[i].m_NextIndex = i - 1;
		}
		assert(i == 1);
		m_Buffer[1].m_Salt = 0;
		m_Buffer[1].m_NextIndex = EndMarker;
		m_maxUsed = MaxSize - 1;
		m_FreeListStartIndex = MaxSize - 1;

		// 0 is not used because it's nil
		m_Buffer[0].m_Salt = ~0;
		m_Buffer[0].m_NextIndex = UsedMarker;
	}

	// max index that is allowed for EntityIndex (max entity count at a time)
	static constexpr EntityIndex GetTSize()
	{
		return MaxSize;
	}

	//!
	bool IsFull() const
	{
		return m_FreeListStartIndex == EndMarker;
	}

	// O(n) n=FreeList Size
	// useful for serialization (Reset should be called before inserting the first known element)
	// Arguments:
	//   Handle - must be not nil
	void InsertKnownHandle(const CSaltHandle& Handle)
	{
		assert((bool)Handle);   // must be not nil

		if (!IsUsed(Handle.GetIndex()))
			RemoveFromFreeList(Handle.GetIndex());

		if (Handle.GetIndex() > m_maxUsed)
			m_maxUsed = Handle.GetIndex();

		SSaltBufferElement& rElement = m_Buffer[Handle.GetIndex()];

		rElement.m_Salt = Handle.GetSalt();
		rElement.m_NextIndex = UsedMarker;      // mark used
	}

	// O(1) = fast
	// Returns:
	//   nil if there was not enough space, valid SaltHandle otherwise
	CSaltHandle InsertDynamic()
	{
		if (m_FreeListStartIndex == EndMarker)
			return CSaltHandle();   // buffer is full

		// update bounds
		if (m_FreeListStartIndex > m_maxUsed)
			m_maxUsed = m_FreeListStartIndex;

		CSaltHandle ret(m_Buffer[m_FreeListStartIndex].m_Salt, m_FreeListStartIndex);

		SSaltBufferElement& rElement = m_Buffer[m_FreeListStartIndex];

		m_FreeListStartIndex = rElement.m_NextIndex;
		rElement.m_NextIndex = UsedMarker;  // mark used

		assert(IsUsed(ret.GetIndex()));   // Index was not used, Insert() wasn't called or Remove() called twice

		return ret;
	}

	// O(n) = slow
	// Returns:
	//   nil if there was not enough space, valid SaltHandle otherwise
	CSaltHandle InsertStatic()
	{
		if (m_FreeListStartIndex == EndMarker)
			return CSaltHandle();   // buffer is full

		// find last available index O(n)
		EntityIndex LastFreeIndex = m_FreeListStartIndex;
		EntityIndex* pPrevIndex = &m_FreeListStartIndex;

		for (;; )
		{
			SSaltBufferElement& rCurrElement = m_Buffer[LastFreeIndex];

			if (rCurrElement.m_NextIndex == EndMarker)
				break;

			pPrevIndex = &(rCurrElement.m_NextIndex);
			LastFreeIndex = rCurrElement.m_NextIndex;
		}

		// remove from end
		*pPrevIndex = EndMarker;

		// update bounds (actually with introduction of InsertStatic/Dynamic() the m_maxUsed becomes useless)
		if (LastFreeIndex > m_maxUsed)
			m_maxUsed = LastFreeIndex;

		CSaltHandle ret(m_Buffer[LastFreeIndex].m_Salt, LastFreeIndex);

		SSaltBufferElement& rElement = m_Buffer[LastFreeIndex];

		rElement.m_NextIndex = UsedMarker;  // mark used

		assert(IsUsed(ret.GetIndex()));   // Index was not used, Insert() wasn't called or Remove() called twice

		return ret;
	}

	// O(1) - don't call for invalid handles and don't remove objects twice
	void Remove(const CSaltHandle& Handle)
	{
		assert((bool)Handle);     // must be not nil

		EntityIndex Index = Handle.GetIndex();

		assert(IsUsed(Index));    // Index was not used, Insert() wasn't called or Remove() called twice

		EntitySalt& rSalt = m_Buffer[Index].m_Salt;
		EntitySalt oldSalt = rSalt;

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
		if (m_FreeListStartIndex == EndMarker)
			printf("Debug (max size:%d, no free element): ", MaxSize);
		else
			printf("Debug (max size:%d, free index: %d): ", MaxSize, m_FreeListStartIndex);

		for (EntityIndex i = 0; i < MaxSize; ++i)
		{
			if (m_Buffer[i].m_NextIndex == EndMarker)
				printf("%d.%d ", (int)i, (int)m_Buffer[i].m_Salt);
			else if (m_Buffer[i].m_NextIndex == UsedMarker)
				printf("%d.%d* ", (int)i, (int)m_Buffer[i].m_Salt);
			else
				printf("%d.%d->%d ", (int)i, (int)m_Buffer[i].m_Salt, (int)m_Buffer[i].m_NextIndex);
		}

		printf("\n");
	}

	// O(1)
	// Returns:
	//   true=handle is referenceing to a valid object, false=handle is not or referencing to an object that was removed
	bool IsValid(const CSaltHandle& rHandle) const
	{
		if (!rHandle)
		{
			assert(!"Invalid Handle, please have caller test and avoid call on invalid handle");
			return false;
		}

		if (rHandle.GetIndex() > MaxSize)
		{
			assert(0);
			return false;
		}

		return m_Buffer[rHandle.GetIndex()].m_Salt == rHandle.GetSalt();
	}

	// O(1) - useful for iterating the used elements, use together with GetMaxUsed()
	bool IsUsed(const EntityIndex Index) const
	{
		return m_Buffer[Index].m_NextIndex == UsedMarker;     // is marked used?
	}

	// useful for iterating the used elements, use together with IsUsed()
	EntityIndex GetMaxUsed() const
	{
		return m_maxUsed;
	}

private: // ----------------------------------------------------------------------------------------

	// O(n) n=FreeList Size
	// Returns:
	//   Index must be part of the FreeList
	void RemoveFromFreeList(const EntityIndex Index)
	{
		if (m_FreeListStartIndex == Index)     // first index
		{
			m_FreeListStartIndex = m_Buffer[Index].m_NextIndex;
		}
		else                                // not the first index
		{
			EntityIndex old = m_FreeListStartIndex;
			EntityIndex it = m_Buffer[old].m_NextIndex;

			for (;; )
			{
				EntityIndex next = m_Buffer[it].m_NextIndex;

				if (it == Index)
				{
					m_Buffer[old].m_NextIndex = next;
					break;
				}

				assert(next != EndMarker);         // end index, would mean the element was not in the list

				old = it;
				it = next;
			}
		}

		m_Buffer[Index].m_NextIndex = UsedMarker; // mark used
	}

	// ------------------------------------------------------------------------------------------------

	struct SSaltBufferElement
	{
		EntitySalt  m_Salt;                                  //!< 0.. is counting up on every remove, should never wrap around
		EntityIndex m_NextIndex;                             //!< linked list of free or used elements, EndMarker is used as end marker and for not valid elements, UsedMarker is used for used elements
	};

	SSaltBufferElement m_Buffer[MaxSize];               //!< freelist and salt buffer elements in one, [0] is not used
	EntityIndex             m_FreeListStartIndex;          //!< EndMarker if empty, index in m_Buffer otherwise
	EntityIndex             m_maxUsed;                     //!< to enable fast iteration through the used elements - is constantly growing execpt when calling Reset()
};

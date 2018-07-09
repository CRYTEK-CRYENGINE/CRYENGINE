// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  A wrapper that counts the number of times the wrapped object
               has been set. This is useful for netserializing an object
               that might be given a "new value" that's the same as the old value.
   -------------------------------------------------------------------------
   History:
   - 16/06/2009 : Created by Alex McCarthy
*************************************************************************/
#ifndef __COUNTED_VALUE_H__
#define __COUNTED_VALUE_H__

template<typename T>
struct CountedValue
{
public:
	CountedValue() : m_lastProducedId(0), m_lastConsumedId(0) {}

	typedef uint32 TCountedID;

	void SetAndDirty(const T& value)
	{
		m_value = value;
		++m_lastProducedId;
		CRY_ASSERT(m_lastProducedId > 0);
	}

	const T* GetLatestValue()
	{
		bool bHasNewValue = IsDirty(); // check for dirtiness before updating ids
		m_lastConsumedId = m_lastProducedId;

		return bHasNewValue ? &m_value : NULL;
	}

	inline bool IsDirty() const
	{
		return m_lastProducedId != m_lastConsumedId;
	}

	const T& Peek() const
	{
		return m_value;
	}

	TCountedID GetLatestID() const
	{
		return m_lastProducedId;
	}

	//! This method should only be used to update the object during serialization!
	void UpdateDuringSerializationOnly(const T& value, TCountedID lastProducedId)
	{
		m_value = value;
		m_lastProducedId = lastProducedId;
	}

private:
	TCountedID m_lastProducedId;
	TCountedID m_lastConsumedId;
	T          m_value;
};

#endif //__COUNTED_VALUE_H__

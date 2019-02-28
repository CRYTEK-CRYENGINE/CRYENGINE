// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// handle can be ==0 nil, or !=0 to store a reference to an object
// this object can be valid or invalid
class CSaltHandle
{
public:
	// default constructor (set to invalid handle)
	CSaltHandle()
	{
		SetNil();
	}

	// constructor
	CSaltHandle(EntitySalt Salt, EntityIndex Index)
		: m_Index(Index), m_Salt(Salt)
	{
	}

	// comparison operator
	bool operator==(const CSaltHandle& rhs) const
	{
		return m_Salt == rhs.m_Salt && m_Index == rhs.m_Index;
	}

	// comparison operator
	bool operator!=(const CSaltHandle& rhs) const
	{
		return !(*this == rhs);
	}

	// conversion to bool
	// e.g. if(id){ ..nil.. } else { ..valid or not valid.. }
	operator bool() const
	{
		return m_Salt != 0 || m_Index != 0;
	}

	// useful for logging/debugging
	// and getting a small index to cache data for an entity
	// (check with GetSalt() if the cache element is still valid)
	EntityIndex GetIndex() const
	{
		return m_Index;
	}

	// used by the SaltBufferArray<>
	EntitySalt GetSalt() const
	{
		return m_Salt;
	}

	EntityId GetId() const { return (((uint32)GetSalt()) << std::numeric_limits<EntitySalt>::digits) | ((uint32)GetIndex()); }
	static CSaltHandle GetHandleFromId(EntityId id) { return CSaltHandle(id >> std::numeric_limits<EntitySalt>::digits, id & std::numeric_limits<EntityIndex>::max()); }

private: // --------------------------------------------------------------

	EntitySalt  m_Salt;                                  // 1.. is counting up on every remove, should never wrap around, 0 means invlid handle
	EntityIndex m_Index;                                 // Index in CSaltBufferArray

	// handle can be nil, or store a reference to an object
	void SetNil()
	{
		m_Salt = 0;
		m_Index = 0;
	}
};

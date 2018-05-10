// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CHANGELIST_H__
#define __CHANGELIST_H__

#pragma once

#include <CryNetwork/ISerialize.h>

template<class T>
class CChangeList
{
public:
	typedef std::vector<std::pair<SNetObjectID, T>> Objects;

	bool AddChange(SNetObjectID id, const T& what)
	{
		bool firstChangeInThisChangeList = false;
		size_t sizeStart = m_changeListLoc.size();
		uint16 slot = id.id;
		if (sizeStart <= slot)
		{
			// resize takes a reference, passing a reference to a static const technically requires an out-of-line definition.
			// gcc/clang linker can trip over the need for an address otherwise.
			const uint16 invalidId = SNetObjectID::InvalidId;
			m_changeListLoc.resize(slot + 1, invalidId);
		}
		uint16& refLoc = m_changeListLoc[slot];
		if (refLoc == SNetObjectID::InvalidId) // object is new to the list?
		{
			refLoc = static_cast<uint16>(m_objects.size());
			m_objects.push_back(std::make_pair(id, what));
			firstChangeInThisChangeList = true;
		}
		else // add more aspects to an existing change
		{
			NET_ASSERT(m_objects.size() > refLoc);
			std::pair<SNetObjectID, T>& obj = m_objects[refLoc];
			NET_ASSERT(obj.first == id);
			obj.second |= what;
		}
		return firstChangeInThisChangeList;
	}

	Objects& GetObjects() { return m_objects; }

	void     Flush()
	{
		// TODO: is it maybe quicker to just memset() here?
		//       or just do a m_changeListLoc.clear()?
		for (typename Objects::const_iterator iter = m_objects.begin(); iter != m_objects.end(); ++iter)
			if (iter->first)
				m_changeListLoc[iter->first.id] = SNetObjectID::InvalidId;
		m_objects.clear();
	}

	void Remove(SNetObjectID id)
	{
		if (m_changeListLoc.size() <= id.id)
			return;
		if (m_changeListLoc[id.id] == SNetObjectID::InvalidId)
			return;
		if (m_objects[m_changeListLoc[id.id]].first.salt != id.salt)
		{
			NET_ASSERT(false);
			return;
		}
		// need to move everything back one element from the location vector
		for (size_t i = m_changeListLoc[id.id] + 1; i != m_objects.size(); ++i)
			m_changeListLoc[m_objects[i].first.id]--;
		// since we're removing an object from mid way through the object array
		m_objects.erase(m_objects.begin() + m_changeListLoc[id.id]);
		m_changeListLoc[id.id] = SNetObjectID::InvalidId;
	}

	bool IsChanged(SNetObjectID id, const T& what)
	{
		if (m_changeListLoc.size() <= id.id)
			return false;
		if (m_changeListLoc[id.id] == SNetObjectID::InvalidId)
			return false;
		if (m_objects[m_changeListLoc[id.id]].first.salt != id.salt)
		{
			NET_ASSERT(false);
			return false;
		}
		return m_objects[m_changeListLoc[id.id]].second & what;
	}

	void GetMemoryStatistics(ICrySizer* pSizer, bool countingThis = false)
	{
		SIZER_COMPONENT_NAME(pSizer, "CChangeList");

		if (countingThis)
			pSizer->Add(*this);
		pSizer->AddContainer(m_changeListLoc);
		pSizer->AddContainer(m_objects);
	}

private:
	// index into Objects of each object-id
	std::vector<uint16> m_changeListLoc;
	// vector of (object-id, aspect-changed-bitmask)
	Objects             m_objects;
};

#endif

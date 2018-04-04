// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   EntityTimeoutList.cpp
//  Version:     v1.00
//  Created:     9/10/2006 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "EntityTimeoutList.h"
#include <CrySystem/ITimer.h>
#include "EntityCVars.h"

// To be as efficient as possible this class uses funky linked list tricks. Instead of having
// head and tail members in the class, the head and tail indices are stored in the first element
// of the vector. The linked list superficially is therefore a circular list, in which the tail
// links back to the head and vice versa. Initially there is only one node which wraps around
// to itself. This setup allows insertion and deletion code to be faster, since there are no
// special cases when the node being inserted/removed is at the beginning/end. Note that this
// zero element is, logically speaking, not part of the list, but is only there as part of the
// linked list mechanics.

CEntityTimeoutList::CEntityTimeoutList(ITimer* pTimer)
	: m_pTimer(pTimer), m_lock(0)
{
	Clear();

	if (!m_pTimer)
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CEntityTimeoutList::CEntityTimeoutList() - pTimer = 0: Not seen timeout will not work, resulting in wasted memory.");
}

void CEntityTimeoutList::ResetTimeout(EntityId id, float timeOffset)
{
	if (id)
	{
		WriteLock lock(m_lock);
		int index = IdToHandle(id).GetIndex() + 1;

		if (int(m_entries.size()) <= index)
		{
			bool wasEmpty = m_entries.empty();

			m_entries.resize(index + 1);

			if (wasEmpty)
			{
				m_entries[0].m_next = 0;
				m_entries[0].m_prev = 0;
			}

			if (CVar::pDebugNotSeenTimeout->GetIVal())
				CryLogAlways("CEntityTimeoutList::ResetTimeout() - resizing entries (total memory used = %d)", int(m_entries.size() * sizeof(CEntry)));
		}

		CEntry& entry = m_entries[index];
		int next = entry.m_next;
		int prev = entry.m_prev;

		if (next == -1 && CVar::pDebugNotSeenTimeout->GetIVal())
			CryLogAlways("EntityId (0x%X) added to not seen timeout list", id);

		if (next >= 0)
			m_entries[next].m_prev = prev;
		if (prev >= 0)
			m_entries[prev].m_next = next;

		float time = m_pTimer ? m_pTimer->GetCurrTime() : 0.0f;
		int indexPrev = m_entries[0].m_prev;
		if (timeOffset || entry.m_timeOffset)
		{
			if (timeOffset)
				entry.m_timeOffset = timeOffset;
			time += entry.m_timeOffset;
			for (; indexPrev > 0 && m_entries[indexPrev].m_time > time; indexPrev = m_entries[indexPrev].m_prev)
				;
		}

		entry.m_next = m_entries[indexPrev].m_next;
		entry.m_prev = indexPrev;
		entry.m_id = id;
		entry.m_time = time;

		m_entries[m_entries[indexPrev].m_next].m_prev = index;
		m_entries[indexPrev].m_next = index;
	}
}

EntityId CEntityTimeoutList::PopTimeoutEntity(float timeout)
{
	WriteLock lock(m_lock);
	if (m_entries.empty() || m_entries[0].m_next == 0)
		return 0;

	float time = (m_pTimer ? m_pTimer->GetCurrTime() : 0.0f);
	int index = m_entries[0].m_next;
	if (m_entries[index].m_time + timeout > time)
		return 0;

	int next = m_entries[index].m_next;
	int prev = m_entries[index].m_prev;
	if (next >= 0)
		m_entries[next].m_prev = prev;
	if (prev >= 0)
		m_entries[prev].m_next = next;
	m_entries[index].m_next = -1;
	m_entries[index].m_prev = -1;
	m_entries[index].m_timeOffset = 0.0f;

	return m_entries[index].m_id;
}

void CEntityTimeoutList::Clear()
{
	stl::free_container(m_entries);
}

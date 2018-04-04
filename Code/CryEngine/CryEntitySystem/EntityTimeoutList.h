// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   EntityTimeoutList.h
//  Version:     v1.00
//  Created:     9/10/2006 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __ENTITYTIMEOUTLIST_H__
#define __ENTITYTIMEOUTLIST_H__

#include <vector>
#include "SaltHandle.h"

typedef unsigned int EntityId; // Copied from IEntity.h

class CEntityTimeoutList
{
public:
	CEntityTimeoutList(ITimer* pTimer);

	void     ResetTimeout(EntityId id, float customTimeout = 0.0f);
	EntityId PopTimeoutEntity(float timeout);
	void     Clear();

private:
	static ILINE CSaltHandle<> IdToHandle(const EntityId id) { return CSaltHandle<>(id >> 16, id & 0xffff); }

	class CEntry
	{
	public:
		CEntry() : m_id(0), m_time(0.0f), m_timeOffset(0.0f), m_next(-1), m_prev(-1) {}

		EntityId m_id;
		float    m_time;
		float    m_timeOffset;
		int      m_next;
		int      m_prev;
	};

	typedef DynArray<CEntry> EntryContainer;
	EntryContainer m_entries;
	volatile int   m_lock;
	ITimer*        m_pTimer;
};

#endif //__ENTITYTIMEOUTLIST_H__

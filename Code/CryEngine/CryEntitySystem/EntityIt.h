// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EntitySystem.h"
#include "Entity.h"

class CEntity;
struct IEntity;

class CEntityItMap final : public IEntityIt
{
public:
	CEntityItMap()
	{
		m_nRefCount = 0;
		MoveFirst();
	}
	virtual ~CEntityItMap() {}

	virtual bool IsEnd() override
	{
		uint32 dwMaxUsed = (uint32)g_pIEntitySystem->m_EntitySaltBuffer.GetMaxUsed();

		// jump over gaps
		while (m_id <= (int)dwMaxUsed)
		{
			if (g_pIEntitySystem->m_EntityArray[m_id] != 0)
				return false;

			++m_id;
		}

		return m_id > (int)dwMaxUsed; // we passed the last element
	}

	virtual IEntity* This() override      { return !IsEnd() ? g_pIEntitySystem->m_EntityArray[m_id] : nullptr; }
	virtual IEntity* Next() override      { return !IsEnd() ? g_pIEntitySystem->m_EntityArray[m_id++] : nullptr; }
	virtual void     MoveFirst() override { m_id = 0; };
	virtual void     AddRef() override    { m_nRefCount++; }
	virtual void     Release() override   { --m_nRefCount; if (m_nRefCount <= 0) { delete this; } }

protected: // ---------------------------------------------------

	int m_nRefCount;                          //
	int m_id;                                 //
};

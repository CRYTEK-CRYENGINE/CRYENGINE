// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#if !defined(AFX_ENTITYIT_H__95AE38A4_7F15_4069_A97A_2F3A1F06F670__INCLUDED_)
#define AFX_ENTITYIT_H__95AE38A4_7F15_4069_A97A_2F3A1F06F670__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include "EntitySystem.h"

class CEntity;
struct IEntity;

class CEntityItMap : public IEntityIt
{
public:
	CEntityItMap(CEntitySystem* pEntitySystem) : m_pEntitySystem(pEntitySystem)
	{
		assert(pEntitySystem);
		m_nRefCount = 0;
		MoveFirst();
	}

	bool IsEnd()
	{
		uint32 dwMaxUsed = (uint32)m_pEntitySystem->m_EntitySaltBuffer.GetMaxUsed();

		// jump over gaps
		while (m_id <= (int)dwMaxUsed)
		{
			if (m_pEntitySystem->m_EntityArray[m_id] != 0)
				return false;

			++m_id;
		}

		return m_id > (int)dwMaxUsed; // we passed the last element
	}

	IEntity* This()
	{
		if (IsEnd())   // might advance m_id
			return 0;
		else
			return (IEntity*)m_pEntitySystem->m_EntityArray[m_id];
	}

	IEntity* Next()
	{
		if (IsEnd())   // might advance m_id
			return 0;
		else
			return (IEntity*)m_pEntitySystem->m_EntityArray[m_id++];
	}

	void MoveFirst() { m_id = 0; };

	void AddRef()    { m_nRefCount++; }

	void Release()   { --m_nRefCount; if (m_nRefCount <= 0) { delete this; } }

protected: // ---------------------------------------------------

	CEntitySystem* m_pEntitySystem;           //
	int            m_nRefCount;               //
	int            m_id;                      //
};

#endif // !defined(AFX_ENTITYIT_H__95AE38A4_7F15_4069_A97A_2F3A1F06F670__INCLUDED_)

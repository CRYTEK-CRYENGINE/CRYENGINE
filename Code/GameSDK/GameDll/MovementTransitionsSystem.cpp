// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: 
Holds a CMovementTransitions object for each entity class
-------------------------------------------------------------------------
History:
- 4:29:2010	19:48 : Created by Sven Van Soom
*************************************************************************/
#include "StdAfx.h"
#include "Player.h"
#include "MovementTransitions.h"
#include "MovementTransitionsSystem.h"

///////////////////////////////////////////////////////////////////////////////
CMovementTransitionsSystem::CMovementTransitionsSystem()
{
}


///////////////////////////////////////////////////////////////////////////////
CMovementTransitionsSystem::~CMovementTransitionsSystem()
{
	MovementTransitionsVector::iterator iEnd = m_entityTransitions.end();
	for(MovementTransitionsVector::iterator i = m_entityTransitions.begin(); i != iEnd; ++i)
	{
		delete (*i);
	}
}

///////////////////////////////////////////////////////////////////////////////
void CMovementTransitionsSystem::Flush()
{
	MovementTransitionsVector::iterator iEnd = m_entityTransitions.end();
	for(MovementTransitionsVector::iterator i = m_entityTransitions.begin(); i != iEnd; ++i)
	{
		delete (*i);
	}
	stl::free_container(m_entityTransitions);
}

///////////////////////////////////////////////////////////////////////////////
void CMovementTransitionsSystem::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));

	MovementTransitionsVector::const_iterator iEnd = m_entityTransitions.end();
	for(MovementTransitionsVector::const_iterator i = m_entityTransitions.begin(); i != iEnd; ++i)
	{
		(*i)->GetMemoryUsage(s);
	}
}


///////////////////////////////////////////////////////////////////////////////
void CMovementTransitionsSystem::Reload() const
{
	MovementTransitionsVector::const_iterator iEnd = m_entityTransitions.end();
	for(MovementTransitionsVector::const_iterator i = m_entityTransitions.begin(); i != iEnd; ++i)
	{
		(*i)->Load();
	}
}


///////////////////////////////////////////////////////////////////////////////
CMovementTransitions*const CMovementTransitionsSystem::GetMovementTransitions(IEntity* pEntity)
{
	return GetMovementTransitions(pEntity->GetClass(), pEntity->GetScriptTable());
}


///////////////////////////////////////////////////////////////////////////////
CMovementTransitions*const CMovementTransitionsSystem::GetMovementTransitions(IEntityClass* pEntityClass, SmartScriptTable pEntityScript)
{
	// Check for useSpecialMovementTransitions property in LUA
	if(!pEntityScript)
		return NULL;

	SmartScriptTable pPropertiesTable;
	pEntityScript->GetValue("Properties", pPropertiesTable);
	if (!pPropertiesTable)
		return NULL;

	int useMovementTransitions = 0;
	if(!pPropertiesTable->GetValue("useSpecialMovementTransitions", useMovementTransitions) || !useMovementTransitions)
		return NULL;

	// Look for cached movement transitions
	MovementTransitionsVector::iterator iEnd = m_entityTransitions.end();
	for(MovementTransitionsVector::iterator i = m_entityTransitions.begin(); i != iEnd; ++i)
	{
		if ((*i)->GetEntityClass() == pEntityClass)
		{
			return (*i);
		}
	}

	// Create new movement transition
	CMovementTransitions* pTransitions = new CMovementTransitions(pEntityClass);
	CRY_ASSERT(pTransitions);
	if (pTransitions)
		m_entityTransitions.push_back(pTransitions);
	return pTransitions;
}


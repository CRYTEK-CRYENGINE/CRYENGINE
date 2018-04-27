// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: 
	Holds a CMovementTransitions object for each entity class
-------------------------------------------------------------------------
History:
- 4:29:2010	19:48 : Created by Sven Van Soom
*************************************************************************/
#pragma once
#ifndef __MOVEMENT_TRANSITIONS_SYSTEM_H
#define __MOVEMENT_TRANSITIONS_SYSTEM_H

class CMovementTransitions;
class CMovementTransitionsSystem
{
public:
	// public methods
	CMovementTransitionsSystem();
	~CMovementTransitionsSystem();

	CMovementTransitions*const GetMovementTransitions(IEntity* pEntity);
	CMovementTransitions*const GetMovementTransitions(IEntityClass* pEntityClass, SmartScriptTable pEntityScript);

	void GetMemoryUsage(ICrySizer* s) const;
	void Reload() const;
	void Flush();

private:
	// private types
	typedef std::vector<CMovementTransitions*> MovementTransitionsVector;

	// private methods
	CMovementTransitionsSystem(const CMovementTransitionsSystem&) {}
	CMovementTransitionsSystem& operator=(const CMovementTransitionsSystem&) { return *this; }

	// private fields
	MovementTransitionsVector m_entityTransitions;
};

#endif // __MOVEMENT_TRANSITIONS_SYSTEM_H
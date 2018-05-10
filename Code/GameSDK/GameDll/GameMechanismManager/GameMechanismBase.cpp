// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameMechanismBase.h"
#include "GameMechanismManager.h"

CGameMechanismBase::CGameMechanismBase(const char * className)
{
	memset (& m_linkedListPointers, 0, sizeof(m_linkedListPointers));

#if INCLUDE_NAME_IN_GAME_MECHANISMS
	m_className = className;
#endif

	CGameMechanismManager * manager = CGameMechanismManager::GetInstance();
	manager->RegisterMechanism(this);
}

CGameMechanismBase::~CGameMechanismBase()
{
	CGameMechanismManager * manager = CGameMechanismManager::GetInstance();
	manager->UnregisterMechanism(this);
}

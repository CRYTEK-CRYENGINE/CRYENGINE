// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GAMEMECHANISMMANAGER_H__
#define __GAMEMECHANISMMANAGER_H__

#include "GameMechanismEvents.h"

class CGameMechanismBase;

class CGameMechanismManager
{
	public:
	CGameMechanismManager();
	~CGameMechanismManager();
	void Update(float dt);
	void Inform(EGameMechanismEvent gmEvent, const SGameMechanismEventData * data = NULL);
	void RegisterMechanism(CGameMechanismBase * mechanism);
	void UnregisterMechanism(CGameMechanismBase * mechanism);

	static ILINE CGameMechanismManager * GetInstance()
	{
		assert (s_instance);
		return s_instance;
	}

	private:
	static CGameMechanismManager * s_instance;
	CGameMechanismBase * m_firstMechanism;

#if !defined(_RELEASE)
	int m_cvarWatchEnabled;
	int m_cvarLogEnabled;
#endif
};

#endif //__GAMEMECHANISMMANAGER_H__
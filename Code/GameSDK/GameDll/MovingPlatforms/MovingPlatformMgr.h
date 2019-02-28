// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GAME_MOVING_PLATFORM_MGR_H__
#define __GAME_MOVING_PLATFORM_MGR_H__ 

#include "GameRules.h"

class CMovingPlatformMgr
{
	struct SContact
	{
		SContact()
			: fTimeSinceFirstContact(0.f)
			, fTimeSinceLastContact(0.f)
		{}
		ILINE void Update(const float dt) { fTimeSinceFirstContact += dt; fTimeSinceLastContact += dt; }
		ILINE void Refresh() { fTimeSinceLastContact = 0.f; }
		float fTimeSinceFirstContact;
		float fTimeSinceLastContact;
	};

public:
	CMovingPlatformMgr();
	~CMovingPlatformMgr();

	int OnCollisionLogged( const EventPhys* pEvent );
	int OnDeletedLogged( const EventPhys* pEvent );
	void Update( const float dt );
	void Reset();

	/* Static Physics Event Receivers */
	static int StaticOnCollision( const EventPhys * pEvent );
	static int StaticOnDeleted( const EventPhys * pEvent );

protected:
	typedef std::map<EntityId, SContact> TContactList;
	TContactList m_contacts;
};

#endif

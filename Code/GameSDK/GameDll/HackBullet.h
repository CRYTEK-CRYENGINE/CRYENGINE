// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _HACK_BULLET_H_
#define _HACK_BULLET_H_

#include "Projectile.h"



class CHackBullet : public CProjectile
{
private:
	virtual void HandleEvent(const SGameObjectEvent& event);
};



#endif

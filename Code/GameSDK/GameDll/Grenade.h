// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Grenades

-------------------------------------------------------------------------
History:
- 11:12:2009   10:30 : Created by Claire Allan

*************************************************************************/
#ifndef __GRENADE_H__
#define __GRENADE_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Projectile.h"

class CGrenade : public CProjectile
{
private:
	typedef CProjectile BaseClass;

public:
	CGrenade();
	virtual ~CGrenade();

	virtual void Explode(const CProjectile::SExplodeDesc& explodeDesc);
	virtual void Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale /*=1.0f*/);
	virtual void HandleEvent(const SGameObjectEvent &event);

protected:
	virtual bool ShouldCollisionsDamageTarget() const;
	virtual void ProcessEvent(const SEntityEvent& event);

	bool m_detonationFailed;
};

class CSmokeGrenade : public CGrenade
{
private:
	typedef CGrenade inherited;

public:
	virtual void Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale /*=1.0f*/);
};

#endif

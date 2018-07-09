// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Rock

-------------------------------------------------------------------------
History:
- 5:5:2006   15:24 : Created by Márcio Martins

*************************************************************************/
#ifndef __CHAFF_H__
#define __CHAFF_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Projectile.h"
#include <CryCore/Containers/VectorSet.h>

#define MAX_SPAWNED_CHAFFS	16

class CChaff : public CProjectile
{
public:
	CChaff();
	virtual ~CChaff();

	virtual void HandleEvent(const SGameObjectEvent &);

	virtual Vec3 GetPosition(void);

	static VectorSet<CChaff*> s_chaffs;
};


#endif // __CHAFF_H__
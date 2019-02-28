// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description:  Item/weapon accessories

-------------------------------------------------------------------------
History:
- 28:1:2008   Created by Benito G.R. 

*************************************************************************/

#ifndef _ACCESSORY_H_
#define _ACCESSORY_H_

#include "Item.h"

class CAccessory: public CItem
{
	typedef CItem BaseClass;

public:

	CAccessory() {};
	virtual			~CAccessory() {};
	
	virtual void PickUp(EntityId pickerId, bool sound, bool select/* =true */, bool keepHistory/* =true */, const char *setup = NULL);
	virtual bool IsAccessory() { return true; };

	virtual void OnEnterFirstPerson();
	virtual void OnEnterThirdPerson();

	virtual void SetParentId(EntityId parentId);
	virtual void Physicalize(bool enable, bool rigid);
	virtual ePhysicalization	FindPhysicalisationType(bool enable, bool rigid);

	void GetMemoryUsage(ICrySizer *pSizer )const
	{
		pSizer->AddObject(this, sizeof(*this));
		CItem::GetInternalMemoryUsage(pSizer); // collect memory of parent class
	}

};

#endif

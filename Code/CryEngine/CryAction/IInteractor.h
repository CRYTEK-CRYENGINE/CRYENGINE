// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Interactor interface.

   -------------------------------------------------------------------------
   History:
   - 26:6:2006   17:06 : Created by Márcio Martins

*************************************************************************/
#ifndef __IINTERACTOR_H__
#define __IINTERACTOR_H__

#pragma once

struct IInteractor : public IGameObjectExtension
{
	virtual bool IsUsable(EntityId entityId) const = 0;
	virtual bool IsLocked() const = 0;
	virtual int  GetLockIdx() const = 0;
	virtual int  GetLockedEntityId() const = 0;
	virtual void SetQueryMethods(char* pMethods) = 0;
	virtual int  GetOverEntityId() const = 0;
	virtual int  GetOverSlotIdx() const = 0;
};

#endif // __IINTERACTOR_H__

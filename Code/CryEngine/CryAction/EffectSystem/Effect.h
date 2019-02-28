// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	Base class for effects managed by the effect system

   -------------------------------------------------------------------------
   History:
   - 17:01:2006:		Created by Marco Koegler

*************************************************************************/
#ifndef __EFFECT_H__
#define __EFFECT_H__
#pragma once

#include "../IEffectSystem.h"

class CEffect : public IEffect
{
public:
	// IEffect
	virtual bool         Activating(float delta);
	virtual bool         Update(float delta);
	virtual bool         Deactivating(float delta);
	virtual bool         OnActivate();
	virtual bool         OnDeactivate();
	virtual void         SetState(EEffectState state);
	virtual EEffectState GetState();
	// ~IEffect
private:
	EEffectState m_state;
};

#endif //__EFFECT_H__

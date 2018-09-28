// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	Blood splat effect

   -------------------------------------------------------------------------
   History:
   - 17:01:2006:		Created by Marco Koegler

*************************************************************************/
#ifndef __BLOODSPLATS_H__
#define __BLOODSPLATS_H__

#include "Effect.h"

class CBloodSplats : public CEffect
{
public:

	void         Init(int type, const CTimeValue& maxTime);
	// IEffect overrides
	virtual bool Update(const CTimeValue& delta);
	virtual bool OnActivate();
	virtual bool OnDeactivate();
	virtual void GetMemoryUsage(ICrySizer* s) const;
	// ~IEffect overrides

private:
	int   m_type;           // 0 human, 1 alien
	CTimeValue m_maxTime;        // maximum time until effect expires
	CTimeValue m_currentTime;    // current time
};

#endif //__BLOODSPLATS_H__

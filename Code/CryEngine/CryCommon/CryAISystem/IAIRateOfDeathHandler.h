// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Interface to apply modification to an AI's handling of the
   Rate of Death balancing system

   -------------------------------------------------------------------------
   History:
   - 07:09:2009: Created by Kevin Kirst

*************************************************************************/

#ifndef __IAI_RATEOFDEATH_HANDLER_H__
#define __IAI_RATEOFDEATH_HANDLER_H__

#include <CryAISystem/IAgent.h> // <> required for Interfuscator

struct IAIRateOfDeathHandler
{
	// <interfuscator:shuffle>
	virtual ~IAIRateOfDeathHandler() {}

	//! Calculate and return how long the target should stay alive from now.
	virtual float GetTargetAliveTime(const IAIObject* pAI, const IAIObject* pTarget, EAITargetZone eTargetZone, float& fFireDazzleTime) = 0;

	//! Calculate and return how long the AI should take to react to firing at the target from now.
	virtual float GetFiringReactionTime(const IAIObject* pAI, const IAIObject* pTarget, const Vec3& vTargetPos) = 0;

	//! Calculate and return the zone the target is currently in.
	virtual EAITargetZone GetTargetZone(const IAIObject* pAI, const IAIObject* pTarget) = 0;
	// </interfuscator:shuffle>

	void GetMemoryUsage(ICrySizer* pSizer) const { /*LATER*/ }
};

#endif //__IAI_RATEOFDEATH_HANDLER_H__

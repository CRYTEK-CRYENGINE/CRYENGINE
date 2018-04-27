// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// --------------------------------------------------------------------------------------
//  File name:   Adapters.h
//  Created:     02/02/2009 by Matthew
//  Description: Implements adapters for AI objects from external interfaces to internal
//               This is purely a translation layer without concrete instances
//               They can have no state and must remain abstract
//
//               * Note that this is fast becoming redundant! Mainly shells now *
//
// --------------------------------------------------------------------------------------
//  History:
//
/////////////////////////////////////////////////////////////////////////////////////////

#ifndef __AI_ADAPTERS_H_
#define __AI_ADAPTERS_H_

#pragma once

#include <CryAISystem/IAgent.h>
#include "AIObject.h"
#include "ObjectContainer.h"

#include <CryAISystem/IAIGroup.h>

CWeakRef<CAIObject> GetWeakRefSafe(IAIObject* pObj);

class CPipeUserAdapter
	: public IPipeUser
{
public:
	virtual ~CPipeUserAdapter() {}

	virtual bool       SelectPipe(int id, const char* name, CWeakRef<CAIObject> refArgument, int goalPipeId = 0, bool resetAlways = false, const GoalParams* node = 0) = 0;
	virtual IGoalPipe* InsertSubPipe(int id, const char* name, CWeakRef<CAIObject> refArgument = NILREF, int goalPipeId = 0, const GoalParams* node = 0) = 0;

private:
	bool       SelectPipe(int id, const char* name, IAIObject* pArgument = 0, int goalPipeId = 0, bool resetAlways = false, const GoalParams* node = 0)
	{ return SelectPipe(id, name, GetWeakRefSafe(pArgument), goalPipeId, resetAlways, node); }
	IGoalPipe* InsertSubPipe(int id, const char* name, IAIObject* pArgument = 0, int goalPipeId = 0, const GoalParams* node = 0)
	{ return InsertSubPipe(id, name, GetWeakRefSafe(pArgument), goalPipeId, node); }

};

class CAIGroupAdapter
	: public IAIGroup
{
public:
	// cppcheck-suppress passedByValue
	virtual CWeakRef<CAIObject> GetAttentionTarget(bool bHostileOnly = false, bool bLiveOnly = false, const CWeakRef<CAIObject> refSkipTarget = NILREF) const = 0;

private:
	IAIObject* GetAttentionTarget(bool bHostileOnly = false, bool bLiveOnly = false) const;
};

#endif //__AI_ADAPTERS_H_

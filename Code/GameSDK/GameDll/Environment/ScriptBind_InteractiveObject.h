// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Script bind functions forCrysis2 interactive object

-------------------------------------------------------------------------
History:
- 14:12:2009: Created by Benito G.R.

*************************************************************************/

#pragma once

#ifndef _SCRIPTBIND_INTERACTIVE_OBJECT_H_
#define _SCRIPTBIND_INTERACTIVE_OBJECT_H_

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>

#include "InteractiveObjectRegistry.h"


class CInteractiveObjectEx;

class CScriptBind_InteractiveObject :
	public CScriptableBase
{

private:

	typedef std::map<EntityId, CInteractiveObjectEx*> TInteractiveObjectsMap;

public:
	CScriptBind_InteractiveObject(ISystem *pSystem, IGameFramework *pGameFramework);
	virtual ~CScriptBind_InteractiveObject();

	virtual void GetMemoryUsage(ICrySizer *pSizer) const;
	

	void AttachTo(CInteractiveObjectEx *pInteractiveObject);
	void Detach(EntityId entityId);

	int CanUse(IFunctionHandler *pH, ScriptHandle userId);
	int Use(IFunctionHandler *pH, ScriptHandle userId);
	int StopUse(IFunctionHandler *pH, ScriptHandle userId);
	int AbortUse(IFunctionHandler *pH);

	ILINE CInteractiveObjectRegistry& GetObjectDataRegistry() { return m_objectDataRegistry; }

private:
	void RegisterMethods();

	CInteractiveObjectEx *GetInteractiveObject(IFunctionHandler *pH);

	ISystem					*m_pSystem;
	IGameFramework	*m_pGameFrameWork;

	TInteractiveObjectsMap m_interactiveObjectsMap;
	CInteractiveObjectRegistry m_objectDataRegistry;
};

#endif
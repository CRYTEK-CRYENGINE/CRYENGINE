// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Script bind functions for Crysis2 interactive object

-------------------------------------------------------------------------
History:
- 14:12:2009: Created by Benito G.R.

*************************************************************************/

#include "StdAfx.h"
#include "InteractiveObject.h"
#include "ScriptBind_InteractiveObject.h"


CScriptBind_InteractiveObject::CScriptBind_InteractiveObject( ISystem *pSystem, IGameFramework *pGameFramework )
: m_pSystem(pSystem)
, m_pGameFrameWork(pGameFramework)
{
	Init(pSystem->GetIScriptSystem(), m_pSystem, 1);

	RegisterMethods();
}

CScriptBind_InteractiveObject::~CScriptBind_InteractiveObject()
{

}

void CScriptBind_InteractiveObject::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_InteractiveObject::

	SCRIPT_REG_TEMPLFUNC(CanUse, "userId");
	SCRIPT_REG_TEMPLFUNC(Use, "userId");
	SCRIPT_REG_TEMPLFUNC(StopUse, "userId");
	SCRIPT_REG_TEMPLFUNC(AbortUse, "");
}

void CScriptBind_InteractiveObject::AttachTo( CInteractiveObjectEx *pInteractiveObject )
{
	IScriptTable *pScriptTable = pInteractiveObject->GetEntity()->GetScriptTable();

	if (pScriptTable)
	{
		SmartScriptTable thisTable(m_pSS);

		thisTable->SetValue("__this", ScriptHandle(pInteractiveObject->GetEntityId()));
		thisTable->Delegate(GetMethodsTable());

		pScriptTable->SetValue("interactiveObject", thisTable);
	}

	m_interactiveObjectsMap.insert(TInteractiveObjectsMap::value_type(pInteractiveObject->GetEntityId(), pInteractiveObject));
}

void CScriptBind_InteractiveObject::Detach( EntityId entityId )
{
	m_interactiveObjectsMap.erase(entityId);
}

CInteractiveObjectEx * CScriptBind_InteractiveObject::GetInteractiveObject( IFunctionHandler *pH )
{
	void* pThis = pH->GetThis();

	if (pThis)
	{
		const EntityId objectId = (EntityId)(UINT_PTR)pThis;
		TInteractiveObjectsMap::const_iterator cit = m_interactiveObjectsMap.find(objectId);
		if (cit != m_interactiveObjectsMap.end())
		{
			return cit->second;
		}
	}

	return NULL;
}

int CScriptBind_InteractiveObject::CanUse( IFunctionHandler *pH, ScriptHandle userId )
{
	CInteractiveObjectEx *pInteractiveObject = GetInteractiveObject(pH);
	if (pInteractiveObject)
	{
		return pH->EndFunction(pInteractiveObject->CanUse((EntityId)userId.n));
	}
	
	return pH->EndFunction();
}

int CScriptBind_InteractiveObject::Use( IFunctionHandler *pH, ScriptHandle userId )
{
	CInteractiveObjectEx *pInteractiveObject = GetInteractiveObject(pH);
	if (pInteractiveObject)
	{
		pInteractiveObject->Use((EntityId)userId.n);
	}

	return pH->EndFunction();
}

int CScriptBind_InteractiveObject::StopUse( IFunctionHandler *pH, ScriptHandle userId )
{
	CInteractiveObjectEx *pInteractiveObject = GetInteractiveObject(pH);
	if (pInteractiveObject)
	{
		pInteractiveObject->StopUse((EntityId)userId.n);
	}

	return pH->EndFunction();
}

int CScriptBind_InteractiveObject::AbortUse( IFunctionHandler *pH )
{
	CInteractiveObjectEx *pInteractiveObject = GetInteractiveObject(pH);
	if (pInteractiveObject)
	{
		pInteractiveObject->AbortUse();
	}

	return pH->EndFunction();
}

void CScriptBind_InteractiveObject::GetMemoryUsage(ICrySizer *pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddContainer(m_interactiveObjectsMap);
	pSizer->AddObject(m_objectDataRegistry);	
}
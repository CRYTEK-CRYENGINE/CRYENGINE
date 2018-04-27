// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 27:10:2004   11:29 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ScriptBind_Weapon.h"
#include "Item.h"
#include "Weapon.h"
#include "IGameObject.h"
#include "Actor.h"


#define REUSE_VECTOR(table, name, value)	\
	{ if (table->GetValueType(name) != svtObject) \
	{ \
	table->SetValue(name, (value)); \
	} \
		else \
	{ \
	SmartScriptTable v; \
	table->GetValue(name, v); \
	v->SetValue("x", (value).x); \
	v->SetValue("y", (value).y); \
	v->SetValue("z", (value).z); \
	} \
	}


//------------------------------------------------------------------------
CScriptBind_Weapon::CScriptBind_Weapon(ISystem *pSystem, IGameFramework *pGameFramework)
: m_pSystem(pSystem),
	m_pGameFW(pGameFramework)
{
	Init(pSystem->GetIScriptSystem(), m_pSystem, 1);

	RegisterMethods();
	RegisterGlobals();
}

//------------------------------------------------------------------------
CScriptBind_Weapon::~CScriptBind_Weapon()
{
}

//------------------------------------------------------------------------
void CScriptBind_Weapon::AttachTo(CWeapon *pWeapon)
{
	IScriptTable *pScriptTable = ((CItem *)pWeapon)->GetEntity()->GetScriptTable();

	if (pScriptTable)
	{
		SmartScriptTable thisTable(m_pSS);

		thisTable->SetValue("__this", ScriptHandle(pWeapon->GetEntityId()));
		thisTable->Delegate(GetMethodsTable());

		pScriptTable->SetValue("weapon", thisTable);
	}
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::SetAmmoCount(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());

	if (pFireMode)
	{
		if (pH->GetParamType(2) != svtNumber)
			return pH->EndFunction();

		const char *ammoName = 0;
		if (pH->GetParamType(1) == svtString)
			pH->GetParam(1, ammoName);

		IEntityClass* pAmmoType = pFireMode->GetAmmoType();

		if (ammoName)
			pAmmoType = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammoName);

		int ammo = 0;
		pH->GetParam(2, ammo);

		pWeapon->SetAmmoCount(pAmmoType, ammo);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::GetAmmoCount(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
	
	if (pFireMode)
		return pH->EndFunction(pFireMode->GetAmmoCount());

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::GetClipSize(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());

	if (pFireMode)
		return pH->EndFunction(pFireMode->GetClipSize());

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::GetAmmoType(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());

	if (pFireMode)
		if (IEntityClass * pCls = pFireMode->GetAmmoType())
			return pH->EndFunction(pCls->GetName());

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::SetCurrentFireMode(IFunctionHandler *pH, const char *name)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	pWeapon->SetCurrentFireMode(name);

	return pH->EndFunction();
}


int CScriptBind_Weapon::SupportsAccessory(IFunctionHandler *pH, const char *accessoryName)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(accessoryName);
	const SAccessoryParams *params = pClass ? pWeapon->GetAccessoryParams(pClass) : NULL;
	return pH->EndFunction(params != 0);
}

int CScriptBind_Weapon::GetAccessory(IFunctionHandler *pH, const char *accessoryName)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	CItem *pItem = pWeapon->GetAccessory(accessoryName);					
	
	if(!pItem)
		return 0;

	IEntity *pEntity  = pItem->GetEntity();

	if(!pEntity)
		return 0;
	
	IScriptTable *pScriptTable = pEntity->GetScriptTable();

	return pH->EndFunction( pScriptTable );
}

int CScriptBind_Weapon::AttachAccessory(IFunctionHandler *pH, const char *className, bool attach, bool force)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	if (className)
		pWeapon->AttachAccessory(className, attach, true, force);

	return pH->EndFunction();
}

int CScriptBind_Weapon::SwitchAccessory(IFunctionHandler *pH, const char *className)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	if (className)
		pWeapon->SwitchAccessory(className);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::Reload(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (pWeapon)
		pWeapon->Reload();

	return pH->EndFunction();
}


//------------------------------------------------------------------------
void CScriptBind_Weapon::RegisterGlobals()
{
}

//------------------------------------------------------------------------
void CScriptBind_Weapon::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_Weapon::

	SCRIPT_REG_TEMPLFUNC(SetAmmoCount, "")
	SCRIPT_REG_TEMPLFUNC(GetAmmoCount, "")
	SCRIPT_REG_TEMPLFUNC(GetClipSize, "")
	SCRIPT_REG_TEMPLFUNC(GetAmmoType, "")

	SCRIPT_REG_TEMPLFUNC(SupportsAccessory, "accessoryName");
	SCRIPT_REG_TEMPLFUNC(GetAccessory, "accessoryName");
	SCRIPT_REG_TEMPLFUNC(AttachAccessory, "accessoryName, attach, force");
	SCRIPT_REG_TEMPLFUNC(SwitchAccessory, "accessoryName");

	SCRIPT_REG_TEMPLFUNC(SetCurrentFireMode, "name")

	SCRIPT_REG_TEMPLFUNC(Reload, "")

}

//------------------------------------------------------------------------
CItem *CScriptBind_Weapon::GetItem(IFunctionHandler *pH)
{
	void *pThis = pH->GetThis();

	if (pThis)
	{
		IItem *pItem = m_pGameFW->GetIItemSystem()->GetItem((EntityId)(UINT_PTR)pThis);
		if (pItem)
			return static_cast<CItem *>(pItem);
	}

	return 0;
}

//------------------------------------------------------------------------
CWeapon *CScriptBind_Weapon::GetWeapon(IFunctionHandler *pH)
{
	void *pThis = pH->GetThis();

	if (pThis)
	{
		IItem *pItem = m_pGameFW->GetIItemSystem()->GetItem((EntityId)(UINT_PTR)pThis);
		if (pItem)
		{
			IWeapon *pWeapon=pItem->GetIWeapon();
			if (pWeapon)
				return static_cast<CWeapon *>(pWeapon);
		}
	}

	return 0;
}

#undef REUSE_VECTOR
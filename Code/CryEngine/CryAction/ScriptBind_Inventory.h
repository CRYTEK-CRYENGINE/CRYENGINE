// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Script Binding for Inventory

   -------------------------------------------------------------------------
   History:
   - 4:9:2005   15:29 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCRIPTBIND_INVENTORY_H__
#define __SCRIPTBIND_INVENTORY_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>

struct IItemSystem;
struct IGameFramework;
class CInventory;

class CScriptBind_Inventory :
	public CScriptableBase
{
public:
	CScriptBind_Inventory(ISystem* pSystem, IGameFramework* pGameFramework);
	virtual ~CScriptBind_Inventory();
	void         Release() { delete this; };
	void         AttachTo(CInventory* pInventory);
	void         DetachFrom(CInventory* pInventory);

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	//! <code>Inventory.Destroy()</code>
	//! <description>Destroys the inventory.</description>
	int Destroy(IFunctionHandler* pH);

	//! <code>Inventory.Clear()</code>
	//! <description>Clears the inventory.</description>
	int Clear(IFunctionHandler* pH);

	//! <code>Inventory.Dump()</code>
	//! <description>Dumps the inventory.</description>
	int Dump(IFunctionHandler* pH);

	//! <code>Inventory.GetItemByClass( className )</code>
	//!		<param name="className">Class name.</param>
	//! <description>Gets item by class name.</description>
	int GetItemByClass(IFunctionHandler* pH, const char* className);

	//! <code>Inventory.GetGrenadeWeaponByClass( className )</code>
	//!		<param name="className">Class name.</param>
	//! <description>Gets grenade weapon by class name.</description>
	int GetGrenadeWeaponByClass(IFunctionHandler* pH, const char* className);

	//! <code>Inventory.HasAccessory( accessoryName )</code>
	//!		<param name="accessoryName">Accessory name.</param>
	//! <description>Checks if the inventory contains the specified accessory.</description>
	int HasAccessory(IFunctionHandler* pH, const char* accessoryName);

	//! <code>Inventory.GetCurrentItemId()</code>
	//! <description>Gets the identifier of the current item.</description>
	int GetCurrentItemId(IFunctionHandler* pH);

	//! <code>Inventory.GetCurrentItem()</code>
	//! <description>Gets the current item.</description>
	int GetCurrentItem(IFunctionHandler* pH);

	//! <code>Inventory.GetAmmoCount(ammoName)</code>
	//!		<param name="ammoName">Ammunition name.</param>
	//! <description>Gets the amount of the specified ammunition name.</description>
	int GetAmmoCount(IFunctionHandler* pH, const char* ammoName);

	//! <code>Inventory.GetAmmoCapacity( ammoName )</code>
	//!		<param name="ammoName">Ammunition name.</param>
	//! <description>Gets the capacity for the specified ammunition.</description>
	int GetAmmoCapacity(IFunctionHandler* pH, const char* ammoName);

	//! <code>Inventory.SetAmmoCount( ammoName, count )</code>
	//!		<param name="ammoName">Ammunition name.</param>
	//!		<param name="count">Ammunition amount.</param>
	//! <description>Sets the amount of the specified ammunition.</description>
	int SetAmmoCount(IFunctionHandler* pH, const char* ammoName, int count);

private:
	void        RegisterGlobals();
	void        RegisterMethods();

	CInventory* GetInventory(IFunctionHandler* pH);

	ISystem*        m_pSystem;
	IEntitySystem*  m_pEntitySystem;
	IGameFramework* m_pGameFramework;
};

#endif //__SCRIPTBIND_INVENTORY_H__

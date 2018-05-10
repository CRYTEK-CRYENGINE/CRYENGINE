// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:

   -------------------------------------------------------------------------
   History:
   - 30:9:2004   14:21 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCRIPTBIND_ITEMSYSTEM_H__
#define __SCRIPTBIND_ITEMSYSTEM_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>

class CItemSystem;
class CEquipmentManager;

class CScriptBind_ItemSystem :
	public CScriptableBase
{
public:
	CScriptBind_ItemSystem(ISystem* pSystem, CItemSystem* pItemSystem, IGameFramework* pGameFramework);
	virtual ~CScriptBind_ItemSystem();

	void         Release() { delete this; };

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	//! <code>ItemSystem.Reset()</code>
	//! <description>Resets the item system.</description>
	int Reset(IFunctionHandler* pH);

	//! <code>ItemSystem.GiveItem( itemName )</code>
	//!		<param name="itemName">Item name.</param>
	//! <description>Gives the specified item.</description>
	int GiveItem(IFunctionHandler* pH, const char* itemName);

	//! <code>ItemSystem.GiveItemPack( actorId, packName )</code>
	//!		<param name="actorId">Actor identifier.</param>
	//!		<param name="packName">Pack name.</param>
	//! <description>Gives the item pack to the specified actor.</description>
	int GiveItemPack(IFunctionHandler* pH, ScriptHandle actorId, const char* packName);

	//! <code>ItemSystem.GetPackPrimaryItem( packName )</code>
	//!		<param name="packName">Pack name.</param>
	//! <description>Gets the primary item of the specified pack.</description>
	int GetPackPrimaryItem(IFunctionHandler* pH, const char* packName);

	//! <code>ItemSystem.GetPackNumItems()</code>
	//! <description>Get the number of items in the specified pack.</description>
	//!		<param name="packName">Pack name.</param>
	int GetPackNumItems(IFunctionHandler* pH, const char* packName);

	//! <code>ItemSystem.GetPackItemByIndex( packName, index )</code>
	//!		<param name="packName">Pack name.</param>
	//!		<param name="index">Pack index.</param>
	//! <description>Gets a pack item from its index.</description>
	int GetPackItemByIndex(IFunctionHandler* pH, const char* packName, int index);

	//! <code>ItemSystem.SetActorItem( actorId, itemId, keepHistory )</code>
	//!		<param name="actorId">Actor identifier.</param>
	//!		<param name="itemId">Item identifier.</param>
	//!		<param name="keepHistory">True to keep history, false otherwise.</param>
	//! <description>Sets an actor item.</description>
	int SetActorItem(IFunctionHandler* pH, ScriptHandle actorId, ScriptHandle itemId, bool keepHistory);

	//! <code>ItemSystem.SetActorItemByName( actorId, name, keepHistory )</code>
	//!		<param name="actorId">Actor identifier.</param>
	//!		<param name="name">Actor item name.</param>
	//!		<param name="keepHistory">True to keep history, false otherwise.</param>
	//! <description>Sets an actor item by name.</description>
	int SetActorItemByName(IFunctionHandler* pH, ScriptHandle actorId, const char* name, bool keepHistory);

	//! <code>ItemSystem.SerializePlayerLTLInfo( reading )</code>
	//!		<param name="reading">Boolean value.</param>
	//! <description>Serializes player LTL info.</description>
	int SerializePlayerLTLInfo(IFunctionHandler* pH, bool reading);

private:
	void RegisterGlobals();
	void RegisterMethods();

	CItemSystem*       m_pItemSystem;
	IGameFramework*    m_pGameFramework;
	CEquipmentManager* m_pEquipmentManager;
};

#endif //__SCRIPTBIND_ITEMSYSTEM_H__

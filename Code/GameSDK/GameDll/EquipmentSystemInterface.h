// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   EquipmentSystemInterface.h
//  Version:     v1.00
//  Created:     07/07/2006 by AlexL
//  Compilers:   Visual Studio.NET
//  Description: Interface for Editor to access CryAction/Game specific equipments
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __EQUIPMENTSYSTEMINTERFACE_H__
#define __EQUIPMENTSYSTEMINTERFACE_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <CrySandbox/IEditorGame.h>

// some forward decls
class CEditorGame;
struct IItemSystem;
struct IEquipmentManager;

class CEquipmentSystemInterface : public IEquipmentSystemInterface
{
public:
	CEquipmentSystemInterface(CEditorGame* pEditorGame, IGameToEditorInterface* pGTE);
	~CEquipmentSystemInterface();

	// return iterator with all available equipment items
	virtual IEquipmentItemIteratorPtr CreateEquipmentItemIterator(const char* type="");
	virtual IEquipmentItemIteratorPtr CreateEquipmentAccessoryIterator(const char* type);

	// delete all equipment packs
	virtual void DeleteAllEquipmentPacks();

	// load a single equipment pack from an XmlNode
	// Equipment Pack is basically
	// <EquipPack name="BasicPack">
	//   <Items>
	//      <Scar type="Weapon" />
	//      <SOCOM type="Weapon" />
	//   </Items>
	//   <Ammo Scar="50" SOCOM="70" />
	// </EquipPack>

	virtual bool LoadEquipmentPack(const XmlNodeRef& rootNode);	

	// set the players equipment pack. maybe we enable this, but normally via FG only
	// virtual void SetPlayerEquipmentPackName(const char *packName);
protected:
	void InitItems(IGameToEditorInterface* pGTE);

protected:
	class CIterator;

	CEditorGame* m_pEditorGame;
	IItemSystem* m_pIItemSystem;
	IEquipmentManager* m_pIEquipmentManager;

	typedef std::vector<string> TNameArray;
	typedef std::map<string, TNameArray> TItemMap;
	typedef std::map<string, TItemMap> TAccessoryMap;
	// maybe make it a multimap, or contain real item desc instead of only name
	TItemMap m_itemMap;
	TAccessoryMap m_accessoryMap;
};

#endif

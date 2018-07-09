// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   EquipmentSystem.cpp
//  Version:     v1.00
//  Created:     07/07/2006 by AlexL
//  Compilers:   Visual Studio.NET
//  Description: Interface for Editor to access CryAction/Game specific equipments
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "EquipmentSystemInterface.h"

#include <CryGame/IGame.h>
#include <CryGame/IGameFramework.h>
#include <IItemSystem.h>

#include "GameXmlParamReader.h"

CEquipmentSystemInterface::CEquipmentSystemInterface(CEditorGame* pEditorGame, IGameToEditorInterface *pGameToEditorInterface)
: m_pEditorGame(pEditorGame)
{
	m_pIItemSystem = gEnv->pGameFramework->GetIItemSystem();
	m_pIEquipmentManager = m_pIItemSystem->GetIEquipmentManager();
	InitItems(pGameToEditorInterface);
}

CEquipmentSystemInterface::~CEquipmentSystemInterface()
{
}

class CEquipmentSystemInterface::CIterator : public IEquipmentSystemInterface::IEquipmentItemIterator
{
public:
	CIterator(const TItemMap& equipmentMap, const char* type)
	{
		m_nRefs = 0;
		m_type  = type;
		if (m_type.empty())
		{
			m_mapIterCur = equipmentMap.begin();
			m_mapIterEnd = equipmentMap.end();
		}
		else
		{
			m_mapIterCur = equipmentMap.find(type);
			m_mapIterEnd = m_mapIterCur;
			if (m_mapIterEnd != equipmentMap.end())
				++m_mapIterEnd;
		}
		if (m_mapIterCur != m_mapIterEnd)
		{
			m_itemIterCur = m_mapIterCur->second.begin();
			m_itemIterEnd = m_mapIterCur->second.end();
		}
	}
	void AddRef()
	{
		++m_nRefs;
	}
	void Release()
	{
		if (--m_nRefs <= 0)
			delete this;
	}
	bool Next(SEquipmentItem& outItem)
	{
		if (m_mapIterCur != m_mapIterEnd)
		{
			if (m_itemIterCur != m_itemIterEnd)
			{
				outItem.name = (*m_itemIterCur).c_str();
				outItem.type = m_mapIterCur->first.c_str();
				++m_itemIterCur;
				if (m_itemIterCur == m_itemIterEnd)
				{
					++m_mapIterCur;
					if (m_mapIterCur != m_mapIterEnd)
					{
						m_itemIterCur = m_mapIterCur->second.begin();
						m_itemIterEnd = m_mapIterCur->second.end();
					}
				}
				return true;
			}
		}
		outItem.name = "";
		outItem.type = "";
		return false;
	}

	int m_nRefs;
	string m_type;
	TItemMap::const_iterator m_mapIterCur;
	TItemMap::const_iterator m_mapIterEnd;
	TNameArray::const_iterator m_itemIterCur;
	TNameArray::const_iterator m_itemIterEnd;
};

// return iterator with all available equipment items
IEquipmentSystemInterface::IEquipmentItemIteratorPtr 
CEquipmentSystemInterface::CreateEquipmentItemIterator(const char* type)
{
	return new CIterator(m_itemMap, type);
}

// return iterator with all available accessories for an item
IEquipmentSystemInterface::IEquipmentItemIteratorPtr 
CEquipmentSystemInterface::CreateEquipmentAccessoryIterator(const char* type)
{
	TAccessoryMap::const_iterator itemAccessoryMap = m_accessoryMap.find(type);

	if(itemAccessoryMap != m_accessoryMap.end())
	{
		return new CIterator(itemAccessoryMap->second, "");
	}
	
	return NULL;
}

// delete all equipment packs
void CEquipmentSystemInterface::DeleteAllEquipmentPacks()
{
	m_pIEquipmentManager->DeleteAllEquipmentPacks();
}

bool CEquipmentSystemInterface::LoadEquipmentPack(const XmlNodeRef& rootNode)
{
	return m_pIEquipmentManager->LoadEquipmentPack(rootNode);
}

namespace
{
	template <class Container> void ToContainer(const char** names, int nameCount, Container& container)
	{
		while (nameCount > 0)
		{
			container.push_back(*names);
			++names;
			--nameCount;
		}
	}
}

void CEquipmentSystemInterface::InitItems(IGameToEditorInterface* pGTE)
{
	// Get ItemParams from ItemSystem
	// Creates the following entries
	// "item"               All Item classes
	// "item_selectable"    All Item classes which can be selected
	// "item_givable",      All Item classes which can be given
	// "weapon"             All Weapon classes (an Item of class 'Weapon' or an Item which has ammo)
	// "weapon_selectable"  All Weapon classes which can be selected
	// "weapon_givable"     All Weapon classes which can be given
	// and for any weapon which has ammo
	// "ammo_WEAPONNAME"    All Ammos for this weapon

	IItemSystem* pItemSys = m_pIItemSystem;
	int maxCountItems = pItemSys->GetItemParamsCount();
	int maxAllocItems = maxCountItems+1; // allocate one more to store empty
	const char** allItemClasses = new const char*[maxAllocItems];
	const char** givableItemClasses = new const char*[maxAllocItems];
	const char** selectableItemClasses = new const char*[maxAllocItems];
	const char** allWeaponClasses = new const char*[maxAllocItems];
	const char** givableWeaponClasses = new const char*[maxAllocItems];
	const char** selectableWeaponClasses = new const char*[maxAllocItems];

	int numAllItems = 0;
	int numAllWeapons = 0;
	int numGivableItems = 0;
	int numSelectableItems = 0;
	int numSelectableWeapons = 0;
	int numGivableWeapons = 0;
	std::set<string> allAmmosSet;

	// store default "---" -> "" value
	{
		const char* empty = "";
		selectableWeaponClasses[numSelectableWeapons++] = empty;
		givableWeaponClasses[numGivableWeapons++] = empty;
		allWeaponClasses[numAllWeapons++] = empty;
		selectableItemClasses[numSelectableItems++] = empty;
		givableItemClasses[numGivableItems++] = empty;
		allItemClasses[numAllItems++] = empty;
		allAmmosSet.insert(empty);
	}

	for (int i=0; i<maxCountItems; ++i)
	{
		const char* itemName = pItemSys->GetItemParamName(i);
		allItemClasses[numAllItems++] = itemName;

		const char* itemDescriptionFile = pItemSys->GetItemParamsDescriptionFile(itemName);
		
		CRY_ASSERT(itemDescriptionFile);
		XmlNodeRef itemRootParams = gEnv->pSystem->LoadXmlFromFile(itemDescriptionFile);

		if (!itemRootParams)
		{
			GameWarning("Item description file %s doesn't exist for item %s", itemDescriptionFile, itemName);
			continue;
		}

		CGameXmlParamReader rootReader(itemRootParams);
		const char* inheritItem = itemRootParams->getAttr("inherit");
		bool isInherited = (inheritItem && inheritItem[0] != 0);
		if (isInherited)
		{
			const char* baseItemFile = pItemSys->GetItemParamsDescriptionFile(inheritItem);
			itemRootParams = gEnv->pSystem->LoadXmlFromFile(baseItemFile);
		}

		if (itemRootParams)
		{
			bool givable = false;
			bool selectable = false;
			bool uiWeapon = false;
			XmlNodeRef childItemParams = itemRootParams->findChild("params");
			if (childItemParams) 
			{
				CGameXmlParamReader reader(childItemParams);

				//the equipeable flag is supposed to be used for weapons that are not givable
				//but that are still needed to be in the equipment weapon list.
				bool equipeable = false;
				reader.ReadParamValue<bool>("equipeable", equipeable);
				reader.ReadParamValue<bool>("giveable", givable);
				givable |= equipeable;

				reader.ReadParamValue<bool>("selectable", selectable);
				reader.ReadParamValue<bool>("ui_weapon", uiWeapon);
			}

			if (givable)
				givableItemClasses[numGivableItems++] = itemName;
			if (selectable)
				selectableItemClasses[numSelectableItems++] = itemName;

			XmlNodeRef childAccessoriesNode = itemRootParams->findChild("accessories");

			if(childAccessoriesNode)
			{
				const int numAccessories = childAccessoriesNode->getChildCount();

				for(int childIdx = 0; childIdx < numAccessories; childIdx++)
				{
					XmlNodeRef childAccessory = childAccessoriesNode->getChild(childIdx);

					if(childAccessory && stricmp(childAccessory->getTag(), "accessory") == 0)
					{
						const char* accessoryName = childAccessory->getAttr("name");
						const char* categoryName = childAccessory->getAttr("category");			

						if(categoryName && accessoryName)
						{
							m_accessoryMap[itemName][categoryName].push_back(accessoryName);
						}
					}
				}
			}

			XmlNodeRef ammosNode = itemRootParams->findChild("ammos");
			if (ammosNode)
			{
				CGameXmlParamReader reader(ammosNode);

				const int maxAmmos = reader.GetUnfilteredChildCount();
				int numAmmos = 0;
				const char** ammoNames = new const char*[maxAmmos];
				for (int j=0; j<maxAmmos; ++j)
				{
					XmlNodeRef ammoChildNode = reader.GetFilteredChildAt(j);
					if ((ammoChildNode != NULL) && stricmp(ammoChildNode->getTag(), "ammo") == 0)
					{
						const char* ammoName = ammoChildNode->getAttr("name");
						if (ammoName && ammoName[0])
						{
							ammoNames[numAmmos] = ammoName;
							++numAmmos;
							allAmmosSet.insert(ammoName);
						}
					}
				}
				if (numAmmos > 0)
				{
					// make it a weapon when there's ammo
					allWeaponClasses[numAllWeapons++] = itemName;
					if (selectable)
						selectableWeaponClasses[numSelectableWeapons++] = itemName;
					if (givable)
						givableWeaponClasses[numGivableWeapons++] = itemName;

					string ammoEntryName = "ammo_";
					ammoEntryName+=itemName;
					pGTE->SetUIEnums(ammoEntryName.c_str(), ammoNames, numAmmos);
				}
				delete[] ammoNames;
			}
			else
			{
				const char* itemClass = itemRootParams->getAttr("class");
				if (uiWeapon || (itemClass != 0 && stricmp(itemClass, "weapon") == 0))
				{
					// make it a weapon when there's ammo
					allWeaponClasses[numAllWeapons++] = itemName;
					if (selectable)
						selectableWeaponClasses[numSelectableWeapons++] = itemName;
					if (givable)
						givableWeaponClasses[numGivableWeapons++] = itemName;
				}
			}
		}
	}

	int numAllAmmos = 0;
	const int allAmmoCount = allAmmosSet.size();
	const char** allAmmos = new const char*[allAmmoCount];
	std::set<string>::const_iterator iter (allAmmosSet.begin());
	while (iter != allAmmosSet.end())
	{
		PREFAST_ASSUME(numAllAmmos > 0 && numAllAmmos < allAmmoCount);
		allAmmos[numAllAmmos++] = iter->c_str();
		++iter;
	}
	pGTE->SetUIEnums("ammos", allAmmos, numAllAmmos);
	ToContainer(allAmmos+1, numAllAmmos-1, m_itemMap["Ammo"]);
	delete[] allAmmos;

	pGTE->SetUIEnums("weapon_selectable", selectableWeaponClasses, numSelectableWeapons);
	pGTE->SetUIEnums("weapon_givable", givableWeaponClasses, numGivableWeapons);
	pGTE->SetUIEnums("weapon", allWeaponClasses, numAllWeapons);
	pGTE->SetUIEnums("item_selectable", selectableItemClasses, numSelectableItems);
	pGTE->SetUIEnums("item_givable", givableItemClasses, numGivableItems);
	pGTE->SetUIEnums("item", allItemClasses, numAllItems);

	ToContainer(allItemClasses+1,numAllItems-1,m_itemMap["Item"]);
	ToContainer(givableItemClasses+1,numGivableItems-1,m_itemMap["ItemGivable"]);
	ToContainer(allWeaponClasses+1,numAllWeapons-1,m_itemMap["Weapon"]);

	delete[] selectableWeaponClasses;
	delete[] givableWeaponClasses;
	delete[] allWeaponClasses;
	delete[] selectableItemClasses;
	delete[] givableItemClasses; 
	delete[] allItemClasses;
}

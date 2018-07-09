// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   EquipmentManager.cpp
//  Version:     v1.00
//  Created:     07/07/2006 by AlexL
//  Compilers:   Visual Studio.NET
//  Description: EquipmentManager to handle item packs
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "EquipmentManager.h"
#include "ItemSystem.h"
#include "IActorSystem.h"
#include "Inventory.h"

namespace
{
void DumpPacks(IConsoleCmdArgs* pArgs)
{
	CEquipmentManager* pMgr = static_cast<CEquipmentManager*>(CCryAction::GetCryAction()->GetIItemSystem()->GetIEquipmentManager());
	if (pMgr)
	{
		pMgr->DumpPacks();
	}
}

bool InitConsole()
{
	REGISTER_COMMAND("eqp_DumpPacks", DumpPacks, VF_NULL, "Prints all equipment pack information to console");
	return true;
}

bool ShutdownConsole()
{
	gEnv->pConsole->RemoveCommand("eqp_DumpPacks");
	return true;
}
};

// helper class to make sure begin and end callbacks get called correctly
struct CGiveEquipmentPackNotifier
{
	CEquipmentManager* pEM;

	CGiveEquipmentPackNotifier(CEquipmentManager* pEM_, IActor* pActor) : pEM(pEM_){ if (pEM && pActor->IsClient()) pEM->OnBeginGiveEquipmentPack(); }
	~CGiveEquipmentPackNotifier() { if (pEM) pEM->OnEndGiveEquipmentPack(); }
};

CEquipmentManager::CEquipmentManager(CItemSystem* pItemSystem)
	: m_pItemSystem(pItemSystem)
{
	static bool sInitVars(InitConsole());
}

CEquipmentManager::~CEquipmentManager()
{
	ShutdownConsole();
}

void CEquipmentManager::Reset()
{
	stl::free_container(m_listeners);
}

// Clear all equipment packs
void CEquipmentManager::DeleteAllEquipmentPacks()
{
	std::for_each(m_equipmentPacks.begin(), m_equipmentPacks.end(), stl::container_object_deleter());
	m_equipmentPacks.clear();
}

// Loads equipment packs from rootNode
void CEquipmentManager::LoadEquipmentPacks(const XmlNodeRef& rootNode)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Equipment Packs");

	if (rootNode->isTag("EquipPacks") == false)
		return;

	for (int i = 0; i < rootNode->getChildCount(); ++i)
	{
		XmlNodeRef packNode = rootNode->getChild(i);
		LoadEquipmentPack(packNode, true);
	}
}

// Load all equipment packs from a certain folder
void CEquipmentManager::LoadEquipmentPacksFromPath(const char* path)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Equipment Packs");

	ICryPak* pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	string realPath(path);
	realPath.TrimRight("/\\");
	string search(realPath);
	search += "/*.xml";

	intptr_t handle = pCryPak->FindFirst(search.c_str(), &fd);
	if (handle != -1)
	{
		do
		{
			// fd.name contains the profile name
			string filename = path;
			filename += "/";
			filename += fd.name;

			MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "EquipmentPack XML (%s)", filename.c_str());

			XmlNodeRef rootNode = gEnv->pSystem->LoadXmlFromFile(filename.c_str());

			// load from XML node
			const bool ok = rootNode ? LoadEquipmentPack(rootNode) : false;
			if (!ok)
			{
				GameWarning("[EquipmentMgr]: Cannot load XML file '%s'. Skipping.", filename.c_str());
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

// Load an equipment pack from an XML node
bool CEquipmentManager::LoadEquipmentPack(const XmlNodeRef& rootNode, bool bOverrideExisting)
{
	if (rootNode->isTag("EquipPack") == false)
		return false;

	const char* packName = rootNode->getAttr("name");
	const char* primaryName = rootNode->getAttr("primary");

	if (!packName || packName[0] == 0)
		return false;

	// re-use existing pack
	SEquipmentPack* pPack = GetPack(packName);
	if (pPack == 0)
	{
		pPack = new SEquipmentPack;
		m_equipmentPacks.push_back(pPack);
	}
	else if (bOverrideExisting == false)
		return false;

	pPack->Init(packName);

	for (int iChild = 0; iChild < rootNode->getChildCount(); ++iChild)
	{
		const XmlNodeRef childNode = rootNode->getChild(iChild);
		if (childNode == 0)
			continue;

		if (childNode->isTag("Items"))
		{
			pPack->PrepareForItems(childNode->getChildCount());
			for (int i = 0; i < childNode->getChildCount(); ++i)
			{
				XmlNodeRef itemNode = childNode->getChild(i);
				const char* itemName = itemNode->getTag();
				const char* itemType = itemNode->getAttr("type");
				const char* itemSetup = itemNode->getAttr("setup");
				pPack->AddItem(itemName, itemType, itemSetup);
			}
		}
		else if (childNode->isTag("Ammo")) // legacy
		{
			const char* ammoName = "";
			const char* ammoCount = "";
			int nAttr = childNode->getNumAttributes();
			for (int j = 0; j < nAttr; ++j)
			{
				if (childNode->getAttributeByIndex(j, &ammoName, &ammoCount))
				{
					int nAmmoCount = atoi(ammoCount);
					pPack->m_ammoCount[ammoName] = nAmmoCount;
				}
			}
		}
		else if (childNode->isTag("Ammos"))
		{
			for (int i = 0; i < childNode->getChildCount(); ++i)
			{
				XmlNodeRef ammoNode = childNode->getChild(i);
				if (ammoNode->isTag("Ammo") == false)
					continue;
				const char* ammoName = ammoNode->getAttr("name");
				if (ammoName == 0 || ammoName[0] == '\0')
					continue;
				int nAmmoCount = 0;
				ammoNode->getAttr("amount", nAmmoCount);
				pPack->m_ammoCount[ammoName] = nAmmoCount;
			}
		}
	}
	// assign primary.
	if (pPack->HasItem(primaryName))
		pPack->m_primaryItem = primaryName;
	else
		pPack->m_primaryItem = "";

	return true;
}

// Give an equipment pack (resp. items/ammo) to an actor
bool CEquipmentManager::GiveEquipmentPack(IActor* pActor, const char* packName, bool bAdd, bool selectPrimary)
{
	if (!pActor)
		return false;

	// causes side effects, don't remove
	CGiveEquipmentPackNotifier notifier(this, pActor);

	SEquipmentPack* pPack = GetPack(packName);

	if (pPack == 0)
	{
		const IEntity* pEntity = pActor->GetEntity();
		GameWarning("[EquipmentMgr]: Cannot give pack '%s' to Actor '%s'. Pack not found.", packName, pEntity ? pEntity->GetName() : "<unnamed>");
		return false;
	}

	IInventory* pInventory = pActor->GetInventory();
	if (pInventory == 0)
	{
		const IEntity* pEntity = pActor->GetEntity();
		GameWarning("[EquipmentMgr]: Cannot give pack '%s' to Actor '%s'. No inventory.", packName, pEntity ? pEntity->GetName() : "<unnamed>");
		return false;
	}

	bool bHasNoWeapon = false;
	bool bHasAnySelected = false;
	const char* strNoWeapon = "NoWeapon";

	if (bAdd == false)
	{
		IItem* pItem = m_pItemSystem->GetItem(pInventory->GetCurrentItem());
		if (pItem)
		{
			pItem->Select(false);
			m_pItemSystem->SetActorItem(pActor, (EntityId)0, false);
		}
		pInventory->RemoveAllItems(true);
		pInventory->ResetAmmo();
	}
	else
	{
		// Since we're adding items, check on the current status of NoWeapon
		bHasNoWeapon = pInventory->GetCountOfClass(strNoWeapon) > 0;
	}

	std::vector<SEquipmentPack::SEquipmentItem>::const_iterator itemIter = pPack->m_items.begin();
	std::vector<SEquipmentPack::SEquipmentItem>::const_iterator itemIterEnd = pPack->m_items.end();

	for (; itemIter != itemIterEnd; ++itemIter)
	{
		const SEquipmentPack::SEquipmentItem& item = *itemIter;

		// If the equipmentPack specifies a primary weapon then select this item if it's the specified item, if
		// the equipmentPack doesn't specify a primary weapon then just select the first item of the set (fallback)
		bool bPrimaryItem = (itemIter == pPack->m_items.begin());
		if (!pPack->m_primaryItem.empty())
			bPrimaryItem = (pPack->m_primaryItem.compare(item.m_name) == 0);

		EntityId itemId = m_pItemSystem->GiveItem(pActor, item.m_name, false, bPrimaryItem, true); // don't select

		// Update state of NoWeapon
		bHasNoWeapon |= (item.m_name == strNoWeapon);
		bHasAnySelected |= bPrimaryItem;

		if (!item.m_setup.empty())
		{
			if (IItem* pItem = m_pItemSystem->GetItem(itemId))
			{
				//remove all current accessories (initial setup in xml) and attach the specified list
				pItem->DetachAllAccessories();

				int numAccessories = item.m_setup.size();

				for (int i = 0; i < numAccessories; i++)
				{
					m_pItemSystem->GiveItem(pActor, item.m_setup[i]->GetName(), false, false, false);
					pItem->AttachAccessory(item.m_setup[i], true, true, true, true);
				}
			}
		}
	}

	// Handle the case where NoWeapon is not currently in possession of the actor (CE-1290)
	// In this case, give the NoWeapon item anyway, and then select it if nothing else is selected
	if (bHasNoWeapon == false)
	{
		GameWarning("[EquipmentMgr]: The pack '%s' does not contain '%s', given it anyway because it's required.", packName, strNoWeapon);
		m_pItemSystem->GiveItem(pActor, strNoWeapon, false, !bHasAnySelected);
	}

	// Handle ammo
	std::map<string, int>::const_iterator iter = pPack->m_ammoCount.begin();
	std::map<string, int>::const_iterator iterEnd = pPack->m_ammoCount.end();

	if (bAdd)
	{
		while (iter != iterEnd)
		{
			if (iter->second > 0)
			{
				IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(iter->first);
				if (pClass)
				{
					const int count = pInventory->GetAmmoCount(pClass) + iter->second;
					pInventory->SetAmmoCount(pClass, count);
					if (gEnv->bServer)
					{
						pActor->GetGameObject()->InvokeRMI(CInventory::Cl_SetAmmoCount(),
						                                   TRMIInventory_Ammo(pClass->GetName(), count),
						                                   eRMI_ToRemoteClients);
					}
					/*
					          if(IActor* pIventoryActor = pInventory->GetActor())
					            if(pIventoryActor->IsClient())
					              pIventoryActor->NotifyInventoryAmmoChange(pClass, iter->second);
					 */
				}
				else
				{
					GameWarning("[EquipmentMgr]: Invalid AmmoType '%s' in Pack '%s'.", iter->first.c_str(), packName);
				}
			}
			++iter;
		}
	}
	else
	{
		while (iter != iterEnd)
		{
			if (iter->second > 0)
			{
				IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(iter->first);
				if (pClass)
				{
					pInventory->SetAmmoCount(pClass, iter->second);
					/*
					          if(IActor* pIventoryActor = pInventory->GetActor())
					            if(pIventoryActor->IsClient())
					              pIventoryActor->NotifyInventoryAmmoChange(pClass, iter->second);
					 */
				}
				else
				{
					GameWarning("[EquipmentMgr]: Invalid AmmoType '%s' in Pack '%s'.", iter->first.c_str(), packName);
				}
			}
			++iter;
		}
	}

	return true;
}

CEquipmentManager::SEquipmentPack* CEquipmentManager::GetPack(const char* packName) const
{
	for (TEquipmentPackVec::const_iterator iter = m_equipmentPacks.begin();
	     iter != m_equipmentPacks.end(); ++iter)
	{
		if (stricmp((*iter)->m_name.c_str(), packName) == 0)
			return *iter;
	}
	return 0;
}

void CEquipmentManager::PreCacheEquipmentPackResources(const char* packName, IEquipmentPackPreCacheCallback& preCacheCallback)
{
	SEquipmentPack* pEquipPack = GetPack(packName);
	if (pEquipPack)
	{
		const int itemCount = pEquipPack->m_items.size();
		for (int i = 0; i < itemCount; ++i)
		{
			const SEquipmentPack::SEquipmentItem& item = pEquipPack->m_items[i];
			preCacheCallback.PreCacheItemResources(item.m_name.c_str());

			int numSetup = pEquipPack->m_items[i].m_setup.size();
			for (int j = 0; j < numSetup; j++)
			{
				preCacheCallback.PreCacheItemResources(pEquipPack->m_items[i].m_setup[j]->GetName());
			}
		}
	}
}

// return iterator with all available equipment packs
IEquipmentManager::IEquipmentPackIteratorPtr
CEquipmentManager::CreateEquipmentPackIterator()
{
	class CEquipmentIterator : public IEquipmentPackIterator
	{
	public:
		CEquipmentIterator(CEquipmentManager* pMgr)
		{
			m_nRefs = 0;
			m_pMgr = pMgr;
			m_cur = m_pMgr->m_equipmentPacks.begin();
		}
		void AddRef()
		{
			++m_nRefs;
		}
		void Release()
		{
			if (0 == --m_nRefs)
				delete this;
		}
		int GetCount()
		{
			return m_pMgr->m_equipmentPacks.size();
		}
		const char* Next()
		{
			const char* name = 0;
			if (m_cur != m_end)
			{
				name = (*m_cur)->m_name.c_str();
				++m_cur;
			}
			return name;
		}
		int                         m_nRefs;
		CEquipmentManager*          m_pMgr;
		TEquipmentPackVec::iterator m_cur;
		TEquipmentPackVec::iterator m_end;
	};
	return new CEquipmentIterator(this);
}

void CEquipmentManager::RegisterListener(IEquipmentManager::IListener* pListener)
{
	stl::push_back_unique(m_listeners, pListener);
}

void CEquipmentManager::UnregisterListener(IEquipmentManager::IListener* pListener)
{
	stl::find_and_erase(m_listeners, pListener);
}

void CEquipmentManager::OnBeginGiveEquipmentPack()
{
	TListenerVec::iterator iter = m_listeners.begin();
	while (iter != m_listeners.end())
	{
		(*iter)->OnBeginGiveEquipmentPack();
		++iter;
	}
}

void CEquipmentManager::OnEndGiveEquipmentPack()
{
	TListenerVec::iterator iter = m_listeners.begin();
	while (iter != m_listeners.end())
	{
		(*iter)->OnEndGiveEquipmentPack();
		++iter;
	}
}

void CEquipmentManager::DumpPack(const SEquipmentPack* pPack) const
{
	CryLogAlways("Pack: '%s' Primary='%s' ItemCount=%" PRISIZE_T " AmmoCount=%" PRISIZE_T,
	             pPack->m_name.c_str(), pPack->m_primaryItem.c_str(), pPack->m_items.size(), pPack->m_ammoCount.size());

	CryLogAlways("   Items:");
	for (std::vector<SEquipmentPack::SEquipmentItem>::const_iterator iter = pPack->m_items.begin();
	     iter != pPack->m_items.end(); ++iter)
	{
		CryLogAlways("   '%s' : '%s'", iter->m_name.c_str(), iter->m_type.c_str());

		int numAccessories = iter->m_setup.size();

		for (int i = 0; i < numAccessories; i++)
		{
			CryLogAlways("			Accessory: '%s'", iter->m_setup[i]->GetName());
		}
	}

	CryLogAlways("   Ammo:");
	for (std::map<string, int>::const_iterator iter = pPack->m_ammoCount.begin();
	     iter != pPack->m_ammoCount.end(); ++iter)
	{
		CryLogAlways("   '%s'=%d", iter->first.c_str(), iter->second);
	}
}

void CEquipmentManager::DumpPacks()
{
	// all sessions
	CryLogAlways("[EQP] PackCount=%" PRISIZE_T, m_equipmentPacks.size());
	for (TEquipmentPackVec::const_iterator iter = m_equipmentPacks.begin();
	     iter != m_equipmentPacks.end(); ++iter)
	{
		const SEquipmentPack* pPack = *iter;
		DumpPack(pPack);
	}
}

void CEquipmentManager::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_SUBCOMPONENT_NAME(pSizer, "EquipmentManager");
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_equipmentPacks);
}

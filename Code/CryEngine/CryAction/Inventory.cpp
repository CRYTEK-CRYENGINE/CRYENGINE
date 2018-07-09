// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 18:8:2005   17:27 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Inventory.h"
#include <CryEntitySystem/IEntitySystem.h>
#include "IGameObject.h"
#include "CryAction.h"
#include "ScriptBind_Inventory.h"
#include "IActorSystem.h"

//------------------------------------------------------------------------
CInventory::CInventory()
	: m_pGameFrameWork(nullptr)
	, m_pActor(nullptr)
	, m_bSerializeLTL(false)
	, m_iteratingListeners(false)
	, m_ignoreNextClear(false)
{
	m_stats.slots.reserve(gEnv->pConsole->GetCVar("i_inventory_capacity")->GetIVal());
}

//------------------------------------------------------------------------
CInventory::~CInventory()
{
	CCryAction* pCryAction = static_cast<CCryAction*>(gEnv->pGameFramework);
	pCryAction->GetInventoryScriptBind()->DetachFrom(this);
}

//------------------------------------------------------------------------
bool CInventory::Init(IGameObject* pGameObject)
{
	SetGameObject(pGameObject);
	// attach script bind
	CCryAction* pCryAction = static_cast<CCryAction*>(gEnv->pGameFramework);
	pCryAction->GetInventoryScriptBind()->AttachTo(this);

	m_pGameFrameWork = pCryAction;

	m_pActor = pCryAction->GetIActorSystem()->GetActor(pGameObject->GetEntityId());

	return true;
}

//------------------------------------------------------------------------
bool CInventory::ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params)
{
	ResetGameObject();

	Destroy();

	return true;
}

//------------------------------------------------------------------------
void CInventory::PostReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params)
{
	// attach script bind
	CCryAction* pCryAction = static_cast<CCryAction*>(gEnv->pGameFramework);
	pCryAction->GetInventoryScriptBind()->AttachTo(this);

	m_pActor = pCryAction->GetIActorSystem()->GetActor(pGameObject->GetEntityId());
}

//------------------------------------------------------------------------
void CInventory::FullSerialize(TSerialize ser)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Inventory serialization");

	ser.BeginGroup("InventoryItems");

	ser.Value("CurrentItem", m_stats.currentItemId);
	ser.Value("HolsterItem", m_stats.holsteredItemId);
	ser.Value("LastItem", m_stats.lastItemId);
	int s = m_stats.slots.size();
	ser.Value("InventorySize", s);
	if (ser.IsReading())
		m_stats.slots.resize(s);

	ser.Value("Slots", m_stats.slots);

	ser.BeginGroup("accessorySlots");
	int exSize = m_stats.accessorySlots.size();
	ser.Value("Size", exSize);

	for (int i = 0; i < IInventory::eInventorySlot_Last; ++i)
	{
		ser.BeginGroup("SlotInfo");
		ser.Value("slotInfoCount", m_stats.slotsInfo[i].count);
		ser.Value("slotInfoLastSelected", m_stats.slotsInfo[i].lastSelected);
		ser.EndGroup();
	}

	if (ser.IsReading())
	{
		m_stats.accessorySlots.resize(0);
		if (exSize > 0)
			m_stats.accessorySlots.reserve(exSize);
	}
	for (int i = 0; i < exSize; ++i)
	{
		string accessoryName;
		if (ser.IsWriting())
			accessoryName = m_stats.accessorySlots[i]->GetName();

		ser.BeginGroup("Class");
		ser.Value("AccessoryName", accessoryName);
		ser.EndGroup();

		if (ser.IsReading())
		{
			IEntityClass* pAccessoryClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(accessoryName);
			CRY_ASSERT(pAccessoryClass);
			if (pAccessoryClass != NULL)
			{
				m_stats.accessorySlots.push_back(pAccessoryClass);
			}
		}
	}
	ser.EndGroup();//"accessorySlots"

	ser.BeginGroup("Ammo");
	if (ser.IsReading())
	{
		m_stats.ammoInfo.clear();
	}

	TAmmoInfoMap::iterator ammoInfoIt = m_stats.ammoInfo.begin();
	int ammoAmount = m_stats.ammoInfo.size();
	ser.Value("AmmoAmount", ammoAmount);
	for (int i = 0; i < ammoAmount; ++i)
	{
		string name;
		int amount = 0;
		int capacity = 0;
		if (ser.IsWriting())
		{
			IEntityClass* pAmmoClass = ammoInfoIt->first;
			CRY_ASSERT(pAmmoClass);
			name = (pAmmoClass) ? pAmmoClass->GetName() : "";

			const SAmmoInfo& ammoInfo = ammoInfoIt->second;

			amount = ammoInfo.GetCount();
			//users = ammoInfo.GetUserCount();
			capacity = ammoInfo.GetCapacity();
			// Only use this iterator writing. If we're reading, we change the size
			// of the map and end up with an out of sync (and invalid) iterator.
			++ammoInfoIt;
		}
		ser.BeginGroup("Ammo");
		ser.Value("AmmoName", name);
		ser.Value("Bullets", amount);
		ser.Value("Capacity", capacity);
		ser.EndGroup();
		if (ser.IsReading())
		{
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			CRY_ASSERT(pClass);
			TAmmoInfoMap::iterator it = m_stats.ammoInfo.find(pClass);
			if (it == m_stats.ammoInfo.end())
			{
				m_stats.ammoInfo[pClass] = SAmmoInfo(amount, capacity);
			}
			else
			{
				it->second.SetCount(amount);
				it->second.SetCapacity(capacity);
			}
		}
	}
	ser.EndGroup();

	/*
	   ser.BeginGroup("CategorySlots");

	   for (TSlotsInfo::iterator it = m_stats.slotsInfo.begin(); it != m_stats.slotsInfo.end(); ++it)
	   {
	    ser.Value("Count", it->second.count);
	   }

	   ser.EndGroup();*/

	ser.EndGroup();
}

void CInventory::PostSerialize()
{
	for (int i = 0; i < IInventory::eInventorySlot_Last; ++i)
	{
		const int itemIndex = FindItem(m_stats.slotsInfo[i].lastSelected);

		// For whatever reason we don't have this last item in the inventory,
		// so find a suitable item in the same slot.
		if (itemIndex == -1)
		{
			const EntityId entityId = GetAnyEntityInSlot(i);

			SetLastSelectedInSlot(entityId);
		}
	}

	// Benito - This is last minute workaround to solve a strange bug:
	// If the game is saved while you had equipped a 'heavy weapon' (dynamic one, not present in the level) but this ones is holstered at the moment of save
	// when the game is loaded, the item is not restored properly, because the entity does not get a serialize call.
	// This code ensures that weapon goes back to the hands of the player, adding as many checks here as possible before doing the final call
	const bool ownerActorIsClientNotInVehicle = (m_pActor != NULL) && m_pActor->IsClient() && (m_pActor->GetLinkedVehicle() == NULL);
	if (ownerActorIsClientNotInVehicle)
	{
		IItem* pCurrentItem = gEnv->pGameFramework->GetIItemSystem()->GetItem(m_stats.currentItemId);
		if ((pCurrentItem != NULL) && (pCurrentItem->GetOwnerId() == 0))
		{
			if (pCurrentItem->CanUse(m_pActor->GetEntityId()))
			{
				m_stats.currentItemId = 0; //Reset and use it again
				pCurrentItem->Use(m_pActor->GetEntityId());
			}
		}
	}
}

//------------------------------------------------------------------------
void CInventory::SerializeInventoryForLevelChange(TSerialize ser)
{
	IActor* pActor = GetActor();
	if (!pActor)
		return;

	if (ser.IsReading())
	{
		m_stats.ammoInfo.clear();
	}

	m_bSerializeLTL = true;

	//Items by class (accessories)
	ser.BeginGroup("accessorySlots");
	int exSize = m_stats.accessorySlots.size();
	ser.Value("Size", exSize);

	if (ser.IsReading())
	{
		m_stats.accessorySlots.resize(0);
		if (exSize > 0)
			m_stats.accessorySlots.reserve(exSize);
	}
	for (int i = 0; i < exSize; ++i)
	{
		string accessoryName;
		if (ser.IsWriting())
			accessoryName = m_stats.accessorySlots[i]->GetName();

		ser.BeginGroup("Class");
		ser.Value("AccessoryName", accessoryName);
		ser.EndGroup();

		if (ser.IsReading())
		{
			IEntityClass* pAccessoryClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(accessoryName);
			CRY_ASSERT(pAccessoryClass);
			if (pAccessoryClass)
			{
				m_stats.accessorySlots.push_back(pAccessoryClass);
			}
		}
	}
	ser.EndGroup();//"accessorySlots"

	if (ser.IsReading())
	{
		for (int r = 0; r < m_stats.slots.size(); ++r)
		{
			IItem* pItem = m_pGameFrameWork->GetIItemSystem()->GetItem(m_stats.slots[r]);
			if (pItem)
			{
				pItem->Drop();
				pItem->GetEntity()->SetFlags(pItem->GetEntity()->GetFlags() | ENTITY_FLAG_UPDATE_HIDDEN);
				pItem->GetEntity()->Hide(true);
				gEnv->pEntitySystem->RemoveEntity(m_stats.slots[r]);
			}
		}
	}

	int numItems = 0;

	string currentItemNameOnReading;

	if (ser.IsReading())
	{
		m_stats.slots.clear();
		m_stats.currentItemId = 0;
		m_stats.holsteredItemId = 0;
		m_stats.lastItemId = 0;

		string itemName;
		ser.Value("numOfItems", numItems);
		for (int i = 0; i < numItems; ++i)
		{
			ser.BeginGroup("Items");
			bool nextItemExists = false;
			ser.Value("nextItemExists", nextItemExists);
			if (nextItemExists)
			{
				ser.Value("ItemName", itemName);
				EntityId id = m_pGameFrameWork->GetIItemSystem()->GiveItem(pActor, itemName.c_str(), false, false, false);
				IItem* pItem = m_pGameFrameWork->GetIItemSystem()->GetItem(id);
				if (pItem)
				{
					//actual serialization
					pItem->SerializeLTL(ser);
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Couldn't spawn inventory item %s, got id %u.", itemName.c_str(), id);
				}
			}

			ser.EndGroup();
		}
		ser.Value("CurrentItemName", itemName);
		if (stricmp(itemName.c_str(), "none"))
		{
			currentItemNameOnReading = itemName;
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(itemName.c_str());
			if (pClass)
			{
				if (IItem* pItem = m_pGameFrameWork->GetIItemSystem()->GetItem(GetItemByClass(pClass)))
				{
					if (pActor->GetCurrentItem() && pActor->GetCurrentItem() != pItem)
						pActor->GetCurrentItem()->Select(false);
					pItem->Select(true);
					m_stats.currentItemId = pItem->GetEntityId();
				}
				else
					m_stats.currentItemId = m_pGameFrameWork->GetIItemSystem()->GiveItem(pActor, itemName.c_str(), false, true, false);
			}
		}
	}
	else
	{
		numItems = m_stats.slots.size();
		ser.Value("numOfItems", numItems);

		for (int i = 0; i < numItems; ++i)
		{
			ser.BeginGroup("Items");
			IItem* pItem = m_pGameFrameWork->GetIItemSystem()->GetItem(m_stats.slots[i]);
			bool nextItem = true;
			if (pItem)
			{
				ser.Value("nextItemExists", nextItem);
				ser.Value("ItemName", pItem->GetEntity()->GetClass()->GetName());
				pItem->SerializeLTL(ser);
			}
			else
			{
				nextItem = false;
				ser.Value("nextItemExists", nextItem);
			}
			ser.EndGroup();
		}
		bool currentItemIsInInventory = stl::find(m_stats.slots, m_stats.currentItemId);
		IItem* pCurrentItem = NULL;

		if (currentItemIsInInventory)
		{
			pCurrentItem = m_pGameFrameWork->GetIItemSystem()->GetItem(m_stats.currentItemId);
		}
		else
		{
			pCurrentItem = m_pGameFrameWork->GetIItemSystem()->GetItem(GetHolsteredItem());
			//Fallback to last selected one...
			if (!pCurrentItem)
			{
				pCurrentItem = m_pGameFrameWork->GetIItemSystem()->GetItem(GetLastItem());
				// desperate fallback to any weapon...
				// this fallback should never be needed. However, right now it happens if the player loads a savegame where a heavyweapon is being used, right before the end of the mission.
				// that is a bug, but at this point is safer to just do this bruteforce fallback instead of fixing it
				// TODO: to fix that and remove this fallback...
				if (!pCurrentItem)
				{
					for (int i = 0; i < numItems; ++i)
					{
						IItem* pItem = m_pGameFrameWork->GetIItemSystem()->GetItem(m_stats.slots[i]);
						if (pItem)
						{
							const char* pCategoryName = m_pGameFrameWork->GetIItemSystem()->GetItemCategory(pItem->GetEntity()->GetClass()->GetName());
							if (pCategoryName)
							{
								EInventorySlots slotType = GetSlotForItemCategory(pCategoryName);
								if (slotType == eInventorySlot_Weapon)
								{
									pCurrentItem = pItem;
									break;
								}
							}
						}
					}
				}
			}
		}

		if (pCurrentItem)
		{
			ser.Value("CurrentItemName", pCurrentItem->GetEntity()->GetClass()->GetName());
		}
		else
		{
			string name("none");
			ser.Value("CurrentItemName", name);
		}
	}

	//**************************************AMMO

	ser.BeginGroup("Ammo");

	TAmmoInfoMap::iterator ammoInfoIt = m_stats.ammoInfo.begin();
	int ammoAmount = m_stats.ammoInfo.size();
	ser.Value("AmmoAmount", ammoAmount);
	for (int i = 0; i < ammoAmount; ++i)
	{
		string name;
		int amount = 0;
		int users = 0;
		int capacity = 0;
		if (ser.IsWriting())
		{
			IEntityClass* pAmmoClass = ammoInfoIt->first;
			CRY_ASSERT(pAmmoClass);
			name = (pAmmoClass) ? pAmmoClass->GetName() : "";
			const SAmmoInfo& ammoInfo = ammoInfoIt->second;
			;
			amount = ammoInfo.GetCount();
			capacity = ammoInfo.GetCapacity();

			++ammoInfoIt;
		}
		ser.BeginGroup("Ammo");
		ser.Value("AmmoName", name);
		ser.Value("Bullets", amount);
		ser.Value("Capacity", capacity);
		ser.EndGroup();
		if (ser.IsReading())
		{
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			CRY_ASSERT(pClass);
			TAmmoInfoMap::iterator it = m_stats.ammoInfo.find(pClass);
			if (it == m_stats.ammoInfo.end())
			{
				m_stats.ammoInfo[pClass] = SAmmoInfo(amount, capacity);
			}
			else
			{
				it->second.SetCount(amount);
				it->second.SetCapacity(capacity);
			}
		}
	}

	ser.EndGroup();

	m_bSerializeLTL = false;
}

//------------------------------------------------------------------------
void CInventory::ProcessEvent(const SEntityEvent& event)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (event.event == ENTITY_EVENT_RESET)
	{
		if (gEnv->IsEditor())
		{
			if (event.nParam[0]) // entering game mode in editor
				m_editorstats = m_stats;
			else
			{
				// leaving game mode
				if (m_stats.currentItemId != INVALID_ENTITYID)
					m_pGameFrameWork->GetIItemSystem()->SetActorItem(m_pActor, INVALID_ENTITYID, false);
				m_stats = m_editorstats;

				//Validate inventory, some things might have changed, like some FG removing items while in editor game
				Validate();
			}
		}
	}
}

//------------------------------------------------------------------------
uint64 CInventory::GetEventMask() const 
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_RESET);
}

//------------------------------------------------------------------------
bool CInventory::AddItem(EntityId id)
{
	if (FindItem(id) > -1)
		return true;

	if (m_stats.slots.size() >= gEnv->pConsole->GetCVar("i_inventory_capacity")->GetIVal())
		return false;

	AddItemToCategorySlot(id);

	m_stats.slots.push_back(id);
	std::sort(m_stats.slots.begin(), m_stats.slots.end(), compare_slots());

	TListenerVec::iterator iter = m_listeners.begin();
	m_iteratingListeners = true;
	while (iter != m_listeners.end())
	{
		(*iter)->OnAddItem(id);
		++iter;
	}
	m_iteratingListeners = false;

	return true;
}

//------------------------------------------------------------------------
bool CInventory::RemoveItem(EntityId id)
{
	bool result = stl::find_and_erase(m_stats.slots, id);

	if (result)
		RemoveItemFromCategorySlot(id);

	return result;
}

//------------------------------------------------------------------------
void CInventory::RemoveAllItems(bool forceClear)
{
	this->Clear(forceClear);
}

//------------------------------------------------------------------------
int CInventory::Validate()
{
	TInventoryVector copyOfSlots;
	copyOfSlots.reserve(m_stats.slots.size());
	std::swap(copyOfSlots, m_stats.slots);

	int count = 0;
	const int slotCount = copyOfSlots.size();

	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
	for (int i = 0; i < slotCount; ++i)
	{
		EntityId itemId = copyOfSlots[i];
		IEntity* pEntity = pEntitySystem->GetEntity(itemId);
		if (!pEntity)
		{
			++count;
		}
		else
		{
			m_stats.slots.push_back(itemId);
		}
	}

	return count;
}

//------------------------------------------------------------------------
void CInventory::Destroy()
{
	//
	//CryLog("%s::CInventory::Destroy()",GetEntity()->GetName());
	//
	if (!GetISystem()->IsSerializingFile())
	{
		IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
		IItemSystem* pItemSystem = gEnv->pGameFramework->GetIItemSystem();

		TInventoryVector deleteList = m_stats.slots;
		for (TInventoryIt it = deleteList.begin(); it != deleteList.end(); ++it)
		{
			EntityId entityId = *it;

			IItem* pItem = pItemSystem->GetItem(entityId);
			if (pItem)
			{
				RemoveItemFromCategorySlot(pItem->GetEntityId());
				pItem->RemoveOwnerAttachedAccessories();
				pItem->AttachToHand(false);
				pItem->AttachToBack(false);
				pItem->SetOwnerId(0);
			}

			pEntitySystem->RemoveEntity(entityId);
		}
	}

	Clear();
}

//------------------------------------------------------------------------
void CInventory::Clear(bool forceClear)
{
	if (m_ignoreNextClear && !forceClear)
	{
		m_ignoreNextClear = false;
		return;
	}

	if (!gEnv->bServer)
	{
		IItemSystem* pItemSystem = gEnv->pGameFramework->GetIItemSystem();

		TInventoryIt end = m_stats.slots.end();
		for (TInventoryIt it = m_stats.slots.begin(); it != end; ++it)
		{
			if (IItem* pItem = pItemSystem->GetItem(*it))
			{
				pItem->RemoveOwnerAttachedAccessories();
				pItem->AttachToHand(false);
				pItem->AttachToBack(false);
				pItem->SetOwnerId(0);
				pItem->GetEntity()->Hide(true);
			}
		}
	}

	m_stats.slots.clear();
	m_stats.accessorySlots.clear();
	ResetAmmoAndUsers();

	for (int i = 0; i < IInventory::eInventorySlot_Last; ++i)
	{
		m_stats.slotsInfo[i].Reset();
	}

	m_stats.currentItemId = 0;
	m_stats.holsteredItemId = 0;
	m_stats.lastItemId = 0;

	TListenerVec::iterator iter = m_listeners.begin();
	m_iteratingListeners = true;
	while (iter != m_listeners.end())
	{
		(*iter)->OnClearInventory();
		++iter;
	}
	m_iteratingListeners = false;
}

//------------------------------------------------------------------------
#if DEBUG_INVENTORY_ENABLED

void CInventory::Dump() const
{
	struct Dumper
	{
		Dumper(EntityId entityId, const char* desc)
		{
			IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
			IEntity* pEntity = pEntitySystem->GetEntity(entityId);
			CryLogAlways(">> Id: %u [%s] $3%s $5%s", entityId, pEntity ? pEntity->GetName() : "<unknown>", pEntity ? pEntity->GetClass()->GetName() : "<unknown>", desc ? desc : "");
		}
	};

	int count = GetCount();
	CryLogAlways("-- $3%s$1's Inventory: %d Items --", GetEntity()->GetName(), count);

	if (count)
	{
		for (TInventoryCIt it = m_stats.slots.begin(); it != m_stats.slots.end(); ++it)
			Dumper dump(*it, 0);
	}

	CryLogAlways(">> --");

	Dumper current(m_stats.currentItemId, "Current");
	Dumper last(m_stats.lastItemId, "Last");
	Dumper holstered(m_stats.holsteredItemId, "Holstered");

	CryLogAlways("-- $3%s$1's Inventory: %" PRISIZE_T " Ammo Types --", GetEntity()->GetName(), m_stats.ammoInfo.size());

	if (!m_stats.ammoInfo.empty())
	{
		for (TAmmoInfoMap::const_iterator ait = m_stats.ammoInfo.begin(); ait != m_stats.ammoInfo.end(); ++ait)
			CryLogAlways(">> [%s] $3%d$1/$3%d", ait->first->GetName(), ait->second.GetCount(), GetAmmoCapacity(ait->first));
	}
}

#endif //DEBUG_INVENTORY_ENABLED

//------------------------------------------------------------------------
int CInventory::GetCapacity() const
{
	return gEnv->pConsole->GetCVar("i_inventory_capacity")->GetIVal();
}

//------------------------------------------------------------------------
int CInventory::GetCount() const
{
	return m_stats.slots.size();
}

//------------------------------------------------------------------------
int CInventory::GetCountOfClass(const char* className) const
{
	int count = 0;

	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;

	IEntityClass* pClass = (className != NULL) ? pEntitySystem->GetClassRegistry()->FindClass(className) : NULL;
	if (pClass)
	{
		for (TInventoryCIt it = m_stats.slots.begin(); it != m_stats.slots.end(); ++it)
		{
			IEntity* pEntity = pEntitySystem->GetEntity(*it);
			if ((pEntity != NULL) && (pEntity->GetClass() == pClass))
			{
				++count;
			}
		}
		TInventoryVectorEx::const_iterator endEx = m_stats.accessorySlots.end();
		for (TInventoryVectorEx::const_iterator cit = m_stats.accessorySlots.begin(); cit != endEx; ++cit)
		{
			if (*cit == pClass)
			{
				count++;
			}
		}
	}

	return count;
}
//------------------------------------------------------------------------
int CInventory::GetCountOfCategory(const char* categoryName) const
{
	IItemSystem* pItemSystem = gEnv->pGameFramework->GetIItemSystem();

	int count = 0;
	TInventoryCIt end = m_stats.slots.end();
	for (TInventoryCIt it = m_stats.slots.begin(); it != end; ++it)
	{
		IItem* pItem = pItemSystem->GetItem(*it);
		if (pItem)
		{
			const char* cat = pItemSystem->GetItemCategory(pItem->GetEntity()->GetClass()->GetName());
			if (!strcmp(cat, categoryName))
				count++;
		}

	}
	TInventoryVectorEx::const_iterator endEx = m_stats.accessorySlots.end();
	for (TInventoryVectorEx::const_iterator cit = m_stats.accessorySlots.begin(); cit != endEx; ++cit)
	{
		const char* cat = pItemSystem->GetItemCategory((*cit)->GetName());
		if (!strcmp(cat, categoryName))
			count++;
	}

	return count;
}

//------------------------------------------------------------------------
int CInventory::GetCountOfUniqueId(uint8 uniqueId) const
{
	//Skip uniqueId 0
	if (!uniqueId)
		return 0;

	IItemSystem* pItemSystem = gEnv->pGameFramework->GetIItemSystem();

	int count = 0;
	TInventoryCIt end = m_stats.slots.end();
	for (TInventoryCIt it = m_stats.slots.begin(); it != end; ++it)
	{
		IItem* pItem = pItemSystem->GetItem(*it);
		if (pItem)
		{
			uint8 id = pItemSystem->GetItemUniqueId(pItem->GetEntity()->GetClass()->GetName());
			if (id == uniqueId)
				count++;
		}

	}

	TInventoryVectorEx::const_iterator endEx = m_stats.accessorySlots.end();
	for (TInventoryVectorEx::const_iterator cit = m_stats.accessorySlots.begin(); cit != endEx; ++cit)
	{
		uint8 id = pItemSystem->GetItemUniqueId((*cit)->GetName());
		if (id == uniqueId)
			count++;
	}

	return count;
}

//------------------------------------------------------------------------
int CInventory::GetSlotCount(int slotId) const
{
	return m_stats.slotsInfo[slotId].count;
}

//------------------------------------------------------------------------
EntityId CInventory::GetItem(int slotId) const
{
	if (slotId < 0 || slotId >= m_stats.slots.size())
		return 0;
	return m_stats.slots[slotId];
}

//------------------------------------------------------------------------
const char* CInventory::GetItemString(int slotId) const
{
	if (slotId < 0 || slotId >= m_stats.slots.size())
		return "";

	EntityId ItemId = GetItem(slotId);
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(ItemId);
	if (pEntity)
		return pEntity->GetClass()->GetName();
	else
		return "";
}

//------------------------------------------------------------------------
EntityId CInventory::GetItemByClass(IEntityClass* pClass, IItem* pIgnoreItem) const
{
	if (!pClass)
		return 0;

	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;

	TInventoryCIt end = m_stats.slots.end();
	for (TInventoryCIt it = m_stats.slots.begin(); it != end; ++it)
	{
		if (IEntity* pEntity = pEntitySystem->GetEntity(*it))
			if (pEntity->GetClass() == pClass)
				if (!pIgnoreItem || pIgnoreItem->GetEntity() != pEntity)
					return *it;
	}

	return 0;
}

//------------------------------------------------------------------------
IItem* CInventory::GetItemByName(const char* name) const
{
	if (!name)
		return 0;

	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;

	TInventoryCIt end = m_stats.slots.end();
	for (TInventoryCIt it = m_stats.slots.begin(); it != end; ++it)
	{
		if (IEntity* pEntity = pEntitySystem->GetEntity(*it))
			if (!strcmp(pEntity->GetName(), name))
				return gEnv->pGameFramework->GetIItemSystem()->GetItem(pEntity->GetId());
	}

	return 0;
}

//------------------------------------------
int CInventory::GetAccessoryCount() const
{
	return m_stats.accessorySlots.size();
}

//----------------------------------------
const char* CInventory::GetAccessory(int slotId) const
{
	if (slotId >= 0 && slotId < m_stats.accessorySlots.size())
	{
		return m_stats.accessorySlots[slotId]->GetName();
	}

	return NULL;
}

//----------------------------------------
const IEntityClass* CInventory::GetAccessoryClass(int slotId) const
{
	if (slotId >= 0 && slotId < m_stats.accessorySlots.size())
	{
		return m_stats.accessorySlots[slotId];
	}

	return NULL;
}

//----------------------------------------
bool CInventory::HasAccessory(IEntityClass* pClass) const
{
	int size = m_stats.accessorySlots.size();
	for (int i = 0; i < size; i++)
	{
		if (m_stats.accessorySlots[i] == pClass)
			return true;
	}

	return false;
}

//-------------------------------------------------------------------
bool CInventory::AddAccessory(IEntityClass* accessoryClass)
{
	if (accessoryClass == NULL)
		return false;

	if (GetAccessorySlotIndex(accessoryClass) > -1)
		return true;

	m_stats.accessorySlots.push_back(accessoryClass);
	std::sort(m_stats.accessorySlots.begin(), m_stats.accessorySlots.end(), compare_class_slots());

	TListenerVec::iterator iter = m_listeners.begin();
	m_iteratingListeners = true;
	while (iter != m_listeners.end())
	{
		(*iter)->OnAddAccessory(accessoryClass);
		++iter;
	}
	m_iteratingListeners = false;

	return true;
}

//-------------------------------------------------------------------
int CInventory::GetAccessorySlotIndex(IEntityClass* accessoryClass) const
{
	for (int i = 0; i < m_stats.accessorySlots.size(); i++)
	{
		if (m_stats.accessorySlots[i] == accessoryClass)
			return i;
	}
	return -1;
}

//------------------------------------------------------------------------
int CInventory::FindItem(EntityId itemId) const
{
	for (int i = 0; i < m_stats.slots.size(); i++)
	{
		if (m_stats.slots[i] == itemId)
			return i;
	}
	return -1;
}

//------------------------------------------------------------------------
int CInventory::FindNext(IEntityClass* pClass, const char* category, int firstSlot, bool wrap) const
{
	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
	for (int i = (firstSlot > -1) ? firstSlot + 1 : 0; i < m_stats.slots.size(); i++)
	{
		IEntity* pEntity = pEntitySystem->GetEntity(m_stats.slots[i]);
		bool ok = true;
		if (pEntity->GetClass() != pClass)
			ok = false;
		if (ok && category && category[0] && strcmp(m_pGameFrameWork->GetIItemSystem()->GetItemCategory(pEntity->GetClass()->GetName()), category))
			ok = false;

		if (ok)
			return i;
	}

	if (wrap && firstSlot > 0)
	{
		for (int i = 0; i < firstSlot; i++)
		{
			IEntity* pEntity = pEntitySystem->GetEntity(m_stats.slots[i]);
			bool ok = true;
			if (pEntity->GetClass() != pClass)
				ok = false;
			if (ok && category && category[0] && strcmp(m_pGameFrameWork->GetIItemSystem()->GetItemCategory(pEntity->GetClass()->GetName()), category))
				ok = false;

			if (ok)
				return i;
		}
	}

	return -1;
}

//------------------------------------------------------------------------
int CInventory::FindPrev(IEntityClass* pClass, const char* category, int firstSlot, bool wrap) const
{
	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
	for (int i = (firstSlot > -1) ? firstSlot + 1 : 0; i < m_stats.slots.size(); i++)
	{
		IEntity* pEntity = pEntitySystem->GetEntity(m_stats.slots[firstSlot - i]);
		bool ok = true;
		if (pEntity->GetClass() != pClass)
			ok = false;
		if (ok && category && category[0] && strcmp(m_pGameFrameWork->GetIItemSystem()->GetItemCategory(pEntity->GetClass()->GetName()), category))
			ok = false;

		if (ok)
			return i;
	}

	if (wrap && firstSlot > 0)
	{
		int count = GetCount();
		for (int i = 0; i < firstSlot; i++)
		{
			IEntity* pEntity = pEntitySystem->GetEntity(m_stats.slots[count - i + firstSlot]);
			bool ok = true;
			if (pEntity->GetClass() != pClass)
				ok = false;
			if (ok && category && category[0] && strcmp(m_pGameFrameWork->GetIItemSystem()->GetItemCategory(pEntity->GetClass()->GetName()), category))
				ok = false;

			if (ok)
				return i;
		}
	}

	return -1;
}

//------------------------------------------------------------------------
EntityId CInventory::GetHolsteredItem() const
{
	return m_stats.holsteredItemId;
}

//------------------------------------------------------------------------
EntityId CInventory::GetCurrentItem() const
{
	return m_stats.currentItemId;
}

//------------------------------------------------------------------------
void CInventory::SetCurrentItem(EntityId itemId)
{
	m_stats.currentItemId = itemId;

	SetLastSelectedInSlot(itemId);
}

//------------------------------------------------------------------------
void CInventory::SetLastItem(EntityId itemId)
{
	m_stats.lastItemId = itemId;
}

//------------------------------------------------------------------------
EntityId CInventory::GetLastItem() const
{
	if (FindItem(m_stats.lastItemId) > -1)
		return m_stats.lastItemId;

	return 0;
}

//------------------------------------------------------------------------
void CInventory::HolsterItem(bool holster)
{
	//CryLogAlways("%s::HolsterItem(%s)", GetEntity()->GetName(), holster?"true":"false");

	if (!holster)
	{
		if (m_stats.holsteredItemId)
		{
			IItem* pItem = m_pGameFrameWork->GetIItemSystem()->GetItem(m_stats.holsteredItemId);

			if (pItem && pItem->CanSelect())
			{
				m_pGameFrameWork->GetIItemSystem()->SetActorItem(GetActor(), m_stats.holsteredItemId, false);
			}
			else
			{
				m_pGameFrameWork->GetIItemSystem()->SetActorItem(GetActor(), GetLastItem(), false);
			}
		}

		m_stats.holsteredItemId = 0;
	}
	else if (m_stats.currentItemId && (!m_stats.holsteredItemId || m_stats.holsteredItemId == m_stats.currentItemId))
	{
		m_stats.holsteredItemId = m_stats.currentItemId;
		m_pGameFrameWork->GetIItemSystem()->SetActorItem(GetActor(), (EntityId)0, false);
	}
}

//------------------------------------------------------------------------
int CInventory::GetAmmoTypesCount() const
{
	return m_stats.ammoInfo.size();
}

//------------------------------------------------------------------------
IEntityClass* CInventory::GetAmmoType(int idx) const
{
	CRY_ASSERT(idx < m_stats.ammoInfo.size());
	TAmmoInfoMap::const_iterator it = m_stats.ammoInfo.begin();
	std::advance(it, idx);
	return it->first;
}

//------------------------------------------------------------------------
void CInventory::SetAmmoCount(IEntityClass* pAmmoType, int count)
{
	int capacity = GetAmmoCapacity(pAmmoType);
	CRY_ASSERT(pAmmoType);
	if (pAmmoType)
	{
		if (capacity > 0)
			m_stats.ammoInfo[pAmmoType].SetCount(min(count, capacity));
		else
			m_stats.ammoInfo[pAmmoType].SetCount(count);
	}

	TListenerVec::iterator iter = m_listeners.begin();
	m_iteratingListeners = true;
	while (iter != m_listeners.end())
	{
		(*iter)->OnSetAmmoCount(pAmmoType, count);
		++iter;
	}
	m_iteratingListeners = false;
}

//------------------------------------------------------------------------
int CInventory::GetAmmoCount(IEntityClass* pAmmoType) const
{
	TAmmoInfoMap::const_iterator it = m_stats.ammoInfo.find(pAmmoType);
	if (it != m_stats.ammoInfo.end())
		return it->second.GetCount();

	return 0;
}

//------------------------------------------------------------------------
void CInventory::SetAmmoCapacity(IEntityClass* pAmmoType, int max)
{
	//
	//CryLog("%s::CInventory::SetAmmoCapacity(%s,%d)", GetEntity()->GetName(),pAmmoType->GetName(), max);
	//
	if (pAmmoType)
	{
		m_stats.ammoInfo[pAmmoType].SetCapacity(max);

		if (GetAmmoCount(pAmmoType) > max)
		{
			SetAmmoCount(pAmmoType, max);
		}
	}
}

//------------------------------------------------------------------------
int CInventory::GetAmmoCapacity(IEntityClass* pAmmoType) const
{
	TAmmoInfoMap::const_iterator it = m_stats.ammoInfo.find(pAmmoType);
	if (it != m_stats.ammoInfo.end())
		return it->second.GetCapacity();

	return 0;
}

//------------------------------------------------------------------------
void CInventory::ResetAmmo()
{
	TAmmoInfoMap::iterator ammoInfoEndIt = m_stats.ammoInfo.end();
	for (TAmmoInfoMap::iterator ammoInfoIt = m_stats.ammoInfo.begin(); ammoInfoIt != ammoInfoEndIt; ++ammoInfoIt)
	{
		ammoInfoIt->second.ResetCount();
	}
}

//------------------------------------------------------------------------
void CInventory::SetInventorySlotCapacity(IInventory::EInventorySlots slotId, unsigned int capacity)
{
	CRY_ASSERT_MESSAGE(((slotId >= 0) && (slotId < IInventory::eInventorySlot_Last)), "Invalid inventory slot!");

	m_stats.slotsInfo[slotId].maxCapacity = capacity;
}

//------------------------------------------------------------------------
void CInventory::AssociateItemCategoryToSlot(const char* itemCategory, IInventory::EInventorySlots slotId)
{
	CRY_ASSERT_MESSAGE(((slotId >= 0) && (slotId < IInventory::eInventorySlot_Last)), "Invalid inventory slot!");

	m_stats.categoriesToSlot.insert(TCategoriesToSlot::value_type(itemCategory, slotId));
}

//------------------------------------------------------------------------
EntityId CInventory::GetLastSelectedInSlot(IInventory::EInventorySlots slotId) const
{
	CRY_ASSERT_MESSAGE(((slotId >= 0) && (slotId < IInventory::eInventorySlot_Last)), "Invalid inventory slot!");

	return m_stats.slotsInfo[slotId].lastSelected;
}

//------------------------------------------------------------------------
void CInventory::SetLastSelectedInSlot(EntityId entityId)
{
	EInventorySlots slot = GetSlotFromEntityID(entityId);
	if (slot < eInventorySlot_Last)
	{
		m_stats.slotsInfo[slot].lastSelected = entityId;
	}
}

//------------------------------------------------------------------------
void CInventory::AddItemToCategorySlot(EntityId entityId)
{
	EInventorySlots slot = GetSlotFromEntityID(entityId);
	if (slot < eInventorySlot_Last)
	{
		m_stats.slotsInfo[slot].count++;

		if ((m_stats.slotsInfo[slot].lastSelected == 0) || (gEnv->pEntitySystem->GetEntity(m_stats.slotsInfo[slot].lastSelected) == NULL))
		{
			m_stats.slotsInfo[slot].lastSelected = entityId;
		}
	}
}

//------------------------------------------------------------------------
void CInventory::RemoveItemFromCategorySlot(EntityId entityId)
{
	EInventorySlots slot = GetSlotFromEntityID(entityId);
	if (slot < eInventorySlot_Last)
	{
		m_stats.slotsInfo[slot].count--;
		if ((m_stats.slotsInfo[slot].count > 0) && (entityId == m_stats.slotsInfo[slot].lastSelected))
		{

			const EntityId entityID = GetAnyEntityInSlot(slot);
			if (entityID != 0)
			{
				m_stats.slotsInfo[slot].lastSelected = entityID;
			}
		}
		else
		{
			m_stats.slotsInfo[slot].lastSelected = 0;
		}
	}
}

//------------------------------------------------------------------------
bool CInventory::IsAvailableSlotForItemClass(const char* itemClass) const
{
	const char* category = m_pGameFrameWork->GetIItemSystem()->GetItemCategory(itemClass);

	//If not category info, assume there is space
	if (!category || category[0] == '\0')
		return true;

	return IsAvailableSlotForItemCategory(category);
}

//------------------------------------------------------------------------
bool CInventory::IsAvailableSlotForItemCategory(const char* itemCategory) const
{
	TCategoriesToSlot::const_iterator catToSlotCit = m_stats.categoriesToSlot.find(CONST_TEMP_STRING(itemCategory));
	if (catToSlotCit == m_stats.categoriesToSlot.end())
		return true;

	const SSlotInfo& slotInfo = m_stats.slotsInfo[catToSlotCit->second];

	return (slotInfo.maxCapacity > slotInfo.count);
}

//------------------------------------------------------------------------
bool CInventory::AreItemsInSameSlot(const char* itemClass1, const char* itemClass2) const
{
	const char* category1 = m_pGameFrameWork->GetIItemSystem()->GetItemCategory(itemClass1);
	if (!category1 || category1[0] == '\0')
		return false;

	const char* category2 = m_pGameFrameWork->GetIItemSystem()->GetItemCategory(itemClass2);
	if (!category2 || category2[0] == '\0')
		return false;

	TCategoriesToSlot::const_iterator cit1 = m_stats.categoriesToSlot.find(CONST_TEMP_STRING(category1));
	TCategoriesToSlot::const_iterator cit2 = m_stats.categoriesToSlot.find(CONST_TEMP_STRING(category2));
	TCategoriesToSlot::const_iterator end = m_stats.categoriesToSlot.end();

	return ((cit1 != end) && (cit2 != end) && (cit1->second == cit2->second));
}

//----------------------------------------------------------------------
IInventory::EInventorySlots CInventory::GetSlotForItemCategory(const char* itemCategory) const
{
	TCategoriesToSlot::const_iterator cit1 = m_stats.categoriesToSlot.find(CONST_TEMP_STRING(itemCategory));

	return (cit1 != m_stats.categoriesToSlot.end()) ? cit1->second : IInventory::eInventorySlot_Last;
}

//------------------------------------------------------------------------
// client side call for RemoveAllItems
void CInventory::RMIReqToServer_RemoveAllItems() const
{
	TRMIInventory_Dummy Info;

	GetGameObject()->InvokeRMI(SvReq_RemoveAllItems(), Info, eRMI_ToServer);
}

// RMI receiver in the server to remove all items from the inventory. changes are automatically propagated to the clients
IMPLEMENT_RMI(CInventory, SvReq_RemoveAllItems)
{
	IItemSystem* pItemSystem = CCryAction::GetCryAction()->GetIItemSystem();

	IItem* pItem = pItemSystem->GetItem(GetCurrentItem());
	if (pItem)
	{
		pItem->Select(false);
		pItemSystem->SetActorItem(GetActor(), (EntityId)0, false);
	}

	Destroy();

	if (gEnv->bMultiplayer)
	{
		TRMIInventory_Dummy Info;
		GetGameObject()->InvokeRMI(Cl_RemoveAllAmmo(), Info, eRMI_ToAllClients);
	}
	else
	{
		ResetAmmo();
	}

	return true;
}

//------------------------------------------------------------------------
// client side call for AddItem
void CInventory::RMIReqToServer_AddItem(const char* _pszItemClass) const
{
	TRMIInventory_Item Info(_pszItemClass);

	GetGameObject()->InvokeRMI(SvReq_AddItem(), Info, eRMI_ToServer);
}

// RMI receiver in the server to add an item to the inventory. change is automatically propagated to the clients
IMPLEMENT_RMI(CInventory, SvReq_AddItem)
{
	TRMIInventory_Item Info(params);

	gEnv->pGameFramework->GetIItemSystem()->GiveItem(GetActor(), Info.m_ItemClass.c_str(), false, true, true);

	return true;
}

//------------------------------------------------------------------------
// client side call for RemoveItem
void CInventory::RMIReqToServer_RemoveItem(const char* _pszItemClass) const
{
	TRMIInventory_Item Info(_pszItemClass);

	GetGameObject()->InvokeRMI(SvReq_RemoveItem(), Info, eRMI_ToServer);
}

// RMI receiver in the server to remove an item from the inventory. change is automatically propagated to the clients
IMPLEMENT_RMI(CInventory, SvReq_RemoveItem)
{
	TRMIInventory_Item Info(params);

	IItemSystem* pItemSystem = CCryAction::GetCryAction()->GetIItemSystem();

	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(Info.m_ItemClass.c_str());
	if (pClass)
	{
		IItem* pItem = pItemSystem->GetItem(GetItemByClass(pClass));
		if (pItem && pItem->GetEntityId() == GetCurrentItem())
		{
			pItem->Select(false);
			pItemSystem->SetActorItem(GetActor(), (EntityId)0, false);
		}

		if (pItem)
			gEnv->pEntitySystem->RemoveEntity(pItem->GetEntityId());
	}
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CInventory, Cl_SetAmmoCapacity)
{
	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(params.m_AmmoClass.c_str());
	CRY_ASSERT(pClass);

	SetAmmoCapacity(pClass, params.m_iAmount);

	return true;
}

//------------------------------------------------------------------------
// client side call for SetAmmoCount
void CInventory::RMIReqToServer_SetAmmoCount(const char* _pszAmmoClass, int _iAmount) const
{
	TRMIInventory_Ammo Info(_pszAmmoClass, _iAmount);

	GetGameObject()->InvokeRMI(SvReq_SetAmmoCount(), Info, eRMI_ToServer);
}

// RMI receiver in the server to set the ammo count for an ammo class in the inventory.
IMPLEMENT_RMI(CInventory, SvReq_SetAmmoCount)
{
	TRMIInventory_Ammo Info(params);

	GetGameObject()->InvokeRMI(Cl_SetAmmoCount(), Info, eRMI_ToAllClients);
	return true;
}

// RMI receiver in clients for SetAmmoCount. This is needed because ammo changes are not automatically propagated
IMPLEMENT_RMI(CInventory, Cl_SetAmmoCount)
{
	TRMIInventory_Ammo Info(params);

	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(Info.m_AmmoClass.c_str());
	if (pClass)
		SetAmmoCount(pClass, Info.m_iAmount);
	return true;
}

// RMI receiver in clients for RemoveAllAmmo. This is needed because ammo changes are not automatically propagated
IMPLEMENT_RMI(CInventory, Cl_RemoveAllAmmo)
{
	ResetAmmo();
	return true;
}

//------------------------------------------------------------------------
// client side call for AddEquipmentPack
void CInventory::RMIReqToServer_AddEquipmentPack(const char* _pszEquipmentPack, bool _bAdd, bool _bPrimary) const
{
	TRMIInventory_EquipmentPack Info(_pszEquipmentPack, _bAdd, _bPrimary);

	GetGameObject()->InvokeRMI(SvReq_AddEquipmentPack(), Info, eRMI_ToServer);
}

// RMI receiver in the server to add an equipment pack to the inventory. change is automatically propagated to the clients
IMPLEMENT_RMI(CInventory, SvReq_AddEquipmentPack)
{
	TRMIInventory_EquipmentPack Info(params);

	CCryAction::GetCryAction()->GetIItemSystem()->GetIEquipmentManager()->GiveEquipmentPack(GetActor(), Info.m_EquipmentPack.c_str(), Info.m_bAdd, Info.m_bPrimary);

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CInventory::AddAmmoUser(IEntityClass* pAmmoType)
{
	CRY_ASSERT(pAmmoType);

	if (pAmmoType)
	{
		m_stats.ammoInfo[pAmmoType].AddUser();
	}
}

//////////////////////////////////////////////////////////////////////////
void CInventory::RemoveAmmoUser(IEntityClass* pAmmoType)
{
	CRY_ASSERT(pAmmoType);

	TAmmoInfoMap::iterator ammoUserIt = m_stats.ammoInfo.find(pAmmoType);

	CRY_ASSERT_MESSAGE(ammoUserIt != m_stats.ammoInfo.end(), "Trying to remove user of an ammo which never was added.");

	if (ammoUserIt != m_stats.ammoInfo.end())
	{
		ammoUserIt->second.RemoveUser();
	}
}

//////////////////////////////////////////////////////////////////////////
int CInventory::GetNumberOfUsersForAmmo(IEntityClass* pAmmoType) const
{
	TAmmoInfoMap::const_iterator ammoUserCit = m_stats.ammoInfo.find(pAmmoType);
	if (ammoUserCit != m_stats.ammoInfo.end())
	{
		return ammoUserCit->second.GetUserCount();
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CInventory::ResetAmmoAndUsers()
{
	m_stats.ammoInfo.clear();
}

void CInventory::AddListener(struct IInventoryListener* pListener)
{
	CRY_ASSERT_MESSAGE(m_iteratingListeners == false, "AddListener called during a listener broadcast.");
	stl::push_back_unique(m_listeners, pListener);
}

void CInventory::RemoveListener(struct IInventoryListener* pListener)
{
	CRY_ASSERT_MESSAGE(m_iteratingListeners == false, "RemoveListener called during a listener broadcast.");
	stl::find_and_erase(m_listeners, pListener);
}

//////////////////////////////////////////////////////////////////////////
void CInventory::SetHolsteredItem(EntityId itemId)
{
	m_stats.holsteredItemId = itemId;
}

//////////////////////////////////////////////////////////////////////////
void CInventory::IgnoreNextClear()
{
	m_ignoreNextClear = true;
}

//////////////////////////////////////////////////////////////////////////
CInventory::EInventorySlots CInventory::GetSlotFromEntityID(EntityId entityID) const
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityID);

	if (pEntity)
	{
		const char* category = m_pGameFrameWork->GetIItemSystem()->GetItemCategory(pEntity->GetClass()->GetName());

		if (!category || category[0] == '\0')
			return eInventorySlot_Last;

		TCategoriesToSlot::const_iterator catToSlotCit = m_stats.categoriesToSlot.find(CONST_TEMP_STRING(category));
		if (catToSlotCit == m_stats.categoriesToSlot.end())
			return eInventorySlot_Last;

		return catToSlotCit->second;
	}

	return eInventorySlot_Last;
}

EntityId CInventory::GetAnyEntityInSlot(int slot) const
{
	TInventoryCIt it = m_stats.slots.begin();
	const TInventoryCIt iEnd = m_stats.slots.end();
	for (; it != iEnd; ++it)
	{
		if (GetSlotFromEntityID(*it) == slot)
		{
			return *it;
		}
	}

	return 0;
}

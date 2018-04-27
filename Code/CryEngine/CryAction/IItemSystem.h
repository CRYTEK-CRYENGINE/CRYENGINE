// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Item System interfaces.

   -------------------------------------------------------------------------
   History:
   - 1:9:2004   19:52 : Created by Márcio Martins
   - 20:6:2005        : Changed IItem to be a game object extension by Craig Tiller

*************************************************************************/

#ifndef __IITEMSYSTEM_H__
#define __IITEMSYSTEM_H__

#pragma once

#include "IGameObject.h"
#include "IItem.h"
#include "IWeapon.h"
#include "IActorSystem.h"
#include <CryFlowGraph/IFlowSystem.h>

enum EItemParamMapTypes
{
	eIPT_None   = -2,
	eIPT_Any    = -1,
	eIPT_Float  = EFlowDataTypes::eFDT_Float,
	eIPT_Int    = EFlowDataTypes::eFDT_Int,
	eIPT_Vec3   = EFlowDataTypes::eFDT_Vec3,
	eIPT_String = EFlowDataTypes::eFDT_String,
};

struct IItemParamsNode
{
	virtual ~IItemParamsNode() {}

	virtual void                   AddRef() const = 0;
	virtual uint32                 GetRefCount() const = 0;
	virtual void                   Release() const = 0;

	virtual int                    GetAttributeCount() const = 0;
	virtual const char*            GetAttributeName(int i) const = 0;
	virtual const char*            GetAttribute(int i) const = 0;
	virtual bool                   GetAttribute(int i, Vec3& attr) const = 0;
	virtual bool                   GetAttribute(int i, Ang3& attr) const = 0;
	virtual bool                   GetAttribute(int i, float& attr) const = 0;
	virtual bool                   GetAttribute(int i, int& attr) const = 0;
	virtual int                    GetAttributeType(int i) const = 0;

	virtual const char*            GetAttribute(const char* name) const = 0;
	virtual bool                   GetAttribute(const char* name, Vec3& attr) const = 0;
	virtual bool                   GetAttribute(const char* name, Ang3& attr) const = 0;
	virtual bool                   GetAttribute(const char* name, float& attr) const = 0;
	virtual bool                   GetAttribute(const char* name, int& attr) const = 0;
	virtual int                    GetAttributeType(const char* name) const = 0;

	virtual const char*            GetAttributeSafe(const char* name) const = 0;

	virtual const char*            GetNameAttribute() const = 0;

	virtual int                    GetChildCount() const = 0;
	virtual const char*            GetChildName(int i) const = 0;
	virtual const IItemParamsNode* GetChild(int i) const = 0;
	virtual const IItemParamsNode* GetChild(const char* name) const = 0;

	virtual void                   SetAttribute(const char* name, const char* attr) = 0;
	virtual void                   SetAttribute(const char* name, const Vec3& attr) = 0;
	virtual void                   SetAttribute(const char* name, float attr) = 0;
	virtual void                   SetAttribute(const char* name, int attr) = 0;

	virtual void                   SetName(const char* name) = 0;
	virtual const char*            GetName() const = 0;

	virtual IItemParamsNode*       InsertChild(const char* name) = 0;
	virtual void                   ConvertFromXML(const XmlNodeRef& root) = 0;
	virtual bool                   ConvertFromXMLWithFiltering(const XmlNodeRef& root, const char* keepWithThisAttrValue) = 0;

	virtual void                   GetMemoryUsage(ICrySizer* pSizer) const = 0;

	void                           Dump(const char* name = 0) const
	{
		int ident = 0;
		DumpNode(this, name ? name : "root", ident);
	}

private:
	void DumpNode(const IItemParamsNode* node, const char* name, int& ident) const
	{
		string dump;
		dump.reserve(10000);
		for (int i = 0; i < ident; i++)
			dump += " ";

		dump += "<";
		dump += name;

		char snum[64];

		for (int a = 0; a < node->GetAttributeCount(); a++)
		{
			const char* attrName = node->GetAttributeName(a);
			dump += " ";
			dump += attrName;
			dump += "=\"";

			switch (node->GetAttributeType(a))
			{
			case eIPT_Int:
				{
					int attr;
					node->GetAttribute(a, attr);
					cry_sprintf(snum, "%d", attr);
					dump += snum;
				}
				break;
			case eIPT_Float:
				{
					float attr;
					node->GetAttribute(a, attr);
					cry_sprintf(snum, "%f", attr);
					dump += snum;
				}
				break;
			case eIPT_String:
				{
					const char* attr = node->GetAttribute(a);
					dump += attr;
				}
				break;
			case eIPT_Vec3:
				{
					Vec3 attr;
					node->GetAttribute(a, attr);
					cry_sprintf(snum, "%f,%f,%f", attr.x, attr.y, attr.z);
					dump += snum;
				}
				break;
			default:
				break;
			}

			dump += "\"";
		}
		if (node->GetChildCount())
		{
			dump += ">";
			CryLogAlways("%s", dump.c_str());

			ident += 2;
			for (int c = 0; c < node->GetChildCount(); c++)
			{
				const char* childName = node->GetChildName(c);
				const IItemParamsNode* child = node->GetChild(c);
				DumpNode(child, childName, ident);
			}
			ident -= 2;

			string finale;
			for (int i = 0; i < ident; i++)
				finale += " ";
			finale += "</";
			finale += name;
			finale += ">";
			CryLogAlways("%s", finale.c_str());
		}
		else
		{
			dump += " />";
			CryLogAlways("%s", dump.c_str());
		}
	}
};

struct IInventory : public IGameObjectExtension
{
	enum EInventorySlots
	{
		eInventorySlot_Weapon = 0,
		eInventorySlot_Explosives,
		eInventorySlot_Grenades,
		eInventorySlot_Special,
		eInventorySlot_Last
	};

	virtual bool                AddItem(EntityId id) = 0;
	virtual bool                RemoveItem(EntityId id) = 0;
	virtual void                RemoveAllItems(bool forceClear = false) = 0;

	virtual bool                AddAccessory(IEntityClass* itemClass) = 0;

	virtual void                Destroy() = 0;
	virtual void                Clear(bool forceClear = false) = 0;

	virtual void                RMIReqToServer_RemoveAllItems() const = 0;
	virtual void                RMIReqToServer_AddItem(const char* _pszItemClass) const = 0;
	virtual void                RMIReqToServer_RemoveItem(const char* _pszItemClass) const = 0;
	virtual void                RMIReqToServer_SetAmmoCount(const char* _pszAmmoClass, int _iAmount) const = 0;
	virtual void                RMIReqToServer_AddEquipmentPack(const char* _pszEquipmentPack, bool _bAdd, bool _bPrimary) const = 0;

	virtual int                 GetCapacity() const = 0;
	virtual int                 GetCount() const = 0;
	virtual int                 GetCountOfClass(const char* className) const = 0;
	virtual int                 GetCountOfCategory(const char* categoryName) const = 0;
	virtual int                 GetCountOfUniqueId(uint8 uniqueId) const = 0;

	virtual int                 GetSlotCount(int slotId) const = 0;

	virtual EntityId            GetItem(int slotId) const = 0;
	virtual const char*         GetItemString(int slotId) const = 0;
	virtual EntityId            GetItemByClass(IEntityClass* pClass, IItem* pIgnoreItem = NULL) const = 0;
	virtual IItem*              GetItemByName(const char* name) const = 0;

	virtual int                 GetAccessoryCount() const = 0;
	virtual const char*         GetAccessory(int slotId) const = 0;
	virtual const IEntityClass* GetAccessoryClass(int slotId) const = 0;
	virtual bool                HasAccessory(IEntityClass* pClass) const = 0;

	virtual int                 FindItem(EntityId itemId) const = 0;

	virtual int                 FindNext(IEntityClass* pClass, const char* category, int firstSlot, bool wrap) const = 0;
	virtual int                 FindPrev(IEntityClass* pClass, const char* category, int firstSlot, bool wrap) const = 0;

	virtual EntityId            GetCurrentItem() const = 0;
	virtual EntityId            GetHolsteredItem() const = 0;
	virtual void                SetCurrentItem(EntityId itemId) = 0;
	virtual void                SetHolsteredItem(EntityId itemId) = 0;

	virtual void                SetLastItem(EntityId itemId) = 0;
	virtual EntityId            GetLastItem() const = 0;
	virtual EntityId            GetLastSelectedInSlot(IInventory::EInventorySlots slotId) const = 0;

	virtual void                HolsterItem(bool holster) = 0;

	virtual void                SerializeInventoryForLevelChange(TSerialize ser) = 0;
	virtual bool                IsSerializingForLevelChange() const = 0;

	virtual int                 GetAmmoTypesCount() const = 0;
	virtual IEntityClass*       GetAmmoType(int idx) const = 0;

	virtual void                SetAmmoCount(IEntityClass* pAmmoType, int count) = 0;
	virtual int                 GetAmmoCount(IEntityClass* pAmmoType) const = 0;
	virtual void                SetAmmoCapacity(IEntityClass* pAmmoType, int max) = 0;
	virtual int                 GetAmmoCapacity(IEntityClass* pAmmoType) const = 0;
	virtual void                ResetAmmo() = 0;

	virtual void                AddAmmoUser(IEntityClass* pAmmoType) = 0;
	virtual void                RemoveAmmoUser(IEntityClass* pAmmoType) = 0;
	virtual int                 GetNumberOfUsersForAmmo(IEntityClass* pAmmoType) const = 0;

	virtual IActor*             GetActor() = 0;

	virtual void                SetInventorySlotCapacity(EInventorySlots slotId, unsigned int capacity) = 0;
	virtual void                AssociateItemCategoryToSlot(const char* itemCategory, EInventorySlots slotId) = 0;
	virtual bool                IsAvailableSlotForItemClass(const char* itemClass) const = 0;
	virtual bool                IsAvailableSlotForItemCategory(const char* itemCategory) const = 0;
	virtual bool                AreItemsInSameSlot(const char* itemClass1, const char* itemClass2) const = 0;
	virtual EInventorySlots     GetSlotForItemCategory(const char* itemCategory) const = 0;

	virtual void                AddListener(struct IInventoryListener* pListener) = 0;
	virtual void                RemoveListener(struct IInventoryListener* pListener) = 0;

	virtual void                IgnoreNextClear() = 0;
};

struct IInventoryListener
{
	virtual ~IInventoryListener(){}
	virtual void OnAddItem(EntityId entityId) = 0;
	virtual void OnSetAmmoCount(IEntityClass* pAmmoType, int count) = 0;
	virtual void OnAddAccessory(IEntityClass* pAccessoryClass) = 0;
	virtual void OnClearInventory() = 0;
};

struct IEquipmentPackPreCacheCallback
{
	virtual ~IEquipmentPackPreCacheCallback(){}
	virtual void PreCacheItemResources(const char* itemName) = 0;
};

// Summary
//   Used to give predefined inventory to actors
struct IEquipmentManager
{
	virtual ~IEquipmentManager(){}
	struct IListener
	{
		virtual ~IListener(){}
		virtual void OnBeginGiveEquipmentPack() {}
		virtual void OnEndGiveEquipmentPack()   {}
	};

	struct IEquipmentPackIterator
	{
		virtual ~IEquipmentPackIterator(){}
		virtual void        AddRef() = 0;
		virtual void        Release() = 0;
		virtual int         GetCount() = 0;
		virtual const char* Next() = 0;
	};
	typedef _smart_ptr<IEquipmentPackIterator> IEquipmentPackIteratorPtr;

	// Clear all equipment packs
	virtual void DeleteAllEquipmentPacks() = 0;

	// Loads equipment packs from rootNode
	virtual void LoadEquipmentPacks(const XmlNodeRef& rootNode) = 0;

	// Load all equipment packs from a certain path
	virtual void LoadEquipmentPacksFromPath(const char* path) = 0;

	// Load an equipment pack from an XML node
	virtual bool LoadEquipmentPack(const XmlNodeRef& rootNode, bool bOverrideExisiting = true) = 0;

	// Give an equipment pack (resp. items/ammo) to an actor
	virtual bool GiveEquipmentPack(IActor* pActor, const char* packName, bool bAdd, bool bSelectPrimary) = 0;

	// Pre-cache all resources needed for the items included in the given pack
	virtual void PreCacheEquipmentPackResources(const char* packName, IEquipmentPackPreCacheCallback& preCacheCallback) = 0;

	// return iterator with all available equipment packs
	virtual IEquipmentManager::IEquipmentPackIteratorPtr CreateEquipmentPackIterator() = 0;

	virtual void RegisterListener(IListener* pListener) = 0;
	virtual void UnregisterListener(IListener* pListener) = 0;
};

struct IActor;

struct IItemSystemListener
{
	virtual ~IItemSystemListener(){}
	virtual void OnSetActorItem(IActor* pActor, IItem* pItem) = 0;
	virtual void OnDropActorItem(IActor* pActor, IItem* pItem) = 0;
	virtual void OnSetActorAccessory(IActor* pActor, IItem* pItem) = 0;
	virtual void OnDropActorAccessory(IActor* pActor, IItem* pItem) = 0;

	virtual void GetMemoryUsage(ICrySizer* pSizer) const {};
};

// Summary
// Query that item system can handle
enum IItemSystemQuery
{
	eISQ_Dump_Item_Names = 0,
	eISQ_Find_Item_By_Name,
	eISQ_Last,
};

// Summary
//   Interface to the Item system
struct IItemSystem
{
	virtual ~IItemSystem(){}
	virtual void                   Reset() = 0;
	virtual void                   Reload() = 0;
	virtual void                   PreReload() = 0;
	virtual void                   PostReload() = 0;
	virtual void                   Scan(const char* folderName) = 0;
	virtual IItemParamsNode*       CreateParams() = 0;
	virtual const IItemParamsNode* GetItemParams(const char* itemName) const = 0;
	virtual const char*            GetItemParamsDescriptionFile(const char* itemName) const = 0;
	virtual int                    GetItemParamsCount() const = 0;
	virtual const char*            GetItemParamName(int index) const = 0;
	virtual uint8                  GetItemPriority(const char* item) const = 0;
	virtual const char*            GetItemCategory(const char* item) const = 0;
	virtual uint8                  GetItemUniqueId(const char* item) const = 0;

	virtual bool                   IsItemClass(const char* name) const = 0;
	virtual const char*            GetFirstItemClass() = 0;
	virtual const char*            GetNextItemClass() = 0;

	virtual void                   RegisterForCollection(EntityId itemId) = 0;
	virtual void                   UnregisterForCollection(EntityId itemId) = 0;

	virtual void                   AddItem(EntityId itemId, IItem* pItem) = 0;
	virtual void                   RemoveItem(EntityId itemId, const char* itemName = NULL) = 0;
	virtual IItem*                 GetItem(EntityId itemId) const = 0;

	virtual void                   SetConfiguration(const char* name) = 0;
	virtual const char*            GetConfiguration() const = 0;

	virtual void*                  Query(IItemSystemQuery query, const void* param = NULL) = 0;

	virtual ICharacterInstance*    GetCachedCharacter(const char* fileName) = 0;
	virtual IStatObj*              GetCachedObject(const char* fileName) = 0;
	virtual void                   CacheObject(const char* fileName, bool useCgfStreaming) = 0;
	virtual void                   CacheGeometry(const IItemParamsNode* geometry) = 0;
	virtual void                   CacheItemGeometry(const char* className) = 0;
	virtual void                   ClearGeometryCache() = 0;

	virtual void                   CacheItemSound(const char* className) = 0;
	virtual void                   ClearSoundCache() = 0;

	virtual void                   Serialize(TSerialize ser) = 0;

	virtual EntityId               GiveItem(IActor* pActor, const char* item, bool sound, bool select, bool keepHistory, const char* setup = NULL, EEntityFlags entityFlags = (EEntityFlags)0) = 0;
	virtual void                   SetActorItem(IActor* pActor, EntityId itemId, bool keepHistory = true) = 0;
	virtual void                   SetActorItem(IActor* pActor, const char* name, bool keepHistory = true) = 0;
	virtual void                   DropActorItem(IActor* pActor, EntityId itemId) = 0;
	virtual void                   SetActorAccessory(IActor* pActor, EntityId itemId, bool keepHistory = true) = 0;
	virtual void                   DropActorAccessory(IActor* pActor, EntityId itemId) = 0;

	virtual void                   RegisterListener(IItemSystemListener* pListener) = 0;
	virtual void                   UnregisterListener(IItemSystemListener* pListener) = 0;

	virtual IEquipmentManager*     GetIEquipmentManager() = 0;

	virtual uint32                 GetItemSocketCount(const char* item) const = 0;
	virtual const char*            GetItemSocketName(const char* item, int idx) const = 0;
	virtual bool                   IsCompatible(const char* item, const char* attachment) const = 0;
	virtual bool                   GetItemSocketCompatibility(const char* item, const char* socket) const = 0;
	virtual bool                   CanSocketBeEmpty(const char* item, const char* socket) const = 0;
};

#endif //__IITEMSYSTEM_H__

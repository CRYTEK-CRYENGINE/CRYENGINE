// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Manages items.

   -------------------------------------------------------------------------
   History:
   - 29:9:2004   18:01 : Created by Márcio Martins

*************************************************************************/
#ifndef __ITEMSYSTEM_H__
#define __ITEMSYSTEM_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryGame/IGameFramework.h>
#include "IItemSystem.h"
#include "ILevelSystem.h"
#include "ItemParams.h"
#include "EquipmentManager.h"

#include <CryCore/Containers/CryListenerSet.h>

#define DEFAULT_ITEM_SCRIPT     "Scripts/Entities/Items/Item.lua"
#define DEFAULT_ITEM_CREATEFUNC "CreateItemTable"
//#define USE_LTL_PRECACHING

//#if defined(USER_benito)
//#define ITEM_SYSTEM_DEBUG_MEMSTATS
//#endif

class CItemSystem :
	public ILevelSystemListener,
	public IItemSystem
{
public:
	CItemSystem(IGameFramework* pGameFramework, ISystem* pSystem);
	virtual ~CItemSystem();

	void Release() { delete this; };
	void Update();

	// IItemSystem
	virtual void                   Reload();
	virtual void                   Reset();
	virtual void                   Scan(const char* folderName);
	virtual IItemParamsNode*       CreateParams() { return new CItemParamsNode; };
	virtual const IItemParamsNode* GetItemParams(const char* itemName) const;
	virtual int                    GetItemParamsCount() const;
	virtual const char*            GetItemParamName(int index) const;
	virtual const char*            GetItemParamsDescriptionFile(const char* itemName) const;
	virtual uint8                  GetItemPriority(const char* item) const;
	virtual const char*            GetItemCategory(const char* item) const;
	virtual uint8                  GetItemUniqueId(const char* item) const;

	virtual bool                   IsItemClass(const char* name) const;

	virtual const char*            GetFirstItemClass();
	virtual const char*            GetNextItemClass();

	virtual void                   RegisterForCollection(EntityId itemId);
	virtual void                   UnregisterForCollection(EntityId itemId);

	virtual void                   AddItem(EntityId itemId, IItem* pItem);
	virtual void                   RemoveItem(EntityId itemId, const char* itemName = NULL);
	virtual IItem*                 GetItem(EntityId itemId) const;

	virtual ICharacterInstance*    GetCachedCharacter(const char* fileName);
	virtual IStatObj*              GetCachedObject(const char* fileName);
	virtual void                   CacheObject(const char* fileName, bool useCgfStreaming);
	virtual void                   CacheGeometry(const IItemParamsNode* geometry);
	virtual void                   CacheItemGeometry(const char* className);
	virtual void                   ClearGeometryCache();

	virtual void                   CacheItemSound(const char* className);
	virtual void                   ClearSoundCache();

	virtual EntityId               GiveItem(IActor* pActor, const char* item, bool sound, bool select = true, bool keepHistory = true, const char* setup = NULL, EEntityFlags entityFlags = (EEntityFlags)0);
	virtual void                   SetActorItem(IActor* pActor, EntityId itemId, bool keepHistory);
	virtual void                   SetActorItem(IActor* pActor, const char* name, bool keepHistory);
	virtual void                   SetActorAccessory(IActor* pActor, EntityId itemId, bool keepHistory);
	virtual void                   DropActorItem(IActor* pActor, EntityId itemId);
	virtual void                   DropActorAccessory(IActor* pActor, EntityId itemId);

	virtual void                   SetConfiguration(const char* name) { m_config = name; };
	virtual const char*            GetConfiguration() const           { return m_config.c_str(); };

	virtual void*                  Query(IItemSystemQuery query, const void* param = NULL);

	virtual void                   RegisterListener(IItemSystemListener* pListener);
	virtual void                   UnregisterListener(IItemSystemListener* pListener);

	virtual void                   Serialize(TSerialize ser);
	virtual void                   SerializePlayerLTLInfo(bool bReading);

	virtual IEquipmentManager*     GetIEquipmentManager() { return m_pEquipmentManager; }

	virtual uint32                 GetItemSocketCount(const char* item) const;
	virtual const char*            GetItemSocketName(const char* item, int idx) const;
	virtual bool                   IsCompatible(const char* item, const char* attachment) const;
	virtual bool                   GetItemSocketCompatibility(const char* item, const char* socket) const;
	virtual bool                   CanSocketBeEmpty(const char* item, const char* socket) const;

	// ~IItemSystem

	// ILevelSystemListener
	virtual void OnLevelNotFound(const char* levelName)          {};
	virtual void OnLoadingLevelEntitiesStart(ILevelInfo* pLevel) {}
	virtual void OnLoadingStart(ILevelInfo* pLevel);
	virtual void OnLoadingComplete(ILevelInfo* pLevel);
	virtual void OnLoadingError(ILevelInfo* pLevel, const char* error)     {};
	virtual void OnLoadingProgress(ILevelInfo* pLevel, int progressAmount) {};
	virtual void OnUnloadComplete(ILevelInfo* pLevel);
	//~ILevelSystemListener

	bool ScanXML(XmlNodeRef& root, const char* xmlFile);

	void RegisterItemClass(const char* name, IGameFramework::IItemCreator* pCreator);
	void PrecacheLevel();

	void GetMemoryUsage(ICrySizer* s) const;

	void PreReload();
	void PostReload();

private:
	void        CreateItemTable(const char* name);
	void        RegisterCVars();
	void        UnregisterCVars();
	void        InsertFolder(const char* folder);

	static void GiveItemCmd(IConsoleCmdArgs* args);
	static void DropItemCmd(IConsoleCmdArgs* args);
	static void GiveItemsHelper(IConsoleCmdArgs* args, bool useGiveable, bool useDebug);
	static void GiveAllItemsCmd(IConsoleCmdArgs* args);
	static void GiveAmmoCmd(IConsoleCmdArgs* args);
	static void GiveDebugItemsCmd(IConsoleCmdArgs* args);
	static void ListItemNames(IConsoleCmdArgs* args);
	static void SaveWeaponPositionCmd(IConsoleCmdArgs* args);

	static ICVar* m_pPrecache;
	static ICVar* m_pItemLimitMP;
	static ICVar* m_pItemLimitSP;

	void DisplayItemSystemStats();
	void DumpItemList(const char* filter);
	void FindItemName(const char* nameFilter);
	void ItemSystemErrorMessage(const char* fileName, const char* errorInfo, bool displayErrorDialog);

#ifdef USE_LTL_PRECACHING
	void PreCacheLevelToLevelLoadout();

	typedef CryFixedArray<const IEntityClass*, 64> TLevelToLevelItemArray;
	TLevelToLevelItemArray m_precacheLevelToLevelItemList;
	enum ELTLPreCacheState
	{
		LTLCS_FORNEXTLEVELLOAD, // indicates that the caching has to be done right after the next level is loaded
		LTLCS_FORSAVEGAME       // indicates that the list is only used for creating saveloads. If a level has LTL items coming from a previous level, we need
		                        // to save them on every savegame created in that level, because we need to recache them in case the player quits and then resume game.
	};
	ELTLPreCacheState m_LTLPrecacheState;
#endif

	struct SSpawnUserData
	{
		SSpawnUserData(const char* cls, uint16 channel) : className(cls), channelId(channel) {}
		const char* className;
		uint16      channelId;
	};

	typedef std::map<string, IStatObj*>           TObjectCache;
	typedef TObjectCache::iterator                TObjectCacheIt;

	typedef std::map<string, ICharacterInstance*> TCharacterCache;
	typedef TCharacterCache::iterator             TCharacterCacheIt;

	typedef struct SItemParamsDesc
	{
		enum ItemFlags
		{
			eIF_PreCached_Sound    = 1 << 0,
			eIF_PreCached_Geometry = 1 << 1
		};

		SItemParamsDesc() : precacheFlags(0), params(0) {};
		~SItemParamsDesc()
		{
			SAFE_RELEASE(params);

			if (!configurations.empty())
			{
				for (std::map<string, CItemParamsNode*>::iterator it = configurations.begin(); it != configurations.end(); ++it)
				{
					SAFE_RELEASE(it->second);
				}
			}
			configurations.clear();
		};

		void GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(filePath);
			pSizer->AddObject(category);
			pSizer->AddObject(params);
			pSizer->AddObject(configurations);
		}

		CItemParamsNode*                   params;
		string                             filePath;
		string                             category;
		uint8                              uniqueId;
		uint8                              priority;
		uint8                              precacheFlags;

		std::map<string, CItemParamsNode*> configurations;

	} SItemParamsDesc;

	typedef struct SItemClassDesc
	{
		SItemClassDesc() : pCreator(0) {};
		SItemClassDesc(IGameObjectExtensionCreatorBase* pCrtr) : pCreator(pCrtr) {};

		IGameObjectExtensionCreatorBase* pCreator;
		void GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }
	} SItemClassDesc;

	typedef std::map<string, SItemClassDesc>   TItemClassMap;
	typedef std::map<string, SItemParamsDesc>  TItemParamsMap;

	typedef std::map<string, int>              TItemSystemSpawnPolicy;

	typedef std::map<EntityId, IItem*>         TItemMap;
	typedef CListenerSet<IItemSystemListener*> TListeners;

	typedef std::vector<string>                TFolderList;

	typedef std::map<EntityId, CTimeValue>     TCollectionMap;

	CTimeValue                     m_precacheTime;

	TObjectCache                   m_objectCache;
	TCharacterCache                m_characterCache;

	ISystem*                       m_pSystem;
	IGameFramework*                m_pGameFramework;
	IEntitySystem*                 m_pEntitySystem;

	XmlNodeRef                     m_playerLevelToLevelSave;

	TItemClassMap                  m_classes;
	TItemParamsMap                 m_params;
	TItemMap                       m_items;
	TListeners                     m_listeners;
	uint32                         m_spawnCount;

	TCollectionMap                 m_collectionmap;

	TFolderList                    m_folders;
	bool                           m_reloading;
	bool                           m_recursing;
	bool                           m_itemParamsFlushed;

	string                         m_config;

	CEquipmentManager*             m_pEquipmentManager;

	TItemParamsMap::const_iterator m_itemClassIterator;

	int                            i_inventory_capacity;
};

#endif //__ITEMSYSTEM_H__

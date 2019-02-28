// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Helper for caching information from entities in the pool
  
 -------------------------------------------------------------------------
  History:
  - 24:06:2010: Created by Kevin Kirst

*************************************************************************/

#ifndef __GAME_CACHE_H__
#define __GAME_CACHE_H__

#include "ActorLuaCache.h"
#include "Utility/CryHash.h"

#if !defined(_RELEASE)
#define GAME_CACHE_DEBUG	1
#else
#define GAME_CACHE_DEBUG	0
#endif

class CGameCharacterDBAs
{
	struct SDBAGroup
	{
		SDBAGroup()
			: m_userCount(0)
		{

		}

		CryHashStringId		m_groupId;
		int					m_userCount;
		std::vector<string> m_dbas;			
	};

	typedef std::vector<SDBAGroup>	TCharacterDBAGroups;
	
	struct SDBAGroupUser
	{
		SDBAGroupUser()
			: m_userId(0)
		{
			m_dbaGroupIndices.reserve(2);		
		}

		ILINE bool operator == (const EntityId& otherUserId) const
		{
			return (m_userId == otherUserId);
		}

		EntityId			m_userId;
		std::vector<int>	m_dbaGroupIndices;
	};

	typedef std::vector<SDBAGroupUser>	TDBAGroupUsers;

public:

	bool AddUserToGroup(const EntityId userId, const std::vector<string>& dbaGroups);
	void RemoveUser(const EntityId userId);

	void LoadXmlData();
	void PreCacheForLevel();
	void Reset();
	void GetMemoryUsage(ICrySizer *s) const;

#if GAME_CACHE_DEBUG
	void Debug();
#else
	ILINE void Debug() {};
#endif

private:

	bool IsEnabled() const;

	int			GetGroupIndexByName(const char* groupName) const;
	SDBAGroup*	GetGroupByIndex(int index);
	bool		IsAlreadyRegistered(const EntityId userId);

	//Dynamic (level heap)
	TDBAGroupUsers m_dbaGroupUsers;

	//Static (loaded at start-up)
	TCharacterDBAGroups	m_dbaGroups; 
};

class CGameCache
{
public:

	typedef CryFixedStringT<256>	TCachedModelName;

	CGameCache();
	virtual ~CGameCache();
	void Init();
	void PrecacheLevel();
	void Reset();
	void GetMemoryUsage(ICrySizer *s) const;

	static bool IsCacheEnabled();
	static bool IsLuaCacheEnabled();
	static void GenerateModelVariation(const string& inputName, TCachedModelName& outputName, SmartScriptTable pEntityScript, int variationCount, int variation);

	void CacheActorClass(IEntityClass *pClass, SmartScriptTable pEntityScript);
	void CacheActorInstance(EntityId entityId, SmartScriptTable pEntityScript, SmartScriptTable pPropertiesOverride = SmartScriptTable(), int modelVariation = -1);
	void RefreshActorInstance(EntityId entityId, SmartScriptTable pEntityScript, SmartScriptTable pPropertiesOverride = SmartScriptTable());
	void CacheEntityArchetype(const char* archetypeName);
	void CacheBodyDamageProfile(const char* bodyDamageFileName, const char* bodyDamagePartsFileName);

	// Lua cache accessors
	SLuaCache_ActorPhysicsParamsPtr GetActorPhysicsParams(IEntityClass *pClass) const;
	SLuaCache_ActorGameParamsPtr GetActorGameParams(IEntityClass *pClass) const;
	SLuaCache_ActorPropertiesPtr GetActorProperties(EntityId entityId) const;

	void GetInstancePropertyValue(const char* szPropertyName, stack_string& szPropertyValue, const XmlNodeRef& entityNode, const SmartScriptTable& pEntityScript, const SmartScriptTable& pPropertiesOverride);

	void CacheTexture(const char* textureFileName, const int textureFlags);
	void CacheGeometry(const char* geometryObjectFileName); 
	void CacheMaterial(const char* materialFileName);

	bool PrepareDBAsFor(const EntityId userId, const std::vector<string>& dbaGroups);
	void RemoveDBAUser(const EntityId userId);

#if GAME_CACHE_DEBUG
	void Debug();
#else
	ILINE void Debug() {};
#endif

private:
	// Lua cache info per actor class instance
	struct SActorClassLuaCache
	{
		SLuaCache_ActorPhysicsParamsPtr pPhysicsParams;
		SLuaCache_ActorGameParamsPtr pGameParams;
	};

	// Lua cache info per actor individual instance
	struct SActorInstanceLuaCache
	{
		SLuaCache_ActorPropertiesPtr pProperties;
	};

	typedef std::map<IEntityClass*, SActorClassLuaCache> TActorClassLuaCacheMap;
	typedef std::map<EntityId, SActorInstanceLuaCache> TActorInstanceLuaCacheMap;
	typedef _smart_ptr<ICharacterInstance> TCharacterInstancePtr;
	typedef std::map<uint32, TCharacterInstancePtr> TEditorCharacterFileModelCache;
	enum ECharacterFileModelCacheType
	{
		eCFMCache_Default = 0,
		eCFMCache_Client,
		eCFMCache_Shadow,

		eCFMCache_COUNT,
	};

	typedef _smart_ptr<ITexture>	TTextureSmartPtr;
	struct STextureKey
	{
		STextureKey()
			: nameHash(0)
			, textureFlags(0)
		{

		}

		STextureKey(const CryHash& _nameHash, const int _textureFlags)
			: nameHash(_nameHash)
			, textureFlags(_textureFlags)
		{

		}

		struct compare
		{
			bool operator()(const STextureKey& k1, const STextureKey& k2) const
			{
				return (k1.nameHash < k2.nameHash) || ((k1.nameHash == k2.nameHash) && (k1.textureFlags < k2.textureFlags));
			}
		};

		CryHash nameHash;
		int		textureFlags;
	};
	typedef	std::map<STextureKey, TTextureSmartPtr, STextureKey::compare> TGameTextureCacheMap;

	typedef _smart_ptr<IStatObj>	TStaticObjectSmartPtr;
	typedef std::map<CryHash, TStaticObjectSmartPtr> TGameStaticObjectCacheMap;
	typedef _smart_ptr<IMaterial>	TMaterialSmartPtr;
	typedef std::map<CryHash, TMaterialSmartPtr>	TGameMaterialCacheMap;

	static SmartScriptTable GetProperties(SmartScriptTable pEntityScript, SmartScriptTable pPropertiesOverride = SmartScriptTable());

	static bool IsClient(EntityId entityId);

	// Lua cache helpers
	const SActorClassLuaCache* GetActorClassLuaCache(IEntityClass *pClass) const;
	const SActorInstanceLuaCache* GetActorInstanceLuaCache(EntityId entityId) const;
	void CreateActorClassLuaCache(IEntityClass *pClass, SmartScriptTable pEntityScript);
	void CreateActorInstanceLuaCache(EntityId entityId, SmartScriptTable pEntityScript, SmartScriptTable pProperties, int modelVariation = -1);
	void UpdateActorInstanceCache(TActorInstanceLuaCacheMap::iterator actorInstanceLuaCacheIt, SmartScriptTable pEntityScript, SmartScriptTable pProperties, int modelVariation = -1);
	bool UpdateActorInstanceCache(SActorInstanceLuaCache &actorInstanceLuaCache, SmartScriptTable pEntityScript, SmartScriptTable pProperties, int modelVariation = -1);
	void CacheFileModels(SmartScriptTable pProperties);
	void CacheAdditionalParams(SmartScriptTable pProperties);

	void CacheActorResources(SmartScriptTable pEntityScript);
	void CacheCustomEntityResources(SmartScriptTable pEntityScript);

	// Character file model cache helpers
	bool AddCachedCharacterFileModel(const ECharacterFileModelCacheType type, const char* szFileName);
	bool IsCharacterFileModelCached(const char* szFileName, uint32& outputFileNameHash) const;

private:
	TActorClassLuaCacheMap m_ActorClassLuaCache;

	TActorInstanceLuaCacheMap m_ActorInstanceLuaCache;

	// Player instance cache
	SActorInstanceLuaCache m_PlayerInstanceLuaCache;

	// Character file model caches
	TEditorCharacterFileModelCache   m_editorCharacterFileModelCache[eCFMCache_COUNT];

	//Multiple resource cache, for game elements which might not have proper resource management
	TGameTextureCacheMap m_textureCache;
	TGameMaterialCacheMap m_materialCache;
	TGameStaticObjectCacheMap m_statiObjectCache;

	//DBA management for characters
	CGameCharacterDBAs m_characterDBAs;

	IActorSystem *m_pActorSystem;
};

#endif //__GAME_CACHE_H__

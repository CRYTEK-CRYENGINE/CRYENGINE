// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/***********************************************************************************
------------------------------------------------------------------------------------
$Id$
$DateTime$
Description:	Caches geometry, particles etc. used by items to prevent repeated file 
							access

------------------------------------------------------------------------------------
History:
- 06:05:2010   10:54 : Moved from ItemSharedParams and expanded by Claire Allan

************************************************************************************/

#pragma once

#ifndef __ITEMRESOURCECACHE_H__
#define __ITEMRESOURCECACHE_H__

#include "ItemString.h"
#include <CryAnimation/ICryAnimation.h>

struct ICharacterInstance;
struct IMaterial;

class CItemGeometryCache
{
	typedef _smart_ptr<ICharacterInstance> TCharacterInstancePtr;
	typedef std::map<uint32, TCharacterInstancePtr> TEditorCacheCharacterMap;

	typedef _smart_ptr<IStatObj>	TStatObjectPtr;
	typedef std::map<uint32, TStatObjectPtr> TCacheStaticObjectMap;

public:

	~CItemGeometryCache()
	{
		//Benito: Cached are already flushed at this time, but this should not 'hurt'
		FlushCaches();
	}

	void FlushCaches()
	{
		m_editorCachedCharacters.clear();
		m_cachedStaticObjects.clear();
	}

	void CacheGeometry(const char* objectFileName, bool useCgfStreaming, uint32 nLoadingFlags=0);
	void CacheGeometryFromXml(XmlNodeRef node, bool useCgfStreaming, uint32 nLoadingFlags=0);

	void GetMemoryStatistics(ICrySizer *s);

private:

	void CheckAndCacheGeometryFromXmlAttr( const char* pAttr, bool useCgfStreaming, uint32 nLoadingFlags );

	bool IsCharacterCached( const uint32 fileNameHash ) const;
	bool IsStaticObjectCached( const uint32 fileNameHash ) const;
	
	TEditorCacheCharacterMap	m_editorCachedCharacters;
	TCacheStaticObjectMap m_cachedStaticObjects;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CItemParticleEffectCache
{
	typedef _smart_ptr<IParticleEffect> TParticleEffectPtr;
	typedef std::map<int, TParticleEffectPtr> TCacheParticleMap;

public:

	~CItemParticleEffectCache()
	{
		FlushCaches();
	}

	void FlushCaches()
	{
		m_cachedParticles.clear();
	}

	void CacheParticle(const char* particleFileName);
	IParticleEffect* GetCachedParticle(const char* particleFileName) const;
	void GetMemoryStatistics(ICrySizer *s);

private:

	bool IsParticleCached(const char* particleFileName) const
	{
		return (GetCachedParticle(particleFileName) != NULL);
	}

	TCacheParticleMap	m_cachedParticles;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CItemMaterialAndTextureCache
{
	typedef _smart_ptr<ITexture> TTexturePtr;
	typedef std::map<uint32, TTexturePtr> TCacheTextureMap;

	typedef _smart_ptr<IMaterial> TMaterialPtr;
	typedef std::map<uint32, TMaterialPtr> TCacheMaterialMap;

public:

	~CItemMaterialAndTextureCache()
	{
		FlushCaches();
	}

	void FlushCaches();

	void CacheTexture(const char* textureFileName, bool noStreaming = false);
	void CacheMaterial(const char* materialFileName);
	void GetMemoryStatistics(ICrySizer *s);

private:

	ITexture* GetCachedTexture(const char* textureFileName) const;
	bool IsTextureCached(const char* textureFileName) const
	{
		return (GetCachedTexture(textureFileName) != NULL);
	}

	IMaterial* GetCachedMaterial(const char* materialFileName) const;
	bool IsMaterialCached(const char* materialFileName) const
	{
		return (GetCachedMaterial(materialFileName) != NULL);
	}

	TCacheTextureMap	m_cachedTextures;
	TCacheMaterialMap	m_cachedMaterials;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CItemPrefetchCHRManager
{
public:
	CItemPrefetchCHRManager();
	~CItemPrefetchCHRManager();

	void Reset();

	void Update(float fCurrTime);

	void Prefetch(const ItemString& geomName);

private:
	struct PrefetchSlot
	{
		ItemString geomName;
		float requestTime;
	};

	typedef std::vector<PrefetchSlot> PrefetchSlotVec;

private:
	CItemPrefetchCHRManager(const CItemPrefetchCHRManager&);
	CItemPrefetchCHRManager& operator = (const CItemPrefetchCHRManager&);

private:
	float m_fTimeout;

	PrefetchSlotVec m_prefetches;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CItemSharedParams;

class CItemAnimationDBAManager
{
private:
	struct SItemDBAInfo
	{
		SItemDBAInfo()
			: m_userCount(0)
			, m_requestedTime(0.0f)
		{
			m_dbas.reserve(2);
#if defined(_DEBUG)
			m_userIds.reserve(16);
#endif	
		}

		void AddUser(const EntityId userId)
		{
#if defined(_DEBUG)
			CRY_ASSERT_MESSAGE(ValidateAddForEntity(userId), "Adding item dba user multiple times!");
#endif	
			m_userCount++;
		}

		void RemoveUser(const EntityId userId)
		{
#if defined(_DEBUG)
			CRY_ASSERT_MESSAGE(ValidateRemoveForEntity(userId), "Removing item dba user which is not!");
#endif	
			m_userCount--;

			CRY_ASSERT(m_userCount >= 0);
		}

		ILINE int GetUserCount() const
		{
			return m_userCount;
		}

		ILINE void AddDbaPath(const ItemString& dbaPath)
		{
			m_dbas.push_back(dbaPath);
		}

		ILINE int GetDBACount() const 
		{
			return m_dbas.size();
		}

		ILINE const char* GetDBA(int i) const
		{
			CRY_ASSERT((i >= 0) && (i < (int)m_dbas.size()));

			return m_dbas[i].c_str();
		}

		ILINE void SetRequestedTime(const float requestTime)
		{
			m_requestedTime = requestTime;
		}

		ILINE float GetRequestedTime() const
		{
			return m_requestedTime;
		}

#if defined(_DEBUG)
		bool ValidateAddForEntity(const EntityId userId)
		{
			bool wasRegistered = stl::find(m_userIds, userId);
			if (!wasRegistered)
			{
				m_userIds.push_back(userId);
			}
			return !wasRegistered;
		}

		bool ValidateRemoveForEntity(const EntityId userId)
		{
			return stl::find_and_erase(m_userIds, userId);
		}
#endif	

	private:

		int m_userCount;
		float m_requestedTime;
		std::vector<ItemString> m_dbas;

#if defined(_DEBUG)
		std::vector<EntityId> m_userIds;
#endif
	};

	typedef std::pair<ItemString, SItemDBAInfo>	TItemDBAPair;
	typedef std::vector<TItemDBAPair> TPreloadDBAArray;

public:

	CItemAnimationDBAManager();

	void Reset();

	void AddDBAUser(const ItemString& animationGroup, const CItemSharedParams* pItemParams, const EntityId itemUserId);
	void RemoveDBAUser(const ItemString& animationGroup, const EntityId itemUserId);
	void RequestDBAPreLoad(const ItemString& animationGroup, const CItemSharedParams* pItemParams);

	void Update(const float currentTime);

	void Debug();

	void GetMemoryStatistics(ICrySizer *s);

private:

	bool IsValidDBAPath(const char* dbaPath) const;
	int IsDbaAlreadyInUse(const ItemString& animationGroup) const; 
	SItemDBAInfo& GetDbaInUse(const int index);
	int IsDbaAlreadyInPreloadList(const ItemString& animationGroup) const;
	SItemDBAInfo& GetDbaInPreloadList(const int index);

	void FreeSlotInPreloadListIfNeeded();
	void RemoveFromPreloadListAndDoNotUnload(const ItemString& animationGroup);

	bool IsDbaManagementEnabled() const;

	TPreloadDBAArray m_inUseDBAs;
	TPreloadDBAArray m_preloadedDBASlots;
};


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CItemResourceCache
{
	typedef std::set<const IEntityClass*> TCachedResourcesClassesVector;

public:

	void FlushCaches()
	{
		m_cachedClasses.clear();
		m_itemGeometryCache.FlushCaches();
		m_ammoGeometryCache.FlushCaches();
		m_particleEffectCache.FlushCaches();
		m_materialsAndTextureCache.FlushCaches();
		m_1pDBAManager.Reset();
		m_pfCHRManager.Reset();
	}

	void CachedResourcesForClassDone(const IEntityClass* pItemClass)
	{
		m_cachedClasses.insert(pItemClass);
	}

	bool AreClassResourcesCached(const IEntityClass* pItemClass) const
	{
		return (m_cachedClasses.find(pItemClass) != m_cachedClasses.end());
	}

	ILINE CItemGeometryCache& GetItemGeometryCache() 
	{ 
		return m_itemGeometryCache; 
	} 

	ILINE CItemGeometryCache& GetAmmoGeometryCache()
	{
		return m_ammoGeometryCache;
	}

	ILINE CItemParticleEffectCache& GetParticleEffectCache()
	{
		return m_particleEffectCache;
	}

	ILINE CItemMaterialAndTextureCache& GetMaterialsAndTextureCache()
	{
		return m_materialsAndTextureCache;
	}

	ILINE CItemAnimationDBAManager& Get1pDBAManager()
	{
		return m_1pDBAManager;
	}

	ILINE CItemPrefetchCHRManager& GetPrefetchCHRManager()
	{
		return m_pfCHRManager;
	}

	void GetMemoryStatistics(ICrySizer *s)
	{
		s->AddObject(m_cachedClasses);
		m_itemGeometryCache.GetMemoryStatistics(s);
		m_ammoGeometryCache.GetMemoryStatistics(s);
		m_particleEffectCache.GetMemoryStatistics(s);
		m_materialsAndTextureCache.GetMemoryStatistics(s);
		m_1pDBAManager.GetMemoryStatistics(s);
	}

private:
	CItemGeometryCache				m_itemGeometryCache;
	CItemGeometryCache				m_ammoGeometryCache;
	CItemParticleEffectCache		m_particleEffectCache;
	CItemMaterialAndTextureCache	m_materialsAndTextureCache;
	CItemAnimationDBAManager	m_1pDBAManager;
	CItemPrefetchCHRManager m_pfCHRManager;

	TCachedResourcesClassesVector m_cachedClasses;
};

#endif //__ITEMRESOURCECACHE_H__

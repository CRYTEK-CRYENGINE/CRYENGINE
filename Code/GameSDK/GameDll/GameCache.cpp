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

#include "StdAfx.h"
#include "GameCache.h"
#include "GameRules.h"
#include "GameCVars.h"
#include "BodyManager.h"
#include "MovementTransitionsSystem.h"
#include "ICryMannequin.h"
#include "HitDeathReactionsSystem.h"
#include <CryAISystem/BehaviorTree/IBehaviorTree.h>

//////////////////////////////////////////////////////////////////////////
CGameCache::CGameCache()
: m_pActorSystem(NULL)
{

}

//////////////////////////////////////////////////////////////////////////
CGameCache::~CGameCache()
{
}

//////////////////////////////////////////////////////////////////////////
bool CGameCache::IsCacheEnabled()
{
	return (g_pGameCVars->g_enablePoolCache != 0);
}

//////////////////////////////////////////////////////////////////////////
bool CGameCache::IsLuaCacheEnabled()
{
	return (!gEnv->IsEditor() && g_pGameCVars->g_enableActorLuaCache != 0);
}

//////////////////////////////////////////////////////////////////////////
bool CGameCache::IsClient(EntityId entityId)
{
	const EntityId localClientId = gEnv->pGameFramework->GetClientActorId();
	return (localClientId == entityId);
}

//////////////////////////////////////////////////////////////////////////
SmartScriptTable CGameCache::GetProperties(SmartScriptTable pEntityScript, SmartScriptTable pPropertiesOverride)
{
	SmartScriptTable pProperties = NULL;

	if (pPropertiesOverride)
	{
		pProperties = pPropertiesOverride;
	}
	else if (pEntityScript)
	{
		pEntityScript->GetValue("Properties", pProperties);
	}

	return pProperties;
}

//////////////////////////////////////////////////////////////////////////
void CGameCache::Init()
{
	IGameFramework *pGameFramework = g_pGame->GetIGameFramework();
	assert(pGameFramework);

	m_pActorSystem = pGameFramework->GetIActorSystem();
	assert(m_pActorSystem);

	m_characterDBAs.LoadXmlData();

}

//////////////////////////////////////////////////////////////////////////
void CGameCache::PrecacheLevel()
{
	LOADING_TIME_PROFILE_SECTION;

	// Cache player model
	if (g_pGameCVars->g_loadPlayerModelOnLoad != 0)
	{
		IEntityClass *pPlayerClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Player");
		assert(pPlayerClass);

		// Cache the player class
		SmartScriptTable pEntityScript = pPlayerClass->GetScriptTable();
		CacheActorClass(pPlayerClass, pEntityScript);

		// Cache the player's properties
		SLuaCache_ActorProperties playerProperties;
		SmartScriptTable pProperties = GetProperties(pEntityScript);
		if (!UpdateActorInstanceCache(m_PlayerInstanceLuaCache, pEntityScript, pProperties))
		{
			GameWarning("[Game Cache] Unable to cache data for local player with class \'%s\'", pPlayerClass->GetName());
		}
		
		if( gEnv->bMultiplayer )
		{
			IEntityClass *pPlayerHeavyClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("PlayerHeavy");
			assert(pPlayerHeavyClass);

			// Cache the playable heavy class
			pEntityScript = pPlayerHeavyClass->GetScriptTable();
			CacheActorClass(pPlayerHeavyClass, pEntityScript);

			// Cache the class's properties
			//SLuaCache_ActorProperties playerProperties;
			SmartScriptTable pEntityProperties = GetProperties(pEntityScript);
			if (!UpdateActorInstanceCache(m_PlayerInstanceLuaCache, pEntityScript, pEntityProperties))
			{
				GameWarning("[Game Cache] Unable to cache data for local player with class \'%s\'", pPlayerHeavyClass->GetName());
			}
		}
	}

	m_characterDBAs.PreCacheForLevel();
}

//////////////////////////////////////////////////////////////////////////
void CGameCache::Reset()
{
	m_ActorClassLuaCache.clear();
	m_ActorInstanceLuaCache.clear();

	for (uint32 uCFMCacheIndex = 0; uCFMCacheIndex < eCFMCache_COUNT; ++uCFMCacheIndex)
	{
		m_editorCharacterFileModelCache[uCFMCacheIndex].clear();
	}

	m_PlayerInstanceLuaCache = SActorInstanceLuaCache();

	m_textureCache.clear();
	m_materialCache.clear();
	m_statiObjectCache.clear();
	m_characterDBAs.Reset();

}

//////////////////////////////////////////////////////////////////////////
void CGameCache::GetMemoryUsage(ICrySizer *s) const
{
	s->AddContainer(m_ActorClassLuaCache);
	s->AddContainer(m_ActorInstanceLuaCache);

	for (uint32 uCFMCacheIndex = 0; uCFMCacheIndex < eCFMCache_COUNT; ++uCFMCacheIndex)
	{
		s->AddContainer(m_editorCharacterFileModelCache[uCFMCacheIndex]);
	}

	s->Add(m_PlayerInstanceLuaCache);
	
	s->AddContainer(m_textureCache);
	s->AddContainer(m_materialCache);
	s->AddContainer(m_statiObjectCache);

	m_characterDBAs.GetMemoryUsage(s);
}

//////////////////////////////////////////////////////////////////////////
const CGameCache::SActorClassLuaCache* CGameCache::GetActorClassLuaCache(IEntityClass *pClass) const
{
	assert(pClass);
	
	const SActorClassLuaCache *pResult = NULL;

	if (IsLuaCacheEnabled())
	{
		TActorClassLuaCacheMap::const_iterator itCacheEntry = m_ActorClassLuaCache.find(pClass);
		if (itCacheEntry != m_ActorClassLuaCache.end())
		{
			pResult = &(itCacheEntry->second);
		}
	}

	return pResult;
}

//////////////////////////////////////////////////////////////////////////
const CGameCache::SActorInstanceLuaCache* CGameCache::GetActorInstanceLuaCache(EntityId entityId) const
{
	assert(entityId > 0);

	const SActorInstanceLuaCache *pResult = NULL;

	if (IsLuaCacheEnabled())
	{
		TActorInstanceLuaCacheMap::const_iterator itCacheEntry = m_ActorInstanceLuaCache.find(entityId);
		if (itCacheEntry != m_ActorInstanceLuaCache.end())
		{
			pResult = &(itCacheEntry->second);
		}
	}

	return pResult;
}

//////////////////////////////////////////////////////////////////////////
SLuaCache_ActorPhysicsParamsPtr CGameCache::GetActorPhysicsParams(IEntityClass *pClass) const
{
	assert(pClass);

	SLuaCache_ActorPhysicsParamsPtr pResult;
	
	if (const SActorClassLuaCache *pLuaCache = GetActorClassLuaCache(pClass))
	{
		pResult.reset(pLuaCache->pPhysicsParams);
	}

	return pResult;
}

//////////////////////////////////////////////////////////////////////////
SLuaCache_ActorGameParamsPtr CGameCache::GetActorGameParams(IEntityClass *pClass) const
{
	assert(pClass);

	SLuaCache_ActorGameParamsPtr pResult;
	
	if (const SActorClassLuaCache *pLuaCache = GetActorClassLuaCache(pClass))
	{
		pResult.reset(pLuaCache->pGameParams);
	}

	return pResult;
}

//////////////////////////////////////////////////////////////////////////
SLuaCache_ActorPropertiesPtr CGameCache::GetActorProperties(EntityId entityId) const
{
	assert(entityId > 0);
	
	SLuaCache_ActorPropertiesPtr pResult;
	
	if (const SActorInstanceLuaCache *pLuaCache = GetActorInstanceLuaCache(entityId))
	{
		pResult.reset(pLuaCache->pProperties);
	}

	return pResult;
}

void CGameCache::GetInstancePropertyValue(const char* szPropertyName, stack_string& propertyValue, const XmlNodeRef& entityNode, 
	const SmartScriptTable& pEntityScript, const SmartScriptTable& pPropertiesOverride)
{
	if (entityNode)
	{
		XmlNodeRef propertiesInstance = entityNode->findChild("Properties");
		if (propertiesInstance)
		{
			propertyValue = propertiesInstance->getAttr(szPropertyName);
		}
	}

	if(propertyValue.empty())
	{
		SmartScriptTable pProperties = GetProperties(pEntityScript, pPropertiesOverride);
		if (pProperties)
		{
			const char* szPropertyValue;
			if(pProperties->GetValue(szPropertyName, szPropertyValue))
			{
				propertyValue = szPropertyValue;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameCache::CacheActorClass(IEntityClass *pClass, SmartScriptTable pEntityScript)
{
	TActorClassLuaCacheMap::const_iterator itCacheEntry = m_ActorClassLuaCache.find(pClass);
	if (itCacheEntry == m_ActorClassLuaCache.end())
	{
		// Create lua cache for this class
		CreateActorClassLuaCache(pClass, pEntityScript);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameCache::CacheActorInstance(EntityId entityId, SmartScriptTable pEntityScript, SmartScriptTable pPropertiesOverride /*= SmartScriptTable()*/, int modelVariation /*= -1*/)
{
	if (!IsClient(entityId))
	{
		TActorInstanceLuaCacheMap::const_iterator itCacheEntry = m_ActorInstanceLuaCache.find(entityId);
		if (itCacheEntry == m_ActorInstanceLuaCache.end())
		{
			SmartScriptTable pProperties = GetProperties(pEntityScript, pPropertiesOverride);
			if (pProperties)
			{
				CacheAdditionalParams(pProperties);

				// Create lua cache for this individual
				CreateActorInstanceLuaCache(entityId, pEntityScript, pProperties, modelVariation);
			}
		}
	}
	else
	{
		RefreshActorInstance(entityId, pEntityScript, pPropertiesOverride);
	}
}


// ============================================================================
//	Cache file models.
//
//	In:		The 'Properties' Lua table where the model file names are to be
//			obtained from (NULL will abort!)
//
void CGameCache::CacheFileModels(SmartScriptTable pProperties)
{
	const char* modelFileName = NULL;
	if (pProperties->GetValue("fileModel", modelFileName))
	{
		AddCachedCharacterFileModel(eCFMCache_Default, modelFileName);
	}
}


//////////////////////////////////////////////////////////////////////////
void CGameCache::CacheAdditionalParams( SmartScriptTable pProperties )
{
	// Preload body damage info
	CBodyDamageManager *pBodyDamageManager = g_pGame->GetBodyDamageManager();
	if (pBodyDamageManager)
	{
		pBodyDamageManager->CacheBodyDamage(pProperties);
		pBodyDamageManager->CacheBodyDestruction(pProperties);
	}

	// Preload the equipment pack
	const char* szEquipmentPackName = 0;
	if (pProperties->GetValue("equip_EquipmentPack", szEquipmentPackName))
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();
		if (pGameRules)
			pGameRules->PreCacheEquipmentPack(szEquipmentPackName);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameCache::CacheActorResources(SmartScriptTable pEntityScript)
{
	HSCRIPTFUNCTION cacheResourcesFunctions(NULL);
	if (pEntityScript && pEntityScript->GetValue("CacheResources", cacheResourcesFunctions))
	{
		Script::Call(gEnv->pScriptSystem, cacheResourcesFunctions, pEntityScript);

		gEnv->pScriptSystem->ReleaseFunc(cacheResourcesFunctions);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CGameCache::AddCachedCharacterFileModel(const ECharacterFileModelCacheType type, const char* szFileName)
{
	uint32 fileNameHash = 0;
	bool bCached = IsCharacterFileModelCached(szFileName, fileNameHash);

	if (!bCached && szFileName && szFileName[0])
	{
		if (gEnv->IsEditor())
		{
			ICharacterInstance *pCachedInstance = gEnv->pCharacterManager->CreateInstance(szFileName);
			if (pCachedInstance)
			{
				m_editorCharacterFileModelCache[type].insert(TEditorCharacterFileModelCache::value_type(fileNameHash, TCharacterInstancePtr(pCachedInstance)));
				bCached = true;
			}
		}
		else
		{
			bool hasLoaded = gEnv->pCharacterManager->LoadAndLockResources( szFileName, 0 );
			if (hasLoaded)
			{
			}
		}
	}

	return bCached;
}

//////////////////////////////////////////////////////////////////////////
void CGameCache::RefreshActorInstance(EntityId entityId, SmartScriptTable pEntityScript, SmartScriptTable pPropertiesOverride /*= SmartScriptTable()*/ )
{
	if (!IsClient(entityId))
	{
		TActorInstanceLuaCacheMap::iterator itCacheEntry = m_ActorInstanceLuaCache.find(entityId);
		if (itCacheEntry != m_ActorInstanceLuaCache.end())
		{
			SmartScriptTable pProperties = GetProperties(pEntityScript, pPropertiesOverride);
			if (pProperties)
			{
				CacheAdditionalParams(pProperties);

				// Update the lua cache for this individual
				UpdateActorInstanceCache(itCacheEntry, pEntityScript, pProperties);
			}
		}
	}
	else
	{
		SmartScriptTable pProperties = GetProperties(pEntityScript, pPropertiesOverride);
		if (pProperties)
		{
			CacheAdditionalParams(pProperties);

			// Update the lua cache for this individual
			UpdateActorInstanceCache(m_PlayerInstanceLuaCache, pEntityScript, pProperties);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameCache::CreateActorClassLuaCache(IEntityClass *pClass, SmartScriptTable pEntityScript)
{
	assert(pClass);
	assert((bool)pEntityScript);

	if (pEntityScript)
	{
		SActorClassLuaCache actorClassLuaCache;

		actorClassLuaCache.pPhysicsParams.reset(new SLuaCache_ActorPhysicsParams);
		if (!actorClassLuaCache.pPhysicsParams->CacheFromTable(pEntityScript, pClass->GetName()))
		{
			GameWarning("[Game Cache] Failed to cache Physics Params for class \'%s\'", pClass->GetName());
		}

		actorClassLuaCache.pGameParams.reset(new SLuaCache_ActorGameParams);
		if (!actorClassLuaCache.pGameParams->CacheFromTable(pEntityScript))
		{
			GameWarning("[Game Cache] Failed to cache Game Params for class \'%s\'", pClass->GetName());
		}

		// Cache movement transitions data
		g_pGame->GetMovementTransitionsSystem().GetMovementTransitions(pClass, pEntityScript);

		// Callback to lua to cache all required resources
		CacheActorResources(pEntityScript);

		std::pair<TActorClassLuaCacheMap::iterator, bool> result = m_ActorClassLuaCache.insert(TActorClassLuaCacheMap::value_type(pClass, actorClassLuaCache));
		assert(result.second == true); // Assert that it was inserted and not already present
	}
	else
	{
		GameWarning("[Game Cache] Unable to cache Lua data for class \'%s\'", pClass->GetName());
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameCache::CreateActorInstanceLuaCache(EntityId entityId, SmartScriptTable pEntityScript, SmartScriptTable pProperties, int modelVariation /*= -1*/)
{
	assert((bool)pEntityScript);
	assert((bool)pProperties);

	if (pEntityScript && pProperties)
	{
		SActorInstanceLuaCache actorInstanceLuaCache;

		std::pair<TActorInstanceLuaCacheMap::iterator, bool> result = m_ActorInstanceLuaCache.insert(TActorInstanceLuaCacheMap::value_type(entityId, actorInstanceLuaCache));
		assert(result.second == true); // Assert that it was inserted and not already present

		UpdateActorInstanceCache(result.first, pEntityScript, pProperties, modelVariation);
	}
	else
	{
		GameWarning("[Game Cache] Unable to cache Lua data for actor Id %d", entityId);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameCache::UpdateActorInstanceCache( TActorInstanceLuaCacheMap::iterator actorInstanceLuaCacheIt, SmartScriptTable pEntityScript, SmartScriptTable pProperties, int modelVariation /*= -1*/)
{
	SActorInstanceLuaCache &actorInstanceLuaCache = actorInstanceLuaCacheIt->second;
	if (!UpdateActorInstanceCache(actorInstanceLuaCache, pEntityScript, pProperties, modelVariation))
	{
		GameWarning("[Game Cache] Failed to cache File Model Info for actor Id %d", actorInstanceLuaCacheIt->first);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CGameCache::UpdateActorInstanceCache(SActorInstanceLuaCache &actorInstanceLuaCache, SmartScriptTable pEntityScript, SmartScriptTable pProperties, int modelVariation /*= -1*/)
{
	actorInstanceLuaCache.pProperties.reset(new SLuaCache_ActorProperties);
	if (actorInstanceLuaCache.pProperties->CacheFromTable(pEntityScript, pProperties))
	{
		const SActorFileModelInfo& fileModelInfo = actorInstanceLuaCache.pProperties->fileModelInfo;

		TCachedModelName modelVariationFileName;
		GenerateModelVariation(fileModelInfo.sFileName, modelVariationFileName, pEntityScript, fileModelInfo.nModelVariations, modelVariation);

		ICharacterManager *pCharacterManager = gEnv->pCharacterManager;
		assert(pCharacterManager);


		// Cache character model
		const char* szCharacterFileModel = modelVariationFileName.c_str();
		AddCachedCharacterFileModel(eCFMCache_Default, szCharacterFileModel);

		// Cache character client file model
		const char* szClientFileModel = fileModelInfo.sClientFileName.c_str();
		AddCachedCharacterFileModel(eCFMCache_Client, szClientFileModel);

		// Cache character shadow file model
		const char* szShadowFileModel = fileModelInfo.sShadowFileName.c_str();
		AddCachedCharacterFileModel(eCFMCache_Shadow, szShadowFileModel);

		g_pGame->GetIGameFramework()->PreloadAnimatedCharacter(pEntityScript.GetPtr());

		// Cache Hit-Death Reactions actor specific files.
		CHitDeathReactionsSystem &hitDeathReactionSystem = g_pGame->GetHitDeathReactionsSystem();
		hitDeathReactionSystem.PreloadActorData(pProperties);

		return true;
	}

	return false;
}

void CGameCache::CacheTexture( const char* textureFileName, const int textureFlags )
{
	const bool validName = (textureFileName && textureFileName[0]);

	if (validName)
	{
		const STextureKey textureKey(CryStringUtils::HashString(textureFileName), textureFlags);

		if (m_textureCache.find(textureKey) == m_textureCache.end() && gEnv->pRenderer)
		{
			ITexture* pTexture = gEnv->pRenderer->EF_LoadTexture(textureFileName, textureFlags);
			if (pTexture)
			{
				m_textureCache.insert(TGameTextureCacheMap::value_type(textureKey, TTextureSmartPtr(pTexture)));
				pTexture->Release();
			}
		}
	}
}

void CGameCache::CacheGeometry(const char* geometryFileName)
{
	const bool validName = (geometryFileName && geometryFileName[0]);
	if (validName)
	{
		stack_string ext(PathUtil::GetExt(geometryFileName));

		if ((ext == "cdf") || (ext == "chr") || (ext == "cga"))
		{
			AddCachedCharacterFileModel(eCFMCache_Default, geometryFileName);
		}
		else
		{
			const CryHash hashName = CryStringUtils::HashString(geometryFileName);

			if (m_statiObjectCache.find(hashName) == m_statiObjectCache.end())
			{
				IStatObj* pStaticObject = gEnv->p3DEngine->LoadStatObj(geometryFileName);
				if (pStaticObject)
				{
					m_statiObjectCache.insert(TGameStaticObjectCacheMap::value_type(hashName, TStaticObjectSmartPtr(pStaticObject)));
				}
			}
		}
	}
}

void CGameCache::CacheMaterial( const char* materialFileName )
{
	const bool validName = (materialFileName && materialFileName[0]);

	if (validName)
	{
		const CryHash hashName = CryStringUtils::HashString(materialFileName);

		if (m_materialCache.find(hashName) == m_materialCache.end())
		{
			IMaterial* pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(materialFileName);
			if (pMaterial)
			{
				m_materialCache.insert(TGameMaterialCacheMap::value_type(hashName, TMaterialSmartPtr(pMaterial)));
			}
		}
	}
}

void CGameCache::GenerateModelVariation( const string& inputName, TCachedModelName& outputName, SmartScriptTable pEntityScript, int variationCount, int variation )
{
	outputName = inputName.c_str();

	if (!outputName.empty() && (variationCount > 0))
	{
		string sFileName = PathUtil::GetFileName(inputName);
		const string::size_type size = sFileName.size();
		if ((size > 3) && (sFileName[size-3] == '_'))
		{
			int desiredVariation = variation;

			//If no variation provided, try to find it in the properties
			if (desiredVariation == -1)
			{
				SmartScriptTable pPropertiesInstance;
				if (pEntityScript && pEntityScript->GetValue("PropertiesInstance", pPropertiesInstance))
				{
					pPropertiesInstance->GetValue("nVariation", desiredVariation);
				}
			}

			desiredVariation = CLAMP(desiredVariation, 1, variationCount);

			// modelName_01 => modelName_XX where XX == nVariation, preserving original path and extension
			outputName.Format("%s%02d", sFileName.substr(0, size-2).c_str(), desiredVariation);
			outputName = PathUtil::Make(PathUtil::GetPathWithoutFilename(inputName), string(outputName.c_str()), PathUtil::GetExt(inputName)).c_str();
		}
	}
}

bool CGameCache::PrepareDBAsFor( const EntityId userId, const std::vector<string>& dbaGroups )
{
	return m_characterDBAs.AddUserToGroup(userId, dbaGroups);
}

void CGameCache::RemoveDBAUser( const EntityId userId )
{
	m_characterDBAs.RemoveUser(userId);
}


// ===========================================================================
//	Cache an entity archetype and everything that it is referencing.
//
//	In:		The entity archetype name. The archetype will be loaded/cached and 
//			have its CacheResources() Lua function called.
//
//	Returns:	Default Lua return value.
//
void CGameCache::CacheEntityArchetype(const char* archetypeName)
{
	IEntityArchetype* pArchetype = gEnv->pEntitySystem->LoadEntityArchetype(archetypeName);
	IF_UNLIKELY (pArchetype == NULL)
	{
		gEnv->pLog->LogError("GameCache: Failed to cache entity archetype '%s' because it doesn't exist.", archetypeName);
		return;
	}
	
	IEntityClass* pEntityClass = pArchetype->GetClass();
	IF_UNLIKELY (pEntityClass == NULL)
	{
		gEnv->pLog->LogError("GameCache: Failed to cache entity archetype '%s' because the archetype didn't point to a valid entity class.", archetypeName);
		return;
	}

	SmartScriptTable pEntityScriptTable = pEntityClass->GetScriptTable();
	IF_UNLIKELY (!pEntityScriptTable)
	{
		gEnv->pLog->LogError("GameCache: Failed to cache entity archetype '%s' because the entity class didn't point to a valid script table.", archetypeName);
		return;
	}

	SmartScriptTable pProperties = pArchetype->GetProperties();
	if (pProperties)
	{
		CacheFileModels(pProperties);
		CacheAdditionalParams(pProperties);
	}

	CacheCustomEntityResources(pEntityScriptTable);

	g_pGame->GetIGameFramework()->PreloadAnimatedCharacter(pEntityScriptTable.GetPtr());

	CHitDeathReactionsSystem &hitDeathReactionSystem = g_pGame->GetHitDeathReactionsSystem();
	hitDeathReactionSystem.PreloadActorData(pProperties);
}


// ============================================================================
//	Cache custom entity resources that are controlled via scripting.
//	
void CGameCache::CacheCustomEntityResources(SmartScriptTable pEntityScript)
{
	HSCRIPTFUNCTION cacheResourcesFunctions(NULL);
	if (pEntityScript && pEntityScript->GetValue("CacheResources", cacheResourcesFunctions))
	{
		Script::Call(gEnv->pScriptSystem, cacheResourcesFunctions, pEntityScript);

		gEnv->pScriptSystem->ReleaseFunc(cacheResourcesFunctions);
	}
}


void CGameCache::CacheBodyDamageProfile(const char* bodyDamageFileName, const char* bodyDamagePartsFileName)
{
	assert(bodyDamageFileName != NULL);
	assert(bodyDamagePartsFileName != NULL);

	CBodyDamageManager *bodyDamageManager = g_pGame->GetBodyDamageManager();
	assert(bodyDamageManager != NULL);

	SBodyDamageDef bodyDamageDef;
	bodyDamageManager->GetBodyDamageDef(bodyDamageFileName, bodyDamagePartsFileName, bodyDamageDef);

	if (!bodyDamageManager->CacheBodyDamage(bodyDamageDef))
	{
		gEnv->pLog->LogError("GameCache: Failed to cache body damage profile '%s' and '%s'!",
			bodyDamageFileName, bodyDamagePartsFileName);
	}
}


ILINE bool CGameCache::IsCharacterFileModelCached( const char* szFileName, uint32& outputFileNameHash ) const
{
	outputFileNameHash = 0;

	if (szFileName && szFileName[0])
	{
		outputFileNameHash = CCrc32::ComputeLowercase(szFileName);
		if (gEnv->IsEditor())
		{
			for (uint32 i = eCFMCache_Default; i < eCFMCache_COUNT; ++i)
			{	
				TEditorCharacterFileModelCache::const_iterator itCache = m_editorCharacterFileModelCache[i].find(outputFileNameHash);

				if (itCache == m_editorCharacterFileModelCache[i].end())
					continue;

				return true;
			}
		}
	}

	return false;
}

#if GAME_CACHE_DEBUG
void CGameCache::Debug()
{
	m_characterDBAs.Debug();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool CGameCharacterDBAs::AddUserToGroup( const EntityId userId, const std::vector<string>& dbaGroups )
{
	if (!IsEnabled() || dbaGroups.empty())
		return false;

	if (IsAlreadyRegistered(userId))
	{
		GameWarning("[Game Character DBA] Trying to register same user twice '%d'", userId);
		return true;
	}

	//Add the new user
	m_dbaGroupUsers.push_back(SDBAGroupUser());

	SDBAGroupUser& newUser = m_dbaGroupUsers.back();
	newUser.m_userId = userId;

	//Update DBA groups
	const int dbaGroupCount = dbaGroups.size();
	for (int i = 0; i < dbaGroupCount; ++i)
	{
		const int groupIndex = GetGroupIndexByName(dbaGroups[i].c_str());
		SDBAGroup* pDBAGroup = GetGroupByIndex(groupIndex);
		if (pDBAGroup)
		{
			if (pDBAGroup->m_userCount == 0)
			{
				//First user? Then prepare the dba's
				for (size_t j = 0; j < pDBAGroup->m_dbas.size(); ++j)
				{
					gEnv->pCharacterManager->DBA_LockStatus(pDBAGroup->m_dbas[j].c_str(), 1, ICharacterManager::eStreamingDBAPriority_Normal);
				}
			}
			pDBAGroup->m_userCount++;

			newUser.m_dbaGroupIndices.push_back(groupIndex);
		}
	}


	return true;
}

//////////////////////////////////////////////////////////////////////////
void CGameCharacterDBAs::RemoveUser( const EntityId userId )
{
	TDBAGroupUsers::iterator userCit = std::find(m_dbaGroupUsers.begin(), m_dbaGroupUsers.end(), userId);

	if (userCit != m_dbaGroupUsers.end())
	{
		const SDBAGroupUser& userInfo = *userCit;

		for (size_t i = 0; i < userInfo.m_dbaGroupIndices.size(); ++i)
		{
			const int groupIdx = userInfo.m_dbaGroupIndices[i];
			CRY_ASSERT((groupIdx >= 0) && (groupIdx < (int)m_dbaGroups.size()));

			SDBAGroup& groupInfo = m_dbaGroups[groupIdx];
			groupInfo.m_userCount--;

			CRY_ASSERT(groupInfo.m_userCount >= 0);

			if (groupInfo.m_userCount == 0)
			{
				//Last user of this group, unlock dba's
				for (size_t j = 0; j < groupInfo.m_dbas.size(); ++j)
				{
					gEnv->pCharacterManager->DBA_LockStatus(groupInfo.m_dbas[j].c_str(), 0, ICharacterManager::eStreamingDBAPriority_Normal);
				}
			}
		}

		m_dbaGroupUsers.erase(userCit);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameCharacterDBAs::Reset()
{
	for(TCharacterDBAGroups::iterator groupCit = m_dbaGroups.begin(); groupCit != m_dbaGroups.end(); ++groupCit)
	{
		SDBAGroup& groupInfo = *groupCit;
		
		if (groupInfo.m_userCount > 0)
		{
			groupInfo.m_userCount = 0;
			for (size_t i = 0; i < groupInfo.m_dbas.size(); ++i)
			{
				gEnv->pCharacterManager->DBA_Unload(groupInfo.m_dbas[i].c_str());
			}
		}
	}

	stl::free_container(m_dbaGroupUsers);
}

//////////////////////////////////////////////////////////////////////////
void CGameCharacterDBAs::PreCacheForLevel()
{
	if (IsEnabled())
	{
		m_dbaGroupUsers.reserve(32); //More than enough for max number of AI
	}
}

//////////////////////////////////////////////////////////////////////////
int CGameCharacterDBAs::GetGroupIndexByName( const char* groupName ) const
{
	CryHashStringId groupId(CryStringUtils::HashString(groupName));

	const int groupCount = m_dbaGroups.size();
	int groupIndex = 0;

	while ((groupIndex < groupCount) && (m_dbaGroups[groupIndex].m_groupId != groupId))
	{
		groupIndex++;
	}

	return (groupIndex < groupCount) ? groupIndex : -1;
}

//////////////////////////////////////////////////////////////////////////
CGameCharacterDBAs::SDBAGroup* CGameCharacterDBAs::GetGroupByIndex(int index)
{
	if (index >= 0)
	{
		CRY_ASSERT(index < (int)m_dbaGroups.size());
		return &m_dbaGroups[index];
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
bool CGameCharacterDBAs::IsAlreadyRegistered( const EntityId userId )
{
	return (std::find(m_dbaGroupUsers.begin(), m_dbaGroupUsers.end(), userId) != m_dbaGroupUsers.end());
}

//////////////////////////////////////////////////////////////////////////
bool CGameCharacterDBAs::IsEnabled() const
{	
#ifndef _RELEASE
	bool charactersDbaManagementEnable = (g_pGameCVars->g_charactersDbaManagementEnable != 0);
#else
	bool charactersDbaManagementEnable = true;
#endif
	return (!gEnv->bMultiplayer) && (!gEnv->IsEditor()) && charactersDbaManagementEnable;
}

//////////////////////////////////////////////////////////////////////////
void CGameCharacterDBAs::GetMemoryUsage( ICrySizer *s ) const
{
	s->AddContainer(m_dbaGroups);
	s->AddContainer(m_dbaGroupUsers);
}

//////////////////////////////////////////////////////////////////////////
void CGameCharacterDBAs::LoadXmlData()
{
	const char* filePath = "Scripts/Entities/actor/Animation/CharacterDBAs.xml";

	XmlNodeRef xmlRoot = gEnv->pSystem->LoadXmlFromFile(filePath);
	if (!xmlRoot)
	{
		GameWarning("Failed to load character dba information '%s'", filePath);
		return;
	}

	const int dbaGroupCount = xmlRoot->getChildCount();
	m_dbaGroups.reserve(dbaGroupCount);
	
//Extra debug to ensure one dba does not go to multiple groups
#ifdef _DEBUG
	std::set<string>	alreadyParsedDBAs;
#endif

	for (int i = 0; i < dbaGroupCount; ++i)
	{
		//Create the group
		XmlNodeRef groupNode = xmlRoot->getChild(i);

		const char* groupName = NULL;
		
		if(groupNode->haveAttr("name"))
		{
			groupName = groupNode->getAttr("name");
		}
		else
		{
			groupName = "DummyGroup";
			GameWarning("[Game Character DBA] Loading group with no name, defaulting to 'DummyGroup'");
		}
		
		m_dbaGroups.push_back(SDBAGroup());

		SDBAGroup& groupInfo = m_dbaGroups[i];

		groupInfo.m_groupId.Set(groupName);

		//Insert dba's in the group
		const int dbaCount = groupNode->getChildCount();
		groupInfo.m_dbas.reserve(dbaCount);

		for (int j = 0; j < dbaCount; ++j)
		{
			XmlNodeRef dbaNode = groupNode->getChild(j);
			
			string newDba = PathUtil::ToUnixPath(string(dbaNode->getAttr("name")));

#ifdef _DEBUG
			CRY_ASSERT_MESSAGE(alreadyParsedDBAs.find(newDba) == alreadyParsedDBAs.end(), "Multiple groups contain this DBA, bad XML data!");
			alreadyParsedDBAs.insert(newDba);
#endif
			groupInfo.m_dbas.push_back(newDba);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
#if GAME_CACHE_DEBUG
void CGameCharacterDBAs::Debug()
{
	if (g_pGameCVars->g_charactersDbaManagementDebug == 0)
		return;

	const float white[4]	= {1.0f, 10.f, 1.0f, 1.0f};
	const float grey[4]	= {0.6f, 0.6f, 0.6f, 1.0f};

	if (IsEnabled())
	{
		float posY = 50.f;
		float posX = 50.f;

		IRenderAuxText::Draw2dLabel(posX, posY, 1.5f, white, false, "Currently locked Character DBAs");
		posY += 15.0f;
		IRenderAuxText::Draw2dLabel(posX, posY, 1.5f, white, false, "======================================");
		posY += 15.0f;

		for (size_t i = 0; i < m_dbaGroups.size(); ++i)
		{
			if (m_dbaGroups[i].m_userCount == 0)
				continue;

			IRenderAuxText::Draw2dLabel(posX, posY, 1.5f, white, false, "Group: '%s' - Users: '%d'", m_dbaGroups[i].m_groupId.GetDebugName(), m_dbaGroups[i].m_userCount);
			posY += 15.0f;

			for (size_t j = 0; j < m_dbaGroups[i].m_dbas.size(); ++j)
			{
				IRenderAuxText::Draw2dLabel(posX + 50.0f, posY, 1.5f, grey, false, "DBA Name: '%s'", m_dbaGroups[i].m_dbas[j].c_str());
				posY += 15.0f;
			}

			posY += 5.0f;
		}
	}
	else
	{
		IRenderAuxText::Draw2dLabel(50.0f, 50.0f, 1.5f, white, false, "Game DBA management for characters disabled");
	}
}
#endif

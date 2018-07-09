// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 18:10:2005   18:00 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include <CryEntitySystem/IEntitySystem.h>
#include <CrySystem/File/ICryPak.h>
#include <CryScriptSystem/IScriptSystem.h>
#include "IGameObject.h"
#include "Actor.h"
#include "Player.h"
#include "ItemParams.h"
#include "WeaponSystem.h"
#include "GameRules.h"
#include "WeaponSharedParams.h"
#include "ProjectileAutoAimHelper.h"
#include "Projectile.h"
#include "Bullet.h"
#include "Rocket.h"
#include "HomingMissile.h"
#include "C4Projectile.h"
#include "Grenade.h"
#include "MikeBullet.h"
#include "LightningBolt.h"
#include "LTAGGrenade.h"
#include "Single.h"
#include "Automatic.h"
#include "Burst.h"
#include "Rapid.h"
#include "Throw.h"
#include "Plant.h"
#include "Detonate.h"
#include "Charge.h"
#include "Shotgun.h"
#include "Melee.h"
#include "Chaff.h"
#include "AutomaticShotgun.h"
#include "LTagSingle.h"
#include "Spammer.h"
#include "HommingSwarmProjectile.h"
#include "KVoltBullet.h"
#include "DataPatchDownloader.h"
#include "EMPGrenade.h"
#include "IronSight.h"
#include "Scope.h"
#include "ThrowIndicator.h"
#include "RecordingSystem.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"

#include "FireModePlugin.h"

#define LINKED_PROJ_MAP_RESERVE 24 //3 shots of 8 pellets should be plenty

//////////////////////////////////////////////////////////////////////////
//Zoom/Fire-modes pool allocator
//////////////////////////////////////////////////////////////////////////

template <class TInterface, class TImplementation>
class CWeaponComponentAllocator
{
	typedef stl::PoolAllocator<sizeof(TImplementation), stl::PoolAllocatorSynchronizationSinglethreaded> WeaponComponentPool;

public:
	ILINE static TInterface * Create()
	{
		return new (m_memoryPool.Allocate()) TImplementation();
	}

	ILINE static void Release(TImplementation* pObject)
	{
		CRY_ASSERT(pObject);

		pObject->~TImplementation();
		m_memoryPool.Deallocate(pObject);
	}

	ILINE static void FreeMemoryPool()
	{
		const size_t memoryUsed = m_memoryPool.GetTotalMemory().nUsed;

		CRY_ASSERT_MESSAGE(memoryUsed == 0, "Freeing memory pool, but there is still objects using it!");
		if (memoryUsed != 0)
		{
			CryLog("WeaponComponentAllocator: Memory pool for '%s' will be freed but is still in use! Current used size '%" PRISIZE_T "'", TImplementation::GetStaticType()->GetName(), memoryUsed);
		}

		m_memoryPool.FreeMemory();
	}

private:

	static WeaponComponentPool m_memoryPool;
};

template <class TInterface, class TImplementation>
typename CWeaponComponentAllocator<TInterface, TImplementation>::WeaponComponentPool CWeaponComponentAllocator<TInterface, TImplementation>::m_memoryPool;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <typename TWeaponCompnentInterface, typename TWeaponComponentImplementation> TWeaponCompnentInterface*	CreateWeaponComponentFromPool()
{
	return CWeaponComponentAllocator<TWeaponCompnentInterface, TWeaponComponentImplementation>::Create();
}

template <typename TWeaponCompnentInterface, typename TWeaponComponentImplementation> void ReturnWeaponComponentToPool(TWeaponCompnentInterface* pWeaponComponentObject)
{
	CWeaponComponentAllocator<TWeaponCompnentInterface, TWeaponComponentImplementation>::Release(static_cast<TWeaponComponentImplementation*>(pWeaponComponentObject));
}

template <typename TWeaponCompnentInterface, typename TWeaponComponentImplementation> void FreeWeaponComponentPool()
{
	CWeaponComponentAllocator<TWeaponCompnentInterface, TWeaponComponentImplementation>::FreeMemoryPool();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define REGISTER_PROJECTILE(name, T)	\
struct C##name##Creator : public IGameObjectExtensionCreatorBase	\
{ \
	IGameObjectExtension* Create(IEntity *pEntity) \
	{ \
		return pEntity->GetOrCreateComponentClass<T>();\
	} \
	void GetGameObjectExtensionRMIData( void ** ppRMI, size_t * nCount ) \
	{ \
		T::GetGameObjectExtensionRMIData( ppRMI, nCount ); \
	} \
}; \
static C##name##Creator _##name##Creator; \
RegisterProjectile(#name, &_##name##Creator);

//------------------------------------------------------------------------
CWeaponSystem::CWeaponSystem(CGame *pGame, ISystem *pSystem)
: m_pGame(pGame),
	m_pSystem(pSystem),
	m_pItemSystem(pGame->GetIGameFramework()->GetIItemSystem()),
	m_pPrecache(0),
	m_reloading(false),
	m_recursing(false)
{
	// register fire modes here
	RegisterFireMode<CSingle>("Single");
	RegisterFireMode<CAutomatic>("Automatic");
	RegisterFireMode<CBurst>("Burst");
	RegisterFireMode<CRapid>("Rapid");
	RegisterFireMode<CThrow>("Throw");
	RegisterFireMode<CPlant>("Plant");
	RegisterFireMode<CDetonate>("Detonate");
	RegisterFireMode<CCharge>("Charge");
	RegisterFireMode<CShotgun>("Shotgun");	
	RegisterFireMode<CAutomaticShotgun>("AutomaticShotgun");
	RegisterFireMode<CLTagSingle>("LTagSingle");
	RegisterFireMode<CSpammer>("Spammer");
	
	// register zoom modes here
	RegisterZoomMode<CIronSight>("IronSight");
	RegisterZoomMode<CScope>("Scope");
	RegisterZoomMode<CThrowIndicator>("ThrowIndicator");

	// register firemode plugins here
	RegisterFireModePlugin<CFireModePlugin_Overheat>();
	RegisterFireModePlugin<CFireModePlugin_Reject>();
	RegisterFireModePlugin<CFireModePlugin_AutoAim>();
	RegisterFireModePlugin<CFireModePlugin_RecoilShake>();

	// register projectile classes here
	REGISTER_PROJECTILE(Projectile, CProjectile);
	REGISTER_PROJECTILE(Bullet, CBullet);
	REGISTER_PROJECTILE(KVoltBullet, CKVoltBullet);
	REGISTER_PROJECTILE(Rocket, CRocket);
	REGISTER_PROJECTILE(HomingMissile, CHomingMissile);
	REGISTER_PROJECTILE(C4Projectile, CC4Projectile); 
	REGISTER_PROJECTILE(Chaff, CChaff);
	REGISTER_PROJECTILE(Grenade, CGrenade);
	REGISTER_PROJECTILE(SmokeGrenade, CSmokeGrenade);
	REGISTER_PROJECTILE(MikeBullet, CMikeBullet);
	REGISTER_PROJECTILE(LightningBolt, CLightningBolt);
	REGISTER_PROJECTILE(HommingSwarmProjectile, CHommingSwarmProjectile);
	REGISTER_PROJECTILE(LTagGrenade, CLTAGGrenade);
	REGISTER_PROJECTILE(EMPGrenade, CEMPGrenade);

	m_pPrecache = gEnv->pConsole->GetCVar("i_precache");

	CBullet::EntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Bullet");

	m_pGame->GetIGameFramework()->GetILevelSystem()->AddListener(this);

	m_linkedProjectiles.reserve(LINKED_PROJ_MAP_RESERVE);

	m_freePoolHandlers.insert(TWeaponComponentPoolFreeFunctions::value_type(CMelee::GetWeaponComponentType(), &FreeWeaponComponentPool<CMelee, CMelee>));

	m_listenersLock = 0;
}

//------------------------------------------------------------------------
CWeaponSystem::~CWeaponSystem()
{
	//DumpPoolSizes();
	FreePools();

	// cleanup current projectiles
	for (TProjectileMap::iterator pit = m_projectiles.begin(); pit != m_projectiles.end(); ++pit)
	{
		// Do not forcefully remove entities here as an entity shutdown will remove from "m_projectiles"!
		gEnv->pEntitySystem->RemoveEntity(pit->first);
	}

	for (TAmmoTypeParams::iterator it = m_ammoparams.begin(); it != m_ammoparams.end(); ++it)
	{
		SAmmoTypeDesc &desc=it->second;
		delete desc.params;
	}

	m_pGame->GetIGameFramework()->GetILevelSystem()->RemoveListener(this);
}

//------------------------------------------------------------------------
void CWeaponSystem::Update(float frameTime)
{
	m_tracerManager.Update(frameTime);
	m_detonationRMIQueue.Update(frameTime);

#ifdef DEBUG_BULLET_PENETRATION
	if (g_pGameCVars->g_bulletPenetrationDebug)
	{
		CBullet::UpdateBulletPenetrationDebug(frameTime);
	}
#endif

#ifndef _RELEASE 
	if (g_pGameCVars->i_debug_itemparams_memusage)
	{
		g_pGame->GetGameSharedParametersStorage()->GetDetailedItemParamMemoryStatistics();
	}

	if (g_pGameCVars->i_debug_weaponparams_memusage)
	{
		g_pGame->GetGameSharedParametersStorage()->GetDetailedWeaponParamMemoryStatistics();
	}
#endif
}

//------------------------------------------------------------------------
void CWeaponSystem::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CWeaponSystem::Reload()
{
	m_reloading = true;

	// cleanup current projectiles
	for (TProjectileMap::iterator pit = m_projectiles.begin(); pit != m_projectiles.end();)
	{
		//Bugfix: RemoveEntity removes projectile from map, thus invalidating iterator
		TProjectileMap::iterator next = pit;        
		++next;
		gEnv->pEntitySystem->RemoveEntity(pit->first, true);
		pit = next;
	}
	m_projectiles.clear();

	for (TAmmoTypeParams::iterator it = m_ammoparams.begin(); it != m_ammoparams.end(); ++it)
	{
		SAmmoTypeDesc &desc=it->second;
		delete desc.params;
	}

	m_ammoparams.clear();
	m_tracerManager.Reset();
	m_weaponAlias.Reset();

	for (TFolderList::iterator it=m_folders.begin(); it!=m_folders.end(); ++it)
	{
		Scan(it->c_str());
	}

	LoadItemParams(g_pGame->GetIGameFramework()->GetIItemSystem());

	m_reloading = false;
}

void CWeaponSystem::LoadItemParams(IItemSystem* pItemSystem)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "WeaponSystem: Load Item Params" );

	LOADING_TIME_PROFILE_SECTION(gEnv->pSystem);
	
	int numItems = pItemSystem->GetItemParamsCount();

	CGameSharedParametersStorage* pStorage = g_pGame->GetGameSharedParametersStorage();

	pStorage->ClearItemParamSets();

	for(int i = 0; i < numItems; i++)
	{
		SLICE_AND_SLEEP();

		const char* pItemName = pItemSystem->GetItemParamName(i);
		const char* itemFile = pItemSystem->GetItemParamsDescriptionFile(pItemName);

		CItemSharedParams* pItemShared = pStorage->GetItemSharedParameters(pItemName, true);
		XmlNodeRef itemRootNodeParams = m_pSystem->LoadXmlFromFile(itemFile);

		if (!itemRootNodeParams)
		{
			return;
		}

		CRY_ASSERT_MESSAGE(pItemShared, "Failed to create item params");
		CRY_ASSERT_MESSAGE(itemRootNodeParams != NULL, "No xml found for item params");

		if (!itemRootNodeParams)
		{
			return;
		}

		if (!pItemShared)
		{
			GameWarning("Warning: ItemParams for item <%s> NULL", pItemName);
			continue;
		}

		XmlNodeRef overrideParamsNode;

		const char* inheritItem = itemRootNodeParams->getAttr("inherit");
		bool isInherited = (inheritItem && inheritItem[0] != 0);
		if (isInherited)
		{
			const char* baseItemFile = pItemSystem->GetItemParamsDescriptionFile(inheritItem);
			overrideParamsNode = itemRootNodeParams;
			itemRootNodeParams = gEnv->pSystem->LoadXmlFromFile(baseItemFile);
			CRY_ASSERT_MESSAGE(itemRootNodeParams != NULL, "No xml found for base item params");

			m_weaponAlias.AddAlias(inheritItem, pItemName);
		}

		bool paramsRead = pItemShared->ReadItemParams(itemRootNodeParams);
		if (paramsRead && overrideParamsNode)
			pItemShared->ReadOverrideItemParams(overrideParamsNode);

		if (paramsRead)
		{
			int isWeapon = 0;

			itemRootNodeParams->getAttr("weaponParams", isWeapon);

			if(isWeapon)
			{
				CWeaponSharedParams* pWeaponShared = pStorage->GetWeaponSharedParameters(pItemName, true);

				CRY_ASSERT_MESSAGE(pWeaponShared, "Failed to create weapon params");

				if(pWeaponShared)
				{
					pWeaponShared->ReadWeaponParams(itemRootNodeParams, pItemShared, pItemName);
				}
			}	
		}
		else
		{
			GameWarning("Warning: ItemParams for item <%s> NULL", pItemName);
		}
	}

	m_itemPackages.Load();
}

//------------------------------------------------------------------------
void CWeaponSystem::OnLoadingStart(ILevelInfo *pLevel)
{
	CCCPOINT(WeaponSystem_OnLoadingStart);

	if (gEnv->IsEditor())
	{
		for (TWeaponComponentPoolFreeFunctions::iterator poolIt = m_freePoolHandlers.begin(); poolIt != m_freePoolHandlers.end(); ++poolIt)
		{
			poolIt->second();
		}
	}

	if (CDataPatchDownloader *pPatcher=g_pGame->GetDataPatchDownloader())
	{
		if (pPatcher->NeedsWeaponSystemReload())
		{
			Reload();			// allows patches to be applied (if any) takes about 1.2 sec on xbox release build
			pPatcher->DoneWeaponSystemReload();
		}
	}

	CRY_ASSERT(m_linkedProjectiles.size() == 0);
}

//------------------------------------------------------------------------
void CWeaponSystem::OnLoadingComplete(ILevelInfo* pLevel)
{
	CCCPOINT(WeaponSystem_OnLoadingComplete);
}

//------------------------------------------------------------------------
void CWeaponSystem::OnUnloadComplete(ILevelInfo* pLevel)
{
	if (!gEnv->IsEditor())
	{
		for (TWeaponComponentPoolFreeFunctions::iterator poolIt = m_freePoolHandlers.begin(); poolIt != m_freePoolHandlers.end(); ++poolIt)
		{
			poolIt->second();
		}
	}

	CRY_ASSERT(m_linkedProjectiles.size() == 0);
}

//------------------------------------------------------------------------
IFireModePlugin* CWeaponSystem::CreateFireModePlugin(const CGameTypeInfo* pluginType)
{
	TFireModePluginCreationRegistry::iterator it = m_pluginCreationRegistry.find(pluginType);
	
	if (it != m_pluginCreationRegistry.end())
	{
		return it->second();
	}

	return NULL;
}

//------------------------------------------------------------------------
template<typename PlugInType>
void CWeaponSystem::RegisterFireModePlugin()
{
	m_pluginCreationRegistry.insert(TFireModePluginCreationRegistry::value_type(PlugInType::GetStaticType(), &CreateWeaponComponentFromPool<IFireModePlugin, PlugInType>));
	m_pluginDestructionRegistry.insert(TFireModePluginDestructionRegistry::value_type(PlugInType::GetStaticType(), &ReturnWeaponComponentToPool<IFireModePlugin, PlugInType>));
	m_freePoolHandlers.insert(TWeaponComponentPoolFreeFunctions::value_type(PlugInType::GetStaticType()->GetName(), &FreeWeaponComponentPool<IFireModePlugin, PlugInType>));
}

//------------------------------------------------------------------------
void CWeaponSystem::DestroyFireModePlugin(IFireModePlugin* pObject)
{
	CRY_ASSERT(pObject);

	TFireModePluginDestructionRegistry::iterator it = m_pluginDestructionRegistry.find(pObject->GetRunTimeType());
	if (it != m_pluginDestructionRegistry.end())
	{
		it->second(pObject);
	}
	else
	{
		CRY_ASSERT_MESSAGE(false, "Destroy fire mode plugin couldn't be executed, fire mode plugin type destructor method not found, leaking memory!");
		CryLogAlways("Destroy fire mode plugin couldn't be executed for fire mode plugin type '%s', leaking memory!", pObject->GetRunTimeType()->GetName());
	}
}

//------------------------------------------------------------------------
CFireMode *CWeaponSystem::CreateFireMode(const char *name)
{
	TFireModeCreationRegistry::iterator it = m_fmCreationRegistry.find(CONST_TEMP_STRING(name));
	if (it != m_fmCreationRegistry.end())
		return it->second();
	return 0;
}

//------------------------------------------------------------------------
template<typename FireModeType>
void CWeaponSystem::RegisterFireMode(const char* componentName)
{
	m_fmCreationRegistry.insert(TFireModeCreationRegistry::value_type(componentName, &CreateWeaponComponentFromPool<CFireMode, FireModeType>));
	m_fmDestructionRegistry.insert(TFireModeDestructionRegistry::value_type(FireModeType::GetStaticType(), &ReturnWeaponComponentToPool<CFireMode, FireModeType>));
	m_freePoolHandlers.insert(TWeaponComponentPoolFreeFunctions::value_type(FireModeType::GetStaticType()->GetName(), &FreeWeaponComponentPool<CFireMode, FireModeType>));
}

//------------------------------------------------------------------------
void CWeaponSystem::DestroyFireMode(CFireMode* pObject)
{
	CRY_ASSERT(pObject);

	TFireModeDestructionRegistry::iterator it = m_fmDestructionRegistry.find(pObject->GetRunTimeType());
	if (it != m_fmDestructionRegistry.end())
	{
		it->second(pObject);
	}
	else
	{
		CRY_ASSERT_MESSAGE(false, "Destroy fire mode couldn't be executed, fire mode type destructor method not found, leaking memory!");
		CryLogAlways("Destroy fire mode couldn't be executed for fire mode type '%s', leaking memory!", pObject->GetRunTimeType()->GetName());
	}
}

//------------------------------------------------------------------------
CMelee* CWeaponSystem::CreateMeleeMode()
{
	return CWeaponComponentAllocator<CMelee, CMelee>::Create();
}

//------------------------------------------------------------------------
void CWeaponSystem::DestroyMeleeMode(CMelee* pObject)
{
	CRY_ASSERT(pObject);

	CWeaponComponentAllocator<CMelee, CMelee>::Release(pObject);
}

//------------------------------------------------------------------------
IZoomMode *CWeaponSystem::CreateZoomMode(const char *name)
{
	TZoomModeCreationRegistry::iterator it = m_zmCreationRegistry.find(CONST_TEMP_STRING(name));
	if (it != m_zmCreationRegistry.end())
		return it->second();
	return 0;
}

//------------------------------------------------------------------------
void CWeaponSystem::DestroyZoomMode(CIronSight* pObject)
{
	CRY_ASSERT(pObject);

	TZoomModeDestructionRegistry::iterator it = m_zmDestructionRegistry.find(pObject->GetRunTimeType());
	if (it != m_zmDestructionRegistry.end())
	{
		it->second(pObject);
	}
	else
	{
		CRY_ASSERT_MESSAGE(false, "Destroy zoom mode couldn't be executed, zoom mode type destructor method not found, leaking memory!");
		CryLogAlways("Destroy zoom mode couldn't be executed for zoom mode type '%s', leaking memory!", pObject->GetRunTimeType()->GetName());
	}
}

//------------------------------------------------------------------------
template<typename ZoomType>
void CWeaponSystem::RegisterZoomMode(const char* componentName)
{
	m_zmCreationRegistry.insert(TZoomModeCreationRegistry::value_type(componentName, &CreateWeaponComponentFromPool<IZoomMode, ZoomType>));
	m_zmDestructionRegistry.insert(TZoomModeDestructionRegistry::value_type(ZoomType::GetStaticType(), &ReturnWeaponComponentToPool<IZoomMode, ZoomType>));
	m_freePoolHandlers.insert(TWeaponComponentPoolFreeFunctions::value_type(ZoomType::GetStaticType()->GetName(), &FreeWeaponComponentPool<IZoomMode, ZoomType>));
}

//------------------------------------------------------------------------
const SAmmoParams* CWeaponSystem::GetAmmoParams( const IEntityClass* pAmmoType )
{
	TAmmoTypeParams::const_iterator it = m_ammoparams.find(pAmmoType);
	if (it == m_ammoparams.end())
	{
		GameWarning("Failed to spawn ammo '%s'! Unknown class or entity class not registered...", pAmmoType?pAmmoType->GetName():"");
		return NULL;
	}

	return it->second.params;
}

//------------------------------------------------------------------------
CProjectile *CWeaponSystem::SpawnAmmo(IEntityClass* pAmmoType, bool isRemote)
{
	const SAmmoParams* pAmmoParams = GetAmmoParams(pAmmoType);
	if(!pAmmoParams)
		return NULL;

	if (pAmmoParams->reusable)
	{
		if (isRemote || (!pAmmoParams->serverSpawn &&
			(pAmmoParams->flags&(ENTITY_FLAG_CLIENT_ONLY|ENTITY_FLAG_SERVER_ONLY))))
			return UseFromPool(pAmmoType, pAmmoParams);
	}

	return DoSpawnAmmo(pAmmoType, isRemote, pAmmoParams);
}


//------------------------------------------------------------------------
CProjectile *CWeaponSystem::DoSpawnAmmo(IEntityClass* pAmmoType, bool isRemote, const SAmmoParams *pAmmoParams)
{
	bool isServer=gEnv->bServer;

	if ( pAmmoParams->serverSpawn && (!isServer || IsDemoPlayback()) )
	{
		if (!pAmmoParams->predictSpawn || isRemote)
			return 0;
	}

	SEntitySpawnParams spawnParams;
	spawnParams.pClass = pAmmoType;
	spawnParams.sName = "ammo";
	spawnParams.nFlags = pAmmoParams->flags | ENTITY_FLAG_NO_PROXIMITY; // No proximity for this entity.
	if (pAmmoParams->serverSpawn)
	{
		// This projectile is going to be bound to the network, make sure we know it's a dynamic entity
		spawnParams.nFlags |= ENTITY_FLAG_NEVER_NETWORK_STATIC;
	}
	
	IEntity *pEntity = NULL;

	if(pAmmoType)
		pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams);

	if (!pEntity)
	{
#if !defined(_RELEASE)
		CRecordingSystem* recordingSystem = g_pGame->GetRecordingSystem();
		if (!(recordingSystem && recordingSystem->IsPlayingBack()))
		{
			// This is normal behaviour during killcam replay, certain entities will be prevented from spawning
			GameWarning("Failed to spawn ammo '%s'! Entity creation failed...", pAmmoType ? pAmmoType->GetName() : "<unknown ammo type class>");
		}
#endif
		return 0;
	}

	CProjectile *pProjectile = GetProjectile(pEntity->GetId());

	if (pProjectile && !isServer && !isRemote && pAmmoParams->predictSpawn)
		pProjectile->GetGameObject()->RegisterAsPredicted();

	return pProjectile;
}

//------------------------------------------------------------------------
bool CWeaponSystem::IsServerSpawn(IEntityClass* pAmmoType) const
{
	if (!pAmmoType)
		return false;

	if (const SAmmoParams *pAmmoParams=GetAmmoParams(pAmmoType))
		return pAmmoParams->serverSpawn!=0;
	return false;
}

//-------------------------------------------	-----------------------------
void CWeaponSystem::RegisterProjectile(const char *name, IGameObjectExtensionCreatorBase *pCreator)
{
	m_projectileregistry.insert(TProjectileRegistry::value_type(name, pCreator));
}

//------------------------------------------------------------------------
const SAmmoParams* CWeaponSystem::GetAmmoParams( const IEntityClass* pAmmoType ) const
{
	TAmmoTypeParams::const_iterator it=m_ammoparams.find(pAmmoType);
	if (it != m_ammoparams.end())
	{
		return it->second.params;
	}

	return NULL;
}

//------------------------------------------------------------------------
void CWeaponSystem::AddProjectile(IEntity *pEntity, CProjectile *pProjectile)
{
	m_projectiles.insert(TProjectileMap::value_type(pEntity->GetId(), pProjectile));
}

//------------------------------------------------------------------------
void CWeaponSystem::RemoveProjectile(CProjectile *pProjectile)
{
	m_projectiles.erase(pProjectile->GetEntity()->GetId());

	RemoveFromPool(pProjectile);
}

//-----------------------------------------------------------------------
void CWeaponSystem::AddLinkedProjectile(EntityId projId, EntityId weaponId, uint8 shotId)
{
	SLinkedProjectileInfo linkInfo(weaponId, shotId);

	m_linkedProjectiles.insert(TLinkedProjectileMap::value_type(projId, linkInfo));
}

//-----------------------------------------------------------------------
void CWeaponSystem::RemoveLinkedProjectile(EntityId projId)
{
	TLinkedProjectileMap::iterator projIt = m_linkedProjectiles.find(projId);

	if(projIt != m_linkedProjectiles.end())
	{
		m_linkedProjectiles.erase(projIt);
	}
}

//------------------------------------------------------------------------
CProjectile *CWeaponSystem::GetProjectile(EntityId entityId)
{
	TProjectileMap::iterator it = m_projectiles.find(entityId);
	if (it != m_projectiles.end())
		return it->second;
	return 0;
}

//------------------------------------------------------------------------
const IEntityClass *CWeaponSystem::GetProjectileClass( const EntityId entityId ) const
{
	TProjectileMap::const_iterator it = m_projectiles.find(entityId);
	if (it != m_projectiles.end())
	{
		return it->second->GetEntity()->GetClass();
	}

	return NULL;
}

//------------------------------------------------------------------------
int  CWeaponSystem::QueryProjectiles(SProjectileQuery& q)
{
    IEntityClass* pClass = q.ammoName?gEnv->pEntitySystem->GetClassRegistry()->FindClass(q.ammoName):0;
    m_queryResults.resize(0);
    if(q.box.IsEmpty())
    {
        for(TProjectileMap::iterator it = m_projectiles.begin();it!=m_projectiles.end();++it)
        {
            IEntity *pEntity = it->second->GetEntity();
            if(pClass == 0 || pEntity->GetClass() == pClass)
            m_queryResults.push_back(pEntity);
        }
    }
    else
    {
        for(TProjectileMap::iterator it = m_projectiles.begin();it!=m_projectiles.end();++it)
        {
            IEntity *pEntity = it->second->GetEntity();
            if(pEntity->GetClass() == pClass && q.box.IsContainPoint(pEntity->GetWorldPos()))
            {
                m_queryResults.push_back(pEntity);
            }
        }
    }
    
    q.nCount = int(m_queryResults.size());
    if(q.nCount)
        q.pResults = &m_queryResults[0];
    return q.nCount;
}

//------------------------------------------------------------------------
void CWeaponSystem::Scan(const char *folderName)
{
	stack_string folder = folderName;
	stack_string search = folder;
	stack_string subName;
	stack_string xmlFile;
	search += "/*.*";

	ICryPak *pPak = gEnv->pCryPak;

	_finddata_t fd;
	intptr_t handle = pPak->FindFirst(search.c_str(), &fd);

	if (!m_recursing)
		CryComment("Loading ammo XML definitions from '%s'!", folderName);

	if (handle > -1)
	{
		do
		{
			if (!strcmp(fd.name, ".") || !strcmp(fd.name, ".."))
				continue;

			if (fd.attrib & _A_SUBDIR)
			{
				subName = folder+"/"+fd.name;
				if (m_recursing)
					Scan(subName.c_str());
				else
				{
					m_recursing=true;
					Scan(subName.c_str());
					m_recursing=false;
				}
				continue;
			}

			if (stricmp(PathUtil::GetExt(fd.name), "xml"))
				continue;

			xmlFile = folder + "/" + fd.name;
			XmlNodeRef rootNode = m_pSystem->LoadXmlFromFile(xmlFile.c_str());

			if (!rootNode)
			{
				GameWarning("Invalid XML file '%s'! Skipping...", xmlFile.c_str());
				continue;
			}

			SLICE_AND_SLEEP();

			if (!ScanXML(rootNode, xmlFile.c_str()))
				continue;

		} while (pPak->FindNext(handle, &fd) >= 0);
	}

	if (!m_recursing)
		CryLog("Finished loading ammo XML definitions from '%s'!", folderName);

	if (!m_reloading && !m_recursing)
		m_folders.push_back(folderName);
}

//------------------------------------------------------------------------
bool CWeaponSystem::ScanXML(XmlNodeRef &root, const char *xmlFile)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "WeaponSystem");
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "Weapon xml (%s)", xmlFile);

	if (strcmpi(root->getTag(), "ammo"))
		return false;

	const char *name = root->getAttr("name");
	if (!name)
	{
		GameWarning("Missing ammo name in XML '%s'! Skipping...", xmlFile);
		return false;
	}

	const char *className = root->getAttr("class");

	if (!className)
	{
		GameWarning("Missing ammo class in XML '%s'! Skipping...", xmlFile);
		return false;
	}

	TProjectileRegistry::iterator it = m_projectileregistry.find(CONST_TEMP_STRING(className));
	if (it == m_projectileregistry.end())
	{
		GameWarning("Unknown ammo class '%s' specified in XML '%s'! Skipping...", className, xmlFile);
		return false;
	}

	const char *scriptName = root->getAttr("script");
	IEntityClassRegistry::SEntityClassDesc classDesc;
	classDesc.sName = name;
	classDesc.sScriptFile = scriptName?scriptName:"";
	//classDesc.pUserProxyData = (void *)it->second;
	//classDesc.pUserProxyCreateFunc = &CreateProxy<CProjectile>;
	classDesc.flags |= ECLF_INVISIBLE;

	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);

	if (!m_reloading && !pClass)
	{
		m_pGame->GetIGameFramework()->GetIGameObjectSystem()->RegisterExtension(name, it->second, &classDesc);
		pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
		assert(pClass);
	}


	TAmmoTypeParams::iterator ait = m_ammoparams.find(pClass);
	if (ait == m_ammoparams.end())
	{
		std::pair<TAmmoTypeParams::iterator, bool> result = m_ammoparams.insert(TAmmoTypeParams::value_type(pClass, SAmmoTypeDesc()));
		ait = result.first;
	}

	SAmmoTypeDesc &desc = ait->second;

	SAFE_DELETE(desc.params);
	desc.params = new SAmmoParams(root, pClass);;

	return true;
}

//------------------------------------------------------------------------
void CWeaponSystem::DebugGun(IConsoleCmdArgs *args)
{
  IGameFramework* pGF = gEnv->pGameFramework;  
  IItemSystem* pItemSystem = pGF->GetIItemSystem();
 
  IActor* pActor = pGF->GetClientActor();
  if (!pActor || !pActor->IsPlayer())
    return;

	IInventory *pInventory = pActor->GetInventory();
  if (!pInventory)
    return;  
  
  // give & select the debuggun 
	EntityId itemId = pInventory->GetItemByClass(CItem::sDebugGunClass);
  if (0 == itemId)        
  {
    // if actor doesn't have it, only give it in editor
    if (!gEnv->IsEditor())
      return;

	if(!CItem::sDebugGunClass)
	{
		CRY_ASSERT_MESSAGE(0, "DebugGun is not defined.");
		return;
	}

		itemId = pItemSystem->GiveItem(pActor, CItem::sDebugGunClass->GetName(), false, true, true);
  }
  pItemSystem->SetActorItem(pActor, itemId, true);      
}

//------------------------------------------------------------------------
void CWeaponSystem::RefGun(IConsoleCmdArgs *args)
{
	IGameFramework* pGF = gEnv->pGameFramework;  
	IItemSystem* pItemSystem = pGF->GetIItemSystem();

	IActor* pActor = pGF->GetClientActor();
	if (!pActor || !pActor->IsPlayer())
		return;

	IInventory *pInventory = pActor->GetInventory();
	if (!pInventory)
		return;

	// give & select the refgun 
	EntityId itemId = pInventory->GetItemByClass(CItem::sRefWeaponClass);
	if (0 == itemId)        
	{
		// if actor doesn't have it, only give it in editor
		if (!gEnv->IsEditor())
			return;

		if(!CItem::sRefWeaponClass)
		{
			CRY_ASSERT_MESSAGE(0, "RefWeapon is not defined.");
			return;
		}

		itemId = pItemSystem->GiveItem(pActor, CItem::sRefWeaponClass->GetName(), false, true, true);
	}
	pItemSystem->SetActorItem(pActor, itemId, true);   

}

//------------------------------------------------------------------------
void CWeaponSystem::CreatePool(IEntityClass *pClass)
{
	TAmmoPoolMap::iterator it=m_pools.find(pClass);

	if (it!=m_pools.end())
		return;

	m_pools.insert(TAmmoPoolMap::value_type(pClass, SAmmoPoolDesc()));
}

//------------------------------------------------------------------------
void CWeaponSystem::FreePool(IEntityClass *pClass)
{
	TAmmoPoolMap::iterator it=m_pools.find(pClass);

	if (it==m_pools.end())
		return;

	SAmmoPoolDesc &desc=it->second;

	while(!desc.frees.empty())
	{
		CProjectile *pFree=desc.frees.front();
		desc.frees.pop_front();

		gEnv->pEntitySystem->RemoveEntity(pFree->GetEntityId(), true);
		--desc.size;
	}

	m_pools.erase(it);
}

//------------------------------------------------------------------------
uint16 CWeaponSystem::GetPoolSize(IEntityClass *pClass)
{
	TAmmoPoolMap::iterator it=m_pools.find(pClass);

	if (it==m_pools.end())
		return 0;

	return it->second.size;
}

//------------------------------------------------------------------------
CProjectile *CWeaponSystem::UseFromPool(IEntityClass *pClass, const SAmmoParams *pAmmoParams)
{
	TAmmoPoolMap::iterator it=m_pools.find(pClass);

	if (it==m_pools.end())
	{
		CreatePool(pClass);
		it=m_pools.find(pClass);
	}

	SAmmoPoolDesc &desc=it->second;

	if (!desc.frees.empty())
	{
		CProjectile *pProjectile=desc.frees.front();
		desc.frees.pop_front();

		pProjectile->GetEntity()->Hide(false);
		pProjectile->ReInitFromPool();
		return pProjectile;
	}
	else
	{
		CProjectile *pProjectile=DoSpawnAmmo(pClass, false, pAmmoParams);
		++desc.size;
		
		return pProjectile;
	}
}

//------------------------------------------------------------------------
bool CWeaponSystem::ReturnToPool(CProjectile *pProjectile)
{
	bool bSuccess = false;
	IEntityClass* const pProjectileClass = pProjectile->GetEntity()->GetClass();

	if (!m_pools.empty())
	{
		TAmmoPoolMap::iterator it=m_pools.find(pProjectileClass);
		assert(it!=m_pools.end());

		// It should not happen, but looks like it can some how while load/saving under certain circumstances...
		if(it != m_pools.end())
		{
			it->second.frees.push_back(pProjectile);

			pProjectile->GetEntity()->Hide(true);
			pProjectile->GetEntity()->SetWorldTM(IDENTITY);
			bSuccess = true;
		}
		else
		{
			// Log trace, and return false (projectile will handle it)
			GameWarning("CWeaponSystem::ReturnToPool(): Trying to return projectile to a pool that doesn't exist (Class: %s)", pProjectileClass ? pProjectileClass->GetName() : "NULL");
		}
	}
	else
	{
		GameWarning("CWeaponSystem::ReturnToPool(): m_pools is empty! (Class: %s)", pProjectileClass ? pProjectileClass->GetName() : "NULL");
	}

	return bSuccess;
}

//------------------------------------------------------------------------
void CWeaponSystem::RemoveFromPool(CProjectile *pProjectile)
{
	TAmmoPoolMap::iterator it=m_pools.find(pProjectile->GetEntity()->GetClass());
	if (it==m_pools.end())
		return;

	if (stl::find_and_erase(it->second.frees, pProjectile))
		--it->second.size;
}

//------------------------------------------------------------------------
void CWeaponSystem::DumpPoolSizes()
{
	CryLog("Ammo Pool Statistics:");
	for (TAmmoPoolMap::iterator it=m_pools.begin(); it!=m_pools.end(); ++it)
	{
		int size=it->second.size;
		CryLog("%s: %d", it->first->GetName(), size);
	}
}

void CWeaponSystem::FreePools()
{
	while (!m_pools.empty())
	{
		FreePool(m_pools.begin()->first);
	}

	stl::free_container(m_pools);

	stl::free_container(m_queryResults);
}

void CWeaponSystem::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "WeaponSystem");
	s->AddObject(this,sizeof(*this));

	m_tracerManager.GetMemoryStatistics(s);
//	s->AddObject(m_fmregistry);
//	s->AddObject(m_zmregistry);
	//s->AddObject(m_projectileregistry);
	s->AddContainer(m_folders);
	s->AddContainer(m_queryResults);

	{
		SIZER_SUBCOMPONENT_NAME(s, "AmmoParams");
		s->AddContainer(m_ammoparams);		
	}
	
	{
		SIZER_SUBCOMPONENT_NAME(s, "Projectiles");
		s->AddContainer(m_projectiles);				
	}

	{
		SIZER_SUBCOMPONENT_NAME(s, "Pools");
		s->AddContainer(m_pools);					
	}
}

//-----------------------------------------------------------------------
void CWeaponSystem::OnResumeAfterHostMigration()
{
	if (gEnv->bServer)
	{
		// Need to find all the projectiles that would have exploded had we been a server
		TProjectileMap::const_iterator end = m_projectiles.end();
		TProjectileMap::iterator it = m_projectiles.begin();
		while (it != end)
		{
			CProjectile *pProjectile = it->second;
			// Move the iterator along before dealing with the projectile since it might get removed
			// from the map and this would invalidate the iterator if we were still pointing at it
			++ it;
			if (pProjectile && pProjectile->ShouldHaveExploded())
			{
				pProjectile->Explode(true);
			}
		}
	}
}

void CWeaponSystem::SAmmoPoolDesc::GetMemoryUsage( ICrySizer *pSizer ) const
{
	pSizer->AddContainer(frees);	
}

//===================================================================================
// Warning this can be called directly from the physics thread! Must be thread safe!
//===================================================================================
void CWeaponSystem::OnProjectilePhysicsPostStep(CProjectile* pProjectile, EventPhysPostStep* pEvent, int bPhysicsThread)
{
	ReadLock lock(m_listenersLock);

	for (std::vector<IProjectileListener *>::const_iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		(*it)->OnProjectilePhysicsPostStep(pProjectile, pEvent, bPhysicsThread);
	}
}

void CWeaponSystem::OnLaunch(CProjectile* pProjectile, const Vec3& pos, const Vec3& velocity)
{
	for (std::vector<IProjectileListener *>::const_iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		(*it)->OnLaunch(pProjectile, pos, velocity);
	}
}

//------------------------------------------------------------------------
void CWeaponSystem::AddListener(IProjectileListener* pListener)
{
	WriteLock lock(m_listenersLock);

	std::vector<IProjectileListener*>::iterator it = std::find(m_listeners.begin(), m_listeners.end(), pListener);

	if (it == m_listeners.end())
	{
		m_listeners.reserve(4);
		m_listeners.push_back(pListener);
	}
}

//------------------------------------------------------------------------
void CWeaponSystem::RemoveListener(IProjectileListener* pListener)
{
	WriteLock lock(m_listenersLock);

	std::vector<IProjectileListener*>::iterator it = std::find(m_listeners.begin(), m_listeners.end(), pListener);

	if (it != m_listeners.end())
	{
		m_listeners.erase(it);

		if (m_listeners.empty())
		{
			stl::free_container(m_listeners);
		}
	}
}

//-----------------------------------------------------------------------
void CDelayedDetonationRMIQueue::AddToQueue(CPlayer* pPlayer, EntityId projectile)
{
	if(m_updateTimer <= 0.f && m_numSentThisFrame < g_pGameCVars->dd_maxRMIsPerFrame)
	{
		m_numSentThisFrame++;

		SendRMI(pPlayer, projectile);
	}
	else
	{
		m_dataQueue.push(SRMIData(pPlayer->GetEntityId(), projectile));

		if(m_updateTimer <= 0.f)
		{
			m_updateTimer = g_pGameCVars->dd_waitPeriodBetweenRMIBatches;
		}
	}
}

//-----------------------------------------------------------------------
void CDelayedDetonationRMIQueue::Update(float frameTime)
{
	if(m_updateTimer - frameTime <= 0.f)
	{
		if(!m_dataQueue.empty())
		{
			m_updateTimer += g_pGameCVars->dd_waitPeriodBetweenRMIBatches;

			while(!m_dataQueue.empty() && m_numSentThisFrame < g_pGameCVars->dd_maxRMIsPerFrame)
			{
				SRMIData front = m_dataQueue.front();
				CPlayer* pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(front.playerId));

				if(pPlayer)
				{
					SendRMI(pPlayer, front.projectileId);
					m_numSentThisFrame++;
				}

				m_dataQueue.pop();
			}
		}
	}
	else
	{
		m_numSentThisFrame = 0;
	}

	m_updateTimer = max(0.f, m_updateTimer - frameTime);
}

//-----------------------------------------------------------------------
void CDelayedDetonationRMIQueue::SendRMI(CPlayer* pPlayer, EntityId projectile)
{
	pPlayer->GetGameObject()->InvokeRMI(CPlayer::ClDelayedDetonation(), CPlayer::EntityParams(projectile), eRMI_ToClientChannel, pPlayer->GetChannelId());
}

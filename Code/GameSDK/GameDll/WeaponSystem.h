// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Weapon System

-------------------------------------------------------------------------
History:
- 18:10:2005   17:41 : Created by Márcio Martins

*************************************************************************/
#ifndef __WEAPONSYSTEM_H__
#define __WEAPONSYSTEM_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IItemSystem.h>
#include <ILevelSystem.h>
#include <IWeapon.h>
#include <CryGame/IGameTokens.h>
#include "Item.h"
#include "TracerManager.h"
#include <CryCore/Containers/VectorMap.h>
#include "AmmoParams.h"
#include "GameParameters.h"
#include "ItemPackages.h"
#include "WeaponAlias.h"
#include "FireModePluginParams.h"
#include "GameTypeInfo.h"

class CGame;
class CProjectile;
class CFireMode;
struct ISystem;
class CPlayer;
class CMelee;
class IFireModePlugin;
class CIronSight;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SProjectileQuery
{
	AABB        box;
	const char* ammoName;
	IEntity     **pResults;
	int         nCount;
	SProjectileQuery()
	{
		pResults = 0;
		nCount = 0;
		ammoName = 0;
	}
};

struct IProjectileListener
{
	// NB: Must be physics-thread safe IF its called from the physics OnPostStep
	virtual void OnProjectilePhysicsPostStep(CProjectile* pProjectile, EventPhysPostStep* pEvent, int bPhysicsThread) {}

	// Called from Main thread
	virtual void OnLaunch(CProjectile* pProjectile, const Vec3& pos, const Vec3& velocity) {}

protected:
	// Should never be called
	virtual ~IProjectileListener() {}
};


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CDelayedDetonationRMIQueue
{
public:
	CDelayedDetonationRMIQueue() 
		: m_updateTimer(0.f)
		, m_numSentThisFrame(0) {};

	~CDelayedDetonationRMIQueue() { while(!m_dataQueue.empty()) { m_dataQueue.pop(); } };

	void AddToQueue(CPlayer* pPlayer, EntityId projectile);
	void Update(float frameTime);
	void SendRMI(CPlayer* pPlayer, EntityId projectile);

protected:

	struct SRMIData
	{
		SRMIData(EntityId player, EntityId proj) 
			: playerId(player)
			, projectileId(proj) {};

		EntityId playerId;
		EntityId projectileId;
	};

	typedef std::queue<SRMIData> TRMIDataQueue;

	int		m_numSentThisFrame;
	float m_updateTimer;
	TRMIDataQueue m_dataQueue;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CWeaponSystem : public ILevelSystemListener
{
	struct SAmmoTypeDesc
	{
		SAmmoTypeDesc(): params(0) {};
		void GetMemoryUsage( ICrySizer *pSizer ) const 
		{
			pSizer->AddObject(params);
		}
		const SAmmoParams *params;
	};

	struct SAmmoPoolDesc
	{
		SAmmoPoolDesc(): size(0) {};
		void GetMemoryUsage( ICrySizer *pSizer ) const; 		
		std::deque<CProjectile *>	frees;
		uint16										size;
	};

	typedef std::map<const CGameTypeInfo*, IFireModePlugin*(*)()>				TFireModePluginCreationRegistry;
	typedef std::map<string, CFireMode*(*)()>														TFireModeCreationRegistry;
	typedef std::map<string, IZoomMode*(*)()>														TZoomModeCreationRegistry;
	typedef std::map<const CGameTypeInfo*, void(*)(IZoomMode*)>					TZoomModeDestructionRegistry;
	typedef std::map<const CGameTypeInfo*, void(*)(CFireMode*)>					TFireModeDestructionRegistry;
	typedef std::map<const CGameTypeInfo*, void (*)(IFireModePlugin*)>	TFireModePluginDestructionRegistry;
	typedef std::map<string, void(*)()>																	TWeaponComponentPoolFreeFunctions;
	typedef std::map<string, IGameObjectExtensionCreatorBase *>					TProjectileRegistry;
	typedef std::map<EntityId, CProjectile *>														TProjectileMap;
	typedef VectorMap<const IEntityClass*, SAmmoTypeDesc>								TAmmoTypeParams;
	typedef std::vector<string>																					TFolderList;
	typedef std::vector<IEntity*>																				TIEntityVector;

	typedef VectorMap<IEntityClass *, SAmmoPoolDesc>							TAmmoPoolMap;	

public:
	CWeaponSystem(CGame *pGame, ISystem *pSystem);
	virtual ~CWeaponSystem();

	struct SLinkedProjectileInfo
	{
		SLinkedProjectileInfo()
		{
			weaponId = 0;
			shotId = 0;
		}

		SLinkedProjectileInfo(EntityId _weapon, uint8 _shot)
		{
			weaponId = _weapon;
			shotId = _shot;
		}

		EntityId weaponId;
		uint8 shotId;
	};

	typedef VectorMap<EntityId, SLinkedProjectileInfo>	TLinkedProjectileMap;

	void Update(float frameTime);
	void Release();

	void GetMemoryStatistics( ICrySizer * );

	void Reload();
	void LoadItemParams(IItemSystem* pItemSystem);

	// ILevelSystemListener
	virtual void OnLevelNotFound(const char* levelName) {};
	virtual void OnLoadingLevelEntitiesStart(ILevelInfo* pLevel) {}
	virtual void OnLoadingStart(ILevelInfo* pLevel);
	virtual void OnLoadingComplete(ILevelInfo* pLevel);
	virtual void OnLoadingError(ILevelInfo* pLevel, const char* error) {};
	virtual void OnLoadingProgress(ILevelInfo* pLevel, int progressAmount) {};
	virtual void OnUnloadComplete(ILevelInfo* pLevel);
	//~ILevelSystemListener

	CFireMode *CreateFireMode(const char *name);
	void DestroyFireMode(CFireMode* pObject);
	
	IZoomMode *CreateZoomMode(const char *name);
	void DestroyZoomMode(CIronSight* pObject);

	CMelee* CreateMeleeMode();
	void DestroyMeleeMode(CMelee* pObject);

	IFireModePlugin* CreateFireModePlugin(const CGameTypeInfo* pluginType);
	void DestroyFireModePlugin(IFireModePlugin* pObject);

	IGameSharedParameters *CreateZoomModeData(const char *name);

	IGameSharedParameters *CreateFireModeData(const char *name);

	CProjectile *SpawnAmmo(IEntityClass* pAmmoType, bool isRemote=false);
	const SAmmoParams* GetAmmoParams( const IEntityClass* pAmmoType );
	bool IsServerSpawn(IEntityClass* pAmmoType) const;
	void RegisterProjectile(const char *name, IGameObjectExtensionCreatorBase *pCreator);
	const SAmmoParams* GetAmmoParams( const IEntityClass* pAmmoType ) const;

	void AddProjectile(IEntity *pEntity, CProjectile *pProjectile);
	void RemoveProjectile(CProjectile *pProjectile);

	void AddLinkedProjectile(EntityId projId, EntityId weaponId, uint8 shotId);
	void RemoveLinkedProjectile(EntityId projId);

	const TLinkedProjectileMap& GetLinkedProjectiles() { return m_linkedProjectiles; }

	CProjectile  *GetProjectile(EntityId entityId);
	const IEntityClass* GetProjectileClass( const EntityId entityId ) const;
	int	QueryProjectiles(SProjectileQuery& q);

	CItemPackages &GetItemPackages() { return m_itemPackages; };
	CTracerManager &GetTracerManager() { return m_tracerManager; };
	const CWeaponAlias &GetWeaponAlias() { return m_weaponAlias; };
	CDelayedDetonationRMIQueue& GetProjectileDelayedDetonationRMIQueue() { return m_detonationRMIQueue; }

	void Scan(const char *folderName);
	bool ScanXML(XmlNodeRef &root, const char *xmlFile);

  static void DebugGun(IConsoleCmdArgs *args = 0);
	static void RefGun(IConsoleCmdArgs *args = 0);

	CProjectile *UseFromPool(IEntityClass *pClass, const SAmmoParams *pAmmoParams);
	bool ReturnToPool(CProjectile *pProjectile);
	void RemoveFromPool(CProjectile *pProjectile);
	void DumpPoolSizes();
	void FreePools();

	void OnResumeAfterHostMigration();

	void AddListener(IProjectileListener* pListener);
	void RemoveListener(IProjectileListener* pListener);

	// Used by projectiles to tell the weapon system where they are
	// Must be physics-thread safe IF its called from the physics OnPostStep
	void OnProjectilePhysicsPostStep(CProjectile* pProjectile, EventPhysPostStep* pEvent, int bPhysicsThread);
	void OnLaunch(CProjectile* pProjectile, const Vec3& pos, const Vec3& velocity);

private: 

	template<typename PlugInType>
	void RegisterFireModePlugin();

	template<typename FireModeType>
	void RegisterFireMode(const char* componentName);

	template<typename ZoomType>
	void RegisterZoomMode(const char* componentName);

	void CreatePool(IEntityClass *pClass);
	void FreePool(IEntityClass *pClass);
	uint16 GetPoolSize(IEntityClass *pClass);
	
	CProjectile *DoSpawnAmmo(IEntityClass* pAmmoType, bool isRemote, const SAmmoParams *pAmmoParams);

	CGame								*m_pGame;
	ISystem							*m_pSystem;
	IItemSystem					*m_pItemSystem;

	CItemPackages			m_itemPackages;
	CTracerManager			m_tracerManager;
	CWeaponAlias				m_weaponAlias;

	TFireModeCreationRegistry						m_fmCreationRegistry;
	TFireModeDestructionRegistry				m_fmDestructionRegistry;
	TZoomModeCreationRegistry						m_zmCreationRegistry;
	TZoomModeDestructionRegistry				m_zmDestructionRegistry;
	TFireModePluginCreationRegistry			m_pluginCreationRegistry;
	TFireModePluginDestructionRegistry	m_pluginDestructionRegistry;
	TWeaponComponentPoolFreeFunctions		m_freePoolHandlers;
	
	std::vector<IProjectileListener*> m_listeners;
	volatile int m_listenersLock;
	
	TProjectileRegistry		m_projectileregistry;
	TAmmoTypeParams			m_ammoparams;
	TProjectileMap				m_projectiles;
	TLinkedProjectileMap	m_linkedProjectiles;

	TAmmoPoolMap				m_pools;

	TFolderList					m_folders;
	bool								m_reloading;
	bool								m_recursing;

	ICVar								*m_pPrecache;
	TIEntityVector			m_queryResults;//for caching queries results
	CDelayedDetonationRMIQueue m_detonationRMIQueue;

};



#endif //__WEAPONSYSTEM_H__

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Script Binding for Game

-------------------------------------------------------------------------
History:
- 14:08:2006   11:30 : Created by AlexL
*************************************************************************/
#ifndef __SCRIPTBIND_GAME_H__
#define __SCRIPTBIND_GAME_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>

struct IGameFramework;
struct ISystem;

class CScriptBind_Game :
	public CScriptableBase
{
	enum EGameCacheResourceType
	{
		eGCRT_Texture = 0,
		eGCRT_TextureDeferredCubemap = 1,
		eGCRT_StaticObject = 2,
		eGCRT_Material = 3,
	};

public:
	CScriptBind_Game(ISystem *pSystem, IGameFramework *pGameFramework);
	virtual ~CScriptBind_Game();

	virtual void GetMemoryUsage(ICrySizer *pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

protected:
	int ShowMainMenu(IFunctionHandler *pH);
	int PauseGame(IFunctionHandler *pH, bool pause);
	int IsMountedWeaponUsableWithTarget(IFunctionHandler *pH);

	int IsPlayer(IFunctionHandler *pH, ScriptHandle entityId);
	int RegisterVTOL(IFunctionHandler *pH, ScriptHandle entityId);
	
	int AddTacticalEntity(IFunctionHandler *pH, ScriptHandle entityId, int type);
	int RemoveTacticalEntity(IFunctionHandler *pH, ScriptHandle entityId, int type);

	int RegisterWithAutoAimManager(IFunctionHandler *pH, ScriptHandle entityId, float innerRadiusFactor, float outerRadiusFactor, float snapRadiusFactor);
	int UnregisterFromAutoAimManager(IFunctionHandler *pH, ScriptHandle entityId);

	int OnAmmoCrateSpawned(IFunctionHandler *pH, bool providesFragGrenades);

	int CacheResource(IFunctionHandler *pH, const char* whoIsRequesting, const char* resourceName, int resourceType, int resourceFlags);
	int CacheActorClassResources(IFunctionHandler *pH, const char* actorEntityClassName);
	int CacheEntityArchetype(IFunctionHandler *pH, const char* archetypeName);
	int CacheBodyDamageProfile(IFunctionHandler *pH, const char* bodyDamageFileName, const char* bodyDamagePartsFileName);

	//Checkpoint System
	int SaveCheckpoint(IFunctionHandler *pH, ScriptHandle checkpointId, const char *fileName);
	int	LoadCheckpoint(IFunctionHandler *pH, const char *fileName);
	int QuickLoad(IFunctionHandler *pH);

	int QueueDeferredKill(IFunctionHandler* pH, ScriptHandle entityId);

#ifndef _RELEASE
	int DebugDrawCylinder(IFunctionHandler *pH, float x, float y, float z, float radius, float height, int r, int g, int b, int a);
	int DebugDrawCone( IFunctionHandler *pH, float x, float y, float z, float radius, float height, int r, int g, int b, int a );
	int DebugDrawAABB(IFunctionHandler *pH, float x, float y, float z, float x2, float y2, float z2, int r, int g, int b, int a);

	int DebugDrawPersistanceDirection(IFunctionHandler *pH, float startX, float startY, float startZ, float dirX, float dirY, float dirZ, int r, int g, int b, float duration);
#endif

	//Environmental weapons
	int OnEnvironmentalWeaponHealthChanged( IFunctionHandler *pH, ScriptHandle entityId );

	int ResetEntity( IFunctionHandler *pH, ScriptHandle entityId );
	int SetDangerousRigidBodyDangerStatus(IFunctionHandler *pH, ScriptHandle entityId, bool isDangerous, ScriptHandle triggerPlayerId);

	int SendEventToGameObject( IFunctionHandler* pH, ScriptHandle entityId, char* event );

	int LoadPrefabLibrary( IFunctionHandler* pH,const char *filename);
	int SpawnPrefab( IFunctionHandler* pH,ScriptHandle entityId, const char *libname, const char *prefabname,uint32 seed,uint32 nMaxSpawn);
	int MovePrefab( IFunctionHandler* pH,ScriptHandle entityId);
	int DeletePrefab( IFunctionHandler* pH,ScriptHandle entityId);
	int HidePrefab( IFunctionHandler* pH,ScriptHandle entityId,bool bHide);

	// Resource caching:
	int CacheEquipmentPack(IFunctionHandler* pH, const char* equipmentPackName);	

private:
	void RegisterGlobals();
	void RegisterMethods();

	ISystem						*m_pSystem;
	IGameFramework		*m_pGameFW;
};

#endif //__SCRIPTBIND_GAME_H__

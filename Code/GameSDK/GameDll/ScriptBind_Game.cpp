// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 14:08:2006   11:29 : Created by AlexL

*************************************************************************/
#include "StdAfx.h"
#include "ScriptBind_Game.h"
#include "Game.h"
#include "Audio/GameAudio.h"
#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IAISystem.h>
#include "IVehicleSystem.h"
#include "IItemSystem.h"
#include "TacticalManager.h"
#include "ICheckPointSystem.h"
#include <CryGame/IGameFramework.h>
#include "AI/Agent.h"
#include "GameRules.h"
#include "GameCache.h"
#include "VTOLVehicleManager/VTOLVehicleManager.h"
#include "EnvironmentalWeapon.h"
#include "Environment/DangerousRigidBody.h"
#include "GamePhysicsSettings.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "GamePhysicsSettings.h"

#include <GameObjects/GameObject.h>

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning(disable: 4244)
#endif

//------------------------------------------------------------------------
CScriptBind_Game::CScriptBind_Game(ISystem *pSystem, IGameFramework *pGameFramework)
: m_pSystem(pSystem),
	m_pGameFW(pGameFramework)
{
	Init(pSystem->GetIScriptSystem(), m_pSystem);
	SetGlobalName("Game");

	RegisterMethods();
	RegisterGlobals();
}

//------------------------------------------------------------------------
CScriptBind_Game::~CScriptBind_Game()
{
}

//------------------------------------------------------------------------
void CScriptBind_Game::RegisterGlobals()
{
	m_pSS->SetGlobalValue("eTacticalEntity_Story", CTacticalManager::eTacticalEntity_Story);
	m_pSS->SetGlobalValue("eTacticalEntity_Item", CTacticalManager::eTacticalEntity_Item);
	m_pSS->SetGlobalValue("eTacticalEntity_Unit", CTacticalManager::eTacticalEntity_Unit);
	m_pSS->SetGlobalValue("eTacticalEntity_Ammo", CTacticalManager::eTacticalEntity_Ammo);
	m_pSS->SetGlobalValue("eTacticalEntity_Prompt", CTacticalManager::eTacticalEntity_Prompt);
	m_pSS->SetGlobalValue("eTacticalEntity_Vehicle", CTacticalManager::eTacticalEntity_Vehicle);
	m_pSS->SetGlobalValue("eTacticalEntity_Hazard", CTacticalManager::eTacticalEntity_Hazard);
	m_pSS->SetGlobalValue("eTacticalEntity_Explosive", CTacticalManager::eTacticalEntity_Explosive);
	m_pSS->SetGlobalValue("eTacticalEntity_MapIcon", CTacticalManager::eTacticalEntity_MapIcon);

	m_pSS->SetGlobalValue("eGameCacheResourceType_Texture", CScriptBind_Game::eGCRT_Texture);
	m_pSS->SetGlobalValue("eGameCacheResourceType_TextureDeferredCubemap", CScriptBind_Game::eGCRT_TextureDeferredCubemap);
	m_pSS->SetGlobalValue("eGameCacheResourceType_StaticObject", CScriptBind_Game::eGCRT_StaticObject);
	m_pSS->SetGlobalValue("eGameCacheResourceType_Material", CScriptBind_Game::eGCRT_Material);

	m_pSS->SetGlobalValue("eGameCacheResourceFlag_TextureNoStream", FT_DONT_STREAM);
	m_pSS->SetGlobalValue("eGameCacheResourceFlag_TextureReplicateAllSides", FT_REPLICATE_TO_ALL_SIDES);


	SCRIPT_REG_GLOBAL(STANCE_PRONE);
	SCRIPT_REG_GLOBAL(STANCE_CROUCH);
	SCRIPT_REG_GLOBAL(STANCE_STAND);
	SCRIPT_REG_GLOBAL(STANCE_RELAXED);
	SCRIPT_REG_GLOBAL(STANCE_LOW_COVER);
	SCRIPT_REG_GLOBAL(STANCE_HIGH_COVER);
	SCRIPT_REG_GLOBAL(STANCE_ALERTED);
	SCRIPT_REG_GLOBAL(STANCE_STEALTH);
	SCRIPT_REG_GLOBAL(STANCE_SWIM);

	g_pGame->GetGamePhysicsSettings()->ExportToLua();
}

//------------------------------------------------------------------------
void CScriptBind_Game::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_Game::

	SCRIPT_REG_TEMPLFUNC(ShowMainMenu, "");
	SCRIPT_REG_TEMPLFUNC(PauseGame, "pause");

	SCRIPT_REG_FUNC(IsMountedWeaponUsableWithTarget);

	SCRIPT_REG_TEMPLFUNC(IsPlayer, "entityId");
	SCRIPT_REG_TEMPLFUNC(RegisterVTOL, "entityId");
	
	SCRIPT_REG_TEMPLFUNC(AddTacticalEntity, "entityId, type");
	SCRIPT_REG_TEMPLFUNC(RemoveTacticalEntity, "entityId, type");

	SCRIPT_REG_TEMPLFUNC(RegisterWithAutoAimManager, "entityId, innerRadiusFactor, outerRadiusFactor, snapRadiusFactor");
	SCRIPT_REG_TEMPLFUNC(UnregisterFromAutoAimManager, "entityId");

	SCRIPT_REG_TEMPLFUNC(OnAmmoCrateSpawned, "providesFragGrenades");
	SCRIPT_REG_TEMPLFUNC(CacheResource, "whoIsRequesting, resourceName, resourceType, resourceFlags");
	SCRIPT_REG_TEMPLFUNC(CacheActorClassResources, "actorEntityClassName");
	SCRIPT_REG_TEMPLFUNC(CacheEntityArchetype, "archetypeName");
	SCRIPT_REG_TEMPLFUNC(CacheBodyDamageProfile, "bodyDamageFileName, bodyDamagePartsFileName");

	SCRIPT_REG_TEMPLFUNC(SaveCheckpoint, "checkpointId, fileName");
	SCRIPT_REG_TEMPLFUNC(LoadCheckpoint, "checkpointId");
	SCRIPT_REG_TEMPLFUNC(QuickLoad, "");

	SCRIPT_REG_TEMPLFUNC(QueueDeferredKill, "entityId");

	SCRIPT_REG_TEMPLFUNC(OnEnvironmentalWeaponHealthChanged, "entityId");
	SCRIPT_REG_TEMPLFUNC(ResetEntity, "entityId");
	SCRIPT_REG_TEMPLFUNC(SetDangerousRigidBodyDangerStatus, "entityId, isDangerous, triggerPlayerId");
	SCRIPT_REG_TEMPLFUNC(SendEventToGameObject, "entityId, event" );

	SCRIPT_REG_TEMPLFUNC(CacheEquipmentPack, "equipmentPackName");

#ifndef _RELEASE
	SCRIPT_REG_TEMPLFUNC(DebugDrawCylinder, "x, y, z, radius, height, r, g, b, a");
	SCRIPT_REG_TEMPLFUNC(DebugDrawCone, "x, y, z, radius, height, r, g, b, a");
	SCRIPT_REG_TEMPLFUNC(DebugDrawAABB, "x, y, z, x2, y2, z2, r, g, b, a");

	SCRIPT_REG_TEMPLFUNC(DebugDrawPersistanceDirection, "startX, startY, startZ, dirX, dirY, dirZ, r, g, b, duration");
#endif

	SCRIPT_REG_TEMPLFUNC(LoadPrefabLibrary, "filename");
	SCRIPT_REG_TEMPLFUNC(SpawnPrefab, "entityId, libname, prefabname, seed, nMaxSpawn");
	SCRIPT_REG_TEMPLFUNC(MovePrefab, "entityId");
	SCRIPT_REG_TEMPLFUNC(DeletePrefab, "entityId");
	SCRIPT_REG_TEMPLFUNC(HidePrefab, "entityId, bHide");

#undef SCRIPT_REG_CLASSNAME
}

//------------------------------------------------------------------------
int CScriptBind_Game::AddTacticalEntity(IFunctionHandler *pH, ScriptHandle id, int type)
{
	g_pGame->GetTacticalManager()->AddEntity((EntityId)id.n, (CTacticalManager::ETacticalEntityType)type);
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Game::RemoveTacticalEntity(IFunctionHandler *pH, ScriptHandle id, int type)
{
	g_pGame->GetTacticalManager()->RemoveEntity((EntityId)id.n, (CTacticalManager::ETacticalEntityType)type);
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Game::RegisterWithAutoAimManager(IFunctionHandler *pH, ScriptHandle entityId, float innerRadiusFactor, float outerRadiusFactor, float snapRadiusFactor)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity((EntityId)entityId.n);
	if (pEntity)
	{
		AABB entityBbox;
		pEntity->GetWorldBounds(entityBbox);

		const float entityRadius = (entityBbox.IsEmpty() == false) ? entityBbox.GetRadius() : 1.0f;
		SAutoaimTargetRegisterParams registerParams;
		registerParams.fallbackOffset = 0.0f;
		registerParams.innerRadius = entityRadius * innerRadiusFactor;
		registerParams.outerRadius = entityRadius * outerRadiusFactor;
		registerParams.snapRadius = entityRadius * snapRadiusFactor;
		registerParams.snapRadiusTagged = entityRadius;
		registerParams.primaryBoneId = -1;
		registerParams.physicsBoneId = -1;
		registerParams.secondaryBoneId = -1;
		
		g_pGame->GetAutoAimManager().RegisterAutoaimTargetObject((EntityId)entityId.n, registerParams);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Game::UnregisterFromAutoAimManager(IFunctionHandler *pH, ScriptHandle entityId)
{
	g_pGame->GetAutoAimManager().UnregisterAutoaimTarget((EntityId)entityId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Game::OnAmmoCrateSpawned(IFunctionHandler *pH, bool providesFragGrenades)
{
	CGameRules* pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		if (providesFragGrenades)
		{
			pGameRules->PreCacheItemResources("FragGrenades");
		}
	}

	return pH->EndFunction();
}

#define LOG_CACHE_RESOURCES_FROM_LUA	0

static void LogLuaCacheResource(const char* whoIsRequesting, const char* type, const char* resourceName, const int flags)
{
#if LOG_CACHE_RESOURCES_FROM_LUA
	if (resourceName && resourceName[0])
	{
		CryLog("[GAME CACHE LUA] by '%s' : %s - %s Flags(%d)", whoIsRequesting, type, resourceName, flags);
	}
#endif
}

//------------------------------------------------------------------------
int CScriptBind_Game::CacheResource(IFunctionHandler *pH, const char* whoIsRequesting, const char* resourceName, int resourceType, int resourceFlags)
{
	//Only cache in pure game mode
	if (gEnv->IsEditor())
		return pH->EndFunction();

	CGameCache& gameCache = g_pGame->GetGameCache();

	switch(resourceType)
	{
	case eGCRT_Texture:
		{
			gameCache.CacheTexture(resourceName, resourceFlags);	

			LogLuaCacheResource(whoIsRequesting, "Texture", resourceName, resourceFlags);
		}	
		break;

	case eGCRT_TextureDeferredCubemap:
		{
			//Some magic strings ops Copy&Pasted from ScriptBind_Entity::ParseLightProperties
			const char* specularCubemap = resourceName;

			if (specularCubemap && strlen(specularCubemap) > 0)
			{
				CryFixedStringT<256> sSpecularName(specularCubemap);
				int strIndex = sSpecularName.find("_diff");
				if(strIndex >= 0)
				{
					sSpecularName = sSpecularName.substr(0, strIndex) + sSpecularName.substr(strIndex + 5, sSpecularName.length());
					specularCubemap = sSpecularName.c_str();
				}

				CryFixedStringT<256> diffuseCubemap;
				diffuseCubemap.Format("%s%s%s.%s",	PathUtil::AddSlash(PathUtil::GetPathWithoutFilename(specularCubemap)).c_str(), 
													PathUtil::GetFileName(specularCubemap).c_str(), "_diff", PathUtil::GetExt(specularCubemap));

				// '\\' in filename causing texture duplication
				string specularCubemapUnix = PathUtil::ToUnixPath(specularCubemap);
				string diffuseCubemapUnix = PathUtil::ToUnixPath(diffuseCubemap);

				gameCache.CacheTexture(specularCubemapUnix.c_str(), resourceFlags);
				gameCache.CacheTexture(diffuseCubemapUnix.c_str(), resourceFlags);

				LogLuaCacheResource(whoIsRequesting, "CubeMap Specular", specularCubemapUnix.c_str(), resourceFlags);
				LogLuaCacheResource(whoIsRequesting, "CubeMap Diffuse", diffuseCubemapUnix.c_str(), resourceFlags);
			}
		}
		break;

	case eGCRT_StaticObject:
		{
			gameCache.CacheGeometry(resourceName);

			LogLuaCacheResource(whoIsRequesting, "Static Object", resourceName, resourceFlags);
		}
		break;

	case eGCRT_Material:
		{
			gameCache.CacheMaterial(resourceName);

			LogLuaCacheResource(whoIsRequesting, "Material", resourceName, resourceFlags);
		}
		break;
	}

	return pH->EndFunction();
}


// ===========================================================================
//	Cache the resources of an actor class (pre-load them).
//
//	In:		The LUA function handler.
//	In:		The name of the actor entity class that we should pre-load 
//			(case-sensitive) (NULL is invalid!)
//
int CScriptBind_Game::CacheActorClassResources(IFunctionHandler *pH, const char* actorEntityClassName)
{
	if (actorEntityClassName == NULL)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, 
			"CacheActorClassResources(): Invalid entity actor class name!");
	}
	else
	{
		IEntityClass* entityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(actorEntityClassName);
		if (entityClass == NULL)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, 
				"CacheActorClassResources(): Unable to cache entity actor class resource '%s'!", actorEntityClassName);
		}	
		else
		{
			g_pGame->GetGameCache().CacheActorClass(entityClass, entityClass->GetScriptTable());
		}
	}		

	return pH->EndFunction();
}


// ===========================================================================
//	Cache an entity archetype and everything that it is referencing.
//
//	In:		The LUA function handler.
//	In:		The entity archetype name. The archetype will be loaded/cached and 
//			have its CacheResources() Lua function called.
//
//	Returns:	Default Lua return value.
//
int CScriptBind_Game::CacheEntityArchetype(IFunctionHandler *pH, const char* archetypeName)
{
	g_pGame->GetGameCache().CacheEntityArchetype(archetypeName);
	return pH->EndFunction();
}


// ===========================================================================
//	Cache a body damage profile.
//
//	In:		The LUA function handler.
//	In:		The body damage file name (NULL is invalid!)
//	In:		The body damage parts file name (NULL is invalid!)
//
//	Returns:	Default Lua return value.
//
int CScriptBind_Game::CacheBodyDamageProfile(
	IFunctionHandler *pH, const char* bodyDamageFileName, const char* bodyDamagePartsFileName)
{
	g_pGame->GetGameCache().CacheBodyDamageProfile(bodyDamageFileName, bodyDamagePartsFileName);
	return pH->EndFunction();
}


//------------------------------------------------------------------------
int CScriptBind_Game::ShowMainMenu(IFunctionHandler *pH)
{
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Game::PauseGame( IFunctionHandler *pH, bool pause )
{
	bool forced = false;

	if (pH->GetParamCount() > 1)
	{
		pH->GetParam(2, forced);
	}
	m_pGameFW->PauseGame(pause, forced);

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
//int CScriptBind_Game::QueryBattleStatus(IFunctionHandler *pH)
//{		
//	float fStatus = SAFE_GAMEAUDIO_BATTLESTATUS_FUNC_RET(QueryBattleStatus());
	
//	return pH->EndFunction(fStatus);
//}

//////////////////////////////////////////////////////////////////////////

int CScriptBind_Game::IsPlayer(IFunctionHandler *pH, ScriptHandle entityId)
{
	EntityId eId = (EntityId)entityId.n;
	if(eId == LOCAL_PLAYER_ENTITY_ID)
		return pH->EndFunction(true);

	IActor *pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(eId);
	return pH->EndFunction(pActor && pActor->IsPlayer());
}

int CScriptBind_Game::RegisterVTOL(IFunctionHandler *pH, ScriptHandle entityId)
{
	EntityId vtolId = (EntityId)entityId.n;
	CVTOLVehicleManager *pVTOLVehicleManager = g_pGame->GetGameRules()->GetVTOLVehicleManager();
	CRY_ASSERT(!gEnv->bMultiplayer || pVTOLVehicleManager);
	if (pVTOLVehicleManager)
	{
		pVTOLVehicleManager->RegisterVTOL(vtolId);
	}

	return pH->EndFunction();
}

int CScriptBind_Game::OnEnvironmentalWeaponHealthChanged( IFunctionHandler *pH, ScriptHandle entityId )
{
	EntityId eId = (EntityId)entityId.n;
	IEntity* pEntity =  gEnv->pEntitySystem->GetEntity(eId);

	if (pH->GetParamCount() == 3)
	{
		float fPrevHealth = 0.0f, fCurrentHealth = 0.0f;

		pH->GetParam(2, fPrevHealth);
		pH->GetParam(3, fCurrentHealth);

		if(pEntity)
		{
			CEnvironmentalWeapon *pEnvWeapon = static_cast<CEnvironmentalWeapon*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(eId, "EnvironmentalWeapon"));
			if(pEnvWeapon)
			{
				pEnvWeapon->SvOnHealthChanged(fPrevHealth, fCurrentHealth); 
			}
		}
	}

	
	return pH->EndFunction();
}

int CScriptBind_Game::ResetEntity( IFunctionHandler *pH, ScriptHandle entityId )
{
	if( IEntity* pEntity =  gEnv->pEntitySystem->GetEntity((EntityId)entityId.n) )
	{
		SEntityEvent event(ENTITY_EVENT_RESET);
		pEntity->SendEvent(event);
	}

	return pH->EndFunction();
}

int CScriptBind_Game::SetDangerousRigidBodyDangerStatus( IFunctionHandler *pH, ScriptHandle entityId, bool isDangerous, ScriptHandle triggerPlayerId )
{
	if( CDangerousRigidBody *pDangerousRigidBody = static_cast<CDangerousRigidBody*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension((EntityId)entityId.n, "DangerousRigidBody")) )
	{
		pDangerousRigidBody->SetIsDangerous(isDangerous, (EntityId)triggerPlayerId.n);
	}

	return pH->EndFunction();
}

int CScriptBind_Game::SendEventToGameObject( IFunctionHandler* pH, ScriptHandle entityId, char* event )
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( (EntityId)entityId.n );
	if (pEntity != NULL)
	{
		CGameObject* pGameObject = static_cast<CGameObject*>(pEntity->GetProxy(ENTITY_PROXY_USER));
		if (pGameObject != NULL)
		{
			SGameObjectEvent goEvent( (uint32)eGFE_ScriptEvent, eGOEF_ToExtensions, IGameObjectSystem::InvalidExtensionID, event );
			pGameObject->SendEvent( goEvent );
		}
	}
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////	

#define GET_ENTITY(i) \
	ScriptHandle hdl;\
	pH->GetParam(i,hdl);\
	EntityId nID = (EntityId)hdl.n;\
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(nID);


//////////////////////////////////////////////////////////////////////////
// IsMountedWeaponUsableWithTarget
// A piece of game-code moved from CryAction when scriptbind_AI moved to the AI system
//////////////////////////////////////////////////////////////////////////
int CScriptBind_Game::IsMountedWeaponUsableWithTarget(IFunctionHandler *pH)
{
	int paramCount = pH->GetParamCount();
	if(paramCount<2)
	{
		GameWarning("%s: too few parameters.", __FUNCTION__);
		return pH->EndFunction();
	}

	GET_ENTITY(1);

	if(!pEntity)
	{
		GameWarning("%s: wrong entity id in parameter 1.", __FUNCTION__);
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		GameWarning("%s: Entity '%s' does not have AI.",__FUNCTION__,  pEntity->GetName());
		return pH->EndFunction();
	}

	EntityId itemEntityId;
	ScriptHandle hdl2;

	if(!pH->GetParam(2,hdl2))
	{
		GameWarning("%s: wrong parameter 2 format.", __FUNCTION__);
		return pH->EndFunction();
	}

	itemEntityId = (EntityId)hdl2.n;

	if (!itemEntityId)
	{
		GameWarning("%s: wrong entity id in parameter 2.", __FUNCTION__);
		return pH->EndFunction();
	}
	
	IGameFramework *pGameFramework = gEnv->pGameFramework;
	IItem* pItem = pGameFramework->GetIItemSystem()->GetItem(itemEntityId);
	if (!pItem)
	{
		//gEnv->pAISystem->Warning("<CScriptBind> ", "entity in parameter 2 is not an item/weapon");
		GameWarning("%s: entity in parameter 2 is not an item/weapon.", __FUNCTION__);
		return pH->EndFunction();
	}

	float minDist = 7;
	bool bSkipTargetCheck = false;
	Vec3 targetPos(ZERO);

	if(paramCount > 2)
	{
		for(int i=3;i <= paramCount ; i++)
		{
			if(pH->GetParamType(i) == svtBool)
				pH->GetParam(i,bSkipTargetCheck);
			else if(pH->GetParamType(i) == svtNumber)
				pH->GetParam(i,minDist);
			else if(pH->GetParamType(i) == svtObject)
				pH->GetParam(i,targetPos);
		}
	}

	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if (!pAIActor)
	{
		GameWarning("%s: entity '%s' in parameter 1 is not an AI actor.", __FUNCTION__, pEntity->GetName());
		return pH->EndFunction();
	}


	IEntity* pItemEntity = pItem->GetEntity();
	if(!pItemEntity)
		return pH->EndFunction();


	if(!pItem->GetOwnerId())
	{
		// weapon is not used, check if it is on a vehicle
		IEntity* pParentEntity = pItemEntity->GetParent();
		if(pParentEntity)
		{
			IAIObject* pParentAI = pParentEntity->GetAI();
			if(pParentAI && pParentAI->GetAIType()==AIOBJECT_VEHICLE)
			{
				// (MATT) Feature was cut and code was tricky, hence ignore weapons in vehicles  {2008/02/15:11:08:51}
				return pH->EndFunction();
			}
		}
	}
	else if( pItem->GetOwnerId()!= pEntity->GetId()) // item is used by someone else?
		return pH->EndFunction(false);

	// check target
	if(bSkipTargetCheck)
		return pH->EndFunction(true);

	IAIObject* pTarget = pAIActor->GetAttentionTarget();
	if(targetPos.IsZero())
	{
		if(!pTarget)
			return pH->EndFunction();
		targetPos = pTarget->GetPos();
	}

	Vec3 targetDir(targetPos - pItemEntity->GetWorldPos());
	Vec3 targetDirXY(targetDir.x, targetDir.y, 0);

	float length2D = targetDirXY.GetLength();
	if(length2D < minDist || length2D<=0)
		return pH->EndFunction();

	targetDirXY /= length2D;//normalize

	IWeapon* pWeapon = pItem->GetIWeapon(); 
	bool vehicleGun = pWeapon && pWeapon->GetHostId();

	if (!vehicleGun)
	{
		Vec3 mountedAngleLimits(pItem->GetMountedAngleLimits());

		float yawRange = DEG2RAD(mountedAngleLimits.z);
		if(yawRange > 0 && yawRange < gf_PI)
		{
			float deltaYaw = pItem->GetMountedDir().Dot(targetDirXY);
			if(deltaYaw < cosf(yawRange))
				return pH->EndFunction(false);
		}

		float minPitch = DEG2RAD(mountedAngleLimits.x);
		float maxPitch = DEG2RAD(mountedAngleLimits.y);

		//maxPitch = (maxPitch - minPitch)/2;
		//minPitch = -maxPitch;

		float pitch = atanf(targetDir.z / length2D);

		if ( pitch < minPitch || pitch > maxPitch )
			return pH->EndFunction(false);
	}

	if(pTarget)
	{
		IEntity* pTargetEntity = pTarget->GetEntity();
		if(pTargetEntity)
		{
			// check target distance and where he's going
			IPhysicalEntity *phys = pTargetEntity->GetPhysics();
			if(phys)
			{
				pe_status_dynamics	dyn;
				phys->GetStatus(&dyn);
				Vec3 velocity ( dyn.v);
				velocity.z = 0;

				float speed = velocity.GetLength2D();
				if(speed>0)
				{
					//velocity /= speed;
					if(length2D< minDist * 0.75f && velocity.Dot(targetDirXY)<=0)
						return pH->EndFunction(false);
				}
			}
		}
	}
	return pH->EndFunction(true);

}

//====================================================================
// Checkpoint Trigger 
//====================================================================
int CScriptBind_Game::SaveCheckpoint(IFunctionHandler *pH, ScriptHandle checkpointId, const char *fileName)
{
	assert(pH);
	assert(fileName);
	bool success = m_pGameFW->GetICheckpointSystem()->SaveGame((EntityId)checkpointId.n, fileName);
	return pH->EndFunction(success);
}

//====================================================================
// Checkpoint Loading 
//====================================================================
int CScriptBind_Game::LoadCheckpoint(IFunctionHandler *pH, const char *fileName)
{
	assert(pH);
	assert(fileName);
	bool success = m_pGameFW->GetICheckpointSystem()->LoadGame(fileName);
	return pH->EndFunction(success);
}

int CScriptBind_Game::QuickLoad( IFunctionHandler *pH )
{
	assert(pH);
	g_pGame->GetIGameFramework()->ExecuteCommandNextFrame("loadLastSave");
	return pH->EndFunction();
}

int CScriptBind_Game::QueueDeferredKill(IFunctionHandler* pH, ScriptHandle entityId)
{
	g_pGame->QueueDeferredKill(static_cast<const EntityId>(entityId.n));
	return pH->EndFunction();
}


//====================================================================
//	Cache an equipment pack.
//
//	In:		Lua interfacing handle.
//	In:		The name of the equipment pack (case sensitive) (NULL
//			is invalid!)
//
//	Returns:	The standard Lua return code.
//
int CScriptBind_Game::CacheEquipmentPack(IFunctionHandler* pH, const char* equipmentPackName)
{
	CGameRules *gameRules = g_pGame->GetGameRules();
	if (gameRules != NULL)
	{
		gameRules->PreCacheEquipmentPack(equipmentPackName);
	}
	return pH->EndFunction();
}


//====================================================================
// Debug
//====================================================================
#ifndef _RELEASE
int CScriptBind_Game::DebugDrawCylinder( IFunctionHandler *pH, float x, float y, float z, float radius, float height, int r, int g, int b, int a )
{
	IRenderAuxGeom* pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if (pRenderAuxGeom)
	{
		SAuxGeomRenderFlags oldFlags = pRenderAuxGeom->GetRenderFlags();
		SAuxGeomRenderFlags newFlags = oldFlags;

		newFlags.SetCullMode(e_CullModeNone);
		newFlags.SetAlphaBlendMode(e_AlphaBlended);
		pRenderAuxGeom->SetRenderFlags(newFlags);

		pRenderAuxGeom->DrawCylinder(Vec3(x, y, z), Vec3(0.f, 0.f, 1.f), radius, height, ColorB(r, g, b, a));
		
		pRenderAuxGeom->SetRenderFlags(oldFlags);
	}
	return pH->EndFunction();
}

int CScriptBind_Game::DebugDrawCone( IFunctionHandler *pH, float x, float y, float z, float radius, float height, int r, int g, int b, int a )
{
	IRenderAuxGeom* pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if (pRenderAuxGeom)
	{
		SAuxGeomRenderFlags oldFlags = pRenderAuxGeom->GetRenderFlags();
		SAuxGeomRenderFlags newFlags = oldFlags;

		newFlags.SetCullMode(e_CullModeNone);
		newFlags.SetAlphaBlendMode(e_AlphaBlended);
		pRenderAuxGeom->SetRenderFlags(newFlags);

		pRenderAuxGeom->DrawCone(Vec3(x,y,z), Vec3(0.f, 0.f, 1.f), radius, height, ColorB(r,g,b,a));
		
		pRenderAuxGeom->SetRenderFlags(oldFlags);
	}
	return pH->EndFunction();
}

int CScriptBind_Game::DebugDrawAABB( IFunctionHandler *pH, float x, float y, float z, float x2, float y2, float z2, int r, int g, int b, int a )
{
	IRenderAuxGeom* pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	if (pRenderAuxGeom)
	{
		SAuxGeomRenderFlags oldFlags = pRenderAuxGeom->GetRenderFlags();
		SAuxGeomRenderFlags newFlags = oldFlags;

		newFlags.SetCullMode(e_CullModeNone);
		newFlags.SetAlphaBlendMode(e_AlphaBlended);
		pRenderAuxGeom->SetRenderFlags(newFlags);

		AABB bbox(Vec3(x, y, z), Vec3(x2, y2, z2));
		pRenderAuxGeom->DrawAABB(bbox, true, ColorB(r, g, b, a), eBBD_Faceted);

		pRenderAuxGeom->SetRenderFlags(oldFlags);
	}
	return pH->EndFunction();
}

int CScriptBind_Game::DebugDrawPersistanceDirection(
		IFunctionHandler *pH,
		float startX, float startY, float startZ, 
		float dirX, float dirY, float dirZ,
		int r, int g, int b,
		float duration)
{
	IPersistantDebug* debugRenderer = gEnv->pGameFramework->GetIPersistantDebug();
	assert(debugRenderer != NULL);

	debugRenderer->Begin("CScriptBind_Game::DebugDrawPersistanceDirection", false);
	
	const Vec3 direction(dirX, dirY, dirZ);
	const float length = direction.GetLength();
	const float radius = max(0.1f, length * 0.05f);

	debugRenderer->AddDirection(Vec3(startX, startY, startZ), radius, direction,
		ColorF(
			((float)r) / 256.0f, 
			((float)g) / 256.0f, 
			((float)b) / 256.0f, 
			1.0f),
		duration);

	return pH->EndFunction();
}
#endif

#include "WorldBuilder.h"

using namespace CryGame;

int CScriptBind_Game::LoadPrefabLibrary( IFunctionHandler* pH, const char *filename )
{	
	CPrefabManager &pPrefabManager=g_pGame->GetWorldBuilder()->GetPrefabManager();
	pPrefabManager.LoadPrefabLibrary(filename);
	return pH->EndFunction();
}

int CScriptBind_Game::SpawnPrefab( IFunctionHandler* pH, ScriptHandle entityId,const char *libname,const char *prefabname,uint32 seed,uint32 nMaxSpawn)
{
	EntityId nID=(EntityId)entityId.n;
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( nID );
	if (pEntity != NULL)
	{		
		CPrefabManager &pPrefabManager=g_pGame->GetWorldBuilder()->GetPrefabManager();
		pPrefabManager.SpawnPrefab(libname,prefabname,nID,seed,nMaxSpawn);		
	}
	return pH->EndFunction();
}

int CScriptBind_Game::MovePrefab( IFunctionHandler* pH, ScriptHandle entityId)
{
	EntityId nID=(EntityId)entityId.n;
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( nID );
	if (pEntity != NULL)
	{		
		CPrefabManager &pPrefabManager=g_pGame->GetWorldBuilder()->GetPrefabManager();
		pPrefabManager.MovePrefab(nID);		
	}
	return pH->EndFunction();
}

int CScriptBind_Game::DeletePrefab( IFunctionHandler* pH, ScriptHandle entityId)
{
	EntityId nID=(EntityId)entityId.n;
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( nID );
	if (pEntity != NULL)
	{		
		CPrefabManager &pPrefabManager=g_pGame->GetWorldBuilder()->GetPrefabManager();
		pPrefabManager.DeletePrefab(nID);
	}
	return pH->EndFunction();
}

int CScriptBind_Game::HidePrefab( IFunctionHandler* pH, ScriptHandle entityId, bool bHide)
{
	EntityId nID=(EntityId)entityId.n;
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( nID );
	if (pEntity != NULL)
	{		
		CPrefabManager &pPrefabManager=g_pGame->GetWorldBuilder()->GetPrefabManager();
		pPrefabManager.HidePrefab(nID, bHide);
	}
	return pH->EndFunction();
}

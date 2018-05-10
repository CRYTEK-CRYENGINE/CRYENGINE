// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 7:10:2004   14:19 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ScriptBind_Actor.h"
#include "IMovementController.h"
#include "Item.h"
#include "Game.h"
#include "Player.h"
#include "GameCVars.h"
#include "Utility/StringUtils.h"
#include "WeaponSystem.h"
#include "FireMode.h"
#include "Projectile.h"

#include <CryGame/IGameFramework.h>
#include <IVehicleSystem.h>
#include <IGameObject.h>
#include <CryMath/Cry_Geo.h>
#include <CryMath/Cry_GeoDistance.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <IViewSystem.h>
#include <CryAISystem/IAIActor.h>
#include "GameRules.h"
#include "PickAndThrowWeapon.h"

#include "UI/HUD/HUDEventDispatcher.h"
#include "PersistantStats.h"

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning(disable: 4244)
#endif

#if !defined(_RELEASE) && !defined(PERFORMANCE_BUILD)
	#define GOD_MODE_ENABLED
#endif

//------------------------------------------------------------------------
CScriptBind_Actor::CScriptBind_Actor(ISystem *pSystem)
	: m_pSystem(pSystem)
	, m_pGameFW(gEnv->pGameFramework)
{
	Init(pSystem->GetIScriptSystem(), pSystem, 1);

	//////////////////////////////////////////////////////////////////////////
	// Init tables.
	//////////////////////////////////////////////////////////////////////////
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_Actor::

  SCRIPT_REG_FUNC(DumpActorInfo);
	SCRIPT_REG_FUNC(Revive);
	SCRIPT_REG_FUNC(Kill);
	SCRIPT_REG_FUNC(ShutDown);
	SCRIPT_REG_FUNC(SetParams);
	SCRIPT_REG_FUNC(GetHeadDir);
	SCRIPT_REG_FUNC(GetAimDir);
	SCRIPT_REG_FUNC(PostPhysicalize);
	SCRIPT_REG_FUNC(GetChannel);
	SCRIPT_REG_FUNC(IsPlayer);
	SCRIPT_REG_FUNC(IsLocalClient);
	SCRIPT_REG_FUNC(LinkToEntity);
	SCRIPT_REG_TEMPLFUNC(GetLinkedVehicleId, "");
	SCRIPT_REG_TEMPLFUNC(SetAngles,"vAngles");
	SCRIPT_REG_FUNC(GetAngles);
	SCRIPT_REG_TEMPLFUNC(SetMovementTarget,"pos,target,up,speed");
	SCRIPT_REG_TEMPLFUNC(CameraShake,"amount,duration,frequency,pos");
	SCRIPT_REG_TEMPLFUNC(SetViewShake,"shakeAngle, shakeShift, duration, frequency, randomness");

	SCRIPT_REG_TEMPLFUNC(SetExtensionParams,"extension,params");

	SCRIPT_REG_TEMPLFUNC(SvRefillAllAmmo, "refillType, refillAll, fragGrenadeCount, refillCurrentGrenadeType");
	SCRIPT_REG_TEMPLFUNC(ClRefillAmmoResult, "ammoRefilled");
	SCRIPT_REG_TEMPLFUNC(SvGiveAmmoClips, "numClips");

	SCRIPT_REG_TEMPLFUNC(SetHealth,"health");
	SCRIPT_REG_TEMPLFUNC(DamageInfo,"shooterID, targetID, weaponID, projectileID, damage, damageType, hitDirection");
	SCRIPT_REG_TEMPLFUNC(GetLowHealthThreshold, "")
	SCRIPT_REG_TEMPLFUNC(SetMaxHealth,"health");
	SCRIPT_REG_FUNC(GetHealth);
	SCRIPT_REG_FUNC(GetMaxHealth);
	SCRIPT_REG_FUNC(IsImmuneToForbiddenArea);

	SCRIPT_REG_TEMPLFUNC(SetPhysicalizationProfile, "profile");
	SCRIPT_REG_TEMPLFUNC(GetPhysicalizationProfile, "");

	SCRIPT_REG_TEMPLFUNC(QueueAnimationState,"animationState");
	SCRIPT_REG_TEMPLFUNC(CreateCodeEvent,"params");
	SCRIPT_REG_FUNC(PauseAnimationGraph);
	SCRIPT_REG_FUNC(ResumeAnimationGraph);
	SCRIPT_REG_FUNC(HurryAnimationGraph);
	SCRIPT_REG_TEMPLFUNC(SetTurnAnimationParams, "turnThresholdAngle, turnThresholdTime");

	SCRIPT_REG_TEMPLFUNC(SetSpectatorMode,"mode, target");
	SCRIPT_REG_TEMPLFUNC(GetSpectatorMode,"");
	SCRIPT_REG_TEMPLFUNC(GetSpectatorState,"");
	SCRIPT_REG_TEMPLFUNC(GetSpectatorTarget, "");

	SCRIPT_REG_TEMPLFUNC(Fall,"hitPosX, hitPosY, hitPosZ");
	
	SCRIPT_REG_TEMPLFUNC(GetExtraHitLocationInfo, "slot, partId");
	SCRIPT_REG_TEMPLFUNC(StandUp,"");

	SCRIPT_REG_TEMPLFUNC(SetForcedLookDir, "dir");
	SCRIPT_REG_TEMPLFUNC(ClearForcedLookDir, "");
	SCRIPT_REG_TEMPLFUNC(SetForcedLookObjectId, "objectId");
	SCRIPT_REG_TEMPLFUNC(ClearForcedLookObjectId, "");

	SCRIPT_REG_TEMPLFUNC(PlayAction, "action");
	SCRIPT_REG_TEMPLFUNC(PlayerSetViewAngles,"vAngles");

	SCRIPT_REG_TEMPLFUNC(CanSpectacularKillOn, "targetId");
	SCRIPT_REG_TEMPLFUNC(StartSpectacularKill, "targetId");
	SCRIPT_REG_TEMPLFUNC(RegisterInAutoAimManager,"RegisterInAutoAimManager");
	SCRIPT_REG_TEMPLFUNC(CheckBodyDamagePartFlags, "partID, materialID, bodyPartFlagsMask");
	SCRIPT_REG_TEMPLFUNC(GetBodyDamageProfileID, "bodyDamageFileName, bodyDamagePartsFileName");
	SCRIPT_REG_TEMPLFUNC(OverrideBodyDamageProfileID, "bodyDamageProfileID");
	SCRIPT_REG_FUNC(IsGod);
	SCRIPT_REG_FUNC(AcquireOrReleaseLipSyncExtension);


	//------------------------------------------------------------------------
	// NETWORK
	//------------------------------------------------------------------------
	SCRIPT_REG_TEMPLFUNC(HolsterItem, "holster");
	SCRIPT_REG_TEMPLFUNC(DropItem, "itemId");
	SCRIPT_REG_TEMPLFUNC(PickUpItem, "itemId");
	SCRIPT_REG_TEMPLFUNC(IsCurrentItemHeavy, "");
	SCRIPT_REG_TEMPLFUNC(SelectNextItem, "direction, keepHistory, category");
	SCRIPT_REG_TEMPLFUNC(SimpleFindItemIdInCategory, "category");
	SCRIPT_REG_TEMPLFUNC(PickUpPickableAmmo, "ammoName, count");

	SCRIPT_REG_TEMPLFUNC(SelectItemByName, "name, [forceFastSelect]");
	SCRIPT_REG_TEMPLFUNC(SelectItem, "itemId, forceSelect");
	SCRIPT_REG_TEMPLFUNC(SelectLastItem, "");

	//------------------------------------------------------------------------

	//------------------------------------------------------------------------
	// HIT REACTION
	//------------------------------------------------------------------------
	SCRIPT_REG_TEMPLFUNC(EnableHitReaction, "");
	SCRIPT_REG_TEMPLFUNC(DisableHitReaction, "");
	
	SCRIPT_REG_TEMPLFUNC(CreateIKLimb,"slot,limbName,rootBone,midBone,endBone,flags");

	SCRIPT_REG_TEMPLFUNC(RefreshPickAndThrowObjectPhysics, "");

	m_pSS->SetGlobalValue("ZEROG_AREA_ID", ZEROG_AREA_ID);

	m_pSS->SetGlobalValue("IKLIMB_LEFTHAND", IKLIMB_LEFTHAND);
	m_pSS->SetGlobalValue("IKLIMB_RIGHTHAND", IKLIMB_RIGHTHAND);

	m_pSS->SetGlobalValue("INVALID_BODYDAMAGEPROFILEID", (int)INVALID_BODYDAMAGEPROFILEID);

	for (uint32 i=0; i<BONE_ID_NUM; i++)
	{
		m_pSS->SetGlobalValue(s_BONE_ID_NAME[i], i);
	}

	SCRIPT_REG_GLOBAL(eBodyDamage_PID_None);
	SCRIPT_REG_GLOBAL(eBodyDamage_PID_Headshot);
	SCRIPT_REG_GLOBAL(eBodyDamage_PID_Foot);
	SCRIPT_REG_GLOBAL(eBodyDamage_PID_Groin);
	SCRIPT_REG_GLOBAL(eBodyDamage_PID_Knee);
	SCRIPT_REG_GLOBAL(eBodyDamage_PID_WeakSpot);
}

//------------------------------------------------------------------------
CScriptBind_Actor::~CScriptBind_Actor()
{
}


//////////////////////////////////////////////////////////////////////////
int CScriptBind_Actor::PlayerSetViewAngles(IFunctionHandler *pH,Ang3 vAngles)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	if (pActor->IsPlayer())
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
		pPlayer->SetViewRotation( Quat( vAngles ) );
	}

	return pH->EndFunction();
}


//------------------------------------------------------------------------
void CScriptBind_Actor::AttachTo(IActor *pActor)
{
	IScriptTable *pScriptTable = pActor->GetEntity()->GetScriptTable();

	if (pScriptTable)
	{
		SmartScriptTable thisTable(m_pSS);

		thisTable->SetValue("__this", ScriptHandle(pActor->GetEntityId()));
		thisTable->Delegate(GetMethodsTable());

		pScriptTable->SetValue("actor", thisTable);
	}
}

//------------------------------------------------------------------------
CActor *CScriptBind_Actor::GetActor(IFunctionHandler *pH)
{
	void *pThis = pH->GetThis();

	if (pThis)
	{
		IActor *pActor = m_pGameFW->GetIActorSystem()->GetActor((EntityId)(UINT_PTR)pThis);
		if (pActor)
			return static_cast<CActor *>(pActor);
	}

	return 0;
}


//------------------------------------------------------------------------
int CScriptBind_Actor::DumpActorInfo(IFunctionHandler *pH)
{
  CActor *pActor = GetActor(pH);
  if (!pActor)
    return pH->EndFunction();

  pActor->DumpActorInfo();
  
  return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::Revive(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	pActor->Revive(CActor::kRFR_ScriptBind);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::Kill(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	pActor->Kill();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::ShutDown(IFunctionHandler *pH)
{
	CActor* pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	pActor->ShutDown();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
//set the actor params, pass the params table to the actor
int CScriptBind_Actor::SetParams(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	if (pActor)
	{
		SmartScriptTable params;

		if (pH->GetParamType(1) != svtNull && pH->GetParam(1, params))
			pActor->SetParamsFromLua(params);
	}
	
	return pH->EndFunction();
}

// has to be changed! (maybe bone position)
//------------------------------------------------------------------------
int CScriptBind_Actor::GetHeadDir(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	Vec3 headDir = FORWARD_DIRECTION;

	if (IMovementController * pMC = pActor->GetMovementController())
	{
		SMovementState ms;
		pMC->GetMovementState( ms );
		headDir = ms.eyeDirection;
	}

	return pH->EndFunction(Script::SetCachedVector( headDir, pH, 1 ));
}

//------------------------------------------------------------------------
int CScriptBind_Actor::GetAimDir(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	Vec3 aimDir = FORWARD_DIRECTION;

	if (IMovementController * pMC = pActor->GetMovementController())
	{
		SMovementState ms;
		pMC->GetMovementState( ms );
		aimDir = ms.aimDirection;
	}

	return pH->EndFunction(Script::SetCachedVector( aimDir, pH, 1 ));
}

//------------------------------------------------------------------------
int CScriptBind_Actor::GetChannel(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	return pH->EndFunction( (int)pActor->GetChannelId() );
}

//------------------------------------------------------------------------
int CScriptBind_Actor::IsPlayer(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	if (pActor && pActor->IsPlayer())
		return pH->EndFunction(1);
	else
		return pH->EndFunction();
} 

//------------------------------------------------------------------------
int CScriptBind_Actor::IsLocalClient(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	if (pActor && pActor->IsClient())
		return pH->EndFunction(1);
	else
		return pH->EndFunction();
} 

//------------------------------------------------------------------------
int CScriptBind_Actor::PostPhysicalize(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	if (pActor)
		pActor->PostPhysicalize();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::GetLinkedVehicleId(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	if (pActor)
	{
		ScriptHandle vehicleHandle;

		if (IVehicle* pVehicle = pActor->GetLinkedVehicle())
		{
			vehicleHandle.n = pVehicle->GetEntityId();
			return pH->EndFunction(vehicleHandle);
		}
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::LinkToEntity(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	if (pActor)
	{
		IEntity *pEntity(0);
		ScriptHandle entityId;

		entityId.n = 0;
		if (pH->GetParamType(1) != svtNull)
			pH->GetParam(1, entityId);

		pActor->LinkToEntity((EntityId)entityId.n);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
//TOFIX:rendundant with CScriptBind_Entity::SetAngles
int CScriptBind_Actor::SetAngles(IFunctionHandler *pH,Ang3 vAngles)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	if (pActor)
		pActor->SetAngles(vAngles);

	return pH->EndFunction();
}

int CScriptBind_Actor::GetAngles(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	Ang3 angles(0,0,0);

	if (pActor)
		angles = pActor->GetAngles();

	return pH->EndFunction( Script::SetCachedVector( (Vec3)angles, pH, 1 ) );
}

int CScriptBind_Actor::SetMovementTarget(IFunctionHandler *pH, Vec3 pos, Vec3 target, Vec3 up, float speed )
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	if (pActor)
		pActor->SetMovementTarget(pos,target,up,speed);
		
	return pH->EndFunction();
}

int CScriptBind_Actor::CameraShake(IFunctionHandler *pH,float amount,float duration,float frequency,Vec3 pos)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

  const char* source = "";
  if (pH->GetParamType(5) != svtNull)
    pH->GetParam(5, source);
    
	pActor->CameraShake(amount,0,duration,frequency,pos,0,source);
		
	return pH->EndFunction();
}

int CScriptBind_Actor::SetViewShake(IFunctionHandler *pH, Ang3 shakeAngle, Vec3 shakeShift, float duration, float frequency, float randomness)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	EntityId actorId = pActor->GetEntityId();
	IView* pView = m_pGameFW->GetIViewSystem()->GetViewByEntityId(actorId);
	if (pView)
	{
		const int SCRIPT_SHAKE_ID = 42;
		pView->SetViewShake(shakeAngle, shakeShift, duration, frequency, randomness, SCRIPT_SHAKE_ID);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::SetExtensionParams(IFunctionHandler* pH, const char *extension, SmartScriptTable params)
{
	CActor * pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();
	bool ok = false;
	if (pActor)
		ok = pActor->GetGameObject()->SetExtensionParams(extension, params);
	if (!ok)
		pH->GetIScriptSystem()->RaiseError("Failed to set params for extension %s", extension);
	return pH->EndFunction();
}

//------------------------------------------------------------------------
//If refillCurrentGrenadeType is true then give grenadeCount of the users inventory grenade type rather than frag grenades (If false defaults to frags)
int CScriptBind_Actor::SvRefillAllAmmo(IFunctionHandler* pH, const char* refillType, bool refillAll, int grenadeCount, bool refillCurrentGrenadeType)
{
	CRY_ASSERT(gEnv->bServer);

	if (!gEnv->bServer)
		return pH->EndFunction(0);

	CActor * pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction(0);

	IInventory *pInventory=pActor->GetInventory();
	if (!pInventory)
		return pH->EndFunction(0);

	bool ammoCollected = false;

	IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
	CRY_ASSERT(pItemSystem);

	//1.- First, Refill ammo for all weapons we have, except explosives/grenades
	if(refillAll || (refillType && strlen(refillType) > 0))
	{
		const int itemCount = pInventory->GetCount();
		for (int i = 0; i < itemCount; ++i)
		{
			IItem* pItem = pItemSystem->GetItem(pInventory->GetItem(i));

			CWeapon* pWeapon = pItem ? static_cast<CWeapon*>(pItem->GetIWeapon()) : NULL;
			if (pWeapon && pWeapon->CanPickUpAutomatically() && !IsGrenadeClass(pWeapon->GetEntity()->GetClass()))
				ammoCollected |= pWeapon->RefillAllAmmo(refillType, refillAll);
		}
	}

	//2. Next, handle explosives/grenades
	if (grenadeCount > 0)
	{
		if(refillCurrentGrenadeType)
		{
			const int grenadeCategories = eICT_Explosive|eICT_Grenade;
			const int numItems = pInventory->GetCount();

			for(int i = 0; i < numItems; i++)
			{
				EntityId itemId = pInventory->GetItem(i);

				IItem* pItem = pItemSystem->GetItem(itemId);

				if(pItem)
				{
					IEntityClass* pItemClass = pItem->GetEntity()->GetClass();
					const char* category = pItemSystem->GetItemCategory(pItemClass->GetName());
					const int categoryType = GetItemCategoryType(category);	

					if(categoryType & grenadeCategories)
					{
						ammoCollected |= RefillOrGiveGrenades(*pActor, *pInventory, pItemClass, grenadeCount);
						break;
					}
				}
			}
		}
		else
		{
			ammoCollected |= RefillOrGiveGrenades(*pActor, *pInventory, CItem::sFragHandGrenadesClass, grenadeCount);
		}
	}

	return pH->EndFunction(ammoCollected ? 1 : 0);
}

//------------------------------------------------------------------------
int CScriptBind_Actor::ClRefillAmmoResult(IFunctionHandler* pH, bool ammoRefilled)
{
	CActor * pActor = GetActor(pH);
	if (pActor)
	{
		if (ammoRefilled)
		{
			CAudioSignalPlayer::JustPlay("Player_GrabAmmo", pActor->GetEntityId());
			if (pActor->GetActorClass() == CPlayer::GetActorClassType())
			{
				static_cast<CPlayer*>(pActor)->RefillAmmo();
			}
		}
		else
		{
			CAudioSignalPlayer::JustPlay("Player_AmmoFull", pActor->GetEntityId());
			SHUDEvent event(eHUDEvent_OnAmmoPickUp);
			event.AddData(SHUDEventData((void*)0));
			event.AddData(SHUDEventData((int)0));
			event.AddData(SHUDEventData((void*)0));
			CHUDEventDispatcher::CallEvent(event);
		}
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::SetHealth(IFunctionHandler *pH, float health)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	pActor->SetHealth(health);

	return pH->EndFunction(pActor->GetHealth());
}
//------------------------------------------------------------------------
int CScriptBind_Actor::DamageInfo(IFunctionHandler *pH, ScriptHandle shooter, ScriptHandle target, ScriptHandle weapon, ScriptHandle projectile, float damage, int damageType, Vec3 hitDirection)
{
	EntityId shooterID = (EntityId)shooter.n;
	EntityId targetID = (EntityId)target.n;
	EntityId weaponID = (EntityId)weapon.n;
	EntityId projectileID = (EntityId)projectile.n;
	CProjectile *pProjectile = g_pGame->GetWeaponSystem()->GetProjectile(projectileID);
	IEntityClass *pProjectileClass = (pProjectile ? pProjectile->GetEntity()->GetClass() : 0);
	CActor *pActor = GetActor(pH);
	if (pActor)
	{
		pActor->DamageInfo(shooterID, weaponID, pProjectileClass, damage, damageType, hitDirection);
	}
	return pH->EndFunction();
}

int CScriptBind_Actor::GetLowHealthThreshold(IFunctionHandler *pH)
{
	return pH->EndFunction(g_pGameCVars->g_playerLowHealthThreshold);
}

//------------------------------------------------------------------------
int CScriptBind_Actor::SetMaxHealth(IFunctionHandler *pH, float health)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	pActor->SetMaxHealth(health);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::GetHealth(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	return pH->EndFunction(pActor->GetHealth());
}

//------------------------------------------------------------------------
int CScriptBind_Actor::IsImmuneToForbiddenArea(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
	{
		return pH->EndFunction();
	}

	return pH->EndFunction(pActor->ImmuneToForbiddenZone());
}

//------------------------------------------------------------------------
int CScriptBind_Actor::GetMaxHealth(IFunctionHandler *pH)
{
  CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	return pH->EndFunction(pActor->GetMaxHealth());
}

//------------------------------------------------------------------------
int CScriptBind_Actor::QueueAnimationState(IFunctionHandler *pH, const char *animationState)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	pActor->QueueAnimationState(animationState);
	
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::CreateCodeEvent(IFunctionHandler *pH,SmartScriptTable params)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	return (pActor->CreateCodeEvent(params));
}

//------------------------------------------------------------------------
int CScriptBind_Actor::PauseAnimationGraph(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (pActor)
		pActor->GetAnimationGraphState()->Pause(true, eAGP_PlayAnimationNode);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::ResumeAnimationGraph(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (pActor)
		pActor->GetAnimationGraphState()->Pause(false, eAGP_PlayAnimationNode);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::HurryAnimationGraph(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (pActor)
		pActor->GetAnimationGraphState()->Hurry();

	return pH->EndFunction();
}


// ===========================================================================
//  Set turn animation parameters.
//
//  In:     The function handler.
//  In:     Threshold angle, in degrees, that is needed before turning is 
//          even considered (>= 0.0f will not modify).
//  In:     The current angle deviation needs to be over the turnThresholdAngle 
//          for longer than this time before the character turns (>= 0.0f will 
//          not modify).
//
//	Returns:	A default exit code (in Lua: void).
//
int CScriptBind_Actor::SetTurnAnimationParams(
	IFunctionHandler* pH, const float turnThresholdAngle, const float turnThresholdTime)
{
	bool result = false;

	if ( (turnThresholdAngle < 0.0f) || (turnThresholdTime < 0.0f) )
	{
		CryLog("SetTurnAnimationParams(): Invalid parameter(s)!");
		assert(false);
	}
	else
	{
		CActor* actor = GetActor(pH);
		if (actor != NULL)
		{
			actor->SetTurnAnimationParams(turnThresholdAngle, turnThresholdTime);
		}
		else
		{
			CryLog("SetTurnAnimationParams(): Unable to obtain actor!");
		}
	}

	return pH->EndFunction();
}


//------------------------------------------------------------------------
int CScriptBind_Actor::SetSpectatorMode(IFunctionHandler *pH, int mode, ScriptHandle targetId)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	pActor->SetSpectatorModeAndOtherEntId(mode, (EntityId)targetId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::GetSpectatorMode(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();
	return pH->EndFunction(pActor->GetSpectatorMode());
}

//------------------------------------------------------------------------
int CScriptBind_Actor::GetSpectatorState(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();
	return pH->EndFunction(pActor->GetSpectatorState());
}

//------------------------------------------------------------------------
int CScriptBind_Actor::GetSpectatorTarget(IFunctionHandler* pH)
{
	CActor* pActor = GetActor(pH);
	if(!pActor)
		return pH->EndFunction();

	return pH->EndFunction(pActor->GetSpectatorTarget());
}

//------------------------------------------------------------------------
int CScriptBind_Actor::Fall(IFunctionHandler *pH, Vec3 hitPos)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	// [Mikko] 11.10.2007 - Moved the check here, since it was causing too much trouble in CActor.Fall().
	// The point of this filtering is to mostly mask out self-induced collision damage on friendly NPCs
	// which are playing special animations.

	bool bForce = false;
	if (pH->GetParamType(2) != svtNull)
		pH->GetParam(2, bForce);

	if(!g_pGameCVars->g_enableFriendlyFallAndPlay && !bForce)
	{
		if (const IAnimatedCharacter* pAC = pActor->GetAnimatedCharacter())
		{
			if ((pAC->GetPhysicalColliderMode() == eColliderMode_NonPushable) ||
				(pAC->GetPhysicalColliderMode() == eColliderMode_PushesPlayersOnly))
			{
				// Only mask for player friendly NPCs.
				if (pActor->GetEntity() && pActor->GetEntity()->GetAI())
				{
					IAIObject* pAI = pActor->GetEntity()->GetAI();
					IAIObject* playerAI = 0;
					if (IActor* pPlayerActor = m_pGameFW->GetClientActor())
						playerAI = pPlayerActor->GetEntity()->GetAI();

					if (playerAI && gEnv->pAISystem
						&& gEnv->pAISystem->GetFactionMap().GetReaction(playerAI->GetFactionID(), pAI->GetFactionID()) > IFactionMap::Hostile)
						return pH->EndFunction();
				}
			}
		}
	}

	if( static_cast<CPlayer*> (pActor)->CanFall() )
	{
		pActor->Fall(hitPos);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::GetExtraHitLocationInfo(IFunctionHandler *pH, int slot, int partId)
{
	CActor* pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	IEntity* pEntity = pActor->GetEntity();
	if (!pEntity)
		return pH->EndFunction();

	ICharacterInstance* pCharacter = pActor->GetEntity()->GetCharacter(slot);
	if (!pCharacter)
		return pH->EndFunction();

	IPhysicalEntity* pPE = pCharacter->GetISkeletonPose()->GetCharacterPhysics();
	if (!pPE)
		return pH->EndFunction();

	pe_status_pos posStatus;
	pe_params_part part;
	part.partid = partId;

	if (!pPE->GetParams(&part) || !pPE->GetStatus(&posStatus))
		return pH->EndFunction();

	SmartScriptTable result = Script::GetCachedTable(pH, 3);

	Matrix34 worldTM = Matrix34::Create(Vec3(posStatus.scale, posStatus.scale, posStatus.scale), posStatus.q, posStatus.pos);
	Matrix34 partWorldTM = worldTM * Matrix34::Create(Vec3(part.scale, part.scale, part.scale), part.q, part.pos);

	result->SetValue("partId", partId);
	result->SetValue("ipart", part.ipart);
	result->SetValue("mass", part.mass);
	result->SetValue("scale", part.scale);
	result->SetValue("pos", partWorldTM.GetTranslation());
	result->SetValue("dir", partWorldTM.GetColumn1());

	IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
	ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
	if (!pSkeletonPose)
		return pH->EndFunction();

	result->SetToNull("attachmentName");
	result->SetToNull("boneName");

	// 1000 is a magic number in CryAnimation
	// see CAttachmentManager::PhysicalizeAttachment
	const int firstAttachmentPartId = 1000;
	
	if (partId >= firstAttachmentPartId)
	{
		IAttachmentManager *pAttchmentManager = pCharacter->GetIAttachmentManager();
		if (IAttachment* pAttachment = pAttchmentManager->GetInterfaceByIndex(partId-firstAttachmentPartId))
			result->SetValue("attachmentName", pAttachment->GetName());
	}
	else
	{
		if (const char* boneName = rIDefaultSkeleton.GetJointNameByID(pSkeletonPose->getBonePhysParentOrSelfIndex(partId)))
			result->SetValue("boneName", boneName);
	}

	return pH->EndFunction(result);
}


//------------------------------------------------------------------------
int CScriptBind_Actor::StandUp(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	pActor->StandUp();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::SetForcedLookDir(IFunctionHandler *pH, CScriptVector dir)
{
	CPlayer* pPlayer = static_cast<CPlayer*>(GetActor(pH));
	if (pPlayer)
	{
		pPlayer->SetForcedLookDir(dir.Get());
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::ClearForcedLookDir(IFunctionHandler *pH)
{
	CPlayer* pPlayer = static_cast<CPlayer*>(GetActor(pH));
	if (pPlayer)
	{
		pPlayer->ClearForcedLookDir();
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::SetForcedLookObjectId(IFunctionHandler *pH, ScriptHandle objectId)
{
	CPlayer* pPlayer = static_cast<CPlayer*>(GetActor(pH));
	if (pPlayer)
	{
		pPlayer->SetForcedLookObjectId((EntityId)objectId.n);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::ClearForcedLookObjectId(IFunctionHandler *pH)
{
	CPlayer* pPlayer = static_cast<CPlayer*>(GetActor(pH));
	if (pPlayer)
	{
		pPlayer->ClearForcedLookObjectId();
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::SetPhysicalizationProfile(IFunctionHandler *pH, const char *profile)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();
	
	uint32 p = 0;
	if (!stricmp(profile, "alive"))
		p = eAP_Alive;
	else if (!stricmp(profile, "unragdoll"))
		p = eAP_NotPhysicalized;
	else if (!stricmp(profile, "ragdoll"))
	{
		if (!pActor->GetLinkedVehicle())
			p = eAP_Ragdoll;
		else
			p = eAP_Alive;
	}
	else if (!stricmp(profile, "spectator"))
		p = eAP_Spectator;
	else if (!stricmp(profile, "sleep"))
		p = eAP_Sleep;
	else
		return pH->EndFunction();

	//Don't turn ragdoll while grabbed
	if(p==eAP_Ragdoll && !pActor->CanRagDollize())
		return pH->EndFunction();

	pActor->GetGameObject()->SetAspectProfile(eEA_Physics, p);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::GetPhysicalizationProfile(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	uint8 profile=pActor->GetGameObject()->GetAspectProfile(eEA_Physics);
	const char *profileName;
	if (profile == eAP_Alive)
		profileName="alive";
	else if (profile == eAP_NotPhysicalized)
		profileName = "unragdoll";
	else if (profile == eAP_Ragdoll)
		profileName = "ragdoll";
	else if (profile == eAP_Sleep)
		profileName = "sleep";
	else if (profile == eAP_Spectator)
		profileName = "spectator";
	else
		return pH->EndFunction();

	return pH->EndFunction(profileName);
}

//------------------------------------------------------------------------
int CScriptBind_Actor::DisableHitReaction(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (pActor)
	{
		pActor->DisableHitReactions();
	}	
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::EnableHitReaction(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (pActor)
	{
		pActor->EnableHitReactions();
	}	
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::CreateIKLimb( IFunctionHandler *pH, int slot, const char *limbName, const char *rootBone, const char *midBone, const char *endBone, int flags)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	if (pActor)
		pActor->CreateIKLimb(SActorIKLimbInfo(slot,limbName,rootBone,midBone,endBone,flags));

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::HolsterItem(IFunctionHandler *pH, bool holster)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	pActor->HolsterItem(holster);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::DropItem(IFunctionHandler *pH, ScriptHandle itemId)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	float impulse=1.0f;
	bool bydeath=false;

	if (pH->GetParamCount()>1 && pH->GetParamType(2)==svtNumber)
		pH->GetParam(2, impulse);

	if (pH->GetParamCount()>2 && pH->GetParamType(3)==svtNumber||pH->GetParamType(2)==svtBool)
		pH->GetParam(3, bydeath);

	pActor->DropItem((EntityId)itemId.n, impulse, true, bydeath);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::PickUpItem(IFunctionHandler *pH, ScriptHandle itemId)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	bool select=true;
	if (pH->GetParamCount() > 1)
	{
		pH->GetParam(2, select);
	}

	pActor->PickUpItem((EntityId)itemId.n, true, select);

	return pH->EndFunction();
}

int CScriptBind_Actor::IsCurrentItemHeavy( IFunctionHandler* pH )
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	bool result = static_cast<CPlayer*> (pActor)->HasHeavyWeaponEquipped();

	return pH->EndFunction( result );
}


//------------------------------------------------------------------------
int CScriptBind_Actor::PickUpPickableAmmo(IFunctionHandler *pH, const char *ammoName, int count)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	IInventory *pInventory = pActor->GetInventory();
	if (!pInventory)
		return pH->EndFunction();

	IEntityClass* pAmmoClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammoName);
	if (!pAmmoClass)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Ammo class %s not found!", ammoName);
		return pH->EndFunction();
	}

	int currAmmo = pInventory->GetAmmoCount( pAmmoClass );
	int finalAmmo = currAmmo + count;
	pInventory->SetAmmoCount( pAmmoClass, finalAmmo );
	pInventory->RMIReqToServer_SetAmmoCount( pAmmoClass->GetName(), finalAmmo );
	if (pActor->IsPlayer())
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
		pPlayer->OnPickedUpPickableAmmo( pAmmoClass, count );
	}

	return pH->EndFunction();
}


//------------------------------------------------------------------------
int CScriptBind_Actor::SelectLastItem(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	pActor->SelectLastItem(true);

	return pH->EndFunction();
}


//------------------------------------------------------------------------
int CScriptBind_Actor::SelectItemByName(IFunctionHandler *pH, const char *name)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	bool forceFastSelect = false;
	if (pH->GetParamType(2) == svtBool)
		pH->GetParam(2, forceFastSelect);

	pActor->SelectItemByName(name, true, forceFastSelect);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::SelectItem(IFunctionHandler *pH, ScriptHandle itemId, bool forceSelect)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	pActor->SelectItem((EntityId)itemId.n, true, forceSelect);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::SelectNextItem(IFunctionHandler *pH, int direction, bool keepHistory, const char *category)
{
	CActor *pActor = GetActor(pH);
	bool found = pActor && (pActor->GetInventory()->GetCountOfCategory(category) > 0);
	if (found)
	{
		int categoryType = GetItemCategoryType(category);
		pActor->SelectNextItem(direction, keepHistory, categoryType);
	}

	return pH->EndFunction(found);
}

//------------------------------------------------------------------------
int CScriptBind_Actor::SimpleFindItemIdInCategory(IFunctionHandler *pH, const char *category)
{
	CActor *pActor = GetActor(pH);
	ScriptHandle result;

	if (pActor)
	{
		EntityId foundEntityId = pActor->SimpleFindItemIdInCategory(category);
		result.n=foundEntityId;
	}

	return pH->EndFunction(result);
}

//------------------------------------------------------------------------
int CScriptBind_Actor::PlayAction(IFunctionHandler *pH, const char* action)
{
	CActor *pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction();

	pActor->PlayAction(action, "");

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Actor::CanSpectacularKillOn(IFunctionHandler *pH, ScriptHandle targetId)
{
	bool bResult = false;

	CActor* pActor = GetActor(pH);
	if (pActor && (pActor->GetActorClass() == CPlayer::GetActorClassType()))
	{
		CPlayer* pKiller = static_cast<CPlayer*>(pActor);
		bResult = pKiller->GetSpectacularKill().CanSpectacularKillOn((EntityId)targetId.n);
	}

	return pH->EndFunction(bResult);
}

//------------------------------------------------------------------------
int CScriptBind_Actor::StartSpectacularKill(IFunctionHandler *pH, ScriptHandle targetId)
{
	bool bResult = false;

	CActor* pActor = GetActor(pH);
	if (pActor && (pActor->GetActorClass() == CPlayer::GetActorClassType()))
	{
		CPlayer* pKiller = static_cast<CPlayer*>(pActor);
		bResult = pKiller->GetSpectacularKill().StartOnTarget((EntityId)targetId.n);
	}

	return pH->EndFunction(bResult);
}

bool CScriptBind_Actor::IsGrenadeClass( const IEntityClass* pEntityClass ) const
{
	return pEntityClass == CItem::sFragHandGrenadesClass;
}

//------------------------------------------------------------------------
int CScriptBind_Actor::RegisterInAutoAimManager(IFunctionHandler *pH)
{
	CActor *pActor = GetActor(pH);

	if (pActor)
		pActor->RegisterInAutoAimManager();

	return pH->EndFunction();
}


// ===========================================================================
//	Check certain flags on a body damage part have been set.
//
//	In:		The function handler
//	In:		The part ID (often obtained from hit information).
//	In:		The material ID (often obtained from hit information).
//	In:		The flag masks (must be a combination of any of the 
//			eBodyDamage_PID_xxx values).
//
//	Returns:	A default exit code (in Lua: True if all of the flags in the 
//				mask match, otherwise false).
//
int CScriptBind_Actor::CheckBodyDamagePartFlags(
	IFunctionHandler *pH, int partID, int materialID, uint32 bodyPartFlagsMask)
{
	bool result = false;

	CActor* actor = GetActor(pH);
	if (actor != NULL)
	{
		const uint32 flags = actor->GetBodyDamagePartFlags(partID, materialID);
		result = ((flags & bodyPartFlagsMask) == bodyPartFlagsMask);
	}

	return pH->EndFunction(result);
}


// ===========================================================================
//	Get a body damage profile ID for a combination of body damage file names.
//
//	In:		The function handler
//	In:		The body damage file name (empty string will ignore).
//	In:		The body damage parts file name (empty string will ignore).
//
//	Returns:	A default exit code (in Lua: A body damage profile ID or
//				-1 on error).
//
int CScriptBind_Actor::GetBodyDamageProfileID(
	IFunctionHandler* pH, 
	const char* bodyDamageFileName, const char* bodyDamagePartsFileName)
{
	CActor* actor = GetActor(pH);
	if (actor != NULL)
	{
		const TBodyDamageProfileId profileID = actor->GetBodyDamageProfileID(
			bodyDamageFileName, bodyDamagePartsFileName);
		if (profileID == INVALID_BODYDAMAGEPROFILEID)
		{
			CryLog("GetBodyDamageProfileID(): Unable to obtain profile ID for '%s' and '%s'.",
				bodyDamageFileName, bodyDamagePartsFileName);
		}
		return pH->EndFunction(profileID);
	}

	return pH->EndFunction();
}


// ===========================================================================
//	Override a damage profile ID.
//
//	In:		The function handler
//	In:		The damage profile ID (INVALID_BODYDAMAGEPROFILEID will revert to
//			the default). Also see LookUpBodyDamageProfileID().
//
//	Returns:	A default exit code (in Lua: void).
//
int CScriptBind_Actor::OverrideBodyDamageProfileID(
	IFunctionHandler* pH, 
	const int bodyDamageProfileID)
{
	CActor* actor = GetActor(pH);
	if (actor != NULL)
	{		
		actor->OverrideBodyDamageProfileID((TBodyDamageProfileId)bodyDamageProfileID);
	}

	return pH->EndFunction();
}


bool CScriptBind_Actor::RefillOrGiveGrenades( CActor& actor, IInventory& inventory, IEntityClass* pGrenadeClass, int grenadeCount )
{
	CRY_ASSERT(pGrenadeClass);

	IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
	CRY_ASSERT(pItemSystem);
	
	bool grenadesRefilled = false;
	bool hadGrenadesBefore = true;

	IItem* pGrenadeItem = pItemSystem->GetItem(inventory.GetItemByClass(pGrenadeClass));
	if (!pGrenadeItem && pGrenadeClass)
	{
		//We don't have grenades yet, give it to the player (this is SP only anyways, but can be only done in the server)
		if (gEnv->bServer)
		{
			pItemSystem->GiveItem(&actor, pGrenadeClass->GetName(), false, false, false);

			//Try to get the item again...
			pGrenadeItem = pItemSystem->GetItem(inventory.GetItemByClass(pGrenadeClass));
			
			hadGrenadesBefore = false;
		}
	}

	if (pGrenadeItem)
	{
		CWeapon* pGrenadeWeapon = static_cast<CWeapon*>(pGrenadeItem->GetIWeapon());
		if (pGrenadeWeapon)
		{
			const int fireModeCount = pGrenadeWeapon->GetNumOfFireModes();
			for (int j = 0; j < fireModeCount; ++j)
			{
				IFireMode* pFireMode = pGrenadeWeapon->GetFireMode(j);
				CRY_ASSERT(pFireMode);

				IEntityClass* pAmmoTypeClass = pFireMode->GetAmmoType();
				if (pAmmoTypeClass)
				{
					const int ammoTypeCount = hadGrenadesBefore ? inventory.GetAmmoCount(pAmmoTypeClass) : 0;
					const int ammoTypeCapacity = inventory.GetAmmoCapacity(pAmmoTypeClass);

					if (ammoTypeCount < ammoTypeCapacity)
					{
						pGrenadeWeapon->SetInventoryAmmoCount(pAmmoTypeClass, min(ammoTypeCount + grenadeCount, ammoTypeCapacity));
						grenadesRefilled = true;
					}
				}

			}
		}
	}

	return grenadesRefilled;
}


int CScriptBind_Actor::RefreshPickAndThrowObjectPhysics( IFunctionHandler *pH )
{
	CActor* pActor = GetActor(pH);
	if(pActor && pActor->GetActorClass() == CPlayer::GetActorClassType())
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
		static IEntityClass* pPickAndThrowWeaponClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("PickAndThrowWeapon");
		IItem* pItem																  = pPlayer->GetCurrentItem(false); 
		if(pItem)
		{
			if (pItem->GetEntity()->GetClass() == pPickAndThrowWeaponClass)
			{
				CPickAndThrowWeapon* pPickAndThrowWeapon = static_cast<CPickAndThrowWeapon*>(pItem);
				pPickAndThrowWeapon->OnObjectPhysicsPropertiesChanged();
			}
		}
	}
	return pH->EndFunction();
}

int CScriptBind_Actor::SvGiveAmmoClips( IFunctionHandler* pH, int numClips )
{
	CRY_ASSERT(gEnv->bServer);

	if (!gEnv->bServer)
		return pH->EndFunction(0);

	CActor * pActor = GetActor(pH);
	if (!pActor)
		return pH->EndFunction(0);

	IInventory *pInventory=pActor->GetInventory();
	if (!pInventory)
		return pH->EndFunction(0);

	int ammoCollected = 0;

	IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();

	int itemCount = pInventory->GetCount();
	for (int i = 0; i < itemCount; ++i)
	{
		IItem* pItem = pItemSystem->GetItem(pInventory->GetItem(i));

		CWeapon* pWeapon = pItem ? static_cast<CWeapon*>(pItem->GetIWeapon()) : NULL;
		if (pWeapon && pWeapon->CanPickUpAutomatically() && !IsGrenadeClass(pWeapon->GetEntity()->GetClass()))
		{
			int fireModeCount = pWeapon->GetNumOfFireModes();
			if (fireModeCount > 0)
			{
				IFireMode* pFireMode = pWeapon->GetFireMode(0);			// Only give ammo for the primary fire mode
				IEntityClass* pAmmoTypeClass = pFireMode->GetAmmoType();
				if (pAmmoTypeClass)
				{
					int ammoTypeCount = pInventory->GetAmmoCount(pAmmoTypeClass);
					int ammoTypeCapacity = pInventory->GetAmmoCapacity(pAmmoTypeClass);

					if (ammoTypeCount < ammoTypeCapacity)
					{
						int clipSize = pFireMode->GetClipSize();
						int finalAmmoCount = ammoTypeCount + (clipSize * numClips);
						finalAmmoCount = std::min(finalAmmoCount, ammoTypeCapacity);

						pWeapon->SetInventoryAmmoCount(pAmmoTypeClass, finalAmmoCount);
						ammoCollected = 1;
					}
				}
			}
		}
	}

	return pH->EndFunction(ammoCollected);
}


int CScriptBind_Actor::IsGod( IFunctionHandler *pH )
{
#ifdef GOD_MODE_ENABLED
	CActor* pActor = GetActor(pH);
	if(pActor && pActor->GetActorClass() == CPlayer::GetActorClassType())
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
		return pH->EndFunction(pPlayer->IsGod());
	}
#endif

	return pH->EndFunction(false);
}

int CScriptBind_Actor::AcquireOrReleaseLipSyncExtension(IFunctionHandler *pH)
{
	if (CActor *pActor = GetActor(pH))
	{
		pActor->AcquireOrReleaseLipSyncExtension();
	}
	return pH->EndFunction();
}

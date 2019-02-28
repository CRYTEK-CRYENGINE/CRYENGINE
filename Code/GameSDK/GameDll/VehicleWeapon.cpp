// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 20:04:2006   13:02 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "VehicleWeapon.h"
#include "Actor.h"
#include <IViewSystem.h>
#include <IVehicleSystem.h>
#include "Single.h"
#include "GameCVars.h"
#include "Player.h"
#include "Game.h"

#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/HUD/HUDEventWrapper.h"


#define MAN_VEHICLE_FRAGMENTS( f )

#define MAN_VEHICLE_CONTEXTS( f )

#define MAN_VEHICLE_TAGS( f ) \
	f( MP ) \
	f( SP )

#define MAN_VEHICLE_TAG_GROUPS( f ) \
	f( playMode )

#define MAN_VEHICLE_SCOPES( f )

#define MAN_VEHICLE_FRAGMENTTAGS( f )

MANNEQUIN_USER_PARAMS( SMannequinVehicleParams, MAN_VEHICLE_FRAGMENTS, MAN_VEHICLE_TAGS, MAN_VEHICLE_TAG_GROUPS, MAN_VEHICLE_SCOPES, MAN_VEHICLE_CONTEXTS, MAN_VEHICLE_FRAGMENTTAGS );



//------------------------------------------------------------------------
CVehicleWeapon::CVehicleWeapon()
: m_dtWaterLevelCheck(0.f)
, m_vehicleId(0)
, m_bOwnerInSeat(false)
, m_audioCacheType(eACT_None)
{  
}

//------------------------------------------------------------------------
CVehicleWeapon::~CVehicleWeapon()
{
	IVehicle* pVehicle = GetVehicle();
	if(pVehicle)
	{
		pVehicle->UnregisterVehicleEventListener(this);
	}
	AudioCache(false);	//uncache whatever is loaded
}


//------------------------------------------------------------------------
bool CVehicleWeapon::Init( IGameObject * pGameObject )
{
  if (!CWeapon::Init(pGameObject))
    return false;

	m_properties.mounted=true;

	ClearItemFlags(eIF_InformClientsAboutUse);

	AudioCache(true);	//cache third person for remote clients

  return true;
}

//------------------------------------------------------------------------
void CVehicleWeapon::PostInit( IGameObject * pGameObject )
{
  CWeapon::PostInit(pGameObject); 
}

//------------------------------------------------------------------------
void CVehicleWeapon::Reset()
{
  CWeapon::Reset();
}

//------------------------------------------------------------------------
void CVehicleWeapon::MountAtEntity(EntityId entityId, const Vec3 &pos, const Ang3 &angles)
{
  CWeapon::MountAtEntity(entityId, pos, angles);  
}

//------------------------------------------------------------------------
void CVehicleWeapon::StartUse(EntityId userId)
{
	if (m_owner.GetId() && userId != m_owner.GetId())
		return; 

  if (GetEntity()->GetParent())
  { 
		const EntityId vehicleId = GetEntity()->GetParent()->GetId();
		m_vehicleId = vehicleId;
    IVehicle* pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(vehicleId);
    assert(pVehicle && "Using VehicleWeapons on non-vehicles may lead to unexpected behavior.");

    if (pVehicle)
    {
      IVehicleSeat* pOwnerSeat = pVehicle->GetWeaponParentSeat(GetEntityId());
      IVehicleSeat* pSeatUser = pVehicle->GetSeatForPassenger(userId);
			m_bOwnerInSeat = (pOwnerSeat == pSeatUser);

			if(userId == g_pGame->GetIGameFramework()->GetClientActorId())
			{
				pVehicle->RegisterVehicleEventListener(this, "CVehicleWeapon");
			}
    }
  }
  
	SetOwnerId(userId);
  Select(true);	
	m_stats.used = true;

	EnableUpdate(true, eIUS_General);
	RequireUpdate(eIUS_General);

  if (OutOfAmmo(false))
    Reload(false);

  m_shootCounter = 0;
	
	if(userId == g_pGame->GetIGameFramework()->GetClientActorId())
	{
		SHUDEvent	event;

		event.eventType			= eHUDEvent_OnItemSelected;
		event.eventIntData	= CItem::GetEntityId();

		CHUDEventDispatcher::CallEvent(event);

		SHUDEventWrapper::FireModeChanged(this, m_firemode);
	}
}

//------------------------------------------------------------------------
void CVehicleWeapon::StopUse(EntityId userId)
{
	if (m_owner.GetId() && userId != m_owner.GetId())
		return;

	if(userId == g_pGame->GetIGameFramework()->GetClientActorId())
	{
		IVehicle* pVehicle = GetVehicle();
		if(pVehicle)
		{
			pVehicle->UnregisterVehicleEventListener(this);
		}
		AudioCache(true, true);	//change cache to third person if it's not available
	}
 
  Select(false);  	
  m_stats.used = false;	

  SetOwnerId(0);

  EnableUpdate(false);

	if(IsZoomed() || IsZoomingInOrOut())
	{
		ExitZoom();
	}

	 m_shootCounter = 0;

	 if (userId == g_pGame->GetIGameFramework()->GetClientActorId())
	 {
		 SHUDEvent event;
		 event.eventType = eHUDEvent_OnItemSelected;
		 event.eventIntData = 0;

		 CHUDEventDispatcher::CallEvent(event);
	 }
}

//------------------------------------------------------------------------
void CVehicleWeapon::StartFire()
{
  if (!CheckWaterLevel())
    return;

	if(!CanFire())
		return;

  CWeapon::StartFire();
}

//------------------------------------------------------------------------
void CVehicleWeapon::Update( SEntityUpdateContext& ctx, int update)
{
	CWeapon::Update(ctx, update);

	if(update==eIUS_General)
  { 
    if (m_fm && m_fm->IsFiring())
    {
      m_dtWaterLevelCheck -= ctx.fFrameTime;      
      
      if (m_dtWaterLevelCheck <= 0.f)
      { 
        if (!CheckWaterLevel())        
          StopFire();          
        
        m_dtWaterLevelCheck = 2.0f;
      }
    }
  
		if (m_stats.mounted)
		{
			UpdateMounted(ctx.fFrameTime);
		}
	}
}

//------------------------------------------------------------------------
bool CVehicleWeapon::CheckWaterLevel() const
{
  // if not submerged at all, skip water level check
	IVehicle* pVehicle = GetVehicle();
	if(pVehicle && pVehicle->GetStatus().submergedRatio < 0.01f)
    return true;
  
  if (gEnv->p3DEngine->IsUnderWater(GetEntity()->GetWorldPos()))
    return false;

  return true;
}

//------------------------------------------------------------------------
void CVehicleWeapon::SetAmmoCount(IEntityClass* pAmmoType, int count)
{ 
  IActor* pOwner = GetOwnerActor();
  
	if (pOwner && !pOwner->IsPlayer() && count < SWeaponAmmoUtils::GetAmmoCount(m_ammo, pAmmoType))
    return;
  
  CWeapon::SetAmmoCount(pAmmoType, count);    
}

//------------------------------------------------------------------------
void CVehicleWeapon::SetInventoryAmmoCount(IEntityClass* pAmmoType, int count)
{
  IActor* pOwner = GetOwnerActor();

  if (pOwner && !pOwner->IsPlayer())
  {
		IVehicle* pVehicle = GetVehicle();
		if (pVehicle && count < pVehicle->GetAmmoCount(pAmmoType))
      return;
  }

  CWeapon::SetInventoryAmmoCount(pAmmoType, count);
}

//------------------------------------------------------------------------
bool CVehicleWeapon::FilterView(SViewParams& viewParams)
{ 
  return false;
}

//------------------------------------------------------------------------

//---------------------------------------------------------------------------
void CVehicleWeapon::UpdateIKMounted(IActor* pActor, const Vec3& vGunXAxis)
{
  // only apply IK when the weapons user is in the weapons owner seat
  if (m_bOwnerInSeat)
    CWeapon::UpdateIKMounted(pActor,vGunXAxis);
}

//------------------------------------------------------------------------
bool CVehicleWeapon::CanZoom() const
{
  if (!CWeapon::CanZoom())
    return false;

  if (!m_bOwnerInSeat)
    return false;

	IVehicle *pVehicle = GetVehicle();
	if(!pVehicle)
		return false;

  IActor* pActor = GetOwnerActor();
	IVehicleSeat* pSeat = pActor ? pVehicle->GetSeatForPassenger(pActor->GetEntityId()) : NULL;
	IVehicleView* pView = pSeat ? pSeat->GetCurrentView() : NULL;
  if (pView && pView->IsThirdPerson())
    return false;

  return true;
}

//---------------------------------------------------------------------
void CVehicleWeapon::UpdateFPView(float frameTime)
{
	CItem::UpdateFPView(frameTime);

	const EntityId ownerId = GetOwnerId();
	
	//only update primary weapon
	IVehicle* pVehicle = GetVehicle();
	if(ownerId && pVehicle && (pVehicle->GetCurrentWeaponId(ownerId, true) != GetEntityId())) 
	{
		UpdateCrosshair(frameTime);
	}
	if (m_fm)
	{
		m_fm->UpdateFPView(frameTime);
	}
	if (m_zm)
	{
		m_zm->UpdateFPView(frameTime);
	}
}

//---------------------------------------------------------------------
void CVehicleWeapon::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	if (event == eVE_ViewChanged)
	{
		CRY_ASSERT(GetOwnerId() == g_pGame->GetIGameFramework()->GetClientActorId());

		const bool isThirdPerson = params.bParam;
		AudioCache(true, isThirdPerson);	//enable what is currently needed (automatically uncache previously unneeded
	}
}

//---------------------------------------------------------------------
void CVehicleWeapon::AudioCache( bool enable, bool isThirdPerson )
{
	REINST("needs verification!");
	//IEntity* pEntity = GetEntity();
	//if(pEntity)
	//{
	//	CryFixedStringT<32> soundCache;
	//	IEntityClass *pClass = pEntity->GetClass();

	//	const EAudioCacheType cacheType = isThirdPerson ? eACT_3p : eACT_fp;
	//	const bool cacheTypeChanging = cacheType != m_audioCacheType;

	//	//uncache previously cached (if calling disable or changing cacheType)
	//	if(m_audioCacheType != eACT_None && (!enable || cacheTypeChanging))
	//	{
	//		GetCacheName(pClass, m_audioCacheType == eACT_3p, soundCache);
	//		gEnv->pSoundSystem->RemoveCachedAudioFile(soundCache.c_str(), false);
	//		m_audioCacheType = eACT_None;
	//	}

	//	//enable and changed type so cache
	//	if(enable && cacheTypeChanging)
	//	{
	//		m_audioCacheType = cacheType;
	//		GetCacheName(pClass, isThirdPerson, soundCache);
	//		gEnv->pSoundSystem->CacheAudioFile(soundCache.c_str(), eAFCT_GAME_HINT);
	//	}
	//}
	//else
	//{
	//	GameWarning("Unable to audio cache for vehicle weapon as entity doesn't exist");
	//}
}

//---------------------------------------------------------------------
void CVehicleWeapon::GetCacheName(const IEntityClass* pClass, const bool bIsThirdPerson, CryFixedStringT<32> &outCacheName)
{
	outCacheName.Format("%s%s", pClass->GetName(), bIsThirdPerson ? "_3p" : "_fp");
}

//---------------------------------------------------------------------
IVehicle* CVehicleWeapon::GetVehicle() const
{
	return gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId);
}

void CVehicleWeapon::UpdateCurrentActionController()
{
	IActionController* pActionController = m_pItemActionController;
	if(!pActionController)
	{
		if(IVehicle* pVehicle = GetVehicle())
		{
			if(pActionController = pVehicle->GetAnimationComponent().GetActionController())
			{
				// Set the PlayMode group flag on the context.
				SAnimationContext &animContext = pActionController->GetContext();
				const SMannequinVehicleParams* pParams = GetMannequinUserParams<SMannequinVehicleParams>(animContext);
				animContext.state.SetGroup(pParams->tagGroupIDs.playMode, gEnv->bMultiplayer ? pParams->tagIDs.MP : pParams->tagIDs.SP);
			}
		}
	}
	SetCurrentActionController(pActionController);
}


// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  Mounted machine gun that can be ripped off by the player
and move around with it - vehicle mounted version
-------------------------------------------------------------------------
History:
- 16:02:2010: Created by SNH

*************************************************************************/

#include "StdAfx.h"
#include "VehicleMountedWeapon.h"

#include "Actor.h"
#include <IViewSystem.h>
#include <IVehicleSystem.h>
#include "Single.h"
#include "GameCVars.h"
#include "Player.h"
#include "Game.h"
#include "IInteractor.h"

#include "UI/HUD/HUDEventDispatcher.h"

//------------------------------------------------------------------------
CVehicleMountedWeapon::CVehicleMountedWeapon()
: m_vehicleId(0)
, m_dtWaterLevelCheck(0.f)
, m_pOwnerSeat(NULL)
, m_pSeatUser(NULL)
, m_localRipUserOffset(ZERO)
, m_previousWSpaceOffsetPosition(ZERO)
, m_usedThisFrame(false)
, m_previousVehicleRotation(IDENTITY)
{  
}

//------------------------------------------------------------------------
void CVehicleMountedWeapon::StartUse(EntityId userId)
{
	IVehicle *pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId);
	if (!IsRippingOff() && pVehicle)
	{ 
		m_pOwnerSeat = pVehicle->GetWeaponParentSeat(GetEntityId());
		m_pSeatUser = pVehicle->GetSeatForPassenger(userId);

		IActor* pOwner = GetOwnerActor();
		if (pOwner && !pOwner->IsPlayer())
		{
			SHUDEvent hudEvent(eHUDEvent_AddEntity);
			hudEvent.AddData((int)pVehicle->GetEntityId());
			CHUDEventDispatcher::CallEvent(hudEvent);
		}

		ClearItemFlags(eIF_InformClientsAboutUse);
 	}

	CHeavyMountedWeapon::StartUse(userId);

	CActor* pActor = GetOwnerActor();
	if (pActor && pActor->IsPlayer())
	{
		static_cast<CPlayer*>(pActor)->RefreshVisibilityState();
	}
}

//------------------------------------------------------------------------
void CVehicleMountedWeapon::StartFire()
{
 	if (!CheckWaterLevel())
 		return;

	CWeapon::StartFire();
}

//------------------------------------------------------------------------
void CVehicleMountedWeapon::Update( SEntityUpdateContext& ctx, int update)
{
	CHeavyMountedWeapon::Update(ctx, update);

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

		// Perform position sync if necessary
		if(IsRippingOff() && gEnv->bMultiplayer)
		{
			CorrectRipperEntityPosition(ctx.fFrameTime); 
		}
	}

	m_usedThisFrame = false;
}

//------------------------------------------------------------------------
void CVehicleMountedWeapon::CorrectRipperEntityPosition(float timeStep)
{
	IVehicle *pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId);
	if(pVehicle)
	{
		const IEntity* pVehicleEnt = pVehicle->GetEntity();

		Vec3 posDiff(ZERO);
		IActor* pOwner = GetOwnerActor();
		if (pOwner && pOwner->IsPlayer())
		{
			CPlayer* pPlayer	 = static_cast<CPlayer*>(pOwner); 
			const Matrix34& wMat = pVehicleEnt->GetWorldTM(); 
			Vec3 vehiclePos		 = wMat.GetTranslation();
			Vec3 currWSpaceRipUserOffset = wMat.TransformPoint(m_localRipUserOffset);

			posDiff = currWSpaceRipUserOffset - m_previousWSpaceOffsetPosition;

			// Don't want to overwrite anyone else changes with an absolute 'set'
			pOwner->GetEntity()->SetPos(pOwner->GetEntity()->GetWorldPos() + posDiff); 

			m_previousWSpaceOffsetPosition = currWSpaceRipUserOffset;

			//Update view limit direction based on change in vehicle rotation
			if(pPlayer->IsClient())
			{
				SViewLimitParams &viewLimits = pPlayer->GetActorParams().viewLimits;
				if(viewLimits.GetViewLimitRangeH()) //Don't do this unless we are currently horizontally constrained
				{
					Quat vehicleRotation(wMat);
					Quat rotationChange = vehicleRotation * m_previousVehicleRotation.GetInverted();

					Vec3 viewLimitDir = rotationChange * viewLimits.GetViewLimitDir();
					viewLimitDir.z = 0.f;
					viewLimitDir.Normalize();

					viewLimits.SetViewLimit(viewLimitDir, 0.01f, 0.01f, 0.f, 0.f, SViewLimitParams::eVLS_Item);

					m_previousVehicleRotation = vehicleRotation;
				}

				//Reset the pitch/roll view angles over time
				Quat viewDirFinal = pPlayer->GetViewQuatFinal();
				Ang3 viewAngles(viewDirFinal);
				float xAdjustment = (float)__fsel(viewAngles.x, max(-viewAngles.x, -0.5f * timeStep), min(-viewAngles.x, 0.5f * timeStep));
				float yAdjustment = (float)__fsel(viewAngles.y, max(-viewAngles.y, -0.5f * timeStep), min(-viewAngles.y, 0.5f * timeStep));

				if(xAdjustment || yAdjustment)
				{
					pPlayer->AddViewAngles(Ang3(xAdjustment, yAdjustment, 0.f));
				}
			}
		}
	}
}

//------------------------------------------------------------------------
bool CVehicleMountedWeapon::CheckWaterLevel() const
{
	IVehicle *pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId);
	if (!pVehicle)
		return true;

	// if not submerged at all, skip water level check
	if (pVehicle->GetStatus().submergedRatio < 0.01f)
		return true;

	if (gEnv->p3DEngine->IsUnderWater(GetEntity()->GetWorldPos()))
		return false;

	return true;
}

//------------------------------------------------------------------------
void CVehicleMountedWeapon::SetAmmoCount(IEntityClass* pAmmoType, int count)
{ 
 	IActor* pOwner = GetOwnerActor();
 
	if (pOwner && !pOwner->IsPlayer() && count < SWeaponAmmoUtils::GetAmmoCount(m_ammo, pAmmoType))
 		return;

	CHeavyMountedWeapon::SetAmmoCount(pAmmoType, count);    
}

//------------------------------------------------------------------------
void CVehicleMountedWeapon::SetInventoryAmmoCount(IEntityClass* pAmmoType, int count)
{
 	IActor* pOwner = GetOwnerActor();
	IVehicle *pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId);

	if (pVehicle && pOwner && !pOwner->IsPlayer())
	{
		if (count < pVehicle->GetAmmoCount(pAmmoType))
 			return;
 	}

	CHeavyMountedWeapon::SetInventoryAmmoCount(pAmmoType, count);
}

//------------------------------------------------------------------------
bool CVehicleMountedWeapon::CanZoom() const
{
	if (!CHeavyMountedWeapon::CanZoom())
		return false;

	IVehicle *pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId);
	if(pVehicle && !IsRippedOff())
	{
		if (m_pSeatUser != m_pOwnerSeat)
			return false;

		IActor* pActor = GetOwnerActor();
		IVehicleSeat* pSeat = pActor ? pVehicle->GetSeatForPassenger(pActor->GetEntityId()) : NULL;
		IVehicleView* pView = pSeat ? pSeat->GetCurrentView() : NULL;
		if (pView && pView->IsThirdPerson())
			return false;
	}

	return true;
}

//----------------------------------------------------------------------
void CVehicleMountedWeapon::FullSerialize(TSerialize ser)
{
	CHeavyMountedWeapon::FullSerialize(ser);
	ser.BeginGroup("VehicleMountedWeapon");
	ser.Value("vehicleId", m_vehicleId);
	ser.EndGroup();
}

//----------------------------------------------------------------------
void CVehicleMountedWeapon::PostSerialize()
{
	CHeavyMountedWeapon::PostSerialize();
	if (m_vehicleId)
		MountAtEntity(m_vehicleId, Vec3(ZERO), Ang3(ZERO));

	CActor* pActor = GetOwnerActor();
	if((pActor != NULL) && (pActor->IsClient()))
	{
		SetViewMode( eIVM_FirstPerson );
		DrawSlot( eIGS_FirstPerson, true, true );
	}
}

//----------------------------------------------------------------------
void CVehicleMountedWeapon::PerformRipOff(CActor* pOwner)
{
	IActionMapManager* pMapManager = gEnv->pGameFramework->GetIActionMapManager();
	assert(pMapManager);
	pMapManager->EnableActionMap("vehicle_general", false);

	IVehicle *pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId);
	if(pVehicle)
	{
		SVehicleEventParams params;
		params.entityId = GetEntityId();

		pVehicle->BroadcastVehicleEvent(eVE_WeaponRemoved, params);

		if(pOwner)
		{
			if(gEnv->bMultiplayer)
			{
				const IEntity* pVehicleEnt = pVehicle->GetEntity();
				IEntity* pEntity		  = pOwner->GetEntity(); 
				const Matrix34& vehWMat   = pVehicleEnt->GetWorldTM(); 
				m_previousWSpaceOffsetPosition = pEntity->GetWorldPos();
				m_localRipUserOffset = vehWMat.GetInverted().TransformPoint(m_previousWSpaceOffsetPosition); 
			}

			pOwner->LinkToVehicle(0);
		}

		m_previousVehicleRotation = pVehicle->GetEntity()->GetWorldRotation();
	}

	CHeavyMountedWeapon::PerformRipOff(pOwner);
}


void CVehicleMountedWeapon::FinishRipOff()
{
	CHeavyMountedWeapon::FinishRipOff();

 	ResetState();
}

void CVehicleMountedWeapon::ApplyViewLimit(EntityId userId, bool apply)
{
	// Ignore the application of view limits, they are handled 
	//	by vehicle code. Allow the cancelling though, since 
	//	HMG ripoff code locks the view during the rip off process

	CActor *pActor = GetActor(userId);
	if(pActor && !apply)
	{
		SActorParams &params = pActor->GetActorParams();

		params.viewLimits.ClearViewLimit(SViewLimitParams::eVLS_Item);

		pActor->SetSpeedMultipler(SActorParams::eSMR_Item, 1.0f);
	}
}

bool CVehicleMountedWeapon::CanUse( EntityId userId ) const
{
	if(m_usedThisFrame)
	{
		return false;
	}

	IVehicle *pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId);

	//Ask the owning vehicle if we can use it
	if (!IsRippingOff() && pVehicle)
	{ 
		if(!pVehicle->IsUsable(userId))
		{
			return false;
		}
	}

	//No vehicle so resort to usual use case
	return CHeavyMountedWeapon::CanUse(userId);
}

void CVehicleMountedWeapon::Use( EntityId userId )
{
	IVehicle *pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId);
	if (!IsRippingOff() && pVehicle)
	{ 
		IActor* pUserActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(userId);
		if(pUserActor && pUserActor->IsPlayer())
		{
			CPlayer* pPlayer = static_cast<CPlayer*>(pUserActor);
			IInteractor* pInteractor = pPlayer->GetInteractor();

			pVehicle->OnUsed(userId, pInteractor->GetOverSlotIdx());
			return;
		}
	}

	CHeavyMountedWeapon::Use(userId);
}

void CVehicleMountedWeapon::MountAtEntity( EntityId entityId, const Vec3 &pos, const Ang3 &angles )
{
	m_vehicleId = entityId;
	CHeavyMountedWeapon::MountAtEntity(entityId, pos, angles);
}

void CVehicleMountedWeapon::StopUse( EntityId userId )
{
	CActor* pOwner = GetOwnerActor();
	if (pOwner && pOwner->IsPlayer())
	{
		static_cast<CPlayer*>(pOwner)->RefreshVisibilityState();
	}

	//Need to make sure we clean up the state here in case the normal logic has not run for some reason (E.g. Being killed during rip off)
	if(IsRippingOff())
	{
		ResetState();
	}
	else
	{
		m_usedThisFrame = true;
	}

	CHeavyMountedWeapon::StopUse(userId);
}



bool CVehicleMountedWeapon::CanRipOff() const
{
	if (!gEnv->bMultiplayer)
	{
		return false;
	}
	return CHeavyMountedWeapon::CanRipOff();
}



void CVehicleMountedWeapon::ResetState()
{
	if(m_pOwnerSeat != NULL)
	{
		if(m_pOwnerSeat == m_pSeatUser)
		{
			m_pOwnerSeat->SetLocked(eVSLS_Locked);
		}

		m_pOwnerSeat->Exit(true);
	}

	m_vehicleId = 0;
	m_pSeatUser = NULL;
	m_pOwnerSeat = NULL;
	SetParentId(0); //remove parent ID to allow first person mode
}

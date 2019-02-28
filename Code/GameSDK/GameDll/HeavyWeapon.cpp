// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ItemSharedParams.h"
#include "HeavyWeapon.h"
#include "FireMode.h"
#include "Actor.h"
#include "GameRules.h"
#include "Player.h"
#include "PersistantStats.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "IVehicleSystem.h"


struct CHeavyWeapon::StopUseAction
{
	StopUseAction(CHeavyWeapon *_weapon, EntityId _userId): pWeapon(_weapon), userId(_userId) {};

	CHeavyWeapon *pWeapon;
	EntityId userId;

	void execute(CItem *_this)
	{
		pWeapon->StopUse(userId);
	}
};



struct CHeavyWeapon::DetachItemAction
{
	DetachItemAction(CHeavyWeapon *_weapon): m_pWeapon(_weapon) {};

	CHeavyWeapon* m_pWeapon;

	void execute(CItem *_this)
	{

		Vec3 impulseDirection = Vec3(g_pGameCVars->i_hmg_impulseLocalDirection_x, g_pGameCVars->i_hmg_impulseLocalDirection_y, g_pGameCVars->i_hmg_impulseLocalDirection_z);

		m_pWeapon->DetachItem(m_pWeapon->GetEntity(), m_pWeapon->GetOwnerActor(), 5.f, impulseDirection);
	}
};



CHeavyWeapon::CHeavyWeapon()
{
}



CHeavyWeapon::~CHeavyWeapon()
{
	if( CGameRules * pGameRules = g_pGame->GetGameRules() )
		pGameRules->UnRegisterClientConnectionListener(this);
}



bool CHeavyWeapon::CanPickUp(EntityId userId) const
{
#if USE_PC_PREMATCH
	if(CGameRules * pGameRules = g_pGame->GetGameRules())
	{
		if(pGameRules->GetPrematchState() != CGameRules::ePS_Match)
		{
			return false;
		}
	}
#endif //USE_PC_PREMATCH

	EntityId ownerId = m_owner.GetId();

	if (ownerId == userId)
		return BaseClass::CanPickUp(userId);

	CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(userId));
	if (pActor && pActor->IsPlayer())
	{
		bool alreadyCarringHeavyWeapon = static_cast<CPlayer*>(pActor)->HasHeavyWeaponEquipped();
		if (alreadyCarringHeavyWeapon)
			return false;
	}
	return BaseClass::CanPickUp(userId);
}



bool CHeavyWeapon::CanUse(EntityId userId) const
{
#if USE_PC_PREMATCH
	if(CGameRules * pGameRules = g_pGame->GetGameRules())
	{
		if(pGameRules->GetPrematchState() != CGameRules::ePS_Match)
		{
			return false;
		}
	}
#endif //USE_PC_PREMATCH

	EntityId ownerId = m_owner.GetId();

	if (ownerId == 0)
	{
		CActor* pActor = GetActor(userId);
		if (pActor && pActor->IsSwimming())
			return false;
		if (pActor && pActor->IsPlayer() && !pActor->IsSwimming())
		{
			bool alreadyCarringHeavyWeapon = static_cast<CPlayer*>(pActor)->HasHeavyWeaponEquipped();
			if (alreadyCarringHeavyWeapon)
				return false;
		}
	}

	return m_sharedparams->params.usable && m_properties.usable;
}



void CHeavyWeapon::Use(EntityId userId)
{
	if(m_properties.usable)
	{
		if (!m_owner.GetId())
		{
			StartUse(userId);
		}
		else if (m_owner.GetId() == userId)
		{
			uint32 deselectionTime = StartDeselection(false);
			m_scheduler.TimerAction(deselectionTime, CSchedulerAction<StopUseAction>::Create(StopUseAction(this, userId)), false);
		}
	}
}



void CHeavyWeapon::StartUse(EntityId userId)
{
	// holster user item here
	SetOwnerId(userId);

	CActor* pOwner = GetOwnerActor();
	if (!pOwner)
		return;

	HighlightWeapon(false);
	Physicalize(false, false);

	if(gEnv->bMultiplayer)
	{
		m_properties.usable &= strlen(g_pGameCVars->g_forceHeavyWeapon->GetString()) == 0;

		CHANGED_NETWORK_STATE(pOwner, CPlayer::ASPECT_CURRENT_ITEM);
	}

	if(IItem* pCurrentItem = pOwner->GetCurrentItem())
	{
		//Don't keep history if we're switching from Pick & Throw otherwsie we'll switch back to it when we're done with the heavy weapon
		static IEntityClass* pPickAndThrowWeaponClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("PickAndThrowWeapon");
		m_pItemSystem->SetActorItem(pOwner, GetEntityId(), pCurrentItem->GetEntity()->GetClass() != pPickAndThrowWeaponClass);
	}
	else
	{
		m_pItemSystem->SetActorItem(pOwner, GetEntityId(), true);
	}

	TriggerRespawn();

	EnableUpdate(true, eIUS_General);
	RequireUpdate(eIUS_General);

	RegisterAsUser();

	HandleHeavyWeaponPro(*pOwner);

	m_stats.brandnew = false;
	m_stats.used = true;
	m_stats.detached = false;

	if(IsClient() && gEnv->pGameFramework->GetClientActorId()==userId)
	{
		if(IEntity* pEntity = GetEntity())
		{
			const char* collectibleId = pEntity->GetClass()->GetName();
			CPersistantStats* pStats = g_pGame->GetPersistantStats();
			if(pStats && pStats->GetStat(collectibleId, EMPS_SPWeaponByName) == 0)
			{
				pStats->SetMapStat(EMPS_SPWeaponByName, collectibleId, eDatabaseStatValueFlag_Available);

				if(!gEnv->bMultiplayer)
				{
					// Show hud unlock msg
					SHUDEventWrapper::DisplayWeaponUnlockMsg(collectibleId);
				}
			}
		}
	}

	if (IsServer())
	{
		pOwner->GetGameObject()->InvokeRMIWithDependentObject(CActor::ClStartUse(), CActor::ItemIdParam(GetEntityId()), eRMI_ToAllClients|eRMI_NoLocalCalls, GetEntityId());
		g_pGame->GetGameRules()->AbortEntityRemoval(GetEntityId());
	}

	OnStartUsing();
}

void CHeavyWeapon::HandleHeavyWeaponPro(CActor& rOwner)
{
}



void CHeavyWeapon::StopUse(EntityId  userId)
{
	CActor *pActor = GetOwnerActor();
	if (!pActor)
		return;

	EnableUpdate(false);

	m_stats.used = false;

	if (IsServer())
		pActor->GetGameObject()->InvokeRMI(CActor::ClStopUse(), CActor::ItemIdParam(GetEntityId()), eRMI_ToAllClients|eRMI_NoLocalCalls);

	Drop(5.0f);
}



void CHeavyWeapon::DeselectAndDrop(EntityId userId)
{
	uint32 deselectionTime = StartDeselection(false);
	uint32 detachTime = (uint32)(deselectionTime * g_pGameCVars->i_hmg_detachWeaponAnimFraction);

	m_scheduler.TimerAction(detachTime, CSchedulerAction<DetachItemAction>::Create(DetachItemAction(this)), false);
	m_scheduler.TimerAction(deselectionTime, CSchedulerAction<StopUseAction>::Create(StopUseAction(this, userId)), false);

	if(gEnv->bServer)
	{
		GetGameObject()->InvokeRMI(ClDeselectAndDrop(), SHeavyWeaponUserParams(userId), eRMI_ToAllClients|eRMI_NoLocalCalls);
	}
}



bool CHeavyWeapon::IsPickable() const
{
	CActor* pOwner = GetOwnerActor();
	bool hasOwner = pOwner != 0 && !pOwner->IsDead();
	return !hasOwner;
}



bool CHeavyWeapon::IsHeavyWeapon() const
{
	return true;
}



bool CHeavyWeapon::IsRippedOff() const
{
	return true;
}



void CHeavyWeapon::GetAngleLimits( EStance stance, float& minAngle, float& maxAngle )
{
	const float minAngleStandValue = -50.0f;
	const float minAngleCrouchValue = -20.0f;
	const float maxAngleValue = 70.0f;

	maxAngle = maxAngleValue;
	minAngle = stance==STANCE_CROUCH ? minAngleCrouchValue : minAngleStandValue;
}



void CHeavyWeapon::PostSerialize()
{
	CWeapon::PostSerialize();
	StartUse(GetOwnerId());
}

void CHeavyWeapon::StartFire(const SProjectileLaunchParams& launchParams)
{
	m_fm->SetProjectileLaunchParams(launchParams);
	BaseClass::StartFire(launchParams);
}

FragmentID CHeavyWeapon::GetSelectAction() const
{
	if (AreAnyItemFlagsSet(eIF_Unholstering))
		return GetFragmentIds().unholster_select;
	else
		return GetFragmentIds().Select;
}



bool CHeavyWeapon::CanSelect() const
{
	if (AreAnyItemFlagsSet(eIF_Unholstering))
		return true;
	return BaseClass::CanSelect();
}




bool CHeavyWeapon::AllowInteraction(EntityId interactionEntity, EInteractionType interactionType)
{
	IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
	IVehicleSystem* pVehicleSystem = pGameFramework->GetIVehicleSystem();
	IVehicle* pInteractiveVehicle = pVehicleSystem->GetVehicle(interactionEntity);

	if (pInteractiveVehicle != 0)
	{
		return false;
	}
	else
	{
		return BaseClass::AllowInteraction(interactionEntity, interactionType);
	}
}

void CHeavyWeapon::InitClient( int channelId )
{
	BaseClass::InitClient(channelId);

	IActor *pActor = GetOwnerActor();
	if(pActor && pActor->GetChannelId() != channelId)
	{
		EntityId ownerId = pActor->GetEntityId();
		GetGameObject()->InvokeRMIWithDependentObject(ClHeavyWeaponUsed(), SHeavyWeaponUserParams(ownerId), eRMI_ToClientChannel, ownerId, channelId);
	}
	else if(m_bIsHighlighted)
	{
		GetGameObject()->InvokeRMI(ClHeavyWeaponHighlighted(), SNoParams(), eRMI_ToClientChannel, channelId);		 
	}
}

void CHeavyWeapon::OnOwnClientEnteredGame()
{
	//If we're registered it's because we want to highlight the weapon (Unless somebody has since picked it up)
	if(!GetOwner())
	{
		HighlightWeapon(true);
	}
}

IMPLEMENT_RMI(CHeavyWeapon, ClDeselectAndDrop)
{
	DeselectAndDrop(params.ownerId);

	return true;
}

IMPLEMENT_RMI(CHeavyWeapon, ClHeavyWeaponUsed)
{
	Use(params.ownerId);

	return true;
}

IMPLEMENT_RMI(CHeavyWeapon, ClHeavyWeaponHighlighted)
{
	if(g_pGame->GetIGameFramework()->GetClientActor() != NULL)
	{
		HighlightWeapon(true);
	}
	else
	{
		g_pGame->GetGameRules()->RegisterClientConnectionListener(this);
	}

	return true;
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description:  Item/weapon accessories

-------------------------------------------------------------------------
History:
- 28:1:2008   Created by Benito G.R. 

*************************************************************************/

#include "StdAfx.h"
#include "Accessory.h"
#include "Player.h"
#include "GameRules.h"
#include "ItemSharedParams.h"



void CAccessory::PickUp(EntityId pickerId, bool sound, bool select/* =true */, bool keepHistory/* =true */, const char *setup /*= NULL*/)
{
	CActor *pActor=GetActor(pickerId);
	if (!pActor)
		return;

	TriggerRespawn();

	GetEntity()->EnablePhysics(false);
	Physicalize(false, false);

	bool soundEnabled = IsSoundEnabled();
	EnableSound(sound);

	SetViewMode(0);		
	SetOwnerId(pickerId);

	CopyRenderFlags(GetOwner());

	Hide(true);
	m_stats.dropped = false;
	m_stats.detached = false;
	m_stats.brandnew = false;


	IInventory *pInventory = pActor->GetInventory();
	if (!pInventory)
	{
		GameWarning("Actor '%s' has no inventory, when trying to pickup '%s'!",pActor->GetEntity()->GetName(),GetEntity()->GetName());
		return;
	}

	if (!pActor->IsPlayer() || pActor->IsClient() || gEnv->bMultiplayer)
	{
		bool hasAccessory = pInventory->HasAccessory(GetEntity()->GetClass());
		bool hasAccessoryForThisWeapon = pInventory->HasAccessory(GetEntity()->GetClass());

		if (!hasAccessoryForThisWeapon)
			pInventory->AddAccessory(GetEntity()->GetClass());
		if (!hasAccessory)
			ProcessAccessoryAmmoCapacities(pInventory, true);
		if (!hasAccessoryForThisWeapon)
			ProcessAccessoryAmmo(pInventory);
	}

	OnPickedUp(pickerId, m_sharedparams->params.unique);	

	if (select)
	{
		PlayAction(GetFragmentIds().pickedup);
	}

	EnableSound(soundEnabled);

	bool isLocalEntity = GetEntity()->GetFlags()&(ENTITY_FLAG_CLIENT_ONLY|ENTITY_FLAG_SERVER_ONLY) ? true : false;

	if (IsServer() && !IsDemoPlayback())
	{
		if(!gEnv->bMultiplayer || isLocalEntity)
			RemoveEntity();
		else if(g_pGame->GetGameRules())
			g_pGame->GetGameRules()->ScheduleEntityRemoval(GetEntityId(),10.0f,false); //Give some time to the clients to pick the msg
	}

	if (IsServer())
	{
		GetGameObject()->SetNetworkParent(pickerId);
		if (!isLocalEntity)
		{
			pActor->GetGameObject()->InvokeRMIWithDependentObject(CActor::ClPickUp(), CActor::PickItemParams(GetEntityId(), m_stats.selected, sound), eRMI_ToAllClients|eRMI_NoLocalCalls, GetEntityId());
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CAccessory::OnEnterFirstPerson()
{
	BaseClass::OnEnterFirstPerson();

	m_stats.fp = true;
}

//////////////////////////////////////////////////////////////////////////
void CAccessory::OnEnterThirdPerson()
{
	BaseClass::OnEnterThirdPerson();

	m_stats.fp = false;
}

//////////////////////////////////////////////////////////////////////////
void CAccessory::SetParentId( EntityId parentId )
{
	BaseClass::SetParentId(parentId);

	m_stats.dropped = false;
}

//////////////////////////////////////////////////////////////////////////
void CAccessory::Physicalize( bool enable, bool rigid )
{
	const bool isMounted = (GetParentId() != 0);

	int profile = eIPhys_NotPhysicalized;
	if (enable && !isMounted)
	{
		profile = rigid ? eIPhys_PhysicalizedRigid : eIPhys_PhysicalizedStatic;
	}

	if (IsServer())
	{
		GetGameObject()->SetAspectProfile(eEA_Physics, profile);
	}

	m_deferPhysicalization = eIPhys_Max;
}

CItem::ePhysicalization CAccessory::FindPhysicalisationType( bool enable, bool rigid )
{
	const bool isMounted = (GetParentId() != 0);

	ePhysicalization physType = eIPhys_NotPhysicalized;

	if(enable && !isMounted)
	{
		physType = rigid ? eIPhys_PhysicalizedRigid : eIPhys_PhysicalizedStatic;
	}

	return physType;
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HUDEventTranslator.h"

#include "HUDEventDispatcher.h"
#include "HUDEventWrapper.h"
#include <CryGame/IGameFramework.h>
#include "Player.h"
#include "IItemSystem.h"
#include "Game.h"
#include "GameRules.h"
#include "PersistantStats.h"
#include "Weapon.h"

//////////////////////////////////////////////////////////////////////////


CHUDEventTranslator::CHUDEventTranslator()
	: m_currentWeapon(0)
	, m_currentVehicle(0)
	, m_currentActor(0)
	, m_checkForVehicleTransition(false)
{
	// Init() is controlled by CHUD::Load
}



CHUDEventTranslator::~CHUDEventTranslator()
{
	// Clear() is controlled by CHUD::Load
}



void CHUDEventTranslator::Init( void )
{
	m_currentWeapon = 0;
	m_currentVehicle = 0;
	m_currentActor = 0;
	m_checkForVehicleTransition = false;

	CHUDEventDispatcher::AddHUDEventListener(this,"OnInitLocalPlayer");
	CHUDEventDispatcher::AddHUDEventListener(this,"OnInitGameRules");
	CHUDEventDispatcher::AddHUDEventListener(this,"OnHUDReload");
	CHUDEventDispatcher::AddHUDEventListener(this,"OnSpawn");
	CHUDEventDispatcher::AddHUDEventListener(this,"OnGameLoad");
	CHUDEventDispatcher::AddHUDEventListener(this,"OnUpdate");
}



void CHUDEventTranslator::Clear( void )
{
	CHUDEventDispatcher::RemoveHUDEventListener(this);

	gEnv->pGameFramework->GetIItemSystem()->UnregisterListener(this);

	CGameRules* pGameRules = g_pGame->GetGameRules();
	if(pGameRules)
	{
		pGameRules->RemoveHitListener(this);
		pGameRules->UnRegisterKillListener(this);
	}

	UnsubscribeWeapon(m_currentWeapon);

	CPlayer* pPlayer = (CPlayer*)gEnv->pGameFramework->GetClientActor();
	if(pPlayer)
	{
		pPlayer->UnregisterPlayerEventListener(this);

		IInventory* pInventory = pPlayer->GetInventory();
		if (pInventory)
			pInventory->RemoveListener(this);
	}

	gEnv->pGameFramework->GetIVehicleSystem()->UnregisterVehicleUsageEventListener(m_currentActor, this);
	if(m_currentVehicle)
	{
		IVehicle* pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_currentVehicle);
		if(pVehicle)
		{
			pVehicle->UnregisterVehicleEventListener(this);
		}
	}
}



void CHUDEventTranslator::OnSetActorItem(IActor *pActor, IItem *pItem)
{
	if(!pActor || pActor->GetEntityId() != gEnv->pGameFramework->GetClientActorId())
		return;

	if(m_currentWeapon)
	{
		UnsubscribeWeapon(m_currentWeapon);
	}

	if(pItem)
	{
		SubscribeWeapon(pItem->GetEntityId());
	}


	SHUDEvent event;
	event.eventType = eHUDEvent_OnItemSelected;
	event.eventIntData = pItem ? pItem->GetEntityId() : 0;

	CHUDEventDispatcher::CallEvent(event);

}



void CHUDEventTranslator::OnAddItem(EntityId entityId)
{
	SHUDEvent event(eHUDEvent_OnSetInventoryAmmoCount);
	CHUDEventDispatcher::CallEvent(event);
}



void CHUDEventTranslator::OnSetAmmoCount(IEntityClass* pAmmoType, int count)
{
	SHUDEvent event(eHUDEvent_OnSetInventoryAmmoCount);
	CHUDEventDispatcher::CallEvent(event);
}



void CHUDEventTranslator::OnAddAccessory(IEntityClass* pAccessoryClass)
{
	if (!pAccessoryClass)
		return;

	CPersistantStats* pPersistantStats = CPersistantStats::GetInstance();
	CRY_ASSERT(pPersistantStats != NULL);
	if (!pPersistantStats)
		return;

	const char* accessoryName = pAccessoryClass->GetName();
	int value = pPersistantStats->GetStat(accessoryName, EMPS_AttachmentUnlocked);
	if (value == 0)
	{
		pPersistantStats->IncrementMapStats(EMPS_AttachmentUnlocked, accessoryName); // Now value is 1
	}
}



void CHUDEventTranslator::OnHit(const HitInfo& hitInfo)
{
	EntityId clientId = gEnv->pGameFramework->GetClientActorId();
	if(clientId != hitInfo.shooterId && clientId != hitInfo.targetId)
		return;

	HitInfo info = hitInfo;

	SHUDEvent event;
	event.eventType = eHUDEvent_OnHit;
	event.eventPtrData = &info;

	CHUDEventDispatcher::CallEvent(event);
}



void CHUDEventTranslator::OnStartReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType)
{
	EntityId clientId = gEnv->pGameFramework->GetClientActorId();
	if(clientId != shooterId)
		return;

	SHUDEvent startReloadEvent( eHUDEvent_OnStartReload );
	CHUDEventDispatcher::CallEvent(startReloadEvent);
}



void CHUDEventTranslator::OnEndReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType)
{
	EntityId clientId = gEnv->pGameFramework->GetClientActorId();
	if(clientId != shooterId)
		return;

	SHUDEvent event;
	event.eventType = eHUDEvent_OnReloaded;
	event.eventPtrData = pWeapon;

	CHUDEventDispatcher::CallEvent(event);
}

void CHUDEventTranslator::OnSetAmmoCount(IWeapon *pWeapon, EntityId shooterId)
{
	EntityId clientId = gEnv->pGameFramework->GetClientActorId();
	if(clientId != shooterId)
		return;

	SHUDEvent event;
	event.eventType = eHUDEvent_OnSetAmmoCount;
	event.eventPtrData = pWeapon;

	CHUDEventDispatcher::CallEvent(event);
}



void CHUDEventTranslator::OnFireModeChanged(IWeapon *pWeapon, int currentFireMode)
{
	SHUDEventWrapper::FireModeChanged( pWeapon, currentFireMode );
}



void CHUDEventTranslator::OnDeath(IActor* pActor, bool bIsGod)
{
	// in multiplayer this event occurs from PlayerStateDead::OnEnter() too early before the client
	// has had chance to increment their deaths stats. This will be done directly from the playerstats code
	// in multiplayer now. Which should fix problems with single life modes and modifiers showing the respawning
	// countdown when they shouldn't
	if (!gEnv->bMultiplayer)
	{
		EntityId clientId = gEnv->pGameFramework->GetClientActorId();
		if( clientId == pActor->GetEntityId() )
		{
			CHUDEventDispatcher::CallEvent(SHUDEvent( eHUDEvent_OnLocalPlayerDeath ));
		}
	}
}

void CHUDEventTranslator::OnEntityKilled(const HitInfo &hitInfo)
{
	EntityId clientId = gEnv->pGameFramework->GetClientActorId();
	const bool clientDeath = (clientId == hitInfo.targetId);
	const bool suicide = (hitInfo.shooterId == hitInfo.targetId);
	if(clientDeath && !suicide)
	{
		SHUDEvent localPlayerKilled(eHUDEvent_OnLocalPlayerKilled);
		localPlayerKilled.AddData(SHUDEventData((int)hitInfo.shooterId));

		CHUDEventDispatcher::CallEvent(localPlayerKilled);
	}
	else if(!clientDeath)
	{
		SHUDEvent remotePlayerDeath(eHUDEvent_OnRemotePlayerDeath);
		remotePlayerDeath.AddData(SHUDEventData((int)hitInfo.targetId));
		CHUDEventDispatcher::CallEvent(remotePlayerDeath);
	}
}

void CHUDEventTranslator::OnHealthChanged(IActor* pActor, float newHealth)
{
	EntityId clientId = gEnv->pGameFramework->GetClientActorId();
	if(clientId != pActor->GetEntityId() )
		return;

	SHUDEvent event(eHUDEvent_OnHealthChanged);
	event.AddData(SHUDEventData(newHealth));

	CHUDEventDispatcher::CallEvent(event);
}



void CHUDEventTranslator::OnItemPickedUp(IActor* pActor, EntityId itemId)
{
	EntityId clientId = gEnv->pGameFramework->GetClientActorId();
	if(clientId != pActor->GetEntityId() )
		return;

	SHUDEvent event(eHUDEvent_OnItemPickUp);
	event.AddData(SHUDEventData((int)itemId));

	CHUDEventDispatcher::CallEvent(event);
}

void CHUDEventTranslator::OnPickedUpPickableAmmo(IActor* pActor, IEntityClass* pAmmoType, int count)
{
	SHUDEvent eventPickUp(eHUDEvent_OnAmmoPickUp);
	eventPickUp.AddData(SHUDEventData((void*)NULL));
	eventPickUp.AddData(SHUDEventData(count));
	eventPickUp.AddData(SHUDEventData((void*)pAmmoType));
	CHUDEventDispatcher::CallEvent(eventPickUp);
}

void CHUDEventTranslator::SubscribeWeapon(EntityId weapon)
{
	if(weapon)
	{
		IItem* pItem = gEnv->pGameFramework->GetIItemSystem()->GetItem(weapon);
		if(pItem)
		{
			IWeapon* pWeapon = pItem->GetIWeapon();
			if(pWeapon)
			{
				pWeapon->AddEventListener(this, "hud");
				OnFireModeChanged(pWeapon, pWeapon->GetCurrentFireMode());
				m_currentWeapon = weapon;
			}
		}
	}
}



void CHUDEventTranslator::UnsubscribeWeapon(EntityId weapon)
{
	if(weapon)
	{
		IItem* pItem = gEnv->pGameFramework->GetIItemSystem()->GetItem(weapon);
		if(pItem)
		{
			IWeapon* pWeapon = pItem->GetIWeapon();
			if(pWeapon)
			{
				pWeapon->RemoveEventListener(this);
			}
		}
	}
	m_currentWeapon = 0;
}


void CHUDEventTranslator::OnStartUse(const EntityId playerId, IVehicle* pVehicle)
{
	// OnStartUse is called again when the player changes seat. In that case don't register listener again.
	EntityId vehicleId = pVehicle->GetEntityId();

	if(m_currentVehicle != vehicleId)
	{
		assert(!m_currentVehicle);
		pVehicle->RegisterVehicleEventListener(static_cast<IVehicleEventListener*>(this), "player_hud");
		m_currentVehicle = vehicleId;

		m_checkForVehicleTransition = true;

		SHUDEvent vehicleEvent(eHUDEvent_OnVehiclePlayerEnter);
		vehicleEvent.AddData(SHUDEventData((int)vehicleId));
		CHUDEventDispatcher::CallEvent(vehicleEvent);
	}
}



void CHUDEventTranslator::OnEndUse(const EntityId playerId, IVehicle* pVehicle)
{
	if(!m_currentVehicle)
		return;

	if(pVehicle)
	{
		assert(m_currentVehicle == pVehicle->GetEntityId());
		pVehicle->UnregisterVehicleEventListener(static_cast<IVehicleEventListener*>(this));

		m_checkForVehicleTransition = false;

		UnsubscribeWeapon(m_currentWeapon);

		//check for detached vehicle weapons, weapon won't be set again when leaving the vehicle with the weapon
		IActor* pLocalActor = gEnv->pGameFramework->GetClientActor();
		if(pLocalActor) 
		{
			IItem* pItem = pLocalActor->GetCurrentItem();
			if(pItem)
			{
				IWeapon* pWeapon = pItem->GetIWeapon();
				if(pWeapon)
				{
					SubscribeWeapon(pItem->GetEntityId());
				}
			}
		}

		SHUDEvent vehicleEvent(eHUDEvent_OnVehiclePlayerLeave);
		vehicleEvent.AddData(SHUDEventData((int)m_currentVehicle));
		CHUDEventDispatcher::CallEvent(vehicleEvent);
	}

	m_currentVehicle = 0;
}



void CHUDEventTranslator::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	assert( m_currentVehicle );
	assert( m_currentActor );

	if(IVehicle* pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(m_currentVehicle))
	{
		switch( event )
		{
		case eVE_Damaged:
			{
			}
			break;
		case eVE_Hit:
			{
			}
			break;
		case eVE_PassengerEnter:
			{
			}
			break;
		case eVE_PassengerExit:
			{
			}
			break;
		case eVE_PassengerChangeSeat:
			{
			}
			break;
		case eVE_SeatFreed:
			{
			}
			break;
		}
	}
}



void CHUDEventTranslator::OnHUDEvent( const SHUDEvent& event )
{
	switch(event.eventType)
	{
	case eHUDEvent_OnUpdate:
		{
			if(m_checkForVehicleTransition)
			{
				if(IVehicle* pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(m_currentVehicle))
				{
					if(IVehicleSeat* pSeat = pVehicle->GetSeatForPassenger(m_currentActor))
					{
						if(pSeat->GetCurrentTransition() == IVehicleSeat::eVT_None)
						{
							SubscribeWeapon(pVehicle->GetCurrentWeaponId(m_currentActor));
							m_checkForVehicleTransition = false;
						}
					}
				}
			}
		}
		break;
	case eHUDEvent_OnInitLocalPlayer:
	case eHUDEvent_OnSpawn:
		{
			OnInitLocalPlayer();
		}
		break;
	case eHUDEvent_OnInitGameRules:
		{
			OnInitGameRules();
		}
		break;
	case eHUDEvent_OnHUDReload:
		{
			OnInitGameRules();
			OnInitLocalPlayer();
		}
		break;
	case eHUDEvent_OnGameLoad:
		{
			OnEndUse(m_currentActor, g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(m_currentVehicle));
		}
		break;
	}
}

void CHUDEventTranslator::OnInitGameRules( void )
{
	gEnv->pGameFramework->GetIItemSystem()->RegisterListener(this);

	CGameRules* pGameRules = g_pGame->GetGameRules();
	if(pGameRules)
	{
		pGameRules->AddHitListener(this);
		pGameRules->RegisterKillListener(this);
	}
}

void CHUDEventTranslator::OnInitLocalPlayer( void )
{
	IActor* pLocalActor = gEnv->pGameFramework->GetClientActor();
	// May be called from HUD reload, before player is ready; in 
	// which case this will be called when the player _is_ ready.
	if(!pLocalActor) 
		return;

	CPlayer* pLocalPlayer = (CPlayer*)(pLocalActor);
	pLocalPlayer->RegisterPlayerEventListener(this);

	IItem* pItem = pLocalActor->GetCurrentItem();
	if(pItem)
	{
		OnSetActorItem(pLocalActor, pItem);
	}

	IInventory* pInventory = pLocalPlayer->GetInventory();
	if (pInventory)
		pInventory->AddListener(this);

	EntityId currentActor = gEnv->pGameFramework->GetClientActorId();
	if(m_currentActor && m_currentActor!=currentActor)
	{
		gEnv->pGameFramework->GetIVehicleSystem()->UnregisterVehicleUsageEventListener(m_currentActor, this);
	}
	
	m_currentActor = currentActor;

	gEnv->pGameFramework->GetIVehicleSystem()->RegisterVehicleUsageEventListener( currentActor, this );

	IVehicle* pLinkedVehicle = pLocalPlayer->GetLinkedVehicle();
	if(pLinkedVehicle)
	{
		OnStartUse(currentActor, pLinkedVehicle);
	}
}

//////////////////////////////////////////////////////////////////////////

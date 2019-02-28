// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description:
Handles player interaction...
* Sometimes prompts and responds to inputs
* Sometimes automatic
**************************************************************************/

#include "StdAfx.h"
#include "PlayerPlugin_Interaction.h"
#include "Player.h"
#include "GameCVars.h"
#include "Utility/CryWatch.h"
#include <IWorldQuery.h>
#include <IInteractor.h>
#include "AutoAimManager.h"
#include "Weapon.h"
#include "PickAndThrowWeapon.h"
#include "IVehicleSystem.h"
#include "AI/GameAIEnv.h"
#include "GameRules.h"
#include "GameRulesModules/IGameRulesObjectivesModule.h"
#include "PlayerVisTable.h"
#include "EnvironmentalWeapon.h"
#include "GameActions.h"
#include "PlayerStateLadder.h"
#include "PlayerPluginEventDistributor.h"
#include "Throw.h"


#if defined(_DEBUG)
static AUTOENUM_BUILDNAMEARRAY(s_playerInteractionTypeNames, PlayerInteractionTypeList);
#endif



namespace
{


	enum EIdenticalItem
	{
		EIdenticalItem_NotIdentical,
		EIdenticalItem_SameClass,
		EIdenticalItem_Identical,
	};


	EIdenticalItem PlayerHasIdenticalItem(IItemSystem* pItemSystem, IInventory* pPlayerInventory, CItem* pItem, IEntityClass* pItemClass)
	{
		EntityId otherItemEntityId = pPlayerInventory->GetItemByClass(pItemClass);
		if (otherItemEntityId == 0)
			return EIdenticalItem_NotIdentical;
		CItem* pOtherItem = static_cast<CItem*>(pItemSystem->GetItem(otherItemEntityId));
		if (pItem->IsIdentical(pOtherItem))
			return EIdenticalItem_Identical;
		return EIdenticalItem_SameClass;
	}



	bool IsInventoryFullForWeapon(IInventory* pPlayerInventory, IWeapon* pWeapon)
	{
		if (!pWeapon)
			return false;

		CWeapon* pWeaponImpl = static_cast<CWeapon*>(pWeapon);
		const TAmmoVector& weaponAmmo = pWeaponImpl->GetWeaponSharedParams()->ammoParams.ammo;
		int numAmmoTypes = weaponAmmo.size();

		for (int i = 0; i < numAmmoTypes; ++i)
		{
			int currentAmmo = pPlayerInventory->GetAmmoCount(weaponAmmo[i].pAmmoClass);
			int maxAmmo = pPlayerInventory->GetAmmoCapacity(weaponAmmo[i].pAmmoClass);
			if (maxAmmo == 0 || currentAmmo < maxAmmo)
				return false;
		}

		return true;
	}


}



CPlayerPlugin_Interaction::CPlayerPlugin_Interaction()
: m_currentlyLookingAtEntityId(0)
, m_lastInventoryFull(false)
, m_previousIgnoreEntityId(0)
{
	m_pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
	CRY_ASSERT(m_pItemSystem);
}


void CPlayerPlugin_Interaction::Enter( CPlayerPluginEventDistributor* pEventDist )
{
	CPlayerPlugin::Enter(pEventDist);
}


void CPlayerPlugin_Interaction::Update(float dt)
{
	assert (IsEntered());

	CRY_PROFILE_FUNCTION(PROFILE_GAME);

#if defined(_DEBUG)
	const SInteractionInfo& interactionInfo = m_ownerPlayer->GetCurrentInteractionInfo();
	IItem * pItem = m_pItemSystem->GetItem(interactionInfo.interactiveEntityId);

	PlayerPluginWatch ("Can use %s (%s)", pItem ? pItem->GetEntity()->GetName() : "nothing", s_playerInteractionTypeNames[interactionInfo.interactionType]);

	if (pItem)
	{
		EntityId newNearestEntityId = pItem->GetEntityId();
		if (m_lastNearestEntityId != newNearestEntityId)
		{
			CryLog("[INTERACTION] %s '%s' is now near interactive %s '%s' (%s)", m_ownerPlayer->GetEntity()->GetClass()->GetName(), m_ownerPlayer->GetEntity()->GetName(), pItem->GetEntity()->GetClass()->GetName(), pItem->GetEntity()->GetName(), s_playerInteractionTypeNames[interactionInfo.interactionType]);
			m_lastNearestEntityId = newNearestEntityId;
		}
	}
#endif

	m_currentlyLookingAtEntityId = m_ownerPlayer->GetGameObject()->GetWorldQuery()->GetLookAtEntityId();

	if( !g_pGameActions->FilterItemPickup()->Enabled() )
	{
		UpdateInteractionInfo();
		UpdateAutoPickup();
	}
	else
	{
		m_interactionInfo.Reset();
	}
}

void CPlayerPlugin_Interaction::UpdateAutoPickup()
{
	const Vec3 ownerPlayerPosition = m_ownerPlayer->GetEntity()->GetWorldPos();

	CGameRules* pGameRules = g_pGame->GetGameRules();
	IGameRulesObjectivesModule* pObjectives = pGameRules ? pGameRules->GetObjectivesModule() : NULL;

	bool inventoryFull = false;

	int numOfEntities = 0;
	const EntityId* entitiesAround = m_ownerPlayer->GetGameObject()->GetWorldQuery()->ProximityQuery(numOfEntities);
	for(int i = 0; i < numOfEntities; i++)
	{
		const EntityId targetEntityId = entitiesAround[i];
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(targetEntityId);
		if(pEntity)
		{
			CItem * pItem = static_cast<CItem*>(m_pItemSystem->GetItem(targetEntityId));
			IInventory* pInventory = m_ownerPlayer->GetInventory();
			//auto pickup ammo
			if(pItem != NULL && pInventory != NULL)
			{
				IEntityClass* pItemClass = pItem->GetEntity()->GetClass();
				if (IsInventoryFullForWeapon(pInventory, pItem->GetIWeapon()))
					inventoryFull = true;
				if (CanAutoPickupAmmo(pItem))
					m_ownerPlayer->PickUpItemAmmo(targetEntityId);
			}
			else if(pObjectives)
			{
				pObjectives->CheckForInteractionWithEntity(targetEntityId, m_ownerPlayer->GetEntityId(), m_interactionInfo);

				if(m_interactionInfo.interactionType != eInteraction_None && !CanInteractGivenType(targetEntityId, m_interactionInfo.interactionType, true))
				{
					m_interactionInfo.interactionType = eInteraction_None;						
				}
			}
		}
	}

	if (inventoryFull && !m_lastInventoryFull && m_ownerPlayer->IsClient())
		CAudioSignalPlayer::JustPlay("Player_AmmoFull", m_ownerPlayer->GetEntityId());
	m_lastInventoryFull = inventoryFull;
}

// This class may want to respond to events in the future... if not, this function should be removed [TF]
void CPlayerPlugin_Interaction::HandleEvent(EPlayerPlugInEvent theEvent, void * data)
{
	CPlayerPlugin::HandleEvent(theEvent, data);

	switch (theEvent)
	{
		case EPE_Reset:
			{
#if defined(_DEBUG)
				m_lastNearestEntityId = 0;
#endif
				m_interactionInfo.Reset();
				m_currentlyLookingAtEntityId = 0;
			}
			break;
	}
}

// Helper function
static EInteractionType DeterminePickAndThrowInteraction(const CPlayer* pPlayer, const IItemSystem* pItemSystem)
{
	if (pPlayer && pPlayer->IsInPickAndThrowMode() && pPlayer->GetInventory())
	{
		static IEntityClass* pPickAndThrowWeaponClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("PickAndThrowWeapon");
		EntityId pickAndThrowWeaponId = pPlayer->GetInventory()->GetItemByClass( pPickAndThrowWeaponClass );
		IItem* pItem = pItemSystem->GetItem( pickAndThrowWeaponId );
		if (pItem)
		{
			CPickAndThrowWeapon* pPickAndThrowWeapon = static_cast<CPickAndThrowWeapon*>(pItem);
			return pPickAndThrowWeapon->GetHeldEntityInteractionType();
		}
	}

	// default fallback if anything fails
	return eInteraction_LetGo;
}

void CPlayerPlugin_Interaction::UpdateInteractionInfo()
{
	m_interactionInfo.displayTime = -1.0f;

	IInteractor* pInteractor = m_ownerPlayer->GetInteractor();

	if (pInteractor != NULL)
	{
		bool withinProximity = true;
		EntityId prevInteractiveEntityId = m_interactionInfo.interactiveEntityId;
		m_interactionInfo.interactiveEntityId = pInteractor->GetOverEntityId();

		if(!m_interactionInfo.interactiveEntityId)
		{
			m_interactionInfo.interactiveEntityId = m_ownerPlayer->GetGameObject()->GetWorldQuery()->GetLookAtEntityId();
			withinProximity = false;
		}

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_interactionInfo.interactiveEntityId);

		IScriptTable *pEntityScript = NULL;
		SmartScriptTable propertiesTable;
		bool bHasPropertyTable = false;
		if (pEntity != NULL)
		{
			pEntityScript = pEntity->GetScriptTable();
			if (pEntityScript)
			{
				bHasPropertyTable = pEntityScript->GetValue("Properties", propertiesTable);

				// When our 'over entity id' changes, check for any vistTest ignore entity ids
				CPlayerVisTable* pVisTable = g_pGame->GetPlayerVisTable();
				if(prevInteractiveEntityId != m_interactionInfo.interactiveEntityId && 
				   pVisTable)
				{
					// If prev ent set an ignore ent.. remove
					if(m_previousIgnoreEntityId)
					{
						pVisTable->RemoveGlobalIgnoreEntity(m_previousIgnoreEntityId); 
					}

					// If new entity sets ignore ent.. apply
					ScriptHandle ignoreEntityId;
					if(bHasPropertyTable && propertiesTable->GetValue("visibilityIgnoreEntityId", ignoreEntityId))
					{
						m_previousIgnoreEntityId = static_cast<EntityId>(ignoreEntityId.n);
						pVisTable->AddGlobalIgnoreEntity(m_previousIgnoreEntityId, "PlayerPluginInteraction - script 'visibilityIgnoreEntityId'"); 
					}
					else
					{
						m_previousIgnoreEntityId = 0; 
					}
				}
			}
		}

		EInteractionType desiredInteraction = eInteraction_None;
		if(m_interactionInfo.interactiveEntityId != INVALID_ENTITYID)
		{
			desiredInteraction = GetInteractionTypeForEntity(m_interactionInfo.interactiveEntityId, pEntity, pEntityScript, propertiesTable, bHasPropertyTable, withinProximity);
				
			if(desiredInteraction == eInteraction_None)
			{
				EntityId buddyEntityId = 0;
				desiredInteraction = GetInteractionWithConstrainedBuddy( pEntity, &buddyEntityId, withinProximity );
				if(desiredInteraction != eInteraction_None)
				{
					m_interactionInfo.interactiveEntityId = buddyEntityId;
				}
			}
		}

		//We need to do a post-pass after finding out what the interaction type is as we want to be able to
		//	pick up ammo in MP even when reloading - Rich S
		if(CanInteractGivenType(m_interactionInfo.interactiveEntityId, desiredInteraction, withinProximity))
		{
			m_interactionInfo.interactionType = desiredInteraction;

			// If interaction type is usable, try to read in the custom use message
			if (desiredInteraction == eInteraction_Use)
			{
				if (bHasPropertyTable)
				{
					char* szUseMessage = NULL;
					if(propertiesTable->GetValue("UseMessage", szUseMessage))
					{
						m_interactionInfo.interactionCustomMsg = szUseMessage;
					}
					else
					{
						m_interactionInfo.interactionCustomMsg = "";
					}
				}
			}

			if(gEnv->bMultiplayer)
			{
				// MP only supports grabbing environmental weapons at the moment I think, so we test the entity
				// for that here, so that we don't show the prompt if not supported
				if(desiredInteraction == eInteraction_Grab)
				{
					static IEntityClass *pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("PickAndThrowWeapon");
					IInventory *pInventory = m_ownerPlayer->GetInventory(); 
					EntityId pickAndThrowWeaponId = pInventory ? pInventory->GetItemByClass(pClass) : 0;
					IItem* pItem = m_pItemSystem->GetItem( pickAndThrowWeaponId );

					if (pItem)
					{
						const EntityId envWeaponId = desiredInteraction == eInteraction_Grab ? m_interactionInfo.interactiveEntityId : pInteractor->GetOverSlotIdx();

						const CEnvironmentalWeapon *pEnvWeapon = static_cast<CEnvironmentalWeapon*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(envWeaponId, "EnvironmentalWeapon"));
						if( !pEnvWeapon || !pEnvWeapon->CanBeUsedByPlayer(m_ownerPlayer))
						{
							m_interactionInfo.interactionType = eInteraction_None;
						}
					}
					else
					{
						m_interactionInfo.interactionType = eInteraction_None;
					}
				}
			}
		}
		else
		{
			m_interactionInfo.interactionType = eInteraction_None;
			if ((desiredInteraction == eInteraction_Grab) || (desiredInteraction == eInteraction_GrabEnemy))
			{
				if (m_ownerPlayer->GetPickAndThrowEntity())
				{
					m_interactionInfo.interactionType = DeterminePickAndThrowInteraction(m_ownerPlayer, m_pItemSystem); 
				}
			}
		}

		if (m_interactionInfo.interactionType != eInteraction_Use && m_interactionInfo.interactionType != eInteraction_LargeObject && m_ownerPlayer->HasHeavyWeaponEquipped())
		{
			bool showStopUsingPrompt = true;
			IItem* pItem;
			if( m_interactionInfo.interactiveEntityId && (pItem = m_pItemSystem->GetItem(m_interactionInfo.interactiveEntityId)) )
			{
				showStopUsingPrompt = !static_cast<CItem*>(pItem)->CanRipOff();
			}

			if(strlen(g_pGameCVars->g_forceHeavyWeapon->GetString()) > 0)
			{
				showStopUsingPrompt = false;
			}

			if(m_interactionInfo.interactionType==eInteraction_Stealthkill)
			{
				showStopUsingPrompt = false;
			}

			if(showStopUsingPrompt)
			{
				m_interactionInfo.interactiveEntityId = m_ownerPlayer->GetCurrentItemId();
				m_interactionInfo.interactionType = eInteraction_StopUsing;
				m_interactionInfo.displayTime = 2.0f;
			}
		}

		if (m_interactionInfo.interactionType == eInteraction_None)
		{
			m_interactionInfo.interactiveEntityId = INVALID_ENTITYID;
		}
	}
	else
	{
		m_interactionInfo.Reset();
	}
}

EInteractionType CPlayerPlugin_Interaction::GetInteractionTypeForEntity(EntityId entityId, IEntity* pEntity, IScriptTable *pEntityScript, const SmartScriptTable& propertiesTable, bool bHasPropertyTable, bool withinProximity)
{
	if(!pEntity)
		return eInteraction_None;

	if (!CanLocalPlayerSee(pEntity))
		return eInteraction_None;

	if(CActor *pActorAI = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityId)))
	{
		//The same actor I'm interacting with already, skip
		if (m_ownerPlayer->GetPickAndThrowEntity() == entityId)
		{
			return DeterminePickAndThrowInteraction(m_ownerPlayer, m_pItemSystem); 
		}

		CStealthKill::ECanExecuteOn canStealthKill = m_ownerPlayer->GetStealthKill().CanExecuteOn(pActorAI, g_pGameCVars->pl_stealthKill_useExtendedRange == 1);
		
		if (canStealthKill==CStealthKill::CE_YES)
			return eInteraction_Stealthkill;

		if (canStealthKill!=CStealthKill::CE_NO_BUT_DONT_DO_OTHER_ACTIONS_ON_IT && CanGrabEnemy( *pActorAI ))
			return eInteraction_GrabEnemy;

		if (bHasPropertyTable)
		{
			int usable=0;
			if(propertiesTable->GetValue("bUsable", usable) && usable)
			{
				return eInteraction_Use;
			}
		}

		return eInteraction_None;
	}

	else if(CItem *pItem = static_cast<CItem*>(m_pItemSystem->GetItem(entityId)))
	{
		return GetInteractionTypeWithItem(pItem);
	}

	else if(bHasPropertyTable)
	{
		const CLargeObjectInteraction& largeObjectInteraction = m_ownerPlayer->GetLargeObjectInteraction();
		if(largeObjectInteraction.IsLargeObject( pEntity, propertiesTable ))
		{
			if(largeObjectInteraction.CanExecuteOn( pEntity, withinProximity ))
			{
				if(gEnv->bMultiplayer)	// In multiplayer we return NONE here, in SP we still run the logic below. 
				{
					return eInteraction_None;
				}
			}
			else if(gEnv->bMultiplayer)
			{
				return eInteraction_None;
			}
		}
	}

	IPhysicalEntity* pPhysicalEntity = pEntity->GetPhysics();
	bool cinematicActive = m_ownerPlayer->IsInCinematicMode();
	if (pPhysicalEntity)
	{
		bool isMarkedAsNonPickable = false;
		bool isRigidBody = (pPhysicalEntity->GetType() == PE_RIGID);
		if (bHasPropertyTable)
		{
			if (isRigidBody && !cinematicActive)
			{
				int pickable = 0;
				if (propertiesTable->GetValue("bPickable", pickable)) // [JB]: bPickable cannot be changed on a per entity instance basis as a single properties table instance is shared between ents of same archetype etc.
				{
					if(!pickable)
					{
						isMarkedAsNonPickable = true; 
					}
					else
					{
						// [JB]: Therefore even if the *type*  is pickable, we need to additionally check if *this* instance is *currently* pickable (could be held by another player). 
						// If a script table does not define this value, we assume the entity is always currently pickable. 
						int currentlyPickable = 0; 
						if(pEntityScript->GetValue("bCurrentlyPickable", currentlyPickable))
						{
							if(currentlyPickable)
							{
								return eInteraction_Grab;
							}
							else
							{
								isMarkedAsNonPickable = true; 
							}
						}
						else
						{
							return eInteraction_Grab;
						}	
					}
				}
			}

			if (strcmp(pEntity->GetClass()->GetName(), "Boid") == 0)
			{
				const SPlayerStats* pPlayerStats = static_cast<const SPlayerStats*>(m_ownerPlayer->GetActorStats());
				if (!pPlayerStats->isInPickAndThrowMode && (pPhysicalEntity->GetType() == PE_PARTICLE) || (pPhysicalEntity->GetType() == PE_ARTICULATED))
				{
					return eInteraction_Grab;
				}
			}

			if(strcmp(pEntity->GetClass()->GetName(),"Ladder")==0)
			{
				return CPlayerStateLadder::IsUsableLadder(*m_ownerPlayer, pEntity, propertiesTable) ? eInteraction_Ladder : eInteraction_None;
			}

			int usable = 0;
			if(propertiesTable->GetValue("bUsable", usable) && usable)
				return eInteraction_Use;
		}

		if (!gEnv->bMultiplayer && isRigidBody && !isMarkedAsNonPickable && !cinematicActive)
		{
			if (HasObjectGrabHelper(entityId))
				return eInteraction_Grab;
		}
	}

	return eInteraction_None;
}

EInteractionType CPlayerPlugin_Interaction::GetInteractionTypeWithItem( CItem* pTargetItem )
{
	EInteractionType interactionType = eInteraction_None;
	CRY_ASSERT(pTargetItem);
	if (!pTargetItem)
		return eInteraction_None;

	EntityId ownerPlayerId = m_ownerPlayer->GetEntityId();

	if (m_ownerPlayer->CanPickupItems())
	{
		bool canBeUsed = pTargetItem->CanUse(ownerPlayerId);

		if(canBeUsed)
		{
			IEntity* pParent = pTargetItem->GetEntity()->GetParent();
			IVehicle* pVehicle;
			if(pParent && (pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(pParent->GetId())) )
			{
				if (pVehicle->IsUsable(ownerPlayerId))
				{
					interactionType = eInteraction_Use;
				}
			}
			else if(!pTargetItem->IsUsed())
			{
				interactionType = pTargetItem->IsPickable() ? eInteraction_PickupItem : eInteraction_Use;
			}
			else if(pTargetItem->GetOwnerId() == ownerPlayerId)
			{
				interactionType = !pTargetItem->CanRipOff() || !pTargetItem->IsRippedOff() ? eInteraction_None : eInteraction_StopUsing;
			}
		}
		else 
		{
			//Should we check this?? (just ported from previous version)
			IInventory *pInventory = m_ownerPlayer->GetInventory();

			if (pInventory != NULL && pInventory->FindItem(pTargetItem->GetEntityId()) == -1 && pTargetItem->CanPickUp(ownerPlayerId))
			{
				IEntityClass* pTargetItemClass = pTargetItem->GetEntity()->GetClass();

				bool isSlotAvailable = m_ownerPlayer->CheckInventoryRestrictions(pTargetItemClass->GetName());
				EIdenticalItem identicalItemMode = PlayerHasIdenticalItem(m_pItemSystem, pInventory, pTargetItem, pTargetItemClass);
				bool inventoryHasItem = (identicalItemMode != EIdenticalItem_NotIdentical);
				bool ammoRestrictionsOk = pTargetItem->CheckAmmoRestrictions(pInventory);
				bool targetItemIsNotAutoPickableButCanBePickedUp = (inventoryHasItem && ammoRestrictionsOk && !pTargetItem->CanPickUpAutomatically());
				m_interactionInfo.swapEntityId = pInventory->GetCurrentItem();

				if (isSlotAvailable)
				{
					//Slot available and I have this item class already
					if (identicalItemMode == EIdenticalItem_NotIdentical)
						interactionType = eInteraction_PickupItem;
					else if (identicalItemMode == EIdenticalItem_SameClass)
						interactionType = eInteraction_ExchangeItem;
					else if (targetItemIsNotAutoPickableButCanBePickedUp)
						interactionType = eInteraction_PickupItem;
				}
				else if (targetItemIsNotAutoPickableButCanBePickedUp)
				{
					//I have this one already, but it requires user confirmation to be picked up (JAW, C4)
					interactionType = eInteraction_PickupItem;
				}
				else
				{
					if (identicalItemMode != EIdenticalItem_Identical)
					{
						//Check if I can exchange with current item
						IItem* pCurrentItem = m_ownerPlayer->GetCurrentItem();
						bool sameSlot = pCurrentItem != NULL && pInventory->AreItemsInSameSlot(pCurrentItem->GetEntity()->GetClass()->GetName(), pTargetItemClass->GetName());
						interactionType = eInteraction_ExchangeItem;
						if (sameSlot)
						{
							if (identicalItemMode == EIdenticalItem_SameClass)
								m_interactionInfo.swapEntityId = pInventory->GetItemByClass(pTargetItemClass);
						}
						else
						{
							const char* cat = m_pItemSystem->GetItemCategory(pTargetItemClass->GetName());
							IInventory::EInventorySlots slot = pInventory->GetSlotForItemCategory(cat);
							m_interactionInfo.swapEntityId = pInventory->GetLastSelectedInSlot(slot);
						}
					}
				}
			}
		}

#if defined(_DEBUG)
		WATCH_ACTOR_STATE_FOR (m_ownerPlayer, "%s a %s '%s', canBeUsed=%d rippedOff=%d mountable=%d mounted=%d owner=%d used=%d usePickUpInput=%d RESULT=%s", (pTargetItem->GetOwnerId() == ownerPlayerId) ? "using" : "near", pTargetItem->GetEntity()->GetClass()->GetName(), pTargetItem->GetDisplayName(), canBeUsed, pTargetItem->IsRippedOff(), pTargetItem->IsMountable(), pTargetItem->IsMounted(), pTargetItem->GetOwnerId(), pTargetItem->IsUsed(), pTargetItem->IsPickable(), s_playerInteractionTypeNames[interactionType]);
#endif
	}

	return interactionType;
}

EInteractionType CPlayerPlugin_Interaction::GetInteractionWithConstrainedBuddy(IEntity* pEntity, EntityId* pOutputBuddyEntityId, const bool withinProximity)
{
	if(gEnv->bMultiplayer)
		return eInteraction_None;

	if((pEntity == NULL) || (pOutputBuddyEntityId == NULL))
		return eInteraction_None;

	IPhysicalEntity* pPhysicalEntity = pEntity->GetPhysics();
	if(pPhysicalEntity == NULL)
		return eInteraction_None;

	// This handles the case when you are looking at one part of a kickable car (a door e.g.) which is constrained to the main car object 
	// Constraint ids start at 1, we could loop until we don't find anyone more, but checking the first one works fine for the special case we are handling here
	pe_status_constraint constraintStatus;
	constraintStatus.id = 1;
	if(pPhysicalEntity->GetStatus(&constraintStatus))
	{
		if(constraintStatus.pBuddyEntity != NULL)
		{
			IEntity* pBuddyEntity = gEnv->pEntitySystem->GetEntityFromPhysics(constraintStatus.pBuddyEntity);
			if(pBuddyEntity != NULL)
			{
				IScriptTable* pBuddyScriptTable = pBuddyEntity->GetScriptTable();
				SmartScriptTable buddyPropertiesTable;
				if(pBuddyScriptTable && pBuddyScriptTable->GetValue("Properties", buddyPropertiesTable))
				{
					const CLargeObjectInteraction& largeObjectInteraction = m_ownerPlayer->GetLargeObjectInteraction();
					const bool canInteract = largeObjectInteraction.IsLargeObject(pBuddyEntity, buddyPropertiesTable) &&
																	 largeObjectInteraction.CanExecuteOn( pEntity, withinProximity );	// We do the 'CanExecuteOn' test in the original object					        
					if(canInteract)
					{
						*pOutputBuddyEntityId = pBuddyEntity->GetId();
						return eInteraction_LargeObject;
					}
				}
			}
		}
	}

	return eInteraction_None;
}

bool CPlayerPlugin_Interaction::CanAutoPickupAmmo(CItem* pTargetItem) const
{
	CRY_ASSERT(pTargetItem);
	if (pTargetItem)
	{
		IEntity* pTargetEntity = pTargetItem->GetEntity();
		IEntityClass* pTargetItemClass = pTargetEntity->GetClass();
		
		if (CanLocalPlayerSee(pTargetEntity))
		{
			EntityId ownerPlayerId = m_ownerPlayer->GetEntityId();

			if (m_ownerPlayer->CanPickupItems() && pTargetItem->CanPickUpAutomatically())
			{
				//Should we check this?? (just ported from previous version)
				IInventory *pInventory = m_ownerPlayer->GetInventory();

				if (pInventory != NULL && pInventory->FindItem(pTargetItem->GetEntityId()) == -1 && pTargetItem->CanPickUp(ownerPlayerId))
				{
					bool ammoRestrictionsOk = pTargetItem->CheckAmmoRestrictions(pInventory);
					bool hasBonusAmmoToPickup = pTargetItem->HasSomeAmmoToPickUp(ownerPlayerId);
					bool scheduledToExchange = pTargetItem->AreAnyItemFlagsSet(CItem::eIF_ExchangeItemScheduled);

					if(ammoRestrictionsOk && hasBonusAmmoToPickup && !scheduledToExchange)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}


bool CPlayerPlugin_Interaction::CanInteractGivenType(EntityId entity, EInteractionType type, bool withinProximity ) const
{
	if(withinProximity || ((type == eInteraction_Stealthkill) || (type == eInteraction_HijackVehicle) || (type == eInteraction_LargeObject)))
	{
		const SPlayerStats* pPlayerStats = static_cast<const SPlayerStats*>(m_ownerPlayer->GetActorStats());
		CRY_ASSERT(pPlayerStats);

		bool canNotInteractInState = m_ownerPlayer->IsOnLedge() || m_ownerPlayer->IsSliding() || (pPlayerStats->animationControlledID != 0);

		if (canNotInteractInState)
			return false;

		if (m_ownerPlayer->GetStealthKill().IsBusy())
			return false;

		if(IItem* pItem = m_ownerPlayer->GetCurrentItem())
		{
			if(IWeapon* pWeapon = pItem->GetIWeapon())
			{
				if(const CThrow* pThrow = crygti_cast<CThrow*>(static_cast<CFireMode*>(pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()))))
				{
					if(pThrow->IsFiring())
					{
						return false;
					}
				}
			}
		}

		//currently busy fiddling with the own weapon
		IItem *pItem = m_ownerPlayer->GetCurrentItem(false);
		CWeapon * pMainWeapon = pItem?static_cast<CWeapon*>(pItem->GetIWeapon()):NULL;

		if(pMainWeapon != NULL && !pMainWeapon->AllowInteraction(entity, type))
		{
			return false;
		}

		if ((type == eInteraction_Grab) || (type == eInteraction_GrabEnemy))
		{
			if (m_ownerPlayer->IsSwimming() || pPlayerStats->pickAndThrowEntity != 0)
				return false;
		}

		if (type == eInteraction_GameRulesPickup && m_ownerPlayer->HasHeavyWeaponEquipped())
		{
			// Can't pick-up game rules items while carrying a heavy weapon
			return false;
		}

		return true;
	}

	return false;
}

bool CPlayerPlugin_Interaction::CanGrabEnemy( const CActor& targetActor ) const
{
	const SAutoaimTarget* pTargetInfo = g_pGame->GetAutoAimManager().GetTargetInfo( targetActor.GetEntityId() );

	bool canGrab = pTargetInfo != NULL && pTargetInfo->HasFlagsSet((int8)(eAATF_AIHostile|eAATF_CanBeGrabbed));

	if(targetActor.GetLinkedVehicle() ||
		m_ownerPlayer->IsCinematicFlagActive(SPlayerStats::eCinematicFlag_LowerWeapon) || 
		!m_ownerPlayer->IsInteractiveActionDone() ||
		targetActor.GetActorStats()->isInBlendRagdoll ||
		m_ownerPlayer->GetStance() == STANCE_CROUCH)
	{
		canGrab = false;
	}

	if(canGrab)
	{
		//Do a distance check (similar values to an average stealth kill)
		const float maxFlatDistanceSqr = (2.25f * 2.25f);
		const float maxHeightDiff = 0.75f;

		const Vec3 playerPosition = m_ownerPlayer->GetEntity()->GetWorldPos();
		const Vec3 targetPosition = targetActor.GetEntity()->GetWorldPos();

		const Vec2 flatDistance(targetPosition - playerPosition);

		canGrab = (flatDistance.GetLength2() < maxFlatDistanceSqr) && (fabs(targetPosition.z - playerPosition.z) < maxHeightDiff);
	}

	return canGrab;
}

bool CPlayerPlugin_Interaction::HasObjectGrabHelper( EntityId entityId ) const
{
	static IEntityClass* pPickAndThrowWeaponClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("PickAndThrowWeapon");

	IInventory* pInventory = m_ownerPlayer->GetInventory();
	if (pInventory)
	{
		EntityId pickAndThrowWeaponId = pInventory->GetItemByClass( pPickAndThrowWeaponClass );
		IItem* pItem = m_pItemSystem->GetItem( pickAndThrowWeaponId );
		if (pItem)
		{
			CPickAndThrowWeapon* pPickAndThrowWeapon = static_cast<CPickAndThrowWeapon*>(pItem);
			return pPickAndThrowWeapon->HasObjectGrabHelper(entityId);
		}
	}

	return false;
}

void CPlayerPlugin_Interaction::SetLookAtTargetId( EntityId targetId )
{
  //Lock interactor to prevent interaction with other entities at the same time
  SInteractionLookAt& lookAtInfo = m_interactionInfo.lookAtInfo;
  lookAtInfo.lookAtTargetId = targetId;
  if ( !targetId ) {
    DisableForceLookAt();
  }
}

void CPlayerPlugin_Interaction::SetLookAtInterpolationTime( float interpolationTime )
{
	m_interactionInfo.lookAtInfo.interpolationTime = interpolationTime;
}

void CPlayerPlugin_Interaction::EnableForceLookAt()
{
  SInteractionLookAt& lookAtInfo = m_interactionInfo.lookAtInfo;
  lookAtInfo.forceLookAt = true;
}

void CPlayerPlugin_Interaction::DisableForceLookAt()
{
  SInteractionLookAt& lookAtInfo = m_interactionInfo.lookAtInfo;
  lookAtInfo.forceLookAt = false;
}

void CPlayerPlugin_Interaction::Reset()
{
	m_interactionInfo.Reset();
	m_lastInventoryFull = false;
	
	if(m_previousIgnoreEntityId)
	{
		CPlayerVisTable* pVisTable = g_pGame->GetPlayerVisTable();
		pVisTable->RemoveGlobalIgnoreEntity(m_previousIgnoreEntityId); 
	}
	m_previousIgnoreEntityId = 0; 
}

bool CPlayerPlugin_Interaction::CanLocalPlayerSee( IEntity *pEntity ) const
{
	CRY_ASSERT(pEntity);

	EntityId targetEntityId = pEntity->GetId();

	//If already looking at this object, skip visibility test
	//It will also help with some weird cases where visibility check fails because the object is partially obstructed
	if (targetEntityId == m_currentlyLookingAtEntityId)
	{
		return true;
	}

	CPlayerVisTable::SVisibilityParams queryTargetParams(targetEntityId);
	queryTargetParams.heightOffset = 0.05f;
	queryTargetParams.queryParams = eVQP_UseCenterAsReference|eVQP_IgnoreSmoke;

	return (g_pGame->GetPlayerVisTable()->CanLocalPlayerSee(queryTargetParams, 10));
}

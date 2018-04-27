// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Player.h"
#include "PlayerEntityInteraction.h"
#include "IInteractor.h"
#include "GameActions.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "PlayerPlugin_Interaction.h"
#include "EntityUtility/EntityScriptCalls.h"
#include "Throw.h"


namespace
{


	void CallEntityScriptMethod(EntityId entityId, const char* methodName, CPlayer* pPlayerArgument, int slotArgument)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);

		if (!pEntity)
			return;

		IScriptTable* pEntityTable = pEntity->GetScriptTable();
		IScriptTable* playerScript = pPlayerArgument->GetEntity()->GetScriptTable();

		EntityScripts::CallScriptFunction(
			pEntity, pEntityTable, methodName,
			playerScript, slotArgument);

	}



	bool PlayerCanInteract(CPlayer* pPlayer)
	{
		const SPlayerStats* pPlayerStats = static_cast<const SPlayerStats*>(pPlayer->GetActorStats());
		CRY_ASSERT(pPlayerStats);

		// During Stealth kill.
		if (pPlayer->GetStealthKill().IsBusy())
			return false;

		// Whilst throwing things. Causes issues.
		if(IItem* pItem = pPlayer->GetCurrentItem())
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

		// On a ledge or sliding.
		if (pPlayer->IsOnLedge() || pPlayer->IsSliding())
			return false;

		if (!pPlayer->IsInteractiveActionDone())
			return false;

		// Must be alive.
		return pPlayer->GetHealth() > 0 && pPlayer->GetSpectatorMode() == 0;
	}



	bool PlayerCanStopUseHeavyWeapon(CPlayer* pPlayer)
	{
		 if (!pPlayer->HasHeavyWeaponEquipped())
			 return false;
		 return pPlayer->GetCurrentInteractionInfo().interactionType != eInteraction_Use;
	}


}



CPlayerEntityInteraction::CPlayerEntityInteraction()
	:	m_useHoldFiredAlready(false)
	,	m_usePressFiredForUse(true)
	,	m_usePressFiredForPickup(false)
	,	m_useButtonPressed(false)
	,	m_autoPickupDeactivatedTime(0.f)
{
}



void CPlayerEntityInteraction::UseEntityUnderPlayer(CPlayer* pPlayer)
{
	if (PlayerCanInteract(pPlayer))
	{
		IInteractor* pInteractor = pPlayer->GetInteractor();
		const EntityId entityId = pInteractor->GetOverEntityId();

		if(IEntity* pLinkedEnt = pPlayer->GetLinkedEntity())
		{
			if(pLinkedEnt->GetId()==entityId)
			{
				// Can't use an entity you are already linked to.
				return;
			}
		}



		const int frameId = gEnv->pRenderer->GetFrameID();
		if (m_lastUsedEntity.CanInteractThisFrame( frameId ))
		{
				CallEntityScriptMethod(
					entityId, "OnUsed",
					pPlayer, pInteractor->GetOverSlotIdx());

				m_lastUsedEntity.Update( frameId );
		}
	}
}



void CPlayerEntityInteraction::ItemPickUpMechanic(CPlayer* pPlayer, const ActionId& actionId, int activationMode)
{
	const CGameActions& actions = g_pGame->Actions();

	if (actionId == actions.preUse)
	{
		if (activationMode == eAAM_OnPress)
		{
			// Interactor HUD needs to know when to start tracking a Use press
			// preUse is used instead of Use as adding an onPress to Use will need too many changes to its handling. preUse is the input as Use.
			SHUDEventWrapper::OnInteractionUseHoldTrack(true);
		}

		return;
	}

	if (actionId == actions.use)
	{
		if (activationMode == eAAM_OnPress)
		{
			m_useButtonPressed = true;
		}
		else if(activationMode == eAAM_OnRelease)
		{
			m_useButtonPressed = false;
		}
	}

	const bool isOnlyPickupAction = (actionId == actions.itemPickup);
	const bool isOnlyUseAction = (actionId == actions.use);
	const bool isOnlyHeavyWeaponRemove = (actionId == actions.heavyweaponremove);
	const bool isUseAction = (isOnlyUseAction || isOnlyPickupAction);

	if (isOnlyHeavyWeaponRemove && activationMode == eAAM_OnPress && PlayerCanStopUseHeavyWeapon(pPlayer))
	{
		ReleaseHeavyWeapon(pPlayer);
		m_useHoldFiredAlready = true;
	}
	else if (isUseAction && activationMode == eAAM_OnPress)
	{
		// This will happen twice on keyboard since Use and itemPickup use same input
		// To avoid refactoring code right now, only allow self.UseEntity to be called once per input frame
		// But make sure this functions properly even if use and itemPickup inputs are changed
		bool fireUseEntity = false;

		if (isOnlyPickupAction)
		{
			if (m_usePressFiredForUse)				// 2nd call this frame, don't call
			{
				m_usePressFiredForUse = false;		// Reset
				m_usePressFiredForPickup = false;	// Reset
			}
			else
			{
				fireUseEntity = true;
				m_usePressFiredForPickup = true;
			}
		}
		else if (isOnlyUseAction)
		{
			if (m_usePressFiredForPickup)			// 2nd call this frame, don't call
			{
				m_usePressFiredForPickup = false;	// Reset
				m_usePressFiredForUse = false;		// Reset
			}
			else
			{
				fireUseEntity = true;
				m_usePressFiredForUse = true;
			}
		}

		if (fireUseEntity)
		{
			// Log("[tlh] @ Player:OnAction: action: "..action.." press path");
			if (PlayerCanStopUseHeavyWeapon(pPlayer))
				ReleaseHeavyWeapon(pPlayer);
			else
				UseEntityUnderPlayer(pPlayer);
		}
	}
	else if (isUseAction && activationMode == eAAM_OnHold && !m_useHoldFiredAlready)
	{
		bool bFired = false;
		IInteractor* pInteractor = pPlayer->GetInteractor();
		if (pInteractor->GetOverEntityId() != 0)
		{
			m_useHoldFiredAlready = true;
			UseEntityUnderPlayer(pPlayer);
			bFired = true;
		}

		SHUDEventWrapper::OnInteractionUseHoldActivated(bFired);
	}
	else if (isUseAction && activationMode == eAAM_OnRelease)
	{
		m_useHoldFiredAlready = false;

		SHUDEventWrapper::OnInteractionUseHoldTrack(false);
	}
}




void CPlayerEntityInteraction::ReleaseHeavyWeapon(CPlayer* pPlayer)
{
	IItem* pCurrentItem = pPlayer->GetCurrentItem();
	if (pCurrentItem && PlayerCanInteract(pPlayer))
	{
		EntityId heavyWeaponEntity = pCurrentItem->GetEntityId();
		CallEntityScriptMethod(
			heavyWeaponEntity, "OnUsed",
			pPlayer, 0);
	}
}

void CPlayerEntityInteraction::JustInteracted( )
{
	m_lastUsedEntity.Update( gEnv->pRenderer->GetFrameID() );
}

void CPlayerEntityInteraction::Update(CPlayer* pPlayer, float frameTime)
{
	if(m_useButtonPressed && m_autoPickupDeactivatedTime <= 0.f)
	{
		IInteractor* pInteractor = pPlayer->GetInteractor();
		if (pInteractor->GetOverEntityId() != 0)
		{
			EInteractionType interactionType = pPlayer->GetCurrentInteractionInfo().interactionType;
			if(interactionType == eInteraction_PickupItem || interactionType == eInteraction_ExchangeItem)
			{
				UseEntityUnderPlayer(pPlayer);
				m_useButtonPressed = false;
				m_autoPickupDeactivatedTime = g_pGameCVars->pl_autoPickupMinTimeBetweenPickups;
			}
		}
	}
	else if(m_autoPickupDeactivatedTime > 0.f)
	{
		m_autoPickupDeactivatedTime -= frameTime;
	}
}

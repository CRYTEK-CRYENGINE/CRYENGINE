// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Carry entity, used for CTFFlag
  
 -------------------------------------------------------------------------
  History:
  - 03:06:2011: Created by Colin Gulliver

*************************************************************************/

#include "StdAfx.h"
#include "CarryEntity.h"

#include "Player.h"
#include "Utility/AttachmentUtils.h"

#define CARRYENTITY_ATTACHMENT_NAME					"left_weapon"
#define CARRYENTITY_CHARACTER_SLOT					0
#define CARRYENTITY_SHADOWCHARACTER_SLOT		5

//------------------------------------------------------------------------
CCarryEntity::CCarryEntity() : 
	m_spawnedWeaponId(0), 
	m_attachedActorId(0), 
	m_previousWeaponId(0), 
	m_cachedLastItemId(0),
	m_playerTagCRC(0),
	m_bSpawnedWeaponAttached(false)
{
	m_oldAttachmentRelTrans.SetIdentity();
}

//------------------------------------------------------------------------
bool CCarryEntity::Init( IGameObject *pGameObject )
{
	bool bSuccess = CNetworkedPhysicsEntity::Init(pGameObject);

	SmartScriptTable pScriptTable = GetEntity()->GetScriptTable();
	if (pScriptTable)
	{
		SmartScriptTable pProperties;
		if (pScriptTable->GetValue("Properties", pProperties))
		{
			const char *pSuffix = NULL;
			if (pProperties->GetValue("ActionSuffix", pSuffix))
			{
				m_actionSuffix = pSuffix;
			}
			if (pProperties->GetValue("ActionSuffixAG", pSuffix))
			{
				m_actionSuffixAG = pSuffix;
			}

			const char* tagName = NULL;
			if(pProperties->GetValue("PlayerTag", tagName))
			{
				m_playerTagCRC = CCrc32::ComputeLowercase(tagName);
			}
		}
	}

	return bSuccess;
}

//------------------------------------------------------------------------
void CCarryEntity::GetMemoryUsage( ICrySizer *pSizer ) const
{
	pSizer->Add(*this);
}

//------------------------------------------------------------------------
void CCarryEntity::SetSpawnedWeaponId( EntityId weaponId )
{
	m_spawnedWeaponId = weaponId;
}

//------------------------------------------------------------------------
void CCarryEntity::AttachTo( EntityId actorId )
{
	if (m_attachedActorId)
	{
		Detach();
	}

	if (actorId)
	{
		Attach(actorId);
	}
}

//------------------------------------------------------------------------
void CCarryEntity::Attach( EntityId actorId )
{
	CPlayer *pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(actorId));
	if (pPlayer)
	{
		const bool bIsOwnerFP = !pPlayer->IsThirdPerson(); 
		IEntity *pPlayerEntity = pPlayer->GetEntity();
				
		IInventory *pInventory = pPlayer->GetInventory();
		m_previousWeaponId = pInventory->GetCurrentItem();
		if (m_previousWeaponId == m_spawnedWeaponId)
		{
			// Current item is the spawned weapon - this won't be happy so get the previous item instead
			m_previousWeaponId = pInventory->GetLastItem();
			if (m_previousWeaponId == m_spawnedWeaponId)
			{
				// Item is still the spawned weapon, can't have this so default to 0
				m_previousWeaponId = 0;
			}
		}

		// Get the secondary weapon id - if the player doesn't have one then use the spawned weapon
		EntityId secondaryWeaponId = pPlayer->SimpleFindItemIdInCategory("secondary");
		bool bIsSpawnedWeapon = false;
		if (secondaryWeaponId == 0)
		{
			secondaryWeaponId = m_spawnedWeaponId;
			bIsSpawnedWeapon = true;
		}

		bool bDoAttach = true; 
		// If swimming attach here - DT#17176 Picking up relay/tick while swimming was reverting to main weapon instead of primary when leaving water, and not hiding attached item.
		if (pPlayer->IsSwimming())
		{
			AttachmentUtils::AttachObject( bIsOwnerFP, pPlayerEntity, GetEntity(), CARRYENTITY_ATTACHMENT_NAME );
			bDoAttach = false; 
		}
		
		EntityId holsteredId = pInventory->GetHolsteredItem();
		if (holsteredId)
		{
			if (pPlayer->IsClient())
			{
				pPlayer->HideLeftHandObject(true);
			}
			m_cachedLastItemId = holsteredId;
			pInventory->SetHolsteredItem(secondaryWeaponId);

			if (gEnv->bServer && bIsSpawnedWeapon)
			{
				pPlayer->PickUpItem(secondaryWeaponId, false, false);
			}
		}
		else
		{
			pPlayer->CancelScheduledSwitch();
			if (!bIsSpawnedWeapon)
			{
				pPlayer->SelectItem(secondaryWeaponId, true, true);
			}
			else if (gEnv->bServer)
			{
				pPlayer->PickUpItem(secondaryWeaponId, false, true);
			}
		}
		

		m_bSpawnedWeaponAttached = bIsSpawnedWeapon;

		pPlayer->EnableSwitchingItems(false);
		pPlayer->EnableIronSights(false);
		pPlayer->EnablePickingUpItems(false);

		pPlayer->SetTagByCRC( m_playerTagCRC, true );

		if(bDoAttach)
		{
			// If not swimming, attach here !B DT10978 Fix for issues with left handed weapons and picking up relays/extraction ticks. We now start the weapon switch prior to attaching the objective so that the left handed weapon doesn't clear it from the left hand
			AttachmentUtils::AttachObject( bIsOwnerFP, pPlayerEntity, GetEntity(), CARRYENTITY_ATTACHMENT_NAME );
			IAttachment* pAttachment = pPlayerEntity->GetCharacter(CARRYENTITY_CHARACTER_SLOT)->GetIAttachmentManager()->GetInterfaceByName(CARRYENTITY_ATTACHMENT_NAME);
			if(pAttachment)
			{
				Vec3 helperPos = GetEntity()->GetStatObj(0)->GetHelperPos("player_grab");
				m_oldAttachmentRelTrans = pAttachment->GetAttRelativeDefault();
				Matrix34 m;
				m.SetIdentity();
				m.SetTranslation(helperPos);
				m.InvertFast();

				// main character
				pAttachment->SetAttRelativeDefault(m_oldAttachmentRelTrans * QuatT(m.GetTranslation(), Quat(m).GetNormalized()));

				// shadow character
				ICharacterInstance* pShadowCharInst = pPlayerEntity->GetCharacter(CARRYENTITY_SHADOWCHARACTER_SLOT);
				IAttachment* pShadowAttachment =  pShadowCharInst ? pShadowCharInst->GetIAttachmentManager()->GetInterfaceByName(CARRYENTITY_ATTACHMENT_NAME) : NULL;
				if(pShadowAttachment)
				{
					pShadowAttachment->SetAttRelativeDefault(m_oldAttachmentRelTrans * QuatT(m.GetTranslation(), Quat(m).GetNormalized()));
				}
			}
		}
	}

	m_attachedActorId = actorId;
}

//------------------------------------------------------------------------
void CCarryEntity::Detach()
{
	CPlayer *pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_attachedActorId));
	if (pPlayer)
	{
		IEntity *pPlayerEntity = pPlayer->GetEntity();
		IEntity *pEntity = GetEntity();

		pPlayer->SetTagByCRC( m_playerTagCRC, false );

		AttachmentUtils::DetachObject( pPlayerEntity, GetEntity(), CARRYENTITY_ATTACHMENT_NAME );

		GetEntity()->DetachThis(IEntity::ATTACHMENT_KEEP_TRANSFORMATION);
		 
		pEntity->SetRotation(Quat::CreateRotationXYZ(Ang3(0.f, 0.f, 0.f)));

		EntityId itemIdToUse = m_cachedLastItemId;
		if (!itemIdToUse)
		{
			itemIdToUse = m_previousWeaponId;
		}

		IInventory *pInventory = pPlayer->GetInventory();
		if (pPlayer->IsSwimming())
		{
			pInventory->SetLastItem(itemIdToUse);
		}
		else
		{
			if (itemIdToUse)
			{
				EntityId holsteredId = pInventory->GetHolsteredItem();
				if (holsteredId)
				{
					pInventory->SetHolsteredItem(itemIdToUse);
				}
				else
				{
					pPlayer->SelectItem(itemIdToUse, false, true);
				}
			}
		}
	
		if (m_bSpawnedWeaponAttached)
		{
			pInventory->RemoveItem(m_spawnedWeaponId);
			IEntity *pSpawnedEntity = gEnv->pEntitySystem->GetEntity(m_spawnedWeaponId);
			if (pSpawnedEntity)
			{
				pSpawnedEntity->SetPos(Vec3(0.f, 0.f, 0.f));
			}
		}

		// revert main character attachment offset
		IAttachment* pAttachment = pPlayerEntity->GetCharacter(CARRYENTITY_CHARACTER_SLOT)->GetIAttachmentManager()->GetInterfaceByName(CARRYENTITY_ATTACHMENT_NAME);
		if(pAttachment)
		{
			pAttachment->SetAttRelativeDefault(m_oldAttachmentRelTrans);
		}

		// revert shadow character attachment offset
		ICharacterInstance* pShadowCharInst = pPlayerEntity->GetCharacter(CARRYENTITY_SHADOWCHARACTER_SLOT);
		IAttachment* pShadowAttachment =  pShadowCharInst ? pShadowCharInst->GetIAttachmentManager()->GetInterfaceByName(CARRYENTITY_ATTACHMENT_NAME) : NULL;
		if(pShadowAttachment)
		{
			pShadowAttachment->SetAttRelativeDefault(m_oldAttachmentRelTrans);
		}

		pPlayer->EnableSwitchingItems(true);
		pPlayer->EnableIronSights(true);
		pPlayer->EnablePickingUpItems(true);
	}

	m_attachedActorId = 0;
	m_previousWeaponId = 0;
	m_cachedLastItemId = 0;
	m_bSpawnedWeaponAttached = false;
}

void CCarryEntity::PostInit( IGameObject *pGameObject )
{
	GetGameObject()->EnableUpdateSlot(this, 0);
	GetGameObject()->SetUpdateSlotEnableCondition(this, 0, eUEC_Always);
}

#undef CARRYENTITY_ATTACHMENT_NAME
#undef CARRYENTITY_CHARACTER_SLOT


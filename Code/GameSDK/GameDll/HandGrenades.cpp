// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryAnimation/ICryAnimation.h>
#include "Actor.h"

#include "HandGrenades.h"

#include "Game.h"
#include "GameActions.h"
#include "Throw.h"
#include "LTagSingle.h"

//-------------------------------------------------
CHandGrenades::CHandGrenades()
	: m_pThrow(nullptr)
	, m_numStowedCopies(0)
	, m_stowSlot(-1)
	, m_quickThrowRequested(false)
	, m_bInQuickThrow(false)
	, m_throwCancelled(false)
{
}

//-------------------------------------------------
CHandGrenades::~CHandGrenades()
{
}

//-------------------------------------------------
void CHandGrenades::InitFireModes()
{
	inherited::InitFireModes();

	int firemodeCount = m_firemodes.size();

	m_pThrow = NULL;
	int throwId = -1;

	for(int i = 0; i < firemodeCount; i++)
	{
		if (crygti_isof<CThrow>(m_firemodes[i]))
		{
			CRY_ASSERT_MESSAGE(!m_pThrow, "Multiple Throw firemodes assigned to weapon");
			m_pThrow = crygti_cast<CThrow*>(m_firemodes[i]);
			throwId = i;
		}
	}

	CRY_ASSERT_MESSAGE(m_pThrow, "No Throw firemode assigned to weapon");

	if(m_pThrow)
	{
		SetCurrentFireMode(throwId);
	}
}

//-------------------------------------------------
bool CHandGrenades::OnActionAttack(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	m_throwCancelled &= (activationMode & (eAAM_OnRelease | eAAM_OnPress)) == 0; //New throw request so disable the cancellation

	if (!CanOwnerThrowGrenade() || m_throwCancelled)
		return true;

	if(activationMode == eAAM_OnPress || (gEnv->bMultiplayer && activationMode == eAAM_OnHold))
	{
		if(m_pThrow && !m_pThrow->IsFiring())
		{
			CRY_ASSERT_MESSAGE(m_pThrow == m_fm, "Currently not in throw firemode");
			m_pThrow->Prime();
			UpdateStowedWeapons();
		}
	}
	else if (activationMode == eAAM_OnRelease)
	{
		StopFire();
	}

	return true;
}

//-------------------------------------------------
bool CHandGrenades::CanSelect() const
{
	return (inherited::CanSelect() && !OutOfAmmo(false));
}

//-------------------------------------------------
bool CHandGrenades::CanDeselect() const
{
	if (gEnv->bMultiplayer)
	{
		return (!m_fm || !m_fm->IsFiring());
	}
	else
	{
		return inherited::CanDeselect();
	}
}

void CHandGrenades::StartSprint(CActor* pOwnerActor)
{
	//don't stop firing grenades
}

bool CHandGrenades::ShouldSendOnShootHUDEvent() const
{
	return false;
}

void CHandGrenades::OnPickedUp(EntityId actorId, bool destroyed)
{
	inherited::OnPickedUp(actorId, destroyed);
	UpdateStowedWeapons();
}

void CHandGrenades::OnDropped(EntityId actorId, bool ownerWasAI)
{
	inherited::OnDropped(actorId, ownerWasAI);
	UpdateStowedWeapons();
}

void CHandGrenades::OnSetAmmoCount(EntityId shooterId)
{
	inherited::OnSetAmmoCount(shooterId);
	UpdateStowedWeapons();
}

void CHandGrenades::OnSelected(bool selected)
{
	inherited::OnSelected(selected);
	UpdateStowedWeapons();
}

int PickStowSlot(IAttachmentManager *pAttachmentManager, const char *slot1, const char *slot2)
{
	int ret = -1;
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(slot1);
	if(pAttachment && !pAttachment->GetIAttachmentObject())
	{
		ret = 0;
	}
	else
	{
		pAttachment = pAttachmentManager->GetInterfaceByName(slot2);
		if(pAttachment && !pAttachment->GetIAttachmentObject())
		{
			ret = 1;
		}
	}

	return ret;
}

void CHandGrenades::UpdateStowedWeapons()
{
	CActor *pOwnerActor = GetOwnerActor();
	if (!pOwnerActor)
		return;

	ICharacterInstance *pOwnerCharacter = pOwnerActor->GetEntity()->GetCharacter(0);
	if (!pOwnerCharacter)
		return;

	IStatObj *pTPObj = GetEntity()->GetStatObj(eIGS_ThirdPerson);
	if (!pTPObj)
		return;


	int ammoCount = m_fm ? pOwnerActor->GetInventory()->GetAmmoCount(m_fm->GetAmmoType()) : 0;
	if (IsSelected() && (ammoCount > 0))
	{
		ammoCount--;
	}
	if (!pOwnerActor->IsThirdPerson())
	{
		ammoCount = 0;
	}

	int numGrenDiff = ammoCount - m_numStowedCopies;
	if(numGrenDiff != 0)
	{
		if (m_stowSlot < 0)
		{
			m_stowSlot = PickStowSlot(pOwnerCharacter->GetIAttachmentManager(), m_sharedparams->params.bone_attachment_01.c_str(), m_sharedparams->params.bone_attachment_02.c_str());
		}

		if (m_stowSlot >= 0)
		{
			bool attach = numGrenDiff > 0;
			int tot = abs(numGrenDiff);

			IAttachmentManager *pAttachmentManager = pOwnerCharacter->GetIAttachmentManager();
			IAttachment *pAttachment = NULL;

			for (int i=0; i<tot; i++)
			{
				//--- Generate the secondary slot from the first by adding one to the attachment name, is all we need at present...
				const char *attach1 = (m_stowSlot == 0) ? m_sharedparams->params.bone_attachment_01.c_str() : m_sharedparams->params.bone_attachment_02.c_str();
				int lenAttachName = strlen(attach1);
				stack_string attach2(attach1, lenAttachName-1);
				attach2 += (attach1[lenAttachName-1]+1);

				if (attach)
				{
					pAttachment = pAttachmentManager->GetInterfaceByName(attach1);
					if(pAttachment && pAttachment->GetIAttachmentObject())
					{
						pAttachment = pAttachmentManager->GetInterfaceByName(attach2);
					}

					if (pAttachment && !pAttachment->GetIAttachmentObject())
					{
						CCGFAttachment *pCGFAttachment = new CCGFAttachment();
						pCGFAttachment->pObj = pTPObj;
						pAttachment->AddBinding(pCGFAttachment);
						pAttachment->HideAttachment(0);
						pAttachment->HideInShadow(0);

						m_numStowedCopies++;
					}
				}
				else
				{
					pAttachment = pAttachmentManager->GetInterfaceByName(attach2);
					if(!pAttachment || !pAttachment->GetIAttachmentObject())
					{
						pAttachment = pAttachmentManager->GetInterfaceByName(attach1);
					}

					if (pAttachment && pAttachment->GetIAttachmentObject())
					{
						pAttachment->ClearBinding();
						m_numStowedCopies--;					
					}
				}
			}
		}
	}
}

bool CHandGrenades::CanOwnerThrowGrenade() const
{
	CActor* pOwnerActor = GetOwnerActor();

	return pOwnerActor ? pOwnerActor->CanFire() : true;
}

void CHandGrenades::FumbleGrenade()
{
	if(m_pThrow)
	{
		CRY_ASSERT_MESSAGE(m_pThrow == m_fm, "Currently not in throw firemode");
		m_pThrow->FumbleGrenade();
	}
}

bool CHandGrenades::AllowInteraction(EntityId interactionEntity, EInteractionType interactionType)
{
	return (interactionType == eInteraction_PickupAmmo) || (inherited::AllowInteraction(interactionEntity, interactionType) && !(m_fm && m_fm->IsFiring()));
}

void CHandGrenades::StartQuickGrenadeThrow()
{
	if (!CanOwnerThrowGrenade())
		return;

	m_quickThrowRequested = true;
	m_bInQuickThrow = true;

	if (m_pThrow && !m_pThrow->IsFiring())
	{
		CRY_ASSERT_MESSAGE(m_pThrow == m_fm, "Currently not in throw firemode");
		m_pThrow->Prime();
		UpdateStowedWeapons();

		if (m_pThrow->IsReadyToThrow())
		{
			m_quickThrowRequested = false;
		}
	}
}

void CHandGrenades::StopQuickGrenadeThrow()
{
	if (m_bInQuickThrow)
	{
		m_bInQuickThrow = false;
		StopFire();
	}
}

void CHandGrenades::ForcePendingActions( uint8 blockedActions /*= 0*/ )
{
	inherited::ForcePendingActions(blockedActions);

	if (m_quickThrowRequested)
	{
		bool bDoCompleteThrow = !m_bInQuickThrow;

		StartQuickGrenadeThrow();

		if (bDoCompleteThrow)
		{
			StopQuickGrenadeThrow();
		}
	}
}

bool CHandGrenades::CancelCharge()
{
	if(gEnv->bMultiplayer)
	{
		m_throwCancelled = true;
		return true;
	}

	return false;
}

uint32 CHandGrenades::StartDeselection(bool fastDeselect)
{
	if (m_zm)
		m_zm->StopZoom();

	return CWeapon::StartDeselection(fastDeselect);
}

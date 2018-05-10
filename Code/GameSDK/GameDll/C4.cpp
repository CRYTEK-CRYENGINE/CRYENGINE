// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 2:3:2005   16:06 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "C4.h"
#include "Plant.h"

#include "Game.h"
#include "Actor.h"
#include "WeaponSystem.h"
#include "GameActions.h"
#include "Detonate.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "C4Projectile.h"

//------------------------------------------------------------------------
CC4::CC4() : m_pDetonatorArmedMaterial(NULL), m_isArmed(false), m_postSerializing(false)
{
	m_detonateSwitch = false;
	m_detonateFM = -1;
	m_plantFM = -1;
}

//------------------------------------------------------------------------
CC4::~CC4()
{
	if (m_pDetonatorArmedMaterial)
	{
		m_pDetonatorArmedMaterial->Release();
	}
};

//------------------------------------------------------------------------
void CC4::InitFireModes()
{
	BaseClass::InitFireModes();

	int firemodeCount = m_firemodes.size();

	m_plantFM = -1;
	m_detonateFM = -1;

	for (int i = 0; i < firemodeCount; i++)
	{
		if (crygti_isof<CPlant>(m_firemodes[i]))
		{
			CRY_ASSERT_MESSAGE((m_plantFM == -1), "Multiple Plant firemodes assigned to weapon");

			m_plantFM = i;
		}
		else if (crygti_isof<CDetonate>(m_firemodes[i]))
		{
			CRY_ASSERT_MESSAGE((m_detonateFM == -1), "Multiple Detonate firemodes assigned to weapon");

			m_detonateFM = i;
		}
	}

	CRY_ASSERT_MESSAGE(m_detonateFM >= 0, "No Detonate firemode assigned to weapon");
	CRY_ASSERT_MESSAGE(m_plantFM >= 0, "No Plant firemode assigned to weapon");

	SetCurrentFireMode(m_plantFM);
}

//------------------------------------------------------------------------
bool CC4::OnActionZoom(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return OnActionAttackSecondary(actorId, actionId, activationMode, value);
}

//------------------------------------------------------------------------
bool CC4::OnActionAttackSecondary(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress && !IsDeselecting())
	{
		IFireMode* pPlantFM = GetFireMode(m_plantFM);
		EntityId projectileId = pPlantFM->GetProjectileId();
		CProjectile* pProjectile = g_pGame->GetWeaponSystem()->GetProjectile(projectileId);
		//Clients will never have a valid projectile here - they need to request as usual
		if (gEnv->bServer && (!pProjectile || !pProjectile->CanDetonate()))
		{
			return false;
		}

		IFireMode* pFM = GetFireMode(m_detonateFM);
		CCCPOINT(C4_ReceiveDetonateInput);
		pFM->StartFire();

		return true;
	}

	return false;
}

//------------------------------------------------------------------------
void CC4::Update(SEntityUpdateContext& ctx, int update)
{
	if (update == eIUS_FireMode)
	{
		IFireMode* pFM = GetFireMode(m_detonateFM);
		pFM->Update(ctx.fFrameTime, ctx.frameID);
	}

	//Update visuals on server
	if (gEnv->bServer && m_pDetonatorArmedMaterial)
	{
		IFireMode* pPlantFM = GetFireMode(m_plantFM);
		EntityId projectileId = pPlantFM->GetProjectileId();
		CProjectile* pProjectile = projectileId ? g_pGame->GetWeaponSystem()->GetProjectile(projectileId) : NULL;
		bool armed = false;
		if (pProjectile)
		{
			armed = pProjectile->CanDetonate();
		}

		if (armed != m_isArmed)
		{
			m_isArmed = armed;
			if (m_isArmed)
			{
				SetDiffuseAndGlow(m_weaponsharedparams->pC4Params->armedLightColour, m_weaponsharedparams->pC4Params->armedLightGlowAmount);
			}
			else
			{
				SetDiffuseAndGlow(m_weaponsharedparams->pC4Params->disarmedLightColour, m_weaponsharedparams->pC4Params->disarmedLightGlowAmount);
			}
			CHANGED_NETWORK_STATE(this, ASPECT_DETONATE);
		}
	}

	BaseClass::Update(ctx, update);
}

//------------------------------------------------------------------------
bool CC4::CanSelect() const
{
	bool canSelect = (BaseClass::CanSelect() && !OutOfAmmo(false));

	//Check for remaining projectiles to detonate
	if (!canSelect)
	{
		if (gEnv->bServer)
		{
			IFireMode* pFM = GetFireMode(GetCurrentFireMode());
			if (pFM)
			{
				EntityId projectileId = pFM->GetProjectileId();
				if (projectileId && g_pGame->GetWeaponSystem()->GetProjectile(projectileId))
				{
					canSelect = true;
				}
			}
		}
		else
		{
			CDetonate* pDetFM = static_cast<CDetonate*>(GetFireMode(m_detonateFM));
			if (pDetFM)
			{
				canSelect = pDetFM->ClientCanDetonate();
			}
		}
	}

	return canSelect;
};

//---------------------------------------------------------------------------------
bool CC4::OnActionFiremode(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return true;
}

//---------------------------------------------------------------------------------
bool CC4::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	if (aspect == ASPECT_DETONATE)
	{
		if (gEnv->bServer && ser.IsWriting())
		{
			//As the C4 won't be updating when the server player is not the C4 owner we need to update the status of the projectile first
			EntityId projectileId = m_fm->GetProjectileId();
			if (projectileId)
			{
				if (CProjectile* pProjectile = g_pGame->GetWeaponSystem()->GetProjectile(projectileId))
				{
					m_isArmed = pProjectile->CanDetonate();
				}
			}
		}

		ser.Value("canDet", static_cast<CC4*>(this), &CC4::NetGetCanDetonate, &CC4::NetSetCanDetonate, 'bool');
		ser.Value("detSwitch", static_cast<CC4*>(this), &CC4::NetGetDetonateSwitch, &CC4::NetSetDetonateSwitch, 'bool');

		if (ser.IsReading() && m_pDetonatorArmedMaterial)
		{
			bool isNowArmed = false;
			CDetonate* pFM = static_cast<CDetonate*>(GetFireMode(m_detonateFM));
			if (pFM)
			{
				isNowArmed = pFM->ClientCanDetonate();
			}

			if (isNowArmed != m_isArmed)
			{
				m_isArmed = isNowArmed;
				if (isNowArmed)
				{
					SetDiffuseAndGlow(m_weaponsharedparams->pC4Params->armedLightColour, m_weaponsharedparams->pC4Params->armedLightGlowAmount);
				}
				else
				{
					SetDiffuseAndGlow(m_weaponsharedparams->pC4Params->disarmedLightColour, m_weaponsharedparams->pC4Params->disarmedLightGlowAmount);
				}
			}
		}
	}

	return BaseClass::NetSerialize(ser, aspect, profile, flags);
}

NetworkAspectType CC4::GetNetSerializeAspects()
{
	return BaseClass::GetNetSerializeAspects() | ASPECT_DETONATE;
}

//---------------------------------------------------------------------------------
void CC4::FullSerialize(TSerialize ser)
{
	CWeapon::FullSerialize(ser);

	ser.Value("m_plantFM", m_plantFM);
	ser.Value("m_detonateFM", m_detonateFM);
	ser.Value("m_detonateSwitch", m_detonateSwitch);
}

//---------------------------------------------------------------------------------
void CC4::PostSerialize()
{
	m_postSerializing = true;
	CWeapon::PostSerialize();
	m_postSerializing = false;
}

//---------------------------------------------------------------------------------
void CC4::NetSetDetonateSwitch(bool detonate)
{
	IActor* pActor = GetOwnerActor();
	if (pActor && !pActor->IsClient() && m_detonateSwitch != detonate)
	{
		IFireMode* pFM = GetFireMode(m_detonateFM);
		CCCPOINT(C4_ReceiveNetDetonateInput);
		pFM->StartFire();
	}

	m_detonateSwitch = detonate;
}

//---------------------------------------------------------------------------------
bool CC4::NetGetCanDetonate() const
{
	return (m_fm->GetProjectileId() != 0 && m_isArmed);
}

//---------------------------------------------------------------------------------
void CC4::NetSetCanDetonate(bool canDetonate)
{
	CDetonate* pFM = static_cast<CDetonate*>(GetFireMode(m_detonateFM));

	if (pFM)
	{
		pFM->SetCanDetonate(canDetonate);
	}
}

//---------------------------------------------------------------------------------
void CC4::RequestDetonate()
{
	if (!gEnv->bServer)
	{
		CCCPOINT(C4_ClientSendDetonateRequest);
		GetGameObject()->InvokeRMI(CC4::SvRequestDetonate(), DefaultParams(), eRMI_ToServer);
	}
}

//---------------------------------------------------------------------------------
IMPLEMENT_RMI(CC4, SvRequestDetonate)
{
	NetSetDetonateSwitch(!m_detonateSwitch);
	CHANGED_NETWORK_STATE(this, ASPECT_DETONATE);

	return true;
}

void CC4::OnEnterFirstPerson()
{
	CWeapon::OnEnterFirstPerson();

	if (!m_pDetonatorArmedMaterial && m_weaponsharedparams->pC4Params)
	{
		IMaterialManager* pMatManager = gEnv->p3DEngine->GetMaterialManager();
		IEntityClass* pClass = (m_accessories.empty() == false) ? m_accessories[0].pClass : NULL;

		const SAccessoryParams* params = GetAccessoryParams(pClass);
		if (params)
		{
			IMaterial* pOriginalMaterial = GetCharacterAttachmentMaterial(0, params->attach_helper.c_str(), params->attachToOwner); //left_weapon
			if (pOriginalMaterial)
			{
				int count = pOriginalMaterial->GetSubMtlCount();
				int armedIndex = m_weaponsharedparams->pC4Params->armedLightMatIndex;
				if (armedIndex >= 0 && armedIndex < count)
				{
					if (m_pDetonatorArmedMaterial = pOriginalMaterial->GetSubMtl(armedIndex))
					{
						m_pDetonatorArmedMaterial->AddRef();
					}
				}
			}
		}
	}

	if (m_pDetonatorArmedMaterial)
	{
		if (m_isArmed)
		{
			SetDiffuseAndGlow(m_weaponsharedparams->pC4Params->armedLightColour, m_weaponsharedparams->pC4Params->armedLightGlowAmount);
		}
		else
		{
			SetDiffuseAndGlow(m_weaponsharedparams->pC4Params->disarmedLightColour, m_weaponsharedparams->pC4Params->disarmedLightGlowAmount);
		}
	}
}

//---------------------------------------------------------------------------------
void CC4::PickUp(EntityId picker, bool sound, bool select, bool keepHistory, const char* setup)
{
	IEntityClass* pC4Ammo = GetProjectileClass();
	CPlant* pPlant = crygti_cast<CPlant*>(GetCFireMode(m_plantFM));
	int numProjectiles = pPlant ? pPlant->GetNumProjectiles() : 0;
	int capacity = SWeaponAmmoUtils::GetAmmoCount(GetWeaponSharedParams()->ammoParams.capacityAmmo, pC4Ammo);

	if (numProjectiles >= capacity)
		SetAmmoCount(pC4Ammo, 0);

	BaseClass::PickUp(picker, sound, select, keepHistory, setup);
}

//---------------------------------------------------------------------------------
void CC4::Drop(float impulseScale, bool selectNext, bool byDeath)
{
	CActor* pOwner = GetOwnerActor();
	IEntityClass* pC4Ammo = GetProjectileClass();
	int dropItems = GetInventoryAmmoCount(pC4Ammo) - 1;
	IInventory* pInventory = GetActorInventory(GetOwnerActor());
	if (pInventory)
		dropItems = min(dropItems, pInventory->GetAmmoCapacity(pC4Ammo));

	BaseClass::Drop(impulseScale, selectNext, byDeath);

	if (!gEnv->bMultiplayer && pOwner && pOwner->IsPlayer())
	{
		if (!m_postSerializing)
		{
			if (dropItems >= 0)
				ResetAmmo(pC4Ammo);
			for (int i = 0; i < dropItems; ++i)
				SpawnAndDropNewC4(pC4Ammo, impulseScale);
		}
		else
		{
			SetInventoryAmmoCount(pC4Ammo, dropItems);
		}
	}
}

void CC4::SetDiffuseAndGlow(const ColorF& newDiffuse, const float& newGlow)
{
	if (m_pDetonatorArmedMaterial)
	{
		const SShaderItem& armedShaderItem = m_pDetonatorArmedMaterial->GetShaderItem();
		if (armedShaderItem.m_pShaderResources && armedShaderItem.m_pShader)
		{
			armedShaderItem.m_pShaderResources->SetColorValue(EFTT_DIFFUSE, newDiffuse);
			armedShaderItem.m_pShaderResources->SetStrengthValue(EFTT_EMITTANCE, newGlow);

			armedShaderItem.m_pShaderResources->UpdateConstants(armedShaderItem.m_pShader);
		}
	}
}

IEntityClass* CC4::GetProjectileClass() const
{
	if (m_ammo.empty())
		return 0;
	return m_ammo[0].pAmmoClass;
}

void CC4::ResetAmmo(IEntityClass* pC4Ammo)
{
	SetAmmoCount(pC4Ammo, 1);
	SetBonusAmmoCount(pC4Ammo, 0);
	SetInventoryAmmoCount(pC4Ammo, 0);
}

void CC4::SpawnAndDropNewC4(IEntityClass* pC4Ammo, float impulseScale)
{
	SEntitySpawnParams newC4;
	newC4.pClass = GetEntity()->GetClass();
	newC4.vPosition = GetEntity()->GetWorldPos();
	newC4.qRotation = GetEntity()->GetWorldRotation();
	IEntity* pNewC4 = gEnv->pEntitySystem->SpawnEntity(newC4);
	CC4* pNewC4Item = static_cast<CC4*>(m_pItemSystem->GetItem(pNewC4->GetId()));

	pNewC4Item->Drop(impulseScale, false, false);
	pNewC4Item->ResetAmmo(pC4Ammo);
}

bool CC4::CanModify() const
{
	return !gEnv->bMultiplayer;
}

void CC4::OnUnlowerItem()
{
	if (CPlant* pPlant = crygti_cast<CPlant*>(GetCFireMode(GetCurrentFireMode())))
	{
		pPlant->CheckAmmo();
	}
}

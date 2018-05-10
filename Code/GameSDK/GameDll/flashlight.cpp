// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "flashlight.h"
#include "ItemSharedParams.h"
#include "GameActions.h"
#include "Weapon.h"
#include "Player.h"
#include "EntityUtility/EntityScriptCalls.h"

namespace
{
	CItem* GetLocalPlayerItem()
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(gEnv->pGameFramework->GetClientActor());

		if (!pPlayer)
		{
			return 0;
		}

		CItem* pCurrentItem = static_cast<CItem*>(pPlayer->GetCurrentItem());

		return pCurrentItem;
	}

	CFlashLight* GetPlayerFlashLight()
	{
		CItem* pItem = GetLocalPlayerItem();

		if (!pItem)
		{
			return 0;
		}

		static IEntityClass* pFlashLightClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Flashlight");

		IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
		IItemSystem* pItemSystem = pGameFramework->GetIItemSystem();

		for (size_t i = 0; i < pItem->GetAccessories().size(); ++i)
		{
			if (pItem->GetAccessories()[i].pClass == pFlashLightClass)
			{
				IItem* pAccessory = pItemSystem->GetItem(pItem->GetAccessories()[i].accessoryId);
				CFlashLight* pFlashLight = static_cast<CFlashLight*>(pAccessory);
				return pFlashLight;
			}
		}

		return 0;
	}
}

bool CFlashLight::m_flashLightEnabled = true;
bool CFlashLight::m_lightEnabled = true;

CFlashLight::CFlashLight()
	: m_fogVolume(0)
	, m_lightId(0)
{
}

CFlashLight::~CFlashLight()
{
	if (m_fogVolume)
	{
		gEnv->pEntitySystem->RemoveEntity(m_fogVolume);
	}
}

void CFlashLight::EnableLight(bool enable)
{
	CWeapon* pWeapon = GetWeapon();

	if (!pWeapon || !m_sharedparams->pFlashLightParams || (m_lightEnabled && enable) || (!m_lightEnabled && !enable))
	{
		return;
	}

	CActor* pOwner = pWeapon->GetOwnerActor();
	bool ownerIsFP = pWeapon->IsOwnerFP();
	int slot = ownerIsFP ? eIGS_FirstPerson : eIGS_ThirdPerson;

	if (enable)
	{
		const char* attachHelper = "light_term";
		const Vec3 direction = Vec3(-1.0f, 0.0f, 0.0f);

		IRenderNode* pCasterException = NULL;

		if (pOwner)
		{
			IEntityRender* pIEntityRender = (pOwner->GetEntity()->GetRenderInterface());


			{
				pCasterException = pIEntityRender->GetRenderNode();
			}
		}

		EntityEffects::SLightAttachParams lightParams;

		lightParams.pCasterException = pCasterException;
		lightParams.color = m_sharedparams->pFlashLightParams->color * m_sharedparams->pFlashLightParams->diffuseMult;
		lightParams.direction = direction;
		lightParams.radius = m_sharedparams->pFlashLightParams->distance;
		lightParams.specularMultiplier = m_sharedparams->pFlashLightParams->specularMult;
		lightParams.projectFov = m_sharedparams->pFlashLightParams->fov;
		lightParams.hdrDynamic = m_sharedparams->pFlashLightParams->HDRDynamic;
		lightParams.projectTexture = m_sharedparams->pFlashLightParams->lightCookie.c_str();
		lightParams.style = m_sharedparams->pFlashLightParams->style;
		lightParams.animSpeed = m_sharedparams->pFlashLightParams->animSpeed;
		lightParams.castShadows = g_pGameCVars->i_flashlight_has_shadows != 0;
		lightParams.firstSafeSlot = eIGS_Last;

		m_lightId = pWeapon->AttachLight(slot, attachHelper, lightParams);

		EnableFogVolume(pWeapon, slot, true);
	}
	else
	{
		if (m_lightId)
		{
			pWeapon->DetachEffect(m_lightId);
		}

		m_lightId = 0;
		EnableFogVolume(pWeapon, slot, false);
	}

	m_lightEnabled = enable;
}

void CFlashLight::EnableFogVolume(CWeapon* pWeapon, int slot, bool enable)
{
	if (!g_pGameCVars->i_flashlight_has_fog_volume)
	{
		return;
	}

	if (!m_sharedparams->pFlashLightParams)
	{
		return;
	}

	IEntity* pFogVolume = 0;

	if (m_fogVolume == 0)
	{
		const Vec3 size = Vec3(
			m_sharedparams->pFlashLightParams->fogVolumeRadius,
			m_sharedparams->pFlashLightParams->fogVolumeSize,
			m_sharedparams->pFlashLightParams->fogVolumeRadius);

		SEntitySpawnParams fogVolumeParams;
		fogVolumeParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("FogVolume");
		fogVolumeParams.nFlags = ENTITY_FLAG_NO_SAVE;
		fogVolumeParams.vPosition = Vec3(ZERO);

		pFogVolume = gEnv->pEntitySystem->SpawnEntity(fogVolumeParams);

		if (!pFogVolume)
		{
			return;
		}

		m_fogVolume = pFogVolume->GetId();

		SmartScriptTable pProperties;
		pFogVolume->GetScriptTable()->GetValue("Properties", pProperties);

		if (pProperties)
		{
			pProperties->SetValue("color_Color", m_sharedparams->pFlashLightParams->fogVolumeColor);
			pProperties->SetValue("GlobalDensity", m_sharedparams->pFlashLightParams->fogVolumeDensity);
			pProperties->SetValue("Size", size);
			pProperties->SetValue("FallOffScale", 0.0f);
		}

		EntityScripts::CallScriptFunction(pFogVolume, pFogVolume->GetScriptTable(), "OnPropertyChange");

		if (auto* pScriptComponent = pFogVolume->GetComponent<IEntityScriptComponent>())
		{
			pScriptComponent->EnableScriptUpdate(true);
		}
	}
	else
	{
		pFogVolume = gEnv->pEntitySystem->GetEntity(m_fogVolume);
	}

	if (!pFogVolume)
	{
		return;
	}

	const char* attachHelper = "lightFog_term";
	const float distance = m_sharedparams->pFlashLightParams->fogVolumeSize * 0.5f;
	ICharacterInstance* pCharacter = pWeapon->GetEntity()->GetCharacter(slot);

	if (enable && pCharacter)
	{
		IAttachmentManager* pAttachmentManager = pCharacter->GetIAttachmentManager();
		IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(attachHelper);

		if (pAttachment)
		{
			CEntityAttachment* pEntityAttachment = new CEntityAttachment();
			pEntityAttachment->SetEntityId(m_fogVolume);
			pAttachment->AddBinding(pEntityAttachment);
			QuatT relative(IDENTITY);
			relative.t.y = distance;
			pAttachment->SetAttRelativeDefault(relative);
		}
	}

	pFogVolume->Hide(!enable);
}

void CFlashLight::OnAttach(bool attach)
{
	EnableLight(attach && m_lightEnabled);
}

void CFlashLight::OnParentSelect(bool select)
{
	bool wasEnabled = m_lightEnabled;
	EnableLight(select && m_lightEnabled);

	if (!select && wasEnabled)
	{
		m_lightEnabled = true;
	}
}

void CFlashLight::OnEnterFirstPerson()
{
	if (m_lightEnabled)
	{
		EnableLight(false);
		EnableLight(true);
	}
}

void CFlashLight::OnEnterThirdPerson()
{
	if (m_lightEnabled)
	{
		EnableLight(false);
		EnableLight(true);
	}
}

void CFlashLight::ToggleFlashLight()
{
	EnableLight(!m_lightEnabled && m_flashLightEnabled);
}

void CFlashLight::EnableFlashLight(bool enable)
{
	m_flashLightEnabled = m_lightEnabled = enable;
	CFlashLight* pFlashLight = GetPlayerFlashLight();

	if (pFlashLight)
	{
		pFlashLight->EnableLight(m_flashLightEnabled);
	}
}

bool CFlashLight::IsFlashLightEnabled()
{
	return m_flashLightEnabled;
}

CWeapon* CFlashLight::GetWeapon() const
{
	IItem* pParentItem = m_pItemSystem->GetItem(GetParentId());

	if (pParentItem == NULL)
	{
		return 0;
	}

	CWeapon* pParentWeapon = static_cast<CWeapon*>(pParentItem->GetIWeapon());
	return pParentWeapon;
}
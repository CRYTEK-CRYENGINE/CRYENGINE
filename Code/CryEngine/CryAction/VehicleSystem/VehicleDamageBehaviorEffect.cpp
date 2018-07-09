// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleDamageBehaviorEffect.h"

#include <CryParticleSystem/ParticleParams.h>
#include "IVehicleSystem.h"
#include "VehicleCVars.h"

DEFINE_SHARED_PARAMS_TYPE_INFO(CVehicleDamageBehaviorEffect::SSharedParams)

//------------------------------------------------------------------------
bool CVehicleDamageBehaviorEffect::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;
	m_pDamageEffect = NULL;
	m_slot = -1;

	CVehicleParams effectParams = table.findChild("Effect");
	if (!effectParams)
	{
		return false;
	}

	string effectName = effectParams.getAttr("effect");

	CryFixedStringT<256> sharedParamsName;
	sharedParamsName.Format("%s::DamageBehaviorEffect::%s", pVehicle->GetEntity()->GetClass()->GetName(), effectName.c_str());

	ISharedParamsManager* pSharedParamsManager = CCryAction::GetCryAction()->GetISharedParamsManager();
	CRY_ASSERT(pSharedParamsManager);

	m_pSharedParams = CastSharedParamsPtr<SSharedParams>(pSharedParamsManager->Get(sharedParamsName));

	if (!m_pSharedParams)
	{
		SSharedParams sharedParams;

		sharedParams.effectName = effectName;

		sharedParams.damageRatioMin = 1.0f;
		table.getAttr("damageRatioMin", sharedParams.damageRatioMin);

		sharedParams.disableAfterExplosion = false;
		effectParams.getAttr("disableAfterExplosion", sharedParams.disableAfterExplosion);

		sharedParams.updateFromHelper = false;
		effectParams.getAttr("updateFromHelper", sharedParams.updateFromHelper);

		m_pSharedParams = CastSharedParamsPtr<SSharedParams>(pSharedParamsManager->Register(sharedParamsName, sharedParams));
	}

	CRY_ASSERT(m_pSharedParams.get());

	return true;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorEffect::Reset()
{
	if (m_slot != -1)
	{
		SEntitySlotInfo info;

		if (m_pVehicle->GetEntity()->GetSlotInfo(m_slot, info) && info.pParticleEmitter)
		{
			info.pParticleEmitter->Activate(false);
			if (m_pSharedParams->updateFromHelper)
			{
				m_pVehicle->NeedsUpdate(IVehicle::eVOU_NoUpdate);
			}
		}

		m_pVehicle->GetEntity()->FreeSlot(m_slot);

		m_slot = -1;
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorEffect::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams)
{
	if (event == eVDBE_MaxRatioExceeded || (event == eVDBE_VehicleDestroyed && m_pSharedParams->disableAfterExplosion) || (event == eVDBE_Repair && behaviorParams.componentDamageRatio < m_pSharedParams->damageRatioMin))
	{
		Reset();

		return;
	}

	if (event == eVDBE_Hit || event == eVDBE_ComponentDestroyed || event == eVDBE_Repair || (event == eVDBE_VehicleDestroyed && !m_pSharedParams->disableAfterExplosion))
	{
		if (m_slot == -1 && !(m_pSharedParams->disableAfterExplosion && m_pVehicle->GetStatus().health <= 0.0f) && event != eVDBE_Repair)
		{
#if ENABLE_VEHICLE_DEBUG
			if (VehicleCVars().v_debugdraw == eVDB_Damage)
			{
				CryLog("Vehicle damage %.2f", behaviorParams.componentDamageRatio);
			}
#endif

			LoadEffect(behaviorParams.pVehicleComponent);
		}

		if (m_slot > -1)
		{
			UpdateEffect(behaviorParams.randomness, behaviorParams.componentDamageRatio);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorEffect::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorEffect::Serialize(TSerialize ser, EEntityAspects aspects)
{
	bool isEffectActive = m_slot > -1;

	ser.Value("isActive", isEffectActive);

	int slot = m_slot;

	ser.ValueWithDefault("slot", slot, -1);

	if (ser.IsReading())
	{
		if (!isEffectActive && m_slot > -1)
		{
			Reset();
		}

		m_slot = slot;
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorEffect::LoadEffect(IVehicleComponent* pComponent)
{
	if (!m_pDamageEffect)
	{
		const SDamageEffect* pDamageEffect = m_pVehicle->GetParticleParams()->GetDamageEffect(m_pSharedParams->effectName.c_str());

		if (!pDamageEffect)
		{
#if ENABLE_VEHICLE_DEBUG
			if (VehicleCVars().v_debugdraw == eVDB_Damage)
			{
				CryLog("Failed to find damage effect %s", m_pSharedParams->effectName.c_str());
			}
#endif

			return;
		}

#if ENABLE_VEHICLE_DEBUG
		if (VehicleCVars().v_debugdraw == eVDB_Damage)
		{
			CryLog("Found effect %s", m_pSharedParams->effectName.c_str());
		}
#endif

		m_pDamageEffect = pDamageEffect;
	}

	if (m_pDamageEffect)
	{
#if ENABLE_VEHICLE_DEBUG
		if (VehicleCVars().v_debugdraw == eVDB_Damage)
		{
			CryLog("Starting vehicle damage effect: %s", m_pDamageEffect->effectName.c_str());
		}
#endif

		if (IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(m_pDamageEffect->effectName.c_str(), "VehicleDamageBehaviorEffect"))
		{
			IEntity* pEntity = m_pVehicle->GetEntity();

			CRY_ASSERT(pEntity);

			m_slot = pEntity->LoadParticleEmitter(m_slot, pEffect, NULL, false, true);

			if (m_pDamageEffect->pHelper)
			{
				Matrix34 tm;

				m_pDamageEffect->pHelper->GetVehicleTM(tm);

				pEntity->SetSlotLocalTM(m_slot, tm);

				if (m_pSharedParams->updateFromHelper)
				{
					m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
				}
			}
			else if (pComponent)
			{
				Matrix34 tm(IDENTITY);

				tm.SetTranslation(pComponent->GetBounds().GetCenter());

				pEntity->SetSlotLocalTM(m_slot, tm);
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorEffect::UpdateEffect(float randomness, float damageRatio)
{
	CRY_ASSERT(m_pDamageEffect);

	if (m_pDamageEffect)
	{
		if (IParticleEmitter* pParticleEmitter = m_pVehicle->GetEntity()->GetParticleEmitter(m_slot))
		{
			SpawnParams spawnParams;

			spawnParams.fPulsePeriod = m_pDamageEffect->pulsePeriod * ((1.0f - randomness) * cry_random(0.0f, 1.0f));

			pParticleEmitter->SetSpawnParams(spawnParams);
		}
	}
}

void CVehicleDamageBehaviorEffect::Update(const float deltaTime)
{
	SEntitySlotInfo slotInfo;
	if (m_pVehicle->GetEntity()->GetSlotInfo(m_slot, slotInfo) && slotInfo.pParticleEmitter)
	{
		Matrix34 tm;
		m_pDamageEffect->pHelper->GetVehicleTM(tm);
		m_pVehicle->GetEntity()->SetSlotLocalTM(m_slot, tm);
	}
}

DEFINE_VEHICLEOBJECT(CVehicleDamageBehaviorEffect);

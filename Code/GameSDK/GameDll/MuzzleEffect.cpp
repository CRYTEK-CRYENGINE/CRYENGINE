// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MuzzleEffect.h"
#include "Weapon.h"
#include "FireMode.h"
#include "FireModeParams.h"


namespace
{


	int BarrelCount(const CFireMode* pFireMode)
	{
		return pFireMode->GetShared()->fireparams.barrel_count;
	}



	bool MuzzleFromFireLocator(const CFireMode* pFireMode)
	{
		return pFireMode->GetShared()->fireparams.muzzleFromFiringLocator;
	}



	const SEffectParams& GetMuzzleFlashParams(const CFireMode* pFireMode)
	{
		return pFireMode->GetShared()->muzzleflash;
	}



	const SEffectParams& GetMuzzleBeamParams(const CFireMode* pFireMode)
	{
		return pFireMode->GetShared()->muzzlebeam;
	}


}



CMuzzleEffect::CMuzzleEffect()
{
	m_beamFxId[0] = m_beamFxId[1] = 0;
}



void CMuzzleEffect::Initialize(CFireMode* pFireMode)
{
	m_mfIds.resize(BarrelCount(pFireMode));
}



void CMuzzleEffect::StartFire(CFireMode* pFireMode)
{
	MuzzleBeamEffect(true, pFireMode);
}



void CMuzzleEffect::StopFire(CFireMode* pFireMode)
{
	MuzzleBeamEffect(false, pFireMode);
}



void CMuzzleEffect::Shoot(CFireMode* pFireMode, Vec3 potentialTarget, int barrel)
{
	MuzzleFlashEffect(pFireMode, barrel);
	SetBeamTarget(pFireMode, potentialTarget);
}



void CMuzzleEffect::MuzzleFlashEffect(CFireMode* pFireMode, int barrel)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CWeapon* pWeapon = pFireMode->GetWeapon();
	const SEffectParams& muzzleFlash = GetMuzzleFlashParams(pFireMode);

	const CItem::SStats &stats = pWeapon->GetStats();

	int slot	= stats.fp ? eIGS_FirstPerson : eIGS_ThirdPerson;
	int id		= stats.fp ? 0 : 1;

	if (!muzzleFlash.effect[id].empty() && !pWeapon->GetEntity()->IsHidden())
	{
		if (barrel < (int)m_mfIds.size())
		{
			IParticleEmitter *pEmitter = NULL;
			bool spawnEffect = true;

			// In mp only spawn muzzle flashes which are visible
			if(g_pGameCVars->g_muzzleFlashCull)
			{
				EntityId weaponOwnerEntityId = pWeapon->GetOwnerId();
				EntityId localClientId = gEnv->pGameFramework->GetClientActorId();

				if(weaponOwnerEntityId != localClientId) // Always want to spawn muzzle flash for client
				{
					Vec3 cameraPos = gEnv->p3DEngine->GetRenderingCamera().GetPosition();
					Vec3 emitterPos;

					emitterPos = pWeapon->GetSlotHelperPos(slot, muzzleFlash.helper[id].c_str(), true);

					float emitterDistFromCameraSq = (emitterPos - cameraPos).GetLengthSquared();

					Sphere emitterSphere(emitterPos, 2.0f);
					if((emitterDistFromCameraSq > g_pGameCVars->g_muzzleFlashCullDistance) ||	// If too far in distance
						(gEnv->p3DEngine->GetRenderingCamera().IsSphereVisible_F(emitterSphere) == false) ) // If not in frustum
					{
						spawnEffect = false;
					}
				}
			}

			if(spawnEffect)
			{
				if (!m_mfIds[barrel].mfId[id])
					AttachEmitters(pFireMode, barrel);

				pEmitter = pWeapon->GetEffectEmitter(m_mfIds[barrel].mfId[id]);
			}

			if (pEmitter)
			{
				QuatTS locator;
				bool useLocator = GetMuzzleFireLocation(pFireMode, &locator);
				SpawnParams sparams;
				pEmitter->GetSpawnParams(sparams);
				sparams.fSizeScale = pWeapon->GetMuzzleFlashScale();
				pEmitter->SetSpawnParams(sparams);

				pEmitter->EmitParticle(NULL, NULL, (useLocator) ? (&locator) : NULL);
			}
		}
	}		  

	bool shouldSendAIEvent = (muzzleFlash.aiVisibilityRadius > 0.0f) && (pWeapon->GetOwner() && pWeapon->GetOwner()->GetAI());
	if (shouldSendAIEvent)
	{
		gEnv->pAISystem->DynOmniLightEvent(pWeapon->GetOwner()->GetWorldPos(),
			muzzleFlash.aiVisibilityRadius, AILE_MUZZLE_FLASH, pWeapon->GetOwnerId());
	}
}



bool CMuzzleEffect::GetMuzzleFireLocation(const CFireMode* pFireMode, QuatTS* location) const
{
	if (MuzzleFromFireLocator(pFireMode))
	{
		location->t = pFireMode->GetFiringPos(ZERO);
		location->q.SetRotationV0V1(Vec3(0, 1, 0), pFireMode->GetFiringDir(ZERO, location->t));
		location->s = 1.0f;
		return true;
	}
	return false;
}



void CMuzzleEffect::MuzzleBeamEffect(bool attach, CFireMode* pFireMode)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CWeapon* pWeapon = pFireMode->GetWeapon();
	const SEffectParams& muzzleBeam = GetMuzzleBeamParams(pFireMode);

	if (attach)
	{
		CRY_PROFILE_REGION(PROFILE_GAME, "CMuzzleEffect::MuzzleBeamEffect() Attach Effect");

		const CItem::SStats &stats = pWeapon->GetStats();

		int slot	= stats.fp ? eIGS_FirstPerson : eIGS_ThirdPerson;
		int id		= stats.fp ? 0 : 1;

		if (!m_beamFxId[id])
		{
			m_beamFxId[id] = pWeapon->AttachEffect(
				slot, muzzleBeam.helperFromAccessory,
				muzzleBeam.effect[0].c_str(), muzzleBeam.helper[0].c_str(),
				Vec3(ZERO), Vec3Constants<float>::fVec3_OneY,
				muzzleBeam.scale[0], false);
		}

		IParticleEmitter *pEmitter = pWeapon->GetEffectEmitter(m_beamFxId[id]);

		if (pEmitter)
		{
			SpawnParams sparams;
			pEmitter->GetSpawnParams(sparams);
			sparams.fSizeScale = pWeapon->GetMuzzleFlashScale();
			pEmitter->SetSpawnParams(sparams);
			pEmitter->EmitParticle();
		}
	}
	else
	{
		CRY_PROFILE_REGION(PROFILE_GAME, "CMuzzleEffect::MuzzleBeamEffect() Detach Effect");
		pWeapon->DetachEffect(m_beamFxId[0]);
		pWeapon->DetachEffect(m_beamFxId[1]);
		m_beamFxId[0] = m_beamFxId[1] = 0;
	}
}



void CMuzzleEffect::AttachEmitters(CFireMode* pFireMode, int barrel)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CWeapon* pWeapon = pFireMode->GetWeapon();
	const SEffectParams& muzzleFlash = GetMuzzleFlashParams(pFireMode);
	const SEffectParams& muzzleBeam = GetMuzzleBeamParams(pFireMode);

	if (m_mfIds.empty())
		return;

	int id = pWeapon->GetStats().fp ? 0 : 1;
	Vec3 offset(ZERO);

	if (muzzleFlash.helper[id].empty())
	{ 
		// if no helper specified, try getting pos from firing locator
		IWeaponFiringLocator *pLocator = pWeapon->GetFiringLocator();            

		if (pLocator && pLocator->GetFiringPos(pWeapon->GetEntityId(), pFireMode, offset))
			offset = pWeapon->GetEntity()->GetWorldTM().GetInvertedFast() * offset;
	}

	if (!muzzleFlash.effect[id].empty() && !m_mfIds[barrel].mfId[id])
	{
		m_mfIds[barrel].mfId[id] = pWeapon->AttachEffect(id, muzzleFlash.helperFromAccessory, muzzleFlash.effect[id].c_str(), 
			muzzleFlash.helper[id].c_str(), offset, Vec3Constants<float>::fVec3_OneY, muzzleFlash.scale[id], false);
	}
}

void CMuzzleEffect::DetachEmitters(CFireMode* pFireMode, int barrel)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CWeapon* pWeapon = pFireMode->GetWeapon();

	for (size_t i=0; i<m_mfIds.size(); ++i)
	{
		pWeapon->DetachEffect(m_mfIds[i].mfId[0]);
		pWeapon->DetachEffect(m_mfIds[i].mfId[1]);

		m_mfIds[i].mfId[0] = m_mfIds[i].mfId[1] = 0;
	}
}

void CMuzzleEffect::SetBeamTarget(CFireMode* pFireMode, Vec3 potentialTarget)
{
	CWeapon* pWeapon = pFireMode->GetWeapon();
	int id = pWeapon->GetStats().fp ? 0 : 1;
	IParticleEmitter* pEmiter = pWeapon->GetEffectEmitter(m_beamFxId[id]);

	if (pEmiter)
	{
		ParticleTarget target;
		target.bPriority = true;
		target.bTarget = true;
		target.fRadius = 0.0f;
		target.vTarget = potentialTarget;
		target.vVelocity = Vec3(ZERO);
		pEmiter->SetTarget(target);
	}
}



void CMuzzleEffect::GetMemoryUsage(ICrySizer * s) const
{
	s->AddContainer(m_mfIds);
}

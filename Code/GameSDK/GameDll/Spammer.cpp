// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Spammer.h"
#include "Player.h"
#include "Projectile.h"
#include "PlayerVisTable.h"
#include <IVehicleSystem.h>
#include <IViewSystem.h>
#include "GameActions.h"


namespace
{
	struct SpammerTarget
	{
		SpammerTarget(const EntityId _targetId, const float _radius)
			: targetId(_targetId)
			, radius(_radius)
		{

		}

		const EntityId targetId;
		const float    radius;
	};



	SpammerTarget GetVisibilityTestTarget( const IEntity* pSourceEntity, const EntityId sourceEntityId, CActor* pActor, const AABB& sourceEntityBounds )
	{
		assert(pSourceEntity != NULL);

		static IEntityClass* s_TacticalEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("TacticalEntity");

		if(pSourceEntity->GetClass() != s_TacticalEntityClass)
		{
			IVehicle* pVehicle = pActor ? pActor->GetLinkedVehicle() : 0;
			if (pVehicle)
			{
				IEntity* pVehicleEntity = pVehicle->GetEntity();
				AABB vehicleBounds;
				pVehicleEntity->GetWorldBounds(vehicleBounds);
				return SpammerTarget(pVehicleEntity->GetId(), vehicleBounds.GetRadius());
			}

			return SpammerTarget( sourceEntityId, sourceEntityBounds.GetRadius() );
		}
		
		const IEntity* pParentEntity = pSourceEntity->GetParent();
		if(pParentEntity)
		{
			AABB parentBounds;
			pParentEntity->GetWorldBounds(parentBounds);
			return SpammerTarget( pParentEntity->GetId(), parentBounds.GetRadius() );
		}
		else
		{
			return SpammerTarget( sourceEntityId, sourceEntityBounds.GetRadius() );
		}
	}

};

CRY_IMPLEMENT_GTI(CSpammer, CSingle);

CSpammerCloudTargets::CSpammerCloudTargets()
	:	m_numLockOns(0)
{
}


bool CSpammerCloudTargets::Empty() const
{
	return m_numLockOns == 0;
}


void CSpammerCloudTargets::Clear()
{
	m_numLockOns = 0;
	m_targets.clear();
}


void CSpammerCloudTargets::LockOn(EntityId target)
{
	++m_numLockOns;
	for (size_t i = 0; i < m_targets.size(); ++i)
	{
		if (m_targets[i].m_target == target)
		{
			m_targets[i].m_numLocks++;
			return;
		}
	}

	STarget newTarget;
	newTarget.m_target = target;
	newTarget.m_numLocks = 1;
	m_targets.push_back(newTarget);
}


int CSpammerCloudTargets::GetNumLockOns() const
{
	return m_numLockOns;
}


int CSpammerCloudTargets::GetNumLockOns(EntityId target) const
{
	for (size_t i = 0; i < m_targets.size(); ++i)
		if (m_targets[i].m_target == target)
			return m_targets[i].m_numLocks;
	return 0;
}


const CSpammerCloudTargets::STarget& CSpammerCloudTargets::GetTarget(int idx) const
{
	return m_targets[idx];
}


int CSpammerCloudTargets::GetNumTargets() const
{
	return static_cast<int>(m_targets.size());
}


EntityId CSpammerCloudTargets::UnlockNext()
{
	EntityId result = 0;
	if (Empty())
		return result;

	STarget& target = *(m_targets.end() - 1);
	--target.m_numLocks;
	result = target.m_target;

	if (target.m_numLocks == 0)
		m_targets.pop_back();

	--m_numLockOns;

	return result;
}



void CSpammerPotentialTargets::Clear()
{
	m_potentialTargets.clear();
	m_totalProbability = 0.0f;
}



void CSpammerPotentialTargets::AddTarget(EntityId target, float probability)
{
	m_totalProbability += probability;
	STarget newTarget;
	newTarget.m_target = target;
	newTarget.m_probability = probability;
	m_potentialTargets.push_back(newTarget);
}



CSpammer::CSpammer()
	: m_state(EState_None)	
	, m_timer(0.0f)
	, m_nextFireTimer(0.0f)
	, m_numLoadedRockets(0)
	, m_firingPending(false)
{
	m_potentialTargets.Clear();
}

void CSpammer::Activate(bool activate)
{
	m_pWeapon->EnableUpdate(true, eIUS_FireMode);
	BaseFiremode::Activate(activate);

	CActor *pOwnerActor = m_pWeapon->GetOwnerActor();
	if(pOwnerActor && pOwnerActor->IsClient())
	{
		if(activate) 
		{
			// Make sure the vehicle we are attached to does not block view of targets. Disable ability to pick up other items whilst using the turret.
			g_pGame->GetPlayerVisTable()->AddGlobalIgnoreEntity(m_pWeapon->GetHostId(), "CSpammer::Activate(true)");
			g_pGameActions->FilterItemPickup()->Enable(true); 
		}
		else
		{
			g_pGame->GetPlayerVisTable()->RemoveGlobalIgnoreEntity(m_pWeapon->GetHostId());
			g_pGameActions->FilterItemPickup()->Enable(false); 
		}
	}
}



void CSpammer::Update(float frameTime, uint32 frameId)
{
	BaseFiremode::Update(frameTime, frameId);

	UpdatePotentialTargets();

	switch (m_state)
	{
		case EState_None:
			m_nextFireTimer -= frameTime;
			if (m_nextFireTimer < 0.0f)
			{
				if (m_firingPending)
					StartFire();
				m_nextFireTimer = 0.0f;
			}
			break;
		case EState_LoadingIn:
			UpdateLoadIn(frameTime);
			break;

		case EState_Bursting:
			UpdateBurst(frameTime);
			break;
	}
}



void CSpammer::StartFire()
{
	if (m_nextFireTimer > 0.0f)
	{
		m_firingPending = true;
		return;
	}
	if (m_state != EState_None)
		return;
	StartLoadingIn();
	m_firingPending = false;
}



void CSpammer::StopFire()
{
	if (m_state != EState_LoadingIn)
		return;
	StartBursting();
	m_firingPending = false;
}



bool CSpammer::ShootRocket(EntityId target)
{
	IEntityClass* pAmmoClass = m_fireParams->fireparams.ammo_type_class;
	int ammoCount = m_pWeapon->GetAmmoCount(pAmmoClass);

	CActor *pOwnerActor = m_pWeapon->GetOwnerActor();

	const bool playerIsShooter = pOwnerActor ? pOwnerActor->IsPlayer() : false;
	const bool clientIsShooter = pOwnerActor ? pOwnerActor->IsClient() : false;

	bool bHit = false;
	ray_hit rayhit;	 
	rayhit.pCollider = 0;

	Vec3 hit = GetProbableHit(WEAPON_HIT_RANGE, &bHit, &rayhit);
	Vec3 pos = GetFiringPos(hit);
	Vec3 dir = GetFiringDir(hit, pos);
	Vec3 vel = GetFiringVelocity(dir);

	int flags = CItem::eIPAF_Default;
	if (IsProceduralRecoilEnabled() && pOwnerActor)
	{
		pOwnerActor->ProceduralRecoil(m_fireParams->proceduralRecoilParams.duration, m_fireParams->proceduralRecoilParams.strength, m_fireParams->proceduralRecoilParams.kickIn, m_fireParams->proceduralRecoilParams.arms);
	}

	float speedOverride = -1.f;
	GetWeapon()->PlayAction(GetFragmentIds().fire, 0, false, flags, speedOverride);

	CheckNearMisses(hit, pos, dir, (hit-pos).len(), 1.0f);

	CProjectile* pAmmo = m_pWeapon->SpawnAmmo(m_fireParams->fireparams.spawn_ammo_class, false);
	const EntityId weaponOwnerId = m_pWeapon->GetOwnerId();
	EntityId ammoId = 0;

	if (pAmmo)
	{
		ammoId = pAmmo->GetEntityId();

		CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId, string().Format("Invalid hit type '%s' in fire params for '%s'", m_fireParams->fireparams.hit_type.c_str(), m_pWeapon->GetEntity()->GetName()));
		CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId == g_pGame->GetGameRules()->GetHitTypeId(m_fireParams->fireparams.hit_type.c_str()), "Sanity Check Failed: Stored hit type id does not match the type string, possibly CacheResources wasn't called on this weapon type");

		pAmmo->SetParams(CProjectile::SProjectileDesc(
			weaponOwnerId, m_pWeapon->GetHostId(), m_pWeapon->GetEntityId(), 
			m_fireParams->fireparams.damage, m_fireParams->fireparams.damage_drop_min_distance, m_fireParams->fireparams.damage_drop_per_meter, m_fireParams->fireparams.damage_drop_min_damage,
			m_fireParams->fireparams.hitTypeId, m_fireParams->fireparams.bullet_pierceability_modifier, m_pWeapon->IsZoomed()));
		// this must be done after owner is set
		pAmmo->InitWithAI();
		pAmmo->SetDestination(target);
		pAmmo->Launch(pos, dir, vel, m_speed_scale);

		m_projectileId = ammoId;

		if (clientIsShooter && pAmmo->IsPredicted() && gEnv->IsClient() && gEnv->bServer)
		{
			pAmmo->GetGameObject()->BindToNetwork();
		}
	}

	if (m_pWeapon->IsServer())
	{
		const char *ammoName = pAmmoClass != NULL ? pAmmoClass->GetName() : NULL;
		g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(m_pWeapon->GetOwner(), GameplayEvent(eGE_WeaponShot, ammoName, 1, (void *)(EXPAND_PTR)m_pWeapon->GetEntityId()));
	}

	OnShoot(weaponOwnerId, ammoId, pAmmoClass, pos, dir, vel);

	if(playerIsShooter)
	{
		const SThermalVisionParams& thermalParams = m_fireParams->thermalVisionParams;
		m_pWeapon->AddShootHeatPulse(pOwnerActor, thermalParams.weapon_shootHeatPulse, thermalParams.weapon_shootHeatPulseTime,
			thermalParams.owner_shootHeatPulse, thermalParams.owner_shootHeatPulseTime);
	}

	m_muzzleEffect.Shoot(this, hit, m_barrelId);
	RecoilImpulse(pos, dir);

	m_fired = true;
	m_next_shot += m_next_shot_dt;

	if (++m_barrelId == m_fireParams->fireparams.barrel_count)
		m_barrelId = 0;

	ammoCount--;

	int clipSize = GetClipSize();
	if (clipSize != -1)
	{
		if (clipSize!=0)
			m_pWeapon->SetAmmoCount(pAmmoClass, ammoCount);
		else
			m_pWeapon->SetInventoryAmmoCount(pAmmoClass, ammoCount);
	}

	m_pWeapon->RequestShoot(pAmmoClass, pos, dir, vel, hit, m_speed_scale, pAmmo? pAmmo->GetGameObject()->GetPredictionHandle() : 0, false);

	return true;
}



void CSpammer::UpdateLoadIn(float frameTime)
{
	const int currentAmmoCount = GetAmmoCount();
	const bool infiniteAmmo = (GetClipSize() < 0);

	if ((!infiniteAmmo) && (m_numLoadedRockets >= currentAmmoCount))
	{
		GetWeapon()->PlayAction(GetFragmentIds().empty_clip);
		StopFire();
		return;
	}

	const SSpammerParams& params = GetShared()->spammerParams;

	const float loadInTime = 1.0f / (params.loadInRate / 60.0f);
	m_timer -= frameTime;

	while (m_timer < 0.0f && m_numLoadedRockets < params.maxNumRockets)
	{
		m_timer += loadInTime;
		AddTarget();
		GetWeapon()->PlayAction(GetFragmentIds().cock);
	}

	if (m_numLoadedRockets != m_targetsAssigned.GetNumLockOns())
	{
		EntityId nextTarget = GetNextLockOnTarget();
		if (nextTarget != 0)
			m_targetsAssigned.LockOn(nextTarget);
	}
}



void CSpammer::UpdateBurst(float frameTime)
{
	const SSpammerParams& params = GetShared()->spammerParams;

	const float burstTime = 1.0f / (params.burstRate / 60.0f);
	m_timer -= frameTime;

	while (m_timer < 0.0f && m_numLoadedRockets != 0)
	{
		m_timer += burstTime;
		--m_numLoadedRockets;
		ShootNextTarget();
	}

	if (m_numLoadedRockets == 0)
		m_state = EState_None;
}



void CSpammer::UpdatePotentialTargets()
{
	const float minLockOnDistance = m_fireParams->spammerParams.minLockOnDistance;
	const float maxLockOnDistance = m_fireParams->spammerParams.maxLockOnDistance;
	const float maxAngleCos = cos_tpl(DEG2RAD(m_fireParams->spammerParams.targetingTolerance));

	const CAutoAimManager& autoAimManager = g_pGame->GetAutoAimManager();
	const TAutoaimTargets& aaTargets = autoAimManager.GetAutoAimTargets();
	const int targetCount = aaTargets.size();

	const Vec3 probableHit = Vec3Constants<float>::fVec3_Zero;
	const Vec3 weaponPos = GetWeaponPosition(probableHit);
	const Vec3 weaponFwd = GetWeaponDirection(weaponPos, probableHit);

	m_potentialTargets.Clear();

	CPlayerVisTable::SVisibilityParams queryTargetParams(0);
	const bool flat2DMode = m_fireParams->spammerParams.targetingFlatMode;

	for (int i = 0; i < targetCount; ++i)
	{
		const SAutoaimTarget& target = aaTargets[i];
		CRY_ASSERT(target.entityId != m_pWeapon->GetOwnerId());

		if (!target.HasFlagSet(eAATF_AIHostile))
			continue;

		IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity(target.entityId);
		if (!pTargetEntity)
			continue;
		CActor* pActor = target.pActorWeak.lock().get();

		AABB bounds;
		pTargetEntity->GetWorldBounds(bounds);
		Vec3 targetPos = bounds.GetCenter();
		Vec3 targetDistVec = (targetPos - weaponPos).normalized();
		float distance = targetPos.GetDistance(weaponPos);

		if (distance <= minLockOnDistance || distance >= maxLockOnDistance)
			continue;

		float alignment;
		if (!flat2DMode)
		{
			alignment = weaponFwd * targetDistVec;
		}
		else
		{
			const CCamera& viewCamera = gEnv->pSystem->GetViewCamera();
			if (!viewCamera.IsPointVisible(targetPos))
				continue;

			alignment = Vec3(weaponFwd.x, weaponFwd.y, 0.0f).GetNormalizedSafe() * Vec3(targetDistVec.x, targetDistVec.y, 0.0f).GetNormalizedSafe();
		}

		if (alignment <= maxAngleCos)
			continue;

		const SpammerTarget finalTargetInfo = GetVisibilityTestTarget(pTargetEntity, target.entityId, pActor, bounds);

		const int kAutoaimVisibilityLatency = 0;
		queryTargetParams.targetEntityId = finalTargetInfo.targetId;
		if (!g_pGame->GetPlayerVisTable()->CanLocalPlayerSee(queryTargetParams, kAutoaimVisibilityLatency))
			continue;

		float priority = 1.0f;
		priority *= finalTargetInfo.radius;
		priority /= m_targetsAssigned.GetNumLockOns(target.entityId)+1;
		const float m = 1.0f / (1.0f - maxAngleCos);
		priority *= m * alignment + (1.0f - m);
		priority *= 0.1f;
		priority = min(priority, 1.0f);

		m_potentialTargets.AddTarget(target.entityId, priority);
	}

	float n = 0.0f;
	size_t num = m_potentialTargets.m_potentialTargets.size();
	for (size_t i = 0; i < num; ++i)
	{
		n = max(n, m_potentialTargets.m_potentialTargets[i].m_probability);
	}
	m_potentialTargets.m_totalProbability = 0.0f;
	for (size_t i = 0; num && i < m_potentialTargets.m_potentialTargets.size(); ++i)
	{
		m_potentialTargets.m_potentialTargets[i].m_probability /= n + FLT_EPSILON;
		m_potentialTargets.m_totalProbability += m_potentialTargets.m_potentialTargets[i].m_probability;
	}
}

void CSpammer::StartLoadingIn()
{
	m_state = EState_LoadingIn;
	m_timer = 0.0f;
	m_targetsAssigned.Clear();
	m_numLoadedRockets = 0;

	const SFireModeParams& params = *GetShared();
	m_nextFireTimer = 1.0f / (params.fireparams.rate / 60.0f);
}



void CSpammer::StartBursting()
{
	const SSpammerParams& params = GetShared()->spammerParams;
	m_timer = 1.0f / (params.burstRate / 60.0f);
	m_state = EState_Bursting;
}



void CSpammer::AddTarget()
{
	++m_numLoadedRockets;
	EntityId nextTarget = GetNextLockOnTarget();
	if (nextTarget != 0)
		m_targetsAssigned.LockOn(nextTarget);
}



void CSpammer::ShootNextTarget()
{
	EntityId nextTarget = m_targetsAssigned.UnlockNext();
	ShootRocket(nextTarget);
}



Vec3 CSpammer::GetWeaponPosition(const Vec3& probableHit) const
{
	Vec3 position(ZERO);

	if(gEnv->bServer)
	{
		CActor* pOwnerActor = m_pWeapon->GetOwnerActor();
		IMovementController* pMC = pOwnerActor ? pOwnerActor->GetMovementController() : NULL;
		
		if (pMC)
		{
			SMovementState state;
			pMC->GetMovementState(state);
			position = state.eyePosition;
		}
		else
		{
			return GetFiringPos(probableHit);
		}
	}

	return position;
}



Vec3 CSpammer::GetWeaponDirection(const Vec3& firingPosition, const Vec3& probableHit) const
{
	Vec3 direction(ZERO);

	if(gEnv->bServer)
	{
		CActor* pOwnerActor = m_pWeapon->GetOwnerActor();
		IMovementController* pMC = pOwnerActor ? pOwnerActor->GetMovementController() : NULL;

		if (pMC)
		{
			SMovementState state;
			pMC->GetMovementState(state);
			direction = state.aimDirection;
		}
		else
		{
			return GetFiringDir(probableHit, firingPosition);
		}
	}

	return direction;
}



struct PotentialTarget
{
	EntityId entity;
	float probability;
};



EntityId CSpammer::GetNextLockOnTarget() const
{
	EntityId result = 0;

	if (!m_potentialTargets.m_potentialTargets.empty())
	{
		float randomProbability = cry_random(0.0f, m_potentialTargets.m_totalProbability);
		int selectedIdx;
		for (selectedIdx = 0; randomProbability > 0.0f; ++selectedIdx)
			randomProbability -= m_potentialTargets.m_potentialTargets[selectedIdx].m_probability;
		--selectedIdx;
		result = m_potentialTargets.m_potentialTargets[selectedIdx].m_target;
	}

	return result;
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Actor.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include "IVehicleSystem.h"
#include "HommingSwarmProjectile.h"
#include "Single.h"



CHommingSwarmProjectile::SVehicleStatus::SVehicleStatus(IEntity* pEntity, IPhysicalEntity* pPhysics)
	:	position(pEntity->GetWorldPos())
	,	front(ZERO)
	,	velocity(ZERO)
	,	speed(0.0f)
{
	pe_status_dynamics status;
	if (pPhysics->GetStatus(&status))
	{
		velocity = status.v;
		speed = velocity.len();
		front = velocity.normalized();
	}
}




CHommingSwarmProjectile::CHommingSwarmProjectile()
	:	m_wanderState(ZERO)
	,	m_hommingDirection(ZERO)
	,	m_controlLostTimer(0.0f)
	,	m_lockedDestination(ZERO)
	,	m_spawnDirection(ZERO)
	,	m_fixedTarget(0)
	,	m_wanderTimer(0.0f)
	,	m_hommingBehaviour(true)
	,	m_curlDirection(fsgnf(cry_random(-1.0f, 1.0f)))
	,	m_curlTime(0.0f)
	,	m_curlShift(cry_random(0.0f, 1.0f))
{
  m_Roll = cry_random(0.0f, gf_PI);
}



CHommingSwarmProjectile::~CHommingSwarmProjectile()
{
}



void CHommingSwarmProjectile::Update(SEntityUpdateContext &ctx, int updateSlot)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	BaseClass::Update(ctx, updateSlot);

	if (!gEnv->bServer || !m_pAmmoParams->pHomingSwarmParams || !m_pPhysicalEntity || updateSlot != 0)
		return;

	const float proxyRadius = m_pAmmoParams->pHomingSwarmParams->proxyRadius;
	if (proxyRadius > 0.0f && ProximityDetector(proxyRadius))
		Explode(CProjectile::SExplodeDesc(true));

	// retrieve target
	Vec3 targetPosition = GetHomingTarget(GetWeapon());

	if (m_controlLostTimer > 0.0f)
	{
		m_controlLostTimer -= ctx.fFrameTime;
		if (m_controlLostTimer < 0.0f)
			m_controlLostTimer = 0.0f;
	}

	SVehicleStatus vehicle(GetEntity(), m_pPhysicalEntity);

	if (vehicle.speed > 0.001f)
	{
		const float maxSpeed = m_pAmmoParams->pHomingSwarmParams->maxSpeed;

		Vec3 force(ZERO);

		force += Seek(vehicle, targetPosition);
		force += Wander(vehicle, ctx.fFrameTime);
		force += Curl(vehicle, ctx.fFrameTime);

		Vec3 velocity = vehicle.velocity + force*ctx.fFrameTime;
		velocity.ClampLength(clamp_tpl(vehicle.speed, 0.0f, maxSpeed));

		ExplodeIfMissingTarget(vehicle, velocity, targetPosition);

		pe_action_set_velocity action;
		action.v = velocity;
		m_pPhysicalEntity->Action(&action);
	}
}



Vec3 CHommingSwarmProjectile::GetHomingTarget(CWeapon* pOwnerWeapon)
{
	Vec3 targetPosition(ZERO);

	if (!m_hommingBehaviour)
		return targetPosition;

	if (m_fixedTarget != 0)
	{
		IEntity* pTarget = gEnv->pEntitySystem->GetEntity(m_fixedTarget);
		if (!pTarget)
			return targetPosition;
		AABB worldBounds;
		pTarget->GetWorldBounds(worldBounds);
		return worldBounds.GetCenter();
	}

	if(pOwnerWeapon)
	{
		if (CheckAllProjectileFlags(ePFlag_firedViaProxy))
		{
			if(m_lockedDestination.IsZero())
			{
				m_lockedDestination = pOwnerWeapon->GetDestination();
				m_hommingDirection = m_initial_dir;
			}

			return m_lockedDestination;
		}
		
		CActor* pOwnerActor = pOwnerWeapon->GetOwnerActor();

		if(!pOwnerActor)
			return pOwnerWeapon->GetDestination(); //targetPosition;

		IMovementController* pMC = pOwnerActor->GetMovementController();
		if (!pMC)
			return pOwnerWeapon->GetDestination(); //targetPosition;

		Vec3 homingGuidePosition(ZERO);
		Vec3 homingGuideDirection(ZERO);

		SMovementState state;
		pMC->GetMovementState(state);

		homingGuidePosition = state.eyePosition;
		homingGuideDirection = state.eyeDirection;

		const float targetDistance = m_pAmmoParams->pHomingSwarmParams->targetDistance;
		targetPosition = (homingGuidePosition + homingGuideDirection*targetDistance);
	}

	return targetPosition;
}



void CHommingSwarmProjectile::HandleEvent(const SGameObjectEvent &event)
{
	if (CheckAnyProjectileFlags(ePFlag_destroying))
		return;

	CProjectile::HandleEvent(event);

	if (!gEnv->bServer || IsDemoPlayback())
		return;

	if (event.event == eGFE_OnCollision)
	{
		if (const EventPhysCollision *pCollision = reinterpret_cast<const EventPhysCollision*>(event.ptr))
		{
			if(pCollision->pEntity[0]->GetType()==PE_PARTICLE)
			{ 
				float bouncy, friction;
				uint32 pierceabilityMat;
				gEnv->pPhysicalWorld->GetSurfaceParameters(pCollision->idmat[1], bouncy, friction, pierceabilityMat);
				pierceabilityMat &= sf_pierceable_mask;
				uint32 pierceabilityProj = GetAmmoParams().pParticleParams ? GetAmmoParams().pParticleParams->iPierceability : 13;
				if (pierceabilityMat > pierceabilityProj)
					return;
			}

			IEntity* pTarget = pCollision->iForeignData[1]==PHYS_FOREIGN_ID_ENTITY ? (IEntity*)pCollision->pForeignData[1] : 0;
			CProjectile::SExplodeDesc explodeDesc(true);
			explodeDesc.impact = false;
			explodeDesc.normal = -pCollision->n;
			explodeDesc.vel = pCollision->vloc[0];
			explodeDesc.targetId = pTarget ? pTarget->GetId() : 0;
			Explode(explodeDesc);
		}

	}
}



void CHommingSwarmProjectile::Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale)
{
	CProjectile::Launch(pos, dir, velocity, speedScale);
	m_spawnDirection = dir;
}



void CHommingSwarmProjectile::Deflected(const Vec3& dir)
{
	m_controlLostTimer = 0.5f; // make parameter later
}

void CHommingSwarmProjectile::FullSerialize(TSerialize ser)
{
	CProjectile::FullSerialize(ser);

	ser.Value("m_controlLostTimer", m_controlLostTimer);
}



Vec3 CHommingSwarmProjectile::Seek(const SVehicleStatus& vehicle, const Vec3& targetPosition)
{
	const SHomingSwarmParams& params = *m_pAmmoParams->pHomingSwarmParams;
	const float hommingSpeed = params.hommingSpeed;
	const float hommingDelay = params.hommingDelay;

	if (targetPosition == Vec3(ZERO))
		return Vec3(ZERO);

	Vec3 seekVelocity = (targetPosition - vehicle.position).GetNormalizedSafe() * vehicle.speed;
	Vec3 seekForce = (seekVelocity - vehicle.velocity) * hommingSpeed;
	float seekMultiplier = SATURATE(m_totalLifetime/hommingDelay);
	seekForce *= seekMultiplier;

	return seekForce;
}



Vec3 CHommingSwarmProjectile::Wander(const SVehicleStatus& vehicle, float deltaTime)
{
	const SHomingSwarmParams& params = *m_pAmmoParams->pHomingSwarmParams;

	const float dist = params.wanderDistance;
	const float radius = params.wanderSphereRadius;
	const float randomness = params.wanderRand;
	const float frequency = params.wanderFrequency;
	const float dampingTime = params.wanderDampingTime;
	const float damping = SATURATE(m_totalLifetime/dampingTime);
	const float invFrequency = 1.0f / frequency;

	const float wanderDist = LERP(0.0f, dist, damping);
	const float wanderRadius = LERP(60.0f, radius, damping);
	const float wanderRand = LERP(1.0f, randomness, damping);

	m_wanderTimer += deltaTime;
	Vec3 result(ZERO);

	while (m_wanderTimer > invFrequency)
	{
		m_wanderTimer -= invFrequency;
		m_wanderState += cry_random_componentwise(Vec3(-wanderRadius), Vec3(wanderRadius));
		m_wanderState.Normalize();
		result += vehicle.front*wanderDist + m_wanderState*wanderRadius;
	}

	return result;
}



Vec3 CHommingSwarmProjectile::Curl(const SVehicleStatus& vehicle, float deltaTime)
{
	const SHomingSwarmParams& params = *m_pAmmoParams->pHomingSwarmParams;

	m_curlTime = m_curlTime + deltaTime*m_curlDirection;
	const float time = m_curlTime + m_curlShift * params.curlFrequency;
	const float delay = params.curlDelay + FLT_EPSILON;
	const float amplitude = params.curlAmplitude * SATURATE(m_curlTime/delay);

	Vec3 up = Vec3(0.0f, 0.0f, 1.0f);
	const Vec3 front = m_spawnDirection;
	const Vec3 right = up.Cross(front).GetNormalized();
	up = front.Cross(right).GetNormalized();

	Vec3 curlVec;
	curlVec.x =
		cos_tpl(params.curlFrequency * 3.1415f * 2.0f * time) *
		amplitude;
	curlVec.y =
		sin_tpl(params.curlFrequency * 3.1415f * 2.0f * time) *
		amplitude;

	Vec3 result = right * curlVec.x + up * curlVec.y;

	return result;
}



void CHommingSwarmProjectile::SetDestination(EntityId targetId)
{
	m_fixedTarget = targetId;
	if (m_fixedTarget == 0)
		m_hommingBehaviour = false;
}


void CHommingSwarmProjectile::ExplodeIfMissingTarget(const SVehicleStatus& vehicle, const Vec3& velocity, const Vec3& targetPosition)
{
	if (0.f < m_pAmmoParams->pHomingSwarmParams->targetMissExplodeDistance)
	{
		const float targetMissExplodeDistanceSquared = square(m_pAmmoParams->pHomingSwarmParams->targetMissExplodeDistance);
		const float distanceToTargetSquared = targetPosition.GetSquaredDistance(vehicle.position);
		if (distanceToTargetSquared < targetMissExplodeDistanceSquared)
		{
			const Vec3 futurePos = vehicle.position + (velocity.GetNormalized() * sqrt(distanceToTargetSquared));
			const float futureDistanceToTargetSquared = targetPosition.GetSquaredDistance(futurePos);
			if (distanceToTargetSquared < futureDistanceToTargetSquared)
			{
				Explode(CProjectile::SExplodeDesc(true));
			}
		}
	}
}
// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   FireCommand.cpp
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   -

 *********************************************************************/
#include "StdAfx.h"
#include "FireCommand.h"
#include "Puppet.h"
#include <CryMath/PNoise3.h>
#include "DebugDrawContext.h"
#include "AIPlayer.h"

#if defined(GetObject)
	#undef GetObject
#endif

//#pragma optimize("", off)
//#pragma inline_depth(0)

//====================================================================
// CFireCommandInstant
//====================================================================
CFireCommandInstant::CFireCommandInstant(IAIActor* pShooter)
	: m_pShooter(((CAIActor*)pShooter)->CastToCPuppet())
	, m_weaponBurstState(BURST_NONE)
	, m_weaponBurstTime(0.0f)
	, m_weaponBurstTimeScale(1.0f)
	, m_weaponBurstTimeWithoutShot(0.0f)
	, m_weaponBurstBulletCount(0)
	, m_curBurstBulletCount(0)
	, m_curBurstPauseTime(0.0f)
	, m_weaponTriggerTime(0.0f)
	, m_weaponTriggerState(true)
	, m_coverFireTime(0.0f)
	, m_ShotsCount(0)
{
}

//
//--------------------------------------------------------------------------------------------------------
void CFireCommandInstant::Reset()
{
	AIAssert(m_pShooter);
	m_weaponBurstState = BURST_NONE;
	m_weaponBurstTime = 0;
	m_weaponBurstTimeScale = 1;
	m_weaponBurstTimeWithoutShot = 0.0f;
	m_weaponBurstBulletCount = 0;
	m_curBurstBulletCount = 0;
	m_curBurstPauseTime = 0;
	m_weaponTriggerTime = 0;
	m_weaponTriggerState = true;

	m_drawFire.Reset();
}

//
//--------------------------------------------------------------------------------------------------------
EAIFireState CFireCommandInstant::Update(IAIObject* pITarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	float dt = GetAISystem()->GetFrameDeltaTime();

	m_coverFireTime = descriptor.coverFireTime * gAIEnv.CVars.RODCoverFireTimeMod;

	if (!canFire || !pITarget)
	{
		m_weaponBurstState = BURST_NONE;
		canFire = false;
	}

	EAimState aim = m_pShooter->GetAimState();
	if (aim != AI_AIM_READY && aim != AI_AIM_FORCED)
	{
		m_weaponBurstState = BURST_NONE;
		canFire = false;
	}

	bool canHit = m_drawFire.Update(dt, m_pShooter, pITarget, descriptor, outShootTargetPos, canFire);

	if (!canFire)
		return eAIFS_Off;

	const float clampAngle = DEG2RAD(5.0f);

	float FakeHitChance = gAIEnv.CVars.RODFakeHitChance;
	if (m_pShooter->m_targetZone == AIZONE_KILL)
		FakeHitChance = 1.0f;
	else if (gAIEnv.configuration.eCompatibilityMode != ECCM_GAME04)
	{
		if (!m_pShooter->HasFiringReactionTimePassed())
			FakeHitChance = 0.0f;
	}

	if (m_drawFire.GetState() != DrawFireEffect::Finished)
	{
		if (gAIEnv.CVars.DebugDrawFireCommand)
		{
			CDebugDrawContext dc;

			Vec3 shooter = m_pShooter->GetPos();

			float x, y, z;
			if (dc->ProjectToScreen(shooter.x, shooter.y, shooter.z, &x, &y, &z))
			{
				if (z >= 0.0f)
				{
					x *= (float)dc->GetWidth() * 0.01f;
					y *= (float)dc->GetHeight() * 0.01f;

					dc->Draw2dLabel(x, y + 10, 1.25f, Col_YellowGreen, true, "%s", "Draw Fire");
				}
			}
		}

		CAIObject* targetObject = static_cast<CAIObject*>(pITarget);
		CAIPlayer* player = pITarget ? pITarget->CastToCAIPlayer() : 0;

		const bool canFakeHit = cry_random(0.0f, 1.0f) <= FakeHitChance;
		const bool allowedToHit = gAIEnv.CVars.AllowedToHit && (!player || gAIEnv.CVars.AllowedToHitPlayer) && m_pShooter->IsAllowedToHitTarget();
		const bool shouldHit = m_pShooter->CanDamageTarget() || canFakeHit;

		if (m_pShooter->AdjustFireTarget(targetObject, outShootTargetPos, allowedToHit && shouldHit, descriptor.spreadRadius,
		                                 DEG2RAD(5.5f), &outShootTargetPos))
		{
			if (m_pShooter->IsFireTargetValid(outShootTargetPos, targetObject))
				return eAIFS_On;
		}

		return eAIFS_Off;
	}

	// Determine how we should handle a memory target fire
	if (!m_pShooter->CanMemoryFire() &&
	    fireMode != FIREMODE_CONTINUOUS && fireMode != FIREMODE_FORCED &&
	    fireMode != FIREMODE_PANIC_SPREAD && fireMode != FIREMODE_KILL)
	{
		m_weaponBurstState = BURST_NONE;

		return eAIFS_Off;
	}

	const int burstBulletCountMin = descriptor.burstBulletCountMin;
	const int burstBulletCountMax = descriptor.burstBulletCountMax;
	const float burstPauseTimeMin = descriptor.burstPauseTimeMin;
	const float burstPauseTimeMax = descriptor.burstPauseTimeMax;
	const float singleFireTriggerTime = descriptor.singleFireTriggerTime;

	bool singleFireMode = singleFireTriggerTime > 0.0f;

	if (singleFireMode)
	{
		m_weaponTriggerTime -= dt;

		if (m_weaponTriggerTime <= 0.0f)
		{
			m_weaponTriggerState = !m_weaponTriggerState;
			m_weaponTriggerTime += singleFireTriggerTime * cry_random(0.75f, 1.25f);
		}
	}
	else
	{
		m_weaponTriggerTime = 0.0f;
		m_weaponTriggerState = true;
	}

	// Burst control
	if (fireMode == FIREMODE_BURST || fireMode == FIREMODE_BURST_DRAWFIRE ||
	    fireMode == FIREMODE_BURST_WHILE_MOVING || fireMode == FIREMODE_BURST_SNIPE)
	{
		float distScale = m_pShooter->GetBurstFireDistanceScale();

		const float lostFadeMinTime = m_coverFireTime * 0.25f;
		const float lostFadeMaxTime = m_coverFireTime;
		const int fadeSteps = 6;

		float a = 1.0f;
		float fade = clamp_tpl((m_pShooter->m_targetLostTime - lostFadeMinTime) / max(lostFadeMaxTime - lostFadeMinTime, FLT_EPSILON), 0.0f, 1.0f);
		a *= 1.0f - floorf(fade * fadeSteps) / (float)fadeSteps; // more lost, less bullets
		a *= distScale;                                          // scaling based on the zone (distance)
		a *= m_pShooter->IsAllowedToHitTarget() ? 1.0f : 0.2f;
		m_curBurstBulletCount = (int)(burstBulletCountMin + (burstBulletCountMax - burstBulletCountMin) * a * m_weaponBurstTimeScale);
		m_curBurstPauseTime = burstPauseTimeMin + (1.0f - sqr(a) * 0.75f) * (burstPauseTimeMax - burstPauseTimeMin) * m_weaponBurstTimeScale;

		float chargeTime = std::max(0.0f, descriptor.fChargeTime);

		if (m_weaponBurstState == BURST_NONE)
		{
			// Init
			m_weaponBurstTime = 0.0f;
			m_weaponBurstTimeScale = cry_random(1, 6) / 6.0f;
			m_weaponBurstState = BURST_FIRE;
			m_weaponBurstBulletCount = 0;
			m_weaponBurstTimeWithoutShot = 0;

			m_pShooter->HandleBurstFireInit();
		}
		else if (m_weaponBurstState == BURST_FIRE)
		{
			int shotCount = m_pShooter->GetProxy()->GetAndResetShotBulletCount();
			m_weaponBurstBulletCount += shotCount;

			m_weaponBurstTimeWithoutShot = shotCount ? 0.0f : (m_weaponBurstTimeWithoutShot + dt);

			if (m_weaponBurstBulletCount >= m_curBurstBulletCount)
			{
				if (m_curBurstPauseTime > 0.0f)
					m_weaponBurstState = BURST_PAUSE;
			}
			else if (!singleFireMode && !shotCount && (m_weaponBurstTimeWithoutShot >= (chargeTime + m_curBurstPauseTime)))
			{
				m_weaponBurstState = BURST_PAUSE;
			}
		}
		else
		{
			// Wait
			m_weaponBurstTime += dt;
			if (m_weaponBurstTime >= m_curBurstPauseTime)
				m_weaponBurstState = BURST_NONE;
		}
	}
	else
	{
		// Allow to fire always.
		m_weaponBurstState = BURST_FIRE;
	}

	if (m_weaponBurstState != BURST_FIRE)
	{
		m_weaponTriggerTime = 0;
		m_weaponTriggerState = true;
		return eAIFS_Off;
	}

	if (!m_weaponTriggerState)
		return eAIFS_Off;

	CAIObject* targetObject = static_cast<CAIObject*>(pITarget);
	CAIPlayer* player = pITarget ? pITarget->CastToCAIPlayer() : 0;

	float burstCanStartHitPercent = 0.0f;

	switch (m_pShooter->m_targetZone)
	{
	case AIZONE_OUT:
		if (pITarget)
			burstCanStartHitPercent = 0.75f;
		break;
	case AIZONE_WARN:
		burstCanStartHitPercent = 0.75f;
		break;
	case AIZONE_COMBAT_FAR:
		burstCanStartHitPercent = 0.65f;
		break;
	case AIZONE_COMBAT_NEAR:
		burstCanStartHitPercent = 0.35f;
		break;
	default:
		break;
	}

	const size_t burstFirstHit = (size_t)((m_curBurstBulletCount * burstCanStartHitPercent) + 0.5f);
	const bool canFakeHit = cry_random(0.0f, 1.0f) <= FakeHitChance;
	const bool allowedToHit = gAIEnv.CVars.AllowedToHit && (!player || gAIEnv.CVars.AllowedToHitPlayer) && m_pShooter->IsAllowedToHitTarget();
	const bool shouldHit = m_pShooter->CanDamageTarget() || canFakeHit;
	const bool burstHit = (size_t)m_weaponBurstBulletCount >= burstFirstHit;

	if (m_pShooter->AdjustFireTarget(targetObject, outShootTargetPos, allowedToHit && shouldHit && burstHit,
	                                 descriptor.spreadRadius, DEG2RAD(5.5f), &outShootTargetPos))
		return eAIFS_On;

	return eAIFS_Off;
}

void CFireCommandInstant::DebugDraw()
{
	if (!m_pShooter) return;

	const float lostFadeMinTime = m_coverFireTime * 0.25f;
	const float lostFadeMaxTime = m_coverFireTime;
	const int fadeSteps = 6;

	float a = 1.0f;
	float fade = clamp_tpl((m_pShooter->m_targetLostTime - lostFadeMinTime) / (lostFadeMaxTime - lostFadeMinTime), 0.0f, 1.0f);
	a *= 1.0f - floorf(fade * fadeSteps) / (float)fadeSteps;  // more lost, less bullets

	CDebugDrawContext dc;
	dc->Draw3dLabel(m_pShooter->GetFirePos() - Vec3(0, 0, 1.5f), 1, "Weapon\nShot:%d/%d\nWait:%.2f/%.2f\nA=%f", m_weaponBurstBulletCount, m_curBurstBulletCount, m_weaponBurstTime, m_curBurstPauseTime, a);
}

float CFireCommandInstant::GetTimeToNextShot() const
{
	return m_curBurstPauseTime;
}

CFireCommandInstant::DrawFireEffect::EState CFireCommandInstant::DrawFireEffect::GetState() const
{
	return m_state.state;
}

bool CFireCommandInstant::DrawFireEffect::Update(float updateTime, CPuppet* pShooter, IAIObject* pTarget, const AIWeaponDescriptor& descriptor,
                                                 Vec3& aimTarget, bool canFire)
{
	if (gAIEnv.CVars.DrawFireEffectEnabled < 1)
	{
		m_state.state = Finished;
		return true;
	}

	if (!pTarget || !pTarget->CastToCAIPlayer())
	{
		m_state.state = Finished;
		return true;
	}

	const float tooClose = gAIEnv.CVars.DrawFireEffectMinDistance;
	const Vec3& firePos = pShooter->GetFirePos();
	const Vec3& targetPos = pTarget->GetPos();
	float distanceSq = Distance::Point_PointSq(targetPos, firePos);

	if (distanceSq <= sqr(tooClose))
	{
		m_state.state = Finished;
		return true;
	}

	float denom = gAIEnv.CVars.DrawFireEffectDecayRange - tooClose;
	float distance = sqrt_tpl(distanceSq);
	float distanceFactor = (distance - tooClose) / max(denom, 1.0f);
	distanceFactor = clamp_tpl(distanceFactor, 0.0f, 1.0f);

	float fovCos = cos_tpl(DEG2RAD(gAIEnv.CVars.DrawFireEffectMinTargetFOV * 0.5f));

	Vec3 viewTarget = (firePos - targetPos) * (1.0f / distance);
	Vec3 targetViewDir = pTarget->GetViewDir();
	float viewAngleCos = viewTarget.dot(targetViewDir);

	float fovFactor = 0.0f;
	if (viewAngleCos < fovCos)
		fovFactor = 2.5f * (1.0f - viewAngleCos) / fovCos;
	fovFactor = clamp_tpl(fovFactor, 0.0f, 1.0f);

	float drawTime = gAIEnv.CVars.DrawFireEffectTimeScale * descriptor.drawTime;
	drawTime = (0.75f * distanceFactor) * drawTime + (0.25f * fovFactor) * drawTime;

	if (drawTime < 0.125f)
	{
		m_state.state = Finished;
		return true;
	}

	if (canFire)
		m_state.time += updateTime;
	else
		m_state.idleTime += updateTime;

	if (canFire && drawTime > 0.0f)
	{
		if (m_state.idleTime >= descriptor.burstPauseTimeMax + 0.15f)
			m_state = State();
		m_state.idleTime = 0.0f;

		if (m_state.time > drawTime)
		{
			m_state.state = Finished;
			return true;
		}

		float targetHeight = 1.75f;
		if (CAIActor* pTargetActor = pTarget->CastToCAIActor())
			targetHeight = targetPos.z - pTargetActor->GetPhysicsPos().z;

		Vec3 targetToShooter = firePos - pTarget->GetPos();
		targetToShooter.z = 0.0f;

		float distance2D = targetToShooter.NormalizeSafe();
		float t = clamp_tpl(m_state.time / drawTime, 0.0f, 1.0f);
		float smoothedInvT = sqr(1.0f - sqr(t));
		float smoothedT = 1.0f - smoothedInvT;

		CPNoise3* noiseGen = gEnv->pSystem->GetNoiseGen();

		float noiseScale = 3.0f * smoothedInvT;
		float noise = noiseScale * noiseGen->Noise1D(m_state.startSeed + m_state.time * descriptor.sweepFrequency);

		Vec3 right(targetToShooter.y, -targetToShooter.x, 0.0f);
		Vec3 front(targetViewDir);
		front.z = clamp_tpl(front.z, -0.35f, 0.35f);
		front.Normalize();

		float startDistance = distance2D - 5.0f;
		float angleDecay = clamp_tpl((distance - gAIEnv.CVars.DrawFireEffectMinDistance) / gAIEnv.CVars.DrawFireEffectDecayRange, 0.0f, 1.0f);
		float maxAngle = DEG2RAD(gAIEnv.CVars.DrawFireEffectMaxAngle) * angleDecay;

		float angle = fabs_tpl(atan2_tpl(targetHeight, startDistance));
		angle = clamp_tpl(angle, 0.0f, maxAngle);
		float verticalOffset = tan_tpl(angle) * distance;

		float targetZ = aimTarget.z;
		float startZ = aimTarget.z - verticalOffset;

		aimTarget += right * (noise * descriptor.sweepWidth);
		aimTarget.z = startZ + (targetZ - startZ) * smoothedT;

		float frontFactor = (fovFactor * smoothedInvT * descriptor.sweepWidth);
		aimTarget += front * frontFactor;

		bool canHit = false;
		if ((aimTarget.z >= (targetPos.z - targetHeight)) && (frontFactor <= 0.35f))
			canHit = true;

		if (aimTarget.z > targetPos.z + 0.05f)
			aimTarget.z -= 2.0f * (aimTarget.z - targetPos.z);

		m_state.state = Running;

		if (gAIEnv.CVars.DebugDrawAmbientFire)
		{
			float terrainZ = gEnv->p3DEngine->GetTerrainElevation(aimTarget.x, aimTarget.y);

			GetAISystem()->AddDebugSphere(Vec3(aimTarget.x, aimTarget.y, terrainZ + 0.075f), 0.175f, 106, 90, 205, 1.5f);
			GetAISystem()->AddDebugSphere(targetPos, 0.25f, 255, 0, 0, 1.5f);
		}

		return canHit;
	}

	return true;
}

void CFireCommandInstant::DrawFireEffect::Reset()
{
	m_state = State();
}

//====================================================================
// CFireCommandLob
//====================================================================
CFireCommandLob::CFireCommandLob(IAIActor* pShooter)
	: m_pShooter(((CAIActor*)pShooter)->CastToCPuppet())
	, m_lastStep(0)
	, m_targetPos(ZERO)
	, m_throwDir(ZERO)
	, m_nextFireTime(0.0f)
	, m_preferredHeight(0.0f)
	, m_projectileSpeedScale(0.0f)
	, m_bestScore(-1)
{
#ifdef CRYAISYSTEM_DEBUG
	m_debugBest = 0.0f;
#endif //CRYAISYSTEM_DEBUG

	assert(m_pShooter);
}

//
//---------------------------------------------------------------------------------------------------------------
CFireCommandLob::~CFireCommandLob()
{
	ClearDebugEvals();
}

//
//---------------------------------------------------------------------------------------------------------------
void CFireCommandLob::Reset()
{
	m_lastStep = 0;
	m_targetPos.zero();
	m_throwDir.zero();
	m_nextFireTime = 0.0f;
	m_preferredHeight = 0.0f;
	m_projectileSpeedScale = 0.0f;
	m_bestScore = -1;
}

//
//---------------------------------------------------------------------------------------------------------------
void CFireCommandLob::ClearDebugEvals()
{
#ifdef CRYAISYSTEM_DEBUG
	m_DEBUG_evals.clear();
	m_debugBest = 0.0f;
#endif //CRYAISYSTEM_DEBUG
}

//
//---------------------------------------------------------------------------------------------------------------
EAIFireState CFireCommandLob::Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	EAIFireState state = eAIFS_Off;

	if (canFire && pTarget && m_pShooter)
	{
		const CTimeValue currTime = GetAISystem()->GetFrameStartTime();

		// First fire should wait for the charge time
		if (m_nextFireTime <= 0.0f)
		{
			m_nextFireTime = currTime + descriptor.fChargeTime;
		}

		// Keep testing lob path if we've already started or if enough time has past since last fire
		bool bGetBestLob = (0 != m_lastStep || m_nextFireTime <= currTime);
		if (bGetBestLob)
		{
			const EAimState aim = m_pShooter->GetAimState();
			const bool bValidAim = (aim == AI_AIM_READY || aim == AI_AIM_FORCED || !IsUsingPrimaryWeapon());
			if (bValidAim)
			{
				state = GetBestLob(pTarget, descriptor, outShootTargetPos);
				if (state != eAIFS_Blocking)
				{
					// Calculate next fire time
					const float fNextFireTime = cry_random(descriptor.burstPauseTimeMin, descriptor.burstPauseTimeMax);
					m_nextFireTime = currTime + fNextFireTime;
					m_lastStep = 0;
				}
				else
				{
					outShootTargetPos = (m_throwDir.IsZero()) ? pTarget->GetPos() : m_throwDir;
				}
			}
			else
			{
				// Let agent take aim first
				outShootTargetPos = pTarget->GetPos();
				state = eAIFS_Blocking;
			}
		}
	}
	else
	{
		m_nextFireTime = 0.0f;
	}

	return state;
}

//
//---------------------------------------------------------------------------------------------------------------
EAIFireState CFireCommandLob::GetBestLob(IAIObject* pTarget, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	assert(m_pShooter);
	assert(pTarget);

	const Vec3 vCurrTargetPos = pTarget->GetPos();
	const Vec3 vCurrShooterPos = m_pShooter->GetPos();
	const Vec3 vCurrShooterFirePos = m_pShooter->GetFirePos();

	if (0 == m_lastStep)
	{
		// First iteration, setup tests
		ClearDebugEvals();

		// Calculate target offset.
		Vec3 targetDir2D(vCurrTargetPos - vCurrShooterPos);
		targetDir2D.z = 0.0f;
		/*
		                                                     ^
		   |targetRightDirection

		 |
		 |
		   shooter ---<distToTarget>------------------   ----target---- (parallelRadius)          -> targetDir2D
		 |
		 | perpendicularRadius
		 */
		const float distScale = m_pShooter->GetParameters().m_fProjectileLaunchDistScale;
		const float distToTarget = targetDir2D.NormalizeSafe();
		const float inaccuracyRadius = distToTarget * gAIEnv.CVars.LobThrowPercentageOfDistanceToTargetUsedForInaccuracySimulation;

		const float randomPercentageForPerpendicularRadius = gAIEnv.CVars.LobThrowSimulateRandomInaccuracy ? cry_random(0.0f, 1.0f) : 0.0f;
		float perpendicularRadius = -inaccuracyRadius + randomPercentageForPerpendicularRadius * (2 * inaccuracyRadius);

		const float randomPercentageForParallelRadius = gAIEnv.CVars.LobThrowSimulateRandomInaccuracy ? cry_random(0.0f, 1.0f) : 0.0f;
		float parallelRadius = randomPercentageForParallelRadius * inaccuracyRadius + (1.0f - distScale) * distToTarget;

		Vec3 targetRightDirection(-targetDir2D.y, targetDir2D.x, 0.0f);
		m_targetPos = vCurrTargetPos + targetRightDirection * perpendicularRadius - targetDir2D * parallelRadius;

		const float minZOffsetForLob = (distToTarget > descriptor.closeDistance) ?
		                               descriptor.preferredHeight : descriptor.preferredHeightForCloseDistance;
		m_preferredHeight = max(minZOffsetForLob, m_targetPos.z - vCurrShooterPos.z + minZOffsetForLob);

		m_bestScore = -1.0f;
	}

	// Flat target dir
	Vec3 targetDir2D = m_targetPos - vCurrShooterPos;
	targetDir2D.z = 0;
#ifdef CRYAISYSTEM_DEBUG
	m_Apex = vCurrShooterFirePos + (0.5 * targetDir2D);
#endif
	const float distToTarget = targetDir2D.NormalizeSafe();

	const float minimumDistanceToFriendsSq = sqr(descriptor.minimumDistanceFromFriends >= 0.0f ?
	                                             descriptor.minimumDistanceFromFriends : gAIEnv.CVars.LobThrowMinAllowedDistanceFromFriends);

	const unsigned maxStepsPerUpdate = 1;
	const unsigned maxSteps = 5;
	for (unsigned iStep = 0; iStep < maxStepsPerUpdate && m_lastStep < maxSteps; ++iStep)
	{
		const float u = (float)m_lastStep / (float)(maxSteps - 1) - 0.25f;
		const Vec3 fireDir = m_targetPos - vCurrShooterFirePos;
		const float x = targetDir2D.Dot(fireDir) - u * distToTarget * 0.25f;
		const float y = fireDir.z;
		const float h = max(1.0f, y + m_preferredHeight);
#ifdef CRYAISYSTEM_DEBUG
		m_Apex.z += h;
#endif
		const float g = descriptor.projectileGravity.z;

		// Try good solution
		const float vy = sqrt_tpl(-h * g);
		const float det = sqr(vy) + 2 * g * y;
		if (det >= 0.0f)
		{
			const float tx = (-vy - sqrt_tpl(det)) / g;
			const float vx = x / tx;

			Vec3 dir = targetDir2D * vx + Vec3(0, 0, vy);
			const float plannedLaunchSpeed = dir.NormalizeSafe();

			Vec3 throwHitPos(ZERO);
			Vec3 throwHitDir(ZERO);
			float throwSpeed = 1.0f;
			Trajectory trajectory;
			const float score = EvaluateThrow(m_targetPos, pTarget->GetViewDir(), dir, plannedLaunchSpeed,
			                                  throwHitPos, throwHitDir, throwSpeed, trajectory, descriptor.maxAcceptableDistanceFromTarget,
			                                  minimumDistanceToFriendsSq);

			if (score >= 0.0f && (score < m_bestScore || m_bestScore < 0.0f))
			{
				const float throwDistance = Distance::Point_Point(vCurrShooterPos, throwHitPos);
				m_throwDir = vCurrShooterPos + throwHitDir * throwDistance;
				m_projectileSpeedScale = plannedLaunchSpeed / throwSpeed;
				m_bestScore = score;
				m_bestTrajectory = trajectory;
			}

			++m_lastStep;
		}
	}

	if (m_lastStep >= maxSteps)
	{
		if (m_bestScore <= 0.0f)
		{
			return eAIFS_Off;
		}
		else if (m_pShooter->CanAimWithoutObstruction(m_throwDir))
		{
#ifdef CRYAISYSTEM_DEBUG
			m_debugBest = m_bestScore;
#endif //CRYAISYSTEM_DEBUG

			if (!outShootTargetPos.IsZero() || CanHaveZeroTargetPos())
			{
				const Vec3& positionToLobTowards = m_throwDir;
				outShootTargetPos = positionToLobTowards;

				Vec3 launchPosition = m_bestTrajectory.GetSample(0).position;
				Vec3 launchVelocity = (positionToLobTowards - launchPosition).GetNormalized();

				// Step through the trajectory samples until we leave the
				// critical zone (defined as "within 5 meters of the shooter").
				// Once the projectile has left the critical zone we'll spawn the
				// projectile with the sampled position and velocity.
				// This eliminates most cases in which the grenade would hit the
				// cover surface or a close wall, detonate, and kill the shooter.

				const Vec3& shooterPosition = m_pShooter->GetPos();

				for (size_t i = 0, n = m_bestTrajectory.GetSampleCount(); i < n; ++i)
				{
					const Trajectory::Sample& sample = m_bestTrajectory.GetSample(i);

					const float shooterToSampleDistanceSq = shooterPosition.GetSquaredDistance(sample.position);
					const float criticalDistanceSq = square(descriptor.lobCriticalDistance);

					if (shooterToSampleDistanceSq > criticalDistanceSq)
					{
						launchPosition = sample.position;
						launchVelocity = sample.velocity;
						break;
					}
				}

				// Pass this information along to the AI Proxy
				m_pShooter->m_State.vLobLaunchPosition = launchPosition;
				m_pShooter->m_State.vLobLaunchVelocity = launchVelocity;

				return eAIFS_On;
			}
			else
			{
				outShootTargetPos.zero();
				return eAIFS_Off;
			}
		}
	}

	return eAIFS_Blocking;
}

//
//---------------------------------------------------------------------------------------------------------------
float CFireCommandLob::EvaluateThrow(const Vec3& targetPos, const Vec3& targetViewDir, const Vec3& dir, float vel,
                                     Vec3& outThrowHitPos, Vec3& outThrowHitDir, float& outSpeed,
                                     Trajectory& trajectory, const float maxAllowedDistanceFromTarget,
                                     const float minimumDistanceToFriendsSq) const
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);
	CCCPOINT(CFireCommandLob_EvaluateThisThrow);

	IAIActorProxy* pPuppetProxy = m_pShooter->GetProxy();

	trajectory.sampleCount = trajectory.MaxSamples;

	outThrowHitPos.zero();
	outThrowHitDir.zero();
	const bool bUsingPrimaryWeapon = IsUsingPrimaryWeapon();
	const ERequestedGrenadeType eReqGrenadeType = bUsingPrimaryWeapon ? eRGT_INVALID : m_pShooter->GetState().requestedGrenadeType;

	bool bPredicted = false;
	if (bUsingPrimaryWeapon)
	{
		outThrowHitDir = dir;
		bPredicted = pPuppetProxy->PredictProjectileHit(vel, outThrowHitPos, outThrowHitDir, outSpeed, &trajectory.positions[0], &trajectory.sampleCount, &trajectory.velocities[0]);
	}
	else
	{
		bPredicted = pPuppetProxy->PredictProjectileHit(dir, vel, outThrowHitPos, outSpeed, eReqGrenadeType, &trajectory.positions[0], &trajectory.sampleCount, &trajectory.velocities[0]);
		outThrowHitDir = dir;
	}

	float score = -1.0f;

	if (bPredicted && IsValidDestination(eReqGrenadeType, outThrowHitPos, minimumDistanceToFriendsSq))
	{
		const Vec3& shooterPos = m_pShooter->GetPos();
		const Vec3 targetToHit = outThrowHitPos - targetPos;

		// Prefer positions that are close to the target
		const float distanceSq = targetToHit.GetLengthSquared();
		const bool acceptThisThrow = maxAllowedDistanceFromTarget < 0 || distanceSq < sqr(maxAllowedDistanceFromTarget);
		if (acceptThisThrow)
			score = distanceSq;
	}

#ifdef CRYAISYSTEM_DEBUG
	m_DEBUG_evals.push_back(SDebugThrowEval());
	SDebugThrowEval* pDebugEval = &m_DEBUG_evals.back();

	pDebugEval->score = 0.0f;
	pDebugEval->fake = false;
	pDebugEval->pos = outThrowHitPos;
	pDebugEval->score = score;

	const size_t sampleCount = trajectory.GetSampleCount();
	pDebugEval->trajectory.resize(sampleCount);
	for (size_t i = 0; i < sampleCount; ++i)
	{
		pDebugEval->trajectory[i] = trajectory.GetSample(i).position;
	}

#endif //CRYAISYSTEM_DEBUG

	return score;
}

//
//---------------------------------------------------------------------------------------------------------------
bool CFireCommandLob::IsValidDestination(ERequestedGrenadeType eReqGrenadeType, const Vec3& throwHitPos,
                                         const float minimumDistanceToFriendsSq) const
{
	bool bValid = true;

	// no friends proximity checks for smoke grenades
	if (eRGT_SMOKE_GRENADE != eReqGrenadeType)
	{
		// make sure not to throw grenade near NOGRENADE_SPOT
		AutoAIObjectIter itAnchors(gAIEnv.pAIObjectManager->GetFirstAIObject(OBJFILTER_TYPE, AIANCHOR_NOGRENADE_SPOT));
		IAIObject* pAvoidAnchor = itAnchors->GetObject();
		while (bValid && (pAvoidAnchor != NULL))
		{
			if (pAvoidAnchor->IsEnabled())
			{
				float avoidDistSq(pAvoidAnchor->GetRadius());
				avoidDistSq *= avoidDistSq;

				const float checkDistance = Distance::Point_PointSq(throwHitPos, pAvoidAnchor->GetPos());
				bValid = (checkDistance >= avoidDistSq);
			}

			// Advance
			itAnchors->Next();
			pAvoidAnchor = itAnchors->GetObject();
		}

		// Check friendly distance
		const float timePrediction = gAIEnv.CVars.LobThrowTimePredictionForFriendPositions;
		AutoAIObjectIter itFriend(gAIEnv.pAIObjectManager->GetFirstAIObject(OBJFILTER_FACTION, m_pShooter->GetFactionID()));
		IAIObject* pFriend = itFriend->GetObject();
		while (bValid && (pFriend != NULL))
		{
			if (pFriend->IsEnabled())
			{
				Vec3 friendPositionToEvaluate = pFriend->GetPos();
				if (CPipeUser* pPipeUser = CastToCPipeUserSafe(pFriend))
				{
					const Vec3& moveDir = pPipeUser->GetMoveDir();
					if (!moveDir.IsZero())
					{
						friendPositionToEvaluate += moveDir * timePrediction;
					}
				}

				const float checkDistanceSq = Distance::Point_PointSq(throwHitPos, friendPositionToEvaluate);
				bValid = (checkDistanceSq >= minimumDistanceToFriendsSq);
			}

			// Advance
			itFriend->Next();
			pFriend = itFriend->GetObject();
		}
	}

	return bValid;
}

//
//---------------------------------------------------------------------------------------------------------------
void CFireCommandLob::DebugDraw()
{
#ifdef CRYAISYSTEM_DEBUG
	CDebugDrawContext dc;

	const ColorB colWhite(255, 255, 255);
	const ColorB colRed(255, 0, 0);
	const ColorB colGreen(0, 255, 0);

	TDebugEvals::const_iterator itDebug = m_DEBUG_evals.begin();
	TDebugEvals::const_iterator itDebugEnd = m_DEBUG_evals.end();
	for (; itDebug != itDebugEnd; ++itDebug)
	{
		const SDebugThrowEval& eval = *itDebug;
		const bool bBest = (m_debugBest >= 0.0f && eval.score == m_debugBest);

		dc->DrawSphere(eval.pos, 0.2f, bBest ? colGreen : colRed);
		dc->Draw3dLabel(eval.pos, bBest ? 1.2f : 0.8f, "%.1f%s", eval.score, bBest ? " [BEST]" : "");
		dc->DrawPolyline(&eval.trajectory[0], eval.trajectory.size(), false, eval.fake ? colRed : colWhite, bBest ? 2.0f : 1.25f);
	}
	dc->DrawSphere(m_Apex, 0.2f, Col_Black);
#endif //CRYAISYSTEM_DEBUG
}

float CFireCommandLob::GetTimeToNextShot() const
{
	return 0.0f;
}

bool CFireCommandLob::GetOverrideAimingPosition(Vec3& overrideAimingPosition) const
{
	if (!m_throwDir.IsZero())
	{
		overrideAimingPosition = m_throwDir;
		return true;
	}
	return false;
}

//====================================================================
// CFireCommandProjectileSlow
//====================================================================
CFireCommandProjectileSlow::CFireCommandProjectileSlow(IAIActor* pShooter) : CFireCommandLob(pShooter)
{

}

//====================================================================
// CFireCommandProjectileFast
//====================================================================
CFireCommandProjectileFast::CFireCommandProjectileFast(IAIActor* pShooter)
	: m_pShooter(((CAIActor*)pShooter)->CastToCPuppet())
	, m_aimPos(ZERO)
	, m_trackingId(0)
{
}

//
//---------------------------------------------------------------------------------------------------------------
void CFireCommandProjectileFast::Reset()
{
	m_trackingId = 0;
}

//
//---------------------------------------------------------------------------------------------------------------
EAIFireState CFireCommandProjectileFast::Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	SAIWeaponInfo weaponInfo;
	m_pShooter->GetProxy()->QueryWeaponInfo(weaponInfo);

	EAIFireState state = eAIFS_Off;

	if (canFire && pTarget && !weaponInfo.shotIsDone && m_pShooter->IsAllowedToHitTarget())
	{
		const EAimState aim = m_pShooter->GetAimState();
		const bool bValidAim = (aim == AI_AIM_READY || aim == AI_AIM_FORCED);

		if (bValidAim)
		{
			const Vec3& vTargetPos = pTarget->GetPos();
			const Vec3& vTargetVel = pTarget->GetVelocity();
			const float aimAheadTime(0.5f);

			// Set the general aim position
			m_aimPos = vTargetPos + vTargetVel * aimAheadTime;

			if (fireMode == FIREMODE_FORCED)
			{
				ChooseShootPoint(outShootTargetPos, pTarget, descriptor.fDamageRadius, 0.4f, CFireCommandProjectileFast::AIM_INFRONT);
				state = eAIFS_On;
			}
			else
			{
				// Try different aim points, prefer front points if possible
				bool bValidShootPoint = false;
				bValidShootPoint = bValidShootPoint || ChooseShootPoint(outShootTargetPos, pTarget, descriptor.fDamageRadius, 0.4f, CFireCommandProjectileFast::AIM_INFRONT);
				bValidShootPoint = bValidShootPoint || ChooseShootPoint(outShootTargetPos, pTarget, descriptor.fDamageRadius, 0.8f, CFireCommandProjectileFast::AIM_SIDES);
				bValidShootPoint = bValidShootPoint || ChooseShootPoint(outShootTargetPos, pTarget, descriptor.fDamageRadius, 0.8f, CFireCommandProjectileFast::AIM_BACK);

				state = (bValidShootPoint ? eAIFS_On : eAIFS_Off);
			}
		}
		else
		{
			// Let agent take aim first
			outShootTargetPos = pTarget->GetPos();
			state = eAIFS_Blocking;
		}
	}

	return state;
}

//
//---------------------------------------------------------------------------------------------------------------
bool CFireCommandProjectileFast::ChooseShootPoint(Vec3& outShootPoint, IAIObject* pTarget, float explRadius, float missRatio, CFireCommandProjectileFast::EAimMode aimMode)
{
	assert(m_pShooter);
	assert(pTarget);

	outShootPoint.zero();
	Vec3 shootAtPos = m_aimPos;
	float fAccuracy = 1.0f;

	if (m_trackingId != 0)
	{
		aimMode = AIM_DIRECTHIT;
	}
	else
	{
		const bool bCanDamageTarget = (m_pShooter->GetAttentionTarget() == pTarget ? m_pShooter->CanDamageTarget() : true);
		fAccuracy = (bCanDamageTarget ? m_pShooter->GetAccuracy(static_cast<CAIObject*>(pTarget)) : 0.f);
		if (cry_random(0.0f, 1.0f) <= fAccuracy)
			aimMode = AIM_DIRECTHIT;
	}

	// Deal with eye-height for aiming at feed
	if (AIM_INFRONT == aimMode && AIOBJECT_VEHICLE != static_cast<CAIObject*>(pTarget)->GetType())
	{
		CAIActor* pTargetActor(CastToCAIActorSafe(pTarget));
		if (pTargetActor)
		{
			const SAIBodyInfo& bodyInfo = pTargetActor->GetBodyInfo();
			shootAtPos.z -= bodyInfo.stanceSize.GetSize().z;
		}
	}
	else if (AIM_DIRECTHIT == aimMode)
	{
		shootAtPos = pTarget->GetPos();
		m_trackingId = pTarget->GetEntityID();
	}
	else
	{
		Vec3 targetForward2D(pTarget->GetEntityDir());
		targetForward2D.z = 0.0f;

		// if target looks down - use direction to shooter as forward
		if (targetForward2D.IsZero(.07f))
		{
			targetForward2D = m_pShooter->GetPos() - pTarget->GetPos();
			targetForward2D.z = 0.0f;
		}

		targetForward2D.NormalizeSafe(Vec3Constants<float>::fVec3_OneY);

		const float shotMissOffset = explRadius * missRatio * (1.0f - fAccuracy);
		const Vec3 shootDir(-targetForward2D.y, targetForward2D.x, 0.0f);
		switch (aimMode)
		{
		case AIM_INFRONT:
			{
				const float leftRightOffset = cry_random(-0.2f, 0.2f);
				shootAtPos += targetForward2D * shotMissOffset + shootDir * shotMissOffset * leftRightOffset;
			}
			break;

		case AIM_SIDES:
			{
				const float leftRightOffset = cry_random(0.5f, 1.0f) * (cry_random(0, 1) == 0 ? 0.4f : -0.4f);
				shootAtPos += targetForward2D * shotMissOffset * 0.1f + shootDir * shotMissOffset * leftRightOffset;
			}
			break;

		case AIM_BACK:
			{
				const float leftRightOffset = cry_random(0.5f, 1.0f) * (cry_random(0, 1) == 0 ? 0.1f : -0.1f);
				shootAtPos += -targetForward2D * shotMissOffset + shootDir * shotMissOffset * leftRightOffset;
			}
			break;

		default:
			CRY_ASSERT_MESSAGE(false, "CFireCommandProjectileFast::ChooseShootPoint Unhandled Aim Mode");
		}
	}

	// Use floor position if possible
	const bool bHasFloorPos = GetFloorPos(outShootPoint, shootAtPos, WalkabilityFloorUpDist, WalkabilityFloorDownDist, WalkabilityDownRadius, AICE_STATIC);
	if (bHasFloorPos)
		outShootPoint.z += 0.5f;
	else
		outShootPoint = shootAtPos;

	// Validate the final position
	const bool bValid = IsValidShootPoint(m_pShooter->GetFirePos(), outShootPoint, explRadius);
	return bValid;
}

//
//---------------------------------------------------------------------------------------------------------------
void CFireCommandProjectileFast::OnReload()
{
	m_trackingId = 0;
}

//
//---------------------------------------------------------------------------------------------------------------
bool CFireCommandProjectileFast::IsValidShootPoint(const Vec3& firePos, const Vec3& shootPoint, float explRadius) const
{
	bool bResult = false;

	const Vec3 vFireDir = (shootPoint - firePos);
	const Vec3 vTargetPos = firePos + vFireDir;
	if (m_pShooter->CanAimWithoutObstruction(vTargetPos))
	{
		// Check for friendlies in the area
		bResult = NoFriendNearAimTarget(explRadius, shootPoint);
	}

	return bResult;
}

//
//---------------------------------------------------------------------------------------------------------------
bool CFireCommandProjectileFast::NoFriendNearAimTarget(float explRadius, const Vec3& shootPoint) const
{
	assert(m_pShooter);

	bool bValid = true;
	const float minAllowedDistanceForFriendsSq = sqr(1.5f * explRadius);

	AutoAIObjectIter itFriend(gAIEnv.pAIObjectManager->GetFirstAIObject(OBJFILTER_FACTION, m_pShooter->GetFactionID()));
	IAIObject* pFriend = itFriend->GetObject();
	while (bValid && (pFriend != NULL))
	{
		if (pFriend->IsEnabled())
		{
			const float distanceSq = Distance::Point_PointSq(shootPoint, pFriend->GetPos());
			bValid = (distanceSq >= minAllowedDistanceForFriendsSq);
		}

		// Advance
		itFriend->Next();
		pFriend = itFriend->GetObject();
	}

	return bValid;
}

//
//---------------------------------------------------------------------------------------------------------------
void CFireCommandProjectileFast::DebugDraw()
{
	if (m_pShooter)
	{
		CDebugDrawContext dc;
		const ColorB color(255, 255, 255, 128);
		dc->DrawLine(m_pShooter->GetFirePos() + Vec3(0, 0, 0.25f), color, m_aimPos + Vec3(0, 0, 0.25f), color);
	}
}

float CFireCommandProjectileFast::GetTimeToNextShot() const
{
	return 0.0f;
}

#if 0
// deprecated and won't compile at all...

//====================================================================
// CFireCommandStrafing
//====================================================================
CFireCommandStrafing::CFireCommandStrafing(IAIActor* pShooter) :
	m_pShooter(((CAIActor*)pShooter)->CastToCPuppet())
{
	CTimeValue fCurrentTime = GetAISystem()->GetFrameStartTime();
	m_fLastStrafingTime = fCurrentTime;
	m_fLastUpdateTime = fCurrentTime;
	m_StrafingCounter = 0;
}

//
//
//
//----------------------------------------------------------------------------------------------------------------
void CFireCommandStrafing::Reset()
{
	AIAssert(m_pShooter);
	CTimeValue fCurrentTime = GetAISystem()->GetFrameStartTime();
	m_fLastStrafingTime = fCurrentTime;
	m_fLastUpdateTime = fCurrentTime;
	m_StrafingCounter = 0;
}

//
//
//
//----------------------------------------------------------------------------------------------------------------
bool CFireCommandStrafing::ManageFireDirStrafing(IAIObject* pTarget, Vec3& vTargetPos, bool& fire, const AIWeaponDescriptor& descriptor)
{
	fire = false;

	const AgentParameters& params = m_pShooter->GetParameters();

	CPuppet* pShooterPuppet = m_pShooter->CastToCPuppet();
	if (!pShooterPuppet)
		return false;

	if (!m_pShooter->GetProxy())
		return false;

	const SAIBodyInfo& bodyInfo = pShooterPuppet->GetBodyInfo();

	bool strafeLoop(false);
	int maxStrafing = (int)params.m_fStrafingPitch;
	if (maxStrafing == 0)
		maxStrafing = 1;

	if (maxStrafing < 0)
	{
		maxStrafing = -maxStrafing;
		strafeLoop = true;
		if (maxStrafing * 2 <= m_StrafingCounter)
			m_StrafingCounter = 0;
	}

	Vec3 vActualFireDir = bodyInfo.vFireDir;
	if (m_StrafingCounter == 0)
	{
		CTimeValue fCurrentTime = GetAISystem()->GetFrameStartTime();
		m_fLastStrafingTime = fCurrentTime;
		m_fLastUpdateTime = fCurrentTime;
		m_StrafingCounter++;
	}
	if (strafeLoop || maxStrafing * 2 > m_StrafingCounter)
	{

		CTimeValue fCurrentTime = GetAISystem()->GetFrameStartTime();
		m_fLastUpdateTime = fCurrentTime;

		// { t : (X-C)N=0,X=P+tV,V=N } <-> t=(C-P)N

		Vec3 N, C, P, X, V, V2;
		float t;
		//		float d,s;

		N = Vec3(0.0, 0.0, 1.0f);
		C = vTargetPos - Vec3(0.0f, 0.0f, 0.3f);
		P = m_pShooter->GetFirePos();
		V = m_pShooter->GetVelocity();
		V2 = pTarget->GetVelocity();
		P += V2 * 2.0f;
		//		d = (C-P).GetLength();
		t = (C - P).Dot(N);

		if (fabs(t) > 0.01f)  //means if we have enough distance to the target.
		{
			X = N;
			X.SetLength(t);
			X += P;

			Vec3 vStrafingDirUnit = X - C;
			float fStrafingLen = vStrafingDirUnit.GetLength();
			Limit(fStrafingLen, 1.0f, 30.0f);

			//			float r = (float)m_StrafingCounter /(float)maxStrafing;

			float fStrafingOffset = ((float)(maxStrafing - m_StrafingCounter)) * fStrafingLen / (float)maxStrafing;
			if (fStrafingOffset < 0)
				fStrafingOffset = 0.0f;

			vTargetPos = vStrafingDirUnit;
			vTargetPos.SetLength(fStrafingOffset);
			vTargetPos += C;

			Vec3 vShootDirection = vTargetPos - P;
			vShootDirection.NormalizeSafe();

			float dotUp = 1.0f;
			float tempSolution = 30.0f;
			if (pShooterPuppet->GetSubType() == CAIObject::STP_2D_FLY)
				tempSolution = 60.0f;

			if (bodyInfo.linkedVehicleEntity && bodyInfo.linkedVehicleEntity->HasAI())
			{
				Matrix33 wm(bodyInfo.linkedVehicleEntity->GetWorldTM());
				Vec3 vUp = wm.GetColumn(2);
				//float dotUp = vUp.Dot( Vec3(0.0f,0.0f,1.0f) );
				//i hope this version is correct [MM]
				dotUp = vUp.Dot(Vec3(0.0f, 0.0f, 1.0f));
			}
			/*
			      {
			        GetAISystem()->AddDebugLine(m_pShooter->GetFirePos(), vTargetPos, 0, 0, 255, 0.1f);
			      }
			 */
			if (pShooterPuppet->GetSubType() == CAIObject::STP_2D_FLY)
			{
			}
			else
			{
				pShooterPuppet->m_State.vAimTargetPos = vTargetPos;
			}

			bool rejected = false;
			if (dotUp > cos_tpl(DEG2RAD(tempSolution)))
			{
				float dot = vShootDirection.Dot(vActualFireDir);

				if (dot > cos_tpl(DEG2RAD(tempSolution)))
				{
					if (pShooterPuppet->GetSubType() == CAIObject::STP_2D_FLY)
					{
						if (pShooterPuppet->CheckAndAdjustFireTarget(vTargetPos, descriptor.spreadRadius, vTargetPos, -1, -1))
							fire = true;
						else
							rejected = true;
					}
					else
					{
						if (pShooterPuppet->CheckAndAdjustFireTarget(vTargetPos, descriptor.spreadRadius, vTargetPos, -1, -1))
						{
							fire = true;
						}
						else
							rejected = true;
					}
				}
			}
			/*
			      if ( fire== true )
			      {
			        Vec3 v1 = m_pShooter->GetFirePos();
			        GetAISystem()->AddDebugLine(v1, vTargetPos, 255, 255, 255, 0.1f);
			        Vec3 v2 =vActualFireDir * 100.0f;
			        GetAISystem()->AddDebugLine(v1, v1 + v2, 255, 0, 0, 0.1f);
			      }
			 */
			if (rejected == true)
			{
				if ((fCurrentTime - m_fLastStrafingTime).GetSeconds() > 0.8f)
				{
					m_fLastStrafingTime = fCurrentTime;
					m_StrafingCounter++;
				}
			}
			if ((fCurrentTime - m_fLastStrafingTime).GetSeconds() > 0.08f)
			{
				m_fLastStrafingTime = fCurrentTime;
				m_StrafingCounter++;
			}
		}
	}
	return false;
}

//
//
//
//----------------------------------------------------------------------------------------------------------------
EAIFireState CFireCommandStrafing::Update(IAIObject* pTargetSrc, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	AIAssert(m_pShooter);
	AIAssert(GetAISystem());

	if (!canFire)
	{
		Reset();
		return eAIFS_Off;
	}

	if (!pTargetSrc)
	{
		Reset();
		return eAIFS_Off;
	}

	CPuppet* pShooterPuppet = m_pShooter->CastToCPuppet();
	if (!pShooterPuppet)
	{
		Reset();
		return eAIFS_Off;
	}

	CAIObject* pTarget = (CAIObject*)pTargetSrc;

	CAIObject* pAssoc = pTarget->GetAssociation().GetAIObject();
	if (pAssoc)
		pTarget = pAssoc;

	Vec3 vTargetPos = pTarget->GetPos();

	int targetType = pTarget->GetType();
	if (targetType == AIOBJECT_VEHICLE)
	{
		EntityId driverID = pTarget->GetProxy()->GetLinkedDriverEntityId();

		if (driverID)
		{
			if (IEntity* entity = gEnv->pEntitySystem->GetEntity(driverID))
			{
				if (entity->HasAI())
				{
					pTarget = (CAIObject*)entity->GetAI();
					vTargetPos = pTarget->GetPos();
					vTargetPos.z -= 2.0f;
				}
			}
		}
	}

	bool fire = false;

	ManageFireDirStrafing(pTarget, vTargetPos, fire, descriptor);
	outShootTargetPos = vTargetPos;

	return (fire ? eAIFS_On : eAIFS_Off);
}

float CFireCommandStrafing::GetTimeToNextShot() const
{
	return 0.0f;
}
//====================================================================
// CFireCommandFastLightMOAR
//====================================================================
CFireCommandFastLightMOAR::CFireCommandFastLightMOAR(IAIActor* pShooter) :
	m_pShooter((CAIActor*)pShooter)
{
}

void CFireCommandFastLightMOAR::Reset()
{
	m_startTime = GetAISystem()->GetFrameStartTime();
	m_startFiring = false;
}

//
//
//
//----------------------------------------------------------------------------------------------------------------
EAIFireState CFireCommandFastLightMOAR::Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{

	AIAssert(m_pShooter);
	AIAssert(GetAISystem());

	if (!pTarget)
		return eAIFS_Off;

	const float chargeTime = descriptor.fChargeTime;
	float drawTime = descriptor.drawTime;

	float dt = GetAISystem()->GetFrameDeltaTime();
	float elapsedTime = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds();
	Vec3 targetPos(pTarget->GetPos());
	Vec3 dirToTarget(targetPos - m_pShooter->GetFirePos());

	float distToTarget = dirToTarget.GetLength();
	float sightRange2 = m_pShooter->GetParameters().m_PerceptionParams.sightRange / 2;
	if (sightRange2 > 0 && distToTarget > sightRange2)
		drawTime *= 1 + (distToTarget - sightRange2) / sightRange2;
	// The moment it is
	if (canFire && !m_startFiring)
	{
		m_laggedTarget = m_pShooter->GetFirePos() + m_pShooter->GetViewDir() * distToTarget;
		m_startFiring = true;
		m_startDist = distToTarget;
		m_rand.set(cry_random(-1.0f, 1.0f), cry_random(-1.0f, 1.0f));
	}

	if (m_startFiring)
	{
		float f = min(dt * 2.0f, 1.0f);
		m_laggedTarget += (targetPos - m_laggedTarget) * f;

		float t = 0;

		if (elapsedTime < chargeTime)
		{
			// Wait until the weapon has charged
			t = 0;
		}
		else if (elapsedTime < chargeTime + drawTime)
		{
			t = ((elapsedTime - chargeTime) / drawTime);
		}
		else
		{
			t = 1.0f;
		}

		/*
		   // Spiral draw fire.
		   Matrix33	basis;
		   basis.SetRotationVDir(dirToTarget.GetNormalizedSafe());
		   // Wobbly offset
		   float	oa = sinf(cosf(t * gf_PI * 33.321f) * gf_PI + t * gf_PI * 45.0f) * cosf(t * gf_PI * 13.1313f) + cosf(t * gf_PI * 6.1313f) * cosf(t * gf_PI * 4.1313f);
		   float	or = cosf(sinf(t * gf_PI * 22.521f) * gf_PI + t * gf_PI * 64.1f) * sinf(t * gf_PI * 9.1413f) + cosf(t * gf_PI * 7.3313f) * sinf(t * gf_PI * 3.6313f);

		   float	ang = t * gf_PI * 4.0f + oa * DEG2RAD(4.0f);
		   float	rad = (sqrtf(1.0f - t) * 6.0f) * (0.9f + or * 0.2f);

		   Vec3	offset(sinf(ang) * rad, 0, -cosf(ang) * rad);

		   outShootTargetPos = m_laggedTarget + basis * offset;
		 */

		// Line draw fire.
		Vec3 dir = m_laggedTarget - m_pShooter->GetFirePos();
		//		dir.z = 0;
		float dist = dir.GetLength();
		if (dist > 0.0f)
			dir /= dist;

		Matrix33 basis;
		basis.SetRotationVDir(dir);

		const float drawLen = 20.0f;
		float minDist = dist - drawLen;
		if (minDist < m_startDist * 0.5f) minDist = m_startDist * 0.5f;

		//		float	startHeight = ((minDist - dist) / drawLen) * drawLen / 5.0f; //(minDist - dist) / (dist - minDist) * (drawLen / 3.0f);

		//float	a = (sinf(t * gf_PI * 3.321f + m_rand.x * 0.23424f) + sinf(t * gf_PI * 17.321f + m_rand.y * 0.433221) * 0.2f) / 1.2f;
		float a = (t < 1 ? sinf((descriptor.sweepFrequency * t + m_rand.x) * gf_PI) * descriptor.sweepWidth * (1 - t * t) : 0);
		Vec3 h;
		h.x = a;//cosf(gf_PI/2 + a/2 * gf_PI/2) * startHeight;
		h.y = 0;
		h.z = (t - 1) * (2 + distToTarget / max(sightRange2, 2.f));//m_startDist/(2.f*max(distToTarget,0.01f)) ;//sinf(gf_PI/2 + a/2 * gf_PI/2) * startHeight;
		h = h * basis;

		Vec3 start = m_laggedTarget + dir * (minDist - dist) + h;  //basis.GetRow(2) * startHeight;
		Vec3 end = m_laggedTarget;

		GetAISystem()->AddDebugLine(m_laggedTarget + dir * (minDist - dist), m_laggedTarget, 0, 0, 0, 0.1f);

		GetAISystem()->AddDebugLine(start, end, 255, 255, 255, 0.1f);

		Vec3 shoot = start + (end - start) * t * t;
		GetAISystem()->AddDebugLine(shoot, shoot + basis.GetRow(2), 255, 0, 0, 0.1f);

		Vec3 wob;
		wob.x = sinf(t * gf_PI * 4.321f + m_rand.x) + cosf(t * gf_PI * 15.321f + m_rand.y * 0.433221f) * 0.2f;
		wob.y = 0;
		wob.z = 0;//cosf(t * gf_PI * 3.231415f + m_rand.y) + sinf(t * gf_PI * 16.321f + m_rand.x * 0.333221) * 0.2f;
		wob = wob * basis;

		GetAISystem()->AddDebugLine(shoot, shoot + wob, 255, 0, 0, 0.1f);

		outShootTargetPos = shoot + wob * 0.5f * sqrtf(1 - t);

		//		float	wobble = sinf(cosf(t * gf_PI * 33.321f) * gf_PI + t * gf_PI * 45.0f) * cosf(t * gf_PI * 13.1313f) + cosf(t * gf_PI * 6.1313f) * cosf(t * gf_PI * 4.1313f);

		/*		Matrix33	basis;
		    basis.SetRotationVDir(dirToTarget.GetNormalizedSafe());

		    Vec3	wob;
		    wob.x = sinf(cosf(t * gf_PI * 33.321f) * gf_PI + t * gf_PI * 45.0f) * cosf(t * gf_PI * 13.1313f) + cosf(t * gf_PI * 6.1313f) * cosf(t * gf_PI * 4.1313f);
		    wob.y = 0;
		    wob.z = cosf(sinf(t * gf_PI * 22.521f) * gf_PI + t * gf_PI * 64.1f) * sinf(t * gf_PI * 9.1413f) + cosf(t * gf_PI * 7.3313f) * sinf(t * gf_PI * 3.6313f);

		    Vec3	pos = m_pShooter->GetFirePos();
		    pos.z = m_laggedTarget.z;

		    outShootTargetPos = m_laggedTarget + (advance * dir); // + basis * (wob * 0.2f);*/
	}

	return (canFire ? eAIFS_On : eAIFS_Off);
}

float CFireCommandFastLightMOAR::GetTimeToNextShot() const
{
	return 0.0f;
}

//====================================================================
// CFireCommandHunterMOAR
//====================================================================
CFireCommandHunterMOAR::CFireCommandHunterMOAR(IAIActor* pShooter)
	: m_pShooter((CAIActor*) pShooter)
{
}

EAIFireState CFireCommandHunterMOAR::Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	if (!pTarget)
		return eAIFS_Off;

	float elapsedTime = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds() - descriptor.fChargeTime;
	float dt = GetAISystem()->GetFrameDeltaTime();

	const Vec3& targetPos = pTarget->GetPos();

	if (!m_startFiring)
	{
		m_lastPos = m_targetPos = targetPos;
		m_startFiring = true;
	}

	if (m_startFiring)
	{
		if (elapsedTime < 0)
		{
			// Wait until the weapon has charged
			return (canFire ? eAIFS_On : eAIFS_Off);
		}

		const float oscillationFrequency = 1.0f; // GetPortFloat( pActInfo, 1 );
		const float oscillationAmplitude = 1.0f; // GetPortFloat( pActInfo, 2 );
		//		const float duration = 6.0f; // GetPortFloat( pActInfo, 3 );
		const float alignSpeed = 8.0f; // GetPortFloat( pActInfo, 4 );

		if (dt > 0)
		{
			// make m_lastPos follow m_targetPos with limited speed
			Vec3 aim = targetPos + (targetPos - m_targetPos) * 2.0f;
			m_targetPos = targetPos;
			Vec3 dir = aim - m_lastPos;
			float maxDelta = alignSpeed * dt;
			if (maxDelta > 0)
			{
				if (dir.GetLengthSquared() > maxDelta * maxDelta)
					dir.SetLength(maxDelta);
			}
			m_lastPos += dir;
		}

		// a - vertical offset
		float a = min(8.0f, max((2.5f - elapsedTime) * 3.0f, -0.5f));
		a += cosf(elapsedTime * (100.0f * gf_PI / 180.0f)) * 1.5f; // add some high frequency low amplitude vertical oscillation

		// b - horizontal offset
		float b = sinf(powf(elapsedTime, 1.4f) * oscillationFrequency * (2.0f * gf_PI));
		b += sinf(elapsedTime * 355.0f * (gf_PI / 180.0f)) * 0.1f;
		b *= max(2.5f - elapsedTime, 0.0f) * oscillationAmplitude;

		Vec3 fireDir = m_lastPos - m_pShooter->GetFirePos();
		Vec3 fireDir2d(fireDir.x, fireDir.y, 0);
		Vec3 sideDir = fireDir.Cross(fireDir2d);
		Vec3 vertDir = sideDir.Cross(fireDir).normalized();
		sideDir.NormalizeSafe();

		outShootTargetPos = m_lastPos - vertDir * a + sideDir * b;
	}

	return (canFire ? eAIFS_On : eAIFS_Off);
}

float CFireCommandHunterMOAR::GetTimeToNextShot() const
{
	return 0.0f;
}
//====================================================================
// CFireCommandHunterSweepMOAR
//====================================================================
CFireCommandHunterSweepMOAR::CFireCommandHunterSweepMOAR(IAIActor* pShooter)
	: m_pShooter((CAIActor*) pShooter)
{
}

EAIFireState CFireCommandHunterSweepMOAR::Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	if (!pTarget)
		return eAIFS_Off;

	float elapsedTime = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds() - descriptor.fChargeTime;
	float dt = GetAISystem()->GetFrameDeltaTime();

	const Vec3& targetPos = pTarget->GetPos();

	if (!m_startFiring)
	{
		m_lastPos = m_targetPos = targetPos;
		m_startFiring = true;
	}

	if (m_startFiring)
	{
		if (elapsedTime < 0)
		{
			// Wait until the weapon has charged
			return (canFire ? eAIFS_On : eAIFS_Off);
		}

		const float baseDist = 20.0f;      // GetPortFloat( pActInfo, 1 );
		const float leftRightSpeed = 6.0f; // GetPortFloat( pActInfo, 2);
		const float width = 10.0f;         // GetPortFloat( pActInfo, 3);
		const float nearFarSpeed = 7.0f;   // GetPortFloat( pActInfo, 4);
		const float dist = 7.0f;           // GetPortFloat( pActInfo, 5);
		const float duration = 10.0f;      // GetPortFloat( pActInfo, 6);

		Matrix34 worldTransform = m_pShooter->GetEntity()->GetWorldTM();
		Vec3 fireDir = worldTransform.GetColumn1();
		Vec3 sideDir = worldTransform.GetColumn0();
		Vec3 upDir = worldTransform.GetColumn2();
		Vec3 pos = worldTransform.GetColumn3();

		// runs from 0 to 2*PI and indicates where in the 2*PI long cycle we are now
		float t = 2.0f * gf_PI * elapsedTime / duration;

		// NOTE Mrz 31, 2007: <pvl> essetially a Lissajous curve, with some
		// irregularity added by a higher frequency harmonic and offset vertically
		// because we don't want the hunter to shoot at the sky.
		outShootTargetPos = m_pShooter->GetPos() +
		                    baseDist * fireDir +
		                    dist * sinf(nearFarSpeed * t) * upDir +
		                    0.2f * dist * sinf(2.3f * nearFarSpeed * t + 1.0f /*phase shift*/) * upDir +
		                    -1.8f * dist * upDir +
		                    width * sinf(leftRightSpeed * t) * sideDir +
		                    0.2f * width * sinf(2.3f * leftRightSpeed * t + 1.0f /*phase shift*/) * sideDir;
	}

	return (canFire ? eAIFS_On : eAIFS_Off);
}

float CFireCommandHunterSweepMOAR::GetTimeToNextShot() const
{
	return 0.0f;
}
//====================================================================
// CFireCommandHunterSingularityCannon
//====================================================================
CFireCommandHunterSingularityCannon::CFireCommandHunterSingularityCannon(IAIActor* pShooter)
	: m_pShooter((CAIActor*) pShooter)
{
}

EAIFireState CFireCommandHunterSingularityCannon::Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	if (!pTarget)
		return eAIFS_Off;

	float elapsedTime = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds() - descriptor.fChargeTime;
	float dt = GetAISystem()->GetFrameDeltaTime();

	const Vec3& targetPos = pTarget->GetPos();
	outShootTargetPos = targetPos;

	if (!m_startFiring)
	{
		m_targetPos = targetPos;
		m_startFiring = true;
	}

	/*
	   if ( m_startFiring )
	   {
	    if ( elapsedTime < 0 )
	    {
	      // Wait until the weapon has charged
	      return (canFire ? eAIFS_On : eAIFS_Off);
	    }
	   }
	 */

	return (canFire ? eAIFS_On : eAIFS_Off);
}

float CFireCommandHunterSingularityCannon::GetTimeToNextShot() const
{
	return 0.0f;
}

//====================================================================
// CFireCommandHurricane
//====================================================================
CFireCommandHurricane::CFireCommandHurricane(IAIActor* pShooter) :
	/*m_fLastShootingTime(0),
	   m_fDrawFireLastTime(0),
	   m_nDrawFireCounter(0),
	   m_JustReset(false),
	   m_bIsTriggerDown(false),
	   m_bIsBursting(false),
	   m_fTimeOut(-1.f),
	   m_fLastBulletOutTime(0.f),
	   m_fFadingToNormalTime(0.f),
	   m_fLastValidTragetTime(0.f),*/
	m_pShooter(((CAIActor*)pShooter)->CastToCPuppet())
{
}

void CFireCommandHurricane::Reset()
{
}

//
//
//
//----------------------------------------------------------------------------------------------------------------
EAIFireState CFireCommandHurricane::Update(IAIObject* pITarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	if (!pITarget)
		return eAIFS_Off;

	if (!canFire)
		return eAIFS_Off;

	CAIObject* pTarget((CAIObject*)pITarget);

	bool fireNow(SelectFireDirNormalFire(pTarget, 1.f, outShootTargetPos));

	// Check if weapon is not pointing too differently from where we want to shoot
	// The outShootTargetPos is initialized with the aim target.
	if (fireNow && !IsWeaponPointingRight(outShootTargetPos))
		return eAIFS_Off;

	if (fireNow && !ValidateFireDirection(outShootTargetPos - m_pShooter->GetFirePos(), false))
		return eAIFS_Off;

	return (fireNow ? eAIFS_On : eAIFS_Off);
}

//
//--------------------------------------------------------------------------------------------------------
bool CFireCommandHurricane::SelectFireDirNormalFire(CAIObject* pTarget, float fAccuracyRamping, Vec3& outShootTargetPos)
{
	//accuracy/missing management: miss if not in front of player or if accuracy needed
	float shooterCurrentAccuracy = m_pShooter->GetAccuracy(pTarget);
	bool bAccuracyMiss = cry_random(0.f, 100.f) > (shooterCurrentAccuracy * 100.f);
	bool bMissNow = bAccuracyMiss; // || m_pShooter->GetAmbientFire();

	++dbg_ShotCounter;

	if (bMissNow)
	{
		outShootTargetPos = ChooseMissPoint(pTarget);
	}
	else
	{
		Vec3 vTargetPos;
		Vec3 vTargetDir;
		// In addition to common fire checks, make sure that we do not hit something too close between us and the target.
		// check if we don't hit anything (rocks/trees/cars)
		bool lowDamage = fAccuracyRamping < 1.0f;
		//		if(pTarget->GetType() == AIOBJECT_PLAYER)
		//			lowDamage = lowDamage || !GetAISystem()->GetCurrentDifficultyProfile()->allowInstantKills;
		if (!lowDamage)
			lowDamage = cry_random(0.f, 100.f) > (shooterCurrentAccuracy * 50.f);  // decide if want to hit legs/arms (low damage)
		if (!m_pShooter->CheckAndGetFireTarget_Deprecated(pTarget, lowDamage, vTargetPos, vTargetDir))
			return false;
		outShootTargetPos = vTargetPos;

		if (!outShootTargetPos.IsZero(1.f))
		{
			++dbg_ShotHitCounter;
		}
	}
	//
	if (outShootTargetPos.IsZero(1.f))
	{
		outShootTargetPos = pTarget->GetPos();
		return false;
	}

	return true;
}

//
//	we have candidate shooting direction, let's check if it is good
//
//--------------------------------------------------------------------------------------------------------
bool CFireCommandHurricane::ValidateFireDirection(const Vec3& fireVector, bool mustHit)
{
	ray_hit hit;
	int rwiResult;
	IPhysicalWorld* pWorld = gEnv->pPhysicalWorld;
	rwiResult = pWorld->RayWorldIntersection(
	  m_pShooter->GetFirePos(), fireVector, COVER_OBJECT_TYPES,
	  AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER,
	  &hit, 1);
	// if hitting something too close to shooter - bad fire direction
	float fireVectorLen;
	if (rwiResult != 0)
	{
		fireVectorLen = fireVector.GetLength();
		if (mustHit || hit.dist < fireVectorLen * (m_pShooter->GetFireMode() == FIREMODE_FORCED ? 0.3f : 0.73f))
			return false;
	}
	return !m_pShooter->CheckFriendsInLineOfFire(fireVector, false);
}

//
//--------------------------------------------------------------------------------------------------------
bool CFireCommandHurricane::IsWeaponPointingRight(const Vec3& shootTargetPos)  //const
{
	// Check if weapon is not pointing too differently from where we want to shoot
	Vec3 fireDir(shootTargetPos - m_pShooter->GetFirePos());
	float dist2(fireDir.GetLengthSquared());
	// if within 2m from target - make very loose angle check
	float trhAngle(fireDir.GetLengthSquared() < sqr(5.f) ? 73.f : 22.f);
	fireDir.normalize();
	const SAIBodyInfo& bodyInfo = m_pShooter->GetBodyInfo();

	//dbg_AngleDiffValue = acos_tpl( fireDir.Dot(bodyInfo.vFireDir));
	dbg_AngleDiffValue = RAD2DEG(acos_tpl((fireDir.Dot(bodyInfo.vFireDir))));

	if (fireDir.Dot(bodyInfo.vFireDir) < cos_tpl(DEG2RAD(trhAngle)))
	{
		dbg_AngleTooDifferent = true;
		return false;
	}
	dbg_AngleTooDifferent = false;
	return true;
}

//
//--------------------------------------------------------------------------------------------------------
Vec3 CFireCommandHurricane::ChooseMissPoint(CAIObject* pTarget) const
{
	Vec3 vTargetPos(pTarget->GetPos());
	float targetGroundLevel = pTarget->GetPhysicsPos().z;
	return m_pShooter->ChooseMissPoint_Deprecated(vTargetPos);
}

float CFireCommandHurricane::GetTimeToNextShot() const
{
	return 0.0f;
}
#endif // 0
//
//---------------------------------------------------------------------------------------------------------------

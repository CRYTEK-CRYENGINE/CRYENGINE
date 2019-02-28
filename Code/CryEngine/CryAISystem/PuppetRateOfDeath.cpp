// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Rate of Death handling for the Puppet

   -------------------------------------------------------------------------
   History:
   - 07:09:2009: Migrated over code from Puppet.cpp (Kevin Kirst)

*************************************************************************/

#include "StdAfx.h"
#include "Puppet.h"
#include <CryAISystem/IAIRateOfDeathHandler.h>

//#pragma optimize("", off)
//#pragma inline_depth(0)

//===================================================================
// GetTargetAliveTime
//===================================================================
float CPuppet::GetTargetAliveTime()
{
	float fTargetStayAliveTime = gAIEnv.CVars.RODAliveTime;

	const CAIObject* pLiveTarget = GetLiveTarget(m_refAttentionTarget).GetAIObject();
	if (!pLiveTarget)
		return fTargetStayAliveTime;

	const CAIActor* pLiveActor = pLiveTarget->CastToCAIActor();
	if (!pLiveActor)
		return fTargetStayAliveTime;

	CCCPOINT(CPuppet_GetTargetAliveTime);

	// Delegate to handle if available
	if (m_pRODHandler)
	{
		fTargetStayAliveTime = m_pRODHandler->GetTargetAliveTime(this, pLiveTarget, m_targetZone, m_targetDazzlingTime);
	}
	else
	{
		Vec3 vTargetDir = pLiveTarget->GetPos() - GetPos();
		const float fTargetDist = vTargetDir.NormalizeSafe();

		// Scale target life time based on target speed.
		const float fMoveInc = gAIEnv.CVars.RODMoveInc;
		{
			float fIncrease = 0.0f;

			const Vec3& vTargetVel = pLiveTarget->GetVelocity();
			const float fSpeed = vTargetVel.GetLength2D();

			if ((pLiveTarget->GetType() == AIOBJECT_PLAYER) &&
			    (m_fireMode != FIREMODE_MELEE) && (m_fireMode != FIREMODE_MELEE_FORCED))
			{
				// Super speed run or super jump.
				if (fSpeed > 12.0f || vTargetVel.z > 7.0f)
				{
					fIncrease += fMoveInc * 2.0f;

					// Dazzle the shooter for a moment.
					m_targetDazzlingTime = 1.0f;
				}
				else if (fSpeed > 6.0f)
				{
					fIncrease += fMoveInc;
				}
			}
			else if (fSpeed > 6.0f)
			{
				fIncrease *= fMoveInc;
			}

			fTargetStayAliveTime += fIncrease;
		}

		// Scale target life time based on target stance.
		const float fStanceInc = gAIEnv.CVars.RODStanceInc;
		{
			float fIncrease = 0.0f;

			const SAIBodyInfo& bi = pLiveActor->GetBodyInfo();
			if (bi.stance == STANCE_CROUCH && m_targetZone > AIZONE_KILL)
				fIncrease += fStanceInc;
			else if (bi.stance == STANCE_PRONE && m_targetZone >= AIZONE_COMBAT_FAR)
				fIncrease += fStanceInc * fStanceInc;

			fTargetStayAliveTime += fIncrease;
		}

		// Scale target life time based on target vs. shooter orientation.
		const float fDirectionInc = gAIEnv.CVars.RODDirInc;
		{
			float fIncrease = 0.0f;

			const float thr1 = cosf(DEG2RAD(30.0f));
			const float thr2 = cosf(DEG2RAD(95.0f));
			float dot = -vTargetDir.Dot(pLiveTarget->GetViewDir());
			if (dot < thr2)
				fIncrease += fDirectionInc * 2.0f;
			else if (dot < thr1)
				fIncrease += fDirectionInc;

			fTargetStayAliveTime += fIncrease;
		}

		if (!m_allowedToHitTarget)
		{
			// If the agent is set not to be allowed to hit the target, let the others shoot first.
			const float fAmbientFireInc = gAIEnv.CVars.RODAmbientFireInc;
			fTargetStayAliveTime += fAmbientFireInc;
		}
		else if (m_targetZone == AIZONE_KILL)
		{
			// Kill much faster when the target is in kill-zone (but not if in a vehicle)
			const SAIBodyInfo bi = GetBodyInfo();
			if (!bi.GetLinkedVehicleEntity())
			{
				const float fKillZoneInc = gAIEnv.CVars.RODKillZoneInc;
				fTargetStayAliveTime += fKillZoneInc;
			}
		}
	}

	CCCPOINT(CPuppet_GetTargetAliveTime_A);
	return max(0.0f, fTargetStayAliveTime);
}

//===================================================================
// UpdateHealthTracking
//===================================================================
void CPuppet::UpdateHealthTracking()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	// Update target health tracking.
	float fTargetHealth = 0.0f;
	float fTargetMaxHealth = 1.0f;

	// Convert the current health to normalized range (1.0 == normal max health).
	const CAIObject* pTarget = GetLiveTarget(m_refAttentionTarget).GetAIObject();
	IAIActorProxy* pTargetProxy = (pTarget ? pTarget->GetProxy() : NULL);
	if (pTargetProxy)
	{
		CCCPOINT(CPuppet_UpdateHealthTracking);

		const float fProxyMaxHealth = pTargetProxy->GetActorMaxHealth();

		const float fCurrHealth = (pTargetProxy->GetActorHealth() + pTargetProxy->GetActorArmor());
		const float fCurrMaxHealth = (fProxyMaxHealth + pTargetProxy->GetActorMaxArmor());

		fTargetMaxHealth = fCurrMaxHealth / fProxyMaxHealth;
		fTargetHealth = fCurrHealth / fProxyMaxHealth;
	}

	// Calculate the rate of death.
	float fTargetStayAliveTime = GetTargetAliveTime();

	// This catches the case when the target turns on and off the armor.
	if (m_targetDamageHealthThr > fTargetMaxHealth)
		m_targetDamageHealthThr = fTargetMaxHealth;

	if (fTargetHealth >= m_targetDamageHealthThr || fTargetStayAliveTime <= FLT_EPSILON)
	{
		m_targetDamageHealthThr = fTargetHealth;
	}
	else
	{
		const float fFrametime = GetAISystem()->GetFrameDeltaTime();
		m_targetDamageHealthThr = max(0.0f, m_targetDamageHealthThr - (1.0f / fTargetStayAliveTime) * m_Parameters.m_fAccuracy * fFrametime);
	}

	if (gAIEnv.CVars.DebugDrawDamageControl > 0)
	{
		if (!m_targetDamageHealthThrHistory)
			m_targetDamageHealthThrHistory = new CValueHistory<float>(100, 0.1f);
		m_targetDamageHealthThrHistory->Sample(m_targetDamageHealthThr, GetAISystem()->GetFrameDeltaTime());

#ifdef CRYAISYSTEM_DEBUG
		UpdateHealthHistory();
#endif
	}
}

//====================================================================
// GetFiringReactionTime
//====================================================================
float CPuppet::GetFiringReactionTime(const Vec3& targetPos) const
{
	// Apply archetype modifier
	float fReactionTime = gAIEnv.CVars.RODReactionTime * GetParameters().m_PerceptionParams.reactionTime;

	const CAIActor* pLiveTarget = GetLiveTarget(m_refAttentionTarget).GetAIObject();
	if (!pLiveTarget)
		return fReactionTime;

	const CAIActor* pLiveActor = pLiveTarget->CastToCAIActor();
	if (!pLiveActor)
		return fReactionTime;

	// Delegate to handle if available
	if (m_pRODHandler)
	{
		fReactionTime = m_pRODHandler->GetFiringReactionTime(this, pLiveTarget, targetPos);
	}
	else
	{
		if (IsAffectedByLight())
		{
			EAILightLevel iTargetLightLevel = pLiveTarget->GetLightLevel();
			if (iTargetLightLevel == AILL_MEDIUM)
			{
				fReactionTime += gAIEnv.CVars.RODReactionMediumIllumInc;
			}
			else if (iTargetLightLevel == AILL_DARK)
			{
				fReactionTime += gAIEnv.CVars.RODReactionDarkIllumInc;
			}
			else if (iTargetLightLevel == AILL_SUPERDARK)
			{
				fReactionTime += gAIEnv.CVars.RODReactionSuperDarkIllumInc;
			}
		}

		const float fDistInc = min(1.0f, gAIEnv.CVars.RODReactionDistInc);
		// Increase reaction time if the target is further away.
		if (m_targetZone == AIZONE_COMBAT_NEAR)
			fReactionTime += fDistInc;
		else if (m_targetZone >= AIZONE_COMBAT_FAR)
			fReactionTime += fDistInc * 2.0f;

		// Increase the reaction time of the target is leaning.
		const SAIBodyInfo& bi = pLiveActor->GetBodyInfo();
		if (fabsf(bi.lean) > 0.01f)
		{
			fReactionTime += gAIEnv.CVars.RODReactionLeanInc;
		}

		Vec3 dirTargetToShooter = GetPos() - pLiveTarget->GetPos();
		dirTargetToShooter.Normalize();
		const float fLeaningDot = pLiveTarget->GetViewDir().Dot(dirTargetToShooter);

		const float thr1 = cosf(DEG2RAD(30.0f));
		const float thr2 = cosf(DEG2RAD(95.0f));
		if (fLeaningDot < thr1)
			fReactionTime += gAIEnv.CVars.RODReactionDirInc;
		else if (fLeaningDot < thr2)
			fReactionTime += gAIEnv.CVars.RODReactionDirInc * 2.0f;
	}

	return fReactionTime;
}

//====================================================================
// UpdateFireReactionTimer
//====================================================================
void CPuppet::UpdateFireReactionTimer(const Vec3& vTargetPos)
{
	const float fFrametime = GetAISystem()->GetFrameDeltaTime();
	const float fReactionTime = GetFiringReactionTime(vTargetPos);

	// Update the fire reaction timer
	bool bReacting = false;
	if (IsAllowedToHitTarget() && AllowedToFire())
	{
		if (GetAttentionTargetType() == AITARGET_VISUAL && GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE)
		{
			m_firingReactionTime = min(m_firingReactionTime + fFrametime, fReactionTime + 0.001f);
			bReacting = true;
		}
	}

	if (!bReacting)
	{
		m_firingReactionTime = max(0.0f, m_firingReactionTime - fFrametime);
	}

	m_firingReactionTimePassed = m_firingReactionTime >= fReactionTime;
}

//====================================================================
// UpdateTargetZone
//====================================================================
void CPuppet::UpdateTargetZone(CWeakRef<CAIObject> refTarget)
{
	CAIObject* pTarget = refTarget.GetAIObject();
	if (!pTarget)
	{
		m_targetZone = AIZONE_OUT;
		return;
	}

	// Delegate to handler if available
	if (m_pRODHandler)
	{
		m_targetZone = m_pRODHandler->GetTargetZone(this, pTarget);
	}
	else
	{
		const float fKillRange = gAIEnv.CVars.RODKillRangeMod;
		const float fCombatRange = gAIEnv.CVars.RODCombatRangeMod;

		// Calculate off of attack range
		const float fDistToTargetSqr = Distance::Point_PointSq(GetPos(), pTarget->GetPos());
		if (fDistToTargetSqr < sqr(m_Parameters.m_fAttackRange * fKillRange))
			m_targetZone = AIZONE_KILL;
		else if (fDistToTargetSqr < sqr(m_Parameters.m_fAttackRange * (fCombatRange + fKillRange) / 2))
			m_targetZone = AIZONE_COMBAT_NEAR;
		else if (fDistToTargetSqr < sqr(m_Parameters.m_fAttackRange * fCombatRange))
			m_targetZone = AIZONE_COMBAT_FAR;
		else if (fDistToTargetSqr < sqr(m_Parameters.m_fAttackRange))
			m_targetZone = AIZONE_WARN;
		else
			m_targetZone = AIZONE_OUT;
	}
}

std::vector<Vec3> CPuppet::s_projectedPoints(16);

//====================================================================
// UpdateTargetTracking
//====================================================================
Vec3 CPuppet::UpdateTargetTracking(CWeakRef<CAIObject> refTarget, const Vec3& vTargetPos)
{
	const float fFrametime = GetAISystem()->GetFrameDeltaTime();
	const float fReactionTime = GetFiringReactionTime(vTargetPos);

	// Update the fire reaction timer
	UpdateFireReactionTimer(vTargetPos);

	const CAIObject* pTarget = refTarget.GetAIObject();
	const CAIActor* pLiveTarget = GetLiveTarget(refTarget).GetAIObject();
	if (!pLiveTarget)
	{
		ResetTargetTracking();
		m_targetBiasDirection += (Vec3(0, 0, -1) - m_targetBiasDirection) * fFrametime;
		return vTargetPos;
	}

	CCCPOINT(CPuppet_UpdateTargetTracking);

	// Update the current target's zone
	UpdateTargetZone(refTarget);

	float fFocusTargetValue = 0.0f;
	fFocusTargetValue += pTarget->GetVelocity().GetLength() / 3.0f;
	fFocusTargetValue += m_targetEscapeLastMiss;
	if (m_targetZone >= AIZONE_WARN)
		fFocusTargetValue += 1.0f;
	Limit(fFocusTargetValue, 0.0f, 1.0f);
	if (fFocusTargetValue > m_targetFocus)
		m_targetFocus += (fFocusTargetValue - m_targetFocus) * fFrametime;
	else
		m_targetFocus += (fFocusTargetValue - m_targetFocus) * 0.4f * fFrametime;

	// Calculate a silhouette which is later used to miss the player intentionally.
	// The silhouette is the current target AABB extruded into the movement direction.
	// The silhouette exists on a plane which separates the shooter and the target.
	if (!m_bDryUpdate || !m_targetSilhouette.valid)
	{
		m_targetSilhouette.valid = true;

		//AIAssert(pLiveTarget->GetProxy());

		// Calculate the current and predicted AABB of the target.
		const float MISS_PREDICTION_TIME = 0.8f;

		AABB aabbCur, aabbNext;

		IPhysicalEntity* pPhys = 0;

		// Marcio: pLiveTarget will be a different object if it was retrieved by association.
		if (pLiveTarget)
		{
			pPhys = pLiveTarget->GetPhysics(true);
			if (!pPhys)
				pPhys = pLiveTarget->GetProxy() ? pLiveTarget->GetProxy()->GetPhysics(true) : 0;
		}
		else
		{
			pPhys = pTarget->GetPhysics(true);
			if (!pPhys)
				pPhys = pTarget->GetProxy() ? pTarget->GetProxy()->GetPhysics(true) : 0;
		}

		if (!pPhys)
		{
			AIWarning("CPuppet::UpdateTargetTracking() Target %s does not have physics!", pTarget->GetName());
			AIAssert(0);
			ResetTargetTracking();
			m_targetBiasDirection += (Vec3(0, 0, -1) - m_targetBiasDirection) * fFrametime;
			return vTargetPos;
		}

		pe_status_pos statusPos;
		pPhys->GetStatus(&statusPos);
		aabbCur.Reset();
		aabbCur.Add(statusPos.BBox[0] + statusPos.pos);
		aabbCur.Add(statusPos.BBox[1] + statusPos.pos);
		aabbNext = aabbCur;
		aabbCur.min.z -= 0.05f; // This adjustment moves any ground effect slightly in front of the target.
		aabbNext.min.z -= 0.05f;

		Vec3 vel = pTarget->GetVelocity() * MISS_PREDICTION_TIME;
		aabbNext.Move(vel);

		// Create a list of points which is used to calculate the silhouette.
		Vec3 points[16];
		SetAABBCornerPoints(aabbCur, &points[0]);
		SetAABBCornerPoints(aabbNext, &points[8]);

		m_targetSilhouette.center = aabbCur.GetCenter() + vel * 0.5f;

		// Project points on a plane between the shooter and the target.
		Vec3 dir = m_targetSilhouette.center - GetPos();
		dir.NormalizeSafe();
		m_targetSilhouette.baseMtx.SetRotationVDir(dir, 0.0f);

		const Vec3& u = m_targetSilhouette.baseMtx.GetColumn0();
		const Vec3& v = m_targetSilhouette.baseMtx.GetColumn2();

		for (unsigned i = 0; i < 16; ++i)
		{
			Vec3 pt = points[i] - m_targetSilhouette.center;
			s_projectedPoints[i].Set(u.Dot(pt), v.Dot(pt), 0.0f);
		}

		// The silhouette is the convex hull of all the points in the two AABBs.
		m_targetSilhouette.points.clear();
		ConvexHull2D(m_targetSilhouette.points, s_projectedPoints);
	}

	// Calculate a direction that is used to calculate the miss points on the silhouette.
	Vec3 desiredTargetBias(0, 0, 0);

	// 1) Bend the direction towards the target movement direction.
	if (pTarget)
		desiredTargetBias += pTarget->GetVelocity().GetNormalizedSafe(Vec3(0, 0, 0));

	// 2) Bend the direction towards the point that is visible to the shooter.
	// Note: If the target is visible vTargetPos==m_targetSilhouette.center.
	desiredTargetBias += ((vTargetPos - m_targetSilhouette.center) / 2.0f) * (1 - m_targetEscapeLastMiss);

	// 3) Bend the direction towards ground if not trying to adjust the away from obstructed area.
	desiredTargetBias.z -= 0.1f + 0.5f * (1 - m_targetEscapeLastMiss);

	// 4) If the current aim is obstructed, try to climb the silhouette to the other side.
	if (m_targetEscapeLastMiss > 0.0f && !m_targetLastMissPoint.IsZero())
	{
		Vec3 deltaProj = m_targetSilhouette.ProjectVectorOnSilhouette(m_targetBiasDirection);
		deltaProj.NormalizeSafe();
		if (!deltaProj.IsZero())
		{
			float lastMissAngle = atan2f(deltaProj.y, deltaProj.x);

			const Vec3& u = m_targetSilhouette.baseMtx.GetColumn0();
			const Vec3& v = m_targetSilhouette.baseMtx.GetColumn2();

			// Choose the climb direction based on the current side
			float a = u.Dot(m_targetBiasDirection) < 0.0f ? -gf_PI / 2 : gf_PI / 2;

			float x = cosf(lastMissAngle + a);
			float y = sinf(lastMissAngle + a);

			desiredTargetBias += (u * x + v * y) * m_targetEscapeLastMiss;
		}
	}

	if (desiredTargetBias.NormalizeSafe(ZERO) > 0.0f)
	{
		m_targetBiasDirection += (desiredTargetBias - m_targetBiasDirection) * 4.0f * fFrametime;
		m_targetBiasDirection.NormalizeSafe(ZERO);
	}

	m_targetPosOnSilhouettePlane = m_targetSilhouette.IntersectSilhouettePlane(GetFirePos(), vTargetPos);
	Vec3 targetPosOnSilhouettePlaneProj = m_targetSilhouette.ProjectVectorOnSilhouette(m_targetPosOnSilhouettePlane - m_targetSilhouette.center);

	// Calculate the distance between the target pos and the silhouette.
	if (Overlap::Point_Polygon2D(targetPosOnSilhouettePlaneProj, m_targetSilhouette.points))
	{
		// Inside the polygon, zero dist.
		m_targetDistanceToSilhouette = 0.0f;
	}
	else
	{
		// Distance to the nearest edge.
		Vec3 pt;
		m_targetDistanceToSilhouette = Distance::Point_Polygon2D(targetPosOnSilhouettePlaneProj, m_targetSilhouette.points, pt);
	}

	return vTargetPos;
}

//====================================================================
// CanDamageTarget
//====================================================================
bool CPuppet::CanDamageTarget(IAIObject* target) const
{
	// Never hit when in panic spread fire mode.
	if (m_fireMode == FIREMODE_PANIC_SPREAD)
		return false;

	if (m_Parameters.m_fAccuracy < 0.001f)
		return false;

	CAIObject* fireTargetObject = GetFireTargetObject();
	tAIObjectID fireTargetID = fireTargetObject ? fireTargetObject->GetAIObjectID() : 0;

	CAIObject* player = GetAISystem()->GetPlayer();

	bool isCurrentFireTarget = !target || (target->GetAIObjectID() == fireTargetID);
	//bool targetIsPlayer = target && player && target->GetAIObjectID() == player->GetAIObjectID();

	if (isCurrentFireTarget)
	{
		// Allow to hit always when requested kill fire mode.
		if (m_fireMode == FIREMODE_KILL)
			return true;

		if (m_targetDazzlingTime > 0.0f)
			return false;

		// HACK: [16:05:08 mieszko]: I'm not sure it's a hack. I just don't think this kind of things should be
		// decided purely on C++ side (see the way m_firingReactionTimePassed value is determined).
		if (gAIEnv.configuration.eCompatibilityMode != ECCM_GAME04)
		{
			if (/*targetIsPlayer && */
			  (m_fireMode != FIREMODE_VEHICLE) && (m_fireMode != FIREMODE_FORCED) && (m_fireMode != FIREMODE_KILL) && (m_fireMode != FIREMODE_OFF) && !HasFiringReactionTimePassed())
				return false;
		}
	}

	CAIObject* targetAIObject = target ? static_cast<CAIObject*>(target) : fireTargetObject;
	const bool isAIObjectTarget = targetAIObject ? (targetAIObject->GetType() == AIOBJECT_TARGET) : false;
	if (isAIObjectTarget)
		return true;

	CWeakRef<CAIObject> refTarget = GetWeakRef(targetAIObject);
	const CAIActor* pLiveTargetActor = GetLiveTarget(refTarget).GetAIObject();
	if (!pLiveTargetActor || !pLiveTargetActor->GetProxy())
		return true;

	// If the target is at low health, allow short time of mercy for it.
	if (pLiveTargetActor->IsLowHealthPauseActive())
		return false;

	CCCPOINT(CPuppet_CanDamageTarget);

	float maxHealth = pLiveTargetActor->GetProxy()->GetActorMaxHealth();
	float health = 0.001f + pLiveTargetActor->GetProxy()->GetActorHealth() + pLiveTargetActor->GetProxy()->GetActorArmor();

	float thr = m_targetDamageHealthThr * maxHealth;

	return (thr > 0.0f) && (health >= thr);
}

//====================================================================
// CanDamageTargetWithMelee
//====================================================================
bool CPuppet::CanDamageTargetWithMelee() const
{
	const CAIObject* pLiveTarget = GetLiveTarget(m_refAttentionTarget).GetAIObject();
	if (!pLiveTarget)
		return true;

	// If the target is at low health, allow short time of mercy for it.
	if (const CAIActor* pLiveTargetActor = pLiveTarget->CastToCAIActor())
		if (pLiveTargetActor->IsLowHealthPauseActive())
			return false;

	return true;
}

//====================================================================
// ResetTargetTracking
//====================================================================
void CPuppet::ResetTargetTracking()
{
	m_targetLastMissPoint.zero();
	m_targetEscapeLastMiss = 0.0f;
	m_targetFocus = 0.0f;
	m_lastHitShotsCount = ~0l;
	m_lastMissShotsCount = ~0l;

	if (m_targetSilhouette.valid)
		m_targetSilhouette.Reset();
}

//====================================================================
// GetCoverFireTime
//====================================================================
float CPuppet::GetCoverFireTime() const
{
	return m_CurrentWeaponDescriptor.coverFireTime * gAIEnv.CVars.RODCoverFireTimeMod;
}

//====================================================================
// GetBurstFireDistanceScale
//====================================================================
float CPuppet::GetBurstFireDistanceScale() const
{
	float fResult = 1.0f;

	switch (m_targetZone)
	{
	case AIZONE_KILL:
		fResult = 1.0f;
		break;
	case AIZONE_COMBAT_NEAR:
		fResult = 0.9f;
		break;
	case AIZONE_COMBAT_FAR:
		fResult = 0.9f;
		break;
	case AIZONE_WARN:
		fResult = 0.4f;
		break;
	case AIZONE_OUT:
		fResult = 0.0f;
		break;

	// Ignore or default don't alter the scale
	case AIZONE_IGNORE:
	default:
		fResult = 1.0f;
		break;
	}

	return fResult;
}

//====================================================================
// GetBurstFireDistanceScale
//====================================================================
void CPuppet::HandleBurstFireInit()
{
	// When starting burst in warn zone, force first bullets to miss
	// TODO: not nice to handle it this way!
	if (m_targetZone == AIZONE_WARN)
		m_targetSeenTime = std::max(0.0f, m_targetSeenTime - (cry_random(1, 3)) / 10.0f);

	/* Márcio: does not work for Burst fire weapons - does not seem to have any benefits anyways
	   IAIActorProxy *pProxy = GetProxy();
	   if (pProxy)
	    pProxy->GetAndResetShotBulletCount();
	 */
}

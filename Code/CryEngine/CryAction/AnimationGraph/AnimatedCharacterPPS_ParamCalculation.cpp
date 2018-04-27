// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimatedCharacter.h"
#include "AnimationGraphCVars.h"
#include "PersistantDebug.h"

//--------------------------------------------------------------------------------

QuatT CAnimatedCharacter::CalculateDesiredLocation() const
{
	ANIMCHAR_PROFILE;

	// Disabled this optimization because it leads to improper calculation of
	// parameters, which leads to improper footstep sounds (and wrong animevents
	// timing in general)
	//
	//if (m_simplifyMovement)
	//{
	//	return QuatT(IDENTITY);
	//}

	if (m_curFrameTime <= 0.0f)
	{
		return QuatT(IDENTITY);
	}

	// NOTE: This whole function was optimized to ignore xy rotations and only consider z rotations!

	// ANIM-UNSUPPORTED 2+ Not sure we really need this, so disable it for now
	//
	// Use zero/identity prediction while anim target is active (prevents some jittering from PMC in end of smart objects).
	//if ((m_pAnimTarget != NULL) && m_pAnimTarget->activated)
	//{
	//	return QuatT(IDENTITY);
	//}

	QuatT invEntLocation = m_entLocation.GetInverted();

	// NOTE: prediction doesn't take animation motion into account, this is a pure game-based entity motion prediction

#ifdef _DEBUG
	/*
	   if (DebugFilter())
	   {
	    DebugHistory_AddValue("eDH_TEMP00", m_smoothedActualAnimVelocity);
	    DebugHistory_AddValue("eDH_TEMP01", RecentCollision() ? 1.0f : 0.0f);
	    DebugHistory_AddValue("eDH_TEMP02", (float)(m_curFrameID - m_collisionFrameID));
	   }
	   /**/
#endif

	QuatT desiredLocalLocation;
	if (RecentCollision())
	{
		QuatT predictedWorldLocation = ApplyWorldOffset(m_entLocation, m_requestedEntityMovement);

		Vec3 predictionWorldLocationOffset = predictedWorldLocation.t - m_entLocation.t;
		predictionWorldLocationOffset = RemovePenetratingComponent(predictionWorldLocationOffset, m_collisionNormal[0]);
		predictionWorldLocationOffset = RemovePenetratingComponent(predictionWorldLocationOffset, m_collisionNormal[1]);
		predictionWorldLocationOffset = RemovePenetratingComponent(predictionWorldLocationOffset, m_collisionNormal[2]);
		predictionWorldLocationOffset = RemovePenetratingComponent(predictionWorldLocationOffset, m_collisionNormal[3]);

		desiredLocalLocation.t = invEntLocation.q * predictionWorldLocationOffset;
	}
	else
	{
		desiredLocalLocation.t = invEntLocation.q * m_requestedEntityMovement.t;
	}

	desiredLocalLocation.q.SetRotationZ(m_requestedEntityMovement.q.GetRotZ());

	return desiredLocalLocation;
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::CalculateParamsForCurrentMotions()
{
	if (m_doMotionParams)
	{
		ANIMCHAR_PROFILE;

		IEntity* pEntity = GetEntity();
		CRY_ASSERT(pEntity);

		ICharacterInstance* pCharacterInstance = pEntity->GetCharacter(0);
		if (pCharacterInstance == NULL)
			return;

		ISkeletonAnim* pSkeletonAnim = pCharacterInstance->GetISkeletonAnim();
		if (pSkeletonAnim == NULL)
			return;

		SetDesiredLocalLocation(pSkeletonAnim, m_desiredLocalLocation, (float)m_curFrameTime);
		if (m_pShadowSkeletonAnim)
			SetDesiredLocalLocation(m_pShadowSkeletonAnim, m_desiredLocalLocation, (float)m_curFrameTime);

#ifdef _DEBUG
		DebugGraphQT(m_desiredLocalLocation, "eDH_DesiredLocalLocationTX", "eDH_DesiredLocalLocationTY", "eDH_DesiredLocalLocationRZ");
		//DebugGraphMotionParams(pSkeletonAnim);
#endif
	}
}

//--------------------------------------------------------------------------------

#define MAX_EXEC_QUEUE (0x8u);

void CAnimatedCharacter::SetDesiredLocalLocation(ISkeletonAnim* pSkeletonAnim, const QuatT& desiredLocalLocation, float fDeltaTime)
{
	CRY_ASSERT(desiredLocalLocation.IsValid());

	{
		uint32 NumAnimsInQueue = pSkeletonAnim->GetNumAnimsInFIFO(0);
		uint32 nMaxActiveInQueue = MAX_EXEC_QUEUE;
		if (nMaxActiveInQueue > NumAnimsInQueue)
			nMaxActiveInQueue = NumAnimsInQueue;
		if (NumAnimsInQueue > nMaxActiveInQueue)
			NumAnimsInQueue = nMaxActiveInQueue;

		uint32 active = 0;
		for (uint32 a = 0; a < NumAnimsInQueue; a++)
		{
			const CAnimation& anim = pSkeletonAnim->GetAnimFromFIFO(0, a);
			active += anim.IsActivated() ? 1 : 0;
		}

		if (active > nMaxActiveInQueue)
			active = nMaxActiveInQueue;
		nMaxActiveInQueue = active;

		const SParametricSampler* pLMG = NULL;
		for (int32 a = nMaxActiveInQueue - 1; (a >= 0) && !pLMG; --a)
		{
			const CAnimation& anim = pSkeletonAnim->GetAnimFromFIFO(0, a);
			pLMG = anim.GetParametricSampler();
		}
	}

	const Vec3 dir = desiredLocalLocation.GetColumn1();
	float turnAngle = atan2f(-dir.x, dir.y);

	float turnSpeed = turnAngle / fDeltaTime;

	const Vec2 deltaVector(desiredLocalLocation.t.x, desiredLocalLocation.t.y);
	const float travelDist = deltaVector.GetLength();

	const Vec2 deltaDir = (travelDist > 0.0f) ? (deltaVector / travelDist) : Vec2(0, 0);
	float travelAngle = (deltaDir.x == 0.0f && deltaDir.y == 0.0f ? 0.0f : atan2f(-deltaDir.x, deltaDir.y));

	float travelSpeed;
	if (gEnv->bMultiplayer)
	{
		travelSpeed = travelDist / fDeltaTime;
	}
	else
	{
		const float cosGroundSlope = fabsf(cosf(m_fGroundSlopeMoveDirSmooth));
		travelSpeed = (cosGroundSlope > FLT_EPSILON) ? (travelDist / (fDeltaTime * cosGroundSlope)) : (travelDist / fDeltaTime);
	}

	// TravelAngle smoothing
	{
		const Vec2 newStrafe = Vec2(-sin_tpl(travelAngle), cos_tpl(travelAngle));
		if (CAnimationGraphCVars::Get().m_enableTurnAngleSmoothing)
		{
			SmoothCD(m_fDesiredStrafeSmoothQTX, m_fDesiredStrafeSmoothRateQTX, fDeltaTime, newStrafe, 0.10f);
		}
		else
		{
			m_fDesiredStrafeSmoothQTX = newStrafe;
			m_fDesiredStrafeSmoothRateQTX.zero();
		}
		travelAngle = Ang3::CreateRadZ(Vec2(0, 1), m_fDesiredStrafeSmoothQTX);
	}

	const bool modulateTurnSpeedByTravelAngle = true;
	if (modulateTurnSpeedByTravelAngle)
	{
		static float minimum = DEG2RAD(10.0f); // do not change turnSpeed when travelAngle is below this
		static float maximum = DEG2RAD(40.0f); // force turnspeed to zero when travelAngle is above this
		const float turnSpeedScale = 1.0f - clamp_tpl(fabsf(travelAngle) - minimum, 0.0f, maximum - minimum) / (maximum - minimum);
		turnSpeed *= turnSpeedScale;
	}

	// TurnSpeed smoothing
	{
		if (CAnimationGraphCVars::Get().m_enableTurnSpeedSmoothing)
		{
			SmoothCD(m_fDesiredTurnSpeedSmoothQTX, m_fDesiredTurnSpeedSmoothRateQTX, fDeltaTime, turnSpeed, 0.40f);
		}
		else
		{
			m_fDesiredTurnSpeedSmoothQTX = turnSpeed;
			m_fDesiredTurnSpeedSmoothRateQTX = 0.0f;
		}
	}

	// TravelSpeed smoothing
	{
		if (CAnimationGraphCVars::Get().m_enableTravelSpeedSmoothing)
		{
			SmoothCD(m_fDesiredMoveSpeedSmoothQTX, m_fDesiredMoveSpeedSmoothRateQTX, fDeltaTime, travelSpeed, 0.04f);
		}
		else
		{
			m_fDesiredMoveSpeedSmoothQTX = travelSpeed;
			m_fDesiredMoveSpeedSmoothRateQTX = 0.0f;
		}
	}

	SetMotionParam(pSkeletonAnim, eMotionParamID_TravelSpeed, m_fDesiredMoveSpeedSmoothQTX);
	SetMotionParam(pSkeletonAnim, eMotionParamID_TurnSpeed, m_fDesiredTurnSpeedSmoothQTX * CAnimationGraphCVars::Get().m_turnSpeedParamScale);
	SetMotionParam(pSkeletonAnim, eMotionParamID_TravelAngle, travelAngle);
	// eMotionParamID_TravelSlope
	SetMotionParam(pSkeletonAnim, eMotionParamID_TurnAngle, turnAngle);
	// eMotionParamID_TravelDist
	SetMotionParam(pSkeletonAnim, eMotionParamID_StopLeg, 0);
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::SetMotionParam(ISkeletonAnim* const pSkeletonAnim, const EMotionParamID motionParamID, const float value) const
{
	// Allow parameters to be overridden by the movement request
	float overriddenValue(value);
	m_moveRequest.prediction.GetParam(motionParamID, overriddenValue);

	// Set desired parameter (note: the last parameter is not used anymore so put to 0.0f)
	pSkeletonAnim->SetDesiredMotionParam(motionParamID, overriddenValue, 0.0f);
}

void CAnimatedCharacter::SetBlendWeightParam(const EMotionParamID motionParamID, const float value, const uint8 targetFlags /* = eBWPT_All*/)
{
	if (motionParamID >= eMotionParamID_BlendWeight && motionParamID <= eMotionParamID_BlendWeight_Last)
	{
		//only allowed on the generic blend weights params

		IEntity* pEntity = GetEntity();
		CRY_ASSERT(pEntity);

		if (targetFlags & eBWPT_FirstPersonSkeleton)
		{
			if (ICharacterInstance* pCharacterInstance = pEntity->GetCharacter(0))
			{
				if (ISkeletonAnim* pSkeletonAnim = pCharacterInstance->GetISkeletonAnim())
				{
					pSkeletonAnim->SetDesiredMotionParam(motionParamID, value, 0.0f);
				}
			}
		}

		if (targetFlags & eBWPT_ShadowSkeleton)
		{
			if (m_hasShadowCharacter)
			{
				if (ICharacterInstance* pShadowCharacter = pEntity->GetCharacter(m_shadowCharacterSlot))
				{
					if (ISkeletonAnim* pShadowSkeletonAnim = pShadowCharacter->GetISkeletonAnim())
					{
						pShadowSkeletonAnim->SetDesiredMotionParam(motionParamID, value, 0.0f);
					}
				}
			}
		}

	}
	else
	{
		CryLogAlways("[CryAnimation] ERROR: motionParam that was sent: %d is not within allowed range of blendweights %d to %d", motionParamID, eMotionParamID_BlendWeight, eMotionParamID_BlendWeight_Last);
	}
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   LookAim_Helper.h
//  Version:     v1.00
//  Created:     25/01/2010 by Sven Van Soom
//  Description: Helper class for setting up and updating Looking and Aiming
//
//		Currently it's only a very thin wrapper adding blending to the 
//		LookAtSimple pose modifier to allow combined looking & aiming. It still
//		uses the old LookIK system when available.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"

#include <CryAnimation/ICryAnimation.h>
#include "LookAim_Helper.h"
#include "Player.h"
#include <CryExtension/CryCreateClassInstance.h>


CLookAim_Helper::CLookAim_Helper()
	: m_initialized(false)
	, m_canUseLookAtComplex(false)
	, m_canUseLookAtSimple(false)
	, m_lookAtWeight(0.0f)
	, m_lookAtFadeInSpeed(0.0f)
	, m_lookAtFadeOutSpeed(0.0f)
	, m_lookAtInterpolatedTargetGlobal(ZERO)
	, m_lookAtTargetRate(ZERO)
	, m_lookAtTargetGlobal(ZERO)
	, m_lookAtTargetSmoothTime(0.0f)
	, m_availableHandsForFiring((1 << IItem::eIH_Left) | (1 << IItem::eIH_Right))
	, m_lastAimPoseAnimID(0)
	, m_lastAimYaw(0.0f)
	, m_aimIsSwitchingArms(false)
	, m_vLastAimTarget(ZERO)
{
}


void CLookAim_Helper::UpdateLook(CPlayer* pPlayer, ICharacterInstance* pCharacter, bool bEnabled, f32 FOV, const Vec3& targetGlobal, const uint32 lookIKLayer)
{
	if (!m_initialized)
	{
		Init(pPlayer, pCharacter);
	}

	bool useLookAtComplex;
	bool useLookAtSimple;

	if (m_canUseLookAtComplex)
	{
		// for now just use the old 'complex' look at method until we sort out how to properly blend old and new look at
		useLookAtComplex = true;
		useLookAtSimple = false;
	}
	else
	{
		useLookAtComplex = true; // for backwards compatibility reasons we still update the old look-at even when m_canUseLookAtComplex is false
		useLookAtSimple = m_canUseLookAtSimple;
	}

	// ---------------------------
	// Complex (old style) Look-At
	// ---------------------------

	ISkeletonPose * pSkeletonPose = pCharacter->GetISkeletonPose();
	IAnimationPoseBlenderDir* pIPoseBlenderLook = pSkeletonPose->GetIPoseBlenderLook();
	if (pIPoseBlenderLook)
	{
		bool state = useLookAtComplex && bEnabled;
		pIPoseBlenderLook->SetState(state);
		if (state)
		{
			pIPoseBlenderLook->SetFadeoutAngle(FOV);   //IMPORTANT:: the parameter needs to be in radians. So use DEG2RAD(FOV)
			pIPoseBlenderLook->SetTarget(targetGlobal);
			pIPoseBlenderLook->SetLayer(lookIKLayer);
		}
	}

	// ---------------------------
	// Simple Head-Only Look-At
	// ---------------------------
	if (m_canUseLookAtSimple)
	{
		float frameTime = gEnv->pTimer->GetFrameTime();

		// Fade In/Out the Weight
		m_lookAtWeight = bEnabled ? CLAMP(m_lookAtWeight + (frameTime * m_lookAtFadeInSpeed), 0.0f, 1.0f) : CLAMP(m_lookAtWeight - (frameTime * m_lookAtFadeOutSpeed), 0.0f, 1.0f);

		// Blend To The Target
		if (targetGlobal.IsValid())
		{
			m_lookAtTargetGlobal = targetGlobal;
		}
		SmoothCD(m_lookAtInterpolatedTargetGlobal, m_lookAtTargetRate, frameTime, m_lookAtTargetGlobal, m_lookAtTargetSmoothTime);

		// Push the LookAtSimple PoseModifier
		if (useLookAtSimple && (m_lookAtWeight > 0.0f))
		{
			m_lookAtSimple->SetTargetGlobal(m_lookAtInterpolatedTargetGlobal);
			m_lookAtSimple->SetWeight(m_lookAtWeight);
			pCharacter->GetISkeletonAnim()->PushPoseModifier(15, m_lookAtSimple, "LookAim_Helper");
		}
	}
}


void CLookAim_Helper::Init(CPlayer* pPlayer, ICharacterInstance* pCharacter)
{
	if (m_initialized)
		return;

	m_initialized = true;

	// Looking
	SActorParams &params = pPlayer->GetActorParams();
	int16 lookAtSimpleHeadJoint = pCharacter->GetIDefaultSkeleton().GetJointIDByName(params.lookAtSimpleHeadBoneName);

	m_canUseLookAtSimple = (lookAtSimpleHeadJoint != -1);
	m_canUseLookAtComplex = params.canUseComplexLookIK;

	if (m_canUseLookAtSimple)
	{
		if (!m_lookAtSimple.get())
			CryCreateClassInstance<AnimPoseModifier::CLookAtSimple>(AnimPoseModifier::CLookAtSimple::GetCID(), m_lookAtSimple);

		m_lookAtWeight = 1.0;
		m_lookAtFadeInSpeed = 2.0f; // fade in in 0.5 second(s)
		m_lookAtFadeOutSpeed = 2.0f; // fade out in 0.5 second(s)
		m_lookAtTargetSmoothTime = 0.1f; // smoothly blend towards target in this amount of seconds
		m_lookAtInterpolatedTargetGlobal.zero();
		m_lookAtTargetRate.zero();
		m_lookAtTargetGlobal.zero();
		m_lookAtSimple->SetJointId(lookAtSimpleHeadJoint);
	}

	// Aiming
	m_availableHandsForFiring = (1<<IItem::eIH_Left) | (1<<IItem::eIH_Right);
	m_lastAimPoseAnimID = 0;
	m_lastAimYaw = 0.0f;
	m_aimIsSwitchingArms = false;
	m_vLastAimTarget.Set(0.0f, 0.0f, 0.0f);
}


void CLookAim_Helper::UpdateDynamicAimPoses(CPlayer* pPlayer, ICharacterInstance* pCharacter, const SActorParams& params, const bool aimEnabled, const int32 aimIKLayer, const Vec3& vAimTarget, const SMovementState& curMovementState)
{
	if (!m_initialized)
	{
		Init(pPlayer, pCharacter);
	}

	CRY_ASSERT(params.useDynamicAimPoses);

	bool isIdle = curMovementState.desiredSpeed < 0.1f;
	const SActorParams::SDynamicAimPose& pose = isIdle ? params.idleDynamicAimPose : params.runDynamicAimPose;

	const float AIM_BLEND_IN_TIME = 0.3f;
	const float AIM_BLEND_OVER_TIME = 0.4f;
	const float AIM_BLEND_OUT_TIME = 0.4f;

	const float frameTime = gEnv->pTimer->GetFrameTime();

	ISkeletonPose* pISkeletonPose = pCharacter->GetISkeletonPose();
	IAnimationPoseBlenderDir* pIPoseBlenderAim = pISkeletonPose->GetIPoseBlenderAim();
	if (pIPoseBlenderAim)
	{
		pIPoseBlenderAim->SetLayer(aimIKLayer);
		// No need to set fade-out and fade-in speed, blend-time, fadeoutangle, as they are not used
	}

	const Vec3 vAimLocal = pPlayer->GetAnimatedCharacter()->GetAnimLocation().q.GetInverted() * curMovementState.aimDirection.GetNormalized();
	float yaw = atan2(vAimLocal.x, vAimLocal.y);
	float pitch = asin(vAimLocal.z);

	if (aimEnabled && !m_aimIsSwitchingArms && ((m_lastAimYaw*yaw < 0) && (fabsf(m_lastAimYaw)>DEG2RAD(90) )))
	{
		m_aimIsSwitchingArms = true;
	}

	ISkeletonAnim* pISkeletonAnim = pCharacter->GetISkeletonAnim();
	if (aimEnabled && !m_aimIsSwitchingArms)
	{
		CryCharAnimationParams animParams;

		animParams.m_nLayerID = aimIKLayer;
		animParams.m_nFlags |= CA_LOOP_ANIMATION;
		animParams.m_nFlags |= CA_ALLOW_ANIM_RESTART;

		const char* animName = pose.bothArmsAimPose;
		m_availableHandsForFiring = (1<<IItem::eIH_Left) | (1<<IItem::eIH_Right);

		if (fabsf(yaw) > params.bothArmsAimHalfFOV + params.bothArmsAimPitchFactor*fabsf(pitch))
		{
			if (yaw < 0)
			{
				animName = pose.leftArmAimPose;
				if (pose.leftArmAimPose != pose.bothArmsAimPose)
					m_availableHandsForFiring = (1<<IItem::eIH_Left);
			}
			else
			{
				animName = pose.rightArmAimPose;
				if (pose.rightArmAimPose != pose.bothArmsAimPose)
					m_availableHandsForFiring = (1<<IItem::eIH_Right);
			}
		}

		IAnimationSet* pAnimSet = pCharacter->GetIAnimationSet();
		const int animID = pAnimSet->GetAnimIDByName(animName);
		if (animID != m_lastAimPoseAnimID)
		{
			if (m_lastAimPoseAnimID == 0)
				animParams.m_fTransTime = AIM_BLEND_IN_TIME;
			else
				animParams.m_fTransTime = AIM_BLEND_OVER_TIME;

			pISkeletonAnim->StartAnimationById(animID, animParams);
			m_lastAimPoseAnimID = animID;
		}
	}
	else
	{
		pISkeletonAnim->StopAnimationInLayer(aimIKLayer,AIM_BLEND_OUT_TIME);
		m_availableHandsForFiring = 0;
		m_lastAimPoseAnimID = 0;
	}

	bool isAimPosePlaying = (pISkeletonAnim->GetNumAnimsInFIFO(aimIKLayer) > 0);
	if (m_aimIsSwitchingArms && !isAimPosePlaying)
	{
		m_aimIsSwitchingArms = false;
	}


	if (pIPoseBlenderAim)
	{
		if (!m_aimIsSwitchingArms)
		{
			pIPoseBlenderAim->SetState(isAimPosePlaying);
			if (isAimPosePlaying)
				pIPoseBlenderAim->SetTarget(vAimTarget);
			m_vLastAimTarget = vAimTarget;
		}
		else
		{
			pIPoseBlenderAim->SetState(true);
			pIPoseBlenderAim->SetTarget(m_vLastAimTarget);
		}
	}

	m_lastAimYaw = yaw;
}

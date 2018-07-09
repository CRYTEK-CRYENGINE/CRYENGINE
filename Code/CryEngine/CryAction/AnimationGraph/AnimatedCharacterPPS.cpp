// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryExtension/CryCreateClassInstance.h>
#include "AnimatedCharacter.h"
#include "CryAction.h"
#include "AnimationGraphCVars.h"
#include "PersistantDebug.h"
#include <CryAnimation/IFacialAnimation.h>
#include "AnimatedCharacterEventProxies.h"

#include "CryActionCVars.h"

#include "DebugHistory.h"

#include <IViewSystem.h>
#include <CryAISystem/IAIObject.h>

#include "../Animation/PoseAligner/PoseAligner.h"

#define UNKNOWN_GROUND_HEIGHT -1E10f
//#define USE_PROCEDURAL_LANDING

//--------------------------------------------------------------------------------

bool CAnimatedCharacter::EvaluateSimpleMovementConditions() const
{
	IEntity* pEntity = GetEntity();
	CRY_ASSERT(pEntity);

	if ((m_pAnimTarget != NULL) && m_pAnimTarget->activated)
		return false;

	EMovementControlMethod eMvmt = GetMCMH();
	if (eMvmt == eMCM_Animation || eMvmt == eMCM_AnimationHCollision)
		return false;

	if (pEntity->IsHidden() && !(pEntity->GetFlags() & ENTITY_FLAG_UPDATE_HIDDEN))
		return true;

	if (gEnv->IsDedicated())
		return true;

	if (CAnimationGraphCVars::Get().m_forceSimpleMovement != 0)
		return true;

	//--- Don't do simplified movement for the player client
	if (m_isPlayer && GetEntityId() == CCryAction::GetCryAction()->GetClientActorId())
		return false;

	if ((m_pCharacter == NULL) || !m_pCharacter->IsCharacterVisible())
		return true;

	return false;
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::UpdateSimpleMovementConditions()
{
	bool simplifyMovement = EvaluateSimpleMovementConditions();
	bool forceDisableSlidingContactEvents = (CAnimationGraphCVars::Get().m_disableSlidingContactEvents != 0);

	// TODO: For some reason the current frame id is always one more than the last reset frame id, should be the same, afaik.
	if ((m_simplifyMovement != simplifyMovement) ||
	    (m_forceDisableSlidingContactEvents != forceDisableSlidingContactEvents) ||
	    ((m_lastResetFrameId + 5) >= m_curFrameID) ||
	    !m_bSimpleMovementSetOnce) // Loading a singleplayer map in game takes too long between character reset and update.
	{
		m_bSimpleMovementSetOnce = true;

		m_forceDisableSlidingContactEvents = forceDisableSlidingContactEvents;
		m_simplifyMovement = simplifyMovement;

		GetGameObject()->SetUpdateSlotEnableCondition(this, 0, m_simplifyMovement ? eUEC_Never : eUEC_Always);

		IEntity* pEntity = GetEntity();
		//string name = pEntity->GetName();
		//CryLogAlways("AC[%s]: simplified movement %s!", name, (m_simplifyMovement ? "enabled" : "disabled"));
		IPhysicalEntity* pPhysEnt = pEntity->GetPhysics();
		if ((pPhysEnt != NULL) && (pPhysEnt->GetType() == PE_LIVING))
		{
			pe_params_flags pf;

			if (m_simplifyMovement || m_forceDisableSlidingContactEvents)
			{
				pf.flagsAND = ~lef_report_sliding_contacts;
				//CryLogAlways("AC[%s]: events disabled!", name);
			}
			else
			{
				pf.flagsOR = lef_report_sliding_contacts;
				//CryLogAlways("AC[%s]: events enabled!", name);
			}

			pPhysEnt->SetParams(&pf);
		}
	}
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//- PreAnimationUpdate -----------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

void CAnimatedCharacter::PreAnimationUpdate()
{
	ANIMCHAR_PROFILE;

	if (m_curFrameTime <= 0.0f)
		return;

	AcquireRequestedBehaviourMovement();

	m_desiredLocalLocation = CalculateDesiredLocation();
}

float CAnimatedCharacter::GetAngularSpeedHorizontal() const
{
	const float prevFrameTimeInv = (float)__fsel(-(float)m_prevFrameTime, 0.0f, __fres((float)m_prevFrameTime + FLT_MIN));
	return m_actualEntMovement.q.GetRotZ() * prevFrameTimeInv;
}

void CAnimatedCharacter::UpdateSkeletonSettings()
{
	ANIMCHAR_PROFILE_SCOPE("UpdateSkeletonSettings");

	if (m_pSkeletonAnim != NULL)
	{
		m_pSkeletonAnim->SetAnimationDrivenMotion(1); // Tell motion playback to calculate root/locator trajectory.

		if (m_isPlayer && GetEntityId() == CCryAction::GetCryAction()->GetClientActorId())
		{
			// Force the client skeleton to update always, even when seemingly invisible
			m_pSkeletonPose->SetForceSkeletonUpdate(ISkeletonPose::kForceSkeletonUpdatesInfinitely);
		}
	}
	if (m_pShadowSkeletonAnim != NULL)
	{
		m_pShadowSkeletonAnim->SetAnimationDrivenMotion(1); // Tell motion playback to calculate root/locator trajectory.
	}

	if (m_pPoseAligner)
	{
		m_pPoseAligner->Clear();
	}
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::UpdateTime()
{
	float frameTime = gEnv->pTimer->GetFrameTime();
	m_curFrameTimeOriginal = frameTime;
	frameTime = (float) __fsel(-frameTime, 0.0001f, frameTime);

	CRY_ASSERT(NumberValid(frameTime));

	m_curFrameID = gEnv->nMainFrameID;

	// This is cached to be used by SelectLocomotionState, to not touch global timer more than once per frame.
	m_curFrameStartTime = gEnv->pTimer->GetFrameStartTime();

	m_prevFrameTime = m_curFrameTime;
	m_curFrameTime = frameTime;

	m_elapsedTimeMCM[eMCMComponent_Horizontal] += (float)m_prevFrameTime;
	m_elapsedTimeMCM[eMCMComponent_Vertical] += (float)m_prevFrameTime;

#ifdef _DEBUG
	DebugHistory_AddValue("eDH_FrameTime", (float)m_curFrameTime);

	if (DebugTextEnabled())
	{
		const ColorF cWhite = ColorF(1, 1, 1, 1);
		IRenderAuxText::Draw2dLabel(10, 50, 2.0f, (float*)&cWhite, false, "FrameTime Cur[%f] Prev[%f]", m_curFrameTime, m_prevFrameTime);
	}
#endif
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::AcquireRequestedBehaviourMovement()
{
	ANIMCHAR_PROFILE_DETAILED;

	m_requestedEntityMovementType = RequestedEntMovementType_Undefined;
	m_requestedIJump = 0;

	if (m_moveRequestFrameID < m_curFrameID)
	{
		m_requestedEntityMovement.SetIdentity();
		return;
	}

	CRY_ASSERT(m_moveRequest.velocity.IsValid());
	CRY_ASSERT(m_moveRequest.rotation.IsValid());

	m_requestedEntityMovement.q = m_moveRequest.rotation;
	m_requestedEntityMovement.t.zero();

	m_bDisablePhysicalGravity = (m_moveRequest.type == eCMT_Swim);

	switch (m_moveRequest.type)
	{
	case eCMT_None:
		CRY_ASSERT(false);
		break;

	case eCMT_Normal:
		CRY_ASSERT(m_requestedEntityMovementType != RequestedEntMovementType_Impulse);
		m_requestedEntityMovementType = RequestedEntMovementType_Absolute;
		m_requestedEntityMovement.t = m_moveRequest.velocity;
		break;

	case eCMT_Fly:
	case eCMT_Swim:
	case eCMT_ZeroG:
		CRY_ASSERT(m_requestedEntityMovementType != RequestedEntMovementType_Impulse);
		m_requestedEntityMovementType = RequestedEntMovementType_Absolute;
		m_requestedEntityMovement.t = m_moveRequest.velocity;
		m_requestedIJump = 3;
		break;

	case eCMT_JumpAccumulate:
		CRY_ASSERT(m_requestedEntityMovementType != RequestedEntMovementType_Impulse);
		m_requestedEntityMovementType = RequestedEntMovementType_Absolute;
		m_requestedEntityMovement.t = m_moveRequest.velocity;
		m_requestedIJump = 1;
		break;

	case eCMT_JumpInstant:
		CRY_ASSERT(m_requestedEntityMovementType != RequestedEntMovementType_Impulse);
		m_requestedEntityMovementType = RequestedEntMovementType_Absolute;
		m_requestedEntityMovement.t = m_moveRequest.velocity;
		m_requestedIJump = 2;
		break;

	case eCMT_Impulse:
		CRY_ASSERT(m_requestedEntityMovementType != RequestedEntMovementType_Absolute);
		m_requestedEntityMovementType = RequestedEntMovementType_Impulse;
		CRY_ASSERT(m_curFrameTime > 0.0f);
		if (m_curFrameTime > 0.0f)
		{
			// TODO: Impulses are per frame at the moment, while normal movement is per second.
			// NOTE: Not anymore =). Impulses are now velocity per second through out the player/alien code.
			m_requestedEntityMovement.t = m_moveRequest.velocity /* / m_curFrameTime*/;
		}
		break;
	}

	CRY_ASSERT(m_requestedEntityMovement.IsValid());

	// rotation is per frame (and can't be per second, since Quat's can only represent a max angle of 360 degrees),
	// Convert velocity from per second into per frame.
	m_requestedEntityMovement.t *= (float)m_curFrameTime;

	if (NoMovementOverride())
	{
		m_requestedEntityMovement.SetIdentity();
		m_requestedEntityMovementType = RequestedEntMovementType_Absolute;
		m_requestedIJump = 0;
	}

#if DEBUG_VELOCITY()
	if (DebugVelocitiesEnabled())
		AddDebugVelocity(m_requestedEntityMovement, (float)m_curFrameTime, "AnimatedCharacted Input", Col_Yellow);
#endif

#ifdef _DEBUG
	DebugGraphQT(m_requestedEntityMovement, "eDH_ReqEntMovementTransX", "eDH_ReqEntMovementTransY", "eDH_ReqEntMovementRotZ");

	if (DebugTextEnabled())
	{
		Ang3 requestedEntityRot(m_requestedEntityMovement.q);
		const ColorF cWhite = ColorF(1, 1, 1, 1);
		IRenderAuxText::Draw2dLabel(350, 50, 2.0f, (float*)&cWhite, false, "Req Movement[%.2f, %.2f, %.2f | %.2f, %.2f, %.2f]",
		                            m_requestedEntityMovement.t.x / m_curFrameTime, m_requestedEntityMovement.t.y / m_curFrameTime, m_requestedEntityMovement.t.z / m_curFrameTime,
		                            RAD2DEG(requestedEntityRot.x), RAD2DEG(requestedEntityRot.y), RAD2DEG(requestedEntityRot.z));
	}
#endif

	CRY_ASSERT(m_requestedEntityMovement.IsValid());
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//- PostAnimationUpdate ----------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

void CAnimatedCharacter::PostAnimationUpdate()
{
	//ANIMCHAR_PROFILE_SCOPE("SetAnimGraphspeedInputs_Simplified");
	ANIMCHAR_PROFILE;

	if (m_curFrameTime <= 0.0f)
		return;
}

void CAnimatedCharacter::ComputeGroundSlope()
{
	const float time = (float)m_curFrameTime;

	IEntity* pEntity = GetEntity();
	CRY_ASSERT(pEntity != NULL);
	IPhysicalEntity* pPhysEntity = pEntity->GetPhysics();

	Vec3 normal = Vec3(0.0f, 0.0f, 1.0f);
	if (pPhysEntity && pPhysEntity->GetType() == PE_LIVING)
	{
		pe_status_living status;
		pPhysEntity->GetStatus(&status);
		normal = status.groundSlope;
	}

	Vec3 up(0, 0, 1);
	Vec3 rootNormal = normal;
	Vec3 groundNormal = rootNormal * m_entLocation.q;
	groundNormal.x = 0.0f;
	float cosine = up | groundNormal;
	Vec3 sine = up % groundNormal;
	float groundSlope = atan2f(sgn(sine.x) * sine.GetLength(), cosine);
	SmoothCD(m_fGroundSlopeMoveDirSmooth, m_fGroundSlopeMoveDirRate, time, groundSlope, 0.20f);

	m_pSkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelSlope, m_fGroundSlopeMoveDirSmooth, time);

	float fGroundAngle = RAD2DEG(acos_tpl(rootNormal.z));
	if (fGroundAngle > 35.0f)
		fGroundAngle = 35.0f;
	SmoothCD(m_fGroundSlopeSmooth, m_fGroundSlopeRate, time, fGroundAngle, 0.20f);
}

void CAnimatedCharacter::PostProcessingUpdate()
{
	ANIMCHAR_PROFILE;

	IEntity* pEntity = GetEntity();
	CRY_ASSERT(pEntity != NULL);

	if (m_pSkeletonAnim == NULL)
		return;
	if (m_pSkeletonPose == NULL)
		return;

	ComputeGroundSlope();

	if (CAnimationGraphCVars::Get().m_groundAlignAll == 0)
		return;

	bool fallen = false;
	bool isFirstPerson = false;
	bool allowLandBob = false;
	bool isMultiplayer = gEnv->bMultiplayer;
	if (IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(pEntity->GetId()))
	{
		fallen = pActor->IsFallen();
		isFirstPerson = pActor->IsThirdPerson() == false;
		allowLandBob = pActor->AllowLandingBob();
	}

	bool bWasDisabled = (m_lastPostProcessUpdateFrameID != m_curFrameID - 1);
	m_lastPostProcessUpdateFrameID = m_curFrameID;

	if (bWasDisabled)
	{
		m_fJumpSmooth = 0.0f;
		m_fJumpSmoothRate = 0.0f;
	}

	const float JUMP_SMOOTH_RATE = 0.30f;
	SmoothCD(m_fJumpSmooth, m_fJumpSmoothRate, (float)m_curFrameTime, f32(m_moveRequest.jumping), JUMP_SMOOTH_RATE);
	if (m_moveRequest.jumping)
		m_fJumpSmooth = 1.0f;

	//

	IPhysicalEntity* pPhysEnt = pEntity->GetPhysics();
	if (!pPhysEnt)
		return;
	if (pPhysEnt->GetType() != PE_LIVING)
		return;

	if (!m_pPoseAligner)
		return;
	if (!m_pPoseAligner->Initialize(*pEntity, m_pCharacter))
		return;

	float poseBlendWeight = clamp_tpl(1.0f - m_fJumpSmooth, 0.0f, 1.0f);

#ifdef USE_PROCEDURAL_LANDING

	// Procedural landing code

	pe_status_living status;
	pPhysEnt->GetStatus(&status);

	float entHeight = pEntity->GetWorldPos().z;
	float groundHeight = status.groundHeight;
	bool groundHeightValid = !status.bFlying;

	bool snapAlignFeet = false;
	float landingOffset = 0.0f;

	if (allowLandBob && m_landBobParams.IsValid())
	{
		const f32 IN_AIR_HEIGHT[2] = { 0.2f, 0.1f };

		if (pPhysEnt != NULL)
		{
			bool inAir = status.bFlying != 0;

			//			IRenderAuxText::Draw2dLabel(10, 100, 1.3f, (float*)&Vec4(1, 1, 1, 1), false, "Ground: %f Ent: %f", groundHeight, entHeight);

			if (inAir != m_wasInAir)
			{
				m_wasInAir = inAir;

				if (!inAir)
				{
					m_landBobTime = m_landBobParams.maxTime;
					float fallFactor = clamp_tpl((m_fallMaxHeight - entHeight) / m_landBobParams.maxFallDist, 0.0f, 1.0f);
					m_totalLandBob = fallFactor * clamp_tpl(m_landBobParams.maxBob, 0.0f, 0.3f);

					snapAlignFeet = true;
				}
				else
				{
					m_fallMaxHeight = entHeight;
				}
			}

			if (inAir)
			{
				m_fallMaxHeight = max(entHeight, m_fallMaxHeight);
			}
		}

		if (m_landBobTime > 0.0f)
		{
			float landingBobFactor = CAnimationGraphCVars::Get().m_landingBobTimeFactor;
			float landingBobLandFactor = CAnimationGraphCVars::Get().m_landingBobLandTimeFactor;
			CRY_ASSERT_MESSAGE((landingBobFactor >= 0.0f) && (landingBobFactor <= 1.0f), "Invalid value for m_landingBobTimeFactor cvar, must be 0..1");
			landingBobFactor = clamp_tpl(landingBobFactor, 0.0f, 1.0f);
			landingBobLandFactor = clamp_tpl(landingBobLandFactor, 0.0f, landingBobFactor);

			m_landBobTime = max(m_landBobTime - (float)m_curFrameTime, 0.0f);

			f32 t = clamp_tpl(1.0f - (m_landBobTime / m_landBobParams.maxTime), 0.0f, 1.0f);
			if (t > landingBobFactor)
			{
				f32 st = 1.0f - ((t - landingBobFactor) / (1.0f - landingBobFactor));
				t = (1.0f + sin_tpl((st - 0.5f) * gf_PI)) * 0.5f;
			}
			else
			{
				t = min(t / landingBobLandFactor, 1.0f);
			}
			landingOffset -= t * m_totalLandBob;
			snapAlignFeet = true;

			//			IRenderAuxText::Draw2dLabel(10, 220, 1.3f, (float*)&Vec4(1, 1, 1, 1), false, "FT: %f t: %f", m_landBobTime, t);
		}
	}
	else
	{
		m_wasInAir = false;
	}

	if (allowLandBob)
	{
		if (snapAlignFeet)
			poseBlendWeight = 1.0f;

		PoseAligner::CPose* pPoseAligner = (PoseAligner::CPose*)m_pPoseAligner.get();
		pPoseAligner->SetRootOffsetAdditional(Vec3(0.0f, 0.0f, landingOffset));

		const uint chainCount = pPoseAligner->GetChainCount();
		for (uint i = 0; i < chainCount; ++i)
		{
			pPoseAligner->GetChain(i)->SetTargetForceWeightOne(snapAlignFeet);
		}
	}

#endif

	// PoseAligner

	m_pPoseAligner->SetRootOffsetEnable(m_groundAlignmentParams.IsFlag(eGA_PoseAlignerUseRootOffset));
	m_pPoseAligner->SetBlendWeight(poseBlendWeight);
	m_pPoseAligner->Update(m_pCharacter, m_entLocation, (float)m_curFrameTime);
}

//--------------------------------------------------------------------------------

QuatT CAnimatedCharacter::CalculateDesiredAnimMovement() const
{
	ANIMCHAR_PROFILE_DETAILED;

	IEntity* pEntity = GetEntity();
	CRY_ASSERT(pEntity);
	IEntity* pParent = pEntity->GetParent();

	QuatT desiredAnimMovement(IDENTITY);

	// If simplified, anim location will be set to entity anyway, so we don't need any of this.
	if (m_simplifyMovement)
		return desiredAnimMovement;

	// ASSET MOVEMENT
	if (m_pSkeletonAnim != NULL)
	{
		QuatT assetAnimMovement;
		if (CAnimationGraphCVars::Get().m_useMovementPrediction)
		{
			// TODO: Figure out how many frames ahead the physics request we're calculating is
			//   This depends on how the CSystem::Update() is implemented, which cvars are set, and which platform we run on,
			//   so this information should be passed along somehow by the CSystem.
			//   For now we assume physics request have exactly 1 frame of delay.

			if (CAnimationGraphCVars::Get().m_useQueuedRotation)
			{
				assetAnimMovement = m_pSkeletonAnim->CalculateRelativeMovement((float)m_curFrameTime, 1);
			}
			else
			{
				assetAnimMovement = m_pSkeletonAnim->CalculateRelativeMovement((float)m_curFrameTime, 0);

				const QuatT futureAssetAnimMovement = m_pSkeletonAnim->CalculateRelativeMovement((float)m_curFrameTime, 1);

				assetAnimMovement.t = assetAnimMovement.q * futureAssetAnimMovement.t; // request the 'future' translation, properly adjusted for the fact that the rotation we got back from cryanimation is relative to the rotation of the current frame
			}
		}
		else
		{
			assetAnimMovement = m_pSkeletonAnim->CalculateRelativeMovement((float)m_curFrameTime, 0);
		}

		//
		assetAnimMovement.t = m_entLocation.q * assetAnimMovement.t;
		assetAnimMovement.q.Normalize();

		desiredAnimMovement = assetAnimMovement;

		/*{
		   IRenderAuxGeom*	pAuxGeom	= gEnv->pAuxGeomRenderer;
		   pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
		   pAuxGeom->DrawLine( desiredAnimMovement.t,RGBA8(0x00,0xff,0x00,0x00),desiredAnimMovement.t+Vec3(0,0,+20),RGBA8(0xff,0xff,0xff,0x00) );
		   }*/
	}

#ifdef _DEBUG
	DebugGraphQT(desiredAnimMovement, "eDH_AnimAssetTransX", "eDH_AnimAssetTransY", "eDH_AnimAssetRotZ", "eDH_AnimAssetTransZ");
#endif

	// This assert was removed, because Troppers use physicalized animation controlled movement to dodge.
	// Have not investigated what problems this might cause, them penetrating into obstacles, entity trying to catch up with animation, etc.
	// Luciano said it seems to work fine. We'll see...
	//CRY_ASSERT((GetMCMH() != eMCM_Animation) || (m_colliderMode != eColliderMode_Pushable));

	if (NoMovementOverride())
	{
		desiredAnimMovement.SetIdentity();
	}

	// This is the only point where the eMCM_AnimationHCollision differs from the eMCM_Animation
	// (though only when having 'atomic update')
	if (RecentCollision() && (GetMCMH() != eMCM_Animation) && (GetMCMH() != eMCM_AnimationHCollision))
	{
		desiredAnimMovement.t = RemovePenetratingComponent(desiredAnimMovement.t, m_collisionNormal[0]);
		desiredAnimMovement.t = RemovePenetratingComponent(desiredAnimMovement.t, m_collisionNormal[1]);
		desiredAnimMovement.t = RemovePenetratingComponent(desiredAnimMovement.t, m_collisionNormal[2]);
		desiredAnimMovement.t = RemovePenetratingComponent(desiredAnimMovement.t, m_collisionNormal[3]);
	}

	//DebugGraphQT(desiredAnimMovement, eDH_TEMP00, eDH_TEMP01, eDH_TEMP03); DebugHistory_AddValue(eDH_TEMP02, desiredAnimMovement.t.z);

	CRY_ASSERT(desiredAnimMovement.IsValid());
	return desiredAnimMovement;
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::CalculateAndRequestPhysicalEntityMovement()
{
	ANIMCHAR_PROFILE_DETAILED;

	const QuatT desiredAnimMovement = CalculateDesiredAnimMovement();

#if DEBUG_VELOCITY()
	if (DebugVelocitiesEnabled())
		AddDebugVelocity(desiredAnimMovement, (float)m_curFrameTime, "Anim Movement", Col_Blue);
#endif

	const QuatT wantedEntMovement = CalculateWantedEntityMovement(desiredAnimMovement);

	RequestPhysicalEntityMovement(wantedEntMovement);
}

//--------------------------------------------------------------------------------

QuatT CAnimatedCharacter::CalculateWantedEntityMovement(const QuatT& desiredAnimMovement) const
{
	ANIMCHAR_PROFILE_DETAILED;

	if (NoMovementOverride())
	{
#if DEBUG_VELOCITY()
		if (DebugVelocitiesEnabled())
			AddDebugVelocity(IDENTITY, (float)m_curFrameTime, "Wanted: NoMovementOverride", Col_GreenYellow);
#endif
		return IDENTITY;
	}

	CRY_ASSERT(desiredAnimMovement.IsValid());

#ifdef _DEBUG
	if (DebugFilter())
	{
		DebugHistory_AddValue("eDH_TEMP00", desiredAnimMovement.t.z);
		DebugHistory_AddValue("eDH_TEMP01", m_expectedEntMovement.z);
		DebugHistory_AddValue("eDH_TEMP02", m_actualEntMovement.t.z);
		DebugHistory_AddValue("eDH_TEMP10", m_requestedEntityMovement.t.z);
	}
#endif

	QuatT overriddenEntMovement = OverrideEntityMovement(m_requestedEntityMovement, desiredAnimMovement);

	QuatT wantedEntMovement;
	if ((desiredAnimMovement.t == overriddenEntMovement.t) && (desiredAnimMovement.q == overriddenEntMovement.q))
	{
		wantedEntMovement = overriddenEntMovement;
	}
	else
	{
		wantedEntMovement = MergeMCM(overriddenEntMovement, desiredAnimMovement, false);
	}
	CRY_ASSERT(wantedEntMovement.IsValid());

	// Entity Clamping
	if (AllowEntityClamping())
		wantedEntMovement = DoEntityClamping(wantedEntMovement);

	// Forced Movement (do we really need this?)
	if (m_hasForcedMovement)
	{
#if DEBUG_VELOCITY()
		if (DebugVelocitiesEnabled())
		{
			AddDebugVelocity(m_forcedMovementRelative, (float)m_curFrameTime, "ForcedMovement (Relative", Col_IndianRed);
		}
#endif

		wantedEntMovement = ApplyWorldOffset(wantedEntMovement, m_forcedMovementRelative);
	}

#if DEBUG_VELOCITY()
	if (DebugVelocitiesEnabled())
	{
		EMovementControlMethod mcmh = GetMCMH();
		EMovementControlMethod mcmv = GetMCMV();
		char* szMcmH = "???";
		switch (mcmh)
		{
		case eMCM_Entity:
			szMcmH = "ENT";
			break;
		case eMCM_ClampedEntity:
			szMcmH = "ENT_CL";
			break;
		case eMCM_SmoothedEntity:
			szMcmH = "ENT_SM";
			break;
		case eMCM_DecoupledCatchUp:
			szMcmH = "DEC";
			break;
		case eMCM_Animation:
			szMcmH = "ANM";
			break;
		case eMCM_AnimationHCollision:
			szMcmH = "ANM_HC";
			break;
		}
		char* szMcmV = "???";
		switch (mcmv)
		{
		case eMCM_Entity:
			szMcmV = "ENT";
			break;
		case eMCM_ClampedEntity:
			szMcmV = "ENT_CL";
			break;
		case eMCM_SmoothedEntity:
			szMcmV = "ENT_SM";
			break;
		case eMCM_DecoupledCatchUp:
			szMcmV = "DEC";
			break;
		case eMCM_Animation:
			szMcmV = "ANM";
			break;
		case eMCM_AnimationHCollision:
			szMcmV = "ANM_HC";
			break;
		}

		char buffer[256];
		const char* const mcmHTag = m_currentMovementControlMethodTags[eMCMComponent_Horizontal];
		const char* const mcmVTag = m_currentMovementControlMethodTags[eMCMComponent_Vertical];
		cry_sprintf(buffer, "Wanted: Hor:%s%s%s(%s) Ver:%s%s(%s)", szMcmH, m_moveOverride_useAnimXY ? "+ANMXY" : "", m_moveOverride_useAnimRot ? "+ANMQ" : "", mcmHTag, szMcmV, m_moveOverride_useAnimZ ? "+ANMZ" : "", mcmVTag);

		AddDebugVelocity(wantedEntMovement, (float)m_curFrameTime, buffer, Col_GreenYellow);
	}
#endif

	return wantedEntMovement;
}

//--------------------------------------------------------------------------------

// PROTOTYPE: Limit entity velocity to animation velocity projected along entity velocity direction.
// Problems due to:
// - Animations can play at a faster speed than how they look good. Using this method to limit prone (for example)
//   results in too-fast animations being "normal"
// - Making entity movement dependant on animation results in a very variable entity speed
// - Whenever the clamping happens the prediction becomes wrong - the AC tends to end up ahead of the entity (when
//   the clamping is done after the animation is determined by the prediction etc.
// - If the entity movement is clamped before the animation properties are set then starting becomes impossible - AC
//   tells entity it can't move, so entity doesn't move, and that makes the AC not move etc
//
QuatT CAnimatedCharacter::DoEntityClamping(const QuatT& wantedEntMovement) const
{
	IEntity* pEntity = GetEntity();
	CRY_ASSERT(pEntity != NULL);

	const float frameTime = (float)m_curFrameTime;

	ICharacterInstance* pCharacterInstance = pEntity->GetCharacter(0);
	ISkeletonAnim* pSkeletonAnim = (pCharacterInstance != NULL) ? pCharacterInstance->GetISkeletonAnim() : NULL;

	QuatT clampedWantedEntMovement(wantedEntMovement);

	float requestedEntVelocity = iszero(frameTime) ? 0.0f : wantedEntMovement.t.GetLength() / frameTime;
	if ((requestedEntVelocity > 0.0f) && (pSkeletonAnim != NULL))
	{
		const QuatT& qt = pSkeletonAnim->GetRelMovement();
		Vec3 t1 = qt.t * qt.q;
		Vec3 assetAnimMovement = m_entLocation.q * t1;

		Vec3 requestedEntMovementDir = m_requestedEntityMovement.t.GetNormalizedSafe();
		float projectedActualAnimVelocity = ((float)m_prevFrameTime > 0.0f) ? ((assetAnimMovement | requestedEntMovementDir) / (float)m_prevFrameTime) : 0.0f;

		clampedWantedEntMovement.t *= clamp_tpl(projectedActualAnimVelocity / requestedEntVelocity, 0.0f, 1.0f);
	}

	return clampedWantedEntMovement;
}

void CAnimatedCharacter::RequestPhysicalEntityMovement(const QuatT& wantedEntMovement)
{
	ANIMCHAR_PROFILE_DETAILED;

	IEntity* pEntity = GetEntity();
	CRY_ASSERT(pEntity != NULL);

	m_expectedEntMovement.zero();

	IPhysicalEntity* pPhysEnt = pEntity->GetPhysics();
	if (pPhysEnt == NULL)
		return;

	if (pPhysEnt->GetType() == PE_ARTICULATED)
		return;

	// local/world movement into force/velocity or whatever.
	// TODO: look at the tracker apply functions, see what they do different.

	const float frameTime = (float)m_curFrameTime;

	CRY_ASSERT(wantedEntMovement.IsValid());

	// This will update/activate the collider if needed, which makes it safe to move again.
	// (According to Craig we don't need to wake up other things that the requester might have put to sleep).
	if (!wantedEntMovement.t.IsZero())
		RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_ForceSleep, "ReqPhysEntMove");

	IEntity* pParent = pEntity->GetParent();
	if (pParent != NULL)
	{
		// When mounted and we have entity driven movement the only code to update the entity matrix is the vehicle (ref: Michael Rauh)
		// TODO: If needed, find a solution that supports split horizontal & vertical components. Could just MergeMCM cur & wanted locations?
		//CRY_ASSERT(GetMCMH() == GetMCMV());
		if (GetMCMH() != eMCM_Entity)
		{
			QuatT curEntLocationLocal(pEntity->GetLocalTM());
			curEntLocationLocal.q.Normalize();
			CRY_ASSERT(curEntLocationLocal.IsValid());
			Quat ParentOrientation = pParent->GetWorldRotation();
			CRY_ASSERT(ParentOrientation.IsValid());
			Quat ParentOrientationInv = ParentOrientation.GetInverted();
			CRY_ASSERT(ParentOrientationInv.IsValid());
			QuatT wantedEntLocationLocal;
			wantedEntLocationLocal.t = curEntLocationLocal.t + ParentOrientationInv * wantedEntMovement.t;
			wantedEntLocationLocal.q = curEntLocationLocal.q * wantedEntMovement.q;
			CRY_ASSERT(wantedEntLocationLocal.IsValid());
			Matrix34 wantedEntLocationLocalMat(wantedEntLocationLocal);
			CRY_ASSERT(wantedEntLocationLocalMat.IsValid());
			pEntity->SetLocalTM(wantedEntLocationLocalMat);

			pe_action_move move;
			move.dir = ZERO;
			move.iJump = 0;

			pPhysEnt->Action(&move);
		}
	}
	else
	{
		{
			ANIMCHAR_PROFILE_SCOPE("UpdatePhysicalEntityMovement_RequestToPhysics");

			// Force normal movement type during animation controlled movement,
			// since the entity needs to catch up regardless of what movement type game code requests.
			if ((m_requestedEntityMovementType == RequestedEntMovementType_Absolute) ||
			    ((GetMCMH() == eMCM_Animation || GetMCMH() == eMCM_AnimationHCollision) && (GetMCMV() == eMCM_Animation)))
			{
				const float curFrameTimeInv = (frameTime != 0.0f) ? (1.0f / frameTime) : 0.0f;
				pe_action_move move;
				move.dir = wantedEntMovement.t * curFrameTimeInv;
				move.iJump = m_requestedIJump;
				move.dt = static_cast<float>(frameTime);
				m_expectedEntMovement = wantedEntMovement.t;

				CRY_ASSERT(move.dir.IsValid());

				float curMoveVeloHash = 5.0f * move.dir.x + 7.0f * move.dir.y + 3.0f * move.dir.z;
				float actualMoveVeloHash = 5.0f * m_actualEntMovement.t.x * curFrameTimeInv + 7.0f * m_actualEntMovement.t.y * curFrameTimeInv + 3.0f * m_actualEntMovement.t.z * curFrameTimeInv;
				if ((curMoveVeloHash != m_prevMoveVeloHash) || (move.iJump != m_prevMoveJump) || (actualMoveVeloHash != curMoveVeloHash) || RecentQuickLoad())
				{
					ANIMCHAR_PROFILE_SCOPE("UpdatePhysicalEntityMovement_RequestToPhysics_Movement");

					if (!move.dir.IsZero() && !wantedEntMovement.t.IsZero())
						CRY_ASSERT(m_colliderModeLayers[eColliderModeLayer_ForceSleep] == eColliderMode_Undefined);

#if DEBUG_VELOCITY()
					if (DebugVelocitiesEnabled())
						AddDebugVelocity(wantedEntMovement, frameTime, "Physics Request", Col_Red);
#endif

					pPhysEnt->Action(&move);
					m_prevMoveVeloHash = curMoveVeloHash;
					m_prevMoveJump = move.iJump;

					pe_params_flags pf;
					if (m_bDisablePhysicalGravity ||
					    (m_colliderMode == eColliderMode_Disabled) ||
					    (m_colliderMode == eColliderMode_PushesPlayersOnly) ||
					    (m_colliderMode == eColliderMode_Spectator) ||
					    (GetMCMV() == eMCM_Animation))
						pf.flagsOR = pef_ignore_areas;
					else
						pf.flagsAND = ~pef_ignore_areas;
					pPhysEnt->SetParams(&pf);
				}
			}
			else if (m_requestedEntityMovementType == RequestedEntMovementType_Impulse)
			{
				ANIMCHAR_PROFILE_SCOPE("UpdatePhysicalEntityMovement_RequestToPhysics_Impulse");

				pe_action_impulse impulse;
				impulse.impulse = wantedEntMovement.t;
				impulse.iApplyTime = 0;
				m_expectedEntMovement = wantedEntMovement.t;

				pPhysEnt->Action(&impulse);
			}
			else
			{
				if (m_requestedEntityMovementType != RequestedEntMovementType_Undefined) // Can be underfined right after start, if there is no requested movement yet.
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "AnimatedCharacter::UpdatePhysicalEntityMovement() - Undefined movement type %d, expected absolute(1) or impulse(2).", m_requestedEntityMovementType);

				// When there is no valid requested movement, clear the requested velocity in living entity.
				pe_action_move move;
				move.dir = ZERO;
				move.iJump = 0;

				pPhysEnt->Action(&move);
			}
		}

		if (!NoMovementOverride())
		{
			Quat newRotation = m_entLocation.q * wantedEntMovement.q;

			if (m_hasForcedOverrideRotation)
			{
#if DEBUG_VELOCITY()
				if (DebugVelocitiesEnabled())
				{
					QuatT forcedRot(Vec3Constants<float>::fVec3_Zero, m_entLocation.q.GetInverted() * m_forcedOverrideRotationWorld);
					AddDebugVelocity(forcedRot, (float)m_curFrameTime, "ForcedOverrideRotation", Col_Orchid);
				}
#endif

				newRotation = m_forcedOverrideRotationWorld;
			}

			newRotation.Normalize();
			CRY_ASSERT(newRotation.IsValid());

			if (CAnimationGraphCVars::Get().m_useQueuedRotation)
			{
				m_pComponentPrepareCharForUpdate->QueueRotation(newRotation);
			}
			else
			{
				m_entLocation.q = newRotation;

				pEntity->SetRotation(newRotation, { ENTITY_XFORM_USER, ENTITY_XFORM_NOT_REREGISTER });
			}
		}

		// Send expected location to AI so pathfollowing which runs in parallel with physics becomes more accurate
		{
			IAIObject* pAIObject = GetEntity()->GetAI();
			if (pAIObject)
			{
				pAIObject->SetExpectedPhysicsPos(m_entLocation.t + m_expectedEntMovement);
			}
		}
	}

	m_hasForcedOverrideRotation = false;
	m_hasForcedMovement = false;
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::UpdatePhysicalColliderMode()
{
	ANIMCHAR_PROFILE_DETAILED;

	bool debug = (CAnimationGraphCVars::Get().m_debugColliderMode != 0);

	// If filtered result is Undefined it will be forced to Pushable below.
	EColliderMode newColliderMode = eColliderMode_Undefined;

	//#ifdef _DEBUG
	if (debug && DebugFilter())
	{
		if (m_isPlayer)
			m_colliderModeLayers[eColliderModeLayer_Debug] = (EColliderMode)CAnimationGraphCVars::Get().m_forceColliderModePlayer;
		else
			m_colliderModeLayers[eColliderModeLayer_Debug] = (EColliderMode)CAnimationGraphCVars::Get().m_forceColliderModeAI;
	}
	//#endif

	bool disabled = false;
	for (int layer = 0; layer < eColliderModeLayer_COUNT; layer++)
	{
		if (m_colliderModeLayers[layer] != eColliderMode_Undefined)
		{
			newColliderMode = m_colliderModeLayers[layer];
			if (m_colliderModeLayers[layer] == eColliderMode_Disabled)
				disabled = true;
		}
	}

	if (disabled && (m_colliderModeLayers[eColliderModeLayer_Debug] == eColliderMode_Undefined))
		newColliderMode = eColliderMode_Disabled;

	if (newColliderMode == eColliderMode_Undefined)
		newColliderMode = eColliderMode_Pushable;

	//#ifdef _DEBUG
	if (debug && DebugFilter())
	{
		ColorF color(1, 0.8f, 0.6f, 1);
		static float x = 50.0f;
		static float y = 70.0f;
		static float h = 20.0f;

		string name = GetEntity()->GetName();
		IRenderAuxText::Draw2dLabel(x, y - h, 1.7f, (float*)&color, false, "ColliderMode (%s)", name.c_str());

		int layer;
		const char* tag;
		for (layer = 0; layer < eColliderModeLayer_COUNT; layer++)
		{
			tag = (m_colliderModeLayersTag[layer] == NULL) ? "" : m_colliderModeLayersTag[layer];
			IRenderAuxText::Draw2dLabel(x, y + (float)layer * h, 1.7f, (float*)&color, false, "  %s(%d): %s(%d)   %s", g_szColliderModeLayerString[layer], layer, g_szColliderModeString[m_colliderModeLayers[layer]], m_colliderModeLayers[layer], tag);
		}
		IRenderAuxText::Draw2dLabel(x, y + (float)layer * h, 1.7f, (float*)&color, false, "  FINAL: %s(%d)", g_szColliderModeString[newColliderMode], newColliderMode);
	}
	//#endif

	if (m_pRigidColliderPE)
	{
		pe_params_pos pos;
		pos.pos = GetEntity()->GetPos();

		m_pRigidColliderPE->SetParams(&pos);
	}

	if ((newColliderMode == m_colliderMode) && !RecentQuickLoad() && !m_forcedRefreshColliderMode)
		return;

	if (!m_forcedRefreshColliderMode &&
	    (newColliderMode != eColliderMode_Disabled) &&
	    (newColliderMode != eColliderMode_PushesPlayersOnly) &&
	    (newColliderMode != eColliderMode_Spectator) &&
	    ((m_curFrameID <= 0) || ((m_lastResetFrameId + 1) > m_curFrameID)))
	{
		return;
	}

	m_forcedRefreshColliderMode = false;
	EColliderMode prevColliderMode = m_colliderMode;
	m_colliderMode = newColliderMode;

	CRY_ASSERT(m_colliderMode != eColliderMode_Undefined);

	IEntity* pEntity = GetEntity();
	CRY_ASSERT(pEntity);

	IPhysicalEntity* pPhysEnt = pEntity->GetPhysics();
	if (pPhysEnt == NULL || pPhysEnt->GetType() != PE_LIVING)
		return;

	pe_player_dynamics pd;
	pe_params_part pp;
	pe_params_flags pf;

	const unsigned int characterGeomCollTypeFlags = m_characterCollisionFlags;

	if (m_colliderMode == eColliderMode_Disabled)
	{
		pd.bActive = 0;
		pd.collTypes = ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid | ent_living;
		pp.flagsAND = ~characterGeomCollTypeFlags;

		pe_action_move move;
		move.dir = ZERO;
		move.iJump = 0;
		pPhysEnt->Action(&move);
		pf.flagsAND = ~(pef_ignore_areas | lef_loosen_stuck_checks);
	}
	else if (m_colliderMode == eColliderMode_GroundedOnly)
	{
		pd.bActive = 1;
		pd.collTypes = ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid | ent_living;
		pp.flagsAND = ~characterGeomCollTypeFlags;
		pp.flagsColliderAND = ~characterGeomCollTypeFlags;
		pf.flagsAND = ~(pef_ignore_areas | lef_loosen_stuck_checks);
	}
	else if (m_colliderMode == eColliderMode_Pushable)
	{
		pd.bActive = 1;
		pd.collTypes = ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid | ent_living;

		pp.flagsOR = characterGeomCollTypeFlags;
		pp.flagsColliderOR = characterGeomCollTypeFlags;

		// Enable the loosen stuck checks (essentially disabling an addition sanity
		// check in livingentity::step) for all animated characters except for the
		// local player instance (remote clients' livingentities get synchronized).
		bool bIsClient = GetEntityId() == CCryAction::GetCryAction()->GetClientActorId();
		pf.flagsOR = pef_pushable_by_players | (lef_loosen_stuck_checks & - (int)!bIsClient);
		pf.flagsAND = ~(pef_ignore_areas | (lef_loosen_stuck_checks & - (int)bIsClient));
	}
	else if (m_colliderMode == eColliderMode_NonPushable)
	{
		pd.bActive = 1;
		pd.collTypes = ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid | ent_living;
		pp.flagsOR = characterGeomCollTypeFlags;
		pp.flagsColliderOR = characterGeomCollTypeFlags;
		pf.flagsOR = lef_loosen_stuck_checks;
		pf.flagsAND = ~pef_pushable_by_players & ~pef_ignore_areas;
	}
	else if (m_colliderMode == eColliderMode_PushesPlayersOnly)
	{
		pd.bActive = 1;
		pd.collTypes = ent_living;
		pd.gravity.zero();
		pp.flagsOR = characterGeomCollTypeFlags;
		pp.flagsColliderOR = characterGeomCollTypeFlags;
		pf.flagsOR = pef_ignore_areas | lef_loosen_stuck_checks;
		pf.flagsAND = ~pef_pushable_by_players;
	}
	else if (m_colliderMode == eColliderMode_Spectator)
	{
		pd.bActive = 1;
		pd.collTypes = ent_terrain;

		if (CAnimationGraphCVars::Get().m_spectatorCollisions)
		{
			pd.collTypes |= ent_static;
		}

		pp.flagsAND = ~characterGeomCollTypeFlags;
		pp.flagsColliderOR = characterGeomCollTypeFlags;

		pe_action_move move;
		move.dir = ZERO;
		move.iJump = 0;
		pPhysEnt->Action(&move);
		pd.gravity.zero();
		pf.flagsOR = pef_ignore_areas;
		pf.flagsAND = ~lef_loosen_stuck_checks;
	}
	else
	{
		CRY_ASSERT(!"ColliderMode not implemented!");
		return;
	}

	if ((m_colliderMode == eColliderMode_NonPushable) && (m_pFeetColliderPE == NULL))
		CreateExtraSolidCollider();

	if ((m_colliderMode != eColliderMode_NonPushable) && (m_pFeetColliderPE != NULL))
		DestroyExtraSolidCollider();

	pp.ipart = 0;
	pPhysEnt->SetParams(&pd);
	pPhysEnt->SetParams(&pp);
	pPhysEnt->SetParams(&pf);
}

void CAnimatedCharacter::EnableRigidCollider(float radius)
{
	IPhysicalEntity* pPhysEnt = GetEntity()->GetPhysics();
	if (pPhysEnt && !m_pRigidColliderPE)
	{
		pe_params_pos pp;
		pp.pos = GetEntity()->GetWorldPos();
		pp.iSimClass = 2;
		m_pRigidColliderPE = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_RIGID, &pp, GetEntity(), PHYS_FOREIGN_ID_ENTITY);

		primitives::capsule prim;
		prim.axis.Set(0, 0, 1);
		prim.center.Set(0.0f, 0.0f, 0.5f);
		prim.r = radius;
		prim.hh = 0.6f;
		IGeometry* pPrimGeom = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::capsule::type, &prim);
		phys_geometry* pGeom = gEnv->pPhysicalWorld->GetGeomManager()->RegisterGeometry(pPrimGeom, 0);
		pPrimGeom->Release();
		pGeom->nRefCount = 0;

		pe_geomparams gp;
		gp.pos.zero();
		gp.flags = m_characterCollisionFlags;
		gp.flagsCollider = m_characterCollisionFlags;
		gp.mass = 0.0f;
		gp.density = 0.0f;
		m_pRigidColliderPE->AddGeometry(pGeom, &gp, 103);
	}
}

void CAnimatedCharacter::DisableRigidCollider()
{
	if (m_pRigidColliderPE)
	{
		gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_pRigidColliderPE);
		m_pRigidColliderPE = NULL;
	}
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::CreateExtraSolidCollider()
{
	// TODO: The solid collider created below is not supported/ignored by AI walkability tests.
	// It needs to be disabled until AI probes are adjusted.
	if (CAnimationGraphCVars::Get().m_enableExtraSolidCollider == 0)
		return;

	if (m_pFeetColliderPE != NULL)
		DestroyExtraSolidCollider();

	IPhysicalEntity* pPhysEnt = GetEntity()->GetPhysics();
	if (pPhysEnt == NULL)
		return;

	pe_params_pos pp;
	pp.pos = GetEntity()->GetWorldPos();
	pp.iSimClass = 2;
	m_pFeetColliderPE = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_ARTICULATED, &pp, GetEntity(), PHYS_FOREIGN_ID_ENTITY);

	primitives::capsule prim;
	prim.axis.Set(0, 0, 1);
	prim.center.zero();
	prim.r = 0.45f;
	prim.hh = 0.6f;
	IGeometry* pPrimGeom = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::capsule::type, &prim);
	phys_geometry* pGeom = gEnv->pPhysicalWorld->GetGeomManager()->RegisterGeometry(pPrimGeom, 0);
	pGeom->nRefCount = 0;

	pe_geomparams gp;
	gp.pos.zero();
	gp.flags = geom_colltype_solid & ~m_characterCollisionFlags;
	gp.flagsCollider = 0;
	gp.mass = 0.0f;
	gp.density = 0.0f;
	//gp.minContactDist = 0.0f;
	m_pFeetColliderPE->AddGeometry(pGeom, &gp, 102); // some arbitrary id (100 main collider, 101 veggie collider)

	pe_params_articulated_body pab;
	pab.pHost = pPhysEnt;
	pab.posHostPivot = Vec3(0, 0, 0.8f);
	pab.bGrounded = 1;
	m_pFeetColliderPE->SetParams(&pab);
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::DestroyExtraSolidCollider()
{
	if (m_pFeetColliderPE == NULL)
		return;

	gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_pFeetColliderPE);
	m_pFeetColliderPE = NULL;
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::UpdatePhysicsInertia()
{
	ANIMCHAR_PROFILE_DETAILED;

	IPhysicalEntity* pPhysEnt = GetEntity()->GetPhysics();
	if (pPhysEnt && (pPhysEnt->GetType() == PE_LIVING) &&
	    !GetEntity()->IsHidden()) // Hidden entities will ignore inertia sets, we don't want to invalidate our cache because of that
	{
		pe_player_dynamics dynNew;

		int mcmh = GetMCMH();
		int mcmv = GetMCMV();
		if ((mcmv == eMCM_Animation) || (mcmh == eMCM_Animation) || (mcmh == eMCM_AnimationHCollision) || (m_colliderMode == eColliderMode_PushesPlayersOnly))
		{
			dynNew.kInertia = 0.0f;
			dynNew.kInertiaAccel = 0.0f;
			if (m_colliderMode != eColliderMode_PushesPlayersOnly)
				dynNew.timeImpulseRecover = 0.0f;
			else
				dynNew.timeImpulseRecover = m_params.timeImpulseRecover;
		}
		else
		{
			dynNew.kInertia = m_params.inertia;
			dynNew.kInertiaAccel = m_params.inertiaAccel;
			dynNew.timeImpulseRecover = m_params.timeImpulseRecover;
		}

		// Only change inertia values when they have changed since the last set
		if ((m_fPrevInertia != dynNew.kInertia) ||
		    (m_fPrevInertiaAccel != dynNew.kInertiaAccel) ||
		    (m_fPrevTimeImpulseRecover != dynNew.timeImpulseRecover))
		{
			pPhysEnt->SetParams(&dynNew, 1);

			m_fPrevInertia = dynNew.kInertia;
			m_fPrevInertiaAccel = dynNew.kInertiaAccel;
			m_fPrevTimeImpulseRecover = dynNew.timeImpulseRecover;
		}
	}
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::GetCurrentEntityLocation()
{
	if (CAnimationGraphCVars::Get().m_useQueuedRotation)
	{
		m_prevEntLocation = m_entLocation;
	}
	else
	{
		// m_entLocation.q was already updated to its new value when we synchronously set
		// the rotation in RequestPhysicalEntityMovement, so we should be careful not to
		// copy it over m_prevEntLocation here until we calculated m_actualEntMovement
		// The q component of m_prevEntLocation will be filled at the end of this function
		m_prevEntLocation.t = m_entLocation.t;
	}

	IEntity* pEntity = GetEntity();
	CRY_ASSERT(pEntity != NULL);

	m_entLocation.q = pEntity->GetWorldRotation();
	m_entLocation.t = pEntity->GetWorldPos();

	CRY_ASSERT(m_entLocation.IsValid());

	m_actualEntMovement = GetWorldOffset(m_prevEntLocation, m_entLocation);

	// actualEntMovement measures from the previous frame, so we need to use prevFrameTime.
	const float prevFrameTimeInv = (float)__fsel(-(float)m_prevFrameTime, 0.0f, __fres((float)m_prevFrameTime + FLT_MIN));  // approximates: (m_prevFrameTime > 0.0f) ? (1.0f / (float)m_prevFrameTime) : 0.0f;

	const Vec3 velocity = m_actualEntMovement.t * prevFrameTimeInv;
	const float speed = velocity.GetLength();

	const float prevSpeed = m_actualEntSpeed;
	m_actualEntSpeed = speed;

	const float speed2D = Vec2(velocity).GetLength();

	const float prevSpeed2D = m_actualEntSpeedHorizontal;
	m_actualEntSpeedHorizontal = speed2D;

	m_actualEntMovementDirHorizontal.x = (float)__fsel(-(speed2D - FLT_MIN), 0.0f, velocity.x / (speed2D + FLT_MIN));// approximates: (speed2D > FLT_MIN) ? (velocity2D / speed2D) : Vec2Constants<float>::fVec2_Zero;
	m_actualEntMovementDirHorizontal.y = (float)__fsel(-(speed2D - FLT_MIN), 0.0f, velocity.y / (speed2D + FLT_MIN));

	const Vec2 prevVelocity2D = m_actualEntVelocityHorizontal;
	m_actualEntVelocityHorizontal = Vec2(velocity);
	m_actualEntAccelerationHorizontal = (m_actualEntVelocityHorizontal - prevVelocity2D) * prevFrameTimeInv;

	m_actualEntTangentialAcceleration = (speed - prevSpeed) * prevFrameTimeInv;

#ifdef _DEBUG
	//DebugHistory_AddValue("eDH_TEMP02", (m_prevFrameTime != 0.0f) ? m_actualEntMovement.t.GetLength2D() / m_prevFrameTime : 0.0f);
#endif

#ifdef _DEBUG
	DebugGraphQT(m_entTeleportMovement, "eDH_EntTeleportMovementTransX", "eDH_EntTeleportMovementTransY", "eDH_EntTeleportMovementRotZ");
#endif

	m_entTeleportMovement.SetIdentity();

	if (!CAnimationGraphCVars::Get().m_useQueuedRotation)
	{
		m_prevEntLocation.q = m_entLocation.q;
	}

#if DEBUG_VELOCITY()
	if (DebugVelocitiesEnabled())
		AddDebugVelocity(m_actualEntMovement, (float)m_curFrameTime, "Entity Movement", Col_Green, true);
#endif
}

//--------------------------------------------------------------------------------

bool CAnimatedCharacter::EnableProceduralLeaning()
{
	return (m_moveRequest.proceduralLeaning > 0.0f);
}

//--------------------------------------------------------------------------------

float CAnimatedCharacter::GetProceduralLeaningScale()
{
	return m_moveRequest.proceduralLeaning;
}

//--------------------------------------------------------------------------------

QuatT CAnimatedCharacter::CalculateProceduralLeaning()
{
	ANIMCHAR_PROFILE_DETAILED;

	float frameTimeScale = ((float)m_curFrameTime > 0.0f) ? (1.0f / ((float)m_curFrameTime / 0.02f)) : 0.0f;
	float curving = 0.0f;
	Vec2 prevVelo(ZERO);
	Vec3 avgAxx(0);
	float weightSum = 0.0f;
	CTimeValue curTime = gEnv->pTimer->GetFrameStartTime();
	for (int i = 0; i < NumAxxSamples; i++)
	{
		int j = (m_reqLocalEntAxxNextIndex + i) % NumAxxSamples;

		// AGE WEIGHT
		float age = CTimeValue(curTime.GetValue() - m_reqEntTime[j].GetValue()).GetSeconds();
		float ageFraction = clamp_tpl(age / 0.3f, 0.0f, 1.0f);
		float weight = (0.5f - fabs(0.5f - ageFraction)) * 2.0f;
		weight = 1.0f - sqr(1.0f - weight);
		weight = sqr(weight);

		weightSum += weight;
		avgAxx.x += m_reqLocalEntAxx[j].x * weight * frameTimeScale;
		avgAxx.y += m_reqLocalEntAxx[j].y * weight * frameTimeScale;

		// CURVING
		float area = fabs(m_reqEntVelo[j].Cross(prevVelo));
		float len = prevVelo.GetLength() * m_reqEntVelo[j].GetLength();
		if (len > 0.0f)
			area /= len;
		curving += area * weight;
		prevVelo = m_reqEntVelo[j];
	}

	CRY_ASSERT(avgAxx.IsValid());
	if (weightSum > 0.0f)
	{
		avgAxx /= weightSum;
		curving /= weightSum;
	}
	CRY_ASSERT(avgAxx.IsValid());

	/*
	   Vec3 curAxx;
	   curAxx.x = m_reqLocalEntAxx[(m_reqLocalEntAxxNextIndex-1)%NumAxxSamples].x * frameTimeScale;
	   curAxx.y = m_reqLocalEntAxx[(m_reqLocalEntAxxNextIndex-1)%NumAxxSamples].y * frameTimeScale;
	 */

	float curvingFraction = clamp_tpl(curving / 0.3f, 0.0f, 1.0f);
	curvingFraction = 1.0f - sqr(1.0f - curvingFraction);
	float pulldownFraction = sqr(1.0f - clamp_tpl(curvingFraction / 0.3f, 0.0f, 1.0f));

	Vec3 prevActualEntVeloW = ((float)m_curFrameTime > 0.0f) ? (m_requestedEntityMovement.t / (float)m_curFrameTime) : ZERO;
	//Vec3 prevActualEntVeloW = (m_curFrameTime > 0.0f) ? (m_actualEntMovement.t / m_curFrameTime) : ZERO;
	prevActualEntVeloW.z = 0.0f; // Only bother about horizontal accelerations.
	float smoothVeloBlend = clamp_tpl((float)m_curFrameTime / 0.1f, 0.0f, 1.0f);
	Vec3 prevSmoothedActualEntVelo = m_smoothedActualEntVelo;
	CRY_ASSERT(prevSmoothedActualEntVelo.IsValid());
	m_smoothedActualEntVelo = LERP(m_smoothedActualEntVelo, prevActualEntVeloW, smoothVeloBlend);
	Vec3 smoothedActualEntVeloL = m_entLocation.q.GetInverted() * m_smoothedActualEntVelo;
	CRY_ASSERT(smoothedActualEntVeloL.IsValid());

	Vec3 deltaVelo = (m_smoothedActualEntVelo - prevSmoothedActualEntVelo);
	deltaVelo = m_entLocation.q.GetInverted() * deltaVelo;
	m_reqLocalEntAxx[m_reqLocalEntAxxNextIndex].x = deltaVelo.x;
	m_reqLocalEntAxx[m_reqLocalEntAxxNextIndex].y = deltaVelo.y;
	m_reqEntVelo[m_reqLocalEntAxxNextIndex].x = m_smoothedActualEntVelo.x;
	m_reqEntVelo[m_reqLocalEntAxxNextIndex].y = m_smoothedActualEntVelo.y;
	m_reqEntTime[m_reqLocalEntAxxNextIndex] = curTime;//gEnv->pTimer->GetFrameStartTime();
	m_reqLocalEntAxxNextIndex = (m_reqLocalEntAxxNextIndex + 1) % NumAxxSamples;

	// EQUALIZE VELOCITIES
	float velo = m_smoothedActualEntVelo.GetLength();
	float equalizeScale = 1.0f;
	if ((velo >= 0.0f) && (velo < 1.5f))
	{
		float fraction = (velo - 0.0f) / 1.5f;
		equalizeScale *= LERP(1.0f, 2.5f, fraction);
	}
	else if ((velo >= 1.5f) && (velo < 3.0f))
	{
		float fraction = (velo - 1.5f) / 1.5f;
		equalizeScale *= LERP(2.5f, 1.0f, fraction);
	}
	else if ((velo >= 3.0f) && (velo < 6.0f))
	{
		float fraction = (velo - 3.0f) / 3.0f;
		equalizeScale *= LERP(1.0f, 0.7f, fraction);
	}
	else if ((velo >= 6.0f) && (velo < 10.0f))
	{
		float fraction = (velo - 6.0f) / 4.0f;
		equalizeScale *= LERP(0.7f, 0.3f, fraction);
	}
	else if ((velo >= 10.0f) && (velo < 50.0f))
	{

		float fraction = (min(velo, 20.0f) - 10.0f) / 10.0f;
		equalizeScale *= LERP(0.3f, 0.1f, fraction);
	}

	float scale = 1.0f;
	scale *= (1.0f + curvingFraction);
	scale *= equalizeScale;
	CRY_ASSERT(avgAxx.IsValid());
	Vec3 avgAxxScaled = avgAxx * scale;
	CRY_ASSERT(avgAxx.IsValid());

	if (avgAxxScaled.IsEquivalent(ZERO))
		avgAxxScaled.zero();

	// CLAMP AMOUNT
	float alignmentOriginal = (smoothedActualEntVeloL | m_avgLocalEntAxx);
	float amount = avgAxxScaled.GetLength();
	float amountMaxNonCurving = (0.5f + 0.5f * clamp_tpl(alignmentOriginal * 2.0f, 0.0f, 1.0f));
	float amountMax = 0.35f;
	amountMax *= LERP(amountMaxNonCurving, 1.0f, curvingFraction);
	if (amount > 0.00001f) // (AdamR) Changed comparison from 0.0f to handle occasional X360 denormals
		avgAxxScaled = (avgAxxScaled / amount) * min(amount, amountMax);

	CRY_ASSERT(avgAxxScaled.IsValid());

	float axxContinuityMag = avgAxxScaled.GetLength() * m_avgLocalEntAxx.GetLength();
	float axxContinuityDot = (avgAxxScaled | m_avgLocalEntAxx);
	float axxContinuity = (axxContinuityMag > 0.0f) ? (axxContinuityDot / axxContinuityMag) : 0.0f;
	axxContinuity = clamp_tpl(axxContinuity, 0.0f, 1.0f);
	float axxBlend = ((float)m_curFrameTime / 0.2f);
	axxBlend *= 0.5f + 0.5f * axxContinuity;
	float axxBlendClamped = clamp_tpl(axxBlend, 0.0f, 1.0f);
	m_avgLocalEntAxx = LERP(m_avgLocalEntAxx, avgAxxScaled, axxBlendClamped);
	CRY_ASSERT(m_avgLocalEntAxx.IsValid());

	amount = m_avgLocalEntAxx.GetLength();

	Vec3 lean = Vec3(0, 0, 1) + m_avgLocalEntAxx;
	lean.NormalizeSafe(Vec3(0, 0, 1));
	float pulldown = max(LERP((-0.0f), (-0.25f), pulldownFraction), -amount * 0.6f);
	//pulldown = 0.0f;

	QuatT leaning;
	leaning.q.SetRotationV0V1(Vec3(0, 0, 1), lean);
	leaning.t.zero();
	leaning.t = -m_avgLocalEntAxx;
	leaning.t *= (0.8f + 0.4f * curvingFraction);
	leaning.t.z = pulldown;

	leaning.t *= 0.1f;
	if (!m_isPlayer)
		leaning.t *= 0.5f;

	float proceduralLeaningScale = GetProceduralLeaningScale();
	clamp_tpl(proceduralLeaningScale, 0.0f, 1.0f);
	leaning = leaning.GetScaled(proceduralLeaningScale);

#if _DEBUG && defined(USER_david)
	if (DebugFilter())
	{
		CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();

		if (pPD != NULL)
		{
			/*
			      DebugHistory_AddValue("eDH_TEMP00", m_actualEntMovement.t.x / m_curFrameTime);
			      DebugHistory_AddValue("eDH_TEMP01", m_actualEntMovement.t.y / m_curFrameTime);
			      DebugHistory_AddValue("eDH_TEMP02", m_smoothedActualEntVelo.x);
			      DebugHistory_AddValue("eDH_TEMP03", m_smoothedActualEntVelo.y);
			      DebugHistory_AddValue("eDH_TEMP04", curAxx.x);
			      DebugHistory_AddValue("eDH_TEMP05", curAxx.y);
			      DebugHistory_AddValue("eDH_TEMP06", avgAxx.x);
			      DebugHistory_AddValue("eDH_TEMP07", avgAxx.y);

			      DebugHistory_AddValue("eDH_TEMP10", alignmentOriginal);
			      DebugHistory_AddValue("eDH_TEMP11", amountMaxNonCurving);
			      DebugHistory_AddValue("eDH_TEMP12", equalizeScale);
			      DebugHistory_AddValue("eDH_TEMP13", curvingFraction);
			      DebugHistory_AddValue("eDH_TEMP14", scale);

			      DebugHistory_AddValue("eDH_TEMP15", amount);
			      DebugHistory_AddValue("eDH_TEMP16", m_avgLocalEntAxx.x);
			      DebugHistory_AddValue("eDH_TEMP17", m_avgLocalEntAxx.y);
			   /**/
			//*
			DebugHistory_AddValue("eDH_TEMP00", m_smoothedActualEntVelo.x);
			DebugHistory_AddValue("eDH_TEMP01", m_smoothedActualEntVelo.y);
			DebugHistory_AddValue("eDH_TEMP02", m_avgLocalEntAxx.x);
			DebugHistory_AddValue("eDH_TEMP03", m_avgLocalEntAxx.y);

			DebugHistory_AddValue("eDH_TEMP10", curving);
			DebugHistory_AddValue("eDH_TEMP11", curvingFraction);
			DebugHistory_AddValue("eDH_TEMP12", pulldownFraction);
			DebugHistory_AddValue("eDH_TEMP13", pulldown);

			DebugHistory_AddValue("eDH_TEMP06", alignmentOriginal);
			/**/

			/*
			      Vec3 bump(0,0,0.1f);
			      pPD->Begin(UNIQUE("AnimatedCharacter.FakeLeaning"), true);
			      pPD->AddLine(m_animLocation.t+bump, m_animLocation.t+bump + m_entLocation.q*lean, ColorF(1,1,1,1), 0.5f);
			      pPD->AddLine(m_animLocation.t+bump+Vec3(0,0,1), m_animLocation.t+bump+Vec3(0,0,1) + m_entLocation.q*m_avgLocalEntAxx, ColorF(0.5f,1,0.5f,1), 0.5f);
			      pPD->Begin(UNIQUE("AnimatedCharacter.FakeLeaningAxx"), false);
			      pPD->AddLine(m_animLocation.t+bump, m_animLocation.t+bump + m_entLocation.q*m_avgLocalEntAxx, ColorF(1.0f,1.0f,1.0f,1), 5.0f);
			 */
		}
	}
#endif

	return leaning;
}

//--------------------------------------------------------------------------------

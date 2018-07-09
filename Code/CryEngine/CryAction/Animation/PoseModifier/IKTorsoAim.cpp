// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IRenderAuxGeom.h>

#include "IKTorsoAim.h"

CRYREGISTER_CLASS(CIKTorsoAim)

CIKTorsoAim::STorsoAim_CVars CIKTorsoAim::s_CVars;

#define DO_DEBUG_RENDER 0

#if DO_DEBUG_RENDER

static Vec3 g_effPos;
static Vec3 g_effFwds;
static Vec3 g_aimJointPos;
static Vec3 g_localTgtDir;
static QuatT g_newAimJointPos;

#endif //DO_DEBUG_RENDER

//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CIKTorsoAim::STorsoAim_CVars::InitCvars()
{
	if (m_initialized)
		return;

	m_initialized = true;

	REGISTER_CVAR(STAP_DEBUG, 0, VF_NULL, "Enable STAP Debug Rendering");
	REGISTER_CVAR(STAP_DISABLE, 0, VF_NULL, "Disable torso orientation");
	REGISTER_CVAR(STAP_TRANSLATION_FUDGE, 1, VF_NULL, "Enable STAP Translation Fudge");
	REGISTER_CVAR(STAP_TRANSLATION_FEATHER, 1, VF_NULL, "Enable STAP Translation Feathering");
	REGISTER_CVAR(STAP_LOCK_EFFECTOR, 0, VF_NULL, "Lock the STAP Effector Joint");
	REGISTER_CVAR(STAP_OVERRIDE_TRACK_FACTOR, -1.0f, VF_NULL, "Override the base anim tracking factor");
}

void CIKTorsoAim::STorsoAim_CVars::ReleaseCVars()
{
	/*
	   if (gEnv->pConsole)
	   {
	   gEnv->pConsole->UnregisterVariable("STAP_DEBUG", true);
	   gEnv->pConsole->UnregisterVariable("STAP_DISABLE", true);
	   gEnv->pConsole->UnregisterVariable("STAP_TRANSLATION_FUDGE", true);
	   gEnv->pConsole->UnregisterVariable("STAP_TRANSLATION_FEATHER", true);
	   gEnv->pConsole->UnregisterVariable("STAP_LOCK_EFFECTOR", true);
	   gEnv->pConsole->UnregisterVariable("STAP_OVERRIDE_TRACK_FACTOR", true);
	   }
	 */
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CIKTorsoAim::SParams::SParams()
{
	blend = 0.0f;
	blendPosition = 0.0f;
	targetDirection.zero();
	viewOffset.SetIdentity();
	absoluteTargetPosition.zero();
	baseAnimTrackFactor = 1.0f;
	numParents = 0;
	effectorJoint = -1;
	aimJoint = -1;
	weights[0] = 1.0f;
}

CIKTorsoAim::CIKTorsoAim()
{
	Init();
};
//

void CIKTorsoAim::Init()
{
	m_aimToEffector = Vec3(ZERO);
	m_effectorDirQuat.SetIdentity();
	m_lastProcessedEffector.SetIdentity();
}

void CIKTorsoAim::InitCVars()
{
	s_CVars.InitCvars();
}

void CIKTorsoAim::SetBlendWeight(float blend)
{
	m_setParams.blend = blend;
}

void CIKTorsoAim::SetBlendWeightPosition(float blendPos)
{
	m_setParams.blendPosition = blendPos;
}

void CIKTorsoAim::SetTargetDirection(const Vec3& direction)
{
	m_setParams.targetDirection = direction;
}

void CIKTorsoAim::SetViewOffset(const QuatT& offset)
{
	m_setParams.viewOffset = offset;
}

void CIKTorsoAim::SetAbsoluteTargetPosition(const Vec3& targetPosition)
{
	m_setParams.absoluteTargetPosition = targetPosition;
}

void CIKTorsoAim::SetFeatherWeights(uint32 weights, const f32* customBlends)
{
	assert(weights <= TORSOAIM_MAX_JOINTS);
	memcpy(m_setParams.weights, customBlends, sizeof(f32) * weights);
	m_setParams.numParents = weights - 1;
}

void CIKTorsoAim::SetJoints(uint32 jntEffector, uint32 jntAdjustment)
{
	m_setParams.aimJoint = jntAdjustment;
	m_setParams.effectorJoint = jntEffector;
}

void CIKTorsoAim::SetBaseAnimTrackFactor(float factor)
{
	m_setParams.baseAnimTrackFactor = factor;
}

// IAnimationPoseModifier

bool CIKTorsoAim::Prepare(const SAnimationPoseModifierParams& params)
{
	bool ShowDebug = s_CVars.STAP_DEBUG != 0;

	const float XPOS = 20.0f;
	const float YPOS = 35.0f;
	const float FONT_SIZE = 2.0f;
	const float FONT_COLOUR[4] = { 1, 1, 1, 1 };
	if (ShowDebug)
	{
		IRenderAuxText::Draw2dLabel(XPOS, YPOS, FONT_SIZE, FONT_COLOUR, false, "Torso Aim Pose: %f (%f, %f, %f)", m_params.blend, m_params.targetDirection.x, m_params.targetDirection.y, m_params.targetDirection.z);
		IRenderAuxText::Draw2dLabel(XPOS, YPOS + 15.0f, FONT_SIZE, FONT_COLOUR, false, "Absolute Position: (%f, %f, %f)", m_setParams.absoluteTargetPosition.x, m_setParams.absoluteTargetPosition.y, m_setParams.absoluteTargetPosition.z);
	}

#if DO_DEBUG_RENDER

	const float SPHERE_SIZE = 0.1f;
	IRenderAuxGeom* pAux = gEnv->pRenderer->GetIRenderAuxGeom();
	const ColorF red(1.0f, 0.0f, 0.0f, 1.0f);
	const ColorF green(0.0f, 1.0f, 0.0f, 1.0f);
	const ColorF yellow(1.0f, 1.0f, 0.0f, 1.0f);
	const ColorF blue(0.0f, 0.0f, 1.0f, 1.0f);

	//--- TODO
	if (ShowDebug)
	{
		//--- Debug draw
		Vec3 effTarget = g_effPos + (g_effFwds * 100.0f);
		Vec3 localTgtPos = g_effPos + (g_localTgtDir * 100.0f);

		SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
		pAux->SetRenderFlags(renderFlags);
		pAux->DrawSphere(params.location * effTarget, SPHERE_SIZE, red);
		pAux->DrawSphere(params.location * localTgtPos, SPHERE_SIZE, green);
		pAux->DrawLine(params.location * g_aimJointPos, yellow, params.location * effTarget, red);
		pAux->DrawLine(params.location * g_aimJointPos, yellow, params.location * localTgtPos, green);
		//--- Debug draw
	}
	//--- TODO
	if (ShowDebug)
	{
		const float CAMERA_EFFECTOR_LEN = 5.0f;
		//--- Debug draw
		QuatT wsAim = params.location * g_newAimJointPos;
		QuatT wsEffector = params.location * m_lastProcessedEffector;

		pAux->DrawLine(wsEffector.t, red, m_params.targetDirection, green);
		pAux->DrawSphere(wsEffector.t, SPHERE_SIZE, red);
		pAux->DrawSphere(wsAim.t, SPHERE_SIZE, green);

		pAux->DrawLine(wsEffector.t, red, wsEffector.t + (wsEffector.GetColumn1() * CAMERA_EFFECTOR_LEN), blue);

		//--- Draw effector AXIS
		//const float AXIS_LENGTH = 0.5f;
		//Vec3 DEBUG_init_right = pAbsolutePose[m_params.effectorJoint].GetColumn0();
		//Vec3 DEBUG_init_fwd = pAbsolutePose[m_params.effectorJoint].GetColumn1();
		//Vec3 DEBUG_init_up = pAbsolutePose[m_params.effectorJoint].GetColumn2();
		//pAux->DrawLine(params.locationNextPhysics * pAbsolutePose[m_params.effectorJoint].t, red, params.locationNextPhysics * (pAbsolutePose[m_params.effectorJoint].t + (DEBUG_init_right * AXIS_LENGTH)), red);
		//pAux->DrawLine(params.locationNextPhysics * pAbsolutePose[m_params.effectorJoint].t, green, params.locationNextPhysics * (pAbsolutePose[m_params.effectorJoint].t + (DEBUG_init_fwd * AXIS_LENGTH)), green);
		//pAux->DrawLine(params.locationNextPhysics * pAbsolutePose[m_params.effectorJoint].t, blue, params.locationNextPhysics * (pAbsolutePose[m_params.effectorJoint].t + (DEBUG_init_up * AXIS_LENGTH)), blue);
		//--- Debug draw
	}

#endif //DO_DEBUG_RENDER

	m_params = m_setParams;

	return true;
}

bool CIKTorsoAim::Execute(const SAnimationPoseModifierParams& params)
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	const IDefaultSkeleton& rIDefaultSkeleton = params.GetIDefaultSkeleton();
	if ((m_params.effectorJoint < 0) || (m_params.aimJoint < 0))
		return false;

	bool StapDisable = s_CVars.STAP_DISABLE != 0;
	bool StapLockEffector = s_CVars.STAP_LOCK_EFFECTOR != 0;
	float OverrideTrackFactor = s_CVars.STAP_OVERRIDE_TRACK_FACTOR;
	if (StapDisable)
	{
		return false;
	}

	uint32 numJoints = params.pPoseData->GetJointCount();
	;
	params.pPoseData->SetJointAbsolute(0, QuatT(params.pPoseData->GetJointRelative(0)));
	for (uint32 i = 1; i < numJoints; i++)
	{
		params.pPoseData->SetJointAbsolute(i,
		                                   params.pPoseData->GetJointAbsolute(rIDefaultSkeleton.GetJointParentIDByID(i)) *
		                                   params.pPoseData->GetJointRelative(i));
	}

	QuatT inverseInitialAim = params.pPoseData->GetJointAbsolute(m_params.aimJoint).GetInverted();

	//	CryLogAlways("CIKTorsoAim::Execute: Anim:(%f, %f, %f, %f)", params.locationNextAnimation.q.v.x, params.locationNextAnimation.q.v.y, params.locationNextAnimation.q.v.z, params.locationNextAnimation.q.w);
	//	CryLogAlways("CIKTorsoAim::Execute: Phys:(%f, %f, %f, %f)", params.locationNextPhysics.q.v.x, params.locationNextPhysics.q.v.y, params.locationNextPhysics.q.v.z, params.locationNextPhysics.q.w);

	const bool RemoveInitialRoll = true;

	Vec3 localTgtDir = m_params.targetDirection;

	Quat relHeadQuat;
	Vec3 effFwds = params.pPoseData->GetJointAbsolute(m_params.effectorJoint).GetColumn1();

	Vec3 initialEffectorMS = effFwds;
	Vec3 targetEffectorMS = localTgtDir;

	//--- Calculate initial roll, ready for removal
	Vec3 idealRight = params.pPoseData->GetJointAbsolute(m_params.effectorJoint).GetColumn1().Cross(Vec3Constants<float>::fVec3_OneZ).GetNormalizedSafe();
	float upDP = params.pPoseData->GetJointAbsolute(m_params.effectorJoint).GetColumn2().Dot(idealRight);
	float initialRoll = (f32)(g_PI * 0.5f) - acos_tpl(upDP);

	Quat effectorDirQuat = m_effectorDirQuat;
	Vec3 aimToEffector = m_aimToEffector;
	float baseFactor = (float) __fsel(OverrideTrackFactor, OverrideTrackFactor, m_params.baseAnimTrackFactor);
	if (!StapLockEffector)
	{
		if (baseFactor > 0.0f)
		{
			effectorDirQuat = RemoveInitialRoll ? Quat::CreateRotationVDir(initialEffectorMS, initialRoll) : Quat::CreateRotationVDir(initialEffectorMS);
			aimToEffector = inverseInitialAim * params.pPoseData->GetJointAbsolute(m_params.effectorJoint).t;

			if (baseFactor == 1.0f)
			{
				m_effectorDirQuat = effectorDirQuat;
				m_aimToEffector = aimToEffector;
			}
			else
			{
				effectorDirQuat = Quat::CreateSlerp(m_effectorDirQuat, effectorDirQuat, baseFactor);
				aimToEffector.SetLerp(m_aimToEffector, aimToEffector, baseFactor);
			}
		}
	}

	Quat rebase = !effectorDirQuat* params.pPoseData->GetJointAbsolute(m_params.aimJoint).q;
	Quat targetDirQuat = Quat::CreateRotationVDir(targetEffectorMS);
	Quat absHeadQuat = targetDirQuat * rebase;
	relHeadQuat = inverseInitialAim.q * absHeadQuat;
	relHeadQuat.NormalizeSafe();

	bool useAbsoluteTarget = (m_params.absoluteTargetPosition.IsZero() == false);

	const bool useAbsoluteCameraPos = true;

	Vec3 viewOffsetTran = (targetDirQuat * !effectorDirQuat) * m_params.viewOffset.t;

	Vec3 absoluteOffset(0.0f, 0.0f, 0.0f);
	if (useAbsoluteTarget)
	{
		absoluteOffset = m_params.absoluteTargetPosition;
	}

	Vec3 relativeOffset = absoluteOffset - params.pPoseData->GetJointAbsolute(m_params.aimJoint).t;

#if DO_DEBUG_RENDER
	g_effPos = params.pPoseData->GetJointAbsolute(m_params.effectorJoint).t;
	g_effFwds = effFwds;
	g_aimJointPos = params.pPoseData->GetJointAbsolute((m_params.aimJoint).t;
	                                                   g_localTgtDir = localTgtDir;
#endif //DO_DEBUG_RENDER

	int16 idx = m_params.aimJoint;
	int16 childidx = -1;
	for (int16 i = m_params.numParents; i >= 0; i--)
	{
		int16 pidx = rIDefaultSkeleton.GetJointParentIDByID(idx);
		float weight = m_params.weights[i] * m_params.blend;

		if (childidx < 0)
		{
			Quat adjustFactored = relHeadQuat * !params.pPoseData->GetJointAbsolute(idx).q * m_params.viewOffset.q * params.pPoseData->GetJointAbsolute(idx).q;
			Quat lerpSpine = Quat::CreateSlerp(Quat(IDENTITY), adjustFactored, weight);
			params.pPoseData->SetJointAbsoluteO(idx, params.pPoseData->GetJointAbsolute(idx).q * lerpSpine);

			if (useAbsoluteCameraPos)
			{
				Vec3 newEffectorT = params.pPoseData->GetJointAbsolute(m_params.aimJoint) * aimToEffector;
				relativeOffset = absoluteOffset - newEffectorT;
			}
		}
		else
		{
			Quat targetPose = params.pPoseData->GetJointAbsolute(childidx).q * !params.pPoseData->GetJointRelative(childidx).q;
			Quat Diff = !params.pPoseData->GetJointAbsolute(idx).q * targetPose;
			Quat lerpSpine = Quat::CreateSlerp(Quat(IDENTITY), Diff, weight);
			params.pPoseData->SetJointAbsoluteO(idx, params.pPoseData->GetJointAbsolute(idx).q * lerpSpine);
		}

		if (useAbsoluteTarget)
		{
			params.pPoseData->SetJointAbsoluteP(idx, params.pPoseData->GetJointAbsolute(idx).t + relativeOffset * (weight * m_params.blendPosition));
		}

		params.pPoseData->SetJointAbsoluteP(idx, params.pPoseData->GetJointAbsolute(idx).t + viewOffsetTran * weight);
		params.pPoseData->SetJointAbsoluteO(idx, params.pPoseData->GetJointAbsolute(idx).q.GetNormalized());

		childidx = idx;
		assert(params.pPoseData->GetJointAbsolute(idx).IsValid());
		assert(params.pPoseData->GetJointAbsolute(idx).q.IsUnit());

		idx = pidx;
	}

	idx = m_params.aimJoint;
	for (int16 i = m_params.numParents; i >= 0; i--)
	{
		int16 pidx = rIDefaultSkeleton.GetJointParentIDByID(idx);
		params.pPoseData->SetJointRelative(idx, params.pPoseData->GetJointAbsolute(pidx).GetInverted() * params.pPoseData->GetJointAbsolute(idx));

		assert(params.pPoseData->GetJointRelative(idx).IsValid());
		assert(params.pPoseData->GetJointRelative(idx).q.IsUnit());

		idx = pidx;
	}

	for (int i = 1; i < numJoints; i++)
	{
		int32 p = rIDefaultSkeleton.GetJointParentIDByID(i);
		params.pPoseData->SetJointAbsolute(i, params.pPoseData->GetJointAbsolute(p) * params.pPoseData->GetJointRelative(i));
		assert(params.pPoseData->GetJointAbsolute(idx).IsValid());
		assert(params.pPoseData->GetJointAbsolute(idx).q.IsUnit());
	}

#if DO_DEBUG_RENDER
	g_newAimJointPos = params.pPoseData->GetJointAbsolute(m_params.aimJoint);
#endif //DO_DEBUG_RENDER

	m_lastProcessedEffector = params.pPoseData->GetJointAbsolute(m_params.effectorJoint);

	                                                             return true;
}

void CIKTorsoAim::Synchronize()
{
}

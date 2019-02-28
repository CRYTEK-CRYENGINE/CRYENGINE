// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PoseAlignerChain.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include "../CharacterInstance.h"
#include "../Model.h"
#include "../CharacterManager.h"
#include "PoseModifierHelper.h"
#include "../DrawHelper.h"

ILINE const IKLimbType* FindIkLimbType(const SAnimationPoseModifierParams& params, LimbIKDefinitionHandle limbHandle)
{
	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);

	int limbIndex = defaultSkeleton.GetLimbDefinitionIdx(limbHandle);
	if (limbIndex < 0)
		return NULL;

	return defaultSkeleton.GetLimbDefinition(limbIndex);
}

ILINE float ComputePoseAbsoluteOffsetMax(
  const SAnimationPoseModifierParams& params,
  const IKLimbType& limb,
  const float relativeOffsetMax)
{
	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);

	uint rootIndex = limb.m_arrLimbChildren[0];
	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);
	float defaultZ = defaultSkeleton.m_poseDefaultData.GetJointsAbsoluteMain()[rootIndex].t.z;
	float currentZ = pPoseData->GetJointsAbsolute()[rootIndex].t.z;
	float difference = defaultZ - currentZ;
	float poseAbsoluteOffsetMax = currentZ + difference;
	poseAbsoluteOffsetMax -= (defaultZ - relativeOffsetMax);
	return poseAbsoluteOffsetMax;
}

ILINE bool AlignJointToPlane(const SAnimationPoseModifierParams& params, uint32 jointIndex, const Vec3& jointVector, const Vec3& planeNormal)
{
	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return false;

	QuatT* const __restrict pPoseRelative = pPoseData->GetJointsRelative();
	QuatT* const __restrict pPoseAbsolute = pPoseData->GetJointsAbsolute();

	QuatT& jointAbsolute = pPoseAbsolute[jointIndex];
	Quat orientation = Quat::CreateRotationV0V1(jointVector, planeNormal) * jointAbsolute.q;
	PoseModifierHelper::SetJointAbsolute(params, jointIndex, &orientation, NULL);
	return true;
}

/*
   CPoseAlignerChain
 */

CRYREGISTER_CLASS(CPoseAlignerChain)

//

CPoseAlignerChain::CPoseAlignerChain()
{
	m_targetLockPosition = ZERO;
	m_targetLockOrientation = IDENTITY;
	m_targetLockPlane.Set(Vec3(0.0f, 0.0f, 1.0f), 0.0f);
}

//

void CPoseAlignerChain::Initialize(LimbIKDefinitionHandle solver, int contactJointIndex)
{
	m_state.solver = solver;
	m_state.rootJointIndex = -1;
	m_state.targetJointIndex = -1;
	m_state.contactJointIndex = contactJointIndex;
	m_state.m_pIkLimbType = NULL;

	m_state.eLockMode = eLockMode_Store;
}

//

void CPoseAlignerChain::AlignToTarget(const SAnimationPoseModifierParams& params)
{
	const Vec3& targetVector = params.pPoseData->GetJointAbsolute(
	  m_stateExecute.contactJointIndex).q.GetColumn2().GetNormalized();
	AlignJointToPlane(params, m_stateExecute.targetJointIndex, targetVector, m_stateExecute.target.plane.n);
}

bool CPoseAlignerChain::MoveToTarget(const SAnimationPoseModifierParams& params, Vec3& contactPosition)
{
	float offsetMax = ComputePoseAbsoluteOffsetMax(params, *m_stateExecute.m_pIkLimbType, m_stateExecute.target.offsetMax);
	float offsetMin = ComputePoseAbsoluteOffsetMax(params, *m_stateExecute.m_pIkLimbType, m_stateExecute.target.offsetMin - 0.1f);

	const Vec3& contactPositionAnimation = params.pPoseData->GetJointAbsolute(
	  m_stateExecute.contactJointIndex).t;
	const Vec3& targetPositionAnimation = params.pPoseData->GetJointAbsolute(
	  m_stateExecute.targetJointIndex).t;

	contactPosition = contactPositionAnimation;

	if (m_stateExecute.eLockMode != eLockMode_Apply)
	{
		Ray ray(contactPosition, Vec3(0.0f, 0.0f, -1.0f));
		ray.origin.z += m_stateExecute.target.distance;
		bool bContactFound = Intersect::Ray_Plane(ray, m_stateExecute.target.plane, contactPosition);
		if (!bContactFound)
			return false;
	}

	contactPosition.z = clamp_tpl(contactPosition.z, offsetMin, offsetMax);

	if (m_stateExecute.eLockMode == eLockMode_Apply)
	{
		contactPosition = params.location.GetInverted() * m_targetLockPosition;
	}

	Vec3 contactOffset = contactPosition - contactPositionAnimation;
	Vec3 contactToTerget = targetPositionAnimation - contactPosition;

	Vec3 targetPosition = contactPosition + contactToTerget + contactOffset;

	return PoseModifierHelper::IK_Solver(
	  PoseModifierHelper::GetDefaultSkeleton(params), m_stateExecute.solver, targetPosition,
	  *Skeleton::CPoseData::GetPoseData(params.pPoseData));
}

// IAnimationPoseModifier

bool CPoseAlignerChain::Prepare(const SAnimationPoseModifierParams& params)
{
	if (!m_state.m_pIkLimbType && m_state.solver)
		m_state.m_pIkLimbType = FindIkLimbType(params, m_state.solver);
	if (!m_state.m_pIkLimbType)
		return false;

	m_state.rootJointIndex = m_state.m_pIkLimbType->m_arrRootToEndEffector[0];
	m_state.targetJointIndex = m_state.m_pIkLimbType->m_arrJointChain.back().m_idxJoint;
	m_state.m_pIkLimbType = m_state.m_pIkLimbType;
	m_stateExecute = m_state;
	return true;
}

bool CPoseAlignerChain::Execute(const SAnimationPoseModifierParams& params)
{
	DEFINE_PROFILER_FUNCTION();

	if (!m_stateExecute.m_pIkLimbType)
		return false;

	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return false;

	QuatT* const __restrict pPoseRelative = pPoseData->GetJointsRelative();
	QuatT* const __restrict pPoseAbsolute = pPoseData->GetJointsAbsolute();

	if (m_stateExecute.targetJointIndex < 0)
		return false;

	//

	QuatT poseRelativeTransformations[16];
	QuatT poseAbsoluteTransformations[16];
	uint poseTransformationCount = min(uint(m_stateExecute.m_pIkLimbType->m_arrLimbChildren.size()),
	                                   uint(sizeof(poseRelativeTransformations) / sizeof(poseAbsoluteTransformations[0])));
	const int16* pPoseTransformationIndices = &m_stateExecute.m_pIkLimbType->m_arrLimbChildren[0];
	for (uint i = 0; i < poseTransformationCount; ++i)
	{
		poseRelativeTransformations[i] = pPoseRelative[pPoseTransformationIndices[i]];
		poseAbsoluteTransformations[i] = pPoseAbsolute[pPoseTransformationIndices[i]];
	}

	//

	if (m_stateExecute.eLockMode == eLockMode_Apply)
	{
		m_stateExecute.target.plane = m_targetLockPlane;
	}

	const Vec3 contactPositionAnimation = pPoseAbsolute[m_stateExecute.contactJointIndex].t;
	Vec3 contactPosition = contactPositionAnimation;

	static const uint32 ITERATION_COUNT = 8;
	uint interations = 0;
	for (; interations < ITERATION_COUNT; ++interations)
	{
		for (uint j = 0; j < poseTransformationCount; ++j)
		{
			pPoseRelative[pPoseTransformationIndices[j]] = poseRelativeTransformations[j];
			pPoseAbsolute[pPoseTransformationIndices[j]] = poseAbsoluteTransformations[j];
		}

		AlignToTarget(params);
		bool bMoved = MoveToTarget(params, contactPosition);
		if (!bMoved)
			break;
	}

	AlignToTarget(params);

	if (m_stateExecute.eLockMode == eLockMode_Store)
	{
		m_targetLockPosition = params.location * contactPosition;
		m_targetLockPlane = m_stateExecute.target.plane;

		m_targetLockOrientation = params.location.q * pPoseAbsolute[m_stateExecute.targetJointIndex].q;
	}
	else if (m_stateExecute.eLockMode == eLockMode_Apply)
	{
		Quat o = params.location.GetInverted().q * m_targetLockOrientation;
		PoseModifierHelper::SetJointAbsolute(params, m_stateExecute.targetJointIndex, &o, NULL);
	}

	//

	for (uint i = 0; i < poseTransformationCount; ++i)
	{
		pPoseRelative[pPoseTransformationIndices[i]].SetNLerp(
		  poseRelativeTransformations[i], pPoseRelative[pPoseTransformationIndices[i]], m_stateExecute.target.targetWeight);
		pPoseAbsolute[pPoseTransformationIndices[i]].SetNLerp(
		  poseAbsoluteTransformations[i], pPoseAbsolute[pPoseTransformationIndices[i]], m_stateExecute.target.targetWeight);
	}

	//

#if !defined(_RELEASE)
	static ICVar* const pCVar = gEnv->pConsole->GetCVar("a_poseAlignerDebugDraw");
	if (pCVar)
		if (pCVar->GetIVal() > 1)
		{
			Vec3 position = contactPositionAnimation;

			// Debug Draw
			SAuxGeomRenderFlags flags;
			flags.SetAlphaBlendMode(e_AlphaBlended);
			flags.SetCullMode(e_CullModeNone);
			flags.SetDepthTestFlag(e_DepthTestOn);
			g_pAuxGeom->SetRenderFlags(flags);
			DrawHelper::Cylinder cylinder;
			cylinder.position = params.location * position;
			cylinder.direction = params.location.q * m_stateExecute.target.plane.n;
			cylinder.radius = 0.25f;
			cylinder.height = 0.01f;
			cylinder.color = ColorB(0xff, 0xff, 0xff, 0xc0);
			DrawHelper::Draw(cylinder);

			float poseAbsoluteOffsetMax = ComputePoseAbsoluteOffsetMax(params, *m_stateExecute.m_pIkLimbType, m_stateExecute.target.offsetMax);

			Ray ray(position + Vec3(0.0f, 0.0f, m_stateExecute.target.distance), Vec3(0.0f, 0.0f, -m_stateExecute.target.distance * 2.0f));
			Intersect::Ray_Plane(ray, m_stateExecute.target.plane, cylinder.position);
			if (cylinder.position.z > poseAbsoluteOffsetMax)
				cylinder.position.z = poseAbsoluteOffsetMax;
			cylinder.position = params.location * cylinder.position;
			cylinder.color = ColorB(0xff, 0xff, 0x80, 0xc0);
			DrawHelper::Draw(cylinder);

			flags.SetDepthTestFlag(e_DepthTestOff);
			g_pAuxGeom->SetRenderFlags(flags);

			DrawHelper::Sphere sphere;
			sphere.radius = 0.05f;

			sphere.position = params.location * contactPositionAnimation;
			sphere.color = ColorB(0x00, 0x00, 0xff, 0xff);
			DrawHelper::Draw(sphere);

			sphere.position = params.location * contactPositionAnimation;
			sphere.color = ColorB(0xff, 0x00, 0x00, 0xff);
			DrawHelper::Draw(sphere);

			//

			Vec3 position0 = pPoseAbsolute[m_stateExecute.m_pIkLimbType->m_arrLimbChildren[0]].t;
			Vec3 position1 = position0;
			position1.z = poseAbsoluteOffsetMax;

			position0 = params.location * position0;
			position1 = params.location * position1;

			DrawHelper::Segment segment;
			segment.positions[0] = position0;
			segment.positions[1] = position1;
			DrawHelper::DepthTest(false);
			DrawHelper::Draw(segment);

			//

			float offsetMax = ComputePoseAbsoluteOffsetMax(params, *m_stateExecute.m_pIkLimbType, m_stateExecute.target.offsetMax);
			float offsetMin = ComputePoseAbsoluteOffsetMax(params, *m_stateExecute.m_pIkLimbType, m_stateExecute.target.offsetMin - 0.1f);

			cylinder.radius = 0.125f;
			cylinder.color = ColorB(0xff, 0xff, 0xff, 0xc0);

			cylinder.position = params.location * Vec3(position.x, position.y, -abs(offsetMin) * 0.5f);
			cylinder.direction = Vec3(0.0f, 0.0f, +1.0f);
			cylinder.height = abs(offsetMin);
			DrawHelper::Draw(cylinder);

			cylinder.position = params.location * Vec3(position.x, position.y, abs(offsetMax) * 0.5f);
			cylinder.direction = Vec3(0.0f, 0.0f, -1.0f);
			cylinder.height = abs(offsetMax);
			DrawHelper::Draw(cylinder);
		}
#endif

	return true;
}

void CPoseAlignerChain::Synchronize()
{
}

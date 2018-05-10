// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PoseBlenderLook.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include "../CharacterInstance.h"
#include "../Model.h"
#include "../CharacterManager.h"
#include "../FacialAnimation/FacialInstance.h"
#include "PoseModifierHelper.h"
#include "DirectionalBlender.h"
#include <CryExtension/CryCreateClassInstance.h>

CRYREGISTER_CLASS(CPoseBlenderLook)
CPoseBlenderLook::CPoseBlenderLook()
{
	m_additionalRotationLeft.SetIdentity();
	m_additionalRotationRight.SetIdentity();
	m_pAttachmentEyeLeft = 0;
	m_pAttachmentEyeRight = 0;
}

//
bool CPoseBlenderLook::Prepare(const SAnimationPoseModifierParams& params)
{
	if (!PrepareInternal(params))
	{
		Synchronize();  // Make sure to synchronize on failure, we dont want m_Get to keep the old values when m_Set is already set
		return false;
	}

	return true;
}

bool CPoseBlenderLook::PrepareInternal(const SAnimationPoseModifierParams& params)
{
	//	float fTextColor[4] = {1,0,0,1};
	//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fTextColor, false,"Prepare Look-IK" );
	//	g_YLine+=16.0f;

	m_blender.m_dataIn = m_blender.m_Set;

	if ((m_blender.m_dataIn.bUseDirIK == 0) && (m_blender.m_dataOut.fDirIKBlend == 0.0f))
	{
		// Assume we start looking within the FOV
		m_blender.m_fFieldOfViewSmooth = 1;
		m_blender.m_fFieldOfViewRate = 0;

		m_blender.ClearSmoothingRates();
	}

	m_blender.m_numActiveDirPoses = 0;
	if (Console::GetInst().ca_UseLookIK == 0)
		return false;

	CCharInstance* pInstance = PoseModifierHelper::GetCharInstance(params);
	if (pInstance->m_pDefaultSkeleton->m_ObjectType == CGA)
		return false;  //error-check: we shouldn't execute the PM any more

	uint32 useLookIK = (m_blender.m_dataIn.bUseDirIK || m_blender.m_dataOut.fDirIKBlend);
	if (useLookIK == 0)
		return false;

	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);
	uint32 numDirBlends = defaultSkeleton.m_poseBlenderLookDesc.m_blends.size();
	if (numDirBlends == 0)
		return false;  //error-check: we shouldn't execute the PM any more
	for (uint32 d = 0; d < numDirBlends; d++)
	{
		if (defaultSkeleton.m_poseBlenderLookDesc.m_blends[d].m_nParaJointIdx < 0 || defaultSkeleton.m_poseBlenderLookDesc.m_blends[d].m_nStartJointIdx < 0 || defaultSkeleton.m_poseBlenderLookDesc.m_blends[d].m_nReferenceJointIdx < 0)
		{
			const char* pModelName = pInstance->m_pDefaultSkeleton->GetModelFilePath();
			g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, pInstance->GetFilePath(), "CryAnimation: No look-bone specified for model: %s", pModelName);
			return false;  //error-check: we shouldn't execute the PM any more
		}
	}

	uint32 numRotJoints = defaultSkeleton.m_poseBlenderLookDesc.m_rotations.size();
	if (numRotJoints == 0)
	{
		const char* pModelName = pInstance->m_pDefaultSkeleton->GetModelFilePath();
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, pInstance->GetFilePath(), "CryAnimation: No LookIK-Setup for model: %s", pModelName);
		return false;   //error-check: we shouldn't execute the PM any more
	}

	if (defaultSkeleton.m_poseBlenderLookDesc.m_blends.size())
	{
		int32 nParaJointIdx = defaultSkeleton.m_poseBlenderLookDesc.m_blends[0].m_nParaJointIdx;
		if (nParaJointIdx > 0)
		{
			if (m_blender.m_dataIn.bUseDirIK && Console::GetInst().ca_DrawAimPoses)
			{
				g_pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);
				static Ang3 angle1(0, 0, 0);
				angle1 += Ang3(0.01f, 0.02f, 0.08f);
				AABB aabb = AABB(Vec3(-0.09f, -0.09f, -0.09f), Vec3(+0.09f, +0.09f, +0.09f));
				OBB obb1 = OBB::CreateOBBfromAABB(Matrix33::CreateRotationXYZ(angle1), aabb);
				g_pAuxGeom->DrawOBB(obb1, m_blender.m_dataIn.vDirIKTarget, 1, RGBA8(0x00, 0x00, 0xff, 0xff), eBBD_Extremes_Color_Encoded);
			}

			const f32 fFrameTime = max(0.0f, params.timeDelta);
			const bool bFadeOut = (m_blender.m_nDirIKDistanceFadeOut || m_blender.m_dataIn.bUseDirIK == 0);
			const f32 fIKBlendRate = bFadeOut ? -m_blender.m_dataIn.fDirIKFadeOutTime : m_blender.m_dataIn.fDirIKFadeInTime;
			const f32 fIkBlendDelta = fIKBlendRate * fFrameTime;
			m_blender.m_dataOut.fDirIKBlend = clamp_tpl(m_blender.m_dataOut.fDirIKBlend + fIkBlendDelta, 0.0f, 1.0f);
		}
	}

	m_blender.m_dataOut.fDirIKInfluence = 0;
	if (m_blender.m_nDirIKDistanceFadeOut == 0 && m_blender.m_dataOut.fDirIKBlend < 0.001f)
		return false;  //IK not visible: we shouldn't execute the PM any more

	CSkeletonAnim* pSkeletonAnim = PoseModifierHelper::GetSkeletonAnim(params);
	CAnimationSet* pAnimationSet = pInstance->m_pDefaultSkeleton->m_pAnimationSet;
	uint32 nDirIKLayer = m_blender.m_dataIn.nDirLayer;
	if (nDirIKLayer < 1 || nDirIKLayer >= numVIRTUALLAYERS)
		return false;

	//----------------------------------------------------------------------------------------
	//-----  check if there are dir-poses in the same layer like the DIR-IK              -----
	//-----  this part will be replaced by a VEG (Parametric Sampler)                    -----
	//-----  an Aim-Pose or Look-Pose is simply a 2D-VEG that we can play in any layer   -----
	//----------------------------------------------------------------------------------------
	for (uint32 i = 0; i < MAX_EXEC_QUEUE * 2; i++)
	{
		m_blender.m_DirInfo[i].m_fWeight = 0;
		m_blender.m_DirInfo[i].m_nGlobalDirID0 = -1;
	}
	f32 fLayerWeight = pSkeletonAnim->m_layers[nDirIKLayer].m_transitionQueue.m_fLayerTransitionWeight;
	const DynArray<CAnimation>& rCurLayer = pSkeletonAnim->m_layers[nDirIKLayer].m_transitionQueue.m_animations;
	uint32 numAnimsInLayer = rCurLayer.size();
	uint32 numActiveAnims = 0;
	for (uint32 a = 0; a < numAnimsInLayer; a++)
	{
		if (!rCurLayer[a].IsActivated())
			break;
		numActiveAnims++;
	}
	for (uint32 i = 0; i < numActiveAnims; i++)
	{
		int32 nAnimID = rCurLayer[i].GetAnimationId();
		assert(rCurLayer[i].IsActivated());
		const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(nAnimID);
		assert(pAnim);
		assert(pAnim->m_nGlobalAnimId > -1);
		if (pAnim->m_nAssetType == AIM_File)
		{
			GlobalAnimationHeaderAIM& rGAH = g_AnimationManager.m_arrGlobalAIM[pAnim->m_nGlobalAnimId];
			uint32 numPoses = rGAH.m_arrAimIKPosesAIM.size();
			if (numPoses == 0)
				continue;
			if (rGAH.m_nExist == 0)
				continue;

			SDirInfo& rCurDirInfo = m_blender.m_DirInfo[m_blender.m_numActiveDirPoses];
			rCurDirInfo.m_fWeight = rCurLayer[i].GetTransitionWeight() * fLayerWeight;
			rCurDirInfo.m_nGlobalDirID0 = pAnim->m_nGlobalAnimId;
			m_blender.m_numActiveDirPoses++;
		}
	}
	if (m_blender.m_numActiveDirPoses == 0)
		return false; //no look-pose specified

	//----------------------------------------------------------------------

	IAttachmentManager* pIAttachmentManager = params.pCharacterInstance->GetIAttachmentManager();
	const char* pstrLEye = defaultSkeleton.m_poseBlenderLookDesc.m_eyeAttachmentLeftName.c_str();
	int32 alidx = pIAttachmentManager->GetIndexByName(pstrLEye);
	if (alidx >= 0)
	{
		m_pAttachmentEyeLeft = (CAttachmentBONE*)pIAttachmentManager->GetInterfaceByIndex(alidx);
	}
	else
	{
		m_pAttachmentEyeLeft = NULL;
	}

	const char* pstrREye = defaultSkeleton.m_poseBlenderLookDesc.m_eyeAttachmentRightName.c_str();
	int32 aridx = pIAttachmentManager->GetIndexByName(pstrREye);
	if (aridx >= 0)
	{
		m_pAttachmentEyeRight = (CAttachmentBONE*)pIAttachmentManager->GetInterfaceByIndex(aridx);
	}
	else
	{
		m_pAttachmentEyeRight = NULL;
	}

	m_ql.SetIdentity();
	if (m_pAttachmentEyeLeft)
	{
		m_pAttachmentEyeLeft->m_addTransformation.SetIdentity();
		m_ql = m_pAttachmentEyeLeft->GetAttRelativeDefault();
		m_EyeIdxL = m_pAttachmentEyeLeft->GetJointID();
	}

	m_qr.SetIdentity();
	if (m_pAttachmentEyeRight)
	{
		m_pAttachmentEyeRight->m_addTransformation.SetIdentity();
		m_qr = m_pAttachmentEyeRight->GetAttRelativeDefault();
		m_EyeIdxR = m_pAttachmentEyeRight->GetJointID();
	}

	m_eyeLimitHalfYawRadians = defaultSkeleton.m_poseBlenderLookDesc.m_eyeLimitHalfYawRadians;
	m_eyeLimitPitchRadiansUp = defaultSkeleton.m_poseBlenderLookDesc.m_eyeLimitPitchRadiansUp;
	m_eyeLimitPitchRadiansDown = defaultSkeleton.m_poseBlenderLookDesc.m_eyeLimitPitchRadiansDown;

	return true;
}

bool CPoseBlenderLook::Execute(const SAnimationPoseModifierParams& params)
{
	DEFINE_PROFILER_FUNCTION();

	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return false;

	//	float fTextColor[4] = {1,0,0,1};
	//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fTextColor, false,"Execute Look-IK" );
	//	g_YLine+=16.0f;

	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);
	uint32 numDB = defaultSkeleton.m_poseBlenderLookDesc.m_blends.size();
	uint32 numRotJoints = defaultSkeleton.m_poseBlenderLookDesc.m_rotations.size();
	uint32 numPosJoints = defaultSkeleton.m_poseBlenderLookDesc.m_positions.size();

	const SJointsAimIK_Pos* pPos = 0;
	if (numPosJoints)
		pPos = &defaultSkeleton.m_poseBlenderLookDesc.m_positions[0];
	m_blender.ExecuteDirectionalIK(params, &defaultSkeleton.m_poseBlenderLookDesc.m_blends[0], numDB, &defaultSkeleton.m_poseBlenderLookDesc.m_rotations[0], numRotJoints, pPos, numPosJoints);

	//--------------------------------------------------------------------------
	//--- procedural adjustments
	//--------------------------------------------------------------------------
	{
		const CDefaultSkeleton::SJoint* parrModelJoints = &defaultSkeleton.m_arrModelJoints[0];
		QuatT* const __restrict pRelPose = pPoseData->GetJointsRelative();
		QuatT* const __restrict pAbsPose = pPoseData->GetJointsAbsolute();

		uint32 numJoints = defaultSkeleton.GetJointCount();
		pAbsPose[0] = pRelPose[0];
		for (uint32 j = 1; j < numJoints; j++)
		{
			int32 p = parrModelJoints[j].m_idxParent;
			pAbsPose[j] = pAbsPose[p] * pRelPose[j];
			pAbsPose[j].q.Normalize();
		}

		const QuatTS& rAnimLocationNext = params.location;
		Vec3 vLocalAimIKTarget = (m_blender.m_dataIn.vDirIKTarget - rAnimLocationNext.t) * rAnimLocationNext.q;
		vLocalAimIKTarget /= params.pCharacterInstance->GetUniformScale();

		if (m_pAttachmentEyeLeft)
		{
			QuatT LAbsolutePose = pAbsPose[m_EyeIdxL] * m_ql;
			Vec3 CamPosL = !(pAbsPose[m_EyeIdxL].q * m_ql.q) * (vLocalAimIKTarget - LAbsolutePose.t).GetNormalizedSafe(Vec3(0, 1, 0));
			LimitEye(CamPosL);
			Quat absLEye = Quat::CreateRotationVDir(CamPosL);
			m_additionalRotationLeft = absLEye;

#ifdef EDITOR_PCDEBUGCODE
			if (Console::GetInst().ca_DrawLookIK == 2)
			{
				g_pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags | e_DepthTestOff);
				QuatT w = QuatT(rAnimLocationNext);
				g_pAuxGeom->DrawLine(w * LAbsolutePose.t, RGBA8(0xff, 0x80, 0x80, 0xff), w * (LAbsolutePose.t + absLEye.GetColumn0() * 0.04f), RGBA8(0xff, 0x00, 0x00, 0xff));
				g_pAuxGeom->DrawLine(w * LAbsolutePose.t, RGBA8(0x80, 0xff, 0x80, 0xff), w * (LAbsolutePose.t + absLEye.GetColumn1() * 0.04f), RGBA8(0x00, 0xff, 0x00, 0xff));
				g_pAuxGeom->DrawLine(w * LAbsolutePose.t, RGBA8(0x80, 0x80, 0xff, 0xff), w * (LAbsolutePose.t + absLEye.GetColumn2() * 0.04f), RGBA8(0x00, 0x00, 0xff, 0xff));
				Quat q = m_additionalRotationLeft;
				float fTextColor[4] = { 1, 0, 0, 1 };
				g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.4f, fTextColor, false, "EyeLeft: %f %f %f %f  m_IKBlend: %f", q.w, q.v.x, q.v.y, q.v.z, m_blender.m_dataOut.fDirIKInfluence);
				g_YLine += 16.0f;
			}
#endif
		}

		if (m_pAttachmentEyeRight)
		{
			QuatT RAbsolutePose = pAbsPose[m_EyeIdxR] * m_qr;
			Vec3 CamPosR = !(pAbsPose[m_EyeIdxR].q * m_qr.q) * (vLocalAimIKTarget - RAbsolutePose.t).GetNormalizedSafe(Vec3(0, 1, 0));
			LimitEye(CamPosR);
			Quat absREye = Quat::CreateRotationVDir(CamPosR);
			m_additionalRotationRight = absREye;

#ifdef EDITOR_PCDEBUGCODE
			if (Console::GetInst().ca_DrawLookIK == 2)
			{
				g_pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags | e_DepthTestOff);
				QuatT w = QuatT(rAnimLocationNext);
				g_pAuxGeom->DrawLine(w * RAbsolutePose.t, RGBA8(0xff, 0x00, 0x00, 0xff), w * (RAbsolutePose.t + absREye.GetColumn0() * 0.04f), RGBA8(0xff, 0x00, 0x00, 0xff));
				g_pAuxGeom->DrawLine(w * RAbsolutePose.t, RGBA8(0x00, 0xff, 0x00, 0xff), w * (RAbsolutePose.t + absREye.GetColumn1() * 0.04f), RGBA8(0x00, 0xff, 0x00, 0xff));
				g_pAuxGeom->DrawLine(w * RAbsolutePose.t, RGBA8(0x00, 0x00, 0xff, 0xff), w * (RAbsolutePose.t + absREye.GetColumn2() * 0.04f), RGBA8(0x00, 0x00, 0xff, 0xff));
				Quat q = m_additionalRotationRight;
				float fTextColor[4] = { 1, 0, 0, 1 };
				g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.4f, fTextColor, false, "EyeRight: %f %f %f %f  m_IKBlend: %f", q.w, q.v.x, q.v.y, q.v.z, m_blender.m_dataOut.fDirIKInfluence);
				g_YLine += 16.0f;
			}
#endif
		}

	}

	return true;
}

// TEMP: Move this to DrawHelper.h

namespace DrawHelper
{
void Frame(const Vec3& position, const Vec3& axisX, const Vec3& axisY, const Vec3& axisZ);
void Frame(const Matrix34& location, const Vec3& scale);
void Frame(const QuatT& location, const Vec3& scale);
} // namespace DrawHelper

void CPoseBlenderLook::Synchronize()
{
	m_blender.m_Get = m_blender.m_dataOut;

	if (m_pAttachmentEyeLeft)
	{
#if !defined(RELEASE)
		if (Console::GetInst().ca_DrawLookIK == 1)
			DrawHelper::Frame(QuatT(m_pAttachmentEyeLeft->GetAttWorldAbsolute()), Vec3(0.2f, 0.2f, 0.2f));
#endif
		m_pAttachmentEyeLeft->m_addTransformation.q.SetNlerp(IDENTITY, m_additionalRotationLeft, m_blender.m_Get.fDirIKInfluence);
		m_pAttachmentEyeLeft = NULL;
	}

	if (m_pAttachmentEyeRight)
	{
#if !defined(RELEASE)
		if (Console::GetInst().ca_DrawLookIK == 1)
			DrawHelper::Frame(QuatT(m_pAttachmentEyeRight->GetAttWorldAbsolute()), Vec3(0.2f, 0.2f, 0.2f));
#endif
		m_pAttachmentEyeRight->m_addTransformation.q.SetNlerp(IDENTITY, m_additionalRotationRight, m_blender.m_Get.fDirIKInfluence);
		m_pAttachmentEyeRight = NULL;
	}
}

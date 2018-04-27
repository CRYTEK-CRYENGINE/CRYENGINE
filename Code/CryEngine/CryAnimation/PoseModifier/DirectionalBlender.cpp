// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "DirectionalBlender.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include "../CharacterInstance.h"
#include "../Model.h"
#include "../CharacterManager.h"
#include "PoseModifierHelper.h"

#define MAX_ROTATION_JOINTS 64

//-----------------------------------------------------------------------------------------------------
//-----                  compute pose for AIM-IK and for Look-IK                            -----------
//-----------------------------------------------------------------------------------------------------
bool SDirectionalBlender::ExecuteDirectionalIK(const SAnimationPoseModifierParams& params, const DirectionalBlends* rDirBlends, const uint32 numDB, const SJointsAimIK_Rot* rRot, const uint32 numRotJoints, const SJointsAimIK_Pos* rPos, const uint32 numPosJoints)
{
	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return false;

	float fColDebug[4] = { 1, 1, 0, 1 };

	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);
	const CDefaultSkeleton::SJoint* parrModelJoints = &defaultSkeleton.m_arrModelJoints[0];

	QuatT* const __restrict pRelPose = pPoseData->GetJointsRelative();
	QuatT* const __restrict pAbsPose = pPoseData->GetJointsAbsolute();

	const QuatTS& rAnimLocationNext = params.location;
	Vec3 vLocalAimIKTarget = (m_dataIn.vDirIKTarget - rAnimLocationNext.t) * rAnimLocationNext.q;
	vLocalAimIKTarget /= params.pCharacterInstance->GetUniformScale();

	uint32 numJoints = pPoseData->GetJointCount();
	pAbsPose[0] = pRelPose[0];
	for (uint32 i = 1; i < numJoints; i++)
		pAbsPose[i] = pAbsPose[defaultSkeleton.GetJointParentIDByID(i)] * pRelPose[i];

	//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.6f, fColDebug, false,"rAnimLocationNext: %f (%f %f %f)",rAnimLocationNext.q.w,rAnimLocationNext.q.v.x,rAnimLocationNext.q.v.y,rAnimLocationNext.q.v.z );
	//	g_YLine+=16.0f;

	const QuatT wRefJoint = QuatT(rAnimLocationNext) * pAbsPose[rDirBlends->m_nReferenceJointIdx];

	const Vec3 fw = wRefJoint.q.GetColumn1();
	Vec3 up = (rDirBlends->m_nReferenceJointIdx) ? wRefJoint.q.GetColumn0() : wRefJoint.q.GetColumn2();
	const Vec3 si = up % fw;

	const Matrix33 p33 = Matrix33(wRefJoint.q);
	Plane LR_Plane;
	LR_Plane.SetPlane(up % fw, wRefJoint.t);
	Plane FB_Plane;
	FB_Plane.SetPlane(si % up, wRefJoint.t);
	Plane UD_Plane;
	UD_Plane.SetPlane(fw % si, wRefJoint.t);
	const uint32 LR = isneg(LR_Plane | m_dataIn.vDirIKTarget);
	const uint32 FB = isneg(FB_Plane | m_dataIn.vDirIKTarget);
	const float UD = UD_Plane | m_dataIn.vDirIKTarget;
	const Vec3 OnPlane = m_dataIn.vDirIKTarget - UD * UD_Plane.n;
	const f32 CosineY = (OnPlane - wRefJoint.t).GetNormalizedSafe(Vec3(0, 1, 0)) | fw;

	for (int i = 0; i < MAX_POLAR_COORDINATES_SMOOTH; ++i)
	{
		m_polarCoordinatesSmooth[i].Prepare();
	}
	/*
	   g_pAuxGeom->DrawLine(	wRefJoint.t,RGBA8(0x7f,0x00,0x00,0xff), wRefJoint.t+up*10,RGBA8(0xff,0x00,0x00,0xff) );
	   g_pAuxGeom->DrawLine(	wRefJoint.t,RGBA8(0x00,0x7f,0x00,0xff), wRefJoint.t+fw*10,RGBA8(0x00,0xff,0x00,0xff) );
	   g_pAuxGeom->DrawLine(	wRefJoint.t,RGBA8(0x00,0x00,0x7f,0xff), wRefJoint.t+si*10,RGBA8(0x00,0x00,0xff,0xff) );


	   g_pAuxGeom->Draw2dLabel( 1,g_YLine, 2.4f, fColDebug, false,"LR: %f   UD: %f   OnPlane: %f %f %f   CosineY: %f",(LR_Plane|m_vDirIKTargetSmooth),UD,OnPlane.x,OnPlane.y,OnPlane.z, CosineY );
	   g_YLine+=26.0f;

	   if (LR==0)
	   {
	   g_pAuxGeom->Draw2dLabel( 1,g_YLine, 2.4f, fColDebug, false,"Left Side");
	   g_YLine+=26.0f;
	   }
	   if (LR==1)
	   {
	   g_pAuxGeom->Draw2dLabel( 1,g_YLine, 2.4f, fColDebug, false,"Right Side");
	   g_YLine+=26.0f;
	   }

	   if (FB==0)
	   {
	   g_pAuxGeom->Draw2dLabel( 1,g_YLine, 2.4f, fColDebug, false,"Front Side");
	   g_YLine+=26.0f;
	   }
	   if (FB==1)
	   {
	   g_pAuxGeom->Draw2dLabel( 1,g_YLine, 2.4f, fColDebug, false,"Back Side");
	   g_YLine+=26.0f;
	   }
	 */

	//------------------------------------------------------------------------------------
	//---------         here we know that we can actually use AimIK          -------------
	//------------------------------------------------------------------------------------
	const uint32 clampedNumDB = min(numDB, (uint32)MAX_NUM_DB);
	for (uint32 d = 0; d < clampedNumDB; d++)
	{
		m_dbw[d].m_nAnimTokenCRC32 = rDirBlends[d].m_AnimTokenCRC32;
		m_dbw[d].m_nParaJointIdx = rDirBlends[d].m_nParaJointIdx;
		m_dbw[d].m_nRotParaJointIdx = rDirBlends[d].m_nRotParaJointIdx;
		m_dbw[d].m_nStartJointIdx = rDirBlends[d].m_nStartJointIdx;
		m_dbw[d].m_nRotStartJointIdx = rDirBlends[d].m_nRotStartJointIdx;
		m_dbw[d].m_fWeight = 0.0f;
	}

	//Some rules to disable Aim-IK. Target is too close to the (first) parameter joint
	if (0.f < m_dataIn.fDirIKMinDistanceSquared)
	{
		const int32 parameterJoint0Idx = m_dbw[0].m_nParaJointIdx;
		const Vec3& parameterJoint0LocalPosition = pAbsPose[parameterJoint0Idx].t;
		const f32 lookDistanceSquared = (vLocalAimIKTarget - parameterJoint0LocalPosition).GetLengthSquared();

		if (m_nDirIKDistanceFadeOut == 0)
		{
			if (lookDistanceSquared < m_dataIn.fDirIKMinDistanceSquared)
			{
				m_nDirIKDistanceFadeOut = 1;
			}
		}
		else
		{
			const f32 offsetForHisteresis = 0.05f;
			const f32 dirIKMinDistanceSquaredOffseted = m_dataIn.fDirIKMinDistanceSquared + offsetForHisteresis;
			if (dirIKMinDistanceSquaredOffseted < lookDistanceSquared)
			{
				m_nDirIKDistanceFadeOut = 0;
			}
		}
	}
	else
	{
		m_nDirIKDistanceFadeOut = 0;
	}

	f32 fWeightOfAllAimPoses = 0;
	for (int32 i = 0; i < m_numActiveDirPoses; i++)
		fWeightOfAllAimPoses += m_DirInfo[i].m_fWeight;

	const f32 fStatus = f32(CosineY > cosf(m_dataIn.fDirIKFadeoutRadians));
	const f32 fieldOfViewSmoothSeconds = 0.2f;
	SmoothCD(m_fFieldOfViewSmooth, m_fFieldOfViewRate, params.timeDelta, fStatus, fieldOfViewSmoothSeconds);

	//	g_YLine+=16.0f;
	//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.6f, fColDebug, false,"fStatus: %f  m_fDirIKFadeoutAngle: %f",fStatus,m_fDirIKFadeoutAngle );
	//	g_YLine+=16.0f;

	QuatT arrRelPose[MAX_ROTATION_JOINTS];
	f32 fIKBlend = clamp_tpl(m_dataOut.fDirIKBlend, 0.0f, 1.0f) - 0.5f;
	fIKBlend = fIKBlend / (0.5f + 2.0f * fIKBlend * fIKBlend) + 0.5f;
	fIKBlend *= m_fFieldOfViewSmooth;
	m_dataOut.fDirIKInfluence = fIKBlend * fWeightOfAllAimPoses;

	const f32 t0 = 1.0f - m_dataOut.fDirIKInfluence;
	assert(t0 < 1.001f);
	assert(t0 > -0.001f);

	const f32 t1 = fIKBlend;
	const SJointsAimIK_Rot* pAimIK_Rot = &rRot[0];
	for (uint32 r = 0; r < numRotJoints; r++)
	{
		int32 j = pAimIK_Rot[r].m_nJointIdx;

		arrRelPose[r].q.w = pRelPose[j].q.w;
		pRelPose[j].q.w *= t0;
		arrRelPose[r].q.v.x = pRelPose[j].q.v.x;
		pRelPose[j].q.v.x *= t0;
		arrRelPose[r].q.v.y = pRelPose[j].q.v.y;
		pRelPose[j].q.v.y *= t0;
		arrRelPose[r].q.v.z = pRelPose[j].q.v.z;
		pRelPose[j].q.v.z *= t0;
		arrRelPose[r].t.x = pRelPose[j].t.x;
		pRelPose[j].t.x *= t0;
		arrRelPose[r].t.y = pRelPose[j].t.y;
		pRelPose[j].t.y *= t0;
		arrRelPose[r].t.z = pRelPose[j].t.z;
		pRelPose[j].t.z *= t0;
	}

	assert(m_numActiveDirPoses);

	// TODO: Turn this around so that a single aimpose for an animation is not accumulated separately ever.
	f32 fTotalWeight = t0;
	const GlobalAnimationHeaderAIM* parrGlobalAIM = &g_AnimationManager.m_arrGlobalAIM[0];
	for (int32 a = 0; a < m_numActiveDirPoses; a++)
	{
		assert(m_DirInfo[a].m_nGlobalDirID0 >= 0);
		const GlobalAnimationHeaderAIM& rAIM = parrGlobalAIM[m_DirInfo[a].m_nGlobalDirID0];
		const uint32 nAnimTokenCRC32 = rAIM.m_AnimTokenCRC32;
		const f32 fAimPoseWeight = m_DirInfo[a].m_fWeight;
		const int polarCoordinatesIndex = GetOrAssignPolarCoordinateSmoothIndex(m_DirInfo[a].m_nGlobalDirID0);
		for (uint32 d = 0; d < clampedNumDB; d++)
		{
			if (m_dbw[d].m_nAnimTokenCRC32 == nAnimTokenCRC32)
			{
				m_dbw[d].m_fWeight += t1 * fAimPoseWeight;
				AccumulateAimPoses(params, rAIM, arrRelPose, t1 * fAimPoseWeight, vLocalAimIKTarget, m_dbw[d].m_nParaJointIdx, m_dbw[d].m_nRotParaJointIdx, m_dbw[d].m_nStartJointIdx, m_dbw[d].m_nRotStartJointIdx, rRot, numRotJoints, rPos, CosineY, (LR | FB << 1), polarCoordinatesIndex);
				fTotalWeight += t1 * fAimPoseWeight;
#ifdef EDITOR_PCDEBUGCODE
				if (Console::GetInst().ca_DrawAimIKVEGrid) DebugVEGrid(params, m_DirInfo[a], fAimPoseWeight, fIKBlend, m_dbw[d].m_nParaJointIdx, rRot, numRotJoints, rPos, numPosJoints);
#endif
				break;
			}
		}
	}
	assert(fabsf(fTotalWeight - 1.0f) < 0.001f);

	for (uint32 r = 0; r < numRotJoints; r++)
	{
		int32 j = pAimIK_Rot[r].m_nJointIdx;
		int32 p = parrModelJoints[j].m_idxParent;

		if ((pRelPose[j].q.w * pRelPose[j].q.w + pRelPose[j].q.v.len2()) < FLT_EPSILON)
		{
			if (Console::GetInst().ca_AnimWarningLevel >= 2)
			{
				Vec3 v = pRelPose[j].q.v;
				gEnv->pLog->LogError("Relative rotation of joint with index %d for character: %s is too small: [w=%f, x=%f, y=%f, z=%f]", j, params.pCharacterInstance->GetFilePath(), pRelPose[j].q.w, v.x, v.y, v.z);
				CSkeletonAnim* pSkeletonAnim = (CSkeletonAnim*)params.GetISkeletonAnim();
				pSkeletonAnim->DebugLogTransitionQueueState();
			}
			pRelPose[j].q.SetIdentity();
		}
		else
		{
			pRelPose[j].q.Normalize();
		}

		pAbsPose[j] = pAbsPose[p] * pRelPose[j];
	}

#ifdef EDITOR_PCDEBUGCODE
	for (uint32 g = 0; g < numDB; g++)
	{
		int32 nDirJointIdx = m_dbw[g].m_nParaJointIdx;
		f32 wR = m_dbw[g].m_fWeight;

		float fTextColor[4] = { 1, 0, 0, 1 };
		if (Console::GetInst().ca_DrawAimPoses && wR > 0.01f)
		{
			Vec3 RWBone = rAnimLocationNext.q * pAbsPose[nDirJointIdx].t;
			Vec3 WStart = rAnimLocationNext.t + RWBone;
			Vec3 realdir = pAbsPose[nDirJointIdx].q.GetColumn1();
			Vec3 WEnd = WStart + rAnimLocationNext.q * realdir * 10;

			g_pAuxGeom->DrawLine(WStart, RGBA8(0x00, 0x7f, 0x7f, 0xff), WEnd, RGBA8(0x00, 0xff, 0xff, 0xff));
			GlobalAnimationHeaderAIM rAIM;
			Vec2 vPolarCoord = Vec2(rAIM.Debug_PolarCoordinate(pAbsPose[nDirJointIdx].q));
			g_YLine += 16.0f;
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.4f, fTextColor, false, "final polar: %f %f   wR: %f", vPolarCoord.x, vPolarCoord.y, wR);
			g_YLine += 16.0f;
		}
		//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fTextColor, false,"Aim-IK Influence: %f %f %f",m_fDirIKInfluence,fTotalWeight,fIKBlend );
		//	g_YLine+=16.0f;
	}
#endif

	if (CosineY < 0.0f)
		return 0;

	return true;
}

void SDirectionalBlender::AccumulateAimPoses(const SAnimationPoseModifierParams& params, const GlobalAnimationHeaderAIM& rAim, const QuatT* arrRelPose, const f32 fIKBlend, const Vec3& vLocalAimIKTarget, const int32 nParaJointIdx, const int32 nRotParaJointIdx, const int32 nStartJointIdx, const int32 nRotStartJointIdx, const SJointsAimIK_Rot* rRot, const uint32 numRotJoints, const SJointsAimIK_Pos* rPos, const f32 CosineY, const uint32 sides, const uint32 polarCoordinatesIndex)
{
	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return;

	CCharInstance* pInstance = PoseModifierHelper::GetCharInstance(params);

#ifdef EDITOR_PCDEBUGCODE
	Matrix34 debugGridLocation;
	debugGridLocation.SetRotationX(-gf_PI / 2);
	debugGridLocation.AddTranslation(params.location.t + Vec3(0.f, 0.f, 1.6f));
#endif

	const uint32 numAimPosesCAF = rAim.m_arrAimIKPosesAIM.size();
	assert(numAimPosesCAF == 9 || numAimPosesCAF == 15 || numAimPosesCAF == 21);
	if (numAimPosesCAF == 0)
		return;

	const uint32 AimPoseMid = uint32(numAimPosesCAF / 2);

	const CDefaultSkeleton* pDefaultSkeleton = pInstance->m_pDefaultSkeleton;
	const CDefaultSkeleton::SJoint* parrModelJoints = &pDefaultSkeleton->m_arrModelJoints[0];

	////////////////////////////////////////////////////////////////////////////

	const float fTextColor[4] = { 1, 0, 0, 1 };

	QuatT qtemp;
	const DynArray<Vec3>& rAimMidPosistions = rAim.m_arrAimIKPosesAIM[AimPoseMid].m_arrPosition;
	const Quat* const __restrict pMiddleRot = &rAim.m_arrAimIKPosesAIM[AimPoseMid].m_arrRotation[0];
	const Vec3* __restrict pMiddlePos = 0;
	if (!rAimMidPosistions.empty())
		pMiddlePos = &rAimMidPosistions[0];

	QuatT* const __restrict jointsRelative = pPoseData->GetJointsRelative();
	const QuatT* const __restrict jointsAbsolute = pPoseData->GetJointsAbsolute();

	//g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fTextColor, false,"rAim.m_nExist: %016x",rAim.m_nExist);
	//g_YLine+=16.0f;

	Vec3 polarCoordinatesSmooth(ZERO);
	bool needToCalculatePolarCoordinates = true;
	if (polarCoordinatesIndex < MAX_POLAR_COORDINATES_SMOOTH)
	{
		needToCalculatePolarCoordinates = !m_polarCoordinatesSmooth[polarCoordinatesIndex].smoothingDone;
	}

	if (needToCalculatePolarCoordinates)
	{
		QuatT localAbsJointCalc[MAX_ROTATION_JOINTS];
		for (uint32 r = 0; r < numRotJoints; ++r)
		{
			const SJointsAimIK_Rot& rotJoint = rRot[r];
			if (rotJoint.m_nPreEvaluate)
			{
				const int32 jointIdx = rotJoint.m_nJointIdx;

				const int32 parentJointIdx = parrModelJoints[jointIdx].m_idxParent;
				qtemp = arrRelPose[r];
				if (rAim.m_nExist & (uint64(1) << r))
				{
					//	CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);
					//	const char* pjointname = defaultSkeleton.GetJointNameByID(j);
					//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fTextColor, false,"rot joint: %d %s",j,pjointname);
					//	g_YLine+=16.0f;

					if (rotJoint.m_nAdditive == 0)
						qtemp.q = pMiddleRot[r];

					const int32 posIdx = rotJoint.m_nPosIndex;
					if (posIdx > -1 && pMiddlePos)
					{
						if (rPos[posIdx].m_nAdditive == 0)
							qtemp.t = pMiddlePos[posIdx];
					}
				}

				QuatT parentJointAbs = jointsAbsolute[parentJointIdx];
				const int32 rotJointParentIndex = rotJoint.m_nRotJointParentIdx;
				if (rotJointParentIndex != -1)
				{
					parentJointAbs = localAbsJointCalc[rotJointParentIndex];
				}
				localAbsJointCalc[r] = parentJointAbs * qtemp;
			}
		}

		const Quat paramaterJointAbsoluteOrientation = (0 <= nRotParaJointIdx) ? localAbsJointCalc[nRotParaJointIdx].q : jointsAbsolute[nParaJointIdx].q;
		const Quat qMiddle = paramaterJointAbsoluteOrientation * rAim.m_MiddleAimPoseRot;

		//	QuatT qMiddle2 = jointsAbsolute[widx]*rAim.m_MiddleAimPoseRot;
		//	g_pAuxGeom->DrawLine( qMiddle2.t,col, qMiddle2.t+qMiddle2.GetColumn1()*10.0f,col );
		//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fTextColor, false,"qMiddle: %f (%f %f %f)",qMiddle.w,  qMiddle.v.x,qMiddle.v.y,qMiddle.v.z );
		//	g_YLine+=16.0f;

		Vec3 vRefPose = Vec3(0, 0, 1.6f);
		if (nStartJointIdx > -1)
		{
			vRefPose = (0 <= nRotStartJointIdx) ? localAbsJointCalc[nRotStartJointIdx].t : jointsAbsolute[nStartJointIdx].t;
		}

		const Vec3 vAimFromRefPosition = (vLocalAimIKTarget - vRefPose).GetNormalized();
		const Vec3 vAimVec = vAimFromRefPosition * qMiddle;

		Vec2 polarCoordinates;
		polarCoordinates.x = atan2f(-vAimVec.x, vAimVec.y);
		f32 s1, c1;
		sincos_tpl(polarCoordinates.x, &s1, &c1);
		polarCoordinates.y = -atan2f(vAimVec.z, -vAimVec.x * s1 + vAimVec.y * c1);

		polarCoordinates += m_dataIn.vPolarCoordinatesOffset;

		const f32 nearPi = gf_PI - 0.01f;
		const f32 nearHalfPi = 0.5f * gf_PI - 0.01f;

		const bool isTargetBehindCharacter = ((sides & 2) != 0);
		if (isTargetBehindCharacter)
		{
			const bool isTargetRightOfCharacter = ((sides & 1) != 0);
			//    g_pAuxGeom->Draw2dLabel(1,g_YLine, 2.4f, fTextColor, false,"Behind the Character, to his %s", isTargetRightOfCharacter ? "right" : "left");
			//    g_YLine+=26.0f;

			if (!isTargetRightOfCharacter && polarCoordinates.x < 0.0f)
				polarCoordinates.x = nearPi;

			if (isTargetRightOfCharacter && polarCoordinates.x > 0.0f)
				polarCoordinates.x = -nearPi;
		}

		{
			polarCoordinates.x = clamp_tpl(polarCoordinates.x, -nearPi, nearPi);
			polarCoordinates.y = clamp_tpl(polarCoordinates.y, -nearHalfPi, nearHalfPi);
		}

		polarCoordinatesSmooth = polarCoordinates;
		if (polarCoordinatesIndex < MAX_POLAR_COORDINATES_SMOOTH)
		{
			m_polarCoordinatesSmooth[polarCoordinatesIndex].Smooth(polarCoordinates, params.timeDelta, m_dataIn.fPolarCoordinatesSmoothTimeSeconds, m_dataIn.vPolarCoordinatesMaxRadiansPerSecond);
			polarCoordinatesSmooth = m_polarCoordinatesSmooth[polarCoordinatesIndex].value;
		}

#ifdef EDITOR_PCDEBUGCODE
		if (Console::GetInst().ca_DrawAimIKVEGrid)
		{
			const ColorB colours[MAX_POLAR_COORDINATES_SMOOTH + 1] = { Col_Red, Col_Green, Col_Blue, Col_Yellow, Col_Magenta, Col_Cyan, Col_Salmon, Col_Violet, Col_Wheat };
			const uint32 colorIndex = min(polarCoordinatesIndex, MAX_POLAR_COORDINATES_SMOOTH);
			const ColorB& col = colours[colorIndex];
			static Ang3 ang3 = Ang3(0, 0, 0);
			ang3 += Ang3(0.01f, 0.02f, 0.13f);
			AABB aabb = AABB(Vec3(-0.02f, -0.02f, -0.02f), Vec3(+0.02f, +0.02f, +0.02f));
			OBB obb;
			obb.SetOBBfromAABB(Matrix33::CreateRotationXYZ(ang3), aabb);
			g_pAuxGeom->DrawOBB(obb, debugGridLocation * Vec3(polarCoordinatesSmooth), 1, col, eBBD_Faceted);
			if (Console::GetInst().ca_DrawAimIKVEGrid > 1)
			{
				const ColorB targetColor(col.r / 3, col.g / 3, col.b / 3, col.a);
				g_pAuxGeom->DrawOBB(obb, debugGridLocation * Vec3(polarCoordinates), 1, targetColor, eBBD_Faceted);
			}
		}
#endif
	}
	else
	{
		assert(polarCoordinatesIndex < MAX_POLAR_COORDINATES_SMOOTH);
		polarCoordinatesSmooth = m_polarCoordinatesSmooth[polarCoordinatesIndex].value;
	}

	//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fTextColor, false,"polarCoordinates: %f %f",polarCoordinates.x,polarCoordinates.y );,
	//	g_YLine+=16.0f;

	//g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fTextColor, false,"m_PolarCoordinatesSmooth: %f %f",polarCoordinatesSmooth.x,polarCoordinatesSmooth.y );
	//g_YLine+=16.0f;

	uint8 idx, i0, i1, i2, i3;
	const f32 fx = (polarCoordinatesSmooth.x + gf_PI) / (gf_PI / 8);
	const f32 fy = (polarCoordinatesSmooth.y + gf_PI / 2) / (gf_PI / 8);
	int32 ix = int32(fx);
	int32 iy = int32(fy);
	const f32 px = fx - f32(ix);
	const f32 py = fy - f32(iy);

	ix = clamp_tpl(ix, 0, 15);
	iy = clamp_tpl(iy, 0, 7);

	f32 weights[256];
	for (uint32 i = 0; i < numAimPosesCAF; i++)
		weights[i] = 0;

	const CHUNK_GAHAIM_INFO::VirtualExample* pPolarGrid = &rAim.m_PolarGrid[0];
	CHUNK_GAHAIM_INFO::VirtualExample example;

	const f32 ew0 = (1 - px) * (1 - py);
	idx = (iy + 0) * CHUNK_GAHAIM_INFO::XGRID + (ix + 0);
	example = pPolarGrid[idx];
	i0 = example.i0;
	if (i0 == 0xff) return;
	i1 = example.i1;
	if (i1 == 0xff) return;
	i2 = example.i2;
	if (i2 == 0xff) return;
	i3 = example.i3;
	if (i3 == 0xff) return;
	weights[i0] += f32(example.v0) / f32(0x2000) * ew0;
	weights[i1] += f32(example.v1) / f32(0x2000) * ew0;
	weights[i2] += f32(example.v2) / f32(0x2000) * ew0;
	weights[i3] += f32(example.v3) / f32(0x2000) * ew0;

	const f32 ew1 = px * (1 - py);
	idx = (iy + 0) * CHUNK_GAHAIM_INFO::XGRID + (ix + 1);
	example = pPolarGrid[idx];
	i0 = example.i0;
	if (i0 == 0xff) return;
	i1 = example.i1;
	if (i1 == 0xff) return;
	i2 = example.i2;
	if (i2 == 0xff) return;
	i3 = example.i3;
	if (i3 == 0xff) return;
	weights[i0] += f32(example.v0) / f32(0x2000) * ew1;
	weights[i1] += f32(example.v1) / f32(0x2000) * ew1;
	weights[i2] += f32(example.v2) / f32(0x2000) * ew1;
	weights[i3] += f32(example.v3) / f32(0x2000) * ew1;

	const f32 ew3 = (1 - px) * py;
	idx = (iy + 1) * CHUNK_GAHAIM_INFO::XGRID + (ix + 0);
	example = pPolarGrid[idx];
	i0 = example.i0;
	if (i0 == 0xff) return;
	i1 = example.i1;
	if (i1 == 0xff) return;
	i2 = example.i2;
	if (i2 == 0xff) return;
	i3 = example.i3;
	if (i3 == 0xff) return;
	weights[i0] += f32(example.v0) / f32(0x2000) * ew3;
	weights[i1] += f32(example.v1) / f32(0x2000) * ew3;
	weights[i2] += f32(example.v2) / f32(0x2000) * ew3;
	weights[i3] += f32(example.v3) / f32(0x2000) * ew3;

	const f32 ew2 = px * py;
	idx = (iy + 1) * CHUNK_GAHAIM_INFO::XGRID + (ix + 1);
	example = pPolarGrid[idx];
	i0 = example.i0;
	if (i0 == 0xff) return;
	i1 = example.i1;
	if (i1 == 0xff) return;
	i2 = example.i2;
	if (i2 == 0xff) return;
	i3 = example.i3;
	if (i3 == 0xff) return;
	weights[i0] += f32(example.v0) / f32(0x2000) * ew2;
	weights[i1] += f32(example.v1) / f32(0x2000) * ew2;
	weights[i2] += f32(example.v2) / f32(0x2000) * ew2;
	weights[i3] += f32(example.v3) / f32(0x2000) * ew2;

	f32 sum = 0.0f;
	for (uint32 i = 0; i < numAimPosesCAF; i++)
		sum += weights[i];
	assert(fabs(sum - 1.0f) < 0.001);

	for (uint32 i = 0; i < numAimPosesCAF; i++)
	{
		const DynArray<Vec3>& rAimPosistions = rAim.m_arrAimIKPosesAIM[i].m_arrPosition;

		const Quat* arrRotations = &rAim.m_arrAimIKPosesAIM[i].m_arrRotation[0];
		const Vec3* arrPositions = 0;

		if (!rAimPosistions.empty())
			arrPositions = &rAimPosistions[0];

		f32 w = weights[i] * fIKBlend;
		if (w)
		{
			//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fTextColor, false,"Index: %d  weights: %f",i,weights[i] );
			//	g_YLine+=16.0f;
			for (uint32 r = 0; r < numRotJoints; r++)
			{
				const int32 j = rRot[r].m_nJointIdx;
				qtemp = arrRelPose[r];

				//	if (i==0)
				//	{
				//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fTextColor, false,"exist: %d  %f %f %f %f  joint-name: %s",(rAim.m_nExist&(uint64(1)<<r))!=0,pMiddleRot[r].w,pMiddleRot[r].v.x,pMiddleRot[r].v.y,pMiddleRot[r].v.z,  pAimIK_Rot[r].m_strJointName );
				//	g_YLine+=16.0f;
				//	}

				if (rAim.m_nExist & (uint64(1) << r))
				{
					qtemp.q = arrRotations[r];
					if (rRot[r].m_nAdditive)
						qtemp.q = arrRelPose[r].q * (!pMiddleRot[r] * qtemp.q);     //additive blend

					const int32 posIdx = rRot[r].m_nPosIndex;
					if (posIdx > -1 && arrPositions)
					{
						qtemp.t = arrPositions[posIdx];
						if (rPos[posIdx].m_nAdditive)
							qtemp.t = arrRelPose[r].t - (pMiddlePos[posIdx] - qtemp.t);     //additive blend
					}
				}
				jointsRelative[j].q += qtemp.q * w;
				jointsRelative[j].t += qtemp.t * w;
			}
		}
	}

	/*
	   for (uint32 r=0; r<1; r++)
	   {
	   for (uint32 i=0; i<numAimPosesCAF; i++)
	   {
	   const Quat *arrRotations = &rAim.m_arrAimIKPosesAIM[i].m_arrRotation[0];
	   int32 j	= pAimIK_Rot[r].m_nJointIdx;
	   g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fTextColor, false,"exist: %d  Quat(%f %f %f %f)  joint-name: %s",(rAim.m_nExist&(uint64(1)<<r))!=0,arrRotations[r].w,arrRotations[r].v.x,arrRotations[r].v.y,arrRotations[r].v.z,  pAimIK_Rot[r].m_strJointName );
	   g_YLine+=16.0f;
	   }
	   g_YLine+=16.0f;
	   }

	   for (uint32 p=0; p<1; p++)
	   {
	   for (uint32 i=0; i<numAimPosesCAF; i++)
	   {
	   const Vec3 *arrPositions = 0;
	   if (!rAim.m_arrAimIKPosesAIM[i].m_arrPosition.empty())
	   {
	   arrPositions = &rAim.m_arrAimIKPosesAIM[i].m_arrPosition[0];
	   }
	   int32 j	= pAimIK_Pos[p].m_nJointIdx;
	   g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fTextColor, false,"exist: %d  Vec3(%f %f %f)  joint-name: %s",(rAim.m_nExist&(uint64(1)<<p))!=0,arrPositions[p].x,arrPositions[p].y,arrPositions[p].z,  pAimIK_Pos[p].m_strJointName );
	   g_YLine+=16.0f;
	   }
	   g_YLine+=16.0f;
	   } */

}

#ifdef EDITOR_PCDEBUGCODE
	#pragma warning(push)
	#pragma warning(disable : 6262)// debug stuff only (assume enough stack space)
void SDirectionalBlender::DebugVEGrid(const SAnimationPoseModifierParams& params, const SDirInfo& rAimInfo, const f32 fAimPoseWeight, const f32 fIKBlend, const int32 widx, const SJointsAimIK_Rot* rRot, const uint32 numRotJoints, const SJointsAimIK_Pos* rPos, const uint32 numPosJoints)
{
	//VExampleInit ve;
	float fTextColor[4] = { 1, 0, 0, 1 };
	g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fTextColor, false, "fAimPoseWeight: %f", fAimPoseWeight);
	g_YLine += 14.0f;
	if (fAimPoseWeight < 0.01f)
		return;

	CCharInstance* pInstance = PoseModifierHelper::GetCharInstance(params);

	Matrix34 debugGridLocation;
	debugGridLocation.SetRotationX(-gf_PI / 2);
	debugGridLocation.AddTranslation(params.location.t + Vec3(0.f, 0.f, 1.6f));

	for (f32 x = -gf_PI; x <= gf_PI + 0.001f; x += gf_PI / 8)
		g_pAuxGeom->DrawLine(debugGridLocation * Vec3(x, -gf_PI / 2, -0.001f), RGBA8(0x1f, 0x0f, 0x7f, 0xff), debugGridLocation * Vec3(x, gf_PI / 2, -0.001f), RGBA8(0x1f, 0x0f, 0x7f, 0xff));
	for (f32 y = -gf_PI / 2; y <= gf_PI / 2 + 0.001f; y += gf_PI / 8)
		g_pAuxGeom->DrawLine(debugGridLocation * Vec3(-gf_PI, y, -0.001f), RGBA8(0x1f, 0x0f, 0x7f, 0xff), debugGridLocation * Vec3(gf_PI, y, -0.001f), RGBA8(0x1f, 0x0f, 0x7f, 0xff));

	CDefaultSkeleton* pDefaultSkeleton = pInstance->m_pDefaultSkeleton;
	uint32 numJoints = pDefaultSkeleton->m_poseDefaultData.GetJointCount();
	const CDefaultSkeleton::SJoint* parrModelJoints = &pDefaultSkeleton->m_arrModelJoints[0];
	CAnimationSet* pAnimationSet = pInstance->m_pDefaultSkeleton->m_pAnimationSet;

	const GlobalAnimationHeaderAIM& rAIM0 = g_AnimationManager.m_arrGlobalAIM[rAimInfo.m_nGlobalDirID0];

	QuadIndices arrQuat[MAX_JOINT_AMOUNT];
	uint32 numAimPoses = uint32(rAIM0.m_fTotalDuration * ANIMATION_30Hz + 1.1f);
	uint32 numQuats = rAIM0.Debug_AnnotateExamples2(numAimPoses, arrQuat);

	//--------------------------------------------------------------------------------
	//----    debug stuff
	//--------------------------------------------------------------------------------
	static Ang3 ang3 = Ang3(0, 0, 0);
	ang3 += Ang3(0.01f, 0.02f, 0.13f);
	AABB aabb1 = AABB(Vec3(-0.01f, -0.01f, -0.01f), Vec3(+0.01f, +0.01f, +0.01f));
	AABB aabb2 = AABB(Vec3(-0.02f, -0.02f, -0.02f), Vec3(+0.02f, +0.02f, +0.02f));
	OBB obb1;
	obb1.SetOBBfromAABB(Matrix33::CreateRotationXYZ(ang3), aabb1);
	OBB obb2;
	obb2.SetOBBfromAABB(Matrix33::CreateRotationXYZ(ang3), aabb2);

	QuatT arrRelPose0[MAX_JOINT_AMOUNT];
	QuatT arrAbsPose0[MAX_JOINT_AMOUNT];
	cryMemcpy(arrRelPose0, pDefaultSkeleton->m_poseDefaultData.GetJointsRelativeMain(), numJoints * sizeof(QuatT));
	for (uint32 i = 0; i < numJoints; i++)
		arrAbsPose0[i] = pDefaultSkeleton->m_poseDefaultData.GetJointsAbsoluteMain()[i];

	QuatT arrRelPose1[MAX_JOINT_AMOUNT];
	QuatT arrAbsPose1[MAX_JOINT_AMOUNT];
	cryMemcpy(arrRelPose1, pDefaultSkeleton->m_poseDefaultData.GetJointsRelativeMain(), numJoints * sizeof(QuatT));
	for (uint32 i = 0; i < numJoints; i++)
		arrAbsPose1[i] = pDefaultSkeleton->m_poseDefaultData.GetJointsAbsoluteMain()[i];

	QuatT arrRelPose2[MAX_JOINT_AMOUNT];
	QuatT arrAbsPose2[MAX_JOINT_AMOUNT];
	cryMemcpy(arrRelPose2, pDefaultSkeleton->m_poseDefaultData.GetJointsRelativeMain(), numJoints * sizeof(QuatT));
	for (uint32 i = 0; i < numJoints; i++)
		arrAbsPose2[i] = pDefaultSkeleton->m_poseDefaultData.GetJointsAbsoluteMain()[i];

	QuatT arrRelPose3[MAX_JOINT_AMOUNT];
	QuatT arrAbsPose3[MAX_JOINT_AMOUNT];
	cryMemcpy(arrRelPose3, pDefaultSkeleton->m_poseDefaultData.GetJointsRelativeMain(), numJoints * sizeof(QuatT));
	for (uint32 i = 0; i < numJoints; i++)
		arrAbsPose3[i] = pDefaultSkeleton->m_poseDefaultData.GetJointsAbsoluteMain()[i];

	QuatT arrRelPose[MAX_JOINT_AMOUNT];
	QuatT arrAbsPose[MAX_JOINT_AMOUNT];
	cryMemcpy(arrRelPose, pDefaultSkeleton->m_poseDefaultData.GetJointsRelativeMain(), numJoints * sizeof(QuatT));
	for (uint32 i = 0; i < numJoints; i++)
		arrAbsPose[i] = pDefaultSkeleton->m_poseDefaultData.GetJointsAbsoluteMain()[i];

	static Ang3 ang = Ang3(0, 0, 0);
	ang += Ang3(0.1f, 0.2f, 0.03f);
	AABB aabb = AABB(Vec3(-0.01f, -0.01f, -0.01f), Vec3(+0.01f, +0.01f, +0.01f));
	OBB obb;
	obb.SetOBBfromAABB(Matrix33::CreateRotationXYZ(ang), aabb);

	ColorB tricol = RGBA8(0x00, 0xff, 0x00, 0xff);

	//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.2f, fTextColor, false,"rAIM0.m_MiddleAimPose: %f %f %f %f",rAIM0.m_MiddleAimPose.w,  rAIM0.m_MiddleAimPose.v.x,rAIM0.m_MiddleAimPose.v.y,rAIM0.m_MiddleAimPose.v.z );
	//	g_YLine+=14.0f;

	for (uint32 aq = 0; aq < numQuats; aq++)
	{
		int8 i0 = arrQuat[aq].i0;
		int8 i1 = arrQuat[aq].i1;
		int8 i2 = arrQuat[aq].i2;
		int8 i3 = arrQuat[aq].i3;

		rAIM0.Debug_Blend4AimPose(pDefaultSkeleton, rRot, numRotJoints, rPos, numPosJoints, i0, i1, i2, i3, arrQuat[aq].w0, arrRelPose0, arrAbsPose0);
		Quat qt0 = rAIM0.m_MiddleAimPose * arrAbsPose0[widx].q;
		Vec2 polar0 = Vec2(rAIM0.Debug_PolarCoordinate(qt0));
		//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.2f, fTextColor, false,"polar0: %f %f",polar0.x,polar0.y );
		//	g_YLine+=14.0f;

		rAIM0.Debug_Blend4AimPose(pDefaultSkeleton, rRot, numRotJoints, rPos, numPosJoints, i0, i1, i2, i3, arrQuat[aq].w1, arrRelPose1, arrAbsPose1);
		Quat qt1 = rAIM0.m_MiddleAimPose * arrAbsPose1[widx].q;
		Vec2 polar1 = Vec2(rAIM0.Debug_PolarCoordinate(qt1));
		//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.2f, fTextColor, false,"polar1: %f %f",polar1.x,polar1.y );
		//	g_YLine+=14.0f;

		rAIM0.Debug_Blend4AimPose(pDefaultSkeleton, rRot, numRotJoints, rPos, numPosJoints, i0, i1, i2, i3, arrQuat[aq].w2, arrRelPose2, arrAbsPose2);
		Quat qt2 = rAIM0.m_MiddleAimPose * arrAbsPose2[widx].q;
		Vec2 polar2 = Vec2(rAIM0.Debug_PolarCoordinate(qt2));
		//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.2f, fTextColor, false,"polar2: %f %f",polar2.x,polar2.y );
		//	g_YLine+=14.0f;

		rAIM0.Debug_Blend4AimPose(pDefaultSkeleton, rRot, numRotJoints, rPos, numPosJoints, i0, i1, i2, i3, arrQuat[aq].w3, arrRelPose3, arrAbsPose3);
		Quat qt3 = rAIM0.m_MiddleAimPose * arrAbsPose3[widx].q;
		Vec2 polar3 = Vec2(rAIM0.Debug_PolarCoordinate(qt3));
		//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.2f, fTextColor, false,"polar3: %f %f",polar3.x,polar3.y );
		//	g_YLine+=24.0f;

		ColorB col2;
		f32 t = 1.0;
		f32 angle0 = acos_tpl(qt0 | qt1);
		f32 step0 = 1.0f / (angle0 * 30.0f);
		Vec2 p0 = polar0;
		g_pAuxGeom->DrawOBB(obb, debugGridLocation * Vec3(polar0), 1, RGBA8(0x00, 0x1f, 0x00, 0x00), eBBD_Extremes_Color_Encoded);
		g_pAuxGeom->DrawOBB(obb, debugGridLocation * Vec3(polar1), 1, RGBA8(0x00, 0x1f, 0x00, 0x00), eBBD_Extremes_Color_Encoded);
		g_pAuxGeom->DrawOBB(obb, debugGridLocation * Vec3(polar2), 1, RGBA8(0x00, 0x1f, 0x00, 0x00), eBBD_Extremes_Color_Encoded);
		g_pAuxGeom->DrawOBB(obb, debugGridLocation * Vec3(polar3), 1, RGBA8(0x00, 0x1f, 0x00, 0x00), eBBD_Extremes_Color_Encoded);
		for (f32 i = step0; i < 3.0f; i += step0)
		{
			t = i;
			if (i > 0.999) t = 1.0;
			rAIM0.Debug_NLerp2AimPose(pDefaultSkeleton, rRot, numRotJoints, rPos, numPosJoints, arrRelPose0, arrRelPose1, t, arrAbsPose);
			Quat qt = rAIM0.m_MiddleAimPose * arrAbsPose[widx].q;
			Vec2 p1 = Vec2(rAIM0.Debug_PolarCoordinate(qt));
			col2.r = uint8(arrQuat[aq].col.r * fAimPoseWeight);
			col2.g = uint8(arrQuat[aq].col.g * fAimPoseWeight);
			col2.b = uint8(arrQuat[aq].col.b * fAimPoseWeight);
			g_pAuxGeom->DrawLine(debugGridLocation * (p0 + arrQuat[aq].height), col2, debugGridLocation * (p1 + arrQuat[aq].height), col2);
			p0 = p1;
			if (t == 1.0)
				break;
		}

		f32 angle1 = acos_tpl(qt1 | qt2);
		f32 step1 = 1.0f / (angle1 * 30.0f);
		for (f32 i = step1; i < 3.0f; i += step1)
		{
			t = i;
			if (i > 0.999) t = 1.0;
			rAIM0.Debug_NLerp2AimPose(pDefaultSkeleton, rRot, numRotJoints, rPos, numPosJoints, arrRelPose1, arrRelPose2, t, arrAbsPose);
			Quat qt = rAIM0.m_MiddleAimPose * arrAbsPose[widx].q;
			Vec2 p1 = Vec2(rAIM0.Debug_PolarCoordinate(qt));
			col2.r = uint8(arrQuat[aq].col.r * fAimPoseWeight);
			col2.g = uint8(arrQuat[aq].col.g * fAimPoseWeight);
			col2.b = uint8(arrQuat[aq].col.b * fAimPoseWeight);
			g_pAuxGeom->DrawLine(debugGridLocation * (p0 + arrQuat[aq].height), col2, debugGridLocation * (p1 + arrQuat[aq].height), col2);
			p0 = p1;
			if (t == 1.0)
				break;
		}

		f32 angle2 = acos_tpl(qt2 | qt3);
		f32 step2 = 1.0f / (angle2 * 30.0f);
		for (f32 i = step2; i < 3.0f; i += step2)
		{
			t = i;
			if (i > 0.999) t = 1.0;
			rAIM0.Debug_NLerp2AimPose(pDefaultSkeleton, rRot, numRotJoints, rPos, numPosJoints, arrRelPose2, arrRelPose3, t, arrAbsPose);
			Quat qt = rAIM0.m_MiddleAimPose * arrAbsPose[widx].q;
			Vec2 p1 = Vec2(rAIM0.Debug_PolarCoordinate(qt));
			col2.r = uint8(arrQuat[aq].col.r * fAimPoseWeight);
			col2.g = uint8(arrQuat[aq].col.g * fAimPoseWeight);
			col2.b = uint8(arrQuat[aq].col.b * fAimPoseWeight);
			g_pAuxGeom->DrawLine(debugGridLocation * (p0 + arrQuat[aq].height), col2, debugGridLocation * (p1 + arrQuat[aq].height), col2);
			p0 = p1;
			if (t == 1.0)
				break;
		}

		f32 angle3 = acos_tpl(qt3 | qt0);
		f32 step3 = 1.0f / (angle3 * 30.0f);
		for (f32 i = step3; i < 3.0f; i += step3)
		{
			t = i;
			if (i > 0.999) t = 1.0;
			rAIM0.Debug_NLerp2AimPose(pDefaultSkeleton, rRot, numRotJoints, rPos, numPosJoints, arrRelPose3, arrRelPose0, t, arrAbsPose);
			Quat qt = rAIM0.m_MiddleAimPose * arrAbsPose[widx].q;
			Vec2 p1 = Vec2(rAIM0.Debug_PolarCoordinate(qt));
			col2.r = uint8(arrQuat[aq].col.r * fAimPoseWeight);
			col2.g = uint8(arrQuat[aq].col.g * fAimPoseWeight);
			col2.b = uint8(arrQuat[aq].col.b * fAimPoseWeight);
			g_pAuxGeom->DrawLine(debugGridLocation * (p0 + arrQuat[aq].height), col2, debugGridLocation * (p1 + arrQuat[aq].height), col2);
			p0 = p1;
			if (t == 1.0)
				break;
		}
	}

	for (int32 y = 0; y < CHUNK_GAHAIM_INFO::YGRID; y++)
	{
		for (int32 x = 0; x < CHUNK_GAHAIM_INFO::XGRID; x++)
		{
			//	if (rAIM0.m_PolarGrid[y*XGRID+x].m_fSmalest<0.2f)
			{
				int i0 = rAIM0.m_PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].i0;
				int i1 = rAIM0.m_PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].i1;
				int i2 = rAIM0.m_PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].i2;
				int i3 = rAIM0.m_PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].i3;
				Vec4 w;
				w.x = f32(rAIM0.m_PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].v0) / f32(0x2000);
				w.y = f32(rAIM0.m_PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].v1) / f32(0x2000);
				w.z = f32(rAIM0.m_PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].v2) / f32(0x2000);
				w.w = f32(rAIM0.m_PolarGrid[y * CHUNK_GAHAIM_INFO::XGRID + x].v3) / f32(0x2000);
				rAIM0.Debug_Blend4AimPose(pDefaultSkeleton, rRot, numRotJoints, rPos, numPosJoints, i0, i1, i2, i3, w, arrRelPose, arrAbsPose);
				Vec3 vPolarCoord = Vec2(rAIM0.Debug_PolarCoordinate(rAIM0.m_MiddleAimPose * arrAbsPose[widx].q));
				g_pAuxGeom->DrawOBB(obb1, debugGridLocation * vPolarCoord, 1, RGBA8(0xff, 0xff, 0xff, 0x00), eBBD_Extremes_Color_Encoded);
			}
		}
	}

	size_t nHowManyVE = rAIM0.VE2.size();
	for (uint32 i = 0; i < nHowManyVE; i++)
	{
		int32 i0 = rAIM0.VE2[i].i0;
		int32 i1 = rAIM0.VE2[i].i1;
		int32 i2 = rAIM0.VE2[i].i2;
		int32 i3 = rAIM0.VE2[i].i3;
		Vec2 polcor;
		polcor.x = rAIM0.VE2[i].polar.x;
		polcor.y = rAIM0.VE2[i].polar.y;

		Vec4 w;
		w.x = rAIM0.VE2[i].w0;
		w.y = rAIM0.VE2[i].w1;
		w.z = rAIM0.VE2[i].w2;
		w.w = rAIM0.VE2[i].w3;
		rAIM0.Debug_Blend4AimPose(pDefaultSkeleton, rRot, numRotJoints, rPos, numPosJoints, i0, i1, i2, i3, w, arrRelPose, arrAbsPose);
		Vec3 vPolarCoord = Vec2(rAIM0.Debug_PolarCoordinate(rAIM0.m_MiddleAimPose * arrAbsPose[widx].q));

		assert(fabs(polcor.x - polcor.x) < 0.00001);
		assert(fabs(polcor.y - polcor.y) < 0.00001);
		g_pAuxGeom->DrawOBB(obb1, debugGridLocation * vPolarCoord, 1, RGBA8(0x00, 0xff, 0xff, 0x00), eBBD_Extremes_Color_Encoded);
	}
}
	#pragma warning(pop)
#endif

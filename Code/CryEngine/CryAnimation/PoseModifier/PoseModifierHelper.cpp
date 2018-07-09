// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PoseModifierHelper.h"

#pragma warning( push )
#pragma warning( disable : 4244 )

namespace PoseModifierHelper
{

void ComputeJointChildrenAbsolute(const CDefaultSkeleton& defaultSkeleton, Skeleton::CPoseData& poseData, const uint parentIndex)
{
	const CDefaultSkeleton::SJoint* pModelJoints = defaultSkeleton.m_arrModelJoints.begin();

	const uint childIndex = parentIndex + pModelJoints[parentIndex].m_nOffsetChildren;
	const uint childCount = pModelJoints[parentIndex].m_numChildren;

	const QuatT* pRelatives = poseData.GetJointsRelative();
	QuatT* pAbsolutes = poseData.GetJointsAbsolute();

	for (uint i = 0; i < childCount; ++i)
	{
		const uint index = childIndex + i;
		pAbsolutes[index] = pAbsolutes[parentIndex] * pRelatives[index];
		ComputeJointChildrenAbsolute(defaultSkeleton, poseData, index);
	}
}

bool IK_Solver(const CDefaultSkeleton& defaultSkeleton, LimbIKDefinitionHandle limbHandle, const Vec3& vLocalPos, Skeleton::CPoseData& poseData)
{
	QuatT* const __restrict pRelPose = poseData.GetJointsRelative();
	QuatT* const __restrict pAbsPose = poseData.GetJointsAbsolute();

	int32 idxDefinition = defaultSkeleton.GetLimbDefinitionIdx(limbHandle);
	if (idxDefinition < 0)
		return false;

	const IKLimbType& rIKLimbType = defaultSkeleton.m_IKLimbTypes[idxDefinition];
	uint32 numLinks = rIKLimbType.m_arrRootToEndEffector.size();
	pAbsPose[0] = pRelPose[0];
	ANIM_ASSET_ASSERT(pRelPose[0].q.IsUnit());
	for (uint32 i = 1; i < numLinks; i++)
	{
		int32 p = rIKLimbType.m_arrRootToEndEffector[i - 1];
		int32 c = rIKLimbType.m_arrRootToEndEffector[i];
		ANIM_ASSET_ASSERT(pRelPose[c].q.IsUnit());
		ANIM_ASSET_ASSERT(pRelPose[p].q.IsUnit());
		pAbsPose[c] = pAbsPose[p] * pRelPose[c];
		ANIM_ASSET_ASSERT(pRelPose[c].q.IsUnit());
	}

	if (rIKLimbType.m_nSolver == *(uint32*)"2BIK")
		IK_Solver2Bones(vLocalPos, rIKLimbType, poseData);
	if (rIKLimbType.m_nSolver == *(uint32*)"3BIK")
		IK_Solver3Bones(vLocalPos, rIKLimbType, poseData);
	if (rIKLimbType.m_nSolver == *(uint32*)"CCDX")
		IK_SolverCCD(vLocalPos, rIKLimbType, poseData);

	const CDefaultSkeleton::SJoint* __restrict parrModelJoints = &defaultSkeleton.m_arrModelJoints[0];
	uint32 numChilds = rIKLimbType.m_arrLimbChildren.size();
	const int16* pJointIdx = &rIKLimbType.m_arrLimbChildren[0];
	for (uint32 i = 0; i < numChilds; ++i)
	{
		int32 c = pJointIdx[i];
		int32 p = parrModelJoints[c].m_idxParent;
		pAbsPose[c] = pAbsPose[p] * pRelPose[c];
		ANIM_ASSET_ASSERT(pAbsPose[c].q.IsUnit());
	}

#ifdef _DEBUG
	uint32 numJoints = defaultSkeleton.GetJointCount();
	for (uint32 j = 0; j < numJoints; j++)
	{
		ANIM_ASSET_ASSERT(pRelPose[j].q.IsUnit());
		ANIM_ASSET_ASSERT(pAbsPose[j].q.IsUnit());
		ANIM_ASSET_ASSERT(pRelPose[j].IsValid());
		ANIM_ASSET_ASSERT(pAbsPose[j].IsValid());
	}
#endif

	return true;
};

void IK_Solver2Bones(const Vec3& goal, const IKLimbType& rIKLimbType, Skeleton::CPoseData& poseData)
{
	QuatT* const __restrict pRelPose = poseData.GetJointsRelative();
	QuatT* const __restrict pAbsPose = poseData.GetJointsAbsolute();

	int32 b0 = rIKLimbType.m_arrJointChain[0].m_idxJoint;
	assert(b0 > 0);                                                     //BaseRoot
	int32 b1 = rIKLimbType.m_arrJointChain[1].m_idxJoint;
	assert(b1 > 0);                                                     //Ball Joint (and Root of Chain)
	int32 b2 = rIKLimbType.m_arrJointChain[2].m_idxJoint;
	assert(b2 > 0);                                                     //Hinge Joint
	int32 b3 = rIKLimbType.m_arrJointChain[3].m_idxJoint;
	assert(b3 > 0);                                                     //EndEffector

	if (((goal - pAbsPose[b3].t) | (goal - pAbsPose[b3].t)) < 0.0000000001f)
		return; //end-effector and new goal are very close ... IK not necessary

	if (1)
	{
		//optional:
		//if we can't reach the goal, because of the length of the bone-segments, then we can apply a little bit of scaling.
		//This looks way better then having hyper-extension snaps and joint -flips.
		f32 fRootToGoal = (pAbsPose[b1].t - goal).GetLength();
		f32 fMaxLength = (pRelPose[b2].t.GetLength() + pRelPose[b3].t.GetLength()) * 0.99999f;
		f32 fDist = fRootToGoal - fMaxLength;
		if (fDist > 0)
		{
			f32 fLegScale = min(1.25f, fRootToGoal / fMaxLength);
			pRelPose[b2].t *= fLegScale;
			pRelPose[b3].t *= fLegScale;
			uint32 numLinks = rIKLimbType.m_arrRootToEndEffector.size();
			for (uint32 i = 1; i < numLinks; i++)
			{
				int32 p = rIKLimbType.m_arrRootToEndEffector[i - 1];
				int32 c = rIKLimbType.m_arrRootToEndEffector[i];
				pAbsPose[c] = pAbsPose[p] * pRelPose[c];
			}
		}
	}

	//This is the analytical solver for 2Bone-IK.
	//the root-joint must be a "Ball-Joint", and the middle-joint a "Hinge-Joint".
	//We operate only with the bone-segments, because we don't want to rely on any predefined joint rotations, .
	//We also want to avoid all trigonometric-functions, because they do not exist on Power-PCs
	//All operations happen directly in quaternion-space.
	if (1)
	{
		//compute an additive-quaternion to rotate the hinge-joint so that ||Root-Endeffector|| == ||Root-Goal|| (or at least as close as possible)
		Vec3 aseg = pAbsPose[b2].t - pAbsPose[b1].t;
		f32 asegsq = aseg | aseg;
		f32 ialen = isqrt_tpl(asegsq);
		if (ialen > 100.0f) return;  //bone has almost no length ... IK not possible
		Vec3 bseg = pAbsPose[b3].t - pAbsPose[b2].t;
		f32 bsegsq = bseg | bseg;
		f32 iblen = isqrt_tpl(bsegsq);
		if (iblen > 100.0f) return;  //bone has almost no length ... IK not possible

		//This a just simple 2D operation, but we have to execute in 3d.
		Vec3 anorm = aseg * ialen;
		assert(anorm.IsUnit(0.01f));
		Vec3 bnorm = bseg * iblen;
		assert(bnorm.IsUnit(0.01f));
		Vec3 vHingeAxis = bnorm % anorm;
		f32 fDot = vHingeAxis | vHingeAxis;
		if (fDot < 0.00001f)  return; //no stable hinge-axis1 ... IK not possible
		vHingeAxis *= isqrt_tpl(fDot);

		f32 fRootToGoal = (pAbsPose[b1].t - goal).GetLength();
		f32 fOCosise = -anorm | bnorm;                                                                                     //the original cosine between a and b
		f32 fNCosine = clamp_tpl((asegsq + bsegsq - fRootToGoal * fRootToGoal) / (2.0f / (ialen * iblen)), -0.99f, 0.99f); //cosine=(a*a+b*b-c*c)/(2ab)
		f32 fDeltaCos = fNCosine * fOCosise + sqrtf((1.0f - fNCosine * fNCosine) * (1.0f - fOCosise * fOCosise));          //fDeltaCos=fNCosine-fOCosine
		f32 fHalfCos = sqrtf(fabsf((fDeltaCos + 1.0f) * 0.5f));
		f32 fHalfSin = sqrtf(fabsf(1.0f - fHalfCos * fHalfCos)) * fsgnnz(fOCosise - fNCosine);
		Quat qAdditive(fHalfCos, vHingeAxis * fHalfSin);

		pAbsPose[b2].q = qAdditive * pAbsPose[b2].q; //adjust the hinge-joint with a simple additive rotation
		pAbsPose[b2].q.NormalizeSafe();
		pRelPose[b2].q = !pAbsPose[b1].q * pAbsPose[b2].q;
		pAbsPose[b3].t = pAbsPose[b2].q * pRelPose[b3].t + pAbsPose[b2].t; //adjust EndEffector
	}

	if (1)
	{
		//compute an additive-quaternion to rotate the ball-joint toward the goal
		//If ||Root-Endeffector|| == ||Root-Goal|| then this procedure will make sure that Goal==EndEffector
		Vec3 vR2E = pAbsPose[b1].t - pAbsPose[b3].t;
		f32 ilenR2E = isqrt_tpl(vR2E | vR2E);
		if (ilenR2E > 100.0f) return; //no stable solution possible
		Vec3 vR2G = pAbsPose[b1].t - goal;
		f32 ilenR2G = isqrt_tpl(vR2G | vR2G);
		if (ilenR2G > 100.0f) return; //no stable solution possible

		Vec3 v0 = vR2E * ilenR2E;
		assert(v0.IsUnit(0.01f));
		Vec3 v1 = vR2G * ilenR2G;
		assert(v1.IsUnit(0.01f));
		f32 dot = (v0 | v1) + 1.0f;
		if (dot < 0.0001f)
			return; //goal already reached...we're done!
		Vec3 v = v0 % v1;
		f32 d = isqrt_tpl(dot * dot + (v | v));
		pAbsPose[b1].q = Quat(dot * d, v * d) * pAbsPose[b1].q; //adjust the ball-joint
		pRelPose[b1].q = !pAbsPose[b0].q * pAbsPose[b1].q;
	}
}

//Joint-limits for PINGER (all 3 legs) and the SCORCER (all 4 legs)
//#define Hinge1_Close (+0.90f)
//#define Hinge1_Open	 (-0.90f)
//#define Hinge2_Close (+0.90f)
//#define Hinge2_Open	 (-0.98f)

//Joint-limits for STALKER and GRUNT
#define Hinge1_Close (+0.95f)
#define Hinge1_Open  (-0.90f)
#define Hinge2_Close (+0.29f)
#define Hinge2_Open  (-0.98f)

void IK_Solver3Bones(const Vec3& goal, const IKLimbType& rIKLimbType, Skeleton::CPoseData& poseData)
{
	QuatT* const __restrict pRelPose = poseData.GetJointsRelative();
	QuatT* const __restrict pAbsPose = poseData.GetJointsAbsolute();

	int32 b0 = rIKLimbType.m_arrJointChain[0].m_idxJoint;
	ANIM_ASSET_ASSERT(b0 > 0);                                                     //BaseRoot
	int32 b1 = rIKLimbType.m_arrJointChain[1].m_idxJoint;
	ANIM_ASSET_ASSERT(b1 > 0);                                                     //Root of Chain
	int32 b2 = rIKLimbType.m_arrJointChain[2].m_idxJoint;
	ANIM_ASSET_ASSERT(b2 > 0);                                                     //Hinge Joint1
	int32 b3 = rIKLimbType.m_arrJointChain[3].m_idxJoint;
	ANIM_ASSET_ASSERT(b3 > 0);                                                     //Hinge Joint2
	int32 b4 = rIKLimbType.m_arrJointChain[4].m_idxJoint;
	ANIM_ASSET_ASSERT(b4 > 0);                                                     //End-Effector
	if (((goal - pAbsPose[b4].t) | (goal - pAbsPose[b4].t)) < 0.0000001f)
		return; //end-effector and new goal are very close ... IK not necessary

	if (0)
	{
		//optional:
		//if we can't reach the goal, because of the length of the bone-segments, then we can apply a little bit of scaling.
		//This looks way better then having hyper-extension snaps or joint-flips.
		f32 fRootToGoal = (pAbsPose[b1].t - goal).GetLength();
		f32 fMaxLength = (pRelPose[b2].t.GetLength() + pRelPose[b3].t.GetLength() + pRelPose[b4].t.GetLength());
		f32 fDist = fRootToGoal - fMaxLength;
		if (fDist > 0)
		{
			f32 fLegScale = min(1.15f, fRootToGoal / fMaxLength);
			pRelPose[b2].t *= fLegScale;
			pRelPose[b3].t *= fLegScale;
			pRelPose[b4].t *= fLegScale;
			uint32 numLinks = rIKLimbType.m_arrRootToEndEffector.size();
			for (uint32 i = 1; i < numLinks; i++)
			{
				int32 p = rIKLimbType.m_arrRootToEndEffector[i - 1];
				int32 c = rIKLimbType.m_arrRootToEndEffector[i];
				pAbsPose[c] = pAbsPose[p] * pRelPose[c];
			}
		}
	}

	if (1)
	{
		//optional:
		//if we can't reach the goal, because of the length of the bone-segments, then we can apply a little bit of scaling.
		//This looks way better then having hyper-extension snaps or joint-flips.
		f32 fRootToGoal = (pAbsPose[b1].t - goal).GetLength();
		f32 fMaxLength = (pRelPose[b4].t.GetLength());
		f32 fDist = fRootToGoal - fMaxLength;
		if (fDist < 0)
		{
			f32 fLegScale = max(0.55f, fRootToGoal / fMaxLength);
			pRelPose[b4].t *= fLegScale;
			uint32 numLinks = rIKLimbType.m_arrRootToEndEffector.size();
			for (uint32 i = 1; i < numLinks; i++)
			{
				int32 p = rIKLimbType.m_arrRootToEndEffector[i - 1];
				int32 c = rIKLimbType.m_arrRootToEndEffector[i];
				pAbsPose[c] = pAbsPose[p] * pRelPose[c];
			}
		}
	}

	if (1)
	{
		//Find the optimal combination of additive-quaternions to rotate both hinge-joints so that ||Root-Endeffector|| == ||Root-Goal||
		//this solver considers all joint-limits
		Vec3 aseg = pAbsPose[b2].t - pAbsPose[b1].t;
		f32 ialen = isqrt_tpl(aseg | aseg);
		Vec3 bseg = pAbsPose[b3].t - pAbsPose[b2].t;
		f32 iblen = isqrt_tpl(bseg | bseg);
		Vec3 cseg = pAbsPose[b4].t - pAbsPose[b3].t;
		f32 iclen = isqrt_tpl(cseg | cseg);
		if (ialen > 100.0f) return;  //bone has almost no length ... IK not possible
		if (iblen > 100.0f) return;  //bone has almost no length ... IK not possible
		if (iclen > 100.0f) return;  //bone has almost no length ... IK not possible
		Vec3 anorm = aseg * ialen;
		Vec3 bnorm = bseg * iblen;
		Vec3 cnorm = cseg * iclen;

		//find the original radiant between aseg and bseg
		Vec3 vHingeAxis1 = bseg % aseg;
		f32 fDot1 = vHingeAxis1 | vHingeAxis1;
		if (fDot1 < 0.00001f) return; //no stable hinge-axis1 ... IK not possible
		vHingeAxis1 *= isqrt_tpl(fDot1);
		f32 fOCosine1 = -anorm | bnorm;
		f32 fClose1 = max(fOCosine1, Hinge1_Close) - fOCosine1;
		f32 fOpen1 = min(fOCosine1, Hinge1_Open) - fOCosine1;
		f32 fOHalfCos1 = sqrt(max(0.0f, (fOCosine1 + 1.0f) * 0.5f));
		Vec3 vOHalfSin1 = vHingeAxis1 * sqrt(fabsf(1.0f - fOHalfCos1 * fOHalfCos1));

		Vec3 vHingeAxis2 = cseg % bseg;
		f32 fDot2 = vHingeAxis2 | vHingeAxis2;
		if (fDot2 < 0.00001f)   return; //no stable hinge-axis2 ... IK not possible
		vHingeAxis2 *= isqrt_tpl(fDot2);
		f32 fOCosine2 = -bnorm | cnorm;
		f32 fClose2 = max(fOCosine2, Hinge2_Close) - fOCosine2;
		f32 fOpen2 = min(fOCosine2, Hinge2_Open) - fOCosine2;
		f32 fOHalfCos2 = sqrt(max(0.0f, (fOCosine2 + 1.0f) * 0.5f));
		Vec3 vOHalfSin2 = vHingeAxis2 * sqrt(fabsf(1.0f - fOHalfCos2 * fOHalfCos2));

		f32 fRootToGoal2 = sqr(pAbsPose[b1].t - goal);
		f32 fRootToEnd2 = sqr(pAbsPose[b1].t - pAbsPose[b4].t);
		Quat qAdditive1;
		qAdditive1.SetIdentity();
		Quat qAdditive2;
		qAdditive2.SetIdentity();

		//compute the "close configuration"
		f32 fNHalfCos1 = sqrt(fabsf((fOCosine1 + fClose1 + 1.0f) * 0.5f));
		Vec3 vNHalfSin1 = vHingeAxis1 * sqrt(fabsf(1.0f - fNHalfCos1 * fNHalfCos1));
		Quat qAdditive1min(fNHalfCos1 * fOHalfCos1 + (vOHalfSin1 | vNHalfSin1), vNHalfSin1 * fOHalfCos1 - fNHalfCos1 * vOHalfSin1); //fNRadian1-fORadian1
		f32 fNHalfCos2 = sqrt(fabsf((fOCosine2 + fClose2 + 1.0f) * 0.5f));
		Vec3 vNHalfSin2 = vHingeAxis2 * sqrt(fabsf(1.0f - fNHalfCos2 * fNHalfCos2));
		Quat qAdditive2min(fNHalfCos2 * fOHalfCos2 + (vOHalfSin2 | vNHalfSin2), vNHalfSin2 * fOHalfCos2 - fNHalfCos2 * vOHalfSin2); //fNRadian2-fORadian2
		Vec3 vEndEffector1 = qAdditive1min * qAdditive2min * cseg + (qAdditive1min * bseg + pAbsPose[b2].t);
		f32 dmin = sqr(pAbsPose[b1].t - vEndEffector1);
		if (fRootToGoal2 <= dmin)
			qAdditive1 = qAdditive1min, qAdditive2 = qAdditive2min;

		//compute the "open configuration"
		f32 fNHalfCos3 = sqrt(fabsf((fOCosine1 + fOpen1 + 1.0f) * 0.5f));
		Vec3 vNHalfSin3 = vHingeAxis1 * sqrt(fabsf(1.0f - fNHalfCos3 * fNHalfCos3));
		Quat qAdditive1max(fNHalfCos3 * fOHalfCos1 + (vOHalfSin1 | vNHalfSin3), vNHalfSin3 * fOHalfCos1 - fNHalfCos3 * vOHalfSin1); //fNRadian1-fORadian1
		f32 fNHalfCos4 = sqrt(fabsf((fOCosine2 + fOpen2 + 1.0f) * 0.5f));
		Vec3 vNHalfSin4 = vHingeAxis2 * sqrt(fabsf(1.0f - fNHalfCos4 * fNHalfCos4));
		Quat qAdditive2max(fNHalfCos4 * fOHalfCos2 + (vOHalfSin2 | vNHalfSin4), vNHalfSin4 * fOHalfCos2 - fNHalfCos4 * vOHalfSin2); //fNRadian2-fORadian2
		Vec3 vEndEffector2 = qAdditive1max * qAdditive2max * cseg + (qAdditive1max * bseg + pAbsPose[b2].t);
		f32 dmax = sqr(pAbsPose[b1].t - vEndEffector2);
		if (fRootToGoal2 >= dmax)
			qAdditive1 = qAdditive1max, qAdditive2 = qAdditive2max;

		if (fRootToGoal2 > dmin && fRootToGoal2 < dmax)
		{
			//Iteratrive Solver to find the optimal combination of additive-quaternions to rotate both hinge-joints so that ||Root-Endeffector|| == ||Root-Goal||
			f32 smin = -1.0f;       //fully folded
			f32 smid = 0.0f;        //current unchanged FK-pose
			f32 smax = 1.0f;        //fully stretched
			f32 dmid = fRootToEnd2; //the current unchanged FK-pose is our "middle-configuration"
			for (uint32 i = 0; i < 30; i++)
			{
				if (fabsf(dmid - fRootToGoal2) < 0.01f)  //optimization is possible by making the threshold bigger
					break;
				if ((fRootToGoal2 >= dmin) && (fRootToGoal2 <= dmid))
					smax = smid, dmax = dmid, smid = (smin + smax) * 0.5f;
				if ((fRootToGoal2 >= dmid) && (fRootToGoal2 <= dmax))
					smin = smid, dmin = dmid, smid = (smin + smax) * 0.5f;

				f32 fNCosine1 = (smid < 0.0f) ? fOCosine1 - fClose1 * smid : fOCosine1 + fOpen1 * smid;
				fNHalfCos1 = sqrt(fabsf((fNCosine1 + 1.0f) * 0.5f));
				vNHalfSin1 = vHingeAxis1 * sqrt(fabsf(1.0f - fNHalfCos1 * fNHalfCos1));
				qAdditive1 = Quat(fNHalfCos1 * fOHalfCos1 + (vOHalfSin1 | vNHalfSin1), vNHalfSin1 * fOHalfCos1 - fNHalfCos1 * vOHalfSin1);
				f32 fNCosine2 = (smid < 0.0f) ? fOCosine2 - fClose2 * smid : fOCosine2 + fOpen2 * smid;
				fNHalfCos2 = sqrt(fabsf((fNCosine2 + 1.0f) * 0.5f));
				vNHalfSin2 = vHingeAxis2 * sqrt(fabsf(1.0f - fNHalfCos2 * fNHalfCos2));
				qAdditive2 = Quat(fNHalfCos2 * fOHalfCos2 + (vOHalfSin2 | vNHalfSin2), vNHalfSin2 * fOHalfCos2 - fNHalfCos2 * vOHalfSin2);
				Vec3 vEndEffector = qAdditive1 * qAdditive2 * cseg + (qAdditive1 * bseg + pAbsPose[b2].t);
				dmid = sqr(pAbsPose[b1].t - vEndEffector);
			}
		}
		pAbsPose[b2].q = qAdditive1 * pAbsPose[b2].q; //adjust the hinge-joint with a simple additive rotation1
		pAbsPose[b2].q.NormalizeSafe();
		pRelPose[b2].q = !pAbsPose[b1].q * pAbsPose[b2].q;
		pAbsPose[b3] = pAbsPose[b2] * pRelPose[b3];   //adjust EndEffector
		pAbsPose[b3].q = qAdditive2 * pAbsPose[b3].q; //adjust the hinge-joint with a simple additive rotation2
		pAbsPose[b3].q.NormalizeSafe();
		pRelPose[b3].q = !pAbsPose[b2].q * pAbsPose[b3].q;
		pAbsPose[b4] = pAbsPose[b3] * pRelPose[b4]; //adjust EndEffector
	}

	if (0)
	{
		//apply a simple Translation Solver to bring the endeffector exactly to the goal
		Vec3 vR2E = pAbsPose[b4].t - pAbsPose[b1].t;
		f32 lenR2E = vR2E.GetLength();
		if (lenR2E < 0.001f)
			return; //no stable solution possible
		Vec3 vR2G = pAbsPose[b1].t - goal;
		f32 lenR2G = vR2G.GetLength();
		if (lenR2G < 0.001f)
			return; //no stable solution possible
		Vec3 vDistance = (vR2E / lenR2E * min(lenR2G, lenR2E * 1.5f) + pAbsPose[b1].t - pAbsPose[b4].t) / 3.0f;

		Vec3 vAddDistance = vDistance;
		pAbsPose[b2].t += vAddDistance;
		pRelPose[b2] = pAbsPose[b1].GetInverted() * pAbsPose[b2];
		pRelPose[b2].q.NormalizeSafe();
		vAddDistance += vDistance;
		pAbsPose[b3].t += vAddDistance;
		pRelPose[b3] = pAbsPose[b2].GetInverted() * pAbsPose[b3];
		pRelPose[b3].q.NormalizeSafe();
		vAddDistance += vDistance;
		pAbsPose[b4].t += vAddDistance;
		pRelPose[b4] = pAbsPose[b3].GetInverted() * pAbsPose[b4];
		pRelPose[b4].q.NormalizeSafe();
	}

	if (1)
	{
		//compute an additive-quaternion to rotate the ball-joint toward the goal
		//If ||Root-Endeffector|| == ||Root-Goal|| then this procedure will make sure that Goal==EndEffector
		Vec3 vR2E = pAbsPose[b1].t - pAbsPose[b4].t;
		f32 ilenR2E = isqrt_tpl(vR2E | vR2E);
		if (ilenR2E > 100.0f) return; //no stable solution possible
		Vec3 vR2G = pAbsPose[b1].t - goal;
		f32 ilenR2G = isqrt_tpl(vR2G | vR2G);
		if (ilenR2G > 100.0f) return; //no stable solution possible
		Vec3 v0 = vR2E * ilenR2E;
		assert(v0.IsUnit(0.01f));
		Vec3 v1 = vR2G * ilenR2G;
		assert(v1.IsUnit(0.01f));
		f32 dot = (v0 | v1) + 1.0f;
		if (dot < 0.0001f)
			return; //goal already reached...we're done!
		Vec3 v = v0 % v1;
		f32 d = isqrt_tpl(dot * dot + (v | v));
		pAbsPose[b1].q = Quat(dot * d, v * d) * pAbsPose[b1].q; //adjust the ball-joint
		pRelPose[b1].q = !pAbsPose[b0].q * pAbsPose[b1].q;
	}
}

void IK_SolverCCD(const Vec3& vTarget, const IKLimbType& rIKLimbType, Skeleton::CPoseData& poseData)
{
	QuatT* const __restrict pRelPose = poseData.GetJointsRelative();
	QuatT* const __restrict pAbsPose = poseData.GetJointsAbsolute();

	f32 fTransCompensation = 0;
	uint32 numLinks = rIKLimbType.m_arrJointChain.size();
	for (uint32 i = 2; i < numLinks; i++)
	{
		int32 p = rIKLimbType.m_arrJointChain[i - 1].m_idxJoint;
		int32 c = rIKLimbType.m_arrJointChain[i].m_idxJoint;
		fTransCompensation += (pAbsPose[c].t - pAbsPose[p].t).GetLengthFast();
	}

	f32 inumLinks = 1.0f / f32(numLinks);
	int32 nRootIdx = rIKLimbType.m_arrJointChain[1].m_idxJoint;              //Root
	int32 nEndEffIdx = rIKLimbType.m_arrJointChain[numLinks - 1].m_idxJoint; //EndEffector
	ANIM_ASSET_ASSERT(nRootIdx < nEndEffIdx);
	int32 iJointIterator = 1;   //numLinks-2;

	// Cyclic Coordinate Descent
	for (uint32 i = 0; i < rIKLimbType.m_nInterations; i++)
	{
		Vec3 vecEnd = pAbsPose[nEndEffIdx].t;             // Position of end effector
		int32 parentLinkIdx = rIKLimbType.m_arrJointChain[iJointIterator - 1].m_idxJoint;
		ANIM_ASSET_ASSERT(parentLinkIdx >= 0);
		int32 LinkIdx = rIKLimbType.m_arrJointChain[iJointIterator].m_idxJoint;
		Vec3 vecLink = pAbsPose[LinkIdx].t;                       // Position of current node
		Vec3 vecLinkTarget = (vTarget - vecLink).GetNormalized(); // Vector current link -> target
		Vec3 vecLinkEnd = (vecEnd - vecLink).GetNormalized();     // Vector current link -> current effector position

		Quat qrel;
		qrel.SetRotationV0V1(vecLinkEnd, vecLinkTarget);
		ANIM_ASSET_ASSERT((fabs_tpl(1 - (qrel | qrel))) < 0.001);   //check if unit-quaternion
		if (qrel.w < 0) qrel.w = -qrel.w;

		f32 ji = iJointIterator * inumLinks + 0.3f;
		f32 t = min(rIKLimbType.m_fStepSize * ji, 0.4f);
		qrel.w *= t + 1.0f - t;
		qrel.v *= t;
		qrel.NormalizeSafe();

		//calculate new relative IK-orientation
		pRelPose[LinkIdx].q = !pAbsPose[parentLinkIdx].q * qrel * pAbsPose[LinkIdx].q;
		pRelPose[LinkIdx].q.NormalizeSafe();

		//calculate new absolute IK-orientation
		for (uint32 j = iJointIterator; j < numLinks; j++)
		{
			int32 c = rIKLimbType.m_arrJointChain[j].m_idxJoint;
			int32 p = rIKLimbType.m_arrJointChain[j - 1].m_idxJoint;
			assert(p >= 0);
			ANIM_ASSET_ASSERT(pRelPose[c].q.IsUnit());
			ANIM_ASSET_ASSERT(pAbsPose[p].q.IsUnit());
			pAbsPose[c] = pAbsPose[p] * pRelPose[c];
			ANIM_ASSET_ASSERT(pAbsPose[c].q.IsUnit());
		}

		f32 fError = (pAbsPose[nEndEffIdx].t - vTarget).GetLength();
		/*
		   static Ang3 angle=Ang3(ZERO);
		   angle.x += 0.1f;
		   angle.y += 0.01f;
		   angle.z += 0.001f;
		   AABB sAABB = AABB(Vec3(-0.1f,-0.1f,-0.1f),Vec3(+0.1f,+0.1f,+0.1f));
		   OBB obb =	OBB::CreateOBBfromAABB( Matrix33::CreateRotationXYZ(angle), sAABB );
		   g_pAuxGeom->DrawOBB(obb,pAbsPose[nEndEffIdx].t,1,RGBA8(0xff,0x00,0x00,0xff),eBBD_Extremes_Color_Encoded);
		 */

		//	float fColor[4] = {0,1,0,1};
		//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.2f, fColor, false,"nUpdateSteps: %d   fError: %f  iJointIterator: %d  ji: %f  t: %f",i,fError,iJointIterator,ji,t );
		//	g_YLine+=12.0f;

		if (fError < rIKLimbType.m_fThreshold) break;
		iJointIterator++;                         //Next link
		if (iJointIterator > (int)(numLinks) - 3) // is it correct, uint - 3 can cause serious problems
			iJointIterator = 1;
	}

	//	float fColor[4] = {0,1,0,1};
	//	f32	fError1	= (pAbsPose[nEndEffIdx].t-vTarget).GetLength();
	//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fColor, false,"fError1: %f",fError1);
	//	g_YLine+=12.0f;

	//-----------------------------------------------------------------------------
	//--do a cheap translation compensation to fix the error
	//-----------------------------------------------------------------------------
	Vec3 absEndEffector = pAbsPose[nEndEffIdx].t; // Position of end effector
	Vec3 vDistance = (vTarget - absEndEffector);  //f32(numCCDJoints);
	f32 fDistance = vDistance.GetLengthFast();
	if (fDistance > fTransCompensation)
		vDistance *= fTransCompensation / fDistance;
	Vec3 bPartDistance = vDistance / f32(numLinks - 0);
	Vec3 vAddDistance = bPartDistance;

	for (uint32 i = 1; i < numLinks; i++)
	{
		int c = rIKLimbType.m_arrJointChain[i].m_idxJoint;
		int p = rIKLimbType.m_arrJointChain[i - 1].m_idxJoint;
		assert(p >= 0);
		pAbsPose[c].t += vAddDistance;
		vAddDistance += bPartDistance;
		ANIM_ASSET_ASSERT(pAbsPose[c].q.IsUnit());
		ANIM_ASSET_ASSERT(pAbsPose[p].q.IsUnit());
		pRelPose[c] = pAbsPose[p].GetInverted() * pAbsPose[c];
		pRelPose[c].q.NormalizeSafe();
	}
	//	f32	fError2	= (pAbsPose[nEndEffIdx].t-vTarget).GetLength();
	//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.4f, fColor, false,"fError2: %f",fError2);
	//	g_YLine+=12.0f;
}

} // namespace PoseModifierHelper

#pragma warning( pop )

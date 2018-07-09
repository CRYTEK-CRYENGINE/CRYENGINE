// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../CharacterInstance.h"

namespace PoseModifierHelper
{

ILINE ISkeletonPose* GetISkeletonPose(const SAnimationPoseModifierParams& params)
{
	return static_cast<CCharInstance*>(params.pCharacterInstance)->CCharInstance::GetISkeletonPose();
}

// NOTE: Get() helpers that don't return an interface should never be used!
// Only here for backward-compatibility.

ILINE CCharInstance* GetCharInstance(const SAnimationPoseModifierParams& params)
{
	return static_cast<CCharInstance*>(params.pCharacterInstance);
}

ILINE CSkeletonPose* GetSkeletonPose(const SAnimationPoseModifierParams& params)
{
	return &(static_cast<CCharInstance*>(params.pCharacterInstance)->m_SkeletonPose);
}

ILINE CSkeletonAnim* GetSkeletonAnim(const SAnimationPoseModifierParams& params)
{
	return &(static_cast<CCharInstance*>(params.pCharacterInstance)->m_SkeletonAnim);
}

ILINE const CDefaultSkeleton& GetDefaultSkeleton(const SAnimationPoseModifierParams& params)
{
	const CDefaultSkeleton& defaultSkeleton = *static_cast<CCharInstance*>(params.pCharacterInstance)->m_pDefaultSkeleton;
	return defaultSkeleton;
}

ILINE const QuatT& GetJointParentAbsoluteSafe(const CDefaultSkeleton& defaultSkeleton, Skeleton::CPoseData& poseData, const uint index)
{
	static const QuatT identity(IDENTITY);
	const int parentIndex = defaultSkeleton.GetJointParentIDByID(index);
	return parentIndex > -1 ? poseData.GetJointAbsolute(parentIndex) : identity;
}

void       ComputeJointChildrenAbsolute(const CDefaultSkeleton& defaultSkeleton, Skeleton::CPoseData& poseData, const uint parentIndex);

ILINE void SetJointRelativeLocation(const CDefaultSkeleton& defaultSkeleton, Skeleton::CPoseData& poseData, const uint index, const QuatT& location)
{
	const CDefaultSkeleton::SJoint* pModelJoints = defaultSkeleton.m_arrModelJoints.begin();

	const int parentIndex = pModelJoints[index].m_idxParent;

	QuatT* pRelatives = poseData.GetJointsRelative();
	QuatT* pAbsolutes = poseData.GetJointsAbsolute();

	pRelatives[index] = location;
	pAbsolutes[index] = parentIndex > -1 ?
	                    (pAbsolutes[parentIndex] * location) : location;

	ComputeJointChildrenAbsolute(defaultSkeleton, poseData, index);
}

ILINE void SetJointRelativePosition(const CDefaultSkeleton& defaultSkeleton, Skeleton::CPoseData& poseData, const uint index, const Vec3& position)
{
	const QuatT* pRelatives = poseData.GetJointsRelative();
	SetJointRelativeLocation(defaultSkeleton, poseData, index, QuatT(pRelatives[index].q, position));
}

ILINE void SetJointRelativeOrientation(const CDefaultSkeleton& defaultSkeleton, Skeleton::CPoseData& poseData, const uint index, const Quat& orientation)
{
	const QuatT* pRelatives = poseData.GetJointsRelative();
	SetJointRelativeLocation(defaultSkeleton, poseData, index, QuatT(orientation, pRelatives[index].t));
}

ILINE void SetJointAbsoluteLocation(const CDefaultSkeleton& defaultSkeleton, Skeleton::CPoseData& poseData, const uint index, const QuatT& location)
{
	const CDefaultSkeleton::SJoint* pModelJoints = defaultSkeleton.m_arrModelJoints.begin();

	const int parentIndex = pModelJoints[index].m_idxParent;

	QuatT* pRelatives = poseData.GetJointsRelative();
	QuatT* pAbsolutes = poseData.GetJointsAbsolute();

	pRelatives[index] = parentIndex > -1 ?
	                    (pAbsolutes[parentIndex].GetInverted() * location) : location;
	pAbsolutes[index] = location;

	ComputeJointChildrenAbsolute(defaultSkeleton, poseData, index);
}

ILINE void SetJointAbsolutePosition(const CDefaultSkeleton& defaultSkeleton, Skeleton::CPoseData& poseData, const uint index, const Vec3& position)
{
	const QuatT* pAbsolutes = poseData.GetJointsAbsolute();
	SetJointAbsoluteLocation(defaultSkeleton, poseData, index, QuatT(pAbsolutes[index].q, position));
}

ILINE void SetJointAbsoluteOrientation(const CDefaultSkeleton& defaultSkeleton, Skeleton::CPoseData& poseData, const uint index, const Quat& orientation)
{
	const QuatT* pAbsolutes = poseData.GetJointsAbsolute();
	SetJointAbsoluteLocation(defaultSkeleton, poseData, index, QuatT(orientation, pAbsolutes[index].t));
}

//

ILINE void SetJointAbsolute(const SAnimationPoseModifierParams& params, uint32 index, const Quat* pOrientation, const Vec3* pPosition)
{
	const CDefaultSkeleton& defaultSkeleton = PoseModifierHelper::GetDefaultSkeleton(params);

	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return;

	QuatT* const __restrict pPoseRelative = pPoseData->GetJointsRelative();
	QuatT* const __restrict pPoseAbsolute = pPoseData->GetJointsAbsolute();

	QuatT& pose = pPoseAbsolute[index];
	if (pOrientation)
		pose.q = *pOrientation;
	if (pPosition)
		pose.t = *pPosition;

	int32 parentIndex = params.GetIDefaultSkeleton().GetJointParentIDByID(index);
	if (parentIndex > -1)
	{
		pPoseRelative[index].t = (pose.t - pPoseAbsolute[parentIndex].t) * pPoseAbsolute[parentIndex].q;
		pPoseRelative[index].q = pPoseAbsolute[parentIndex].q.GetInverted() * pose.q;
	}

	ComputeJointChildrenAbsolute(defaultSkeleton, *pPoseData, index);
}

// Given two vectors formed by
// v0 = origin - from
// v1 = origin - to
// computes the shortest rotation to bring v0 to v1
ILINE bool ComputeRotationForDirectionFromTo(
  const Vec3& origin,
  const Vec3& from,
  const Vec3& to,
  Quat& rotation)
{
	rotation = IDENTITY;

	Vec3 fromDirection = origin - from;
	float fromLengthInverse = isqrt_tpl(fromDirection.GetLengthSquared());
	if (fromLengthInverse > (1.0f / 0.0001f))
		return false;

	Vec3 toDirection = origin - to;
	float toLengthInverse = isqrt_tpl(toDirection.GetLengthSquared());
	if (toLengthInverse > (1.0f / 0.0001f))
		return false;

	fromDirection *= fromLengthInverse;
	toDirection *= toLengthInverse;

	rotation = Quat::CreateRotationV0V1(fromDirection, toDirection);
	return true;
}

// Given two distances formed by
// d0 = distance(origin, from)
// d1 = distance(origin, to);
// computes the shortest rotation around the given pivot to rotate the "from"
// point so that its distance to the origin equals the one from the "to" point.
ILINE bool ComputeRotationForDistanceFromTo(
  const Vec3& origin,
  const Vec3& pivot,
  const Vec3& from,
  const Vec3& to,
  Quat& rotation)
{
	rotation = IDENTITY;

	Vec3 segment0 = origin - pivot;
	float segment0Length = segment0.GetLength();

	Vec3 segment1 = from - pivot;
	float segment1Length = segment1.GetLength();

	// NODE: This should be resolved by comparing the relative length between the
	// two segments and making sure they are within precision tolerance.
	if (segment0Length < 0.0001f || segment1Length < 0.0001f)
		return false;

	Vec3 segment0Direction = segment0.GetNormalized();
	Vec3 segment1Direction = segment1.GetNormalized();

	Vec3 hingeVector = segment0Direction.Cross(segment1Direction);
	float hingeLength2 = hingeVector.GetLengthSquared();
	if (hingeLength2 < 0.00001f)
		return false;
	hingeVector *= isqrt_tpl(hingeLength2);

	float toLength = (origin - to).GetLength();

	float cosOriginal = segment0Direction.Dot(segment1Direction);

	// From the law of cosines:
	// cos = (a*a + b*b + c*c) / (2ab)
	float cosTo =
	  (segment0Length * segment0Length +
	   segment1Length * segment1Length -
	   toLength * toLength) /
	  (segment0Length * segment1Length * 2.0f);
	cosTo = clamp_tpl(cosTo, -0.99f, 0.99f);

	//	cosDelta = cos(acos(cosTo) - acos(cosOriginal))
	float cosDelta = cosOriginal * cosTo +
	                 sqrtf((1.0f - cosOriginal * cosOriginal) * (1.0f - cosTo * cosTo));

	float c = sqrtf(fabsf((cosDelta + 1.0f) * 0.5f));
	float s = sqrtf(fabsf(1.0f - c * c));
	if ((cosOriginal - cosTo) < 0.0f)
		s = -s;

	rotation = Quat(c, hingeVector * s);
	return true;
}

// Given a chain formed by 3 points (p0, p1, p2) and a target, computes two
// shortest rotations to bring p2 to the given target by preserving the
// distance of the given points.
// rotationToDistance has to be applied to p2 using p1 as pivot
// rotationToDirection has to be applied to both p1 and p2 using p0 as pivot
ILINE bool ComputeRotationsForIk2Segments(
  const Vec3& p0,
  const Vec3& p1,
  const Vec3& p2,
  const Vec3& target,
  Quat& rotationToDistance,
  Quat& rotationToDirection)
{
	if (!ComputeRotationForDistanceFromTo(p0, p1, p2, target, rotationToDistance))
		return false;

	rotationToDistance.GetNormalizedSafe();

	Vec3 np2 = rotationToDistance * (p2 - p1) + p1;
	if (!ComputeRotationForDirectionFromTo(p0, np2, target, rotationToDirection))
		return false;

	return true;
}

bool IK_Solver(const CDefaultSkeleton& pDefaultSkeleton, LimbIKDefinitionHandle limbHandle, const Vec3& vLocalPos, Skeleton::CPoseData& poseData);
void IK_Solver2Bones(const Vec3& goal, const IKLimbType& rIKLimbType, Skeleton::CPoseData& poseData);
void IK_Solver3Bones(const Vec3& goal, const IKLimbType& rIKLimbType, Skeleton::CPoseData& poseData);
void IK_SolverCCD(const Vec3& goal, const IKLimbType& rIKLimbType, Skeleton::CPoseData& poseData);

} // namespace PoseModifierHelper

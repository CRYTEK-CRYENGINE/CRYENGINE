// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SkeletonPoser.h"

CSkeletonPoseModifier::SJoint::SJoint()
	: m_crc(0)
	, m_transform(IDENTITY)
	, m_id(-1)
{}

CSkeletonPoseModifier::SJoint::SJoint(const string& name, const QuatT& transform)
	: m_name(name)
	, m_transform(transform)
	, m_id(-1)
{
	m_crc = m_name.length() ? CCrc32::ComputeLowercase(m_name.c_str()) : 0;
}

void CSkeletonPoseModifier::PoseJoint(const string& name, const QuatT& transform)
{
	auto it = std::find_if(m_posedJoints.begin(), m_posedJoints.end(), [name](const SJoint& joint)
	{
		return joint.m_name == name;
	});
	if (it != m_posedJoints.end())
	{
		it->m_transform = transform;
	}
	else
	{
		m_posedJoints.emplace_back(name, transform);
	}
}

bool CSkeletonPoseModifier::Prepare(const SAnimationPoseModifierParams& params)
{
	const IDefaultSkeleton& skeleton = params.GetIDefaultSkeleton();

	for (auto& joint : m_posedJoints)
	{
		if (joint.m_id == -1)
		{
			joint.m_id = skeleton.GetJointIDByCRC32(joint.m_crc);
		}
	}

	return true;
}

void ComputeAbsolutJointTransformationsRecursive(int joint, const IDefaultSkeleton& skel, IAnimationPoseData* pPoseData)
{
	const int parent = skel.GetJointParentIDByID(joint);
	if (parent > -1)
	{
		pPoseData->SetJointAbsolute(joint, pPoseData->GetJointAbsolute(parent) * pPoseData->GetJointRelative(joint));
	}

	for (int i = 0, N = skel.GetJointChildrenCountByID(joint); i < N; ++i)
	{
		ComputeAbsolutJointTransformationsRecursive(skel.GetJointChildIDAtIndexByID(joint, i), skel, pPoseData);
	}
}

void ComputeAbsolutJointTransformations(const std::vector<int>& dirtyRoots, const IDefaultSkeleton& skel, IAnimationPoseData* pPoseData)
{
	for (int dirtyRoot : dirtyRoots)
	{
		ComputeAbsolutJointTransformationsRecursive(dirtyRoot, skel, pPoseData);
	}
}

bool CSkeletonPoseModifier::Execute(const SAnimationPoseModifierParams& params)
{
	IAnimationPoseData* const pPoseData = params.pPoseData;

	const IDefaultSkeleton& skel = params.GetIDefaultSkeleton();

	const int jointCount = skel.GetJointCount();

	std::vector<SJoint> validPosedJoints;
	std::copy_if(m_posedJoints.begin(), m_posedJoints.end(), std::back_inserter(validPosedJoints), [jointCount](const SJoint& joint)
	{
		return joint.m_id > -1 && joint.m_id < jointCount;
	});

	std::vector<bool> dirty(jointCount, false);
	for (auto& joint : validPosedJoints)
	{
		const QuatT rel = pPoseData->GetJointRelative(joint.m_id);
		pPoseData->SetJointRelative(joint.m_id, rel * joint.m_transform);
		dirty[joint.m_id] = true;
	}

	std::vector<bool> visited(jointCount, false);
	std::vector<int> dirtyRoots;
	for (auto& joint : validPosedJoints)
	{
		if (visited[joint.m_id])
		{
			continue;
		}

		visited[joint.m_id] = true;

		int dirtyRoot = joint.m_id;
		int pathIt = skel.GetJointParentIDByID(joint.m_id);
		while (pathIt > -1 && !visited[pathIt])
		{
			if (dirty[pathIt])
			{
				dirtyRoot = pathIt;
			}
			visited[pathIt] = true;
			pathIt = skel.GetJointParentIDByID(pathIt);
		}
		if (pathIt == -1)
		{
			dirtyRoots.push_back(dirtyRoot);
		}
	}

	ComputeAbsolutJointTransformations(dirtyRoots, skel, pPoseData);

	return true;
}

CRYREGISTER_CLASS(CSkeletonPoseModifier)


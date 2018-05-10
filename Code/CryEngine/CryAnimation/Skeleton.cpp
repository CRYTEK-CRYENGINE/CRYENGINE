// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Skeleton.h"

#include "Model.h"
#include "Memory/AlignedAllocator.h"
#include "Memory/PoolAllocator.h"

namespace Skeleton
{

CPoseData::CPoseData()
	: m_jointsBuffer(Memory::AlignedAllocator<16>::Create())
	, m_scalingBuffer(Memory::AlignedAllocator<16>::Create())
	, m_jointCount(0)
{}

void CPoseData::SetMemoryPool(Memory::CPool* pMemoryPool)
{
	assert(pMemoryPool);

	JointsBuffer jointBufferTemp(m_jointsBuffer, Memory::PoolAllocator::Create(*pMemoryPool));
	m_jointsBuffer.swap(jointBufferTemp);

	ScalingBuffer scalingBufferTemp(m_scalingBuffer, Memory::PoolAllocator::Create(*pMemoryPool));
	m_scalingBuffer.swap(scalingBufferTemp);

	if (!m_jointsBuffer)
	{
		m_jointCount = 0;
	}
}

void CPoseData::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(&m_jointsBuffer);
}

uint32 CPoseData::GetAllocationLength() const
{
	if (ICrySizer* pSizer = gEnv->pSystem->CreateSizer())
	{
		pSizer->AddObject(this);
		pSizer->End();

		const auto totalSize = pSizer->GetTotalSize();

		pSizer->Release();

		return totalSize;
	}
	else
	{
		return 0;
	}
}

bool CPoseData::Initialize(uint32 jointCount)
{
	m_jointCount = jointCount;

	m_jointsBuffer.unlock();
	m_jointsBuffer.resize<0>(jointCount, 16);
	m_jointsBuffer.resize<1>(jointCount, 16);
	m_jointsBuffer.resize<2>(jointCount, 16);
	m_jointsBuffer.lock();

	m_scalingBuffer.unlock();
	m_scalingBuffer.resize<0>(0);
	m_scalingBuffer.resize<1>(0);
	m_scalingBuffer.lock();

	if (!m_jointsBuffer)
	{
		m_jointCount = 0;
		return false;
	}

	return true;
}

bool CPoseData::Initialize(const CDefaultSkeleton& skeleton)
{
	return Initialize(skeleton.m_poseDefaultData);
}

bool CPoseData::Initialize(const CPoseData& poseData)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Character Pose Data Cloned");

	m_jointCount = poseData.GetJointCount();
	m_jointsBuffer = poseData.m_jointsBuffer;
	m_scalingBuffer = poseData.m_scalingBuffer;

	if (!m_jointsBuffer)
	{
		m_jointCount = 0;
		return false;
	}

	return true;
}

bool CPoseData::AreScalingBuffersAllocated() const
{
	if (!m_scalingBuffer.empty() && m_scalingBuffer)
	{
		assert(m_scalingBuffer.size<0>() >= m_jointCount);
		assert(m_scalingBuffer.size<1>() >= m_jointCount);
		return true;
	}
	else
	{
		return false;
	}
}

bool CPoseData::IsScalingEnabled() const
{
	return (Console::GetInst().ca_UseScaling != 0) && AreScalingBuffersAllocated();
}

bool CPoseData::EnableScaling()
{
	if (Console::GetInst().ca_UseScaling == 0)
		return false;

	if (AreScalingBuffersAllocated())
		return true;

	m_scalingBuffer.unlock();
	m_scalingBuffer.resize<0>(m_jointCount);
	m_scalingBuffer.resize<1>(m_jointCount);
	m_scalingBuffer.lock();
	if (m_scalingBuffer)
	{
		std::fill(m_scalingBuffer.get<0>(), m_scalingBuffer.get<0>() + m_jointCount, 1.0f);
		std::fill(m_scalingBuffer.get<1>(), m_scalingBuffer.get<1>() + m_jointCount, 1.0f);
		return true;
	}

	return false;
}

uint32 CPoseData::GetJointCount() const
{
	return m_jointCount;
}

const QuatT& CPoseData::GetJointRelative(const uint32 index) const
{
	assert(index < m_jointCount);
	return GetJointsRelative()[index];
}

const Vec3& CPoseData::GetJointRelativeP(const uint32 index) const
{
	assert(index < m_jointCount);
	return GetJointsRelative()[index].t;
}

const Quat& CPoseData::GetJointRelativeO(const uint32 index) const
{
	assert(index < m_jointCount);
	return GetJointsRelative()[index].q;
}

const Diag33 CPoseData::GetJointRelativeS(const uint32 index) const
{
	assert(index < m_jointCount);

	const float* const relativeScaling = GetScalingRelative();
	return Diag33(relativeScaling ? relativeScaling[index] : 1.0f);
}

void CPoseData::SetJointRelative(const uint32 index, const QuatT& transformation)
{
	assert(index < m_jointCount);
	GetJointsRelative()[index] = transformation;
}

void CPoseData::SetJointRelativeP(const uint32 index, const Vec3& position)
{
	assert(index < m_jointCount);
	GetJointsRelative()[index].t = position;
}

void CPoseData::SetJointRelativeO(const uint32 index, const Quat& orientation)
{
	assert(index < m_jointCount);
	GetJointsRelative()[index].q = orientation;
}

void CPoseData::SetJointRelativeS(const uint32 index, const float scaling)
{
	assert(index < m_jointCount);

	if (EnableScaling())
	{
		GetScalingRelative()[index] = scaling;
	}
}

const QuatT& CPoseData::GetJointAbsolute(const uint32 index) const
{
	assert(index < m_jointCount);
	return GetJointsAbsolute()[index];
}

const Vec3& CPoseData::GetJointAbsoluteP(const uint32 index) const
{
	assert(index < m_jointCount);
	return GetJointsAbsolute()[index].t;
}

const Quat& CPoseData::GetJointAbsoluteO(const uint32 index) const
{
	assert(index < m_jointCount);
	return GetJointsAbsolute()[index].q;
}

const Diag33 CPoseData::GetJointAbsoluteS(const uint32 index) const
{
	assert(index < m_jointCount);

	const float* const absoluteScaling = GetScalingAbsolute();
	return Diag33(absoluteScaling ? absoluteScaling[index] : 1.0f);
}

void CPoseData::SetJointAbsolute(const uint32 index, const QuatT& transformation)
{
	assert(index < m_jointCount);
	GetJointsAbsolute()[index] = transformation;
}

void CPoseData::SetJointAbsoluteP(const uint32 index, const Vec3& position)
{
	assert(index < m_jointCount);
	GetJointsAbsolute()[index].t = position;
}

void CPoseData::SetJointAbsoluteO(const uint32 index, const Quat& orientation)
{
	assert(index < m_jointCount);
	GetJointsAbsolute()[index].q = orientation;
}

void CPoseData::SetJointAbsoluteS(const uint32 index, const float scaling)
{
	assert(index < m_jointCount);

	if (EnableScaling())
	{
		GetScalingAbsolute()[index] = scaling;
	}
}

const QuatTS CPoseData::GetJointAbsoluteOPS(const uint32 index) const
{
	assert(index < m_jointCount);

	const QuatT& qt = GetJointsAbsolute()[index];
	const float& s = GetScalingAbsolute() ? GetScalingAbsolute()[index] : 1.0f;

	return QuatTS(qt.q, qt.t, s);
}

const QuatTS CPoseData::GetJointRelativeOPS(const uint32 index) const
{
	assert(index < m_jointCount);

	const QuatT& qt = GetJointsRelative()[index];
	const float& s = GetScalingRelative() ? GetScalingRelative()[index] : 1.0f;

	return QuatTS(qt.q, qt.t, s);
}

void CPoseData::ComputeAbsolutePose(const CDefaultSkeleton& rDefaultSkeleton, bool singleRoot)
{
	const QuatT* const __restrict pRelativePose = GetJointsRelative();
	QuatT* const __restrict pAbsolutePose = GetJointsAbsolute();

	const CDefaultSkeleton::SJoint* const pJoints = &rDefaultSkeleton.m_arrModelJoints[0];

	assert(pAbsolutePose);
	assert(pRelativePose);
	assert(!singleRoot || (pJoints[0].m_idxParent < 0)); // singleRoot => (pJoints[0].m_idxParent < 0)

	if (IsScalingEnabled())
	{
		const float* const __restrict pRelativeScaling = GetScalingRelative();
		float* const __restrict pAbsoluteScaling = GetScalingAbsolute();

		assert(pRelativeScaling);
		assert(pAbsoluteScaling);

		CryPrefetch(&pJoints[0].m_idxParent);
		for (uint32 i = 0; i < m_jointCount; ++i)
		{
			CryPrefetch(&pJoints[i + 1].m_idxParent);
			CryPrefetch(&pAbsolutePose[i + 4]);
			CryPrefetch(&pRelativePose[i + 4]);

			const int32 p = pJoints[i].m_idxParent;
			if (p < 0)
			{
				pAbsolutePose[i] = pRelativePose[i];
				pAbsoluteScaling[i] = pRelativeScaling[i];
			}
			else
			{
				QuatTS parentPose = pAbsolutePose[p];
				parentPose.s = pAbsoluteScaling[p];

				QuatTS localPose = pRelativePose[i];
				localPose.s = pRelativeScaling[i];

				const QuatTS pose = parentPose * localPose;
				pAbsolutePose[i].q = pose.q.GetNormalizedSafe();
				pAbsolutePose[i].t = pose.t;
				pAbsoluteScaling[i] = pose.s;
			}
		}
	}
	else
	{
		if (singleRoot) // TODO: The singleRoot optimization should probably be the standard path in the first place. Do we still support multiple root skeletons?
		{
			CryPrefetch(&pJoints[1].m_idxParent);
			pAbsolutePose[0] = pRelativePose[0];

			for (uint32 i = 1; i < m_jointCount; ++i)
			{
				CryPrefetch(&pJoints[i + 1].m_idxParent);
				CryPrefetch(&pAbsolutePose[i + 4]);
				CryPrefetch(&pRelativePose[i + 4]);

				const int32 p = pJoints[i].m_idxParent;
				assert(p >= 0);

				const QuatT pose = pAbsolutePose[p] * pRelativePose[i];
				pAbsolutePose[i].q = pose.q.GetNormalizedSafe();
				pAbsolutePose[i].t = pose.t;
			}
		}
		else
		{
			CryPrefetch(&pJoints[0].m_idxParent);
			for (uint32 i = 0; i < m_jointCount; ++i)
			{
				CryPrefetch(&pJoints[i + 1].m_idxParent);
				CryPrefetch(&pAbsolutePose[i + 4]);
				CryPrefetch(&pRelativePose[i + 4]);

				const int32 p = pJoints[i].m_idxParent;
				if (p < 0)
				{
					pAbsolutePose[i] = pRelativePose[i];
				}
				else
				{
					const QuatT pose = pAbsolutePose[p] * pRelativePose[i];
					pAbsolutePose[i].q = pose.q.GetNormalizedSafe();
					pAbsolutePose[i].t = pose.t;
				}
			}
		}
	}
}

void CPoseData::ComputeRelativePose(const CDefaultSkeleton& rDefaultSkeleton)
{
	const QuatT* const __restrict pAbsolutePose = GetJointsAbsolute();
	QuatT* const __restrict pRelativePose = GetJointsRelative();

	const CDefaultSkeleton::SJoint* pJoints = &rDefaultSkeleton.m_arrModelJoints[0];

	assert(pRelativePose);
	assert(pAbsolutePose);

	if (IsScalingEnabled())
	{
		const float* const __restrict pAbsoluteScaling = GetScalingAbsolute();
		float* const __restrict pRelativeScaling = GetScalingRelative();

		assert(pRelativeScaling);
		assert(pAbsoluteScaling);

		for (uint32 i = 0; i < m_jointCount; ++i)
		{
			const int32 p = pJoints[i].m_idxParent;
			if (p < 0)
			{
				pRelativePose[i] = pAbsolutePose[i];
				pRelativeScaling[i] = pAbsoluteScaling[i];
			}
			else
			{
				QuatTS parentPose = pAbsolutePose[p];
				parentPose.s = pAbsoluteScaling[p];

				QuatTS localPose = pAbsolutePose[i];
				localPose.s = pAbsoluteScaling[i];

				const QuatTS pose = parentPose.GetInverted() * localPose;
				pRelativePose[i].q = pose.q;
				pRelativePose[i].t = pose.t;
				pRelativeScaling[i] = pose.s;
			}
		}
	}
	else
	{
		// TODO: Implement an optimized path for single-root assets.

		for (uint32 i = 0; i < m_jointCount; ++i)
		{
			const int32 p = pJoints[i].m_idxParent;
			if (p < 0)
			{
				pRelativePose[i] = pAbsolutePose[i];
			}
			else
			{
				const QuatTS pose = pAbsolutePose[p].GetInverted() * pAbsolutePose[i];
				pRelativePose[i].q = pose.q;
				pRelativePose[i].t = pose.t;
			}
		}
	}
}

int CPoseData::GetParentIndex(const CDefaultSkeleton& rDefaultSkeleton, const uint32 index) const
{
	return rDefaultSkeleton.GetJointParentIDByID(int(index));
}

void CPoseData::ResetToDefault(const CDefaultSkeleton& rDefaultSkeleton)
{
	Initialize(rDefaultSkeleton.m_poseDefaultData);
}

#ifndef _RELEASE

namespace {
uint32 s_result;
}

PREFAST_SUPPRESS_WARNING(6262);
void CPoseData::ValidateRelative(const CDefaultSkeleton& rDefaultSkeleton) const
{
	const CDefaultSkeleton::SJoint* pJoints = &rDefaultSkeleton.m_arrModelJoints[0];

	if (Console::GetInst().ca_Validate)
	{
		CRY_ASSERT(m_jointCount <= MAX_JOINT_AMOUNT);
		QuatT g_pAbsolutePose[MAX_JOINT_AMOUNT];
		AABB rAABB;
		rAABB.min.Set(+99999.0f, +99999.0f, +99999.0f);
		rAABB.max.Set(-99999.0f, -99999.0f, -99999.0f);
		const QuatT* const __restrict pRelativePose = GetJointsRelative();
		for (uint32 i = 0; i < m_jointCount; ++i)
		{
			int32 p = pJoints[i].m_idxParent;
			if (p < 0)
				g_pAbsolutePose[i] = pRelativePose[i];
			else
				g_pAbsolutePose[i] = g_pAbsolutePose[p] * pRelativePose[i];
		}

		if (m_jointCount)
		{
			for (uint32 i = 1; i < m_jointCount; i++)
				rAABB.Add(g_pAbsolutePose[i].t);
			s_result = uint32(g_pAbsolutePose[m_jointCount - 1].q.GetLength() + g_pAbsolutePose[m_jointCount - 1].t.GetLength());
			s_result += uint32(rAABB.min.GetLength());
			s_result += uint32(rAABB.max.GetLength());
		}
	}
}

PREFAST_SUPPRESS_WARNING(6262);
void CPoseData::ValidateAbsolute(const CDefaultSkeleton& rDefaultSkeleton) const
{
	const CDefaultSkeleton::SJoint* pJoints = &rDefaultSkeleton.m_arrModelJoints[0];
	if (Console::GetInst().ca_Validate)
	{
		QuatT g_pRelativePose[MAX_JOINT_AMOUNT];
		AABB rAABB;
		rAABB.min.Set(+99999.0f, +99999.0f, +99999.0f);
		rAABB.max.Set(-99999.0f, -99999.0f, -99999.0f);
		const QuatT* const __restrict pAbsolutePose = GetJointsAbsolute();
		for (uint32 i = 0; i < m_jointCount; ++i)
		{
			int32 p = pJoints[i].m_idxParent;
			if (p < 0)
				g_pRelativePose[i] = pAbsolutePose[i];
			else
				g_pRelativePose[i] = pAbsolutePose[p].GetInverted() * pAbsolutePose[i] * g_pRelativePose[p];
		}

		if (m_jointCount)
		{
			for (uint32 i = 1; i < m_jointCount; i++)
				rAABB.Add(pAbsolutePose[i].t);
			s_result = uint32(g_pRelativePose[m_jointCount - 1].q.GetLength() + g_pRelativePose[m_jointCount - 1].t.GetLength());
			s_result += uint32(rAABB.min.GetLength());
			s_result += uint32(rAABB.max.GetLength());
		}
	}
}

void CPoseData::Validate(const CDefaultSkeleton& rDefaultSkeleton) const
{
	ValidateRelative(rDefaultSkeleton);
	ValidateAbsolute(rDefaultSkeleton);
}

#else

void CPoseData::ValidateRelative(const CDefaultSkeleton&) const {}

void CPoseData::ValidateAbsolute(const CDefaultSkeleton&) const {}

void CPoseData::Validate(const CDefaultSkeleton&) const         {}

#endif // _RELEASE

} //namespace Skeleton

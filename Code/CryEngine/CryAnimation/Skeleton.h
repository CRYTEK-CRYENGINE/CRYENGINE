// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Memory/MultiBuffer.h"

class CDefaultSkeleton;
namespace Memory {
class CPool;
}

namespace Skeleton
{

class CPoseData : public IAnimationPoseData
{
public:

	CPoseData();

	static CPoseData* GetPoseData(IAnimationPoseData* pPoseData);

	void              SetMemoryPool(Memory::CPool* pMemoryPool);

	bool              Initialize(uint32 jointCount);
	bool              Initialize(const CDefaultSkeleton& skeleton);
	bool              Initialize(const CPoseData& poseData);

	void              ComputeAbsolutePose(const CDefaultSkeleton& rDefaultSkeleton, bool singleRoot = false);
	void              ComputeRelativePose(const CDefaultSkeleton& rDefaultSkeleton);
	void              ResetToDefault(const CDefaultSkeleton& rDefaultSkeleton);

	int               GetParentIndex(const CDefaultSkeleton& rDefaultSkeleton, const uint32 index) const;

	void              GetMemoryUsage(ICrySizer* pSizer) const;

	/**
	 * Evaluates the total amount of memory dynamically allocated by this object.
	 */
	uint32 GetAllocationLength() const;

	/**
	 * Retrieves model-space orientation, position and scale of a single joint within this pose.
	 * @param index Index of the joint. Must be lower than the number of joints.
	 * @return Current position, orientation and scale values.
	 */
	const QuatTS GetJointAbsoluteOPS(const uint32 index) const;

	/**
	 * Retrieves parent-space orientation, position and scale of a single joint within this pose.
	 * @param index Index of the joint. Must be lower than the number of joints.
	 * @return Current position, orientation and scale values.
	 */
	const QuatTS GetJointRelativeOPS(const uint32 index) const;

	//////////////////////////////////////////////////////////
	// IAnimationPoseData implementation
	//////////////////////////////////////////////////////////
	virtual uint32       GetJointCount() const override;
	virtual const QuatT& GetJointRelative(const uint32 index) const override;
	virtual const Vec3&  GetJointRelativeP(const uint32 index) const override;
	virtual const Quat&  GetJointRelativeO(const uint32 index) const override;
	virtual const Diag33 GetJointRelativeS(const uint32 index) const override;
	virtual void         SetJointRelative(const uint32 index, const QuatT& transformation) override;
	virtual void         SetJointRelativeP(const uint32 index, const Vec3& position) override;
	virtual void         SetJointRelativeO(const uint32 index, const Quat& orientation) override;
	virtual void         SetJointRelativeS(const uint32 index, const float scaling) override;
	virtual const QuatT& GetJointAbsolute(const uint32 index) const override;
	virtual const Vec3&  GetJointAbsoluteP(const uint32 index) const override;
	virtual const Quat&  GetJointAbsoluteO(const uint32 index) const override;
	virtual const Diag33 GetJointAbsoluteS(const uint32 index) const override;
	virtual void         SetJointAbsolute(const uint32 index, const QuatT& transformation) override;
	virtual void         SetJointAbsoluteP(const uint32 index, const Vec3& position) override;
	virtual void         SetJointAbsoluteO(const uint32 index, const Quat& orientation) override;
	virtual void         SetJointAbsoluteS(const uint32 index, const float scaling) override;
	//////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////
	// OBSOLETE:
	// Use single joint Set/Get methods instead!
	ILINE QuatT*            GetJointsRelative();
	ILINE const QuatT*      GetJointsRelative() const;

	ILINE QuatT*            GetJointsAbsolute();
	ILINE const QuatT*      GetJointsAbsolute() const;

	ILINE float*            GetScalingRelative();
	ILINE const float*      GetScalingRelative() const;

	ILINE float*            GetScalingAbsolute();
	ILINE const float*      GetScalingAbsolute() const;

	ILINE JointState*       GetJointsStatus();
	ILINE const JointState* GetJointsStatus() const;

	ILINE const QuatT*      GetJointsRelativeMain() const { return GetJointsRelative(); } // TODO: Duplicated functionality, to be removed
	ILINE const QuatT*      GetJointsAbsoluteMain() const { return GetJointsAbsolute(); } // TODO: Duplicated functionality, to be removed
	ILINE const JointState* GetJointsStatusMain() const   { return GetJointsStatus(); }   // TODO: Duplicated functionality, to be removed
	//////////////////////////////////////////////////////////

	void ValidateRelative(const CDefaultSkeleton& rDefaultSkeleton) const;
	void ValidateAbsolute(const CDefaultSkeleton& rDefaultSkeleton) const;
	void Validate(const CDefaultSkeleton& rDefaultSkeleton) const;

private:

	bool AreScalingBuffersAllocated() const;
	bool IsScalingEnabled() const;
	bool EnableScaling();

	typedef Memory::MultiBuffer<NTypelist::CConstruct<QuatT, QuatT, JointState>::TType> JointsBuffer;
	typedef Memory::MultiBuffer<NTypelist::CConstruct<float, float>::TType>             ScalingBuffer;

	JointsBuffer  m_jointsBuffer;
	ScalingBuffer m_scalingBuffer;
	uint32        m_jointCount;
};

} // namespace Skeleton

namespace Skeleton
{

inline CPoseData* CPoseData::GetPoseData(IAnimationPoseData* pPoseData)
{
	return static_cast<CPoseData*>(pPoseData);
}

inline QuatT* CPoseData::GetJointsRelative()
{
	assert(m_jointsBuffer);
	return m_jointsBuffer.get<0>();
}

inline const QuatT* CPoseData::GetJointsRelative() const
{
	assert(m_jointsBuffer);
	return m_jointsBuffer.get<0>();
}

inline QuatT* CPoseData::GetJointsAbsolute()
{
	assert(m_jointsBuffer);
	return m_jointsBuffer.get<1>();
}

inline const QuatT* CPoseData::GetJointsAbsolute() const
{
	assert(m_jointsBuffer);
	return m_jointsBuffer.get<1>();
}

inline float* CPoseData::GetScalingRelative()
{
	return EnableScaling() ? m_scalingBuffer.get<0>() : nullptr;
}

inline const float* CPoseData::GetScalingRelative() const
{
	return IsScalingEnabled() ? m_scalingBuffer.get<0>() : nullptr;
}

inline float* CPoseData::GetScalingAbsolute()
{
	return EnableScaling() ? m_scalingBuffer.get<1>() : nullptr;
}

inline const float* CPoseData::GetScalingAbsolute() const
{
	return IsScalingEnabled() ? m_scalingBuffer.get<1>() : nullptr;
}

inline JointState* CPoseData::GetJointsStatus()
{
	assert(m_jointsBuffer);
	return m_jointsBuffer.get<2>();
}

inline const JointState* CPoseData::GetJointsStatus() const
{
	assert(m_jointsBuffer);
	return m_jointsBuffer.get<2>();
}

} // namespace Skeleton

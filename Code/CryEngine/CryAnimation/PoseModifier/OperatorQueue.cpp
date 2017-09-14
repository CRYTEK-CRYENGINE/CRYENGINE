// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "OperatorQueue.h"

#include "PoseModifierHelper.h"

/*
   COperatorQueue
 */

CRYREGISTER_CLASS(COperatorQueue)

//

COperatorQueue::COperatorQueue()
{
	m_ops[0].reserve(8);
	m_ops[1].reserve(8);
	m_current = 0;
};

//

void COperatorQueue::PushPosition(uint32 jointIndex, EOp eOp, const Vec3& value)
{
	OperatorQueue::SOp op;
	op.joint = jointIndex;
	op.target = eTarget_Position;
	op.op = eOp;
	op.value.n[0] = value.x;
	op.value.n[1] = value.y;
	op.value.n[2] = value.z;
	op.value.n[3] = 0.0f;
	m_ops[m_current].push_back(op);
}

void COperatorQueue::PushOrientation(uint32 jointIndex, EOp eOp, const Quat& value)
{
	OperatorQueue::SOp op;
	op.joint = jointIndex;
	op.target = eTarget_Orientation;
	op.op = eOp;
	op.value.n[0] = value.v.x;
	op.value.n[1] = value.v.y;
	op.value.n[2] = value.v.z;
	op.value.n[3] = value.w;
	m_ops[m_current].push_back(op);
}

void COperatorQueue::PushStoreRelative(uint32 jointIndex, QuatT& output)
{
	OperatorQueue::SOp op;
	op.joint = jointIndex;
	op.op = eOpInternal_StoreRelative;
	// NOTE: A direct pointer is stored for now. This should be ideally
	// double-buffered and updated upon syncing.
	op.value.p = &output;
	m_ops[m_current].push_back(op);
}

void COperatorQueue::PushStoreAbsolute(uint32 jointIndex, QuatT& output)
{
	OperatorQueue::SOp op;
	op.joint = jointIndex;
	op.op = eOpInternal_StoreAbsolute;
	// NOTE: A direct pointer is stored for now. This should be ideally
	// double-buffered and updated upon syncing.
	op.value.p = &output;
	m_ops[m_current].push_back(op);
}

void COperatorQueue::PushStoreWorld(uint32 jointIndex, QuatT& output)
{
	OperatorQueue::SOp op;
	op.joint = jointIndex;
	op.op = eOpInternal_StoreWorld;
	// NOTE: A direct pointer is stored for now. This should be ideally
	// double-buffered and updated upon syncing.
	op.value.p = &output;
	m_ops[m_current].push_back(op);
}

void COperatorQueue::PushComputeAbsolute()
{
	OperatorQueue::SOp op;
	op.joint = 0;
	op.op = eOpInternal_ComputeAbsolute;
	m_ops[m_current].push_back(op);
}

void COperatorQueue::Clear()
{
	m_ops[m_current].clear();
}

// IAnimationPoseModifier

bool COperatorQueue::Prepare(const SAnimationPoseModifierParams& params)
{
	++m_current &= 1;
	m_ops[m_current].clear();
	return true;
}

bool COperatorQueue::Execute(const SAnimationPoseModifierParams& params)
{
	DEFINE_PROFILER_FUNCTION();

	//	- Store direct pointer to reduce base pointer lookups
	IAnimationPoseData* pParamsPoseData = params.pPoseData;

	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(pParamsPoseData);
	const CDefaultSkeleton& rDefaultSkeleton = (const CDefaultSkeleton&)params.GetIDefaultSkeleton();

	pPoseData->ComputeAbsolutePose(rDefaultSkeleton);

	uint32 jointCount = pParamsPoseData->GetJointCount();

	const std::vector<OperatorQueue::SOp>& ops = m_ops[(m_current + 1) & 1];
	uint32 opCount = uint32(ops.size());
	for (uint32 i = 0; i < opCount; ++i)
	{
		// - Use copy instead of reference
		const OperatorQueue::SOp op = ops[i];

		uint32 jointIndex = op.joint;
		if (jointIndex >= jointCount)
			continue;

		const bool lastOperator = (i == opCount - 1);

		//	- Lookup id regardless if needed (adds runtime cost)
		int32 parent = rDefaultSkeleton.GetJointParentIDByID(op.joint);

		switch (op.op)
		{
		case eOpInternal_OverrideAbsolute:
			{
				switch (op.target)
				{
				case eTarget_Position:
					pParamsPoseData->SetJointAbsoluteP(jointIndex, Vec3(op.value.n[0], op.value.n[1], op.value.n[2]));
					break;

				case eTarget_Orientation:
					pParamsPoseData->SetJointAbsoluteO(jointIndex, Quat(op.value.n[3], op.value.n[0], op.value.n[1], op.value.n[2]));
					break;
				}

				if (parent < 0)
				{
					pParamsPoseData->SetJointRelative(jointIndex, pParamsPoseData->GetJointAbsolute(jointIndex));
					continue;
				}
				pParamsPoseData->SetJointRelative(jointIndex,
				                                  pParamsPoseData->GetJointAbsolute(parent).GetInverted() *
				                                  pParamsPoseData->GetJointAbsolute(jointIndex));

				if (!lastOperator)
				{
					PoseModifierHelper::ComputeAbsoluteTransformation(params, jointIndex);
				}
				break;
			}

		case eOpInternal_OverrideWorld:
			{
				switch (op.target)
				{
				case eTarget_Position:
					params.pPoseData->SetJointAbsoluteP(jointIndex, (Vec3(op.value.n[0], op.value.n[1], op.value.n[2]) - params.location.t) * params.location.q);
					break;

				case eTarget_Orientation:
					params.pPoseData->SetJointAbsoluteO(jointIndex, params.location.q.GetInverted() * Quat(op.value.n[3], op.value.n[0], op.value.n[1], op.value.n[2]));
					break;
				}

				if (parent < 0)
				{
					params.pPoseData->SetJointRelative(jointIndex, params.pPoseData->GetJointAbsolute(jointIndex));
					continue;
				}
				params.pPoseData->SetJointRelative(jointIndex,
				                                   params.pPoseData->GetJointAbsolute(parent).GetInverted() *
				                                   params.pPoseData->GetJointAbsolute(jointIndex));

				if (!lastOperator)
				{
					PoseModifierHelper::ComputeAbsoluteTransformation(params, jointIndex);
				}

				break;
			}

		case eOpInternal_OverrideRelative:
			{
				switch (op.target)
				{
				case eTarget_Position:
					pParamsPoseData->SetJointRelativeP(jointIndex, Vec3(op.value.n[0], op.value.n[1], op.value.n[2]));
					break;

				case eTarget_Orientation:
					pParamsPoseData->SetJointRelativeO(jointIndex, Quat(op.value.n[3], op.value.n[0], op.value.n[1], op.value.n[2]));
					break;
				}

				if (parent < 0)
				{
					pParamsPoseData->SetJointAbsolute(jointIndex, pParamsPoseData->GetJointRelative(jointIndex));
					continue;
				}
				pParamsPoseData->SetJointAbsolute(jointIndex,
				                                  pParamsPoseData->GetJointAbsolute(parent) *
				                                  pParamsPoseData->GetJointRelative(jointIndex));

				if (!lastOperator)
				{
					PoseModifierHelper::ComputeAbsoluteTransformation(params, jointIndex);
				}
				break;
			}

		case eOpInternal_AdditiveAbsolute:
			{
				switch (op.target)
				{
				case eTarget_Position:
					pParamsPoseData->SetJointAbsoluteP(jointIndex,
					                                   pParamsPoseData->GetJointAbsolute(jointIndex).t +
					                                   Vec3(op.value.n[0], op.value.n[1], op.value.n[2]));
					break;

				case eTarget_Orientation:
					const Quat newRot(op.value.n[3], op.value.n[0], op.value.n[1], op.value.n[2]);
					pParamsPoseData->SetJointAbsoluteO(jointIndex, newRot * pParamsPoseData->GetJointAbsolute(jointIndex).q);
					break;
				}

				if (parent < 0)
				{
					pParamsPoseData->SetJointRelative(jointIndex, pParamsPoseData->GetJointAbsolute(jointIndex));
					continue;
				}

				pParamsPoseData->SetJointRelative(jointIndex,
				                                  pParamsPoseData->GetJointAbsolute(parent).GetInverted() *
				                                  pParamsPoseData->GetJointAbsolute(jointIndex));

				if (!lastOperator)
				{
					PoseModifierHelper::ComputeAbsoluteTransformation(params, jointIndex);
				}
				break;
			}

		case eOpInternal_AdditiveRelative:
			{
				switch (op.target)
				{
				case eTarget_Position:
					pParamsPoseData->SetJointRelativeP(jointIndex,
					                                   pParamsPoseData->GetJointRelative(jointIndex).t +
					                                   Vec3(op.value.n[0], op.value.n[1], op.value.n[2]));
					break;

				case eTarget_Orientation:
					const Quat newRot(op.value.n[3], op.value.n[0], op.value.n[1], op.value.n[2]);
					pParamsPoseData->SetJointRelativeO(jointIndex, newRot * pParamsPoseData->GetJointRelative(jointIndex).q);
					break;
				}

				if (parent < 0)
				{
					pParamsPoseData->SetJointAbsolute(jointIndex, pParamsPoseData->GetJointRelative(jointIndex));
					continue;
				}

				pParamsPoseData->SetJointAbsolute(jointIndex,
				                                  pParamsPoseData->GetJointAbsolute(parent) *
				                                  pParamsPoseData->GetJointRelative(jointIndex));

				if (!lastOperator)
				{
					PoseModifierHelper::ComputeAbsoluteTransformation(params, jointIndex);
				}
				break;
			}

		case eOpInternal_StoreRelative:
			{
				*op.value.p = pParamsPoseData->GetJointRelative(op.joint);
				break;
			}

		case eOpInternal_StoreAbsolute:
			{
				*op.value.p = pParamsPoseData->GetJointAbsolute(op.joint);
				break;
			}

		case eOpInternal_StoreWorld:
			{
				*op.value.p = QuatT(params.location * params.pPoseData->GetJointAbsolute(op.joint));
				break;
			}

		case eOpInternal_ComputeAbsolute:
			{
				if (i)
				{
					// TEMP: For now we implicitly always compute the absolute
					// pose, avoid doing so twice.
					pPoseData->ComputeAbsolutePose(rDefaultSkeleton);
				}
				break;
			}
		}
	}

	return true;
}

void COperatorQueue::Synchronize()
{
	m_ops[(m_current + 1) & 1].clear();
}

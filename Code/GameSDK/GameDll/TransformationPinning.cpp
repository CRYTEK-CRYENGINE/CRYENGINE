// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IRenderAuxGeom.h>

#include <CryAnimation/ICryAnimation.h>

#include <CryExtension/ClassWeaver.h>
#include <CryExtension/CryCreateClassInstance.h>

#include "ITransformationPinning.h"
#include "TransformationPinning.h"

CRYREGISTER_CLASS(CTransformationPinning)

//

CTransformationPinning::CTransformationPinning() 
{	
	m_factor	= 0.0f;
	m_jointsInitialised = false;
	m_jointID = -1;
	m_numJoints = 0;
	m_jointTypes = NULL;
	m_source = NULL;
};

CTransformationPinning::~CTransformationPinning() 
{
	if (m_jointTypes)
	{
		delete [] m_jointTypes;
	}
}

//

void CTransformationPinning::SetJoint(uint32 jntID)
{
	m_jointID = jntID;
	m_jointsInitialised = false;
}

void CTransformationPinning::SetSource(ICharacterInstance* source)
{
	m_source = source;
	m_jointsInitialised = false;
}

void CTransformationPinning::Init(const SAnimationPoseModifierParams& params)
{
	m_jointsInitialised = true;
	IDefaultSkeleton& rIDefaultSkeleton = m_source->GetIDefaultSkeleton();
	int numJoints = rIDefaultSkeleton.GetJointCount();
	if (m_numJoints != numJoints)
	{
		if (m_jointTypes)
		{
			delete [] m_jointTypes;
		}
		m_jointTypes = new char[numJoints];
	}
	memset(m_jointTypes, TransformationPinJoint::Copy, sizeof(char) * numJoints);

	if (m_jointID < numJoints)
		m_jointTypes[m_jointID] = TransformationPinJoint::Feather;

	for (int16 i=0; i<numJoints; i++)
	{
		int16 parent = rIDefaultSkeleton.GetJointParentIDByID(i);
		if ((parent >= 0) && (m_jointTypes[parent] != TransformationPinJoint::Copy))
		{
			m_jointTypes[i] = TransformationPinJoint::Inherit;
		}
	}

	m_numJoints = numJoints;
}

void CTransformationPinning::SetBlendWeight(float factor)
{
	m_factor = factor;
}

bool CTransformationPinning::Execute(const SAnimationPoseModifierParams& params)
{
	if (m_factor == 0.0f)
		return false;

	CRY_ASSERT(m_source);

	if (!m_jointsInitialised)
	{
		Init(params);
	}

	ISkeletonPose* pSkeletonPose = params.pCharacterInstance->GetISkeletonPose();
	IDefaultSkeleton& rIDefaultSkeleton = m_source->GetIDefaultSkeleton();
	int sourceJoints = m_source->GetIDefaultSkeleton().GetJointCount();
	CRY_ASSERT(sourceJoints == params.pCharacterInstance->GetIDefaultSkeleton().GetJointCount());
	
	for (int i=0; i<sourceJoints; i++)
	{
		const QuatT &sourceJoint = m_source->GetISkeletonPose()->GetAbsJointByID(i);
		const QuatT &relSourceJoint	 = m_source->GetISkeletonPose()->GetRelJointByID(i);

		CRY_ASSERT(params.pPoseData->GetJointAbsolute(i).IsValid());
		CRY_ASSERT(sourceJoint.IsValid());

		switch (m_jointTypes[i])
		{
		case TransformationPinJoint::Copy:
			if (m_factor < 1.0f)
			{
				QuatT diff = params.pPoseData->GetJointAbsolute(i).GetInverted() * sourceJoint;
				params.pPoseData->SetJointAbsolute(i,
					params.pPoseData->GetJointAbsolute(i) * diff.GetScaled(m_factor));

				diff = params.pPoseData->GetJointRelative(i).GetInverted() * relSourceJoint;
				params.pPoseData->SetJointRelative(i, params.pPoseData->GetJointRelative(i) * diff.GetScaled(m_factor));
			}
			else
			{
				params.pPoseData->SetJointAbsolute(i, sourceJoint);
				params.pPoseData->SetJointRelative(i, relSourceJoint);
			}
			break;
		case TransformationPinJoint::Feather:
			{
				int16 parent = m_source->GetIDefaultSkeleton().GetJointParentIDByID(i);
				QuatT invParent = params.pPoseData->GetJointAbsolute(parent).GetInverted();
				params.pPoseData->SetJointRelative(i, invParent * params.pPoseData->GetJointAbsolute(i));
				QuatT thisJnt = params.pPoseData->GetJointAbsolute(parent) * params.pPoseData->GetJointRelative(i);

				CRY_ASSERT(params.pPoseData->GetJointRelative(i).IsValid());
				CRY_ASSERT(thisJnt.IsValid());
			}
			break;
		case TransformationPinJoint::Inherit:
			break;
		}
	}

	return true;
}

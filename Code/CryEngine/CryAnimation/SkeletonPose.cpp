// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SkeletonPose.h"

#include <CryExtension/ClassWeaver.h>
#include <CryExtension/ICryFactoryRegistryImpl.h>
#include <CryExtension/CryCreateClassInstance.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include "CharacterManager.h"
#include "CharacterInstance.h"
#include "Model.h"
#include "Helper.h"
#include "ControllerOpt.h"
#include "PoseModifier/Recoil.h"

CSkeletonPose::CSkeletonPose()
	: m_nForceSkeletonUpdate(0)
	, m_pPostProcessCallback(nullptr)
	, m_pPostProcessCallbackData(nullptr)
	, m_pInstance(nullptr)
	, m_fDisplaceRadiant(0.0f)
	, m_pPoseDataWriteable(nullptr)
	, m_pPoseDataDefault(nullptr)
	, m_AABB(2.0f)
	, m_bInstanceVisible(false)
	, m_bFullSkeletonUpdate(false)
	, m_bAllNodesValid(0)
	, m_bSetDefaultPoseExecute(false)
	, m_pSkeletonAnim(nullptr)
{
}

CSkeletonPose::~CSkeletonPose()
{
}

//////////////////////////////////////////////////////////////////////////
// initialize the moving skeleton
//////////////////////////////////////////////////////////////////////////
void CSkeletonPose::InitSkeletonPose(CCharInstance* pInstance, CSkeletonAnim* pSkeletonAnim)
{
	m_pInstance = pInstance;
	m_pSkeletonAnim = pSkeletonAnim;
	m_poseData.Initialize(*m_pInstance->m_pDefaultSkeleton);
	m_pPoseDataDefault = &pInstance->m_pDefaultSkeleton->m_poseDefaultData;

	//---------------------------------------------------------------------
	//---                   physics                                    ----
	//---------------------------------------------------------------------
	//SetDefaultPoseExecute(false);

	m_physics.Initialize(*this);
	m_physics.InitializeAnimToPhysIndexArray();
	m_physics.InitPhysicsSkeleton();

	if (m_pInstance->m_pDefaultSkeleton->m_poseBlenderLookDesc.m_error == 0)
	{
		IAnimationPoseBlenderDir* pPBLook = m_PoseBlenderLook.get();
		if (pPBLook == 0)
		{
			CryCreateClassInstance<IAnimationPoseBlenderDir>(CPoseBlenderLook::GetCID(), m_PoseBlenderLook);
		}
	}

	if (m_pInstance->m_pDefaultSkeleton->m_poseBlenderAimDesc.m_error == 0)
	{
		IAnimationPoseBlenderDir* pPBAim = m_PoseBlenderAim.get();
		if (pPBAim == 0)
		{
			CryCreateClassInstance<IAnimationPoseBlenderDir>(CPoseBlenderAim::GetCID(), m_PoseBlenderAim);
		}
	}

}

Skeleton::CPoseData* CSkeletonPose::GetPoseDataWriteable()
{
	return m_pPoseDataWriteable;
}

Skeleton::CPoseData& CSkeletonPose::GetPoseDataForceWriteable()
{
	m_pSkeletonAnim->FinishAnimationComputations();
	return m_poseData;
}

bool CSkeletonPose::PreparePoseDataAndLocatorWriteables(Memory::CPool& memoryPool)
{
	DEFINE_PROFILER_FUNCTION();

	if (m_pPoseDataWriteable)
	{
		m_poseDataWriteable.Initialize(GetPoseDataDefault());
		return true;
	}

	m_poseDataWriteable.SetMemoryPool(&memoryPool);

	if (!m_poseDataWriteable.Initialize(m_poseData))
	{
		return false;
	}

	m_pPoseDataWriteable = &m_poseDataWriteable;

	return true;
}

void CSkeletonPose::SynchronizePoseDataAndLocatorWriteables()
{
	if (m_pPoseDataWriteable)
	{
		m_pInstance->WaitForSkinningJob();

		m_poseData.Initialize(m_poseDataWriteable);
		m_poseDataWriteable.Initialize(0);
		m_pPoseDataWriteable = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
// reset the bone to default position/orientation
// initializes the bones and IK limb pose
//////////////////////////////////////////////////////////////////////////

void CSkeletonPose::SetDefaultPosePerInstance(bool bDataPoseForceWriteable)
{
	Skeleton::CPoseData& poseDataWriteable =
	  bDataPoseForceWriteable ? GetPoseDataForceWriteable() : m_poseData;

	m_pSkeletonAnim->StopAnimationsAllLayers();

	m_bInstanceVisible = 0;
	m_bFullSkeletonUpdate = 0;

	PoseModifier::CRecoil* pRecoil = static_cast<PoseModifier::CRecoil*>(m_recoil.get());
	if (pRecoil)
		pRecoil->SetState(PoseModifier::CRecoil::State());

	CPoseBlenderLook* pPBLook = static_cast<CPoseBlenderLook*>(m_PoseBlenderLook.get());
	if (pPBLook)
		pPBLook->m_blender.Init();

	CPoseBlenderAim* pPBAim = static_cast<CPoseBlenderAim*>(m_PoseBlenderAim.get());
	if (pPBAim)
		pPBAim->m_blender.Init();

	uint32 numJoints = m_pInstance->m_pDefaultSkeleton->GetJointCount();
	if (numJoints)
	{
		for (uint32 i = 0; i < numJoints; i++)
		{
			poseDataWriteable.GetJointsRelative()[i] = m_pInstance->m_pDefaultSkeleton->m_poseDefaultData.GetJointsRelative()[i];
			poseDataWriteable.GetJointsAbsolute()[i] = m_pInstance->m_pDefaultSkeleton->m_poseDefaultData.GetJointsAbsolute()[i];
		}

		for (uint32 i = 1; i < numJoints; i++)
		{
			poseDataWriteable.GetJointsAbsolute()[i] = poseDataWriteable.GetJointsRelative()[i];
			int32 p = m_pInstance->m_pDefaultSkeleton->m_arrModelJoints[i].m_idxParent;
			if (p >= 0)
				poseDataWriteable.GetJointsAbsolute()[i] = poseDataWriteable.GetJointsAbsolute()[p] * poseDataWriteable.GetJointsRelative()[i];
			poseDataWriteable.GetJointsAbsolute()[i].q.Normalize();
		}
	}

	if (m_physics.m_pCharPhysics != NULL)
	{
		if (m_pInstance->m_pDefaultSkeleton->m_ObjectType == CGA)
		{
			m_pInstance->UpdatePhysicsCGA(poseDataWriteable, 1, QuatTS(IDENTITY));
		}
		else if (!m_physics.m_bPhysicsRelinquished)
		{
			m_physics.SynchronizeWithPhysicalEntity(poseDataWriteable, m_physics.m_pCharPhysics, Vec3(ZERO), Quat(IDENTITY), QuatT(IDENTITY), 0);
		}
	}
}

void CSkeletonPose::SetDefaultPoseExecute(bool bDataPoseForceWriteable)
{
	SetDefaultPosePerInstance(bDataPoseForceWriteable);

	IAttachmentManager* pIAttachmentManager = m_pInstance->GetIAttachmentManager();
	uint32 numAttachments = pIAttachmentManager->GetAttachmentCount();
	for (uint32 i = 0; i < numAttachments; i++)
	{
		IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(i);
		IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();
		if (pIAttachmentObject)
		{
			if (pIAttachment->GetType() == CA_SKIN)
				continue;
			if (pIAttachmentObject->GetAttachmentType() != IAttachmentObject::eAttachment_Skeleton)
				continue;

			CCharInstance* pCharacterInstance = (CCharInstance*)pIAttachmentObject->GetICharacterInstance();
			pCharacterInstance->m_SkeletonPose.SetDefaultPosePerInstance(bDataPoseForceWriteable);
		}
	}
}

void CSkeletonPose::SetDefaultPose()
{
	m_bSetDefaultPoseExecute = true;
}

void CSkeletonPose::InitCGASkeleton()
{
	uint32 numJoints = m_pInstance->m_pDefaultSkeleton->m_arrModelJoints.size();

	m_arrCGAJoints.resize(numJoints);

	for (uint32 i = 0; i < numJoints; i++)
	{
		m_arrCGAJoints[i].m_CGAObjectInstance = m_pInstance->m_pDefaultSkeleton->m_arrModelJoints[i].m_CGAObject;
	}

	m_Extents.Clear();
}

const IStatObj* CSkeletonPose::GetStatObjOnJoint(int32 nId) const
{
	if (nId < 0 || nId >= (int)GetPoseDataDefault().GetJointCount())
	{
		assert(0);
		return NULL;
	}
	if (m_arrCGAJoints.size())
	{
		const CCGAJoint& joint = m_arrCGAJoints[nId];
		return joint.m_CGAObjectInstance;
	}

	return 0;
}

IStatObj* CSkeletonPose::GetStatObjOnJoint(int32 nId)
{
	const CSkeletonPose* pConstThis = this;
	return const_cast<IStatObj*>(pConstThis->GetStatObjOnJoint(nId));
}

void CSkeletonPose::SetStatObjOnJoint(int32 nId, IStatObj* pStatObj)
{
	if (nId < 0 || nId >= (int)GetPoseDataDefault().GetJointCount())
	{
		assert(0);
		return;
	}

	assert(m_arrCGAJoints.size());
	// do not handle physicalization in here, use IEntity->SetStatObj instead
	CCGAJoint& joint = m_arrCGAJoints[nId];
	joint.m_CGAObjectInstance = pStatObj;

	m_Extents.Clear();
}

#include "PoseModifier/LimbIk.h"
uint32 CSkeletonPose::SetHumanLimbIK(const Vec3& vWorldPos, const char* strLimb)
{
	if (!m_limbIk.get())
	{
		CryCreateClassInstance<IAnimationPoseModifier>(CLimbIk::GetCID(), m_limbIk);
		assert(m_limbIk.get());
	}

	Vec3 targetPositionLocal = m_pInstance->m_location.GetInverted() * vWorldPos;

	CLimbIk* pLimbIk = static_cast<CLimbIk*>(m_limbIk.get());
	LimbIKDefinitionHandle limbHandle = CCrc32::ComputeLowercase(strLimb);
	pLimbIk->AddSetup(limbHandle, targetPositionLocal);
	return 1;
}

void CSkeletonPose::ApplyRecoilAnimation(f32 fDuration, f32 fKinematicImpact, f32 fKickIn, uint32 nArms)
{
	if (!Console::GetInst().ca_UseRecoil)
		return;

	PoseModifier::CRecoil* pRecoil = static_cast<PoseModifier::CRecoil*>(m_recoil.get());
	if (!pRecoil)
	{
		CryCreateClassInstance<IAnimationPoseModifier>(PoseModifier::CRecoil::GetCID(), m_recoil);
		pRecoil = static_cast<PoseModifier::CRecoil*>(m_recoil.get());
		assert(pRecoil);
	}

	PoseModifier::CRecoil::State state;
	state.time = 0.0f;
	state.duration = fDuration;
	state.strengh = fKinematicImpact; //recoil in cm
	state.kickin = fKickIn;           //recoil in cm
	state.displacerad = m_fDisplaceRadiant;
	state.arms = nArms; //1-right arm  2-left arm   3-both
	pRecoil->SetState(state);

	m_fDisplaceRadiant += 0.9f;
	if (m_fDisplaceRadiant > gf_PI)
		m_fDisplaceRadiant -= gf_PI * 2;
};

float CSkeletonPose::GetExtent(EGeomForm eForm)
{
	CGeomExtent& extent = m_Extents.Make(eForm);
	if (!extent.TotalExtent())
	{
		extent.Clear();
		extent.ReserveParts(m_arrCGAJoints.size());

		for (auto& joint : m_arrCGAJoints)
		{
			if (joint.m_CGAObjectInstance)
				extent.AddPart(joint.m_CGAObjectInstance->GetExtent(eForm));
			else
				extent.AddPart(0.f);
		}
	}

	return extent.TotalExtent();
}

void CSkeletonPose::GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const
{
	CGeomExtent const& ext = m_Extents[eForm];
	for (auto part : ext.RandomPartsAliasSum(points, seed))
	{
		if (part.iPart < m_arrCGAJoints.size())
		{
			CCGAJoint const* pJoint = &m_arrCGAJoints[part.iPart];
			pJoint->m_CGAObjectInstance->GetRandomPoints(part.aPoints, seed, eForm);
			for (auto& point : part.aPoints)
				point <<= QuatTS(GetPoseData().GetJointAbsolute(part.iPart));
		}
		else
			part.aPoints.fill(ZERO);
	}
}

//--------------------------------------------------------------------
//---              hex-dump the skeleton                           ---
//--------------------------------------------------------------------

void CSkeletonPose::ExportSkeleton()
{
	/*
	   FILE* pFile = ::fopen("e:/test.txt", "w");
	   if (!pFile)
	    return;

	   ::fprintf(pFile,
	    "struct Joint\n"
	    "{\n"
	    "\tconst char* name;\n"
	    "\tunsigned int nameCrc32;\n"
	    "\tunsigned int nameCrc32Lowercase;\n"
	    "\tsigned int parent;\n"
	    "\tfloat tx, ty, tz;\n"
	    "\tfloat qx, qy, qz, qw;\n"
	    "};\n"
	    "const Joint joints[] =\n"
	    "{\n");

	   uint32 jointCount = m_pDefaultSkeleton->GetJointCount();
	   for (uint32 i=0; i<jointCount; ++i)
	   {
	    const char* name = m_pDefaultSkeleton->GetJointNameByID(i);
	    int32 parent = m_pDefaultSkeleton->GetJointParentIDByID(i);
	    uint32 crc32Normal = m_pDefaultSkeleton->m_arrModelJoints[i].m_nJointCRC32;
	    uint32 crc32Lower = m_pDefaultSkeleton->m_arrModelJoints[i].m_nJointCRC32Lower;
	    const QuatT& location = m_pDefaultSkeleton->m_poseData.GetJointsRelative()[i];
	    ::fprintf(pFile,
	      "\t{ \"%s\", 0x%x, 0x%x, %d, %ff, %ff, %ff, %ff, %ff, %ff, %ff },\n",
	      name, crc32Normal, crc32Lower, parent,
	      location.t.x, location.t.y, location.t.z,
	      location.q.v.x, location.q.v.y, location.q.v.z, location.q.w);
	   }

	   ::fprintf(pFile, "};\n");
	   ::fclose(pFile);
	 */
}

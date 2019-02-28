// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FeetLock.h"

#include <CryExtension/ClassWeaver.h>
#include <CryExtension/ICryFactoryRegistryImpl.h>
#include <CryExtension/CryCreateClassInstance.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include "../CharacterInstance.h"
#include "../Model.h"
#include "../CharacterManager.h"
#include "PoseModifierHelper.h"

/*

   CFeetPoseStore

 */

CRYREGISTER_CLASS(CFeetPoseStore)

// IAnimationPoseModifier
bool CFeetPoseStore::Execute(const SAnimationPoseModifierParams& params)
{
	DEFINE_PROFILER_FUNCTION();

	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return false;

	const CDefaultSkeleton& rDefaultSkeleton = (const CDefaultSkeleton&)params.GetIDefaultSkeleton();
	QuatT* pRelPose = pPoseData->GetJointsRelative();
	QuatT* pAbsPose = pPoseData->GetJointsAbsolute();

	for (uint32 h = 0; h < MAX_FEET_AMOUNT; h++)
	{
		m_pFeetData[h].m_IsEndEffector = 0;
		uint32 hinit = rDefaultSkeleton.m_strFeetLockIKHandle[h].size();
		if (hinit == 0)
			continue;
		const char* strLIKSolver = rDefaultSkeleton.m_strFeetLockIKHandle[h].c_str();
		LimbIKDefinitionHandle nHandle = CCrc32::ComputeLowercase(strLIKSolver);
		int32 idxDefinition = rDefaultSkeleton.GetLimbDefinitionIdx(nHandle);
		if (idxDefinition < 0)
			continue;
		const IKLimbType& rIKLimbType = rDefaultSkeleton.m_IKLimbTypes[idxDefinition];
		uint32 numLinks = rIKLimbType.m_arrRootToEndEffector.size();
		int32 nRootTdx = rIKLimbType.m_arrRootToEndEffector[0];
		QuatT qWorldEndEffector = pRelPose[nRootTdx];
		for (uint32 i = 1; i < numLinks; i++)
		{
			int32 cid = rIKLimbType.m_arrRootToEndEffector[i];
			qWorldEndEffector = qWorldEndEffector * pRelPose[cid];
			qWorldEndEffector.q.Normalize();
		}
		m_pFeetData[h].m_WorldEndEffector = qWorldEndEffector;
		assert(m_pFeetData[h].m_WorldEndEffector.IsValid());
		m_pFeetData[h].m_IsEndEffector = 1;
	}

#ifdef _DEBUG
	uint32 numJoints = rDefaultSkeleton.GetJointCount();
	for (uint32 j = 0; j < numJoints; j++)
	{
		assert(pRelPose[j].q.IsUnit());
		assert(pAbsPose[j].q.IsUnit());
		assert(pRelPose[j].IsValid());
		assert(pAbsPose[j].IsValid());
	}
#endif

	return false;
}

/*

   CFeetPoseRestore

 */

CRYREGISTER_CLASS(CFeetPoseRestore)

// IAnimationPoseModifier
bool CFeetPoseRestore::Execute(const SAnimationPoseModifierParams& params)
{
	DEFINE_PROFILER_FUNCTION();

	Skeleton::CPoseData* pPoseData = Skeleton::CPoseData::GetPoseData(params.pPoseData);
	if (!pPoseData)
		return false;

	const CDefaultSkeleton& rDefaultSkeleton = (const CDefaultSkeleton&)params.GetIDefaultSkeleton();
	QuatT* const __restrict pAbsPose = pPoseData->GetJointsAbsolute();
	QuatT* const __restrict pRelPose = pPoseData->GetJointsRelative();

	for (uint32 i = 0; i < MAX_FEET_AMOUNT; i++)
	{
		if (m_pFeetData[i].m_IsEndEffector == 0)
			continue;
		const char* strLIKSolver = rDefaultSkeleton.m_strFeetLockIKHandle[i].c_str();
		LimbIKDefinitionHandle nHandle = CCrc32::ComputeLowercase(strLIKSolver);
		int32 idxDefinition = rDefaultSkeleton.GetLimbDefinitionIdx(nHandle);
		if (idxDefinition < 0)
			continue;
		assert(m_pFeetData->m_WorldEndEffector.IsValid());
		const IKLimbType& rIKLimbType = rDefaultSkeleton.m_IKLimbTypes[idxDefinition];
		uint32 numLinks = rIKLimbType.m_arrRootToEndEffector.size();
		int32 lFootParentIdx = rIKLimbType.m_arrRootToEndEffector[numLinks - 2];
		int32 lFootIdx = rIKLimbType.m_arrRootToEndEffector[numLinks - 1];
		PoseModifierHelper::IK_Solver(PoseModifierHelper::GetDefaultSkeleton(params), nHandle, m_pFeetData[i].m_WorldEndEffector.t, *pPoseData);
		pAbsPose[lFootIdx] = m_pFeetData[i].m_WorldEndEffector;
		pRelPose[lFootIdx] = pAbsPose[lFootParentIdx].GetInverted() * m_pFeetData[i].m_WorldEndEffector;
	}

#ifdef _DEBUG
	uint32 numJoints = rDefaultSkeleton.GetJointCount();
	for (uint32 j = 0; j < numJoints; j++)
	{
		assert(pRelPose[j].q.IsUnit());
		assert(pAbsPose[j].q.IsUnit());
		assert(pRelPose[j].IsValid());
		assert(pAbsPose[j].IsValid());
	}
#endif

	return true;
}

/*

   CFeetLock

 */

CFeetLock::CFeetLock()
{
	CryCreateClassInstance(CFeetPoseStore::GetCID(), m_store);
	assert(m_store.get());

	CFeetPoseStore* pStore = static_cast<CFeetPoseStore*>(m_store.get());
	pStore->m_pFeetData = &m_FeetData[0];

	CryCreateClassInstance(CFeetPoseRestore::GetCID(), m_restore);
	assert(m_restore.get());

	CFeetPoseRestore* pRestore = static_cast<CFeetPoseRestore*>(m_restore.get());
	pRestore->m_pFeetData = &m_FeetData[0];
}

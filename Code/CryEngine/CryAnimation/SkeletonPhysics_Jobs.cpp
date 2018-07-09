// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SkeletonPhysics.h"

#include "Model.h"
#include "CharacterInstance.h"
#include "CharacterManager.h"
#include "DrawHelper.h"

// Physics

// given the bone index, (INDEX, NOT ID), returns this bone's parent index
uint32 CSkeletonPhysics::getBoneParentIndex(uint32 nBoneIndex) const
{
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	int parentIndex = rDefaultSkeleton.m_arrModelJoints[nBoneIndex].m_idxParent;
	if (parentIndex < 0)
		parentIndex = 0;  // -1 clamped to 0 ????
	return parentIndex;
}

void CSkeletonPhysics::ResetNonphysicalBoneRotations(Skeleton::CPoseData& poseData, int nLod, float fBlend)
{
	QuatT* const __restrict pJointRelative = poseData.GetJointsRelative();
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;

	// set non-physical bones to their default position wrt parent for LODs>0
	// do it for every bone except the root, parents first
	uint32 numBones = m_arrPhysicsJoints.size();
	//	assert(numBones); having 0 physical bones is a legal case

	for (uint32 nBone = 1; nBone < numBones; ++nBone)
	{
		const CDefaultSkeleton::SJoint& joint = rDefaultSkeleton.m_arrModelJoints[nBone];

		//		if (m_pSkeletonAnim->m_IsAnimPlaying || m_bPhysicsRelinquished)
		{
			if (joint.m_PhysInfo.pPhysGeom)
				continue;
		}

		const CPhysicsJoint& physicsJoint = m_arrPhysicsJoints[nBone];
		const QuatT &qDef = physicsJoint.m_DefaultRelativeQuat;
		if (fBlend >= 1.0f)
		{
			pJointRelative[nBone] = qDef;
		}
		else
		{
			QuatT g(physicsJoint.m_qRelFallPlay, physicsJoint.m_DefaultRelativeQuat.t);
			pJointRelative[nBone].SetNLerp(g, qDef, fBlend);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Multiplies each bone global matrix with the parent global matrix,
// and calculates the relative-to-default-pos matrix. This is essentially
// the process opposite to conversion of global matrices to relative form
// performed by ConvertBoneGlobalToRelativeMatrices()
// NOTE:
//   The root matrix is relative to the world, which is its parent, so it's
//   obviously both the global and relative (they coincide), so it doesn't change
//
// PARAMETERS:
//   bNonphysicalOnly - if set to true, only those bones that have no physical geometry are affected
//
// ASSUMES: in each m_matRelativeToParent matrix, upon entry, there's a matrix relative to the parent
// RETURNS: in each bone global matrix, there's the actual global matrix
void CSkeletonPhysics::UnconvertBoneGlobalFromRelativeForm(Skeleton::CPoseData& poseData, bool bNonphysicalOnlyArg, int nLod, bool bRopeTipsOnly)
{
	uint32 jointCount = poseData.GetJointCount();
	QuatT* const __restrict pJointRelative = poseData.GetJointsRelative();
	QuatT* const __restrict pJointAbsolute = poseData.GetJointsAbsolute();
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;

	uint32 nRopeLevel = 0;
	bool bNonphysicalOnly = true;

	const CDefaultSkeleton::SJoint* pModelJoints = &rDefaultSkeleton.m_arrModelJoints[0];

	// start from 1, since we don't affect the root, which is #0
	for (uint32 i = 1; i < jointCount; ++i)
	{
		const CDefaultSkeleton::SJoint& modelJoint = pModelJoints[i];

		bool bPhysical = modelJoint.m_PhysInfo.pPhysGeom != 0;

		if ((modelJoint.m_PhysInfo.flags & 0xFFFF0000) == 0x30000)
		{
			nRopeLevel = modelJoint.m_numLevels;
		}
		else
		{
			if (modelJoint.m_numLevels < nRopeLevel)
				nRopeLevel = 0;

			// -2 is for rope bones, never force their positions
			if ((!bNonphysicalOnly || !bPhysical) && (!bRopeTipsOnly || nRopeLevel))
			{
				// CRAIG - gross hack - must be fixed properly
				if (modelJoint.m_idxParent == -1)
					continue;
				// END GROSS HACK

				pJointAbsolute[i] = pJointAbsolute[getBoneParentIndex(i)] * pJointRelative[i];
			}
		}

		if (bPhysical) // don't update the 1st physical bone (pelvis) even if bNonphysicalOnlyArg is false
			bNonphysicalOnly = bNonphysicalOnlyArg;
	}
}

// finds the first physicalized parent of the given bone (bone given by index)
int CSkeletonPhysics::getBonePhysParentIndex(int iBoneIndex, int nLod)
{
	int iPrevBoneIndex;
	do
	{
		iBoneIndex = getBoneParentIndex(iPrevBoneIndex = iBoneIndex);
	}
	while (iBoneIndex != iPrevBoneIndex && !GetModelJointPointer(iBoneIndex)->m_PhysInfo.pPhysGeom);

	return iBoneIndex == iPrevBoneIndex ? -1 : iBoneIndex;
}

// Entity

void CSkeletonPhysics::Job_Physics_SynchronizeFromEntityPrepare(Memory::CPool& memoryPool, IPhysicalEntity* pPhysicalEntity)
{
	DEFINE_PROFILER_FUNCTION();

	m_bPhysicsSynchronizeFromEntity = false;
	if (!m_bPhysicsRelinquished)
		return;
	if (!pPhysicalEntity && !m_nAuxPhys)
		return;

	CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;

	// Query for physical entity awakeness

	if (pPhysicalEntity)
	{
		pe_status_awake statusTmp2;
		statusTmp2.lag = 2 + g_pCharacterManager->m_nUpdateCounter - m_idLastSyncFrame;
		m_bPhysicsAwake = pPhysicalEntity->GetStatus(&statusTmp2) != 0;
	}

	for (int i = 0; i < m_nAuxPhys; i++)
	{
		pe_status_awake statusTmp;
		m_bPhysicsAwake |= m_auxPhys[i].pPhysEnt->GetStatus(&statusTmp) != 0;
	}

	if (!m_bPhysicsAwake && !m_bPhysicsWasAwake)
		return;

	if (!pPhysicalEntity)
		return;

	m_bPhysicsSynchronizeFromEntity = true;

	uint32 jointCount = rDefaultSkeleton.GetJointCount();

	int nLod = (m_pCharPhysics != pPhysicalEntity || m_bPhysicsRelinquished) ? GetPhysicsLod() : 0;

	m_pPhysBuffer = (PhysData*)memoryPool.Allocate(jointCount * sizeof(PhysData));
	m_bPhysBufferFilled = false;

	CDefaultSkeleton::SJoint* pModelJoints = &rDefaultSkeleton.m_arrModelJoints[0];
	pe_status_pos partpos;
	partpos.flags = status_local;
	float rscale = 1 / m_fScale;
	for (uint32 i = 0; i < jointCount; ++i)
	{
		m_pPhysBuffer[i].bSet = false;

		if (!pModelJoints[i].m_PhysInfo.pPhysGeom)
			continue;

		partpos.partid = i;
		int32 status = pPhysicalEntity->GetStatus(&partpos);
		if (!status)
			continue;

		m_pPhysBuffer[i].bSet = true;
		m_pPhysBuffer[i].location.q = partpos.q;
		m_pPhysBuffer[i].location.t = partpos.pos * rscale;
		if (m_fPhysBlendTime < m_fPhysBlendMaxTime)
		{
			// if in blending stage, do it only for the root
			for (++i; i < jointCount; ++i)
				m_pPhysBuffer[i].bSet = false;
			break;
		}
	}

	pe_status_joint sj;
	for (uint32 i = 0; i < jointCount; ++i)
	{
		m_pPhysBuffer[i].bSet2 = false;

		if (!pModelJoints[i].m_PhysInfo.pPhysGeom)
			continue;

		sj.idChildBody = i;
		int32 status = pPhysicalEntity->GetStatus(&sj);
		if (!status)
			continue;

		m_pPhysBuffer[i].bSet2 = true;
		m_pPhysBuffer[i].rotation = sj.quat0;
		m_pPhysBuffer[i].angles = sj.q + sj.qext;
	}

	pe_status_pos sp;
	int status2 = pPhysicalEntity->GetStatus(&sp);
	if (status2 == 0)
	{
		g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, rDefaultSkeleton.GetModelFilePath(), "GetStatus() returned 0");
		assert(0);
	}

	if (!m_bPhysicsAwake)
		m_fPhysBlendTime = m_fPhysBlendMaxTime + 0.1f;
}

void CSkeletonPhysics::Job_Physics_SynchronizeFromEntity(Skeleton::CPoseData& poseData, IPhysicalEntity* pPhysicalEntity, QuatT offset)
{
	DEFINE_PROFILER_FUNCTION();
	if (!m_bPhysicsSynchronizeFromEntity)
		return;
	if (!m_pPhysBuffer)
		return;
	CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;

	m_bPhysicsAwake = false;

	int nLod = (m_pCharPhysics != pPhysicalEntity || m_bPhysicsRelinquished) ? GetPhysicsLod() : 0;
	ResetNonphysicalBoneRotations(poseData, nLod, m_fPhysBlendTime * m_frPhysBlendMaxTime);

	QuatT roffs(!offset.q, -offset.t * offset.q);

	uint jointCount = poseData.GetJointCount();

	QuatT* const __restrict pJointRelative = poseData.GetJointsRelative();
	QuatT* const __restrict pJointAbsolute = poseData.GetJointsAbsolute();
	CPhysicsJoint* const __restrict pPhysJoints = &m_arrPhysicsJoints[0];
	PhysData* const pPhysBuffer = &m_pPhysBuffer[0];

	if (pPhysicalEntity)
	{
		QuatT transform(IDENTITY);

		CDefaultSkeleton::SJoint* pModelJoints = &rDefaultSkeleton.m_arrModelJoints[0];
		QuatT location;
		for (uint i = 0; i < jointCount; ++i)
		{
			PhysData& rCurPhysData = pPhysBuffer[i];
			location = pJointAbsolute[i];

			if (rCurPhysData.bSet)
			{
				pJointAbsolute[i] = roffs * rCurPhysData.location;
			}

			if (rCurPhysData.bSet2)
			{
				pJointRelative[i].q = pPhysJoints[i].m_qRelPhysParent * rCurPhysData.rotation * Quat::CreateRotationXYZ(rCurPhysData.angles);
				pJointRelative[i].t = pPhysJoints[i].m_DefaultRelativeQuat.t;
			}

			rCurPhysData.location = location;

			if (i == 1)
				transform = pJointAbsolute[i] * location.GetInverted();
		}

		for (uint i = 0; i < jointCount; ++i)
			pPhysBuffer[i].location = transform * pPhysBuffer[i].location;

		// absolute to relative
		for (uint i = 0; i < jointCount; ++i)
		{
			uint index = jointCount - 1 - i;
			int parent = rDefaultSkeleton.m_arrModelJoints[index].m_idxParent;
			if (parent > -1)
			{
				m_pPhysBuffer[index].location = m_pPhysBuffer[parent].location.GetInverted() * m_pPhysBuffer[index].location;
			}
		}

		int physicsLod = 0;
		for (uint i = 0; i < jointCount; ++i)
		{
			int parentIndex = getBonePhysParentIndex(i, physicsLod);
			if (parentIndex < 0)
				continue;

			Quat parentMatrix(IDENTITY);
			for (int k = getBoneParentIndex(i); k != parentIndex; k = getBoneParentIndex(k))
				parentMatrix = m_pPhysBuffer[k].location.q * parentMatrix;

			Quat physicsJointFrame(IDENTITY);
			CryBonePhysics& physInfo = GetModelJointPointer(i)->m_PhysInfo;
			if (physInfo.flags != -1 &&
			    physInfo.framemtx[0][0] < 10)
			{
				physicsJointFrame = Quat(*(Matrix33*)&physInfo.framemtx[0][0]);
			}

			m_pPhysBuffer[i].angles = Ang3::GetAnglesXYZ(!physicsJointFrame * parentMatrix * m_pPhysBuffer[i].location.q);
		}
	}

	UnconvertBoneGlobalFromRelativeForm(poseData, m_fPhysBlendTime >= m_fPhysBlendMaxTime, nLod);
	poseData.ComputeRelativePose(rDefaultSkeleton);

	m_bPhysicsWasAwake = m_bPhysicsAwake;
	m_idLastSyncFrame = g_pCharacterManager->m_nUpdateCounter;
}

void CSkeletonPhysics::Job_Physics_SynchronizeFromEntityArticulated(Skeleton::CPoseData& poseData, float fDeltaTimePhys)
{
	if (m_bPhysicsRelinquished)
	{
		m_bPhysicsWasAwake = true;

		Job_Physics_SynchronizeFromEntity(poseData, m_pCharPhysics, QuatT(IDENTITY));

		m_fPhysBlendTime += fDeltaTimePhys;
	}
}

void CSkeletonPhysics::Job_Physics_SynchronizeFrom(Skeleton::CPoseData& poseData, float timeDelta)
{
	DEFINE_PROFILER_FUNCTION();

	if (Console::GetInst().DrawPose('p'))
		DrawHelper::Pose(*m_pInstance->m_pDefaultSkeleton, poseData, QuatT(m_location), ColorB(0xff, 0xff, 0x80, 0xff));

	if (!m_pSkeletonAnim->m_IsAnimPlaying && !m_bPhysicsRelinquished)
	{
		Job_Physics_SynchronizeFromAux(poseData);
		return;
	}

	Job_Physics_SynchronizeFromEntityArticulated(poseData, timeDelta);
	Job_Physics_SynchronizeFromAux(poseData);
	Job_Physics_SynchronizeFromImpact(poseData, timeDelta);
	Job_Physics_BlendFromRagdoll(poseData, timeDelta);
}

// Aux

void CSkeletonPhysics::Job_Physics_SynchronizeFromAuxPrepare(Memory::CPool& memoryPool)
{
	m_bPhysicsSynchronizeAux = false;
	m_pPhysAuxBuffer = NULL;
	if (!m_nAuxPhys)
		return;

	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	m_pPhysAuxBuffer = (PhysAuxData*)memoryPool.Allocate(m_nAuxPhys * sizeof(PhysAuxData));
	if (!m_pPhysBuffer)
	{
		uint32 jointCount = rDefaultSkeleton.GetJointCount();
		m_pPhysBuffer = (PhysData*)memoryPool.Allocate(jointCount * sizeof(PhysData));
		m_bPhysBufferFilled = false;
	}

	int bAwake = 0;
	for (int j = 0; j < m_nAuxPhys; ++j)
	{
		m_pPhysAuxBuffer[j].bSet = false;
		m_pPhysAuxBuffer[j].matrix = QuatT(IDENTITY);

		const pe_type peType = m_auxPhys[j].pPhysEnt->GetType();
		if (peType == PE_SOFT)
		{
			pe_status_softvtx ssv;
			if (m_auxPhys[j].pPhysEnt->GetStatus(&ssv) && m_auxPhys[j].pVtx)
			{
				m_pPhysAuxBuffer[j].bSet = true;
				m_pPhysAuxBuffer[j].pPoints = m_auxPhys[j].pVtx;
				for (int i = 0; i <= m_auxPhys[j].nBones; i++)
					m_pPhysAuxBuffer[j].pPoints[i] = ssv.pVtx[i];
				m_pPhysAuxBuffer[j].matrix = QuatT(ssv.qHost, ssv.posHost);
			}
		}
		else
		{
			pe_status_rope sr;
			sr.pPoints = NULL;
			sr.pVtx = NULL;

			if (!m_auxPhys[j].pPhysEnt->GetStatus(&sr))
				continue;

			m_pPhysAuxBuffer[j].matrix = QuatT(sr.qHost, sr.posHost);

			sr.pPoints = m_auxPhys[j].pVtx;
			m_auxPhys[j].pPhysEnt->GetStatus(&sr);
			m_pPhysAuxBuffer[j].bSet = !(sr.bTargetPoseActive == 1 && sr.stiffnessAnim == 0);
			if (!m_auxPhys[j].bTied0)
				m_pPhysAuxBuffer[j].matrix = QuatT(Quat(IDENTITY), sr.pPoints[0]);

			m_pPhysAuxBuffer[j].pPoints = sr.pPoints;
		}

		pe_status_awake statusTmp;
		bAwake |= m_auxPhys[j].pPhysEnt->GetStatus(&statusTmp);
	}

	if (m_bPhysicsRelinquished && !bAwake)
		return;

	m_bPhysicsSynchronizeAux = true;
}

void CSkeletonPhysics::Job_Physics_SynchronizeFromAux(Skeleton::CPoseData& poseData)
{
	if (!m_bPhysicsSynchronizeAux)
		return;
	if (!m_pPhysAuxBuffer)
		return;
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;

	int nLod = m_bPhysicsRelinquished ? GetPhysicsLod() : 0;

	if (m_bPhysicsSynchronizeFromEntity &&
	    !m_pSkeletonAnim->m_IsAnimPlaying && !m_bPhysicsRelinquished)
	{
		ResetNonphysicalBoneRotations(poseData, nLod, 2.0f);
		poseData.ComputeAbsolutePose(rDefaultSkeleton);
	}

	uint32 jointCount = poseData.GetJointCount();

	QuatT* const __restrict pJointRelative = poseData.GetJointsRelative();
	QuatT* const __restrict pJointAbsolute = poseData.GetJointsAbsolute();

	// IZF: We have to buffer this back because we need the locations of the
	// joints before they are modified to blend whatever animation was playing
	// with the physics rope simulation.
	if (m_pPhysBuffer)
	{
		for (uint32 i = 0; i < jointCount; ++i)
			(m_pPhysBuffer[i].location = pJointRelative[i]).t *= m_fScale;
		m_bPhysBufferFilled = true;
	}

	PhysAuxData* const pPhysAuxBuffer = &m_pPhysAuxBuffer[0];

	for (int j = 0; j < m_nAuxPhys; ++j)
	{
		const PhysAuxData& rCurPhysAuxData = pPhysAuxBuffer[j];

		if (!rCurPhysAuxData.bSet || !m_auxPhys[j].nBones)
			continue;

		aux_phys_data& r_cur_aux_phys_data = m_auxPhys[j];

		int iRoot;
		QuatT qparent(IDENTITY), qparent0(IDENTITY), qrphys = QuatT(rCurPhysAuxData.matrix).GetInverted();
		if (r_cur_aux_phys_data.bTied0)
		{
			iRoot = getBonePhysParentIndex(r_cur_aux_phys_data.iBoneTiedTo[0], nLod);
			for (int i = rDefaultSkeleton.m_arrModelJoints[r_cur_aux_phys_data.iBoneTiedTo[0]].m_idxParent; i != iRoot; i = rDefaultSkeleton.m_arrModelJoints[i].m_idxParent)
				qparent = pJointRelative[i] * qparent;
			qparent0 = qparent;
		}
		else if ((iRoot = rDefaultSkeleton.m_arrModelJoints[r_cur_aux_phys_data.pauxBoneInfo[0].iBone].m_idxParent) >= 0)
		{
			qparent0 = qparent = QuatT(r_cur_aux_phys_data.pauxBoneInfo[0].quat0, pJointAbsolute[r_cur_aux_phys_data.pauxBoneInfo[0].iBone].t).GetInverted() * pJointAbsolute[iRoot];
		}

		int nAuxBones = r_cur_aux_phys_data.nBones;
		for (int i = 0; i < nAuxBones; ++i)
		{
			aux_bone_info& rAuxBoneInfo = r_cur_aux_phys_data.pauxBoneInfo[i];
			if (i > 0 && rDefaultSkeleton.m_arrModelJoints[rAuxBoneInfo.iBone].m_idxParent != r_cur_aux_phys_data.pauxBoneInfo[i - 1].iBone)
				qparent = qparent0;
			Vec3 dir = qrphys.q * (rCurPhysAuxData.pPoints[i + 1] - rCurPhysAuxData.pPoints[i]);
			float len = dir.GetLength();
			Vec3 v0 = r_cur_aux_phys_data.pauxBoneInfo[i].dir0;
			Vec3 v1 = len > 1E-5f ? dir / len : v0;
			Quat q0 = r_cur_aux_phys_data.pauxBoneInfo[i].quat0;

			QuatT& matRel = pJointRelative[rAuxBoneInfo.iBone];
			matRel.q = (!qparent.q * Quat::CreateRotationV0V1(v0, v1) * q0).GetNormalized();
			matRel.t = qparent.GetInverted() * qrphys * rCurPhysAuxData.pPoints[i];
			qparent = qparent * matRel;
			pJointAbsolute[rAuxBoneInfo.iBone] = rDefaultSkeleton.m_arrModelJoints[rAuxBoneInfo.iBone].m_idxParent >= 0 ?
			                                     pJointAbsolute[rDefaultSkeleton.m_arrModelJoints[rAuxBoneInfo.iBone].m_idxParent] * matRel : matRel;
		}

		int iParent = r_cur_aux_phys_data.pauxBoneInfo[nAuxBones - 1].iBone;
		if (GetModelJointPointer(iParent)->m_numChildren > 0)
		{
			pJointRelative[iParent = GetModelJointChildIndex(iParent, 0)].t = qparent.GetInverted() * qrphys * rCurPhysAuxData.pPoints[nAuxBones];
			for (; iParent >= 0; iParent = GetModelJointPointer(iParent)->m_numChildren > 0 ? GetModelJointChildIndex(iParent, 0) : -1)
				pJointAbsolute[iParent] = pJointAbsolute[rDefaultSkeleton.m_arrModelJoints[iParent].m_idxParent] * pJointRelative[iParent];
		}
	}
}

//

void CSkeletonPhysics::Job_Physics_SynchronizeFromImpactPrepare(Memory::CPool& memoryPool)
{
	m_pPhysImpactBuffer = NULL;
	if (!m_pCharPhysics)
		return;
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;

	if (m_bPhysicsRelinquished)
		return;
	pe_status_awake physicsStatusAwake;
	m_bPhysicsAwake = m_pCharPhysics->GetStatus(&physicsStatusAwake) != 0;
	if (!m_bPhysicsAwake)
		return;

	if (!Console::GetInst().ca_physicsProcessImpact)
		return;

	int32 jointCount = rDefaultSkeleton.GetJointCount();
	m_pPhysImpactBuffer = (PhysImpactData*)memoryPool.Allocate(jointCount * sizeof(PhysImpactData));
	if (!m_pPhysImpactBuffer)
		return;

	int physicsLod = m_bPhysicsRelinquished ? GetPhysicsLod() : 0;

	pe_status_joint physicsStatusJoint;
	for (int i = 0; i < jointCount; i++)
	{
		m_pPhysImpactBuffer[i].bSet = false;

		if (!GetModelJointPointer(i)->m_PhysInfo.pPhysGeom)
			continue;

		int parentIndex = getBonePhysParentIndex(i, physicsLod);
		if (parentIndex < 0)
			continue;

		physicsStatusJoint.idChildBody = i;
		if (!m_pCharPhysics->GetStatus(&physicsStatusJoint))
			continue;

		m_pPhysImpactBuffer[i].bSet = true;
		m_pPhysImpactBuffer[i].angles = physicsStatusJoint.q;
	}
}

void CSkeletonPhysics::Job_Physics_SynchronizeFromImpact(Skeleton::CPoseData& poseData, float timeDelta)
{
	if (!m_pPhysImpactBuffer)
		return;

	int physicsLod = m_bPhysicsRelinquished ? GetPhysicsLod() : 0;

	int iRoot = -1, jointCount = poseData.GetJointCount();
	for (int i = 0; i < jointCount; ++i)
	{
		if (!m_pPhysImpactBuffer[i].bSet)
			continue;

		int parentIndex = getBonePhysParentIndex(i, physicsLod);
		if (parentIndex < 0)
			continue;
		iRoot += parentIndex - iRoot & iRoot >> 31;

		Quat parentMatrix(IDENTITY);
		for (int k = getBoneParentIndex(i); k != parentIndex; k = getBoneParentIndex(k))
			parentMatrix = poseData.GetJointRelative(k).q * parentMatrix;

		Quat physicsJointFrame(IDENTITY);
		if (GetModelJointPointer(i)->m_PhysInfo.flags != -1 &&
		    GetModelJointPointer(i)->m_PhysInfo.framemtx[0][0] < 10)
		{
			physicsJointFrame = Quat(*(Matrix33*)&GetModelJointPointer(i)->m_PhysInfo.framemtx[0][0]);
		}

		Ang3 animationAngles = Ang3::GetAnglesXYZ(!physicsJointFrame * parentMatrix * poseData.GetJointRelative(i).q);
		poseData.SetJointRelativeO(i,
		                           !parentMatrix * physicsJointFrame *
		                           Quat::CreateRotationXYZ(m_pPhysImpactBuffer[i].angles + animationAngles));

		// Sync back

		if (m_bPhysicsRelinquished)
			continue;

		int parentNextIndex = parentIndex;
		int j;
		while (
		  (GetModelJointPointer(parentIndex)->m_PhysInfo.flags & all_angles_locked) == all_angles_locked &&
		  (j = getBonePhysParentIndex(parentIndex, physicsLod)) >= 0)
		{
			parentIndex = j;
		}

		bool bUpdateParent = false;
		Quat updateParentMatrix(IDENTITY);
		for (; parentNextIndex != parentIndex; parentNextIndex = getBoneParentIndex(parentNextIndex))
		{
			bUpdateParent = true;
			updateParentMatrix = poseData.GetJointRelative(parentNextIndex).q * updateParentMatrix;
		}

		m_pPhysImpactBuffer[i].angles = animationAngles;
		m_pPhysImpactBuffer[i].pivot =
		  (poseData.GetJointAbsolute(parentIndex).GetInverted() *
		  poseData.GetJointAbsolute(i).t) * m_fScale;
		if (bUpdateParent)
			m_pPhysImpactBuffer[i].q0 = updateParentMatrix * physicsJointFrame;
		else
			MARK_UNUSED m_pPhysImpactBuffer[i].q0;
	}
	if (iRoot >= 0)
		m_pPhysImpactBuffer[iRoot].angles = Ang3::GetAnglesXYZ(poseData.GetJointAbsolute(iRoot).q);

	UnconvertBoneGlobalFromRelativeForm(poseData, false, physicsLod);
}

ILINE Quat SafeInterpolation(const Quat& q0, Quat q1, const float w, bool bFlip)
{
	if (bFlip)
		q1 = -q1;
	return q0 * Quat::exp(Quat::log(!q0 * q1) * w);
}

void CSkeletonPhysics::Job_Physics_BlendFromRagdoll(Skeleton::CPoseData& poseData, float timeDelta)
{
	if (m_timeStandingUp < 0.0f)
		return;
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;

	float t = min(1.0f, m_timeStandingUp);

	uint jointCount = poseData.GetJointCount();

	int rootJointIndex = m_iSpineBone[0];
	int rootJointParentIndex = poseData.GetParentIndex(*m_pInstance->m_pDefaultSkeleton, rootJointIndex);
	if (rootJointParentIndex > -1)
	{
		Vec3 localPhysicsJointRootPosition = m_location.GetInverted() * m_physicsJointRootPosition;
		poseData.GetJointsAbsolute()[rootJointIndex].t.SetLerp(
		  localPhysicsJointRootPosition, poseData.GetJointsAbsolute()[rootJointIndex].t, t);
		poseData.GetJointsRelative()[rootJointIndex].t =
		  poseData.GetJointsAbsolute()[rootJointParentIndex].GetInverted() *
		  poseData.GetJointsAbsolute()[rootJointIndex].t;
	}

	if (m_timeStandingUp == 0.0f)
	{
		m_bBlendFromRagdollFlip = (m_arrPhysicsJoints[rootJointIndex].m_qRelFallPlay | poseData.GetJointsRelative()[rootJointIndex].q) < 0.0f;
	}

	poseData.GetJointsRelative()[0].q = SafeInterpolation(
	  m_arrPhysicsJoints[0].m_qRelFallPlay, poseData.GetJointsRelative()[0].q, t, false);
	poseData.GetJointsAbsolute()[0].q = poseData.GetJointsRelative()[0].q;

	for (uint i = 1; i < jointCount; ++i)
	{
		if (i > uint(rootJointIndex))
		{
			poseData.GetJointsRelative()[i].q.SetNlerp(
			  m_arrPhysicsJoints[i].m_qRelFallPlay, poseData.GetJointsRelative()[i].q, t);
		}
		else
		{
			poseData.GetJointsRelative()[i].q = SafeInterpolation(
			  m_arrPhysicsJoints[i].m_qRelFallPlay, poseData.GetJointsRelative()[i].q, t, i == rootJointIndex ? m_bBlendFromRagdollFlip : false);
		}
		poseData.GetJointsAbsolute()[i] = poseData.GetJointsAbsolute()[rDefaultSkeleton.m_arrModelJoints[i].m_idxParent] * poseData.GetJointsRelative()[i];
	}

	m_timeStandingUp += timeDelta;
	if (m_timeStandingUp > 2.0f)
		m_timeStandingUp = -1.0f;
}

//

void CSkeletonPhysics::Job_SynchronizeWithPhysicsPrepare(Memory::CPool& memoryPool)
{
	m_bPhysicsSynchronize = false;
	m_bPhysicsSynchronizeFromEntity = false;

	m_bPhysicsSynchronizeAux = false;

	m_pPhysBuffer = NULL;
	m_pPhysAuxBuffer = NULL;
	m_pPhysImpactBuffer = NULL;

	m_fPhysBlendTime += m_pInstance->m_fDeltaTime;
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;

	if (m_pCharPhysics && m_bPhysicsRelinquished && m_pSkeletonAnim->m_IsAnimPlaying)
	{
		pe_action_awake awake;
		m_pCharPhysics->Action(&awake);
	}

	// Determine the need for a physics sync.
	bool bSyncPhysics =
	  m_bFullSkeletonUpdate &&
	  rDefaultSkeleton.m_ObjectType == CHR &&
	  (gEnv->pPhysicalWorld->GetPhysVars()->lastTimeStep > 0 || m_pInstance->m_CharEditMode) &&
	  Console::GetInst().ca_UsePhysics;

	if (!bSyncPhysics)
		return;

	m_bPhysicsSynchronize = true;

	Job_Physics_SynchronizeFromEntityPrepare(memoryPool, m_pCharPhysics);
	Job_Physics_SynchronizeFromAuxPrepare(memoryPool);
	Job_Physics_SynchronizeFromImpactPrepare(memoryPool);
}

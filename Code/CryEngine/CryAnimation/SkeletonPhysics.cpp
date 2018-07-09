// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SkeletonPhysics.h"

#include "Model.h"
#include "CharacterInstance.h"
#include "CharacterManager.h"
#include "CharacterInstanceProcessing.h"
#include "DrawHelper.h"

CSkeletonPhysics::CSkeletonPhysics()
	: m_pCharPhysics(nullptr)
	, m_ppBonePhysics(nullptr)
	, m_timeStandingUp(0.0f)
	, m_bBlendFromRagdollFlip(false)
	, m_bHasPhysics(false)
	, m_bHasPhysicsProxies(false)
	, m_bPhysicsAwake(false)
	, m_bPhysicsWasAwake(false)
	, m_bPhysicsRelinquished(false)
	, m_location(IDENTITY)
	, m_pPrevCharHost(nullptr)
	, m_physicsJointRootPosition(ZERO)
	, m_pPhysBuffer(nullptr)
	, m_bPhysicsSynchronize(false)
	, m_bPhysicsSynchronizeFromEntity(false)
	, m_bPhysBufferFilled(false)
	, m_pPhysAuxBuffer(nullptr)
	, m_bPhysicsSynchronizeAux(false)
	, m_pPhysImpactBuffer(nullptr)
	, m_vOffset(ZERO)
	, m_nSpineBones(0)
	, m_nAuxPhys(0)
	, m_iSurfaceIdx(0)
	, m_fPhysBlendTime(0.0f)
	, m_fPhysBlendMaxTime(0.0f)
	, m_frPhysBlendMaxTime(0.0f)
	, m_stiffnessScale(0.0f)
	, m_fScale(0.0f)
	, m_fMass(0.0f)
	, m_prevPosPivot(ZERO)
	, m_velPivot(ZERO)
	, m_nPhysJoints(0)
	, m_pPhysUpdateValidator(nullptr)
	, m_pInstance(nullptr)
	, m_pSkeletonPose(nullptr)
	, m_pSkeletonAnim(nullptr)
	, m_bLimpRagdoll(false)
	, m_bSetDefaultPoseExecute(false)
	, m_bFullSkeletonUpdate(false)
	, m_bForcePostSynchronization(false)
	, m_arrCGAJoints(nullptr)
{
	ZeroArray(m_iSpineBone);
}

CSkeletonPhysics::~CSkeletonPhysics()
{
	if (m_pPhysUpdateValidator)
	{
		{
			WriteLock loc(m_pPhysUpdateValidator->lock);
			m_pPhysUpdateValidator->bValid = 0;
		}
		m_pPhysUpdateValidator->Release();
	}
}

//

bool CSkeletonPhysics::Initialize(CSkeletonPose& skeletonPose)
{
	m_location = QuatTS(IDENTITY);

	m_pSkeletonPose = NULL;
	m_pSkeletonAnim = NULL;
	m_pInstance = NULL;

	m_ppBonePhysics = NULL;
	m_timeStandingUp = -1.0f;

	m_pCharPhysics = NULL;
	m_pPrevCharHost = NULL;
	m_pPhysBuffer = NULL;
	m_bPhysicsSynchronize = false;
	m_bPhysicsSynchronizeFromEntity = false;
	m_bPhysBufferFilled = false;

	m_pPhysAuxBuffer = NULL;
	m_bPhysicsSynchronizeAux = false;

	m_pPhysImpactBuffer = NULL;

	m_vOffset = Vec3(0.0f, 0.0f, 0.0f);
	//	m_iSpineBone[3];
	m_nSpineBones = 0;
	m_nAuxPhys = 0;
	m_iSurfaceIdx = 0;
	m_fPhysBlendTime = 1E6f;
	m_fPhysBlendMaxTime = 1.0f;
	m_frPhysBlendMaxTime = 1.0f;
	m_stiffnessScale = 0.0f;
	m_fScale = 0.01f;
	m_fMass = 0.0f;
	m_prevPosPivot.zero();
	m_velPivot.zero();
	//	m_physJointsIdx;
	m_nPhysJoints = -1;
	m_pPhysUpdateValidator = NULL;

	m_bPhysicsRelinquished = false;
	m_bHasPhysics = false;
	m_bHasPhysicsProxies = false;
	m_bPhysicsAwake = false;
	m_bPhysicsWasAwake = 1;
	m_bLimpRagdoll = false;
	m_bSetDefaultPoseExecute = false;

	m_bFullSkeletonUpdate = true;
	m_bForcePostSynchronization = false;

	m_arrCGAJoints = NULL;

	m_pSkeletonPose = &skeletonPose;
	m_pSkeletonAnim = skeletonPose.m_pSkeletonAnim;
	m_pInstance = skeletonPose.m_pInstance;
	m_arrCGAJoints = &skeletonPose.m_arrCGAJoints;
	return true;
}

//

const Skeleton::CPoseData& CSkeletonPhysics::GetPoseData() const            { return m_pSkeletonPose->GetPoseData(); }
Skeleton::CPoseData&       CSkeletonPhysics::GetPoseDataExplicitWriteable() { return m_pSkeletonPose->GetPoseDataExplicitWriteable(); }
Skeleton::CPoseData&       CSkeletonPhysics::GetPoseDataForceWriteable()    { return m_pSkeletonPose->GetPoseDataForceWriteable(); }
Skeleton::CPoseData*       CSkeletonPhysics::GetPoseDataWriteable()         { return m_pSkeletonPose->GetPoseDataWriteable(); }
const Skeleton::CPoseData& CSkeletonPhysics::GetPoseDataDefault() const     { return m_pSkeletonPose->GetPoseDataDefault(); }

int16                      CSkeletonPhysics::GetJointIDByName(const char* szJointName) const
{
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	return rDefaultSkeleton.GetJointIDByName(szJointName);
};

void CSkeletonPhysics::InitPhysicsSkeleton()
{
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	const CDefaultSkeleton::SJoint* parrModelJoints = &rDefaultSkeleton.m_arrModelJoints[0];
	uint32 numJoints = rDefaultSkeleton.m_arrModelJoints.size();
	m_arrPhysicsJoints.resize(numJoints);

	for (uint32 j = 0; j < numJoints; j++)
	{
		m_arrPhysicsJoints[j].m_DefaultRelativeQuat = rDefaultSkeleton.m_poseDefaultData.GetJointsRelative()[j];
		m_arrPhysicsJoints[j].m_qRelPhysParent.SetIdentity();
	}

	for (uint32 i = 1; i < numJoints; ++i)
	{
		int32 p, pp;
		for (p = pp = parrModelJoints[i].m_idxParent; pp > -1 && parrModelJoints[pp].m_PhysInfo.pPhysGeom == 0; pp = parrModelJoints[pp].m_idxParent)
			;
		if (pp > -1)
			m_arrPhysicsJoints[i].m_qRelPhysParent = !rDefaultSkeleton.GetDefaultAbsJointByID(p).q * rDefaultSkeleton.GetDefaultAbsJointByID(pp).q;
	}

}

void CSkeletonPhysics::SetPhysEntOnJoint(int32 nId, IPhysicalEntity* pPhysEnt)
{
	if (nId < 0 || nId >= (int)GetPoseDataDefault().GetJointCount())
	{
		assert(0);
		return;
	}
	if (!m_ppBonePhysics)
		memset(m_ppBonePhysics = new IPhysicalEntity*[GetPoseDataDefault().GetJointCount()], 0, GetPoseDataDefault().GetJointCount() * sizeof(IPhysicalEntity*));
	if (m_ppBonePhysics[nId])
		if (m_ppBonePhysics[nId]->Release() <= 0)
			gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_ppBonePhysics[nId]);
	m_ppBonePhysics[nId] = pPhysEnt;
	if (pPhysEnt)
		pPhysEnt->AddRef();
}

int CSkeletonPhysics::GetPhysIdOnJoint(int32 nId) const
{
	if (nId < 0 || nId >= (int)GetPoseDataDefault().GetJointCount())
	{
		assert(0);
		return -1;
	}

	if (m_arrCGAJoints->size())
	{
		CCGAJoint& joint = (*m_arrCGAJoints)[nId];

		if (joint.m_qqqhasPhysics >= 0)
			return joint.m_qqqhasPhysics;
	}

	return -1;
}

void ResetClothAttachment(const aux_phys_data& auxPhys, CAttachmentManager& AttMan)
{
	if (!auxPhys.nBones)
		if (IAttachment* pAtt = AttMan.GetInterfaceByNameCRC(auxPhys.iBoneTiedTo[1]))
			if (pAtt->GetIAttachmentObject()->GetAttachmentType() == IAttachmentObject::eAttachment_StatObj)
			{
				if (IStatObj* pSrcObj = pAtt->GetIAttachmentObject()->GetIStatObj()->GetCloneSourceObject())
					((CCGFAttachment*)pAtt->GetIAttachmentObject())->pObj = pSrcObj;
				if (auxPhys.pauxBoneInfo)
					pAtt->SetAttRelativeDefault(QuatT(auxPhys.pauxBoneInfo->quat0, auxPhys.pauxBoneInfo->dir0));

			}
}

void CSkeletonPhysics::DestroyPhysics()
{
	if (m_pCharPhysics)
		g_pIPhysicalWorld->DestroyPhysicalEntity(m_pCharPhysics);
	m_pCharPhysics = 0;

	if (m_ppBonePhysics)
	{
		for (int i = 0; i < (int)GetPoseDataDefault().GetJointCount(); i++)
		{
			if (m_ppBonePhysics[i])
			{
				m_ppBonePhysics[i]->Release();
				g_pIPhysicalWorld->DestroyPhysicalEntity(m_ppBonePhysics[i]);
			}
		}
		delete[] m_ppBonePhysics;
		m_ppBonePhysics = 0;
	}

	for (int i = 0; i < m_nAuxPhys; i++)
	{
		ResetClothAttachment(m_auxPhys[i], m_pInstance->m_AttachmentManager);
		g_pIPhysicalWorld->DestroyPhysicalEntity(m_auxPhys[i].pPhysEnt);
		delete[] m_auxPhys[i].pauxBoneInfo;
		delete[] m_auxPhys[i].pVtx;
	}
	m_nAuxPhys = 0;
	m_bPhysicsRelinquished = false;
	m_timeStandingUp = -1;
	SetPrevHost();
}

void CSkeletonPhysics::SetAuxParams(pe_params* pf)
{
	for (int32 i = 0; i < m_nAuxPhys; i++)
		m_auxPhys[i].pPhysEnt->SetParams(pf);
}

int CSkeletonPhysics::getBonePhysParentOrSelfIndex(int iBoneIndex, int nLod) const
{
	if (iBoneIndex >= m_pInstance->m_pDefaultSkeleton->m_arrModelJoints.size())
		return -1;
	int iNextBoneIndex;
	for (; !GetModelJointPointer(iBoneIndex)->m_PhysInfo.pPhysGeom; iBoneIndex = iNextBoneIndex)
		if ((iNextBoneIndex = getBoneParentIndex(iBoneIndex)) == iBoneIndex)
			return -1;
	return iBoneIndex;
}

const CDefaultSkeleton::SJoint* CSkeletonPhysics::GetModelJointPointer(int nBone) const
{
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	return &rDefaultSkeleton.m_arrModelJoints[nBone];
}

CDefaultSkeleton::SJoint* CSkeletonPhysics::GetModelJointPointer(int nBone)
{
	const CSkeletonPhysics* pConstThis = this;
	return const_cast<CDefaultSkeleton::SJoint*>(pConstThis->GetModelJointPointer(nBone));
}

//////////////////////////////////////////////////////////////////////////
// finds the first physicalized child (or itself) of the given bone (bone given by index)
// returns -1 if it's not physicalized
int CSkeletonPhysics::getBonePhysChildIndex(int iBoneIndex, int nLod) const
{
	const CDefaultSkeleton::SJoint* pBoneInfo = GetModelJointPointer(iBoneIndex);
	if (pBoneInfo->m_PhysInfo.pPhysGeom)
		return iBoneIndex;
	unsigned numChildren = pBoneInfo->m_numChildren;
	unsigned nFirstChild = pBoneInfo->m_nOffsetChildren + iBoneIndex;
	if (nFirstChild == iBoneIndex)
		return -1;
	for (unsigned nChild = 0; nChild < numChildren; ++nChild)
	{
		int nResult = getBonePhysChildIndex(nFirstChild + nChild, nLod);
		if (nResult >= 0)
			return nResult;
	}
	return -1;
}

int CSkeletonPhysics::GetPhysRoot() const 
{
	int i, n = m_pInstance->m_pDefaultSkeleton->GetJointCount();
	for(i = 0; i < n && !GetModelJointPointer(i)->m_PhysInfo.pPhysGeom; i++);
	return i < n ? i : -1;
}

int CSkeletonPhysics::GetBoneSurfaceTypeId(int nBoneIndex, int nLod) const
{
	const phys_geometry* pGeom = GetModelJointPointer(nBoneIndex)->m_PhysInfo.pPhysGeom;
	if (pGeom)
		return pGeom->pMatMapping[pGeom->surface_idx];
	else
		return -1;
}

int CSkeletonPhysics::TranslatePartIdToDeadBody(int partid)
{
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	uint32 numJoints = rDefaultSkeleton.GetJointCount();
	if ((unsigned int)partid >= (unsigned int)numJoints)
		return -1;

	int nLod = GetPhysicsLod();
	if (GetModelJointPointer(partid)->m_PhysInfo.pPhysGeom)
		return partid;

	return getBonePhysParentIndex(partid, nLod);
}

void CSkeletonPhysics::BuildPhysicalEntity(
  IPhysicalEntity* pent,
  float mass,
  int surface_idx,
  float stiffness_scale,
  int nLod,
  int partid0,
  const Matrix34& mtxloc)
{
	const Skeleton::CPoseData& poseData = GetPoseData();
	CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	partid0 = partid0 < 0 ? 0 : EntityPhysicsUtils::AllocPartIdRange(partid0, EntityPhysicsUtils::PARTID_MAX_SLOTS);

	float scaleOrg = mtxloc.GetColumn(0).GetLength();
	float scale = scaleOrg;// / m_pInstance->m_location.s;

	//scale = m_pInstance->GetUniformScale();

	Vec3 offset = mtxloc.GetTranslation();
	if (fabs_tpl(scale - 1.0f) < 0.01f)
		scale = 1.0f;

	if (rDefaultSkeleton.m_ObjectType == CGA)
	{
		assert(pent);

		m_pCharPhysics = pent;

		uint32 i;
		float totalVolume = 0;

		uint32 numJoints = m_arrCGAJoints->size();
		assert(numJoints);
		for (i = 0; i < numJoints; i++)
		{
			CCGAJoint* joint = &(*m_arrCGAJoints)[i];
			if (!joint->m_CGAObjectInstance)
				continue;
			phys_geometry* geom = joint->m_CGAObjectInstance->GetPhysGeom();
			if (geom)
			{
				totalVolume += geom->V;
			}
		}
		float density = totalVolume > FLT_EPSILON ? mass / totalVolume : 0.0f;

		pe_articgeomparams params;
		//	uint32 numJoints = m_arrJoints.size();
		//	assert(numJoints);
		for (i = 0; i < numJoints; i++)
		{
			CCGAJoint* joint = &(*m_arrCGAJoints)[i];
			//SJoint *sjoint = &m_arrJoints[i];
			rDefaultSkeleton.m_arrModelJoints[i].m_NodeID = ~0;
			if (!joint->m_CGAObjectInstance)
				continue;

			IStatObj::SSubObject* pSubObj;
			const char* pstrMass;
			if (rDefaultSkeleton.m_pCGA_Object && (pSubObj = rDefaultSkeleton.m_pCGA_Object->GetSubObject(i)) &&
			    (pstrMass = strstr(pSubObj->properties, "mass")))
			{
				params.density = 0;
				for (; *pstrMass && !isdigit((unsigned char)(*pstrMass)); pstrMass++)
					;
				params.mass = (float)atof(pstrMass);
			}
			else
			{
				params.mass = 0;
				params.density = density;
			}

			// Add collision geometry.
			params.flags = geom_collides | geom_floats;
			phys_geometry* geom = joint->m_CGAObjectInstance->GetPhysGeom(PHYS_GEOM_TYPE_NO_COLLIDE),
			             * geomProxy = joint->m_CGAObjectInstance->GetPhysGeom();
			if (geom && !geomProxy)
				params.flags = geom_colltype_ray;
			if (!geom)
				geom = geomProxy, geomProxy = 0;
			params.pos = poseData.GetJointAbsolute(i).t * scale;
			params.q = poseData.GetJointAbsolute(i).q;
			params.scale = scaleOrg;
			params.idbody = i;
			if (geom)
				joint->m_qqqhasPhysics = rDefaultSkeleton.m_arrModelJoints[i].m_NodeID = m_pCharPhysics->AddGeometry(geom, &params, partid0 + i);
			if (geomProxy)
			{
				params.flags |= geom_proxy;
				m_pCharPhysics->AddGeometry(geomProxy, &params, partid0 + i);
			}
		}

		pe_params_joint pj;
		pj.op[0] = -1;
		pj.flagsPivot = 7;
		if (pent->GetType() == PE_ARTICULATED)
			for (i = 0; i < numJoints; i++)
				if ((*m_arrCGAJoints)[i].m_CGAObjectInstance && (*m_arrCGAJoints)[i].m_CGAObjectInstance->GetPhysGeom())
				{
					pj.op[1] = i;
					pj.pivot = poseData.GetJointAbsolute(i).t;
					pent->SetParams(&pj);
				}

		int j, numSlots = (rDefaultSkeleton.m_pCGA_Object) ? rDefaultSkeleton.m_pCGA_Object->GetSubObjectCount() : 0;
		IStatObj::SSubObject* pSubObj;
		for (i = 0; i < numJoints; i++)
			if ((*m_arrCGAJoints)[i].m_CGAObjectInstance && (*m_arrCGAJoints)[i].m_CGAObjectInstance->GetPhysGeom())
				for (j = 0; j < min(numSlots, (int)numJoints); j++)
					if ((pSubObj = rDefaultSkeleton.m_pCGA_Object->GetSubObject(j)) && pSubObj->pStatObj == (*m_arrCGAJoints)[i].m_CGAObjectInstance)
					{
						(*m_arrCGAJoints)[j].m_qqqhasPhysics = i;
						break;
					}

		if (rDefaultSkeleton.m_pCGA_Object)
		{
			rDefaultSkeleton.m_pCGA_Object->PhysicalizeSubobjects(m_pCharPhysics, &mtxloc, mass, density, partid0,
			                                                      strided_pointer<int>(&(*m_arrCGAJoints)[0].m_qqqhasPhysics, sizeof((*m_arrCGAJoints)[0])));
		}

		for (i = 0; i < numJoints; i++)
			(*m_arrCGAJoints)[i].m_qqqhasPhysics = rDefaultSkeleton.m_arrModelJoints[i].m_NodeID;
	}
	else
	{
		pe_type pentype = pent->GetType();
		int i, j;
		pe_geomparams gp;
		pe_articgeomparams agp;
		pe_params_articulated_body pab;
		pe_geomparams* pgp = pentype == PE_ARTICULATED ? &agp : &gp;
		pgp->flags = pentype == PE_LIVING ? 0 : geom_collides | geom_floats;
		pgp->bRecalcBBox = 0;
		float volume[2] = { 0, 0 }, M = 0, density, k;

		const int nJointCount = (int)rDefaultSkeleton.GetJointCount();
		for (i = pab.nJointsAlloc = 0; i < nJointCount; ++i)
		{
			if (GetModelJointPointer(i)->m_PhysInfo.pPhysGeom)
			{
				volume[GetModelJointPointer(i)->m_fMass > 0.0f] += GetModelJointPointer(i)->m_PhysInfo.pPhysGeom->V * cube(scale);
				M += GetModelJointPointer(i)->m_fMass;
				pab.nJointsAlloc++;
			}
		}

		density = volume[1] > 0.0f ? M / volume[1] : 1.0f;
		k = M + volume[0] > 0.0f ? mass / (M + volume[0] * density) : 1.0f;

		pgp->scale = scaleOrg;

		if (surface_idx >= 0)
			pgp->surface_idx = surface_idx;

		pe_action_remove_all_parts tmp;
		pent->Action(&tmp);
		pent->SetParams(&pab);

		for (i = 0; i < nJointCount; i++)
		{
			if (GetModelJointPointer(i)->m_PhysInfo.pPhysGeom)
			{
				rDefaultSkeleton.m_arrModelJoints[i].m_NodeID = ~0;
				pgp->pos = poseData.GetJointAbsolute(i).t * scale + offset;
				pgp->q = poseData.GetJointAbsolute(i).q;
				pgp->flags = /*strstr(GetModelJointIdx(i)->m_strJointName,"Hand") ? geom_no_raytrace :*/ geom_collides | geom_floats;
				if (GetModelJointPointer(i)->m_PhysInfo.pPhysGeom->pForeignData)
					pgp->flags &= ~geom_colltype6;

				agp.idbody = i;
				while ((j = getBonePhysParentIndex(agp.idbody, nLod)) >= 0 &&
				       (GetModelJointPointer(agp.idbody)->m_PhysInfo.flags & all_angles_locked) == all_angles_locked)
				{
					agp.idbody = j;
				}

				if (!(pgp->mass = GetModelJointPointer(i)->m_fMass))
					pgp->mass = GetModelJointPointer(i)->m_PhysInfo.pPhysGeom->V * density;
				pgp->mass *= k;
				rDefaultSkeleton.m_arrModelJoints[i].m_NodeID = pent->AddGeometry(GetModelJointPointer(i)->m_PhysInfo.pPhysGeom, pgp, partid0 + i);
				if (GetModelJointPointer(i)->m_PhysInfo.pPhysGeom->pForeignData)
				{
					pgp->mass = 0.0f;
					pgp->flags = geom_colltype6;
					pent->AddGeometry((phys_geometry*)GetModelJointPointer(i)->m_PhysInfo.pPhysGeom->pForeignData, pgp, i + 200);
				}
			}
			else
			{
				rDefaultSkeleton.m_arrModelJoints[i].m_NodeID = ~0;
			}
		}

		if (pentype == PE_ARTICULATED)
		{
			//SJoint *pBone;
			CDefaultSkeleton::SJoint* pBoneInfo;
			for (i = 0; i < nJointCount; i++)
			{
				if (GetModelJointPointer(i)->m_PhysInfo.pPhysGeom)
				{
					pe_params_joint pj;
					int iParts[16];
					//pBone = &m_arrJoints[i];
					pBoneInfo = GetModelJointPointer(i);

					pj.pSelfCollidingParts = iParts;
					if ((pj.flags = pBoneInfo->m_PhysInfo.flags) != ~0)
						pj.flags |= angle0_auto_kd * 7;
					else
						pj.flags = angle0_locked;

					if (nLod)
						pj.flags &= ~(angle0_auto_kd * 7);

					int iParent0;
					if ((iParent0 = pj.op[0] = getBonePhysParentIndex(i, nLod)) >= 0)
					{
						while ((j = getBonePhysParentIndex(pj.op[0], nLod)) >= 0 && (GetModelJointPointer(pj.op[0])->m_PhysInfo.flags & all_angles_locked) == all_angles_locked)
							pj.op[0] = j;
					}

					if ((pj.flags & all_angles_locked) == all_angles_locked && pj.op[0] >= 0)
						continue;

					pj.op[1] = i;

					pj.pivot = poseData.GetJointAbsolute(i).t;

					pj.pivot = pj.pivot * scale + offset;

					pj.nSelfCollidingParts = 0;
					if (pBoneInfo->m_flags & eJointFlag_NameHasForearm)
					{
						for (j = 0; j < nJointCount && pj.nSelfCollidingParts < CRY_ARRAY_COUNT(iParts); j++)
						{
							const CDefaultSkeleton::SJoint& modelJoint = *GetModelJointPointer(j);
							if (modelJoint.m_PhysInfo.pPhysGeom &&
							    ((modelJoint.m_flags & eJointFlag_NameHasPelvisOrHeadOrSpineOrThigh) ||
							     (modelJoint.m_flags & eJointFlag_NameHasForearm) && i > j))
							{
								pj.pSelfCollidingParts[pj.nSelfCollidingParts++] = j;
							}
						}
					}
					else if (pBoneInfo->m_flags & eJointFlag_NameHasCalf)
					{
						uint32 numJoints = rDefaultSkeleton.GetJointCount();
						for (j = 0; j < (int)numJoints && pj.nSelfCollidingParts < CRY_ARRAY_COUNT(iParts); j++)
						{
							const CDefaultSkeleton::SJoint& modelJoint = *GetModelJointPointer(j);
							if (modelJoint.m_PhysInfo.pPhysGeom &&
							    (modelJoint.m_flags & eJointFlag_NameHasCalf) && i > j)
							{
								pj.pSelfCollidingParts[pj.nSelfCollidingParts++] = j;
							}
						}
					}

					if (pBoneInfo->m_PhysInfo.flags != -1)
					{
						for (j = 0; j < 3; j++)
						{
							pj.limits[0][j] = pBoneInfo->m_PhysInfo.min[j];
							pj.limits[1][j] = pBoneInfo->m_PhysInfo.max[j];
							pj.bounciness[j] = 0;
							pj.ks[j] = pBoneInfo->m_PhysInfo.spring_tension[j] * stiffness_scale;
							if (pj.ks[j] < 0)
								pj.ks[j] = -pj.ks[j], pj.flags |= all_angles_locked;
							pj.kd[j] = pBoneInfo->m_PhysInfo.damping[j];
							if (fabsf(pj.limits[0][j]) < 3)
							{
								pj.qdashpot[j] = 0.3f;
								pj.kdashpot[j] = 40.0f - nLod * 36.0f;
							}
							else
							{
								pj.qdashpot[j] = pj.kdashpot[j] = 0;
							}
						}
					}
					else
					{
						for (j = 0; j < 3; j++)
						{
							pj.limits[0][j] = -1E10f;
							pj.limits[1][j] = 1E10f;
							pj.bounciness[j] = 0;
							pj.ks[j] = 0;
							pj.kd[j] = stiffness_scale;
						}
					}
					if (strstr(GetModelJointPointer(i)->m_strJointName.c_str(), "Thigh") || strstr(GetModelJointPointer(i)->m_strJointName.c_str(), "Calf"))
						pj.flags |= joint_ignore_impulses;

					pj.pMtx0 = 0;
					Matrix33 mtx0;
					if (pBoneInfo->m_PhysInfo.framemtx[0][0] < 10 && pBoneInfo->m_PhysInfo.flags != -1)
					{
						union
						{
							float*    pF;
							Matrix33* pM;
						} u;
						u.pF = (float*)&pBoneInfo->m_PhysInfo.framemtx[0];
						pj.pMtx0 = &(mtx0 = *u.pM);
					}
					else
					{
						mtx0 = Matrix33(IDENTITY);
					}

					if (iParent0 != pj.op[0])
					{
						mtx0 = Matrix33(
						  !poseData.GetJointAbsolute(pj.op[0]).q *
						  poseData.GetJointAbsolute(iParent0).q) * mtx0;
						pj.pMtx0 = &mtx0;
					}
					pent->SetParams(&pj);
				}
			}
		}

		//m_vOffset = offset;
		//m_fScale = scale;
		m_fMass = mass;
		m_iSurfaceIdx = surface_idx;
		m_bPhysicsRelinquished = (nLod > 0);
	}

	for (int i = m_pInstance->m_AttachmentManager.GetAttachmentCount() - 1; i >= 0; i--)
		m_pInstance->m_AttachmentManager.PhysicalizeAttachment(i, nLod, pent, offset);
}

IPhysicalEntity* CSkeletonPhysics::GetCharacterPhysics(const char* pRootBoneName) const
{
	if (!pRootBoneName)
		return m_pCharPhysics;

	for (int i = 0; i < m_nAuxPhys; i++)
	{
		if (!strnicmp(m_auxPhys[i].strName, pRootBoneName, m_auxPhys[i].nChars))
			return m_auxPhys[i].pPhysEnt;
	}

	return m_pCharPhysics;
}
IPhysicalEntity* CSkeletonPhysics::GetCharacterPhysics(int iAuxPhys) const
{
	if (iAuxPhys < 0)
		return m_pCharPhysics;
	if (iAuxPhys >= m_nAuxPhys)
		return 0;

	return m_auxPhys[iAuxPhys].pPhysEnt;
}

void CSkeletonPhysics::DestroyCharacterPhysics(int iMode)
{
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	m_pSkeletonAnim->FinishAnimationComputations();

	int i;
	for (i = 0; i < m_nAuxPhys; i++)
	{
		g_pIPhysicalWorld->DestroyPhysicalEntity(m_auxPhys[i].pPhysEnt, iMode);
		if (iMode == 0)
		{
			ResetClothAttachment(m_auxPhys[i], m_pInstance->m_AttachmentManager);

			delete[] m_auxPhys[i].pauxBoneInfo;
			m_auxPhys[i].pauxBoneInfo = NULL;

			delete[] m_auxPhys[i].pVtx;
			m_auxPhys[i].pVtx = NULL;
		}
	}
	if (iMode == 0)
		m_nAuxPhys = 0;

	if (m_pCharPhysics)
		g_pIPhysicalWorld->DestroyPhysicalEntity(m_pCharPhysics, iMode);
	if (iMode == 0)
	{
		m_pCharPhysics = 0;
		m_bPhysicsRelinquished = false;
		m_timeStandingUp = -1;
		if (m_ppBonePhysics)
		{
			uint32 numJoints = rDefaultSkeleton.GetJointCount();
			for (i = 0; i < (int)numJoints; i++)
				if (m_ppBonePhysics[i])
					m_ppBonePhysics[i]->Release(), g_pIPhysicalWorld->DestroyPhysicalEntity(m_ppBonePhysics[i]);
			delete[] m_ppBonePhysics;
			m_ppBonePhysics = 0;
		}
		SetPrevHost();
	}

	if (iMode == 2)
		for (i = 0; i < m_nAuxPhys; i++)
			if (m_auxPhys[i].pPhysEnt->GetType() == PE_ROPE)
			{
				pe_params_rope pr;
				pr.bTargetPoseActive = 0;
				if (m_auxPhys[i].bTied0)
					pr.pEntTiedTo[0] = m_pCharPhysics;
				if (m_auxPhys[i].bTied1)
					pr.pEntTiedTo[1] = m_pCharPhysics;
				m_auxPhys[i].pPhysEnt->SetParams(&pr);
			}
			else
			{
				int j;
				pe_action_attach_points aap;
				aap.pEntity = m_pCharPhysics;
				aap.nPoints = 1;
				aap.piVtx = &j;
				for (j = 0; j < m_auxPhys[i].nBones; )
				{
					aap.partid = getBonePhysParentIndex(m_auxPhys[i].pauxBoneInfo[j].iBone, 0);
					m_auxPhys[i].pPhysEnt->Action(&aap);
					for (j++; j < m_auxPhys[i].nBones && !strncmp(GetModelJointPointer(getBoneParentIndex(m_auxPhys[i].pauxBoneInfo[j].iBone))->m_strJointName.c_str(),
					                                              m_auxPhys[i].strName, m_auxPhys[i].nChars); j++)
						;
				}
			}
}

IPhysicalEntity* CSkeletonPhysics::CreateCharacterPhysics(
  IPhysicalEntity* pHost,
  float mass,
  int surface_idx,
  float stiffness_scale,
  int nLod,
  const Matrix34& mtxloc)
{
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	if (m_pSkeletonPose->m_bSetDefaultPoseExecute)
	{
		m_pSkeletonPose->SetDefaultPoseExecute(false);
		m_pSkeletonPose->m_bSetDefaultPoseExecute = false;
	}
	m_pSkeletonAnim->FinishAnimationComputations();

	float scale = mtxloc.GetColumn(0).GetLength();

	//scale = m_pInstance->GetUniformScale();

	Vec3 offset = mtxloc.GetTranslation();
	if (fabs_tpl(scale - 1.0f) < 0.01f)
		scale = 1.0f;
	//
	//
	if (m_pCharPhysics)
	{
		int i;
		g_pIPhysicalWorld->DestroyPhysicalEntity(m_pCharPhysics);
		m_pCharPhysics = 0;
		m_bPhysicsRelinquished = false;
		m_timeStandingUp = -1;
		for (i = 0; i < m_nAuxPhys; i++)
		{
			g_pIPhysicalWorld->DestroyPhysicalEntity(m_auxPhys[i].pPhysEnt);

			delete[] m_auxPhys[i].pauxBoneInfo;
			m_auxPhys[i].pauxBoneInfo = NULL;

			delete[] m_auxPhys[i].pVtx;
			m_auxPhys[i].pVtx = NULL;
		}
		m_nAuxPhys = 0;
		if (m_ppBonePhysics)
		{
			uint32 numJoints = rDefaultSkeleton.GetJointCount();
			for (i = 0; i < (int)numJoints; i++)
				if (m_ppBonePhysics[i])
					m_ppBonePhysics[i]->Release(), g_pIPhysicalWorld->DestroyPhysicalEntity(m_ppBonePhysics[i]);
			delete[] m_ppBonePhysics;
			m_ppBonePhysics = 0;
		}
	}

	int iRoot = GetPhysRoot();
	if (iRoot >= 0)
	{
		pe_params_foreign_data pfd;
		pfd.pForeignData = 0;
		pfd.iForeignData = 0;
		if (pHost)
			pHost->GetParams(&pfd);

		pe_params_pos pp;
		pp.iSimClass = 6;
		m_pCharPhysics = g_pIPhysicalWorld->CreatePhysicalEntity(PE_ARTICULATED, 5.0f, &pp, pfd.pForeignData, pfd.iForeignData);

#if !defined(_RELEASE)
		pe_params_flags pf;
		pf.flagsOR = pef_monitor_state_changes;
		m_pCharPhysics->SetParams(&pf);
#endif

		pe_params_articulated_body pab;
		pab.bGrounded = 1;
		pab.scaleBounceResponse = 0.6f;
		pab.bAwake = 0;

		//IVO_x
		pab.pivot = GetPoseData().GetJointAbsolute(iRoot).t;

		m_pCharPhysics->SetParams(&pab);

		BuildPhysicalEntity(m_pCharPhysics, mass, surface_idx, stiffness_scale, 0, -1, mtxloc);

		pe_params_joint pj;
		pj.op[0] = -1;
		pj.op[1] = iRoot;
		pj.flags = all_angles_locked | joint_no_gravity | joint_isolated_accelerations;
		m_pCharPhysics->SetParams(&pj);

		if (pHost)
		{
			pe_params_articulated_body pab1;
			pab1.pivot.zero();
			pab1.pHost = pHost;
			pab1.posHostPivot =	GetPoseData().GetJointAbsolute(iRoot).t * scale + offset;
			pab1.qHostPivot.SetIdentity();
			pab1.bAwake = 0;

			m_pCharPhysics->SetParams(&pab1);
		}

		pp.iSimClass = 4;
		m_pCharPhysics->SetParams(&pp);
		pe_params_outer_entity poe;
		poe.pOuterEntity = m_pCharPhysics;
		m_pCharPhysics->SetParams(&poe);
	}

	m_vOffset = offset;
	m_fScale = scale;
	m_fMass = mass;
	m_iSurfaceIdx = surface_idx;
	m_stiffnessScale = stiffness_scale;
	SetPrevHost(pHost);
	m_bPhysicsRelinquished = 0;
	m_timeStandingUp = -1.0f;

	/*IAttachmentObject* pAttachment;
	   for(int i=m_AttachmentManager.GetAttachmentCount()-1; i>=0; i--)
	   if ((pAttachment = m_AttachmentManager.GetInterfaceByIndex(i)->GetIAttachmentObject()) && pAttachment->GetICharacterInstance())
	   pAttachment->GetICharacterInstance()->CreateCharacterPhysics(m_pCharPhysics, mass,surface_idx, stiffness_scale, nLod);*/

	return m_pCharPhysics;
}

int CSkeletonPhysics::CreateAuxilaryPhysics(IPhysicalEntity* pHost, const Matrix34& mtx, int nLod)
{
	return CreateAuxilaryPhysics(pHost, mtx, mtx.GetColumn(0).len(), m_vOffset, nLod);
}

void CSkeletonPhysics::SetJointPhysInfo(uint32 iJoint, const CryBonePhysics& pi, int nLod)
{
	int i;
	for (i = m_extraPhysInfo.size() - 1; i >= 0 && (m_extraPhysInfo[i].iJoint != iJoint || m_extraPhysInfo[i].nLod != nLod); i--)
		;
	if (i < 0)
	{
		SExtraPhysInfo epi;
		epi.iJoint = iJoint;
		epi.nLod = nLod;
		m_extraPhysInfo.push_back(epi);
		i = m_extraPhysInfo.size() - 1;
	}
	memcpy(&m_extraPhysInfo[i].info, &pi, sizeof(CryBonePhysics));
}
const CryBonePhysics& CSkeletonPhysics::GetJointPhysInfo(uint32 iJoint, int nLod) const
{
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	const CryBonePhysics& piModel = rDefaultSkeleton.m_arrModelJoints[iJoint].m_PhysInfo;
	for (int i = m_extraPhysInfo.size() - 1; i >= 0; i--)
		if (m_extraPhysInfo[i].iJoint == iJoint && m_extraPhysInfo[i].nLod == nLod)
		{
			*(int*)(m_extraPhysInfo[i].info.spring_angle + 1) = *(int*)(piModel.spring_angle + 1);
			return m_extraPhysInfo[i].info;
		}
	return piModel;
}

DynArray<SJointProperty> CSkeletonPhysics::GetJointPhysProperties_ROPE(uint32 jointIndex, int nLod) const
{
	CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	if (jointIndex >= (uint32)rDefaultSkeleton.m_arrModelJoints.size())
	{
		CryBonePhysics bp;
		memset(&bp, 0, sizeof(bp));
		return rDefaultSkeleton.GetPhysInfoProperties_ROPE(bp, -(int)jointIndex - 1);
	}

	const char* pJointName = rDefaultSkeleton.m_arrModelJoints[jointIndex].m_strJointName.c_str();
	if (strnicmp(pJointName, "rope", 4) == 0)
	{
		const CDefaultSkeleton::SJoint* pParentJoint = rDefaultSkeleton.GetParent(jointIndex);
		const char* pParentJointName = pParentJoint ? pParentJoint->m_strJointName.c_str() : 0;
		if (pParentJointName == 0 || (strnicmp(pParentJointName, "rope", 4)))
		{
			const char* ptr;
			int32 nRopeOrGrid = 0;
			ptrdiff_t len = (ptr = strchr(pJointName, ' ')) ? ptr - pJointName : ((ptr = strchr(pJointName, '_')) ? ptr - pJointName : strlen(pJointName));
			int32 numJoints = rDefaultSkeleton.m_arrModelJoints.size();
			for (int i = numJoints - 1; i >= 0; i--)
			{
				if (i == jointIndex)
					continue;
				const char* pJoint1Name = rDefaultSkeleton.m_arrModelJoints[i].m_strJointName.c_str();
				if (strncmp(pJoint1Name, pJointName, len) == 0)
				{
					const CDefaultSkeleton::SJoint* pParent1Joint = rDefaultSkeleton.GetParent(i);
					const char* pParentJoint1Name = pParent1Joint ? pParent1Joint->m_strJointName.c_str() : 0;
					if (pParentJoint1Name == 0 || strnicmp(pParentJoint1Name, "rope", 4))
					{
						nRopeOrGrid++;
						break;                //more then one rope-joint has a spine-parent...so we assume its a grid.
					}
				}
			}
			return rDefaultSkeleton.GetPhysInfoProperties_ROPE(GetJointPhysInfo(jointIndex, nLod), nRopeOrGrid);
		}
	}
	const CryBonePhysics& pi = GetJointPhysInfo(jointIndex, nLod);
	if (&pi != &rDefaultSkeleton.m_arrModelJoints[jointIndex].m_PhysInfo)
		return rDefaultSkeleton.GetPhysInfoProperties_ROPE(pi, (pi.flags & joint_isolated_accelerations) != 0);
	return DynArray<SJointProperty>();
}

bool CSkeletonPhysics::SetJointPhysProperties_ROPE(uint32 jointIndex, int nLod, const DynArray<SJointProperty>& props)
{
	CryBonePhysics bonePhysics;
	memset(&bonePhysics, 0, sizeof(bonePhysics));
	if (!CDefaultSkeleton::ParsePhysInfoProperties_ROPE(bonePhysics, props))
		return false;
	SetJointPhysInfo(jointIndex, bonePhysics, nLod);
	return true;
}

void ParsePhysInfoProps(const CryBonePhysics& physInfo, int nLod, pe_params_rope& pr, pe_simulation_params& simp, pe_params_flags& pf, const Quat& qrel)
{
	pr.jointLimit = physInfo.spring_tension[0];
	if (physInfo.min[0] != 0)
		pr.jointLimit = fabs_tpl(physInfo.min[0]);
	if (physInfo.min[1] != 0)
		pr.mass = fabs_tpl(RAD2DEG(physInfo.min[1]));
	if (physInfo.min[2] != 0)
		pr.collDist = fabs_tpl(RAD2DEG(physInfo.min[2]));
	simp.maxTimeStep = physInfo.spring_tension[1];
	if (simp.maxTimeStep <= 0 || simp.maxTimeStep >= 1)
		simp.maxTimeStep = 0.02f;
	pr.stiffnessAnim = max(0.001f, RAD2DEG(physInfo.max[0]));
	pr.stiffnessDecayAnim = RAD2DEG(physInfo.max[1]);
	pr.dampingAnim = RAD2DEG(physInfo.max[2]);
	pr.friction = physInfo.spring_tension[2];
	if (fabs_tpl(physInfo.spring_angle[2]) <= 10.0f)
		pr.jointLimitDecay = -physInfo.spring_angle[2];
	if (physInfo.damping[0] >= 1.0f)
		simp.minEnergy = sqr(physInfo.damping[0] - 1.0f);
	else if (!nLod)
		simp.minEnergy = 0;
	pr.bTargetPoseActive = 2 * isneg(-pr.stiffnessAnim);
	pf.flagsOR = pef_traceable;
	if (!(physInfo.flags & 4))
		pr.bTargetPoseActive >>= 1;
	pf.flagsOR |= rope_target_vtx_rel0;
	pr.flagsCollider = geom_colltype_ray | geom_colltype0 & - nLod;
	pr.collTypes = 0;
	int collChars = ent_living;
	if (!(physInfo.flags & 2))
		pf.flagsOR |= rope_collides_with_attachment, collChars = ent_independent, pr.flagsCollider = geom_colltype_ray;
	if (!(physInfo.flags & 1))
		pr.collTypes = ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid | collChars & - nLod;
	if (fabs_tpl(physInfo.framemtx[1][0] - 9.8f) > 0.02f)
	{
		pf.flagsOR |= pef_ignore_areas;
		simp.gravity = Vec3(0, 0, -physInfo.framemtx[1][0]);
	}
	else
		MARK_UNUSED simp.gravity;
	if (physInfo.flags & 8)
		pr.hingeAxis = qrel * Vec3(0, 1, 0);
	if (physInfo.flags & 16)
		pr.hingeAxis = qrel * Vec3(0, 0, 1);
}

void ParsePhysInfoPropsCloth(const CryBonePhysics& physInfo, int nLod, pe_params_softbody& psb, pe_simulation_params& simp)
{
	if (!(psb.thickness = physInfo.damping[2]))
		MARK_UNUSED psb.thickness;
	psb.collTypes = 0;
	if (!(physInfo.flags & 1))
		psb.collTypes |= ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid | ent_living;
	if ((physInfo.flags & 3) == 0)
		psb.collTypes |= ent_independent;
	if (!(simp.maxTimeStep = physInfo.damping[0]))
		MARK_UNUSED simp.maxTimeStep;
	psb.maxSafeStep = physInfo.damping[1];
	if (!(psb.ks = RAD2DEG(physInfo.max[2])))
		MARK_UNUSED psb.ks;
	psb.friction = physInfo.spring_tension[2];
	psb.shapeStiffnessNorm = RAD2DEG(physInfo.max[0]);
	psb.shapeStiffnessTang = RAD2DEG(physInfo.max[1]);
	psb.airResistance = physInfo.spring_tension[1];
	psb.stiffnessAnim = physInfo.min[0];
	psb.massDecay = psb.stiffnessDecayAnim = physInfo.min[1];
	psb.dampingAnim = physInfo.min[2];
	psb.maxDistAnim = physInfo.spring_angle[2];
	psb.hostSpaceSim = physInfo.framemtx[0][1];
	if (!(psb.nMaxIters = FtoI(physInfo.framemtx[0][2])))
		MARK_UNUSED psb.nMaxIters;
	simp.damping = physInfo.spring_tension[0];
	MARK_UNUSED simp.gravity;
	simp.minEnergy = nLod ? 0.1f : 0.0f;
}

int CSkeletonPhysics::CreateAuxilaryPhysics(IPhysicalEntity* pHost, const Matrix34& mtx, float scale, Vec3 offset, int nLod)
//float *pForcedRopeLen,int *piForcedRopeIdx,int nForcedRopes)
{
	if (m_pSkeletonPose->m_bSetDefaultPoseExecute)
	{
		m_pSkeletonPose->SetDefaultPoseExecute(false);
		m_pSkeletonPose->m_bSetDefaultPoseExecute = false;
	}
	m_pSkeletonAnim->FinishAnimationComputations();
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;

	int i, j, k, nchars;
	const char* pspace;

	// Delete aux physics

	if (m_nAuxPhys)
	{
		for (i = 0; i < m_nAuxPhys; i++)
		{
			pe_params_rope pr;
			if (m_auxPhys[i].bTied0)
				pr.pEntTiedTo[0] = pHost;
			if (m_auxPhys[i].bTied1)
				pr.pEntTiedTo[1] = pHost;
			pr.bTargetPoseActive = 0;
			m_auxPhys[i].pPhysEnt->SetParams(&pr);
			//g_pIPhysicalWorld->DestroyPhysicalEntity(m_auxPhys[i].pPhysEnt);
			//delete[] m_auxPhys[i].pauxBoneInfo;
			//delete[] m_auxPhys[i].pVtx;
		}
		return m_nAuxPhys;
	}

	m_nAuxPhys = 0;
	int nPhysGeoms = 0;

	const int nJointCount = (int)rDefaultSkeleton.GetJointCount();
	for (i = 0; i < nJointCount; i++)
	{
		CDefaultSkeleton::SJoint* pBoneInfo = GetModelJointPointer(i);
		const char* szBoneName = pBoneInfo->m_strJointName.c_str();

		if (!strnicmp(szBoneName, "rope", 4) && pBoneInfo->m_numChildren > 0 || &GetJointPhysInfo(i, nLod) != &m_pInstance->m_pDefaultSkeleton->m_arrModelJoints[i].m_PhysInfo)
		{
			if ((pspace = strchr(szBoneName, ' ')) || (pspace = strchr(szBoneName, '_')))
				nchars = (int)(pspace - szBoneName);
			else
				nchars = strlen(szBoneName);
			for (j = 0; j < m_nAuxPhys && strncmp(m_auxPhys[j].strName, szBoneName, nchars); ++j)
				;
			if (j == m_nAuxPhys)
			{
				m_auxPhys.resize(m_nAuxPhys + 1);
				memset(&m_auxPhys[m_nAuxPhys], 0, sizeof(m_auxPhys[0]));
				m_auxPhys[m_nAuxPhys].strName = szBoneName;
				m_auxPhys[m_nAuxPhys].iBoneTiedTo[0] = i;
				m_auxPhys[m_nAuxPhys].nBones = GetJointPhysInfo(i, 0).flags & 32;
				m_auxPhys[m_nAuxPhys++].nChars = nchars;
			}

		}
		else if (!pBoneInfo->m_PhysInfo.pPhysGeom)
			pBoneInfo->m_PhysInfo.flags = -1;
		else
			nPhysGeoms++;
	}

	if (nPhysGeoms > 0) // don't disable ropes if they are the only physics
		for (i = m_nAuxPhys - 1; i >= 0; i--)
			if (m_auxPhys[i].nBones & 32)
				m_auxPhys[i] = m_auxPhys[--m_nAuxPhys];

	// if we have ropes but no physics proxies, then consider physics was relinquished (fix for RSE-569)
	if (m_nAuxPhys && !m_bHasPhysics)
		m_bPhysicsRelinquished = 1;

	pe_status_pos sp;
	sp.pos = mtx.GetTranslation();
	sp.q = Quat(Matrix33(mtx) / mtx.GetColumn(0).GetLength());

	pe_params_flags pf;
	pf.flagsOR = pef_disabled;
	pe_action_target_vtx atv;
	pe_simulation_params simp;
	pe_params_softbody psb;
	pe_params_outer_entity poe;
	simp.gravity.zero();
	poe.pOuterEntity = pHost;

	for (j = 0; j < m_nAuxPhys; ++j)
	{
		m_auxPhys[j].pPhysEnt = g_pIPhysicalWorld->CreatePhysicalEntity(PE_ROPE);
		pe_params_rope pr;
		int iLastBone = 0, iFirstBone = 0;
		int bCloth = 0, nStrands = 0, iSavedRoot, i1, imin;
		pr.nSegments = 0;
		bool bAutoRope = strnicmp(m_auxPhys[j].strName, "rope", 4) != 0;
		int idxAttachment = -1;
		uint32 numJoints = rDefaultSkeleton.GetJointCount();
		if (bAutoRope)
		{
			pr.nSegments = 1;
			if (!(GetJointPhysInfo(m_auxPhys[j].iBoneTiedTo[0], nLod).flags & joint_isolated_accelerations))
				for (i = iLastBone = m_auxPhys[j].iBoneTiedTo[0]; GetModelJointPointer(i)->m_numChildren > 0; i = GetModelJointChildIndex(i, 0), pr.nSegments++)
					;
			else
			{
				for (i = m_pInstance->m_AttachmentManager.GetAttachmentCount() - 1, iFirstBone = m_auxPhys[j].iBoneTiedTo[0]; i >= 0; i--)
					if (m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetJointID() == iFirstBone)
						if (IAttachmentObject* pObj = m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetIAttachmentObject())
							if (pObj && pObj->GetIStatObj() && (idxAttachment < 0 || strstr(m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetName(), "cloth")))
							{
								m_auxPhys[j].iBoneTiedTo[1] = m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetNameCRC();
								bCloth = 1;
								idxAttachment = i;
								k = 0;
							}
				if (idxAttachment >= 0)
					goto cloth_aux;
			}
		}
		else
			for (i = 0; i < nJointCount; i++)
				if (!strncmp(m_auxPhys[j].strName, GetModelJointPointer(i)->m_strJointName.c_str(), m_auxPhys[j].nChars))
				{
					if (GetModelJointPointer(i)->m_numChildren > 0)
						pr.nSegments++;
					if (i == 0 || strncmp(m_auxPhys[j].strName, GetModelJointPointer(getBoneParentIndex(i))->m_strJointName.c_str(), m_auxPhys[j].nChars))
						pr.nSegments++;
				}
		pr.pPoints = strided_pointer<Vec3>(new Vec3[--pr.nSegments + 1]);
		m_auxPhys[j].pauxBoneInfo = new aux_bone_info[(m_auxPhys[j].nBones = pr.nSegments) + 1];
		if (!m_auxPhys[j].nBones)
			goto bad_bones;
		m_auxPhys[j].pVtx = pr.pPoints.data;
		pr.length = 0;
		m_auxPhys[j].bTied0 = m_auxPhys[j].bTied1 = false;
		m_auxPhys[j].iBoneTiedTo[0] = m_auxPhys[j].iBoneTiedTo[1] = -1;
		m_auxPhys[j].pSubVtx = 0;
		m_auxPhys[j].nSubVtxAlloc = 0;
		m_auxPhys[j].iBoneStiffnessController = -1;

		for (i = k = 0; i < (int)numJoints; )
			if ((!strncmp(m_auxPhys[j].strName, GetModelJointPointer(i)->m_strJointName.c_str(), m_auxPhys[j].nChars) || bAutoRope && i == iLastBone) && GetModelJointPointer(i)->m_numChildren > 0)
			{
				if (k == 0 && getBonePhysParentIndex(iFirstBone = i, nLod) >= 0)
					pr.idPartTiedTo[0] = getBonePhysParentIndex(m_auxPhys[j].iBoneTiedTo[0] = i, nLod);
				if (k > 0 && !bCloth && i != iLastBone)//strnicmp(GetModelJointPointer(getBoneParentIndex(i))->GetJointName(),"rope",4))
				{
					for (i1 = 0, imin = i; i1 < (int)numJoints; i1++)
						if (!strncmp(m_auxPhys[j].strName, GetModelJointPointer(i1)->m_strJointName.c_str(), m_auxPhys[j].nChars) &&
						    strncmp(m_auxPhys[j].strName, GetModelJointPointer(getBoneParentIndex(i1))->m_strJointName.c_str(), m_auxPhys[j].nChars) &&
						    strcmp(GetModelJointPointer(i1)->m_strJointName.c_str(), GetModelJointPointer(imin)->m_strJointName.c_str()) < 0)
							imin = i1;
					if (imin != i)
						iSavedRoot = 0, i = imin, k = 0;
					else
						iSavedRoot = i, i = GetModelJointChildIndex(m_auxPhys[j].pauxBoneInfo[k - 1].iBone, 0);
					bCloth = 1;
					continue;
				}

				pr.pPoints[k] = GetPoseData().GetJointAbsolute(i).t * scale + offset;
				pr.pPoints[k + 1] = GetPoseData().GetJointAbsolute(GetModelJointChildIndex(i, 0)).t * scale + offset;
				((GetModelJointPointer(i)->m_PhysInfo.flags) &= 0xFFFF) |= 0x30000;

				m_auxPhys[j].pauxBoneInfo[k].iBone = i;
				m_auxPhys[j].pauxBoneInfo[k].dir0 = pr.pPoints[k + 1] - pr.pPoints[k];
				if (m_auxPhys[j].pauxBoneInfo[k].dir0.len2() > 0)
					m_auxPhys[j].pauxBoneInfo[k].dir0 *=
					  (m_auxPhys[j].pauxBoneInfo[k].rlen0 = 1.0f / m_auxPhys[j].pauxBoneInfo[k].dir0.GetLength());
				else
				{
					m_auxPhys[j].pauxBoneInfo[k].dir0(0, 0, 1);
					m_auxPhys[j].pauxBoneInfo[k].rlen0 = 1.0f;
				}
				m_auxPhys[j].pauxBoneInfo[k].quat0 = GetPoseData().GetJointAbsolute(i).q;

				pr.pPoints[k] = sp.q * pr.pPoints[k] + sp.pos;
				pr.pPoints[k + 1] = sp.q * pr.pPoints[k + 1] + sp.pos;
				pr.length += (pr.pPoints[k + 1] - pr.pPoints[k]).GetLength();
				iLastBone = GetModelJointChildIndex(i, 0);
				m_auxPhys[j].pauxBoneInfo[++k].iBone = iLastBone;
				m_auxPhys[j].pauxBoneInfo[k].dir0(0, 0, 1);
				m_auxPhys[j].pauxBoneInfo[k].rlen0 = 1.0f;
				m_auxPhys[j].pauxBoneInfo[k].quat0.SetIdentity();
				if (bCloth)
				{
					if (strncmp(m_auxPhys[j].strName, GetModelJointPointer(i = iLastBone)->m_strJointName.c_str(), m_auxPhys[j].nChars) || GetModelJointPointer(i)->m_numChildren == 0)
					{
						for (i = iSavedRoot, imin = -1; i < (int)numJoints; i++)
						{
							if (!strncmp(m_auxPhys[j].strName, GetModelJointPointer(i)->m_strJointName.c_str(), m_auxPhys[j].nChars) && strncmp(m_auxPhys[j].strName, GetModelJointPointer(getBoneParentIndex(i))->m_strJointName.c_str(), m_auxPhys[j].nChars))
							{
								for (i1 = 0; i1 < k && m_auxPhys[j].pauxBoneInfo[i1].iBone != i; i1++)
									;
								if (i1 == k && (imin < 0 || strcmp(GetModelJointPointer(i)->m_strJointName.c_str(), GetModelJointPointer(imin)->m_strJointName.c_str()) < 0))
									imin = i;
							}
						}
						if (imin >= 0)
							i = imin;
						++nStrands;
						++k;
					}
				}
				else
				{
					i = iLastBone;
				}
			}
			else
			{
				++i;
			}
		if (!bCloth && k != pr.nSegments)
		{
			bad_bones:
			g_pIPhysicalWorld->DestroyPhysicalEntity(m_auxPhys[j].pPhysEnt);

			delete[] m_auxPhys[j].pauxBoneInfo;
			m_auxPhys[j].pauxBoneInfo = NULL;

			delete[] m_auxPhys[j].pVtx;
			m_auxPhys[j].pVtx = NULL;

			memmove(&m_auxPhys[j], &m_auxPhys[j + 1], (m_nAuxPhys - 1 - j) * sizeof(m_auxPhys[0]));
			--m_nAuxPhys;
			--j;
			continue;
		}

		if (!is_unused(pr.idPartTiedTo[0]))
		{
			pr.pEntTiedTo[0] = pHost;
			pr.ptTiedTo[0] = pr.pPoints[0];
			m_auxPhys[j].bTied0 = true;
		}

		if (bCloth)
		{
cloth_aux:
			uint16 * pIdx = new uint16[k * 6];
			int nTris = 0, i2, i3, i0, idx;
			for (i1 = 1; i1 < k && !strncmp(GetModelJointPointer(getBoneParentIndex(m_auxPhys[j].pauxBoneInfo[i1].iBone))->m_strJointName.c_str(),
			                                m_auxPhys[j].strName, m_auxPhys[j].nChars); i1++)
				;
			for (i = 0, i0 = i1; i < k; i = i1, i1 = i3)
			{
				if (i1 == k)
				{
					if (!strstr(m_auxPhys[j].strName, "loop"))
						break;
					else
						i2 = 0, i3 = i0;
				}
				else
					for (i3 = (i2 = i1) + 1; i3 < k && !strncmp(GetModelJointPointer(getBoneParentIndex(m_auxPhys[j].pauxBoneInfo[i3].iBone))->m_strJointName.c_str(),
					                                            m_auxPhys[j].strName, m_auxPhys[j].nChars); i3++)
						;
				for (idx = 0; idx < i1 - i - 1 && idx < i3 - i2 - 1; idx++)
				{
					pIdx[nTris * 3] = i + idx;
					pIdx[nTris * 3 + 1] = i + idx + 1;
					pIdx[nTris * 3 + 2] = i2 + idx;
					nTris++;
					pIdx[nTris * 3] = i + idx + 1;
					pIdx[nTris * 3 + 1] = i2 + idx + 1;
					pIdx[nTris * 3 + 2] = i2 + idx;
					nTris++;
				}
				if (idx < i1 - i - 1)
					for (; idx < i1 - i - 1; idx++)
					{
						pIdx[nTris * 3] = i + idx;
						pIdx[nTris * 3 + 1] = i + idx + 1;
						pIdx[nTris * 3 + 2] = i3 - 1;
						nTris++;
					}
				else if (idx < i3 - i2 - 1)
					for (; idx < i3 - i2 - 1; idx++)
					{
						pIdx[nTris * 3] = i1 - 1;
						pIdx[nTris * 3 + 1] = i2 + idx + 1;
						pIdx[nTris * 3 + 2] = i2 + idx;
						nTris++;
					}
			}
			for (i = 0; i < k; i++)
				pr.pPoints[i] -= sp.pos;
			IGeometry* pMesh = g_pIPhysicalWorld->GetGeomManager()->CreateMesh(pr.pPoints, pIdx, 0, 0, nTris, mesh_SingleBB);
			delete[] pIdx;
			// get defSurfaceIdx from udp mat?
			phys_geometry* pgeom = pMesh ? g_pIPhysicalWorld->GetGeomManager()->RegisterGeometry(pMesh) : 0;
			if (pgeom)
				pgeom->nRefCount = 0;
			g_pIPhysicalWorld->DestroyPhysicalEntity(m_auxPhys[j].pPhysEnt);
			pe_params_pos pp;
			pp.pos = sp.pos;
			m_auxPhys[j].pPhysEnt = g_pIPhysicalWorld->CreatePhysicalEntity(PE_SOFT, &pp);
			pe_geomparams gp;
			gp.mass = 1.0f;
			gp.density = 800.0f;
			gp.flagsCollider = geom_colltype6;
			(gp.flags &= ~geom_colltype6) |= geom_can_modify;
			m_auxPhys[j].pPhysEnt->AddGeometry(pgeom, &gp);

			if (idxAttachment >= 0)
			{
				IAttachment* pAtt = m_pInstance->m_AttachmentManager.GetInterfaceByIndex(idxAttachment);
				if (IStatObj* pSrcObj = pAtt->GetIAttachmentObject()->GetIStatObj()->GetCloneSourceObject())
					((CCGFAttachment*)pAtt->GetIAttachmentObject())->pObj = pSrcObj;
				m_pInstance->m_AttachmentManager.DephysicalizeAttachment(idxAttachment, pHost);
				m_pInstance->m_AttachmentManager.PhysicalizeAttachment(idxAttachment, m_auxPhys[j].pPhysEnt, nLod);
			}

			ParsePhysInfoPropsCloth(GetJointPhysInfo(iFirstBone, nLod), nLod, psb, simp);
			m_auxPhys[j].pPhysEnt->SetParams(&psb);
			m_auxPhys[j].pPhysEnt->SetParams(&simp);
			m_auxPhys[j].pPhysEnt->SetParams(&poe);
			MARK_UNUSED simp.damping, simp.maxTimeStep, simp.minEnergy;
			simp.gravity.zero();

			pe_action_attach_points aap;
			aap.pEntity = pHost;
			aap.nPoints = 1;
			aap.piVtx = &i;
			for (i = 0; i < k; )
			{
				aap.partid = getBonePhysParentIndex(m_auxPhys[j].pauxBoneInfo[i].iBone, nLod);
				m_auxPhys[j].pPhysEnt->Action(&aap);
				for (i++; i < k && !strncmp(GetModelJointPointer(getBoneParentIndex(m_auxPhys[j].pauxBoneInfo[i].iBone))->m_strJointName.c_str(),
				                            m_auxPhys[j].strName, m_auxPhys[j].nChars); i++)
					;
			}

			if (idxAttachment >= 0)
			{
				IStatObj* pStatObj = m_pInstance->m_AttachmentManager.GetInterfaceByIndex(idxAttachment)->GetIAttachmentObject()->GetIStatObj();
				gEnv->p3DEngine->LoadStatObj(pStatObj->GetFilePath(), pStatObj->GetGeoName(), 0, false); // force synchronous rendermesh loading
				if (IRenderMesh* pRM = pStatObj->GetRenderMesh())
				{
					strided_pointer<ColorB> pColors(0);
					pRM->LockForThreadAccess();
					aap.partid = iFirstBone;
					if ((pColors.data = (ColorB*)pRM->GetColorPtr(pColors.iStride, FSL_READ)))
					{
						for (aap.nPoints = 0, i = 0; i < pRM->GetVerticesCount(); aap.nPoints += iszero(pColors[i++].g))
							;
						aap.piVtx = new int[aap.nPoints + 1];
						for (i = k = 0; i < pRM->GetVerticesCount(); k += iszero(pColors[i++].g))
							aap.piVtx[k] = i;
						m_auxPhys[j].pPhysEnt->Action(&aap);
						delete[] aap.piVtx;
					}
					pRM->UnLockForThreadAccess();
					pStatObj->UpdateVertices(0, 0, 0, 0, 0);
					MARK_UNUSED atv.posHost;
					m_auxPhys[j].pPhysEnt->Action(&atv);
				}
			}

			int idxParent = m_auxPhys[j].iBoneTiedTo[0] >= 0 ? getBonePhysParentIndex(m_auxPhys[j].iBoneTiedTo[0], nLod) : -1;
			if (idxParent >= 0)
			{
				Quat qParent = GetPoseData().GetJointAbsolute(idxParent).q;
				for (k = 0; k < m_auxPhys[j].nBones; k++)
				{
					m_auxPhys[j].pauxBoneInfo[k].dir0 = !qParent * m_auxPhys[j].pauxBoneInfo[k].dir0;
					m_auxPhys[j].pauxBoneInfo[k].quat0 = !qParent * m_auxPhys[j].pauxBoneInfo[k].quat0;
				}
			}

			if (psb.stiffnessAnim > 0.0f && m_auxPhys[j].nBones)
			{
				QuatT qBase = GetPoseData().GetJointAbsolute(pr.idPartTiedTo[is_unused(pr.idPartTiedTo[0])]).GetInverted();
				atv.points = m_auxPhys[j].pVtx;
				atv.nPoints = m_auxPhys[j].nBones + 1;
				for (i = 0; i < m_auxPhys[j].nBones; i++)
				{
					k = m_auxPhys[j].pauxBoneInfo[i].iBone;
					atv.points[i] = qBase * GetPoseData().GetJointAbsolute(k).t;
				}
				atv.points[i] = qBase * GetPoseData().GetJointAbsolute(GetModelJointChildIndex(k, 0)).t;
				m_auxPhys[j].pPhysEnt->Action(&atv);
			}

			continue;
		}

		char boneAttachName[128];
		int iOtherBone = -1;
		cry_sprintf(boneAttachName, "attach_%.*s", m_auxPhys[j].nChars, m_auxPhys[j].strName);
		if ((iOtherBone = GetJointIDByName(boneAttachName)) >= 0 && (i = getBonePhysParentOrSelfIndex(iOtherBone, nLod)) >= 0 || (i = getBonePhysChildIndex(iLastBone)) >= 0)
		{
			pr.pEntTiedTo[1] = pHost;
			m_auxPhys[j].iBoneTiedTo[1] = iOtherBone >= 0 ? -iOtherBone - 1 : iLastBone;
			pr.idPartTiedTo[1] = i;
			pr.ptTiedTo[1] = pr.pPoints[pr.nSegments];
			m_auxPhys[j].bTied1 = true;
		}
		//if (pForcedRopeLen && *piForcedRopeIdx+i<nForcedRopes)
		//	pr.length = pForcedRopeLen[*piForcedRopeIdx+j];

		const CryBonePhysics& physInfo = GetJointPhysInfo(iFirstBone, nLod);
		if (!(physInfo.flags & joint_isolated_accelerations))
		{
			ParsePhysInfoProps(physInfo, nLod, pr, simp, pf, rDefaultSkeleton.GetPoseData().GetJointsRelative()[iFirstBone].q);
			if (physInfo.framemtx[0][1] >= 2.0f && physInfo.framemtx[0][1] < 100.0f)
			{
				cry_sprintf(boneAttachName, "rope_stiffness_controller_%d", FtoI(physInfo.framemtx[0][1] - 1.0f));
				m_auxPhys[j].iBoneStiffnessController = GetJointIDByName(boneAttachName);
			}
			m_auxPhys[j].bPhysical = true;
			if (m_auxPhys[j].bTied0)
			{
				Quat qParent = GetPoseData().GetJointAbsolute(getBonePhysParentIndex(m_auxPhys[j].iBoneTiedTo[0], nLod)).q;
				for (k = 0; k < m_auxPhys[j].nBones; k++)
				{
					m_auxPhys[j].pauxBoneInfo[k].dir0 = !qParent * m_auxPhys[j].pauxBoneInfo[k].dir0;
					m_auxPhys[j].pauxBoneInfo[k].quat0 = !qParent * m_auxPhys[j].pauxBoneInfo[k].quat0;
				}
			}
			else
				pf.flagsOR &= ~rope_target_vtx_rel0;
		}
		else
		{
			MARK_UNUSED pr.bTargetPoseActive, pr.jointLimit, pr.stiffnessAnim, pr.dampingAnim, pr.stiffnessDecayAnim, simp.gravity, simp.maxTimeStep;
			m_auxPhys[j].bPhysical = false;
			if (!nLod)
				simp.minEnergy = 0;
		}
		pr.surface_idx = *(int*)(physInfo.spring_angle + 1);
		pr.noCollDist = pf.flagsOR & rope_collides_with_attachment ? 0.0f : 0.3f;

		m_auxPhys[j].pPhysEnt->SetParams(&pr);
		m_auxPhys[j].pPhysEnt->SetParams(&simp);
		m_auxPhys[j].pPhysEnt->SetParams(&poe);
		if (m_nAuxPhys > 1 || m_auxPhys[j].bPhysical)
		{
			m_auxPhys[j].pPhysEnt->SetParams(&pf);
			pe_action_awake aa;
			aa.bAwake = m_auxPhys[j].bPhysical;
			m_auxPhys[j].pPhysEnt->Action(&aa);
		}
		if (pf.flagsOR & rope_target_vtx_rel0)
		{
			QuatT qBase = GetPoseData().GetJointAbsolute(pr.idPartTiedTo[is_unused(pr.idPartTiedTo[0])]).GetInverted();
			atv.points = m_auxPhys[j].pVtx;
			atv.nPoints = m_auxPhys[j].nBones + 1;
			for (i = 0; i < m_auxPhys[j].nBones; i++)
			{
				k = m_auxPhys[j].pauxBoneInfo[i].iBone;
				atv.points[i] = qBase * GetPoseData().GetJointAbsolute(k).t;
			}
			atv.points[i] = qBase * GetPoseData().GetJointAbsolute(GetModelJointChildIndex(k, 0)).t;
			m_auxPhys[j].pPhysEnt->Action(&atv);
		}
	}

	/*if (nLod==1)
	   {
	   int nAuxPhys;
	   for(i=nAuxPhys=0;i<m_nAuxPhys;i++)
	   {
	    for(j=0;j<m_auxPhys[i].nBones && !GetModelJointPointer(m_auxPhys[i].pauxBoneInfo[j].iBone)->m_PhysInfo.pPhysGeom;j++);
	    if (j<m_auxPhys[i].nBones)
	    {
	      g_pIPhysicalWorld->DestroyPhysicalEntity(m_auxPhys[i].pPhysEnt);
	      delete[] m_auxPhys[i].pauxBoneInfo;
	      delete[] m_auxPhys[i].pVtx;
	    } else
	      m_auxPhys[nAuxPhys++] = m_auxPhys[i];
	   }
	   m_nAuxPhys = nAuxPhys;
	   }*/

	m_vOffset = offset;
	m_fScale = scale;
	/* IZF temp
	   //if (pForcedRopeLen)
	   //	*piForcedRopeIdx += j;
	   IAttachmentObject* pAttachment;
	   ICharacterInstance *pChar;
	   IPhysicalEntity *pRope;
	   for(int ic=m_pInstance->m_AttachmentManager.GetAttachmentCount()-1; ic>=0; ic--)
	   {
	    if (m_pInstance->m_AttachmentManager.GetInterfaceByIndex(ic)->GetType()==CA_BONE && (pAttachment = m_pInstance->m_AttachmentManager.GetInterfaceByIndex(ic)->GetIAttachmentObject()) && (pChar=pAttachment->GetICharacterInstance()))
	    {
	      ((CCharInstance*)pChar)->m_SkeletonPose.CreateAuxilaryPhysics(pHost, mtx, scale,offset, nLod);//, pForcedRopeLen,piForcedRopeIdx,nForcedRopes);
	      for(int j0=0; pRope=pChar->GetISkeletonPose()->GetCharacterPhysics(j0); j0++)
	      {
	        pe_params_rope pr;
	        pr.pEntTiedTo[0] = pHost;
	        pr.idPartTiedTo[0] = getBonePhysParentOrSelfIndex(m_pInstance->m_AttachmentManager.GetInterfaceByIndex(ic)->GetBoneID());
	        pRope->SetParams(&pr);
	      }
	    }
	   }*/
	return m_nAuxPhys;
}

int CSkeletonPhysics::FillRopeLenArray(float* len, int i0, int sz)
{
	int i, i0org = i0;
	pe_params_rope pr;
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	for (i = 0; i < m_nAuxPhys && i + i0 < sz; i++)
	{
		int32 status = m_auxPhys[i].pPhysEnt->GetParams(&pr);
		if (status == 0)
		{
			g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, rDefaultSkeleton.GetModelFilePath(), "GetParams() returned 0");
			assert(0);
		}
		len[i0 + i] = pr.length;
	}
	i0 += i;

	/* IZF: Refactor
	   IAttachmentObject* pAttachment;
	   ICharacterInstance *pChar;
	   for(i=m_pInstance->m_AttachmentManager.GetAttachmentCount()-1; i>=0; i--)
	    if (m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetType()==CA_BONE &&
	      (pAttachment = m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetIAttachmentObject()) && (pChar=pAttachment->GetICharacterInstance()))
	      i0 += ((CCharInstance*)pChar)->m_SkeletonPose.FillRopeLenArray(len,i0,sz);
	 */
	return i0 - i0org;
}

void CSkeletonPhysics::InitializeAnimToPhysIndexArray()
{
	if (m_nPhysJoints > -1)
		return;

	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	uint32 jointCount = rDefaultSkeleton.GetJointCount();
	m_nPhysJoints = 0;

	for (uint32 i = 0; i < jointCount; ++i)
	{
		if (GetModelJointPointer(i)->m_PhysInfo.pPhysGeom)
			m_nPhysJoints++;
	}

	if (!m_nPhysJoints)
		return;

	m_physJoints.resize(m_nPhysJoints);
	m_physJointsIdx.resize(m_nPhysJoints);
	uint32 j = 0;
	for (uint32 i = 0; i < jointCount; ++i)
	{
		if (GetModelJointPointer(i)->m_PhysInfo.pPhysGeom)
			m_physJointsIdx[j++] = i;
	}
}

void CSkeletonPhysics::SynchronizeWithPhysicalEntity(Skeleton::CPoseData& poseData, IPhysicalEntity* pent, const Vec3& posMaster, const Quat& qMaster, QuatT offset, int iDir)
{
	if (iDir == 0 || (iDir < 0 && pent->GetType() != PE_ARTICULATED && pent->GetType() != PE_RIGID))
	{
		if (pent)
		{
			Physics_SynchronizeToEntity(*pent, offset);
		}
	}
	else
	{
		Job_Physics_SynchronizeFromEntity(poseData, pent, offset);
	}
}

// IZF: We use this for calls outside CryAnimation, which is bad since this
// might write into the pose data as we are processing it in an animation task.
void CSkeletonPhysics::SynchronizeWithPhysicalEntity(IPhysicalEntity* pent, const Vec3& posMaster, const Quat& qMaster)
{
	Skeleton::CPoseData& poseDataWriteable = GetPoseDataForceWriteable();

	SetLocation(IDENTITY);
	uint mode = m_pInstance->m_CharEditMode;
	m_pInstance->m_CharEditMode |= CA_CharacterTool; // force synchronization
	Job_SynchronizeWithPhysicsPrepare(*CharacterInstanceProcessing::GetMemoryPool());
	if (pent)
		SynchronizeWithPhysicalEntity(poseDataWriteable, pent, posMaster, qMaster, QuatT(IDENTITY));
	Job_Physics_SynchronizeFromAux(poseDataWriteable);
	m_pInstance->m_CharEditMode = mode;
	//	UpdateBBox(1);
}

void CSkeletonPhysics::CreateRagdollDefaultPose(Skeleton::CPoseData& poseData)
{
	const QuatT* const __restrict pJointRelative = poseData.GetJointsRelative();
	const QuatT* const __restrict pJointAbsolute = poseData.GetJointsAbsolute();
	uint32 numJoints = poseData.GetJointCount();

	CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	CDefaultSkeleton::SJoint* parrModelJoints = &rDefaultSkeleton.m_arrModelJoints[0];

	for (uint32 i = 0; i < numJoints; ++i)
	{
		int32 pidx = parrModelJoints[i].m_idxParent;
		if (pidx >= 0)
		{
			int32 ParentIdx = parrModelJoints[i].m_idxParent;
			m_arrPhysicsJoints[i].m_DefaultRelativeQuat = pJointAbsolute[i];
			if (ParentIdx >= 0)
				m_arrPhysicsJoints[i].m_DefaultRelativeQuat = pJointAbsolute[ParentIdx].GetInverted() * pJointAbsolute[i];
			int32 p, pp;
			for (p = pp = parrModelJoints[i].m_idxParent; pp > -1 && parrModelJoints[pp].m_PhysInfo.pPhysGeom == 0; pp = parrModelJoints[pp].m_idxParent)
				;
			if (pp > -1)
				m_arrPhysicsJoints[i].m_qRelPhysParent = !pJointAbsolute[p].q * pJointAbsolute[pp].q;
			else
				m_arrPhysicsJoints[i].m_qRelPhysParent.SetIdentity();
		}
	}
}

IPhysicalEntity* CSkeletonPhysics::RelinquishCharacterPhysics(const Matrix34& mtx, float stiffness, bool bCopyJointVelocities, const Vec3& velHost)
{
	Skeleton::CPoseData& poseDataWriteable = GetPoseDataForceWriteable();
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;

	poseDataWriteable.ValidateRelative(rDefaultSkeleton);

	int nLod = 1;
	int iRoot = GetPhysRoot(), numJoints = rDefaultSkeleton.GetJointCount();
	if (iRoot < 0)
		return nullptr;
	m_bPhysicsRelinquished = true;

	if (m_arrPhysicsJoints.empty())
		InitPhysicsSkeleton();
	poseDataWriteable.ComputeRelativePose(rDefaultSkeleton);

	CreateRagdollDefaultPose(poseDataWriteable);

	// store death pose (current) matRelative orientation in bone's m_pqTransform
	for (int i = 0; i < (int)numJoints; i++)
		m_arrPhysicsJoints[i].m_qRelFallPlay = poseDataWriteable.GetJointRelative(i).q;

	ResetNonphysicalBoneRotations(poseDataWriteable, nLod, 1.0f);        // reset nonphysical bones matRel to default pose matRel
	UnconvertBoneGlobalFromRelativeForm(poseDataWriteable, false, nLod); // build matGlobals from matRelativeToParents

	pe_params_articulated_body pab;
	pab.bGrounded = 0;
	pab.scaleBounceResponse = 1;
	pab.bCheckCollisions = 1;
	pab.bCollisionResp = 1;

	IPhysicalEntity* res = g_pIPhysicalWorld->CreatePhysicalEntity(PE_ARTICULATED, &pab);//,&pp,0);
	pe_simulation_params sp;
	sp.iSimClass = 6; // to make sure the entity is not processed until it's ready (multithreaded)
	res->SetParams(&sp);
	m_vOffset.zero();
	BuildPhysicalEntity(res, m_fMass, m_iSurfaceIdx, stiffness, nLod, -1, Matrix34(Vec3(m_fScale), Quat(IDENTITY), Vec3(ZERO)));

	pe_params_joint pj;
	pj.bNoUpdate = 1;
	if (!stiffness)
		for (int i = 0; i < 3; i++)
			pj.kd[i] = 0.1f;
	for (int i = numJoints - 1; i >= 0; i--)
	{
		if (GetModelJointPointer(i)->m_PhysInfo.pPhysGeom)
		{
			pj.op[1] = i;
			if (i == iRoot)
				pj.flags = 0;
			res->SetParams(&pj);
		}
	}

	m_fPhysBlendMaxTime = Console::GetInst().ca_DeathBlendTime;
	if (m_fPhysBlendMaxTime > 0.001f)
	{
		m_frPhysBlendMaxTime = 1.0f / m_fPhysBlendMaxTime;
	}
	else
	{
		m_fPhysBlendMaxTime = 0.0f;
		m_frPhysBlendMaxTime = 1.0f;
	}

	// No blending if this ragdoll is caused by loading a checkpoint
	m_fPhysBlendTime = !gEnv->pSystem->IsSerializingFile() ? 0.0f : m_fPhysBlendMaxTime;

	ResetNonphysicalBoneRotations(poseDataWriteable, nLod, 0.0f);        // restore death pose matRel from m_pqTransform
	UnconvertBoneGlobalFromRelativeForm(poseDataWriteable, false, nLod); // build matGlobals from matRelativeToParents

	m_vOffset.zero();
	m_bPhysicsAwake = m_bPhysicsWasAwake = 1;
	pe_params_pos pp;
	pp.pos = mtx.GetTranslation();
	pp.q = Quat(Matrix33(mtx) / mtx.GetColumn(0).len());
	int bSkelQueued = res->SetParams(&pp) - 1;

	pe_params_rope pr;
	pe_params_flags pf;
	pe_params_timeout pto;
	pe_simulation_params spr;
	pe_params_outer_entity poe;
	pr.bLocalPtTied = 1;
	pto.maxTimeIdle = 4.0f;
	poe.pOuterEntity = res;
	int nAuxPhys = 0;
	for (int i = 0; i < m_nAuxPhys; i++)
	{
		int j = 0;
		for (j = 0; j < m_auxPhys[i].nBones && !GetModelJointPointer(m_auxPhys[i].pauxBoneInfo[j].iBone)->m_PhysInfo.pPhysGeom; j++)
			;
		if (j < m_auxPhys[i].nBones)
		{
			g_pIPhysicalWorld->DestroyPhysicalEntity(m_auxPhys[i].pPhysEnt);

			delete[] m_auxPhys[i].pauxBoneInfo;
			m_auxPhys[i].pauxBoneInfo = NULL;

			delete[] m_auxPhys[i].pVtx;
			m_auxPhys[i].pVtx = NULL;
		}
		else
		{
			const CryBonePhysics& physInfo = GetJointPhysInfo(m_auxPhys[i].nBones ? m_auxPhys[i].pauxBoneInfo[0].iBone : m_auxPhys[i].iBoneTiedTo[0], 1);
			if (m_auxPhys[i].pPhysEnt->GetType() == PE_SOFT)
			{
				pe_params_softbody psb;
				ParsePhysInfoPropsCloth(physInfo, nLod, psb, spr);
				m_auxPhys[i].pPhysEnt->SetParams(&psb, -bSkelQueued >> 31);

				pe_action_attach_points aap;
				aap.pEntity = 0;
				if (!m_auxPhys[j].nBones)
				{
					pe_params_part pp;
					pp.ipart = 0;
					m_auxPhys[i].pPhysEnt->GetParams(&pp);
					res->RemoveGeometry(pp.partid);
					MARK_UNUSED aap.nPoints;
					aap.pEntity = res;
				}
				else
					aap.piVtx = 0;
				m_auxPhys[i].pPhysEnt->Action(&aap, -bSkelQueued >> 31);
				Vec3 pt;
				aap.points = &pt;
				aap.pEntity = res;
				aap.bLocal = 1;
				aap.nPoints = 1;
				aap.piVtx = &j;
				for (j = 0; j < m_auxPhys[i].nBones; )
				{
					aap.partid = getBonePhysParentIndex(m_auxPhys[i].pauxBoneInfo[j].iBone, nLod);
					pt =
					  poseDataWriteable.GetJointAbsolute(aap.partid).GetInverted() *
					  poseDataWriteable.GetJointAbsolute(getBoneParentIndex(m_auxPhys[i].pauxBoneInfo[j].iBone)) *
					  m_arrPhysicsJoints[m_auxPhys[i].pauxBoneInfo[j].iBone].m_DefaultRelativeQuat.t * m_fScale;
					m_auxPhys[i].pPhysEnt->Action(&aap, -bSkelQueued >> 31);
					for (j++; j < m_auxPhys[i].nBones && !strncmp(GetModelJointPointer(getBoneParentIndex(m_auxPhys[i].pauxBoneInfo[j].iBone))->m_strJointName.c_str(), m_auxPhys[i].strName, m_auxPhys[i].nChars); j++)
						;
				}
			}
			else
			{
				if (!(physInfo.flags & joint_isolated_accelerations))
				{
					ParsePhysInfoProps(physInfo, nLod, pr, spr, pf, rDefaultSkeleton.GetPoseData().GetJointsRelative()[m_auxPhys[i].pauxBoneInfo[0].iBone].q);
				}
				else
				{
					MARK_UNUSED pr.bTargetPoseActive, pr.jointLimit, pr.stiffnessAnim, pr.dampingAnim, pr.stiffnessDecayAnim, spr.gravity, spr.maxTimeStep;
					pr.bTargetPoseActive = 0;
					pr.collDist = 0.005f;
					pr.dampingAnim = 0.0f;
					spr.gravity = Vec3(0, 0, -6.0f);
					spr.damping = 1.0f;
				}

				if (m_auxPhys[i].bTied0)
				{
					pr.pEntTiedTo[0] = res;
					pr.idPartTiedTo[0] = getBonePhysParentIndex(j = m_auxPhys[i].iBoneTiedTo[0], nLod);
					Vec3 ptTiedTo =
					  poseDataWriteable.GetJointAbsolute(getBoneParentIndex(j)) *
					  m_arrPhysicsJoints[j].m_DefaultRelativeQuat.t * m_fScale;
					ptTiedTo = poseDataWriteable.GetJointAbsolute(pr.idPartTiedTo[0]).GetInverted() * ptTiedTo;
					if (ptTiedTo.len2() < 1.0f)
						pr.ptTiedTo[0] = ptTiedTo;
				}

				MARK_UNUSED pr.pEntTiedTo[1], pr.idPartTiedTo[1], pr.ptTiedTo[1];
				if (m_auxPhys[i].bTied1)
				{
					pr.pEntTiedTo[1] = res;
					pr.idPartTiedTo[1] = m_auxPhys[i].iBoneTiedTo[1] >= 0 ? getBonePhysChildIndex(m_auxPhys[i].iBoneTiedTo[1], nLod) :
					                     getBonePhysParentOrSelfIndex(-m_auxPhys[i].iBoneTiedTo[1] - 1, nLod);
					int iBone = m_auxPhys[i].pauxBoneInfo[m_auxPhys[i].nBones - 1].iBone;
					iBone = GetModelJointPointer(iBone)->m_numChildren > 0 ? GetModelJointChildIndex(iBone, 0) : pr.idPartTiedTo[1];
					pr.ptTiedTo[1] = poseDataWriteable.GetJointAbsolute(pr.idPartTiedTo[1]).GetInverted() * poseDataWriteable.GetJointAbsolute(iBone).t;
				}

				if (m_auxPhys[i].bPhysical)
					pf.flagsAND &= ~rope_ignore_attachments;
				m_auxPhys[i].pPhysEnt->SetParams(&pr, -bSkelQueued >> 31);
			}
			pf.flagsAND = ~pef_ignore_areas;
			m_auxPhys[i].pPhysEnt->SetParams(&pf, -bSkelQueued >> 31);
			m_auxPhys[i].pPhysEnt->SetParams(&spr, -bSkelQueued >> 31);
			m_auxPhys[i].pPhysEnt->SetParams(&pto, -bSkelQueued >> 31);
			m_auxPhys[i].pPhysEnt->SetParams(&poe, -bSkelQueued >> 31);
			m_auxPhys[nAuxPhys++] = m_auxPhys[i];
		}
	}
	m_nAuxPhys = nAuxPhys;

	MARK_UNUSED pr.pEntTiedTo[1], pr.idPartTiedTo[1];
	IAttachmentObject* pAttachment;
	ICharacterInstance* pChar;
	IPhysicalEntity* pRope;
	for (int i = m_pInstance->m_AttachmentManager.GetAttachmentCount() - 1; i >= 0; i--)
	{
		if (m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetType() == CA_BONE
		    && (pAttachment = m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetIAttachmentObject())
		    && (pChar = pAttachment->GetICharacterInstance()))
		{
			for (int j = 0; pRope = pChar->GetISkeletonPose()->GetCharacterPhysics(j); j++)
			{
				pr.pEntTiedTo[0] = res;
				pr.idPartTiedTo[0] = getBonePhysParentOrSelfIndex(m_pInstance->m_AttachmentManager.GetInterfaceByIndex(i)->GetJointID());
				pRope->SetParams(&pr);
				pRope->SetParams(&poe);
			}
		}
	}

	if (m_pCharPhysics)
	{
		if (bCopyJointVelocities)
		{
			// Copy part dynamics
			switch (Console::GetInst().ca_ApplyJointVelocitiesMode)
			{
			case Console::ApplyJointVelocitiesAnimation:
				m_pInstance->ApplyJointVelocitiesToPhysics(res, pp.q, velHost);
				break;

			case Console::ApplyJointVelocitiesPhysics:
				{
					pe_status_dynamics partStatusSource;
					pe_action_set_velocity asv;
					for (int i = numJoints - 1; i >= 0; i--)
					{
						if (GetModelJointPointer(i)->m_PhysInfo.pPhysGeom)
						{
							partStatusSource.partid = i;
							if (!m_pCharPhysics->GetStatus(&partStatusSource))
								continue;

							asv.partid = i;
							asv.v = partStatusSource.v + velHost;
							asv.w = partStatusSource.w;
							res->Action(&asv);
						}
					}
				}
				break;
			}
		}

		g_pIPhysicalWorld->DestroyPhysicalEntity(m_pCharPhysics);
	}
	if (m_ppBonePhysics)
	{
		for (int i = 0; i < (int)numJoints; i++)
		{
			if (m_ppBonePhysics[i])
			{
				m_ppBonePhysics[i]->Release();
				g_pIPhysicalWorld->DestroyPhysicalEntity(m_ppBonePhysics[i]);
			}
		}
		delete[] m_ppBonePhysics;
		m_ppBonePhysics = 0;
	}

	m_pCharPhysics = res;
	m_bPhysicsRelinquished = 1;
	m_nSpineBones = 0;
	m_bLimpRagdoll = 0;
	m_timeStandingUp = -1;

	sp.iSimClass = 2;
	res->SetParams(&sp);

	return res;
}

bool CSkeletonPhysics::AddImpact(const int partid, Vec3 point, Vec3 impact)
{
	if (m_pCharPhysics)
	{
		const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
		uint32 numJoints = rDefaultSkeleton.GetJointCount();
		if (!Console::GetInst().ca_physicsProcessImpact ||
		    rDefaultSkeleton.m_ObjectType == CGA || (unsigned int)partid >= (unsigned int)numJoints)
		{
			// Exit early if physic impacts aren't processed
			// sometimes out of range value (like -1) is passed to indicate that no impulse should be added to character
			return false;
		}
		//int i;

		//SJoint* pImpactBone = &m_arrJoints[partid];
		const char* szImpactBoneName = GetModelJointPointer(partid)->m_strJointName.c_str();

		//for(i=0; i<4 && !(m_pIKEffectors[i] && m_pIKEffectors[i]->AffectsBone(partid)); i++);
		const char* ptr = strstr(szImpactBoneName, "Spine");
		if (strstr(szImpactBoneName, "Pelvis") || ptr && !ptr[5])
			return false;

		if (strstr(szImpactBoneName, "UpperArm") || strstr(szImpactBoneName, "Forearm"))
			impact *= 0.35f;
		else if (strstr(szImpactBoneName, "Hand"))
			impact *= 0.2f;
		else if (strstr(szImpactBoneName, "Spine1"))
			impact *= 0.2f;

		pe_action_impulse impulse;
		impulse.partid = partid;
		impulse.impulse = impact;
		impulse.point = point;
		m_pCharPhysics->Action(&impulse);

		return true;
	}
	else
	{
		pe_action_impulse impulse;
		impulse.impulse = impact;
		impulse.point = point;
		for (int i = 0; i < m_nAuxPhys; i++)
			m_auxPhys[i].pPhysEnt->Action(&impulse);
	}

	return false;
}

// Forces skinning on the next frame
void CSkeletonPhysics::ForceReskin()
{
	m_bPhysicsWasAwake = true;
}

void CSkeletonPhysics::RequestForcedPostSynchronization()
{
	m_bForcePostSynchronization = true;
}

// Process

void CSkeletonPhysics::Physics_SynchronizeToAux(const Skeleton::CPoseData& poseData)
{
	if (!m_bPhysicsSynchronizeAux)
		return;
	if (!m_pPhysBuffer)
		return;
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;

	pe_params_articulated_body pab;
	if (m_pCharPhysics)
		m_pCharPhysics->GetParams(&pab);

	int nLod = m_bPhysicsRelinquished ? GetPhysicsLod() : 0;

	pe_action_target_vtx atv;
	Matrix34 mtx = Matrix34(m_location);
	int bRopesAwake = 0;
	for (int j = 0; j < m_nAuxPhys; ++j)
	{
		const pe_type peType = m_auxPhys[j].pPhysEnt->GetType();

		QuatT qtParent(IDENTITY), qtParent0(IDENTITY);
		if (m_auxPhys[j].bTied0)
		{
			int parentIndex = getBonePhysParentIndex(m_auxPhys[j].iBoneTiedTo[0], nLod);

			if (m_pPhysBuffer && m_bPhysBufferFilled)
				for (int i = getBoneParentIndex(m_auxPhys[j].iBoneTiedTo[0]); i != parentIndex; i = getBoneParentIndex(i))
					qtParent = m_pPhysBuffer[i].location * qtParent;
			qtParent0 = qtParent;
		}

		pe_params_rope pr;
		pr.bTargetPoseActive = 0;

		// If we are not a ragdoll and we have a valid pose, use the pose to
		// drive rope by setting a target position.
		if (m_pSkeletonAnim->m_IsAnimPlaying && !m_bPhysicsRelinquished)
		{
			pr.bTargetPoseActive = 1;
			for (int i = 0; i < m_auxPhys[j].nBones; ++i)
			{
				pr.bTargetPoseActive =
				  pr.bTargetPoseActive &&
				  (poseData.GetJointsStatus()[m_auxPhys[j].pauxBoneInfo[i].iBone] & eJS_Orientation);
			}
		}

		pe_status_rope sr;
		if (peType == PE_SOFT)
		{
			pe_status_softvtx ssv;
			sr.pPoints = m_auxPhys[j].pVtx;
			if (m_auxPhys[j].pPhysEnt->GetStatus(&ssv))
			{
				if (m_auxPhys[j].nBones <= 0)
				{
					if (IAttachment* pAtt = m_pInstance->m_AttachmentManager.GetInterfaceByNameCRC(m_auxPhys[j].iBoneTiedTo[1]))
						if (IStatObj* pStatObj = pAtt->GetIAttachmentObject()->GetIStatObj())
							if (pAtt->GetIAttachmentObject()->GetAttachmentType() == IAttachmentObject::eAttachment_StatObj)
							{
								IStatObj* pStatObjNew = pStatObj->UpdateVertices(ssv.pVtx, ssv.pNormals, 0, ssv.nVtx, ssv.pVtxMap);
								if (pStatObjNew != pStatObj)
								{
									ssv.pMesh->SetForeignData(pStatObjNew, 0);
									((CCGFAttachment*)pAtt->GetIAttachmentObject())->pObj = pStatObjNew;
								}
								pe_params_part pp;
								pp.ipart = 0;
								m_auxPhys[j].pPhysEnt->GetParams(&pp);
								QuatT qphysBoneW(ssv.qHost, ssv.posHost + ssv.pos), qclothW(ssv.q, ssv.pos), qpart(pp.q, pp.pos);
								if (!m_auxPhys[j].pauxBoneInfo)
								{
									m_auxPhys[j].pauxBoneInfo = new aux_bone_info[1];
									QuatT q0 = pAtt->GetAttRelativeDefault();
									m_auxPhys[j].pauxBoneInfo->quat0 = q0.q;
									m_auxPhys[j].pauxBoneInfo->dir0 = q0.t;
								}
								pAtt->SetAttRelativeDefault(qphysBoneW.GetInverted() * qclothW * qpart);
							}
					continue;
				}
				for (int i = 0; i <= m_auxPhys[j].nBones; ++i)
					sr.pPoints[i] = ssv.pVtx[i];

				sr.bTargetPoseActive = pr.bTargetPoseActive;
				const CryBonePhysics& physInfo = GetJointPhysInfo(m_auxPhys[j].iBoneTiedTo[0], nLod);
				sr.bTargetPoseActive = sr.bTargetPoseActive && physInfo.min[0] > 0.0f;
			}	else 
				continue;
			sr.nVtx = 0;
		}
		else
		{
			sr.pPoints = NULL;
			sr.pVtx = NULL;

			if (!m_auxPhys[j].pPhysEnt->GetStatus(&sr))
				continue;

			if (m_auxPhys[j].iBoneStiffnessController >= 0)
			{
				Quat q = poseData.GetJointRelative(m_auxPhys[j].iBoneStiffnessController).q;
				if (m_pPhysBuffer)
					q = m_pPhysBuffer[m_auxPhys[j].iBoneStiffnessController].location.q;

				float angle = RAD2DEG(atan2_tpl(q.v.len(), q.w)) * 2.0f;
				angle -= 360 * isneg(180.0f - angle);

				if (angle < 0.01f && angle > -90.0f)
					angle = 0.01f;
				else if (angle >= 100.0f || angle < -90.0f)
					angle = 0.0f;

				CryBonePhysics& physInfo = GetModelJointPointer(m_auxPhys[j].iBoneTiedTo[0])->m_PhysInfo;
				pr.stiffnessAnim = RAD2DEG(physInfo.max[0]) * angle * 0.01f;
				pr.dampingAnim = RAD2DEG(physInfo.max[2]) * angle * 0.01f;
			}

			if (m_pCharPhysics)
			{
				if (sr.bTargetPoseActive != 2)
					m_auxPhys[j].pPhysEnt->SetParams(&pr);
			}
			/*
			      if (sr.nVtx)
			      {
			        if (sr.nVtx>m_auxPhys[j].nSubVtxAlloc)
			        {
			          if (m_auxPhys[j].pSubVtx)
			            delete[] m_auxPhys[j].pSubVtx;

			          m_auxPhys[j].pSubVtx = new Vec3[m_auxPhys[j].nSubVtxAlloc = (sr.nVtx-1&~15)+16];
			        }

			        sr.pVtx = m_auxPhys[j].pSubVtx;
			        m_auxPhys[j].pPhysEnt->GetStatus(&sr);
			        sr.pPoints = m_auxPhys[j].pVtx;

			        float len = 0.0f;
			        for (int i=0; i<sr.nVtx-1; ++i)
			          len += (sr.pVtx[i+1]-sr.pVtx[i]).GetLength();

			        f32 rnSegs = 1.0f / m_auxPhys[j].nBones;
			        int i;
			        int iter = 3;
			        do
			        {
			          sr.pPoints[0] = sr.pVtx[0];
			          int ivtx;
			          for (i=ivtx=0; i<m_auxPhys[j].nBones && ivtx<sr.nVtx-1;)
			          {
			            Vec3 dir = sr.pVtx[ivtx+1]-sr.pVtx[ivtx];
			            Vec3 v0 = sr.pVtx[ivtx]-sr.pPoints[i];
			            f32 ka = dir.len2();
			            f32 kb = v0 * dir;
			            f32 kc = v0.len2() - sqr(len * rnSegs);
			            f32 kd = sqrt_tpl(max(0.0f, kb*kb - ka*kc));
			            if (kd-kb<ka)
			              sr.pPoints[++i] = sr.pVtx[ivtx]+dir*((kd-kb)/ka);
			            else
			   ++ivtx;
			          }
			          len *= (1.0f - (m_auxPhys[j].nBones-i) * rnSegs);
			          len += (sr.pVtx[sr.nVtx-1] - sr.pPoints[i]).GetLength();
			        } while(--iter);

			        sr.pPoints[m_auxPhys[j].nBones] = sr.pVtx[sr.nVtx-1];
			        if (i+1<m_auxPhys[j].nBones)
			        {
			          Vec3 dir = sr.pPoints[m_auxPhys[j].nBones] - sr.pPoints[i];
			          rnSegs = 1.0f / (m_auxPhys[j].nBones-i);

			          for (int ivtx = i+1; ivtx < m_auxPhys[j].nBones; ++ivtx)
			            sr.pPoints[ivtx] = sr.pPoints[i] + dir * ((ivtx-i) * rnSegs);
			        }
			      }
			      else
			      {
			        sr.pPoints = m_auxPhys[j].pVtx;
			        m_auxPhys[j].pPhysEnt->GetStatus(&sr);
			      }
			 */
			if (m_auxPhys[j].pVtx)
			{
				sr.pPoints = m_auxPhys[j].pVtx;
				m_auxPhys[j].pPhysEnt->GetStatus(&sr);
			}

		}

		assert(sr.pPoints);
		atv.points = sr.pPoints;
		for (int i = 0; i < m_auxPhys[j].nBones; ++i)
		{
			if (i > 0 && rDefaultSkeleton.m_arrModelJoints[m_auxPhys[j].pauxBoneInfo[i].iBone].m_idxParent != m_auxPhys[j].pauxBoneInfo[i - 1].iBone)
				qtParent = qtParent0;
			const QuatT* pLocation = &poseData.GetJointsAbsolute()[m_auxPhys[j].pauxBoneInfo[i].iBone];
			if (m_pPhysBuffer && m_bPhysBufferFilled)
				pLocation = &m_pPhysBuffer[m_auxPhys[j].pauxBoneInfo[i].iBone].location;

			atv.points[i] = qtParent * pLocation->t;
			qtParent = qtParent * *pLocation;
		}
		int iParent = m_auxPhys[j].pauxBoneInfo[m_auxPhys[j].nBones - 1].iBone;
		if (GetModelJointPointer(iParent)->m_numChildren > 0)
		{
			int childIndex = GetModelJointChildIndex(iParent, 0);
			const QuatT* pLocation = &poseData.GetJointsAbsolute()[childIndex];
			if (m_pPhysBuffer && m_bPhysBufferFilled)
				pLocation = &m_pPhysBuffer[childIndex].location;

			atv.points[m_auxPhys[j].nBones] = qtParent * pLocation->t;
		}
		if (m_pCharPhysics && sr.bTargetPoseActive == 1)
		{
			atv.nPoints = m_auxPhys[j].nBones + 1;
			m_auxPhys[j].pPhysEnt->Action(&atv);
		}

		pe_status_awake statusTmp;
		bRopesAwake |= m_auxPhys[j].pPhysEnt->GetStatus(&statusTmp);
	}
}

void CSkeletonPhysics::Physics_SynchronizeToImpact(float timeDelta)
{
	if (!m_pPhysImpactBuffer)
		return;
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;

	pe_params_joint physicsParamsJoint;
	physicsParamsJoint.bNoUpdate = 1;
	if (timeDelta > 0)
	{
		physicsParamsJoint.animationTimeStep = timeDelta;
		physicsParamsJoint.ranimationTimeStep = 1.0f / timeDelta;
	}

	int physicsLod = m_bPhysicsRelinquished ? GetPhysicsLod() : 0;

	pe_params_flags physicsParamsFlags;
	pe_status_joint physicsStatusJoint;
	pe_status_awake physicsStatusAwake;
	m_bPhysicsAwake = m_pCharPhysics->GetStatus(&physicsStatusAwake) != 0;
	if (m_bPhysicsAwake)
	{
		physicsParamsFlags.flagsOR = pef_disabled;
		m_pCharPhysics->SetParams(&physicsParamsFlags);

		int jointCount = rDefaultSkeleton.GetJointCount();
		for (int i = 0; i < jointCount; ++i)
		{
			if (!m_pPhysImpactBuffer[i].bSet)
				continue;

			int parentIndex = getBonePhysParentIndex(i, physicsLod);
			if (parentIndex < 0)
				continue;

			physicsStatusJoint.idChildBody = i;
			if (!m_pCharPhysics->GetStatus(&physicsStatusJoint))
				continue;

			int j;
			while (
			  (GetModelJointPointer(parentIndex)->m_PhysInfo.flags & all_angles_locked) == all_angles_locked &&
			  (j = getBonePhysParentIndex(parentIndex, physicsLod)) >= 0)
			{
				parentIndex = j;
			}

			const Ang3& animationAngles = m_pPhysImpactBuffer[i].angles;
			physicsParamsJoint.op[0] = parentIndex;
			physicsParamsJoint.op[1] = i;
			if (!m_bPhysicsRelinquished)
			{
				physicsParamsJoint.pivot = m_pPhysImpactBuffer[i].pivot;
				memcpy(&physicsParamsJoint.q0, &m_pPhysImpactBuffer[i].q0, sizeof(physicsParamsJoint.q0));
				physicsParamsJoint.qext = animationAngles;
				physicsParamsJoint.flagsPivot = 8 | 1;
			}
			else
			{
				const Ang3& animationAnglesPrevious = physicsStatusJoint.qext;
				physicsParamsJoint.qtarget = animationAngles - animationAnglesPrevious;
			}
			m_pCharPhysics->SetParams(&physicsParamsJoint);
		}

		int iRoot = GetPhysRoot();
		new(&physicsParamsJoint)pe_params_joint;
		physicsParamsJoint.q = Ang3(ZERO);
		physicsParamsJoint.qext = m_pPhysImpactBuffer[iRoot].angles;
		physicsParamsJoint.op[0] = -1;
		physicsParamsJoint.op[1] = iRoot;
		m_pCharPhysics->SetParams(&physicsParamsJoint);
	}

	if (!is_unused(physicsParamsFlags.flagsOR) && physicsParamsFlags.flagsOR)
	{
		physicsParamsFlags.flagsOR = 0;
		physicsParamsFlags.flagsAND = ~pef_disabled;
		m_pCharPhysics->SetParams(&physicsParamsFlags);
	}
}

void CSkeletonPhysics::ProcessPhysics(Skeleton::CPoseData& poseData, float timeDelta)
{
	if (Console::GetInst().ca_UsePhysics == 0)
		return;

	m_bPhysicsAwake = 0;
	m_vOffset = ZERO;

	if (m_pCharPhysics)
	{
		if (m_pCharPhysics->GetType() == PE_ARTICULATED)
		{
			Physics_SynchronizeToImpact(timeDelta);
			Physics_SynchronizeToEntityArticulated(timeDelta);
		}
		else
		{
			Physics_SynchronizeToEntity(*m_pCharPhysics, QuatT(IDENTITY));
		}

		const int boneIndex = GetPhysRoot();
		if (boneIndex > -1)
		{
			Vec3 offs = -poseData.GetJointAbsolute(boneIndex).t;
			for (int i = m_pInstance->m_AttachmentManager.GetAttachmentCount() - 1; i >= 0; i--)
				m_pInstance->m_AttachmentManager.UpdatePhysAttachmentHideState(i, m_pCharPhysics, offs);
		}

		Physics_SynchronizeToAux(poseData);
	}

	m_bPhysicsWasAwake = m_bPhysicsAwake;
}

// Synch

void CSkeletonPhysics::SynchronizeWithPhysicsPost()
{
	DEFINE_PROFILER_FUNCTION();
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	uint32 objectType = rDefaultSkeleton.m_ObjectType;
	IPhysicalEntity* pCharPhys = GetCharacterPhysics();
	pe_params_flags pf1;
	if (objectType == CGA || pCharPhys && pCharPhys->GetType() == PE_ARTICULATED && pCharPhys->GetParams(&pf1) && pf1.flags & aef_recorded_physics)
	{

		if (
			(m_bFullSkeletonUpdate && m_bForcePostSynchronization) ||
			(m_bFullSkeletonUpdate && m_pSkeletonAnim->m_IsAnimPlaying) ||
			(m_ppBonePhysics)
			)
		{
			m_pInstance->UpdatePhysicsCGA(GetPoseDataExplicitWriteable(), 1, m_location);
			m_bForcePostSynchronization = false;
		}

		if (m_bFullSkeletonUpdate)
		{
			QuatT Offset(IDENTITY);
			int32 numAttachments = m_pInstance->m_AttachmentManager.GetAttachmentCount() - 1;

			for (int32 i = numAttachments - 1; i >= 0; i--)
				m_pInstance->m_AttachmentManager.UpdatePhysicalizedAttachment(i, pCharPhys, Offset);

		}
	}
}

void CSkeletonPhysics::SynchronizeWithPhysics(Skeleton::CPoseData& poseData)
{
	DEFINE_PROFILER_FUNCTION();

	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	uint32 objectType = rDefaultSkeleton.m_ObjectType;
	if (objectType != CHR)
		return;

	pe_params_flags pf;
	if (m_bFullSkeletonUpdate)
	{
		if (gEnv->pPhysicalWorld->GetPhysVars()->lastTimeStep > 0 || m_pInstance->m_CharEditMode)
		{
			ProcessPhysics(poseData, m_pInstance->m_fOriginalDeltaTime);
			pf.flagsAND = ~pef_disabled;
			m_pInstance->SetWasVisible(1);
		}
		else
		{
			m_pInstance->SetWasVisible(0);
		}
	}
	else
	{
		if (m_pCharPhysics || m_nAuxPhys > 1)
			pf.flagsOR = pef_disabled;
	}
	pf.flagsOR |= pef_disabled & - Console::GetInst().ca_DisableAuxPhysics;

	m_pInstance->SetWasVisible(m_pInstance->GetWasVisible() && (((int)m_bFullSkeletonUpdate) > 0));
	SetAuxParams(&pf);
}

// SynchronizeTo

SBatchUpdateValidator SBatchUpdateValidator::g_firstValidator;
volatile int SBatchUpdateValidator::g_lockList;

void CSkeletonPhysics::Physics_SynchronizeToEntity(IPhysicalEntity& physicalEntity, QuatT offset)
{
	DEFINE_PROFILER_FUNCTION();

	IPhysicalEntity* pent = &physicalEntity;

	const Skeleton::CPoseData& poseData = GetPoseData();
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;

	const int physicsChildIndex = GetPhysRoot();

	if (pent->GetType() == PE_ARTICULATED && physicsChildIndex > -1)
	{
		struct pe_status_awake statAwake;
		offset.t = -poseData.GetJointAbsolute(physicsChildIndex).t * m_fScale;
		if (!pent->GetStatus(&statAwake))
		{
			for (uint32 i = 0; i < (unsigned)m_nPhysJoints; ++i)
				(m_physJoints[i] = poseData.GetJointAbsolute(m_physJointsIdx[i])).t *= m_fScale;

			pe_action_batch_parts_update abpu;
			abpu.numParts = m_nPhysJoints;
			abpu.pnumParts = &m_nPhysJoints;
			abpu.posParts = strided_pointer<Vec3>(&m_physJoints[0].t, sizeof(m_physJoints[0]));
			abpu.qParts = strided_pointer<Quat>(&m_physJoints[0].q, sizeof(m_physJoints[0]));
			abpu.posOffs = offset.t;
			abpu.qOffs = offset.q;
			abpu.pIds = &m_physJointsIdx[0];
			if (!m_pPhysUpdateValidator)
			{
				m_pPhysUpdateValidator = new SBatchUpdateValidator;
			}
			m_pPhysUpdateValidator->AddRef();
			abpu.pValidator = m_pPhysUpdateValidator;
			pent->Action(&abpu);
		}
	}
	else
	{
		uint32 jointCount = rDefaultSkeleton.GetJointCount();
		uint32 j = 0;
		for (uint32 i = 0; i < jointCount; ++i)
		{
			if (GetModelJointPointer(i)->m_PhysInfo.pPhysGeom)
				j = i;
		}

		pe_params_part partpos;
		partpos.invTimeStep = 1.0f / max(0.001f, m_pInstance->m_fOriginalDeltaTime);
		for (uint32 i = 0; i < jointCount; ++i)
		{
			if (GetModelJointPointer(i)->m_PhysInfo.pPhysGeom)
			{
				partpos.partid = i;
				partpos.bRecalcBBox = i == j;
				QuatT q = offset * poseData.GetJointAbsolute(i);
				partpos.pos = q.t;
				partpos.q = q.q;
				pent->SetParams(&partpos);
			}
		}
	}

	int bStateChanged = 0;
	for (int i = m_pInstance->m_AttachmentManager.GetAttachmentCount() - 1; i >= 0; i--)
		bStateChanged |= m_pInstance->m_AttachmentManager.UpdatePhysicalizedAttachment(i, pent, offset);
	if (bStateChanged)
	{
		for (uint32 i = 0; i < (unsigned)m_nAuxPhys; ++i)
		{
			if (!m_auxPhys[i].bTied0)
				continue;

			pe_params_rope pr;
			pr.pEntTiedTo[0] = m_pCharPhysics;
			pr.idPartTiedTo[0] = getBonePhysParentIndex(m_auxPhys[i].iBoneTiedTo[0], 0);
			m_auxPhys[i].pPhysEnt->SetParams(&pr);
		}
	}
}

void CSkeletonPhysics::Physics_SynchronizeToEntityArticulated(float timeDelta)
{
	if (!m_bPhysicsRelinquished)
	{
		const Skeleton::CPoseData& poseData = GetPoseData();

		//this is the relative orientation & translation of the animated character for this frame
		QuatT KinematicMovement = IDENTITY;

		if (m_pCharPhysics)
			Physics_SynchronizeToEntity(*m_pCharPhysics, KinematicMovement);

		pe_params_articulated_body pab;
		pab.pivot.zero();
		pab.posHostPivot = KinematicMovement.t + poseData.GetJointAbsolute(GetPhysRoot()).t * m_fScale;
		pab.qHostPivot = KinematicMovement.q;
		pab.bRecalcJoints = m_bPhysicsAwake;
		m_velPivot = (pab.posHostPivot - m_prevPosPivot) / max(0.001f, m_pInstance->m_fOriginalDeltaTime);
		m_velPivot *= (float)isneg(m_velPivot.len2() - sqr(30.0f));
		m_prevPosPivot = pab.posHostPivot;
		m_pCharPhysics->SetParams(&pab);
	}
	else if (m_bPhysicsSynchronizeFromEntity && m_pPhysBuffer && m_pSkeletonAnim->m_IsAnimPlaying)
	{
		int physicsLod = 0;
		const Skeleton::CPoseData& poseData = GetPoseData();
		const uint jointCount = poseData.GetJointCount();

		pe_status_joint physicsStatusJoint;
		pe_params_joint physicsParamsJoint;
		physicsParamsJoint.bNoUpdate = 1;
		for (uint i = 0; i < jointCount; ++i)
		{
			int parentIndex = getBonePhysParentIndex(i, physicsLod);
			if (parentIndex < 0)
				continue;

			physicsStatusJoint.idChildBody = i;
			if (!m_pCharPhysics->GetStatus(&physicsStatusJoint))
				continue;

			physicsParamsJoint.op[1] = i;
			physicsParamsJoint.qtarget = m_pPhysBuffer[i].angles;
			m_pCharPhysics->SetParams(&physicsParamsJoint);
		}
	}
}

// FallAndPlay

void CSkeletonPhysics::FindSpineBones() const
{
	if (m_nSpineBones)
		return;

	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;
	uint32 jointCount = rDefaultSkeleton.GetJointCount();
	for (uint32 i = m_nSpineBones = 0; i < jointCount && m_nSpineBones < 3; ++i)
	{
		if (strstr(GetModelJointPointer(i)->m_strJointName.c_str(), "Pelvis") || strstr(GetModelJointPointer(i)->m_strJointName.c_str(), "Spine"))
		{
			m_iSpineBone[m_nSpineBones++] = i;
		}
	}

	if (!m_nSpineBones)
	{
		m_iSpineBone[0] = GetPhysRoot();
		m_nSpineBones = 1 + (m_iSpineBone[0] >> 31);
	}
}

int CSkeletonPhysics::GetFallingDir() const
{
	if (!m_pCharPhysics)
		return -1;

	FindSpineBones();

	const Skeleton::CPoseData& poseData = GetPoseData();
	const QuatT* pJointsAbsolute = poseData.GetJointsAbsolute();

	pe_status_dynamics sd;
	int status = m_pCharPhysics->GetStatus(&sd);

	Vec3 n(ZERO);
	for (uint32 i = 0; i < m_nSpineBones; ++i)
		n += sd.v * pJointsAbsolute[m_iSpineBone[i]].q;

	Vec3 nAbs = n.abs();
	int i = idxmax3((f32*)&nAbs);
	i = i * 2 + isneg(n[i]);
	return i;
}

ILINE void InitializePoseAbsolute(const CDefaultSkeleton& skeleton, QuatT* pResult)
{
	const QuatT* pJointsAbsolute = skeleton.GetPoseData().GetJointsAbsolute();
	uint32 jointCount = skeleton.GetPoseData().GetJointCount();
	for (uint32 i = 0; i < jointCount; ++i)
		pResult[i] = pJointsAbsolute[i];
}

ILINE void SamplePoseAbsolute(const CDefaultSkeleton& skeleton, const GlobalAnimationHeaderCAF& animation, f32 t, QuatT* pResult)
{
	const Skeleton::CPoseData& poseData = skeleton.GetPoseData();
	const QuatT* pJointsRelativeDefault = poseData.GetJointsRelative();
	uint32 jointCount = poseData.GetJointCount();
	for (uint32 i = 0; i < jointCount; ++i)
	{
		QuatT& result = pResult[i];
		result = pJointsRelativeDefault[i];
		const CDefaultSkeleton::SJoint& joint = skeleton.m_arrModelJoints[i];
		IController* pController = animation.GetControllerByJointCRC32(joint.m_nJointCRC32);
		if (pController)
			pController->GetOP(t, result.q, result.t);

		// Compute absolute value
		if (joint.m_idxParent >= 0)
			result = pResult[joint.m_idxParent] * result;
	}
}

ILINE bool SamplePoseAbsolute(const CDefaultSkeleton& rDefaultSkeleton, int32 animationId, f32 t, QuatT* pResult)
{
	const CAnimationSet& animationSet = *rDefaultSkeleton.m_pAnimationSet;

	const ModelAnimationHeader* pModelAnimation = animationSet.GetModelAnimationHeader(animationId);
	if (!pModelAnimation)
	{
		InitializePoseAbsolute(rDefaultSkeleton, pResult);
		return false;
	}

	if (pModelAnimation->m_nAssetType != CAF_File)
	{
		InitializePoseAbsolute(rDefaultSkeleton, pResult);
		return false;
	}

	const int32 globalId = int32(pModelAnimation->m_nGlobalAnimId);
	if (globalId < 0)
	{
		InitializePoseAbsolute(rDefaultSkeleton, pResult);
		return false;
	}

	GlobalAnimationHeaderCAF& animation = g_AnimationManager.m_arrGlobalCAF[globalId];
	SamplePoseAbsolute(rDefaultSkeleton, animation, t, pResult);
	return true;
}

PREFAST_SUPPRESS_WARNING(6262);
bool CSkeletonPhysics::BlendFromRagdoll(QuatTS& location, IPhysicalEntity*& pPhysicalEntity)
{
	auto ctx = m_pInstance->GetProcessingContext();
	if (ctx && ctx->state == CharacterInstanceProcessing::SContext::EState::StartAnimationProcessed)
		return false;
	if (!m_bPhysicsRelinquished)
		return false;

	m_bPhysicsWasAwake = true;

	const Skeleton::CPoseData& poseData = GetPoseData();
	const CDefaultSkeleton& rDefaultSkeleton = *m_pInstance->m_pDefaultSkeleton;

	uint jointCount = poseData.GetJointCount();
	if (m_arrPhysicsJoints.empty())
		InitPhysicsSkeleton();
	FindSpineBones();
	int rootJointIndex = m_iSpineBone[0];
	if (rootJointIndex < 0 || rootJointIndex >= int(jointCount))
		return false;

	//

	QuatT joints[MAX_JOINT_AMOUNT];
	int animationId = m_pSkeletonAnim->GetAnimFromFIFO(0, 0).GetAnimationId();
	SamplePoseAbsolute(*m_pInstance->m_pDefaultSkeleton, animationId, 0.0f, joints);

	//

	if (m_pPrevCharHost)
		g_pIPhysicalWorld->DestroyPhysicalEntity(m_pPrevCharHost, 2);

	Matrix34 mtxChar = Matrix34(Vec3(m_fScale), Quat(IDENTITY), m_vOffset);
	CreateCharacterPhysics(m_pPrevCharHost, m_fMass, m_iSurfaceIdx, m_stiffnessScale, 0, mtxChar);
	CreateAuxilaryPhysics(m_pCharPhysics, mtxChar);
	if (m_pCharPhysics)
	{
		pe_params_articulated_body pab;
		pab.bAwake = 0;
		pab.pivot.zero();
		pab.posHostPivot = m_vOffset + joints[rootJointIndex].t * m_fScale;
		pab.qHostPivot.SetIdentity();
		pab.bRecalcJoints = 0;
		m_pCharPhysics->SetParams(&pab);
	}
	pPhysicalEntity = m_pPrevCharHost;

	//

	QuatT previousLocation(location);
	m_physicsJointRootPosition = previousLocation * poseData.GetJointAbsolute(rootJointIndex).t;

	// rotate the entity around z by current (physical) z angle - animation z angle
	float angleDelta =
	  Ang3::GetAnglesXYZ(poseData.GetJointAbsolute(rootJointIndex).q).z -
	  Ang3::GetAnglesXYZ(joints[rootJointIndex].q).z;

	location.q.SetRotationXYZ(Ang3(0.0f, 0.0f, angleDelta));

	location.t =
	  previousLocation * poseData.GetJointAbsolute(rootJointIndex).t -
	  location.q * joints[rootJointIndex].t * m_fScale;

	location.s = m_fScale;

	//

	m_arrPhysicsJoints[0].m_qRelFallPlay = poseData.GetJointAbsolute(0).q;
	for (uint i = 1; i < jointCount; ++i)
	{
		m_arrPhysicsJoints[i].m_qRelFallPlay =
		  !poseData.GetJointAbsolute(rDefaultSkeleton.m_arrModelJoints[i].m_idxParent).q *
		  poseData.GetJointAbsolute(i).q;
	}
	m_arrPhysicsJoints[rootJointIndex].m_qRelFallPlay =
	  !location.q* poseData.GetJointAbsolute(rootJointIndex).q;

	//

	m_timeStandingUp = 0;
	m_bBlendFromRagdollFlip = false;
	return true;
}

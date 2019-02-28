// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AttachmentFace.h"

#include "AttachmentManager.h"
#include "CharacterInstance.h"
#include <CryMath/QTangent.h>
#include "CharacterManager.h"

uint32 CAttachmentFACE::Immediate_AddBinding(IAttachmentObject* pIAttachmentObject, ISkin* pISkin, uint32 nLoadingFlags)
{
	if (m_pIAttachmentObject)
	{
		uint32 IsFastUpdateType = m_pAttachmentManager->IsFastUpdateType(m_pIAttachmentObject->GetAttachmentType());
		if (IsFastUpdateType)
			m_pAttachmentManager->RemoveEntityAttachment();
	}

	SAFE_RELEASE(m_pIAttachmentObject);
	m_pIAttachmentObject = pIAttachmentObject;

	if (pIAttachmentObject)
	{
		uint32 IsFastUpdateType = m_pAttachmentManager->IsFastUpdateType(pIAttachmentObject->GetAttachmentType());
		if (IsFastUpdateType)
			m_pAttachmentManager->AddEntityAttachment();
	}
	m_pAttachmentManager->m_TypeSortingRequired++;
	return 1;
}

void CAttachmentFACE::Immediate_ClearBinding(uint32 nLoadingFlags)
{
	ClearBinding_Internal(true);
};

void CAttachmentFACE::ClearBinding_Internal(bool release)
{
	if (m_pIAttachmentObject)
	{
		if (m_pAttachmentManager->m_pSkelInstance)
		{
			uint32 IsFastUpdateType = m_pAttachmentManager->IsFastUpdateType(m_pIAttachmentObject->GetAttachmentType());
			if (IsFastUpdateType)
				m_pAttachmentManager->RemoveEntityAttachment();

			if (release)
			{
				m_pIAttachmentObject->Release();
			}

			m_pIAttachmentObject = 0;
		}
	}
	m_pAttachmentManager->m_TypeSortingRequired++;
}

uint32 CAttachmentFACE::Immediate_SwapBinding(IAttachment* pNewAttachment)
{
	if (pNewAttachment == NULL || pNewAttachment->GetType() != GetType())
	{
		CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "CAttachmentFACE::SwapBinding attempting to swap bindings of non-matching attachment types");
		return 0;
	}

	if (m_pIAttachmentObject)
	{
		if (uint32 retVal = static_cast<SAttachmentBase*>(pNewAttachment)->Immediate_AddBinding(m_pIAttachmentObject))
		{
			ClearBinding_Internal(false);
			return retVal;
		}
	}

	return 0;
}

uint32 CAttachmentFACE::ProjectAttachment(const Skeleton::CPoseData* pPoseData)
{
	CCharInstance* pSkelInstance = m_pAttachmentManager->m_pSkelInstance;
	CDefaultSkeleton* pDefaultSkeleton = pSkelInstance->m_pDefaultSkeleton;
	if (pDefaultSkeleton->m_ObjectType != CHR)
		return 0;

	Vec3 apos = GetAttAbsoluteDefault().t;
	ClosestTri cf;
	f32 fShortestDistance = 999999.0f;
	uint32 nMeshFullyStreamdIn = 1;

	uint32 numAttachments = m_pAttachmentManager->GetAttachmentCount();
	for (uint32 att = 0; att < numAttachments; att++)
	{
		uint32 localtype = m_pAttachmentManager->m_arrAttachments[att]->GetType();
		if (localtype != CA_SKIN)
			continue;
		uint32 isHidden = m_pAttachmentManager->m_arrAttachments[att]->IsAttachmentHidden();
		if (isHidden)
			continue;
		IAttachmentObject* pIAttachmentObject = m_pAttachmentManager->m_arrAttachments[att]->GetIAttachmentObject();
		if (pIAttachmentObject == 0)
			continue;
		IAttachmentSkin* pIAttachmentSkin = pIAttachmentObject->GetIAttachmentSkin();
		if (pIAttachmentSkin == 0)
			continue;
		CSkin* pCModelSKIN = (CSkin*)pIAttachmentSkin->GetISkin();
		if (pCModelSKIN == 0)
			continue;
		CModelMesh* pCModelMeshSKIN = pCModelSKIN->GetModelMesh(0);
		uint32 IsValid = pCModelMeshSKIN->IsVBufferValid();
		if (IsValid == 0)
		{
			nMeshFullyStreamdIn = 0;  //error
			break;
		}
	}

	//--------------------------------------------------------------------

	if (nMeshFullyStreamdIn)
	{
		for (uint32 att = 0; att < numAttachments; att++)
		{
			uint32 localtype = m_pAttachmentManager->m_arrAttachments[att]->GetType();
			if (localtype != CA_SKIN)
				continue;
			uint32 isHidden = m_pAttachmentManager->m_arrAttachments[att]->IsAttachmentHidden();
			if (isHidden)
				continue;
			IAttachmentObject* pIAttachmentObject = m_pAttachmentManager->m_arrAttachments[att]->GetIAttachmentObject();
			if (pIAttachmentObject == 0)
				continue;
			IAttachmentSkin* pIAttachmentSkin = pIAttachmentObject->GetIAttachmentSkin();
			if (pIAttachmentSkin == 0)
				continue;
			CSkin* pCModelSKIN = (CSkin*)pIAttachmentSkin->GetISkin();
			if (pCModelSKIN == 0)
				continue;
			IRenderMesh* pIRenderMeshSKIN = pCModelSKIN->GetIRenderMesh(0);
			if (pIRenderMeshSKIN == 0)
				continue;

			CModelMesh* pCModelMeshSKIN = pCModelSKIN->GetModelMesh(0);
			CAttachmentSKIN* pCAttachmentSkin = (CAttachmentSKIN*)pIAttachmentSkin;
			JointIdType* pRemapTable = &pCAttachmentSkin->m_arrRemapTable[0];
			ClosestTri scf = pCModelMeshSKIN->GetAttachmentTriangle(apos, pRemapTable);
			Vec3 vCenter = (scf.v[0].m_attTriPos + scf.v[1].m_attTriPos + scf.v[2].m_attTriPos) / 3;
			f32 fDistance = (apos - vCenter).GetLength();
			if (fShortestDistance > fDistance)
				fShortestDistance = fDistance, cf = scf;
		}

		if (CModelMesh* pModelMesh = pDefaultSkeleton->GetModelMesh())
		{
			if (pModelMesh->IsVBufferValid())
			{
				ClosestTri scf = pModelMesh->GetAttachmentTriangle(apos, 0);
				Vec3 vTriCenter = (scf.v[0].m_attTriPos + scf.v[1].m_attTriPos + scf.v[2].m_attTriPos) / 3;
				f32 fDistance = (apos - vTriCenter).GetLength();
				if (fShortestDistance > fDistance)
					fShortestDistance = fDistance, cf = scf;
			}
		}

		Vec3 vt0 = cf.v[0].m_attTriPos;
		Vec3 vt1 = cf.v[1].m_attTriPos;
		Vec3 vt2 = cf.v[2].m_attTriPos;
		Vec3 mid = (vt0 + vt1 + vt2) / 3.0f;
		Vec3 x = (vt1 - vt0).GetNormalized();
		Vec3 z = ((vt1 - vt0) % (vt0 - vt2)).GetNormalized();
		Vec3 y = z % x;
		QuatT TriMat = QuatT::CreateFromVectors(x, y, z, mid);
		m_AttRelativeDefault = TriMat.GetInverted() * m_AttAbsoluteDefault;
		m_Triangle = cf;

#ifndef _RELEASE
		if (Console::GetInst().ca_DrawAttachmentProjection && (pSkelInstance->m_CharEditMode & CA_DrawSocketLocation))
		{
			g_pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);
			QuatTS& rPhysLocation = pSkelInstance->m_location;
			g_pAuxGeom->DrawLine(rPhysLocation * vt0, RGBA8(0xff, 0xff, 0xff, 0x00), rPhysLocation * vt1, RGBA8(0xff, 0xff, 0xff, 0x00), 5);
			g_pAuxGeom->DrawLine(rPhysLocation * vt1, RGBA8(0xff, 0xff, 0xff, 0x00), rPhysLocation * vt2, RGBA8(0xff, 0xff, 0xff, 0x00), 5);
			g_pAuxGeom->DrawLine(rPhysLocation * vt2, RGBA8(0xff, 0xff, 0xff, 0x00), rPhysLocation * vt0, RGBA8(0xff, 0xff, 0xff, 0x00), 5);
			g_pAuxGeom->DrawLine(rPhysLocation * mid, RGBA8(0xff, 0xff, 0xff, 0x00), rPhysLocation * m_AttAbsoluteDefault.t, RGBA8(0xff, 0xff, 0xff, 0x00), 10);
		}
#endif
		m_AttFlags |= FLAGS_ATTACH_PROJECTED; //success
		return 1;
	}

	return 0;
}

void CAttachmentFACE::ComputeTriMat()
{
	CCharInstance* pSkelInstance = m_pAttachmentManager->m_pSkelInstance;
	const CDefaultSkeleton* pDefaultSkeleton = pSkelInstance->m_pDefaultSkeleton;
	const CDefaultSkeleton::SJoint* pJoints = &pDefaultSkeleton->m_arrModelJoints[0];
	const QuatT* pJointsAbsolute = &pSkelInstance->m_SkeletonPose.GetPoseData().GetJointAbsolute(0);
	const QuatT* pJointsAbsoluteDefault = pDefaultSkeleton->m_poseDefaultData.GetJointsAbsolute();

	Vec3 tv[3];
	for (uint32 t = 0; t < 3; t++)
	{
		DualQuat wquat(IDENTITY);
		wquat.nq.w = 0.0f;
		for (uint32 o = 0; o < 4; o++)
		{
			f32 w = m_Triangle.v[t].m_attWeights[o];
			if (w)
			{
				uint32 b = m_Triangle.v[t].m_attJointIDs[o];
				int32 p = pJoints[b].m_idxParent;
				DualQuat dqp = pJointsAbsolute[p] * pJointsAbsoluteDefault[p].GetInverted();
				DualQuat dq = pJointsAbsolute[b] * pJointsAbsoluteDefault[b].GetInverted();
				f32 mul = f32(__fsel(dq.nq | dqp.nq, 1.0f, -1.0f));
				dq.nq *= mul;
				dq.dq *= mul;
				wquat += dq * w;
			}
		}
		f32 l = 1.0f / wquat.nq.GetLength();
		wquat.nq *= l;
		wquat.dq *= l;
		tv[t] = wquat * m_Triangle.v[t].m_attTriPos;
	}

	Vec3 tv0 = tv[0];
	Vec3 tv1 = tv[1];
	Vec3 tv2 = tv[2];
	Vec3 mid = (tv0 + tv1 + tv2) / 3.0f;
	Vec3 x = (tv1 - tv0).GetNormalized();
	Vec3 z = ((tv1 - tv0) % (tv0 - tv2)).GetNormalized();
	Vec3 y = z % x;
	QuatT TriMat = QuatT::CreateFromVectors(x, y, z, mid);
	m_AttModelRelative = TriMat * m_AttRelativeDefault;

#ifndef _RELEASE
	if (pSkelInstance->m_CharEditMode & CA_DrawSocketLocation)
	{
		g_pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);
		QuatTS& rPhysLocation = pSkelInstance->m_location;
		g_pAuxGeom->DrawLine(rPhysLocation * tv0, RGBA8(0xff, 0xff, 0xff, 0xff), rPhysLocation * tv1, RGBA8(0xff, 0xff, 0xff, 0xff));
		g_pAuxGeom->DrawLine(rPhysLocation * tv1, RGBA8(0xff, 0xff, 0xff, 0xff), rPhysLocation * tv2, RGBA8(0xff, 0xff, 0xff, 0xff));
		g_pAuxGeom->DrawLine(rPhysLocation * tv2, RGBA8(0xff, 0xff, 0xff, 0xff), rPhysLocation * tv0, RGBA8(0xff, 0xff, 0xff, 0xff));
		g_pAuxGeom->DrawLine(rPhysLocation * mid, RGBA8(0xff, 0x00, 0x00, 0xff), rPhysLocation * m_AttModelRelative.t, RGBA8(0x00, 0xff, 0x00, 0xff));

		if (pSkelInstance->m_CharEditMode & CA_BindPose)
		{
			SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
			renderFlags.SetDepthTestFlag(e_DepthTestOff);
			g_pAuxGeom->SetRenderFlags(renderFlags);
			g_pAuxGeom->DrawSphere(rPhysLocation * m_AttModelRelative.t, 0.015f, RGBA8(0xff, 0x00, 0x00, 0));
		}
	}
#endif

}

void CAttachmentFACE::PostUpdateSimulationParams(bool bAttachmentSortingRequired, const char* pJointName)
{
	m_pAttachmentManager->m_TypeSortingRequired += bAttachmentSortingRequired;
	m_Simulation.PostUpdate(m_pAttachmentManager, pJointName);
};

void CAttachmentFACE::Update_Empty(Skeleton::CPoseData& rPoseData)
{
	if ((m_AttFlags & FLAGS_ATTACH_PROJECTED) == 0)
		ProjectAttachment(&rPoseData); //not projected, so do it now
	if ((m_AttFlags & FLAGS_ATTACH_PROJECTED) == 0)
		return; //Probably failed because mesh was not streamed in. Try again in next frame
	ComputeTriMat();
}

void CAttachmentFACE::Update_Static(Skeleton::CPoseData& rPoseData)
{
	if ((m_AttFlags & FLAGS_ATTACH_PROJECTED) == 0)
	{
		ProjectAttachment(&rPoseData); //not projected, so do it now
		if ((m_AttFlags & FLAGS_ATTACH_PROJECTED) == 0)
			return; //Probably failed because mesh was not streamed in. Try again in next frame
	}
	//This is a CGF. Update and simulate only when visible
	m_AttFlags &= FLAGS_ATTACH_VISIBLE ^ -1;
	const f32 fRadiusSqr = m_pIAttachmentObject->GetRadiusSqr();
	if (fRadiusSqr == 0.0f)
		return;     //if radius is zero, then the object is most probably not visible and we can continue
	if (m_pAttachmentManager->m_fZoomDistanceSq > fRadiusSqr)
		return;  //too small to render. cancel the update
	m_AttFlags |= FLAGS_ATTACH_VISIBLE;
	ComputeTriMat();
	if (m_Simulation.m_nClampType)
		m_Simulation.UpdateSimulation(m_pAttachmentManager, rPoseData, -1, m_AttModelRelative, m_addTransformation, GetName());
}

void CAttachmentFACE::Update_Execute(Skeleton::CPoseData& rPoseData)
{
	if ((m_AttFlags & FLAGS_ATTACH_PROJECTED) == 0)
	{
		ProjectAttachment(&rPoseData); //not projected, so do it now
		if ((m_AttFlags & FLAGS_ATTACH_PROJECTED) == 0)
			return; //Probably failed because mesh was not streamed in. Try again in next frame
	}
	ComputeTriMat();
	if (m_Simulation.m_nClampType)
		m_Simulation.UpdateSimulation(m_pAttachmentManager, rPoseData, -1, m_AttModelRelative, m_addTransformation, GetName());
	m_pIAttachmentObject->ProcessAttachment(this);

	m_AttFlags &= FLAGS_ATTACH_VISIBLE ^ -1;
	const f32 fRadiusSqr = m_pIAttachmentObject->GetRadiusSqr();
	if (fRadiusSqr == 0.0f)
		return;     //if radius is zero, then the object is most probably not visible and we can continue
	if (m_pAttachmentManager->m_fZoomDistanceSq > fRadiusSqr)
		return;  //too small to render. cancel the update
	m_AttFlags |= FLAGS_ATTACH_VISIBLE;
}

const QuatTS CAttachmentFACE::GetAttWorldAbsolute() const
{
	QuatTS rPhysLocation = m_pAttachmentManager->m_pSkelInstance->m_location;
	return rPhysLocation * m_AttModelRelative;
};

void CAttachmentFACE::UpdateAttModelRelative()
{
	ComputeTriMat();
}

size_t CAttachmentFACE::SizeOfThis() const
{
	size_t nSize = sizeof(CAttachmentFACE) + sizeofVector(m_strSocketName);
	return nSize;
}

void CAttachmentFACE::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_strSocketName);
}

void CAttachmentFACE::Serialize(TSerialize ser)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.BeginGroup("CAttachmentFACE");

		bool bHideInMainPass;

		if (ser.IsWriting())
		{
			bHideInMainPass = ((m_AttFlags & FLAGS_ATTACH_HIDE_MAIN_PASS) == FLAGS_ATTACH_HIDE_MAIN_PASS);
		}

		ser.Value("HideInMainPass", bHideInMainPass);

		if (ser.IsReading())
		{
			HideAttachment(bHideInMainPass);
		}

		ser.EndGroup();
	}
}
void CAttachmentFACE::HideAttachment(uint32 x)
{
	m_pAttachmentManager->OnHideAttachment(this, FLAGS_ATTACH_HIDE_MAIN_PASS | FLAGS_ATTACH_HIDE_SHADOW_PASS | FLAGS_ATTACH_HIDE_RECURSION, x != 0);

	if (x)
		m_AttFlags |= (FLAGS_ATTACH_HIDE_MAIN_PASS | FLAGS_ATTACH_HIDE_SHADOW_PASS | FLAGS_ATTACH_HIDE_RECURSION);
	else
		m_AttFlags &= ~(FLAGS_ATTACH_HIDE_MAIN_PASS | FLAGS_ATTACH_HIDE_SHADOW_PASS | FLAGS_ATTACH_HIDE_RECURSION);
}

void CAttachmentFACE::HideInRecursion(uint32 x)
{
	m_pAttachmentManager->OnHideAttachment(this, FLAGS_ATTACH_HIDE_RECURSION, x != 0);

	if (x)
		m_AttFlags |= FLAGS_ATTACH_HIDE_RECURSION;
	else
		m_AttFlags &= ~FLAGS_ATTACH_HIDE_RECURSION;
}

void CAttachmentFACE::HideInShadow(uint32 x)
{
	m_pAttachmentManager->OnHideAttachment(this, FLAGS_ATTACH_HIDE_RECURSION, x != 0);

	if (x)
		m_AttFlags |= FLAGS_ATTACH_HIDE_SHADOW_PASS;
	else
		m_AttFlags &= ~FLAGS_ATTACH_HIDE_SHADOW_PASS;
}

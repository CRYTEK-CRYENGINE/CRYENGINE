// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SkeletonPose.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include "CharacterManager.h"
#include "CharacterInstance.h"
#include "Model.h"
#include "DrawHelper.h"

static const char* strstri(const char* s1, const char* s2)
{
	int i, j, k;
	for (i = 0; s1[i]; i++)
		for (j = i, k = 0; tolower(s1[j]) == tolower(s2[k]); j++, k++)
			if (!s2[k + 1])
				return (s1 + i);

	return NULL;
}

void CSkeletonPose::DrawBBox(const Matrix34& rRenderMat34)
{
	OBB obb = OBB::CreateOBBfromAABB(Matrix33(rRenderMat34), m_AABB);
	Vec3 wpos = rRenderMat34.GetTranslation();
	g_pAuxGeom->DrawOBB(obb, wpos, 0, RGBA8(0xff, 0x00, 0x1f, 0xff), eBBD_Extremes_Color_Encoded);
}

void CSkeletonPose::DrawPose(const Skeleton::CPoseData& pose, const Matrix34& rRenderMat34)
{
	g_pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags | e_DepthTestOff);

	Vec3 vdir = m_pInstance->m_Viewdir;
	CDefaultSkeleton* pDefaultSkeleton = m_pInstance->m_pDefaultSkeleton;

	static Ang3 ang_root(0, 0, 0);
	ang_root += Ang3(0.01f, 0.02f, 0.03f);
	static Ang3 ang_rot(0, 0, 0);
	ang_rot += Ang3(-0.01f, +0.02f, -0.03f);
	static Ang3 ang_pos(0, 0, 0);
	ang_pos += Ang3(-0.03f, -0.02f, -0.01f);

	f32 fUnitScale = m_pInstance->GetUniformScale();
	const QuatT* pJointsAbsolute = pose.GetJointsAbsolute();
	const JointState* pJointState = pose.GetJointsStatus();
	uint32 jointCount = pose.GetJointCount();

	string filterText;

	ICVar* pVar = gEnv->pSystem->GetIConsole()->GetCVar("ca_filterJoints");
	if (pVar)
		filterText = pVar->GetString();

	bool filtered = filterText != "";
	const char* jointName;

	for (uint32 i = 0; i < jointCount; ++i)
	{
		if (filtered)
		{
			jointName = m_pInstance->m_pDefaultSkeleton->m_arrModelJoints[i].m_strJointName.c_str();

			if (strstri(jointName, filterText) == 0)
				continue;
		}

		int32 parentIndex = m_pInstance->m_pDefaultSkeleton->m_arrModelJoints[i].m_idxParent;
		if (parentIndex > -1)
			g_pAuxGeom->DrawBone(rRenderMat34 * pJointsAbsolute[parentIndex].t, rRenderMat34 * pJointsAbsolute[i].t, RGBA8(0xff, 0xff, 0xef, 0xc0));

		f32 scale = 0.7f;
		AABB aabb_rot = AABB(Vec3(-0.011f, -0.011f, -0.011f) * scale, Vec3(+0.011f, +0.011f, +0.011f) * scale);
		OBB obb_rot = OBB::CreateOBBfromAABB(Matrix33::CreateRotationXYZ(ang_rot), aabb_rot);
		if (pJointState[i] & eJS_Orientation)
			g_pAuxGeom->DrawOBB(obb_rot, rRenderMat34 * pJointsAbsolute[i].t, 0, RGBA8(0xff, 0x00, 0x00, 0xff), eBBD_Extremes_Color_Encoded);

		AABB aabb_pos = AABB(Vec3(-0.010f, -0.010f, -0.010f) * scale, Vec3(+0.010f, +0.010f, +0.010f) * scale);
		OBB obb_pos = OBB::CreateOBBfromAABB(Matrix33::CreateRotationXYZ(ang_pos), aabb_pos);
		if (pJointState[i] & eJS_Position)
			g_pAuxGeom->DrawOBB(obb_pos, rRenderMat34 * pJointsAbsolute[i].t, 0, RGBA8(0x00, 0xff, 0x00, 0xff), eBBD_Extremes_Color_Encoded);

		DrawHelper::Frame(rRenderMat34 * Matrix34(pJointsAbsolute[i]), Vec3(0.15f * fUnitScale));
	}
}

ILINE ColorB shader(Vec3 n, Vec3 d0, Vec3 d1, ColorB c)
{
	f32 a = 0.5f * n | d0, b = 0.1f * n | d1, l = min(1.0f, fabs_tpl(a) - a + fabs_tpl(b) - b + 0.05f);
	return RGBA8(uint8(l * c.r), uint8(l * c.g), uint8(l * c.b), c.a);
}

void CSkeletonPose::DrawSkeleton(const Matrix34& rRenderMat34, uint32 shift)
{
	DrawPose(GetPoseData(), rRenderMat34);
}

f32 CSkeletonPose::SecurityCheck()
{
	f32 fRadius = 0.0f;
	uint32 numJoints = GetPoseData().GetJointCount();
	for (uint32 i = 0; i < numJoints; i++)
	{
		f32 t = GetPoseData().GetJointAbsolute(i).t.GetLength();
		if (fRadius < t)
			fRadius = t;
	}
	return fRadius;
}

uint32 CSkeletonPose::IsSkeletonValid()
{
	uint32 numJoints = GetPoseData().GetJointCount();
	for (uint32 i = 0; i < numJoints; i++)
	{
		uint32 valid = GetPoseData().GetJointAbsolute(i).t.IsValid();
		if (valid == 0)
			return 0;
		if (fabsf(GetPoseData().GetJointAbsolute(i).t.x) > 20000.0f)
			return 0;
		if (fabsf(GetPoseData().GetJointAbsolute(i).t.y) > 20000.0f)
			return 0;
		if (fabsf(GetPoseData().GetJointAbsolute(i).t.z) > 20000.0f)
			return 0;
	}
	return 1;
}

size_t CSkeletonPose::SizeOfThis()
{
	size_t TotalSize = 0;

	TotalSize += GetPoseData().GetAllocationLength();

	TotalSize += m_FaceAnimPosSmooth.get_alloc_size();
	TotalSize += m_FaceAnimPosSmoothRate.get_alloc_size();

	TotalSize += m_arrCGAJoints.get_alloc_size();

	TotalSize += m_physics.SizeOfThis();

	return TotalSize;
}

void CSkeletonPose::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_poseData);

	pSizer->AddObject(m_FaceAnimPosSmooth);
	pSizer->AddObject(m_FaceAnimPosSmoothRate);

	pSizer->AddObject(m_arrCGAJoints);
	pSizer->AddObject(m_limbIk);

	pSizer->AddObject(m_PoseBlenderAim);
	pSizer->AddObject(m_PoseBlenderLook);

	pSizer->AddObject(m_physics);
}

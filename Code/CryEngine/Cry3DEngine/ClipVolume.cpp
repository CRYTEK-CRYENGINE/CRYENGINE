// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "ClipVolume.h"
#include <CryEntitySystem/IEntitySystem.h>

CClipVolume::CClipVolume()
	: m_WorldTM(IDENTITY)
	, m_InverseWorldTM(IDENTITY)
	, m_pBspTree(NULL)
	, m_BBoxWS(AABB::RESET)
	, m_BBoxLS(AABB::RESET)
	, m_nFlags(0)
	, m_nStencilRef(0)
	, m_viewDistRatio(100)
{
	memset(m_sName, 0x0, sizeof(m_sName));
}

void CClipVolume::SetName(const char* szName)
{
	cry_strcpy(m_sName, szName);
}

void CClipVolume::GetClipVolumeMesh(_smart_ptr<IRenderMesh>& renderMesh, Matrix34& worldTM) const
{
	renderMesh = m_pRenderMesh;
	worldTM = m_WorldTM;
}

const AABB& CClipVolume::GetClipVolumeBBox() const
{
	return m_BBoxWS;
}

void CClipVolume::Update(_smart_ptr<IRenderMesh> pRenderMesh, IBSPTree3D* pBspTree, const Matrix34& worldTM, uint8 viewDistRatio, uint32 flags)
{
	const bool bMeshUpdated = m_pRenderMesh != pRenderMesh;

	m_pRenderMesh = std::move(pRenderMesh);
	m_pBspTree = pBspTree;
	m_WorldTM = worldTM;
	m_InverseWorldTM = worldTM.GetInverted();
	m_BBoxWS.Reset();
	m_BBoxLS.Reset();
	m_nFlags = flags;
	m_viewDistRatio = viewDistRatio;

	if (m_pRenderMesh)
	{
		m_pRenderMesh->GetBBox(m_BBoxLS.min, m_BBoxLS.max);
		m_BBoxWS.SetTransformedAABB(worldTM, m_BBoxLS);
	}
}

float CClipVolume::GetMaxViewDist() const
{
	const auto pCVars = static_cast<C3DEngine*>(gEnv->p3DEngine)->GetCVars();
	const float viewDistRatioNormalized = m_viewDistRatio == 255 ? 100.0f : m_viewDistRatio * 0.01f;
	const float clampedSize = std::min(GetFloatCVar(e_ViewDistCompMaxSize), m_BBoxWS.GetRadius());
	const float maxViewDist = pCVars->e_ViewDistRatio * clampedSize * viewDistRatioNormalized;

	return std::max(pCVars->e_ViewDistMin, maxViewDist);
}

bool CClipVolume::IsPointInsideClipVolume(const Vec3& point) const
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_pRenderMesh || !m_pBspTree || !m_BBoxWS.IsContainPoint(point))
		return false;

	Vec3 pt = m_InverseWorldTM.TransformPoint(point);
	return m_BBoxLS.IsContainPoint(pt) && m_pBspTree->IsInside(pt);
}

void CClipVolume::GetMemoryUsage(class ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
}

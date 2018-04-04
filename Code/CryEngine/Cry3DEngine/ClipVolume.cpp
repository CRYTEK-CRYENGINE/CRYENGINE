// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"

#include "ClipVolume.h"
#include <CryEntitySystem/IEntitySystem.h>

CClipVolume::CClipVolume()
	: m_nStencilRef(0)
	, m_WorldTM(IDENTITY)
	, m_InverseWorldTM(IDENTITY)
	, m_BBoxWS(AABB::RESET)
	, m_BBoxLS(AABB::RESET)
	, m_pBspTree(NULL)
{
	memset(m_sName, 0x0, sizeof(m_sName));
}

CClipVolume::~CClipVolume()
{
	m_pRenderMesh = NULL;

	AUTO_LOCK(m_lstRenderNodesCritSection);
	for (size_t i = 0; i < m_lstRenderNodes.size(); ++i)
	{
		IRenderNode* pNode = m_lstRenderNodes[i];
		if (pNode->m_pTempData)
			pNode->m_pTempData->userData.m_pClipVolume = NULL;
	}
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

AABB CClipVolume::GetClipVolumeBBox() const
{
	return m_BBoxWS;
}

void CClipVolume::Update(_smart_ptr<IRenderMesh> pRenderMesh, IBSPTree3D* pBspTree, const Matrix34& worldTM, uint32 flags)
{
	const bool bMeshUpdated = m_pRenderMesh != pRenderMesh;

	m_pRenderMesh = pRenderMesh;
	m_pBspTree = pBspTree;
	m_WorldTM = worldTM;
	m_InverseWorldTM = worldTM.GetInverted();
	m_BBoxWS.Reset();
	m_BBoxLS.Reset();
	m_nFlags = flags;

	if (m_pRenderMesh)
	{
		pRenderMesh->GetBBox(m_BBoxLS.min, m_BBoxLS.max);
		m_BBoxWS.SetTransformedAABB(worldTM, m_BBoxLS);
	}
}

bool CClipVolume::IsPointInsideClipVolume(const Vec3& point) const
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_pRenderMesh || !m_pBspTree || !m_BBoxWS.IsContainPoint(point))
		return false;

	Vec3 pt = m_InverseWorldTM.TransformPoint(point);
	return m_BBoxLS.IsContainPoint(pt) && m_pBspTree->IsInside(pt);
}

void CClipVolume::RegisterRenderNode(IRenderNode* pRenderNode)
{
	AUTO_LOCK(m_lstRenderNodesCritSection);
	if (m_lstRenderNodes.Find(pRenderNode) < 0)
	{
		m_lstRenderNodes.Add(pRenderNode);

		if (pRenderNode->m_pTempData)
			pRenderNode->m_pTempData->userData.m_pClipVolume = this;
	}
}
void CClipVolume::UnregisterRenderNode(IRenderNode* pRenderNode)
{
	AUTO_LOCK(m_lstRenderNodesCritSection);
	if (m_lstRenderNodes.Delete(pRenderNode) && pRenderNode->m_pTempData)
		pRenderNode->m_pTempData->userData.m_pClipVolume = NULL;
}

void CClipVolume::GetMemoryUsage(class ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
}

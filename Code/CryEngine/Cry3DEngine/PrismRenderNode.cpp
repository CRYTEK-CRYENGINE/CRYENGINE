// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if !defined(EXCLUDE_DOCUMENTATION_PURPOSE)

	#include "PrismRenderNode.h"

CPrismRenderNode::CPrismRenderNode() : m_pMaterial(0)
{
	GetInstCount(GetRenderNodeType())++;

	m_mat.SetIdentity();
	m_WSBBox = AABB(Vec3(-1, -1, -1), Vec3(1, 1, 1));
	m_pRE = (CREPrismObject*) GetRenderer()->EF_CreateRE(eDATA_PrismObject);
	//	m_pMaterial = GetMatMan()->LoadMaterial("Materials/VolumeData/Default", false);
	m_dwRndFlags |= ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS;
}

CPrismRenderNode::~CPrismRenderNode()
{
	GetInstCount(GetRenderNodeType())--;

	if (m_pRE)
		m_pRE->Release(false);

	Get3DEngine()->FreeRenderNodeState(this);
}

void CPrismRenderNode::SetMatrix(const Matrix34& mat)
{
	m_mat = mat;
	m_WSBBox.SetTransformedAABB(mat, AABB(Vec3(-1, -1, -1), Vec3(1, 1, 1)));
	Get3DEngine()->RegisterEntity(this);
}

const char* CPrismRenderNode::GetName() const
{
	return "PrismObject";
}

void CPrismRenderNode::Render(const SRendParams& rParam, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_pMaterial)
		return;

	CRenderObject* pRO = GetRenderer()->EF_GetObject_Temp(passInfo.ThreadID());   // pointer could be cached

	if (pRO)
	{
		// set basic render object properties
		pRO->m_II.m_Matrix = m_mat;
		pRO->m_ObjFlags |= FOB_TRANS_MASK;
		pRO->m_fSort = 0;
		pRO->m_fDistance = rParam.fDistance;

		// transform camera into object space
		const CCamera& cam(passInfo.GetCamera());
		Vec3 viewerPosWS(cam.GetPosition());

		m_pRE->m_center = m_mat.GetTranslation();

		SShaderItem& shaderItem(m_pMaterial->GetShaderItem(0));

		GetRenderer()->EF_AddEf(m_pRE, shaderItem, pRO, passInfo, EFSLIST_GENERAL, 0);
	}
}

void CPrismRenderNode::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "PrismRenderNode");
	pSizer->AddObject(this, sizeof(*this));
}

void CPrismRenderNode::OffsetPosition(const Vec3& delta)
{
	if (m_pTempData) m_pTempData->OffsetPosition(delta);
	m_WSBBox.Move(delta);
	m_mat.SetTranslation(m_mat.GetTranslation() + delta);
	if (m_pRE) m_pRE->m_center += delta;
}

void CPrismRenderNode::FillBBox(AABB& aabb)
{
	aabb = CPrismRenderNode::GetBBox();
}

EERType CPrismRenderNode::GetRenderNodeType()
{
	return eERType_PrismObject;
}

float CPrismRenderNode::GetMaxViewDist()
{
	return 1000.0f;
}

Vec3 CPrismRenderNode::GetPos(bool bWorldOnly) const
{
	return m_mat.GetTranslation();
}

IMaterial* CPrismRenderNode::GetMaterial(Vec3* pHitPos) const
{
	return m_pMaterial;
}

#endif // EXCLUDE_DOCUMENTATION_PURPOSE

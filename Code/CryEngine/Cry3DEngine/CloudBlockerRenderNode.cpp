// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CloudBlockerRenderNode.h"

CCloudBlockerRenderNode::CCloudBlockerRenderNode()
	: m_WSBBox(1.0f)
	, m_position(0.0f, 0.0f, 0.0f)
	, m_decayStart(0.0f)
	, m_decayEnd(0.0f)
	, m_decayInfluence(0.0)
	, m_bScreenspace(false)
	, m_pOwnerEntity(nullptr)
{
	GetInstCount(GetRenderNodeType())++;

	// cloud blocker must be always visible.
	SetRndFlags(ERF_HUD | ERF_RENDER_ALWAYS, true);
}

CCloudBlockerRenderNode::~CCloudBlockerRenderNode()
{
	GetInstCount(GetRenderNodeType())--;

	Get3DEngine()->FreeRenderNodeState(this);
}

void CCloudBlockerRenderNode::SetMatrix(const Matrix34& mat)
{
	if (m_position == mat.GetTranslation())
		return;

	m_position = mat.GetTranslation();
	m_WSBBox.SetTransformedAABB(mat, AABB(1.0f));

	Get3DEngine()->RegisterEntity(this);
}

void CCloudBlockerRenderNode::OffsetPosition(const Vec3& delta)
{
	if (const auto pTempData = m_pTempData.load()) pTempData->OffsetPosition(delta);
	m_position += delta;
	m_WSBBox.Move(delta);
}

void CCloudBlockerRenderNode::Render(const struct SRendParams& EntDrawParams, const SRenderingPassInfo& passInfo)
{
	// recursive pass isn't supported currently.
	if (!passInfo.RenderClouds() || passInfo.IsRecursivePass())
	{
		return;
	}

	IRenderView* pRenderView = passInfo.GetIRenderView();
	int32 flag = m_bScreenspace ? 1 : 0;
	pRenderView->AddCloudBlocker(m_position, Vec3(m_decayStart, m_decayEnd, m_decayInfluence), flag);
}

void CCloudBlockerRenderNode::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "CloudBlocker");
	pSizer->AddObject(this, sizeof(*this));
}

void CCloudBlockerRenderNode::SetProperties(const SCloudBlockerProperties& properties)
{
	m_decayStart = properties.decayStart;
	m_decayEnd = properties.decayEnd;
	m_decayInfluence = properties.decayInfluence;
	m_bScreenspace = properties.bScreenspace;
}

void CCloudBlockerRenderNode::FillBBox(AABB& aabb)
{
	aabb = CCloudBlockerRenderNode::GetBBox();
}

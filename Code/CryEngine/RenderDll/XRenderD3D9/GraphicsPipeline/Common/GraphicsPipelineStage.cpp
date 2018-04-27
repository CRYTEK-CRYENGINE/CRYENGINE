// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "GraphicsPipelineStage.h"
#include "../StandardGraphicsPipeline.h"

//////////////////////////////////////////////////////////////////////////
CStandardGraphicsPipeline& CGraphicsPipelineStage::GetStdGraphicsPipeline()
{
	assert(m_pGraphicsPipeline);
	return *static_cast<CStandardGraphicsPipeline*>(m_pGraphicsPipeline);
}

//////////////////////////////////////////////////////////////////////////
const CStandardGraphicsPipeline& CGraphicsPipelineStage::GetStdGraphicsPipeline() const
{
	assert(m_pGraphicsPipeline);
	return *static_cast<const CStandardGraphicsPipeline*>(m_pGraphicsPipeline);
}

const CRenderOutput& CGraphicsPipelineStage::GetRenderOutput() const
{
	assert(RenderView()->GetRenderOutput());
	return *RenderView()->GetRenderOutput();
}

CRenderOutput& CGraphicsPipelineStage::GetRenderOutput()
{
	assert(RenderView()->GetRenderOutput());
	return *RenderView()->GetRenderOutput();
}

void CGraphicsPipelineStage::ClearDeviceOutputState()
{
	GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->ClearState(true);
}

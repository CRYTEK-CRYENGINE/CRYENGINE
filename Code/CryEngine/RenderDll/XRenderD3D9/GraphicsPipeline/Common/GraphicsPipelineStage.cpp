// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "GraphicsPipelineStage.h"

//////////////////////////////////////////////////////////////////////////

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

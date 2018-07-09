// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GraphicsPipeline.h"

//////////////////////////////////////////////////////////////////////////
CGraphicsPipeline::CGraphicsPipeline()
{
	m_pipelineStages.fill(nullptr);
}

//////////////////////////////////////////////////////////////////////////
CGraphicsPipeline::~CGraphicsPipeline()
{
	ShutDown();
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::ClearState()
{
	GetDeviceObjectFactory().GetCoreCommandList().Reset();
}

void CGraphicsPipeline::ClearDeviceState()
{
	GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->ClearState(false);
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::ShutDown()
{
	// destroy stages in reverse order to satisfy data dependencies
	for (auto it = m_pipelineStages.rbegin(); it != m_pipelineStages.rend(); ++it)
	{
		if (*it) delete *it;
	}

	m_pipelineStages.fill(nullptr);
	ResetUtilityPassCache();
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::Resize(int renderWidth, int renderHeight)
{
	// Sets the current render resolution on all the pipeline stages.
	for (auto it = m_pipelineStages.begin(); it != m_pipelineStages.end(); ++it)
	{
		if (*it) (*it)->Resize(renderWidth, renderHeight);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::SetCurrentRenderView(CRenderView* pRenderView)
{
	m_pCurrentRenderView = pRenderView;

	// Sets the current render view on all the pipeline stages.
	for (auto it = m_pipelineStages.begin(); it != m_pipelineStages.end(); ++it)
	{
		if (*it) (*it)->SetRenderView(pRenderView);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::Update(CRenderView* pRenderView, EShaderRenderingFlags renderingFlags)
{
	SetCurrentRenderView(pRenderView);

	// Sets the current render view on all the pipeline stages.
	for (auto it = m_pipelineStages.begin(); it != m_pipelineStages.end(); ++it)
	{
		if (*it && (*it)->IsStageActive(renderingFlags))
			(*it)->Update();
	}
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::OnCVarsChanged(CCVarUpdateRecorder& rCVarRecs)
{
	for (auto it = m_pipelineStages.begin(); it != m_pipelineStages.end(); ++it)
	{
		if (*it) (*it)->OnCVarsChanged(rCVarRecs);
	}
}
// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SceneRenderPass.h"

#include "Common/RenderView.h"
#include "Common/ReverseDepth.h"
#include "CompiledRenderObject.h"
#include "GraphicsPipelineStage.h"

int CSceneRenderPass::s_recursionCounter = 0;

CSceneRenderPass::CSceneRenderPass()
	: m_renderPassDesc()
	, m_passFlags(ePassFlags_None)
{
	m_numRenderItemGroups = 0;
	m_profilerSectionIndex = ~0u;

	SetLabel("SCENE_PASS");
}

void CSceneRenderPass::SetupPassContext(uint32 stageID, uint32 stagePassID, EShaderTechniqueID technique, uint32 filter, ERenderListID renderList, uint32 excludeFilter, bool drawCompiledRenderObject)
{
	// the scene render passes which draw CCompiledRenderObject must follow the strict rule of PSOs array and PSO cache in CCompiledRenderObject
	const bool drawable = (drawCompiledRenderObject && stageID < MAX_PIPELINE_SCENE_STAGES) || !drawCompiledRenderObject;
	assert(drawable);
	m_stageID = stageID;
	m_passID = stagePassID;
	m_technique = technique;
	m_batchFilter = drawable ? filter : 0;
	m_excludeFilter = excludeFilter;
	m_renderList = renderList;
}

void CSceneRenderPass::SetPassResources(CDeviceResourceLayoutPtr pResourceLayout, CDeviceResourceSetPtr pPerPassResources)
{
	m_pResourceLayout = pResourceLayout;
	m_pPerPassResourceSet = pPerPassResources;
}

void CSceneRenderPass::SetRenderTargets(CTexture* pDepthTarget, CTexture* pColorTarget0, CTexture* pColorTarget1, CTexture* pColorTarget2, CTexture* pColorTarget3)
{
	m_renderPassDesc.SetRenderTarget(0, pColorTarget0);
	m_renderPassDesc.SetRenderTarget(1, pColorTarget1);
	m_renderPassDesc.SetRenderTarget(2, pColorTarget2);
	m_renderPassDesc.SetRenderTarget(3, pColorTarget3);
	m_renderPassDesc.SetDepthTarget(pDepthTarget);

	CRY_ASSERT_MESSAGE(
		(!pColorTarget0 || !pColorTarget1 || pColorTarget0->GetWidth() == pColorTarget1->GetWidth()) &&
		(!pColorTarget1 || !pColorTarget2 || pColorTarget1->GetWidth() == pColorTarget2->GetWidth()) &&
		(!pColorTarget2 || !pColorTarget3 || pColorTarget2->GetWidth() == pColorTarget3->GetWidth()),
		"Color targets are of different size!");
	CRY_ASSERT_MESSAGE(
		(!pDepthTarget  || !pColorTarget0 || pDepthTarget ->GetWidth() >= pColorTarget0->GetWidth()),
		"Depth target is smaller than the color target(s)!");

	// TODO: refactor, shouldn't need to update the renderpass here but PSOs are compiled before CSceneRenderPass::Prepare is called
	CDeviceRenderPass::UpdateWithReevaluation(m_pRenderPass, m_renderPassDesc);
}

void CSceneRenderPass::SetViewport(const D3DViewPort& viewport)
{
	m_viewPort[0] =
	m_viewPort[1] = viewport;

	if (m_passFlags & CSceneRenderPass::ePassFlags_RenderNearest)
	{
		m_viewPort[1].MinDepth = 0;
		m_viewPort[1].MaxDepth = CRenderer::CV_r_DrawNearZRange;
		if (m_passFlags & CSceneRenderPass::ePassFlags_ReverseDepth)
			m_viewPort[1] = ReverseDepthHelper::Convert(m_viewPort[1]);
	}

	D3DRectangle scissorRect =
	{
		LONG(m_viewPort[0].TopLeftX),
		LONG(m_viewPort[0].TopLeftY),
		LONG(m_viewPort[0].TopLeftX + m_viewPort[0].Width),
		LONG(m_viewPort[0].TopLeftY + m_viewPort[0].Height)
	};

	m_scissorRect = scissorRect;
}

void CSceneRenderPass::SetViewport(const SRenderViewport& viewport)
{
	SetViewport(RenderViewportToD3D11Viewport(viewport));
}

void CSceneRenderPass::SetDepthBias(float constBias, float slopeBias, float biasClamp)
{ 
	m_depthConstBias = constBias; 
	m_depthSlopeBias = slopeBias; 
	m_depthBiasClamp = biasClamp; 
}

void CSceneRenderPass::ExchangeRenderTarget(uint32 slot, CTexture* pNewColorTarget, ResourceViewHandle hRenderTargetView)
{
	CRY_ASSERT(!pNewColorTarget || pNewColorTarget->GetDevTexture());
	m_renderPassDesc.SetRenderTarget(slot, pNewColorTarget, hRenderTargetView);
}

void CSceneRenderPass::ExchangeDepthTarget(CTexture* pNewDepthTarget, ResourceViewHandle hDepthStencilView)
{
	CRY_ASSERT(!pNewDepthTarget || pNewDepthTarget->GetDevTexture());
	m_renderPassDesc.SetDepthTarget(pNewDepthTarget, hDepthStencilView);
}

void CSceneRenderPass::PrepareRenderPassForUse(CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	CDeviceRenderPass::UpdateWithReevaluation(m_pRenderPass, m_renderPassDesc);

	CDeviceGraphicsCommandInterface* pCommandInterface = commandList.GetGraphicsInterface();
	pCommandInterface->PrepareRenderPassForUse(*m_pRenderPass);
	pCommandInterface->PrepareResourcesForUse(EResourceLayoutSlot_PerPassRS, m_pPerPassResourceSet.get());

	if (m_passFlags & ePassFlags_VrProjectionPass)
	{
		if (CVrProjectionManager::IsMultiResEnabledStatic())
		{
			// we don't know the bNearest flag here, so just prepare for both cases
			CVrProjectionManager::Instance()->PrepareProjectionParameters(commandList, GetViewport(false));
			CVrProjectionManager::Instance()->PrepareProjectionParameters(commandList, GetViewport(true));
		}
	}
}

void CSceneRenderPass::BeginRenderPass(CDeviceCommandListRef RESTRICT_REFERENCE commandList, bool bNearest, uint32 profilerSectionIndex, bool bIssueGPUTimestamp) const
{
	// Note: Function has to be threadsafe since it can be called from several worker threads

	if (gcpRendD3D->m_pPipelineProfiler)
		gcpRendD3D->m_pPipelineProfiler->UpdateMultithreadedSection(profilerSectionIndex, true, 0, 0, bIssueGPUTimestamp, &commandList);

#if defined(ENABLE_PROFILING_CODE)
	commandList.BeginProfilingSection();
#endif

	D3D11_VIEWPORT viewport = GetViewport(bNearest);
	bool bViewportSet = false;

	commandList.Reset();

	CDeviceGraphicsCommandInterface* pCommandInterface = commandList.GetGraphicsInterface();
	pCommandInterface->BeginProfilerEvent(GetLabel());
	pCommandInterface->BeginRenderPass(*m_pRenderPass, m_scissorRect);

	if (m_passFlags & ePassFlags_VrProjectionPass)
	{
		bViewportSet = CVrProjectionManager::Instance()->SetRenderingState(commandList, viewport,
			(m_passFlags & ePassFlags_UseVrProjectionState) != 0, (m_passFlags & ePassFlags_RequireVrProjectionConstants) != 0);
	}
	
	if (!bViewportSet)
	{
		pCommandInterface->SetViewports(1, &viewport);
		pCommandInterface->SetScissorRects(1, &m_scissorRect);
	}

	pCommandInterface->SetResourceLayout(m_pResourceLayout.get());
	pCommandInterface->SetResources(EResourceLayoutSlot_PerPassRS, m_pPerPassResourceSet.get());

#if (CRY_RENDERER_DIRECT3D < 120)
	pCommandInterface->SetDepthBias(m_depthConstBias, m_depthSlopeBias, m_depthBiasClamp);
#endif
}

void CSceneRenderPass::EndRenderPass(CDeviceCommandListRef RESTRICT_REFERENCE commandList, bool bNearest, uint32 profilerSectionIndex, bool bIssueGPUTimestamp) const
{
	// Note: Function has to be threadsafe since it can be called from several worker threads

	CDeviceGraphicsCommandInterface* pCommandInterface = commandList.GetGraphicsInterface();
	pCommandInterface->EndProfilerEvent(GetLabel());
	pCommandInterface->EndRenderPass(*m_pRenderPass);

#if (CRY_RENDERER_DIRECT3D < 120)
	pCommandInterface->SetDepthBias(0.0f, 0.0f, 0.0f);
#endif

#if defined(ENABLE_PROFILING_CODE)
	if (gcpRendD3D->m_pPipelineProfiler)
	{
		gcpRendD3D->m_pPipelineProfiler->UpdateMultithreadedSection(profilerSectionIndex, false, commandList.EndProfilingSection().numDIPs,
		                                                            commandList.EndProfilingSection().numPolygons, bIssueGPUTimestamp, &commandList);
	}
	
	gcpRendD3D->AddRecordedProfilingStats(commandList.EndProfilingSection(), m_renderList, true);
#endif

	if (m_passFlags & ePassFlags_UseVrProjectionState)
	{
		CDeviceGraphicsCommandInterface* pCommandInterface = commandList.GetGraphicsInterface();
		CVrProjectionManager::Instance()->RestoreState(commandList);
	}
}

void CSceneRenderPass::BeginExecution()
{
	assert(s_recursionCounter == 0);
	s_recursionCounter += 1;
	
	m_numRenderItemGroups = 0;
	
	if (gcpRendD3D->m_pPipelineProfiler)
		m_profilerSectionIndex = gcpRendD3D->m_pPipelineProfiler->InsertMultithreadedSection(GetLabel());

	if (gcpRendD3D->GetGraphicsPipeline().GetRenderPassScheduler().IsActive())
	{
		gcpRendD3D->GetGraphicsPipeline().GetRenderPassScheduler().AddPass(this);
	}
}

void CSceneRenderPass::EndExecution()
{
	s_recursionCounter -= 1;
}


void CSceneRenderPass::DrawRenderItems(CRenderView* pRenderView, ERenderListID list, int listStart, int listEnd, int profilingListID)
{
	assert(s_recursionCounter == 1);
	
	uint32 nBatchFlags = pRenderView->GetBatchFlags(list);

	if (m_batchFilter != FB_MASK && !(nBatchFlags & m_batchFilter))
		return;

	SGraphicsPipelinePassContext passContext(pRenderView, this, m_technique, m_batchFilter, m_excludeFilter);

	passContext.rendItems.start = listStart < 0 ? 0 : listStart;
	passContext.rendItems.end = listEnd < 0 ? pRenderView->GetRenderItems(list).size() : listEnd;

	if (passContext.rendItems.IsEmpty())
		return;

	passContext.stageID = m_stageID;
	passContext.passID = m_passID;

	const bool bNearest = (list == EFSLIST_NEAREST_OBJECTS) || (list == EFSLIST_FORWARD_OPAQUE_NEAREST) || (list == EFSLIST_TRANSP_NEAREST);

	passContext.renderNearest = bNearest && (m_passFlags & CSceneRenderPass::ePassFlags_RenderNearest);
	passContext.renderListId = list;
	passContext.renderItemGroup = m_numRenderItemGroups++;
	passContext.profilerSectionIndex = m_profilerSectionIndex;

#if defined(DO_RENDERSTATS)
	CD3D9Renderer* pRenderer = gcpRendD3D;

	if (pRenderer->CV_r_stats == 6 || pRenderer->m_pDebugRenderNode || pRenderer->m_bCollectDrawCallsInfoPerNode)
		passContext.pDrawCallInfoPerNode = gcpRendD3D->GetGraphicsPipeline().GetDrawCallInfoPerNode();
	if (pRenderer->m_bCollectDrawCallsInfo)
		passContext.pDrawCallInfoPerMesh = gcpRendD3D->GetGraphicsPipeline().GetDrawCallInfoPerMesh();
#endif
	
	if (gcpRendD3D->GetGraphicsPipeline().GetRenderPassScheduler().IsActive())
	{
		m_passContexts.push_back(passContext);
		return;
	}

	if (!CRenderer::CV_r_NoDraw)
	{
		passContext.pRenderView->DrawCompiledRenderItems(passContext);
	}
}

void CSceneRenderPass::Execute()
{
	for (auto& passContext : m_passContexts)
	{
		passContext.pRenderView->DrawCompiledRenderItems(passContext);
	}
}

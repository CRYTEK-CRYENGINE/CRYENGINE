// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SceneRenderPass.h"

#include "Common/RenderView.h"
#include "Common/ReverseDepth.h"
#include "Common/Textures/TextureHelpers.h"
#include "CompiledRenderObject.h"
#include "GraphicsPipelineStage.h"

#include <algorithm>
#include <iterator>
#include <vector>

int CSceneRenderPass::s_recursionCounter = 0;
CDeviceResourceSetDesc* CSceneRenderPass::s_pDefaultMaterialBindPoints = nullptr;
CDeviceResourceSetDesc* CSceneRenderPass::s_pDefaultDrawExtraRL = nullptr;
CDeviceResourceSetPtr   CSceneRenderPass::s_pDefaultDrawExtraRS = nullptr;

CSceneRenderPass::CSceneRenderPass()
	: m_renderPassDesc()
	, m_passFlags(ePassFlags_None)
{
	m_numRenderItemGroups = 0;

	SetLabel("SCENE_PASS");
}

void CSceneRenderPass::Initialize()
{
	if (!s_pDefaultMaterialBindPoints)
	{
		s_pDefaultMaterialBindPoints = new CDeviceResourceSetDesc();

		// default material bind points
		{
			s_pDefaultMaterialBindPoints->SetConstantBuffer(eConstantBufferShaderSlot_PerMaterial, CDeviceBufferManager::GetNullConstantBuffer(), EShaderStage_AllWithoutCompute);

			for (EEfResTextures texType = EFTT_DIFFUSE; texType < EFTT_MAX; texType = EEfResTextures(texType + 1))
			{
				if (TextureHelpers::IsSlotAvailable(texType))
				{
					EShaderStage shaderStages = TextureHelpers::GetShaderStagesForTexSlot(texType);
					s_pDefaultMaterialBindPoints->SetTexture(texType, CRendererResources::s_pTexNULL, EDefaultResourceViews::Default, shaderStages);
				}
			}
		}
	}

	// default extra per instance
	if (!s_pDefaultDrawExtraRL)
	{
		s_pDefaultDrawExtraRL = new CDeviceResourceSetDesc();

		s_pDefaultDrawExtraRL->SetConstantBuffer(eConstantBufferShaderSlot_SkinQuat, CDeviceBufferManager::GetNullConstantBuffer(), EShaderStage_Vertex);
		s_pDefaultDrawExtraRL->SetConstantBuffer(eConstantBufferShaderSlot_SkinQuatPrev, CDeviceBufferManager::GetNullConstantBuffer(), EShaderStage_Vertex);
		s_pDefaultDrawExtraRL->SetConstantBuffer(eConstantBufferShaderSlot_PerGroup, CDeviceBufferManager::GetNullConstantBuffer(), EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Hull);

		// Deliberately aliasing slots/use-cases here for visibility (e.g. EReservedTextureSlot_ComputeSkinVerts, EReservedTextureSlot_SkinExtraWeights and
		// EReservedTextureSlot_GpuParticleStream). The resource layout will just pick the first.
		s_pDefaultDrawExtraRL->SetBuffer(EReservedTextureSlot_SkinExtraWeights, CDeviceBufferManager::GetNullBufferStructured(), EDefaultResourceViews::Default, EShaderStage_Vertex);
		s_pDefaultDrawExtraRL->SetBuffer(EReservedTextureSlot_ComputeSkinVerts, CDeviceBufferManager::GetNullBufferStructured(), EDefaultResourceViews::Default, EShaderStage_Vertex);
		s_pDefaultDrawExtraRL->SetBuffer(EReservedTextureSlot_GpuParticleStream, CDeviceBufferManager::GetNullBufferStructured(), EDefaultResourceViews::Default, EShaderStage_Vertex);
		s_pDefaultDrawExtraRL->SetBuffer(EReservedTextureSlot_AdjacencyInfo, CDeviceBufferManager::GetNullBufferTyped(), EDefaultResourceViews::Default, EShaderStage_Domain);

		if (!s_pDefaultDrawExtraRS)
		{
			s_pDefaultDrawExtraRS = GetDeviceObjectFactory().CreateResourceSet();
			s_pDefaultDrawExtraRS->Update(*s_pDefaultDrawExtraRL);
		}
	}
}

void CSceneRenderPass::Shutdown()
{
	if (s_pDefaultMaterialBindPoints)
	{
		SAFE_DELETE(s_pDefaultMaterialBindPoints);
	}

	if (s_pDefaultDrawExtraRL)
	{
		s_pDefaultDrawExtraRL->ClearResources();
		SAFE_DELETE(s_pDefaultDrawExtraRL);
	}
	
	if (s_pDefaultDrawExtraRS)
	{
		s_pDefaultDrawExtraRS.reset();
	}	
}

//////////////////////////////////////////////////////////////////////////
bool CSceneRenderPass::FillCommonScenePassStates(const SGraphicsPipelineStateDescription& inputDesc, CDeviceGraphicsPSODesc& psoDesc, CVrProjectionManager* pVRProjectionManager)
{
	CShader* pShader = static_cast<CShader*>(inputDesc.shaderItem.m_pShader);
	SShaderTechnique* pTechnique = pShader->GetTechnique(inputDesc.shaderItem.m_nTechnique, inputDesc.technique, true);
	if (!pTechnique)
		return false;

	CShaderResources* pRes = static_cast<CShaderResources*>(inputDesc.shaderItem.m_pShaderResources);
	const uint64 objectFlags = inputDesc.objectFlags;
	SShaderPass* pShaderPass = &pTechnique->m_Passes[0];

	// Handle quality flags
	CStandardGraphicsPipeline::ApplyShaderQuality(psoDesc, gcpRendD3D->GetShaderProfile(pShader->m_eShaderType));

	psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_REVERSE_DEPTH];

	// Set resource states
	bool bTwoSided = false;

	if (pRes->m_ResFlags & MTL_FLAG_2SIDED)
		bTwoSided = true;

	if (pRes->IsAlphaTested())
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_ALPHATEST];

	if (pRes->m_Textures[EFTT_DIFFUSE] && pRes->m_Textures[EFTT_DIFFUSE]->m_Ext.m_pTexModifier)
		psoDesc.m_ShaderFlags_MD |= pRes->m_Textures[EFTT_DIFFUSE]->m_Ext.m_nUpdateFlags;

	// Merge EDeformType into EVertexModifier to save space/parameters
	if (pRes->m_pDeformInfo)
		psoDesc.m_ShaderFlags_MDV |= EVertexModifier(pRes->m_pDeformInfo->m_eType);

	psoDesc.m_ShaderFlags_MDV |= psoDesc.m_pShader->m_nMDV;

	if (objectFlags & FOB_OWNER_GEOMETRY)
		psoDesc.m_ShaderFlags_MDV &= ~MDV_DEPTH_OFFSET;

	if (objectFlags & FOB_BENDED)
		psoDesc.m_ShaderFlags_MDV |= MDV_BENDING;

	if (!(objectFlags & FOB_TRANS_MASK))
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_OBJ_IDENTITY];

	if (objectFlags & FOB_BLEND_WITH_TERRAIN_COLOR)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_BLEND_WITH_TERRAIN_COLOR];

	psoDesc.m_bAllowTesselation = false;
	psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_NO_TESSELLATION];

	if (objectFlags & FOB_NEAREST)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_NEAREST];

	if (objectFlags & FOB_DISSOLVE)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_DISSOLVE];

	if (psoDesc.m_RenderState & GS_ALPHATEST)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_ALPHATEST];

#ifdef TESSELLATION_RENDERER
	const bool bHasTesselationShaders = pShaderPass && pShaderPass->m_HShader && pShaderPass->m_DShader;
	if (bHasTesselationShaders && (!(objectFlags & FOB_NEAREST) && (objectFlags & FOB_ALLOW_TESSELLATION)))
	{
		psoDesc.m_ShaderFlags_RT &= ~g_HWSR_MaskBit[HWSR_NO_TESSELLATION];
		psoDesc.m_bAllowTesselation = true;
	}
#endif

	psoDesc.m_CullMode = bTwoSided ? eCULL_None : ((pShaderPass && pShaderPass->m_eCull != -1) ? (ECull)pShaderPass->m_eCull : eCULL_Back);
	psoDesc.m_PrimitiveType = inputDesc.primitiveType;

	if (psoDesc.m_bAllowTesselation && (psoDesc.m_PrimitiveType < ept1ControlPointPatchList || psoDesc.m_PrimitiveType > ept4ControlPointPatchList))
	{
		// Hard-coded primitive type should not be set here, do it only if tessellation primitive type not already set
		psoDesc.m_PrimitiveType = ept3ControlPointPatchList;
		psoDesc.m_ObjectStreamMask |= VSM_NORMALS;
	}

	psoDesc.m_ShaderFlags_RT |= pVRProjectionManager->GetRTFlags();

	return true;
}

void CSceneRenderPass::SetupDrawContext(uint32 stageID, uint32 stagePassID, EShaderTechniqueID technique, uint32 includeFilter, uint32 excludeFilter)
{
	m_stageID = stageID;
	m_passID = stagePassID;
	m_technique = technique;
	m_batchIncludeFilter = includeFilter;
	m_batchExcludeFilter = excludeFilter;
}

void CSceneRenderPass::SetPassResources(CDeviceResourceLayoutPtr pResourceLayout, CDeviceResourceSetPtr pPerPassResources)
{
	CRY_ASSERT_MESSAGE(!!pResourceLayout, "Layout to be set turns out to be invalid!");

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
		(!pDepthTarget || !pColorTarget0 || pDepthTarget->GetWidth() >= pColorTarget0->GetWidth()),
		"Depth target is smaller than the color target(s)!");

	// TODO: refactor, shouldn't need to update the renderpass here but PSOs are compiled before CSceneRenderPass::Prepare is called
	CDeviceRenderPass::UpdateWithReevaluation(m_pRenderPass, m_renderPassDesc);
}

void CSceneRenderPass::SetViewport(const D3DViewPort& viewport)
{
	m_viewPort[0] = m_viewPort[1] = viewport;

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
		if (gcpRendD3D.GetVrProjectionManager()->IsMultiResEnabledStatic())
		{
			// we don't know the bNearest flag here, so just prepare for both cases
			gcpRendD3D.GetVrProjectionManager()->PrepareProjectionParameters(commandList, GetViewport(false));
			gcpRendD3D.GetVrProjectionManager()->PrepareProjectionParameters(commandList, GetViewport(true));
		}
	}
}

void CSceneRenderPass::ResolvePass(CGraphicsPipeline& graphicsPipeline, CDeviceCommandListRef RESTRICT_REFERENCE commandList, const std::vector<TRect_tpl<uint16>>& screenBounds) const
{
	const auto textureWidth = graphicsPipeline.GetPipelineResources().m_pTexHDRTarget->GetWidth();
	const auto textureHeight = graphicsPipeline.GetPipelineResources().m_pTexHDRTarget->GetHeight();

	for (const auto& bounds : screenBounds)
	{
		const auto x = bounds.Min.x;
		const auto y = bounds.Min.y;
		const auto w = bounds.Max.x - x;
		const auto h = bounds.Max.y - y;

		SResourceCoordinate region = { x, y, 0, 0 };
		SResourceRegionMapping mapping =
		{
			region,   // src position
			region,   // dst position
			{
				static_cast<UINT>(std::max<int>(0, std::min<int>(w, textureWidth - x))),
				static_cast<UINT>(std::max<int>(0, std::min<int>(h, textureHeight - y))),
				1, 1
			},    // size
			D3D11_COPY_NO_OVERWRITE_CONC // This is being done from job threads
		};

		if (mapping.Extent.Width && mapping.Extent.Height && mapping.Extent.Depth)
			commandList.GetCopyInterface()->Copy(graphicsPipeline.GetPipelineResources().m_pTexHDRTarget->GetDevTexture(), graphicsPipeline.GetPipelineResources().m_pTexSceneTarget->GetDevTexture(), mapping);
	}
}

void CSceneRenderPass::BeginRenderPass(CDeviceCommandListRef RESTRICT_REFERENCE commandList, bool bNearest) const
{
	// Note: Function has to be threadsafe since it can be called from several worker threads

	D3D11_VIEWPORT viewport = GetViewport(bNearest);
	bool bViewportSet = false;

	CDeviceGraphicsCommandInterface* pCommandInterface = commandList.GetGraphicsInterface();
	pCommandInterface->BeginRenderPass(*m_pRenderPass, m_scissorRect);

	if (m_passFlags & ePassFlags_VrProjectionPass)
	{
		bViewportSet = gcpRendD3D.GetVrProjectionManager()->SetRenderingState(commandList, viewport,
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

void CSceneRenderPass::EndRenderPass(CDeviceCommandListRef RESTRICT_REFERENCE commandList, bool bNearest) const
{
	// Note: Function has to be threadsafe since it can be called from several worker threads

	CDeviceGraphicsCommandInterface* pCommandInterface = commandList.GetGraphicsInterface();
	pCommandInterface->EndRenderPass(*m_pRenderPass);

#if (CRY_RENDERER_DIRECT3D < 120)
	pCommandInterface->SetDepthBias(0.0f, 0.0f, 0.0f);
#endif

	if (m_passFlags & ePassFlags_UseVrProjectionState)
	{
		gcpRendD3D.GetVrProjectionManager()->RestoreState(commandList);
	}
}

void CSceneRenderPass::BeginExecution(CGraphicsPipeline& activeGraphicsPipeline)
{
	assert(s_recursionCounter == 0);
	s_recursionCounter += 1;

	m_numRenderItemGroups = 0;

	if (activeGraphicsPipeline.GetRenderPassScheduler().IsActive())
		activeGraphicsPipeline.GetRenderPassScheduler().AddPass(this);
}

void CSceneRenderPass::EndExecution()
{
	s_recursionCounter -= 1;
}

inline void DebugDrawRenderResolve(const std::vector<TRect_tpl<uint16>>& ns, const std::size_t count)
{
	// Color table for debug draw
	const auto a = 64;
	ColorB colors[] = {
		ColorB(255, 0,   0,   a), // Red
		ColorB(0,   255, 0,   a), // Green
		ColorB(0,   0,   255, a), // Blue
		ColorB(255, 255, 0,   a), // Yellow
		ColorB(255, 0,   255, a), // Magenta
		ColorB(0,   255, 255, a), // Cyan
		ColorB(128, 0,   0,   a),
		ColorB(0,   128, 0,   a),
		ColorB(0,   0,   128, a),
		ColorB(128, 128, 0,   a),
		ColorB(128, 0,   128, a),
		ColorB(0,   128, 128, a),
		ColorB(192, 192, 192, a),
		ColorB(128, 128, 128, a),
		ColorB(153, 153, 255, a),
		ColorB(153, 51,  102, a),
		ColorB(255, 255, 204, a),
		ColorB(204, 255, 255, a),
		ColorB(102, 0,   102, a),
		ColorB(255, 128, 128, a),
		ColorB(0,   102, 204, a),
		ColorB(204, 204, 255, a),
		ColorB(0,   0,   128, a),
		ColorB(255, 0,   255, a),
		ColorB(255, 255, 0,   a),
		ColorB(0,   255, 255, a),
		ColorB(128, 0,   128, a),
		ColorB(128, 0,   0,   a),
		ColorB(0,   128, 128, a),
		ColorB(0,   0,   255, a),
		ColorB(0,   204, 255, a),
		ColorB(204, 255, 255, a),
		ColorB(204, 255, 204, a),
		ColorB(255, 255, 153, a),
		ColorB(153, 204, 255, a),
		ColorB(255, 153, 204, a),
		ColorB(204, 153, 255, a),
		ColorB(255, 204, 153, a),
		ColorB(51,  102, 255, a),
		ColorB(51,  204, 204, a),
		ColorB(153, 204, 0,   a),
		ColorB(255, 204, 0,   a),
		ColorB(255, 153, 0,   a),
		ColorB(255, 102, 0,   a),
		ColorB(102, 102, 153, a),
		ColorB(150, 150, 150, a),
		ColorB(0,   51,  102, a),
		ColorB(51,  153, 102, a),
		ColorB(0,   51,  0,   a),
		ColorB(51,  51,  0,   a),
		ColorB(153, 51,  0,   a),
		ColorB(153, 51,  102, a),
		ColorB(51,  51,  153, a),
		ColorB(51,  51,  51,  a),
		ColorB(0,   0,   0,   a),
		ColorB(255, 255, 255, a), };

	if (CRendererCVars::CV_r_RefractionPartialResolvesDebug == 2)
	{
		const auto oldAuxFlags = IRenderAuxGeom::GetAux()->GetRenderFlags();
		IRenderAuxGeom::GetAux()->SetRenderFlags(e_AlphaBlended | e_DepthWriteOff | e_DepthTestOff | e_Mode2D);

		for (const auto& n : ns)
		{
			// Render resolve screen-space bounding box
			Vec3 v0 = { static_cast<float>(n.Min.x), static_cast<float>(n.Min.y), .0f };
			Vec3 v1 = { static_cast<float>(n.Max.x), static_cast<float>(n.Min.y), .0f };
			Vec3 v2 = { static_cast<float>(n.Max.x), static_cast<float>(n.Max.y), .0f };
			Vec3 v3 = { static_cast<float>(n.Min.x), static_cast<float>(n.Max.y), .0f };

			const auto c = colors[count % (sizeof(colors) / sizeof(colors[0]))];
			IRenderAuxGeom::GetAux()->DrawQuad(v0, c, v1, c, v2, c, v3, c);
		}

		IRenderAuxGeom::GetAux()->SetRenderFlags(oldAuxFlags);
	}

#if defined(ENABLE_PROFILING_CODE)
	// Write statstics
	SRenderStatistics& stats = SRenderStatistics::Write();
	++stats.m_refractionPartialResolveCount;
	for (const auto& n : ns)
		stats.m_refractionPartialResolvePixelCount += std::max(0, (n.Max.x - n.Min.x) * (n.Max.y - n.Min.y));
#endif
}

const char* RenderListNames[] =
{
	"INVALID",

	"GENERAL",
	"TERRAIN",
	"SHADOWS",
	"DECAL",
	"WATER_VOLUMES",
	"GENERAL",
	"GENERAL",
	"NEAREST",
	"WATER",
	"AFTER_HDRPOSTPROCESS",
	"AFTER_POSTPROCESS",
	"SHADOW_PASS",
	"HALFRES_PARTICLES",
	"PARTICLES_THICKNESS",
	"LENSOPTICS",
	"VOXELIZE",
	"EYE",
	"FOG_VOLUME",
	"NEAREST",
	"GENERAL",
	"NEAREST",
	"DEBUG_HELPER",
	"SKY",

	"PREPROCESS",
	"NON-NEAREST",
	"NEAREST",
	"CUSTOM",
	"HIGHLIGHT",
};

void CSceneRenderPass::DrawRenderItems(CRenderView* pRenderView, ERenderListID renderList, int listStart, int listEnd)
{
	CRY_ASSERT(s_recursionCounter == 1);

	// Completely skip filling of the command list.
	if (CRenderer::CV_r_NoDraw == 2)
		return;

	// Skip nearest objects lists
	const bool bNearest =
		(renderList == EFSLIST_ZPREPASS_NEAREST) ||
		(renderList == EFSLIST_NEAREST_OBJECTS) ||
		(renderList == EFSLIST_FORWARD_OPAQUE_NEAREST) ||
		(renderList == EFSLIST_TRANSP_NEAREST);
	if (bNearest && (CRenderer::CV_r_nodrawnear == 1) && (m_passFlags & CSceneRenderPass::ePassFlags_RenderNearest))
		return;

	// Skip processing if all elements are filtered away
	if (!pRenderView->HasRenderItems(renderList, m_batchIncludeFilter | FB_COMPILED_OBJECT))
		return;

	// Produce valid range of renderable items and skip processing when there are none
	listEnd   = std::min<int>(listEnd, pRenderView->GetRenderItems(renderList).size());
	listStart = std::min<int>(listStart, listEnd);

	if (listStart >= listEnd)
		return;

	CryStackStringT<char, 80> label; label.Format("%s%s (%s)", m_batchExcludeFilter & FB_ZPREPASS ? "Z " : "", GetLabel(), RenderListNames[pRenderView->GetRecordingRenderList(renderList)]);
	SGraphicsPipelinePassContext passContext(pRenderView, this, m_technique, m_batchIncludeFilter, m_batchExcludeFilter);
	passContext.stageID = m_stageID;
	passContext.passID = m_passID;
	passContext.renderNearest = bNearest && (m_passFlags & CSceneRenderPass::ePassFlags_RenderNearest);
	passContext.renderListId = renderList;
#if defined(ENABLE_PROFILING_CODE)
	passContext.recordListId = pRenderView->GetRecordingRenderList(renderList); // use pseudo-list for stats recording only
#endif
#if defined(DO_RENDERSTATS)
	if (gcpRendD3D->CV_r_stats == 6 || gcpRendD3D->m_pDebugRenderNode || gcpRendD3D->m_bCollectDrawCallsInfoPerNode)
		passContext.pDrawCallInfoPerNode = gcpRendD3D->GetDrawCallInfoPerNode();
	if (gcpRendD3D->m_bCollectDrawCallsInfo)
		passContext.pDrawCallInfoPerMesh = gcpRendD3D->GetDrawCallInfoPerMesh();
#endif

	passContext.groupLabel = label;
	passContext.groupIndex = m_numRenderItemGroups++;
#if defined(ENABLE_SIMPLE_GPU_TIMERS)
	passContext.profilerSectionIndex = gcpRendD3D->m_pPipelineProfiler->InsertMultithreadedSection(passContext.groupLabel.c_str());
#endif

	const bool bTransparent = (renderList == EFSLIST_TRANSP_BW || renderList == EFSLIST_TRANSP_AW || renderList == EFSLIST_TRANSP_NEAREST);
	if (!bTransparent)
		DrawOpaqueRenderItems(passContext, pRenderView, renderList, listStart, listEnd);
	else
		DrawTransparentRenderItems(passContext, pRenderView, renderList, listStart, listEnd);
}

void CSceneRenderPass::DrawOpaqueRenderItems(SGraphicsPipelinePassContext& passContext, CRenderView* pRenderView, ERenderListID renderList, int listStart, int listEnd)
{
	passContext.rendItems = { listStart, listEnd };

	if (pRenderView->GetGraphicsPipeline()->GetRenderPassScheduler().IsActive())
	{
		m_passContexts.emplace_back(std::move(passContext));
		return;
	}

	if (!CRenderer::CV_r_NoDraw)
		pRenderView->DrawCompiledRenderItems(passContext);
}

void CSceneRenderPass::DrawTransparentRenderItems(SGraphicsPipelinePassContext& passContext, CRenderView* pRenderView, ERenderListID renderList, int listStart, int listEnd)
{
	std::vector<SGraphicsPipelinePassContext> passes;

	// Wait for the transparent items' refractive passes sort job (if any)
	pRenderView->WaitForOptimizeTransparentRenderItemsResolvesJob();

	if (!pRenderView->HasResolveForList(renderList))
	{
		passContext.rendItems = { listStart, listEnd };

		passes.emplace_back(std::move(passContext));
	}
	else
	{
		if (CRendererCVars::CV_r_RefractionPartialResolveMode == 0)
		{
			// Fast static mode
			// Single fullscreen Resolve the full screen once, before submitting transparent items.

			const auto& vp = pRenderView->GetViewport();

			SGraphicsPipelinePassContext resolvePass = { GraphicsPipelinePassType::resolve, pRenderView, this };
			resolvePass.stageID    = passContext.stageID;
			resolvePass.passID     = passContext.passID;
			resolvePass.groupLabel = passContext.groupLabel;
			resolvePass.groupIndex = passContext.groupIndex;
			resolvePass.resolveScreenBounds.push_back(TRect_tpl<uint16>{ static_cast<uint16>(vp.x), static_cast<uint16>(vp.y), static_cast<uint16>(vp.width), static_cast<uint16>(vp.height) });
#if defined(ENABLE_SIMPLE_GPU_TIMERS)
			resolvePass.profilerSectionIndex = passContext.profilerSectionIndex;
#endif

			if (CRendererCVars::CV_r_RefractionPartialResolvesDebug)
				DebugDrawRenderResolve(resolvePass.resolveScreenBounds, 0);

			passContext.rendItems = { listStart, listEnd };

			passes.emplace_back(std::move(resolvePass));
			passes.emplace_back(std::move(passContext));
		}
		else
		{
			// Segments were percomputed in job "OptimizeTransparentRenderItemsResolvesJob"
			const auto& segments = static_cast<const CRenderView*>(pRenderView)->GetTransparentSegments(renderList);

			std::size_t count = 0;
			for (const auto& s : segments)
			{
				if (s.rendItems.IsEmpty())
					continue;

				if (s.resolveRects.size())
				{
					SGraphicsPipelinePassContext resolvePass = { GraphicsPipelinePassType::resolve, pRenderView, this };
					resolvePass.stageID    = passContext.stageID;
					resolvePass.passID     = passContext.passID;
					resolvePass.groupLabel = passContext.groupLabel;
					resolvePass.groupIndex = passContext.groupIndex;
					resolvePass.resolveScreenBounds = s.resolveRects;
#if defined(ENABLE_SIMPLE_GPU_TIMERS)
					resolvePass.profilerSectionIndex = passContext.profilerSectionIndex;
#endif

					if (CRendererCVars::CV_r_RefractionPartialResolvesDebug)
						DebugDrawRenderResolve(s.resolveRects, count++);

					passes.emplace_back(std::move(resolvePass));
				}

				passContext.rendItems = s.rendItems;

				passes.push_back(passContext);

			}
		}
	}

	if (pRenderView->GetGraphicsPipeline()->GetRenderPassScheduler().IsActive())
	{
		m_passContexts.reserve(m_passContexts.size() + passes.size());
		std::copy(passes.begin(), passes.end(), std::back_inserter(m_passContexts));

		return;
	}

	if (!CRenderer::CV_r_NoDraw)
	{
		for (const auto& pass : passes)
			pRenderView->DrawCompiledRenderItems(pass);
	}
}

void CSceneRenderPass::Execute()
{
	for (const auto& passContext : m_passContexts)
		passContext.pRenderView->DrawCompiledRenderItems(passContext);
}

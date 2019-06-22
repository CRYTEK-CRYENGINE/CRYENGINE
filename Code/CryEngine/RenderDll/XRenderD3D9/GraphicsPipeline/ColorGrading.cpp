// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ColorGrading.h"

#include "Common/PostProcess/PostEffects.h"
#include <Cry3DEngine/IColorGradingCtrl.h>

const ETEX_Format COLORCHART_FORMAT = eTF_R8G8B8A8;

CColorGradingStage::CColorGradingStage(CGraphicsPipeline& graphicsPipeline)
	: CGraphicsPipelineStage(graphicsPipeline)
	, m_slicesVertexBuffer(~0u)
	, m_colorGradingPrimitive(CRenderPrimitive::eFlags_ReflectShaderConstants)
{
	// Preallocate 2 merge primitives (i.e. up to 8 blending layers)
	m_mergeChartsPrimitives.emplace_back(CRenderPrimitive::eFlags_ReflectShaderConstants);
	m_mergeChartsPrimitives.emplace_back(CRenderPrimitive::eFlags_ReflectShaderConstants);
}

void CColorGradingStage::Init()
{
	// Load identity color chart
	ITexture* pTexture = gEnv->pRenderer->EF_LoadTexture(IColorGradingCtrl::s_szDefaultIdentityLut, FT_NOMIPS | FT_DONT_STREAM | FT_STATE_CLAMP);
	if (pTexture && pTexture->GetWidth() == IColorGradingCtrl::s_colorChartWidth && pTexture->GetHeight() == IColorGradingCtrl::s_colorChartHeight)
	{
		m_pChartIdentity.Assign_NoAddRef(CTexture::GetByID(pTexture->GetTextureID()));
	}
	else
	{
		static bool bPrint = true;
		if (bPrint)
			iLog->LogError("Failed to initialize Color Grading: Default color chart is missing");
		bPrint = false;
	}

	// Allocate render targets
	CTexture* pRT = CTexture::GetOrCreateRenderTarget("ColorGradingMergedLayer", IColorGradingCtrl::s_colorChartWidth, IColorGradingCtrl::s_colorChartHeight, Clr_Empty, eTT_2D, FT_NOMIPS | FT_DONT_STREAM | FT_STATE_CLAMP, COLORCHART_FORMAT);
	if (CTexture::IsTextureExist(pRT))
	{
		m_pMergedLayer.Assign_NoAddRef(pRT);
	}

	pRT = CTexture::GetOrCreateRenderTarget("ColorGradingFinalMerged", IColorGradingCtrl::s_colorChartWidth, IColorGradingCtrl::s_colorChartHeight, Clr_Empty, eTT_2D, FT_NOMIPS | FT_DONT_STREAM | FT_STATE_CLAMP, COLORCHART_FORMAT);
	if (CTexture::IsTextureExist(pRT))
	{
		m_pFinalMergedTexture.Assign_NoAddRef(pRT);
	}

	// Prepare passes
	{
		D3DViewPort viewport;
		viewport.TopLeftX = viewport.TopLeftY = 0.0f;
		viewport.Width = (float)m_pMergedLayer->GetWidth();
		viewport.Height = (float)m_pMergedLayer->GetHeight();
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		m_mergeChartsPass.SetRenderTarget(0, m_pMergedLayer);
		m_mergeChartsPass.SetViewport(viewport);

		m_colorGradingPass.SetRenderTarget(0, m_pFinalMergedTexture);
		m_colorGradingPass.SetViewport(viewport);
	}

	// prepare vertex buffer
	{
		const int chartSize = IColorGradingCtrl::s_colorChartSize;
		stl::aligned_vector<SVF_P3F_C4B_T2F, CRY_PLATFORM_ALIGNMENT> vecSlicesData(6 * chartSize);

		const float fQuadSize = 1.0f / chartSize;
		const float fTexCoordSize = 1.f / chartSize;
		for (uint32 iQuad = 0; iQuad < chartSize; ++iQuad)
		{
			const float fQuadShift = (float)iQuad / chartSize;
			const float fBlue = (float)iQuad / (chartSize - 1.f);

			vecSlicesData[iQuad * 6 + 0].xyz = Vec3(fQuadShift + fQuadSize, 1.f, 0.f);
			vecSlicesData[iQuad * 6 + 0].st = Vec2(fQuadShift + fTexCoordSize, 1.f);
			vecSlicesData[iQuad * 6 + 0].color.dcolor = ColorF(1.f, 1.f, fBlue).pack_argb8888();
			vecSlicesData[iQuad * 6 + 1].xyz = Vec3(fQuadShift + fQuadSize, 0.f, 0.f);
			vecSlicesData[iQuad * 6 + 1].st = Vec2(fQuadShift + fTexCoordSize, 0.f);
			vecSlicesData[iQuad * 6 + 1].color.dcolor = ColorF(1.f, 0.f, fBlue).pack_argb8888();
			vecSlicesData[iQuad * 6 + 2].xyz = Vec3(fQuadShift + 0.f, 1.f, 0.f);
			vecSlicesData[iQuad * 6 + 2].st = Vec2(fQuadShift + 0.f, 1.f);
			vecSlicesData[iQuad * 6 + 2].color.dcolor = ColorF(0.f, 1.f, fBlue).pack_argb8888();

			vecSlicesData[iQuad * 6 + 3].xyz = Vec3(fQuadShift + 0.f, 1.f, 0.f);
			vecSlicesData[iQuad * 6 + 3].st = Vec2(fQuadShift + 0.f, 1.f);
			vecSlicesData[iQuad * 6 + 3].color.dcolor = ColorF(0.f, 1.f, fBlue).pack_argb8888();
			vecSlicesData[iQuad * 6 + 4].xyz = Vec3(fQuadShift + fQuadSize, 0.f, 0.f);
			vecSlicesData[iQuad * 6 + 4].st = Vec2(fQuadShift + fTexCoordSize, 0.f);
			vecSlicesData[iQuad * 6 + 4].color.dcolor = ColorF(1.f, 0.f, fBlue).pack_argb8888();
			vecSlicesData[iQuad * 6 + 5].xyz = Vec3(fQuadShift + 0.f, 0.f, 0.f);
			vecSlicesData[iQuad * 6 + 5].st = Vec2(fQuadShift + 0.f, 0.f);
			vecSlicesData[iQuad * 6 + 5].color.dcolor = ColorF(0.f, 0.f, fBlue).pack_argb8888();
		}

		m_slicesVertexBuffer = gcpRendD3D->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_STATIC, vecSlicesData.size() * sizeof(vecSlicesData[0]));
		gcpRendD3D->m_DevBufMan.UpdateBuffer(m_slicesVertexBuffer, vecSlicesData);
	}
}

void CColorGradingStage::PreparePrimitives(const SColorGradingMergeParams& mergeParams)
{
	if (!m_pMergedLayer || !m_pFinalMergedTexture || m_slicesVertexBuffer == ~0u)
		return;

	// init passes
	m_mergeChartsPass.BeginAddingPrimitives();
	m_colorGradingPass.BeginAddingPrimitives();

	// prepare chart blending primitives
	{
		const auto& chartsToUse = m_pRenderView->GetRendererData().colorGrading.charts;
		const int numLayers = chartsToUse.size();
		for (size_t i = m_mergeChartsPrimitives.size(), end = numLayers / 4 + 1; i < end; ++i)
		{
			m_mergeChartsPrimitives.emplace_back(CRenderPrimitive::eFlags_ReflectShaderConstants);
		}

		int numMergePasses = 0;
		for (size_t curLayer = 0; curLayer < numLayers;)
		{
			CTexture* layerTex[4] = { nullptr, nullptr, nullptr, nullptr };
			Vec4 layerBlendAmount(0, 0, 0, 0);

			int numLayersPerPass = 0;

			// group up to four layers
			for (; curLayer < numLayers && numLayersPerPass < 4; ++curLayer)
			{
				auto& layer = chartsToUse[curLayer];
				if (layer.blendAmount > 0.001f)
				{
					layerTex[numLayersPerPass] = CTexture::GetByID(layer.texID);
					layerBlendAmount[numLayersPerPass] = layer.blendAmount;

					++numLayersPerPass;
				}
			}

			if (numLayersPerPass)
			{
				static CCryNameTSCRC techName("MergeColorCharts");

				uint64 rtFlags = 0;
				rtFlags |= ((numLayersPerPass - 1) & 1) ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0;
				rtFlags |= ((numLayersPerPass - 1) & 2) ? g_HWSR_MaskBit[HWSR_SAMPLE1] : 0;

				CRenderPrimitive& prim = m_mergeChartsPrimitives[numMergePasses];
				prim.SetTechnique(CShaderMan::s_shPostEffectsGame, techName, rtFlags);
				prim.SetFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
				prim.SetRenderState(GS_NODEPTHTEST | (numMergePasses ? GS_BLSRC_ONE | GS_BLDST_ONE : 0));
				prim.SetCustomVertexStream(m_slicesVertexBuffer, EDefaultInputLayouts::P3F_C4B_T2F, sizeof(SVF_P3F_C4B_T2F));
				prim.SetDrawInfo(eptTriangleList, 0, 0, 6 * IColorGradingCtrl::s_colorChartSize);
				for (int i = 0; i < numLayersPerPass; ++i)
				{
					prim.SetTexture(i, layerTex[i]);
				}
				prim.Compile(m_mergeChartsPass);

				prim.GetConstantManager().BeginNamedConstantUpdate();

				static CCryNameR nameLayerBlendAmount("LayerBlendAmount");
				static CCryNameR nameLayerSize("LayerSize");

				prim.GetConstantManager().SetNamedConstant(nameLayerBlendAmount, layerBlendAmount, eHWSC_Pixel);
				Vec4 layerSize((float)m_pMergedLayer->GetWidth(), (float)m_pMergedLayer->GetHeight(), 0, 0);
				prim.GetConstantManager().SetNamedConstant(nameLayerSize, layerSize, eHWSC_Pixel);

				prim.GetConstantManager().EndNamedConstantUpdate(&m_mergeChartsPass.GetViewport(), m_graphicsPipeline.GetCurrentRenderView());

				m_mergeChartsPass.AddPrimitive(&prim);
				++numMergePasses;
			}
		}

		m_pChartToUse = numMergePasses ? m_pMergedLayer : m_pChartIdentity;
	}

	// Apply color grading constants to merged texture
	if (m_pChartToUse)
	{
		static CCryNameTSCRC techName("CombineColorGradingWithColorChart");
		uint64 rtFlags = mergeParams.nFlagsShaderRT & ~g_HWSR_MaskBit[HWSR_SAMPLE1];

		m_colorGradingPrimitive.SetFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		m_colorGradingPrimitive.SetTechnique(CShaderMan::s_shPostEffectsGame, techName, rtFlags);
		m_colorGradingPrimitive.SetRenderState(GS_NODEPTHTEST);
		m_colorGradingPrimitive.SetSampler(0, EDefaultSamplerStates::LinearClamp);
		m_colorGradingPrimitive.SetPrimitiveType(CRenderPrimitive::ePrim_Custom);
		m_colorGradingPrimitive.SetCustomVertexStream(m_slicesVertexBuffer, EDefaultInputLayouts::P3F_C4B_T2F, sizeof(SVF_P3F_C4B_T2F));
		m_colorGradingPrimitive.SetDrawInfo(eptTriangleList, 0, 0, 6 * IColorGradingCtrl::s_colorChartSize);
		m_colorGradingPrimitive.SetTexture(0, m_pChartToUse);
		m_colorGradingPrimitive.Compile(m_colorGradingPass);

		auto& constantManager = m_colorGradingPrimitive.GetConstantManager();

		constantManager.BeginNamedConstantUpdate();

		static CCryNameR pParamName0("ColorGradingParams0");
		static CCryNameR pParamName1("ColorGradingParams1");
		static CCryNameR pParamName2("ColorGradingParams2");
		static CCryNameR pParamName3("ColorGradingParams3");
		static CCryNameR pParamName4("ColorGradingParams4");
		static CCryNameR pParamMatrix("mColorGradingMatrix");

		constantManager.SetNamedConstant(pParamName0, mergeParams.pLevels[0], eHWSC_Pixel);
		constantManager.SetNamedConstant(pParamName1, mergeParams.pLevels[1], eHWSC_Pixel);
		constantManager.SetNamedConstant(pParamName2, mergeParams.pFilterColor, eHWSC_Pixel);
		constantManager.SetNamedConstant(pParamName3, mergeParams.pSelectiveColor[0], eHWSC_Pixel);
		constantManager.SetNamedConstant(pParamName4, mergeParams.pSelectiveColor[1], eHWSC_Pixel);
		constantManager.SetNamedConstantArray(pParamMatrix, mergeParams.pColorMatrix, 3, eHWSC_Pixel);

		constantManager.EndNamedConstantUpdate(&m_colorGradingPass.GetViewport(), m_graphicsPipeline.GetCurrentRenderView());

		m_colorGradingPass.AddPrimitive(&m_colorGradingPrimitive);

		m_pChartToUse = m_pFinalMergedTexture;
	}
}

void CColorGradingStage::Execute()
{
	FUNCTION_PROFILER_RENDERER();
	PROFILE_LABEL_SCOPE("COLORGRADING");

	m_pChartToUse = nullptr;

	CColorGrading* pColorGrading = (CColorGrading*)PostEffectMgr()->GetEffect(EPostEffectID::ColorGrading);
	if (!pColorGrading)
	{
		return;
	}

	SColorGradingMergeParams mergeParams;
	pColorGrading->UpdateParams(mergeParams);

	if (gEnv->IsCutscenePlaying() || (RenderView()->GetFrameId() % max(1, CRenderer::CV_r_ColorgradingChartsCache)) == 0 || AreRenderPassesDirty())
	{
		PreparePrimitives(mergeParams);
	}

	m_mergeChartsPass.Execute();
	m_colorGradingPass.Execute();
}

bool CColorGradingStage::AreRenderPassesDirty() const
{
	if (m_colorGradingPrimitive.IsDirty())
	{
		return true;
	}

	for (auto& prim : m_mergeChartsPrimitives)
	{
		if (prim.IsDirty())
		{
			return true;
		}
	}

	return false;
}

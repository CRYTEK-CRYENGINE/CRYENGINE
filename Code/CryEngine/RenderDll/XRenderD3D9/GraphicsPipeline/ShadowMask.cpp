// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ShadowMask.h"
#include "DriverD3D.h"

#include "../Common/PostProcess/PostProcessUtils.h"
#include "../Common/Include_HLSL_CPP_Shared.h"

#if defined(FEATURE_SVO_GI)
#include "D3D_SVO.h"
#endif

namespace ShadowMaskInternal
{

class CSunShadows
{
	struct STypedConstants
	{
		Matrix44 unitMeshTransform;
		Vec4     camPos;
		Matrix44 screenToShadowBasis; // basis vectors in rows, scales in row3
		Matrix44 noiseProjection;
		Vec4     blendTcNormalize;
		Vec4     oneDivFarDist;
		Vec4     depthTestBias;
		Vec4     irreg_kernel_2d[8];
		Vec4     kernelRadius;
		Vec4     adaption;
		Vec4     invShadowMapSize;
		Vec4     shadowFadingDist;
		Matrix44 blendTexGen;
		Vec4     blendInfo;
		Vec4     lightPos;
	};

	struct SCloudShadowConstants
	{
		Vec4 params;
		Vec4 animParams;
		Vec4 screenspaceShadowsParams;
	};

	struct SCascadePrimitiveContext
	{
		SCascadePrimitiveContext(CRenderView* pRenderView, bool renderScreenspaceShadows, bool texelRelativeBias, bool extendLastCachedCascade, uint64 shaderRtFlags)
			: frustums(pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_SunDynamic))
			, rtFlags(shaderRtFlags)
			, renderScreenspaceShadows(renderScreenspaceShadows)
			, useTexelRelativeBias(texelRelativeBias)
			, pMainRenderView(pRenderView)
		{
			if (extendLastCachedCascade)
			{
				auto& cachedFrustums = pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_SunCached);
				if (!cachedFrustums.empty())
				{
					frustums.push_back(cachedFrustums.back());
				}
			}
		}

		CRenderView*                    pMainRenderView;
		CRenderView::ShadowFrustumsPtr  frustums;
		uint64                          rtFlags;
		bool                            renderScreenspaceShadows;
		bool                            useTexelRelativeBias;
	};

	struct SCustomPrimitiveContext
	{
		SCustomPrimitiveContext(ShadowMapFrustum* pFrustum, uint64 rtFlags)
			: pFrustum(pFrustum)
			, rtFlags(rtFlags)
		{}

		ShadowMapFrustum* pFrustum;
		uint64            rtFlags;
	};

	static const int MaxCustomFrustums = 64;

public:
	CSunShadows(const CShadowMaskStage& stage);
	~CSunShadows();

	void InitPrimitives();
	void ResetPrimitives();
	int  PreparePrimitives(CPrimitiveRenderPass& sliceGenPass, int& firstUnusedStencilValue, bool renderCloudShadows, bool renderScreenSpaceShadows, bool renderSvoShadows,
	                       bool texelRelativeBias, bool extendLastCachedCascade, CPrimitiveRenderPass* pDebugCascadesPass, CRenderView* pRenderView, uint64 qualityFlags);

	void OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater);

private:
	void PrepareCascadePrimitivesNoBlending(CPrimitiveRenderPass& sliceGenPass, int& firstUnusedStencilValue, const SCascadePrimitiveContext& context);
	void PrepareCascadePrimitivesWithBlending(CPrimitiveRenderPass& sliceGenPass, int& firstUnusedStencilValue, const SCascadePrimitiveContext& context);
	void PrepareCascadePrimitivesWithPrepass(CPrimitiveRenderPass& sliceGenPass, CPrimitiveRenderPass* pDebugCascadesPass, int& firstUnusedStencilValue, const SCascadePrimitiveContext& context);

	void PreparePerObjectPrimitives(CRenderPrimitive& primStencil0, CRenderPrimitive& primStencil1, CRenderPrimitive& primSampling, int& firstUnusedStencilValue, const SCustomPrimitiveContext& context);
	void PrepareNearestPrimitive(CRenderPrimitive& primitive, ShadowMapFrustum* pFrustum, uint64 rtFlags);
	void PrepareCustomShadowsPrimitive(CRenderPrimitive& primitive, _smart_ptr<CTexture>& pCloudShadowTex) const;
	bool PrepareDebugPrimitive(CPrimitiveRenderPass& debugPass, CRenderPrimitive& primitive, const ShadowMapFrustum* pFrustum, int stencilRef) const;

	void PrepareStencilPassConstants(CRenderPrimitive& primitive, ShadowMapFrustum* pFrustum) const;
	void PrepareConstantBuffers(CRenderPrimitive& primitive, ShadowMapFrustum* pFrustum, ShadowMapFrustum* pVolumeProvider, bool bScaledVolume) const;

	_smart_ptr<CTexture>                               m_pCloudShadowTex;

	std::array<CRenderPrimitive,  MAX_GSM_LODS_NUM+1>     cachedStencilPrimitives;
	std::array<CRenderPrimitive, (MAX_GSM_LODS_NUM+1)* 3> cachedSamplingPrimitives;
	std::array<CRenderPrimitive,  MAX_GSM_LODS_NUM+1>     cachedDebugPrimitives;
	std::array<CRenderPrimitive,  MaxCustomFrustums  * 3> cachedCustomPrimitives;

	int                     customPrimitiveCount;
	CRenderPrimitive        customShadowPrimitive;
	CRenderPrimitive        nearestShadowPrimitive;

	buffer_handle_t         m_nearestFullscreenTri;
	float                   m_nearestZ;

	const CShadowMaskStage& shadowMaskStage;
};

class CLocalLightShadows
{
	struct STypedConstants
	{
		Matrix44 unitMeshTransform;
		Vec4     lightVolumeSphereAdjust;

		Matrix44 lightShadowProj;
		Vec4     params;

		Vec4     irreg_kernel_2d[8];
		Vec4     vLightPos;
	};

	struct SLocalLightPrimitives
	{
		CRenderPrimitive stencilBackfaces;
		CRenderPrimitive stencilFrontfaces;
		CRenderPrimitive sampling;

		void Reset()
		{
			stencilBackfaces.Reset();
			stencilFrontfaces.Reset();
			sampling.Reset();
		}

		void SetConstantBuffer(EConstantBufferShaderSlot shaderSlot, CConstantBuffer* pBuffer, EShaderStage shaderStages)
		{
			stencilBackfaces.SetInlineConstantBuffer(shaderSlot, pBuffer, shaderStages);
			stencilFrontfaces.SetInlineConstantBuffer(shaderSlot, pBuffer, shaderStages);
			sampling.SetInlineConstantBuffer(shaderSlot, pBuffer, shaderStages);
		}
	};

public:
	CLocalLightShadows(const CShadowMaskStage& stage);

	void InitPrimitives();
	void ResetPrimitives();
	int  PreparePrimitives(std::vector<CPrimitiveRenderPass>& maskGenPasses, int& firstUnusedStencilValue, bool usingScreenSpaceShadows, CRenderView* pRenderView, uint64 qualityFlags);
	
	void OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater);

private:
	int                    PreparePrimitivesForLight(const CRenderView* pRenderView, CPrimitiveRenderPass& sliceGenPass, int& firstUnusedStencilValue, bool usingScreenSpaceShadows, SShadowFrustumToRender* pFrustumToRender, uint64 qualityFlags, int& volumePrimitiveCount, int& quadPrimitiveCount);
	void                   PrepareConstantBuffersForPrimitives(SLocalLightPrimitives& primitives, SShadowFrustumToRender* pFrustumToRender, int side, bool bVolumePrimitive) const;

	std::array<SLocalLightPrimitives, MAX_SHADOWMAP_FRUSTUMS> volumePrimitives;
	std::array<SLocalLightPrimitives, MAX_SHADOWMAP_FRUSTUMS> quadPrimitives;

	const CShadowMaskStage& shadowMaskStage;
};

}

CShadowMaskStage::CShadowMaskStage()
	: m_pSunShadows(stl::make_unique<ShadowMaskInternal::CSunShadows>(*this))
	, m_pLocalLightShadows(stl::make_unique<ShadowMaskInternal::CLocalLightShadows>(*this))
	, m_pShadowMaskRT(nullptr)
	, m_sunShadowPrimitives(0)
	, m_localLightPrimitives(0)
	, m_viewInfoCount(0)
{}

void CShadowMaskStage::Init()
{
	// per view constant buffer
	m_pPerViewConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(HLSL_PerViewGlobalConstantBuffer));
}

void CShadowMaskStage::Prepare()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	CRenderView* pRenderView = RenderView();
	CTexture* pZTexture = pRenderView->GetDepthTarget();

	const uint64 rtFlagsByQuality[] =
	{
		0,                                                                                             // eSQ_Low
		g_HWSR_MaskBit[HWSR_QUALITY],                                                                  // eSQ_Medium
		g_HWSR_MaskBit[HWSR_QUALITY1],                                                                 // eSQ_High
		g_HWSR_MaskBit[HWSR_QUALITY] | g_HWSR_MaskBit[HWSR_QUALITY1] | g_HWSR_MaskBit[HWSR_SAMPLE1],   // eSQ_VeryHigh
	};

	static ICVar* pDebugCascadesCVar           = gEnv->pConsole->GetCVar("e_ShadowsCascadesDebug");
	static ICVar* pAutoBiasCVar                = gEnv->pConsole->GetCVar("e_ShadowsAutoBias");
	static ICVar* pExtendLastCachedCascadeCVar = gEnv->pConsole->GetCVar("e_ShadowsCacheExtendLastCascade");
	const bool debugCascades                   = pDebugCascadesCVar && pDebugCascadesCVar->GetIVal() > 0;
	const bool usingCloudShadows               = rd->GetCloudShadowsEnabled() || rd->m_bVolumetricCloudsEnabled;
	const bool usingScreenSpaceShadows         = CRendererCVars::CV_r_ShadowsScreenSpace != 0;
	const bool texelRelativeBias               = pAutoBiasCVar && pAutoBiasCVar->GetFVal() > 0;
	const bool extendLastCachedCascade         = pExtendLastCachedCascadeCVar && pExtendLastCachedCascadeCVar->GetIVal() > 0;
#if defined(FEATURE_SVO_GI)
	CSvoRenderer* pSR                          = CSvoRenderer::GetInstance();
	const bool usingSvoShadows                 = pSR && pSR->GetTracedSunShadowsRT();
#else
	const bool usingSvoShadows                 = false;
#endif

	// Set rendertarget
	this->m_pShadowMaskRT = CRendererResources::s_ptexShadowMask;

	// Initialize passes
	D3DViewPort viewport;
	viewport.TopLeftX = viewport.TopLeftY = 0.0f;
	viewport.Width = (float)m_pShadowMaskRT->GetWidth();
	viewport.Height = (float)m_pShadowMaskRT->GetHeight();
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	const auto slices = m_pShadowMaskRT->StreamGetNumSlices();
	m_maskGenPasses.reserve(slices);
	for (int shadowMaskSlice = 0; shadowMaskSlice < slices; ++shadowMaskSlice)
	{
		if (m_maskGenPasses.size() <= shadowMaskSlice)
			m_maskGenPasses.emplace_back();

		const SResourceView curSliceDesc = SResourceView::RenderTargetView(DeviceFormats::ConvertFromTexFormat(m_pShadowMaskRT->GetDstFormat()), shadowMaskSlice, 1);
		const ResourceViewHandle curSliceView = m_pShadowMaskRT->GetDevTexture()->GetOrCreateResourceViewHandle(curSliceDesc);

		auto& sliceGenPass = m_maskGenPasses[shadowMaskSlice];
		sliceGenPass.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);
		sliceGenPass.SetTargetClearMask(shadowMaskSlice == 0 ? CPrimitiveRenderPass::eClear_Stencil : CPrimitiveRenderPass::eClear_None);
		sliceGenPass.SetRenderTarget(0, m_pShadowMaskRT, curSliceView);
		sliceGenPass.SetDepthTarget(pZTexture);
		sliceGenPass.SetViewport(viewport);
		sliceGenPass.BeginAddingPrimitives();
	}

	m_debugCascadesPass.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);
	m_debugCascadesPass.SetRenderTarget(0, CRendererResources::s_ptexSceneDiffuse);
	m_debugCascadesPass.SetDepthTarget(pZTexture);
	m_debugCascadesPass.SetViewport(viewport);
	m_debugCascadesPass.BeginAddingPrimitives();

	// Update shared constant buffer data (must be done before preparing primitives)
	m_viewInfoCount = GetGraphicsPipeline().GenerateViewInfo(m_viewInfo);
	rd->GetGraphicsPipeline().GeneratePerViewConstantBuffer(m_viewInfo, m_viewInfoCount, m_pPerViewConstantBuffer);

	// Prepare primitives
	const EShaderQuality shaderQuality = rd->m_cEF.m_ShaderProfiles[eST_Shadow].GetShaderQuality();
	int firstUnusedStencilValue = rd->m_nStencilMaskRef;

	if (CRendererCVars::CV_r_ShadowsMask == 1 || CRendererCVars::CV_r_ShadowsMask == 2)
	{
		m_sunShadowPrimitives = m_pSunShadows->PreparePrimitives(
			m_maskGenPasses.front(),
			firstUnusedStencilValue,
			usingCloudShadows,
			usingScreenSpaceShadows,
			usingSvoShadows,
			texelRelativeBias,
			extendLastCachedCascade,
			debugCascades ? &m_debugCascadesPass : nullptr,
			pRenderView,
			rtFlagsByQuality[shaderQuality]);
	}

	if (CRendererCVars::CV_r_ShadowsMask == 1 || CRendererCVars::CV_r_ShadowsMask == 3)
	{
		m_localLightPrimitives = m_pLocalLightShadows->PreparePrimitives(
			m_maskGenPasses,
			firstUnusedStencilValue,
			usingScreenSpaceShadows,
			pRenderView,
			rtFlagsByQuality[shaderQuality]);
	}

	CRY_ASSERT(rd->m_nStencilMaskRef == STENCIL_VALUE_OUTDOORS+1);
	CRY_ASSERT(rd->m_nStencilMaskRef + firstUnusedStencilValue < STENC_MAX_REF);
	rd->m_nStencilMaskRef += firstUnusedStencilValue;

	// Clear first rendertarget slice and stencil buffer
	{
		auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

		SResourceView colorSlicesDesc = SResourceView::RenderTargetView(DeviceFormats::ConvertFromTexFormat(m_pShadowMaskRT->GetDstFormat()), 0, CRendererCVars::CV_r_ShadowsMask == 1 ? 1 : -1);
		D3DSurface* pColorRTV = m_pShadowMaskRT->GetDevTexture()->GetOrCreateRTV(colorSlicesDesc);
		D3DDepthSurface* pAllSliceDSV = pRenderView->GetDepthTarget()->GetDevTexture()->LookupDSV(EDefaultResourceViews::DepthStencil);

		commandList.GetGraphicsInterface()->ClearSurface(pColorRTV, Clr_Transparent);
		commandList.GetGraphicsInterface()->ClearSurface(pAllSliceDSV, CLEAR_STENCIL, Clr_Unused.r, 0);
	}
}

void CShadowMaskStage::Execute()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	PROFILE_LABEL_SCOPE("SHADOWMASK");

	// sun shadows
	if (m_sunShadowPrimitives > 0)
	{
		PROFILE_LABEL_SCOPE("SHADOWMASK_SUN");
		m_maskGenPasses[0].Execute();
	}

	if (m_debugCascadesPass.GetPrimitiveCount() > 0)
	{
		m_debugCascadesPass.Execute();
	}

	// local lights
	if (m_localLightPrimitives > 0)
	{
		PROFILE_LABEL_SCOPE("SHADOWMASK_DEFERRED_LIGHTS");

		for (int shadowMaskSlice = m_sunShadowPrimitives > 0 ? 1 : 0; shadowMaskSlice < m_maskGenPasses.size(); ++shadowMaskSlice)
		{
			CPrimitiveRenderPass& sliceGenPass = m_maskGenPasses[shadowMaskSlice];
			if (sliceGenPass.GetPrimitiveCount() > 0)
			{
				sliceGenPass.Execute();
			}
		}
	}
}

void CShadowMaskStage::OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater)
{
	m_pSunShadows->OnCVarsChanged(cvarUpdater);
	m_pLocalLightShadows->OnCVarsChanged(cvarUpdater);
}

////////////////////////////////////////////// Shadow mask internal stuff /////////////////////////////////////////////////////////

namespace ShadowMaskInternal
{

CSunShadows::CSunShadows(const CShadowMaskStage& stage)
	: shadowMaskStage(stage)
{
	m_nearestFullscreenTri = gcpRendD3D->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_STATIC, 3 * sizeof(SVF_P3F));
	m_nearestZ = 0;

	InitPrimitives();
}

void CSunShadows::InitPrimitives()
{
	for (auto& prim : cachedSamplingPrimitives) prim.AllocateTypedConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, sizeof(STypedConstants), EShaderStage_Vertex | EShaderStage_Pixel);
	for (auto& prim : cachedSamplingPrimitives) prim.AllocateTypedConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, sizeof(STypedConstants), EShaderStage_Vertex | EShaderStage_Pixel);
	for (auto& prim : cachedStencilPrimitives)  prim.AllocateTypedConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, sizeof(STypedConstants), EShaderStage_Vertex | EShaderStage_Pixel);
	for (auto& prim : cachedDebugPrimitives)    prim.SetFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);

	nearestShadowPrimitive.AllocateTypedConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, sizeof(STypedConstants), EShaderStage_Vertex | EShaderStage_Pixel);
	customShadowPrimitive.AllocateTypedConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, sizeof(SCloudShadowConstants), EShaderStage_Pixel);
}

void CSunShadows::ResetPrimitives()
{
	for (auto& prim : cachedStencilPrimitives)  prim.Reset();
	for (auto& prim : cachedSamplingPrimitives) prim.Reset();
	for (auto& prim : cachedDebugPrimitives)    prim.Reset();
	for (auto& prim : cachedCustomPrimitives)   prim.Reset();

	nearestShadowPrimitive.Reset();
	customShadowPrimitive.Reset();
}

void CSunShadows::OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater)
{
	if (cvarUpdater.GetCVar("e_ShadowsMaxTexRes"))
	{
		ResetPrimitives();
		InitPrimitives();
	}
}

CSunShadows::~CSunShadows()
{
	gcpRendD3D->m_DevBufMan.Destroy(m_nearestFullscreenTri);
}

int CSunShadows::PreparePrimitives(CPrimitiveRenderPass& sliceGenPass, int& firstUnusedStencilValue, bool renderCloudShadows, bool renderScreenSpaceShadows, bool renderSvoShadows,
                                   bool useTexelRelativeBias, bool extendLastCachedCascade, CPrimitiveRenderPass* pDebugCascadesPass, CRenderView* pRenderView, uint64 qualityFlags)
{
	const bool bPrepareCustomPrimitives  = !pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_Custom).empty();
	const bool bPrepareCascadePrimitives = !pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_SunDynamic).empty() || 
	                                       !pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_SunCached).empty() && !extendLastCachedCascade;
	const int previousPrimitiveCount = sliceGenPass.GetPrimitiveCount();

	uint64 rtFlags = qualityFlags;
	rtFlags |= gcpRendD3D->GetShadowJittering() > 0.0f ? g_HWSR_MaskBit[HWSR_SHADOW_JITTERING] : 0;
	rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE0]; // TODO: can be removed once legacy pipeline is gone

	if (bPrepareCascadePrimitives)
	{
		SCascadePrimitiveContext context(pRenderView, false, useTexelRelativeBias, extendLastCachedCascade, rtFlags);
		ShadowMapFrustum* pFirstFrustum = !context.frustums.empty() ? context.frustums.front()->pFrustum : nullptr;
		const bool bCascadeBlending = pFirstFrustum && pFirstFrustum->bBlendFrustum && !pDebugCascadesPass;
		const bool bStencilPrepass = CRendererCVars::CV_r_ShadowMaskStencilPrepass != 0 || pDebugCascadesPass;

		if (bCascadeBlending)
		{
			PrepareCascadePrimitivesWithBlending(sliceGenPass, firstUnusedStencilValue, context);
		}
		else if (bStencilPrepass)
		{
			PrepareCascadePrimitivesWithPrepass(sliceGenPass, pDebugCascadesPass, firstUnusedStencilValue, context);
		}
		else
		{
			PrepareCascadePrimitivesNoBlending(sliceGenPass, firstUnusedStencilValue, context);
		}
	}

	if (bPrepareCustomPrimitives)
	{
		customPrimitiveCount = 0;
		rtFlags &= ~g_HWSR_MaskBit[HWSR_SAMPLE1]; // disable contact hardening for custom frustums

		for (auto& pFrustumToRender : pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_Custom))
		{
			auto pFrustum = pFrustumToRender->pFrustum;

			if (pFrustum->m_eFrustumType == ShadowMapFrustum::e_Nearest)
			{
				PrepareNearestPrimitive(nearestShadowPrimitive, pFrustum, rtFlags);
				nearestShadowPrimitive.Compile(sliceGenPass);

				sliceGenPass.AddPrimitive(&nearestShadowPrimitive);
			}
			else if (customPrimitiveCount < cachedCustomPrimitives.size() - 1)
			{
				CRY_ASSERT(pFrustum->m_eFrustumType == ShadowMapFrustum::e_PerObject);
				SCustomPrimitiveContext context(pFrustum.get(), rtFlags);
				auto& primSampling = cachedCustomPrimitives[customPrimitiveCount++];
				auto& primStencil0 = cachedCustomPrimitives[customPrimitiveCount++];
				auto& primStencil1 = cachedCustomPrimitives[customPrimitiveCount++];

				PreparePerObjectPrimitives(primStencil0, primStencil1, primSampling, firstUnusedStencilValue, context);
				
				primStencil0.Compile(sliceGenPass);
				sliceGenPass.AddPrimitive(&primStencil0);

				primStencil1.Compile(sliceGenPass);
				sliceGenPass.AddPrimitive(&primStencil1);

				primSampling.Compile(sliceGenPass);
				sliceGenPass.AddPrimitive(&primSampling);

			}
		}
	}

	if (renderCloudShadows || renderSvoShadows || renderScreenSpaceShadows)
	{
		_smart_ptr<CTexture> pCloudShadowTex = CRendererResources::s_ptexBlack;
		if (renderCloudShadows)
			pCloudShadowTex = m_pCloudShadowTex;

		PrepareCustomShadowsPrimitive(customShadowPrimitive, pCloudShadowTex);

		customShadowPrimitive.Compile(sliceGenPass);
		sliceGenPass.AddPrimitive(&customShadowPrimitive);
	}

	return sliceGenPass.GetPrimitiveCount() - previousPrimitiveCount;
}

void CSunShadows::PrepareCascadePrimitivesWithPrepass(CPrimitiveRenderPass& sliceGenPass, CPrimitiveRenderPass* pDebugCascadesPass, int& firstUnusedStencilValue, const SCascadePrimitiveContext& context)
{
	const SRenderViewInfo& viewInfo = shadowMaskStage.m_viewInfo[0];

	// stencil primitives first
	for (int i = context.frustums.size() - 1; i >= 0; --i)
	{
		static CCryNameTSCRC techStencil = "ClipVolumeStencil";
		CShader* pShader = CShaderMan::s_shDeferredShading;

		ShadowMapFrustum* pFrustum = context.frustums[i]->pFrustum;
		CRY_ASSERT(pFrustum->nShadowMapLod >= 0 && pFrustum->nShadowMapLod < MAX_GSM_LODS_NUM);

		const int stencilRef = firstUnusedStencilValue + i;
		const bool bReverseDepth = (viewInfo.flags & SRenderViewInfo::eFlags_ReverseDepth) != 0;

		CRenderPrimitive& primStencil = cachedStencilPrimitives[i];
		primStencil.SetTechnique(pShader, techStencil, 0);
		primStencil.SetRenderState((bReverseDepth ? GS_DEPTHFUNC_GEQUAL : GS_DEPTHFUNC_LEQUAL) | GS_STENCIL | GS_NOCOLMASK_RGBA);
		primStencil.SetPrimitiveType(CRenderPrimitive::ePrim_CenteredBox);
		primStencil.SetCullMode(eCULL_Front);
		primStencil.SetStencilState(
		  STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
		  STENCOP_PASS(FSS_STENCOP_KEEP), stencilRef);

		PrepareStencilPassConstants(primStencil, pFrustum);	
		
		primStencil.Compile(sliceGenPass);
		sliceGenPass.AddPrimitive(&primStencil);
	}

	// now prepare sampling primitives
	for (int i = 0; i < context.frustums.size(); ++i)
	{
		static CCryNameTSCRC techSampling = "ShadowMask";
		CShader* pShader = CShaderMan::s_ShaderShadowMaskGen;

		ShadowMapFrustum* pFrustum = context.frustums[i]->pFrustum;
		CRY_ASSERT(pFrustum->nShadowMapLod >= 0 && pFrustum->nShadowMapLod < MAX_GSM_LODS_NUM);

		const bool bScreenspaceShadows = i == 0 && context.renderScreenspaceShadows;
		const int stencilRef = firstUnusedStencilValue + i;

		uint64 rtFlags = context.rtFlags;
		rtFlags |= (bScreenspaceShadows) ? g_HWSR_MaskBit[HWSR_SAMPLE2] : 0;
		rtFlags |= (context.useTexelRelativeBias) ? g_HWSR_MaskBit[HWSR_SAMPLE4] : 0;

		CRenderPrimitive& primSampling = cachedSamplingPrimitives[i];
		primSampling.SetTechnique(pShader, techSampling, rtFlags);
		primSampling.SetRenderState(GS_NODEPTHTEST | GS_STENCIL);
		primSampling.SetCullMode(eCULL_Back);
		primSampling.SetStencilState(
		  STENC_FUNC(FSS_STENCFUNC_EQUAL) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
		  STENCOP_PASS(FSS_STENCOP_KEEP), stencilRef);
		primSampling.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		primSampling.SetTexture(0, pFrustum->pDepthTex ? pFrustum->pDepthTex : CRendererResources::s_ptexFarPlane);
		primSampling.SetTexture(1, CRendererResources::s_ptexShadowJitterMap);
		primSampling.SetTexture(2, CRendererResources::s_ptexLinearDepth);
		primSampling.SetTexture(3, CRendererResources::s_ptexFarPlane);
		primSampling.SetTexture(4, CRendererResources::s_ptexSceneNormalsMap);
		primSampling.SetTexture(5, CRendererResources::s_ptexSceneDiffuse);
		primSampling.SetTexture(6, CRendererResources::s_ptexSceneSpecular);
		primSampling.SetSampler(0, EDefaultSamplerStates::LinearCompare);
		primSampling.SetSampler(1, EDefaultSamplerStates::PointClamp);
		primSampling.SetSampler(2, EDefaultSamplerStates::PointWrap);
		primSampling.SetDrawInfo(eptTriangleList, 0, 0, 3);

		PrepareConstantBuffers(primSampling, pFrustum, pFrustum, false);

		primSampling.Compile(sliceGenPass);
		sliceGenPass.AddPrimitive(&primSampling);

		if (pDebugCascadesPass)
		{
			CRenderPrimitive& primDebug = cachedDebugPrimitives[i];
			if (PrepareDebugPrimitive(*pDebugCascadesPass, primDebug, pFrustum, stencilRef))
			{
				primDebug.Compile(*pDebugCascadesPass);
				pDebugCascadesPass->AddPrimitive(&primDebug);
			}
		}
	}

	firstUnusedStencilValue = firstUnusedStencilValue + context.frustums.size();
}

void CSunShadows::PrepareCascadePrimitivesNoBlending(CPrimitiveRenderPass& sliceGenPass, int& firstUnusedStencilValue, const SCascadePrimitiveContext& context)
{
	static CCryNameTSCRC techSampling = "ShadowMaskVolume";
	CShader* pShader = CShaderMan::s_ShaderShadowMaskGen;

	const SRenderViewInfo& viewInfo = shadowMaskStage.m_viewInfo[0];
	const int maxStencilValue = firstUnusedStencilValue + context.frustums.size() - 1;
	const bool bReverseDepth = (viewInfo.flags & SRenderViewInfo::eFlags_ReverseDepth) != 0;

	for (int i = 0; i < context.frustums.size(); ++i)
	{
		const bool bScreenspaceShadows = i == 0 && context.renderScreenspaceShadows;
		const int stencilRef = maxStencilValue - i;

		ShadowMapFrustum* pFrustum = context.frustums[i]->pFrustum;
		CRY_ASSERT(pFrustum->nShadowMapLod >= 0 && pFrustum->nShadowMapLod < MAX_GSM_LODS_NUM);

		uint64 rtFlags = context.rtFlags;
		rtFlags |= (bScreenspaceShadows) ? g_HWSR_MaskBit[HWSR_SAMPLE2] : 0;
		rtFlags |= (context.useTexelRelativeBias) ? g_HWSR_MaskBit[HWSR_SAMPLE4] : 0;

		CRenderPrimitive& primSampling = cachedSamplingPrimitives[i];
		primSampling.SetTechnique(pShader, techSampling, rtFlags);
		primSampling.SetRenderState((bReverseDepth ? GS_DEPTHFUNC_LEQUAL : GS_DEPTHFUNC_GEQUAL) | GS_STENCIL);
		primSampling.SetCullMode(eCULL_Front);
		primSampling.SetStencilState(
		  STENC_FUNC(FSS_STENCFUNC_GEQUAL) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
		  STENCOP_PASS(FSS_STENCOP_REPLACE), stencilRef);
		primSampling.SetPrimitiveType(CRenderPrimitive::ePrim_CenteredBox);
		primSampling.SetTexture(0, pFrustum->pDepthTex ? pFrustum->pDepthTex : CRendererResources::s_ptexFarPlane);
		primSampling.SetTexture(1, CRendererResources::s_ptexShadowJitterMap);
		primSampling.SetTexture(2, CRendererResources::s_ptexLinearDepth);
		primSampling.SetTexture(3, CRendererResources::s_ptexFarPlane);
		primSampling.SetTexture(4, CRendererResources::s_ptexSceneNormalsMap);
		primSampling.SetTexture(5, CRendererResources::s_ptexSceneDiffuse);
		primSampling.SetTexture(6, CRendererResources::s_ptexSceneSpecular);
		primSampling.SetSampler(0, EDefaultSamplerStates::LinearCompare);
		primSampling.SetSampler(1, EDefaultSamplerStates::PointClamp);
		primSampling.SetSampler(2, EDefaultSamplerStates::PointWrap);

		PrepareConstantBuffers(primSampling, pFrustum, pFrustum, false);

		primSampling.Compile(sliceGenPass);
		sliceGenPass.AddPrimitive(&primSampling);
	}

	firstUnusedStencilValue = maxStencilValue + 1;
}

void CSunShadows::PrepareCascadePrimitivesWithBlending(CPrimitiveRenderPass& sliceGenPass, int& firstUnusedStencilValue, const SCascadePrimitiveContext& context)
{
	static CCryNameTSCRC techSampling = "ShadowMaskVolume";
	CShader* pShader = CShaderMan::s_ShaderShadowMaskGen;

	const SRenderViewInfo& viewInfo = shadowMaskStage.m_viewInfo[0];
	const int maxStencilValue = firstUnusedStencilValue + 2 * context.frustums.size() - 1;
	const bool bReverseDepth = (viewInfo.flags & SRenderViewInfo::eFlags_ReverseDepth) != 0;
	const int gsDepthFunc = bReverseDepth ? GS_DEPTHFUNC_LEQUAL : GS_DEPTHFUNC_GEQUAL;

	for (int i = 0; i < context.frustums.size(); ++i)
	{
		const bool bScreenspaceShadows = i == 0 && context.renderScreenspaceShadows;
		const int stencilRef = maxStencilValue - 2 * i;

		ShadowMapFrustum* pFrustum = context.frustums[i]->pFrustum;
		CRY_ASSERT(pFrustum->nShadowMapLod >= 0 && pFrustum->nShadowMapLod < MAX_GSM_LODS_NUM);

		uint64 rtFlags = context.rtFlags;
		rtFlags |= (bScreenspaceShadows) ? g_HWSR_MaskBit[HWSR_SAMPLE2] : 0;
		rtFlags |= (context.useTexelRelativeBias) ? g_HWSR_MaskBit[HWSR_SAMPLE4] : 0;

		// inner region (no blending)
		{
			CRenderPrimitive& primSampling = cachedSamplingPrimitives[3 * i + 0];
			primSampling.SetTechnique(pShader, techSampling, rtFlags);
			primSampling.SetRenderState(gsDepthFunc | GS_STENCIL | GS_BLSRC_ONE | GS_BLDST_ONE);
			primSampling.SetCullMode(eCULL_Front);
			primSampling.SetStencilState(
			  STENC_FUNC(FSS_STENCFUNC_GREATER) |
			  STENCOP_FAIL(FSS_STENCOP_KEEP) |
			  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
			  STENCOP_PASS(FSS_STENCOP_REPLACE), stencilRef);
			primSampling.SetPrimitiveType(CRenderPrimitive::ePrim_CenteredBox);
			primSampling.SetTexture(0, pFrustum->pDepthTex ? pFrustum->pDepthTex : CRendererResources::s_ptexFarPlane);
			primSampling.SetTexture(1, CRendererResources::s_ptexShadowJitterMap);
			primSampling.SetTexture(2, CRendererResources::s_ptexLinearDepth);
			primSampling.SetTexture(3, CRendererResources::s_ptexFarPlane);
			primSampling.SetTexture(4, CRendererResources::s_ptexSceneNormalsMap);
			primSampling.SetTexture(5, CRendererResources::s_ptexSceneDiffuse);
			primSampling.SetTexture(6, CRendererResources::s_ptexSceneSpecular);
			primSampling.SetSampler(0, EDefaultSamplerStates::LinearCompare);
			primSampling.SetSampler(1, EDefaultSamplerStates::PointClamp);
			primSampling.SetSampler(2, EDefaultSamplerStates::PointWrap);

			PrepareConstantBuffers(primSampling, pFrustum, pFrustum, true);

			primSampling.Compile(sliceGenPass);
			sliceGenPass.AddPrimitive(&primSampling);
		}

		// outer region (blend-out only)
		{
			CRenderPrimitive& primSampling = cachedSamplingPrimitives[3 * i + 1];
			primSampling.SetTechnique(pShader, techSampling, rtFlags | g_HWSR_MaskBit[HWSR_SAMPLE3]);
			primSampling.SetRenderState(gsDepthFunc | GS_STENCIL | GS_BLSRC_ONE | GS_BLDST_ONE);
			primSampling.SetCullMode(eCULL_Front);
			primSampling.SetStencilState(
			  STENC_FUNC(FSS_STENCFUNC_GREATER) |
			  STENCOP_FAIL(FSS_STENCOP_KEEP) |
			  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
			  STENCOP_PASS(FSS_STENCOP_REPLACE), stencilRef - 1);
			primSampling.SetPrimitiveType(CRenderPrimitive::ePrim_CenteredBox);
			primSampling.SetTexture(0, pFrustum->pDepthTex ? pFrustum->pDepthTex : CRendererResources::s_ptexFarPlane);
			primSampling.SetTexture(1, CRendererResources::s_ptexShadowJitterMap);
			primSampling.SetTexture(2, CRendererResources::s_ptexLinearDepth);
			primSampling.SetTexture(3, CRendererResources::s_ptexFarPlane);
			primSampling.SetTexture(4, CRendererResources::s_ptexSceneNormalsMap);
			primSampling.SetTexture(5, CRendererResources::s_ptexSceneDiffuse);
			primSampling.SetTexture(6, CRendererResources::s_ptexSceneSpecular);
			primSampling.SetSampler(0, EDefaultSamplerStates::LinearCompare);
			primSampling.SetSampler(1, EDefaultSamplerStates::PointClamp);
			primSampling.SetSampler(2, EDefaultSamplerStates::PointWrap);

			PrepareConstantBuffers(primSampling, pFrustum, pFrustum, false);

			primSampling.Compile(sliceGenPass);
			sliceGenPass.AddPrimitive(&primSampling);
		}

		// outer region (next frustum blend-in)
		if (i < context.frustums.size() - 1)
		{
			ShadowMapFrustum* pNextFrustum = context.frustums[i + 1]->pFrustum;
			pNextFrustum->pPrevFrustum = pFrustum;

			CRenderPrimitive& primSampling = cachedSamplingPrimitives[3 * i + 2];
			primSampling.SetTechnique(pShader, techSampling, rtFlags);
			primSampling.SetRenderState(GS_STENCIL | gsDepthFunc | GS_BLSRC_ONE | GS_BLDST_ONE);
			primSampling.SetCullMode(eCULL_Front);
			primSampling.SetStencilState(
			  STENC_FUNC(FSS_STENCFUNC_EQUAL) |
			  STENCOP_FAIL(FSS_STENCOP_KEEP) |
			  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
			  STENCOP_PASS(FSS_STENCOP_KEEP), stencilRef - 1);
			primSampling.SetPrimitiveType(CRenderPrimitive::ePrim_CenteredBox);
			primSampling.SetTexture(0, pNextFrustum->pDepthTex ? pNextFrustum->pDepthTex : CRendererResources::s_ptexFarPlane);
			primSampling.SetTexture(1, CRendererResources::s_ptexShadowJitterMap);
			primSampling.SetTexture(2, CRendererResources::s_ptexLinearDepth);
			primSampling.SetTexture(3, CRendererResources::s_ptexFarPlane);
			primSampling.SetTexture(4, CRendererResources::s_ptexSceneNormalsMap);
			primSampling.SetTexture(5, CRendererResources::s_ptexSceneDiffuse);
			primSampling.SetTexture(6, CRendererResources::s_ptexSceneSpecular);
			primSampling.SetSampler(0, EDefaultSamplerStates::LinearCompare);
			primSampling.SetSampler(1, EDefaultSamplerStates::PointClamp);
			primSampling.SetSampler(2, EDefaultSamplerStates::PointWrap);

			PrepareConstantBuffers(primSampling, pNextFrustum, pFrustum, false);

			primSampling.Compile(sliceGenPass);
			sliceGenPass.AddPrimitive(&primSampling);

			pNextFrustum->pPrevFrustum = nullptr;
		}
	}

	firstUnusedStencilValue = maxStencilValue + 1;
}

void CSunShadows::PrepareNearestPrimitive(CRenderPrimitive& primitive, ShadowMapFrustum* pFrustum, uint64 rtFlags)
{
	static CCryNameTSCRC techSampling = "ShadowMaskVolume";
	CShader* pShader = CShaderMan::s_ShaderShadowMaskGen;

	const SRenderViewInfo& viewInfo = shadowMaskStage.m_viewInfo[0];
	const bool bReverseDepth = (viewInfo.flags & SRenderViewInfo::eFlags_ReverseDepth) != 0;
	const int gsDepthFunc = bReverseDepth ? GS_DEPTHFUNC_LEQUAL : GS_DEPTHFUNC_GEQUAL;

	if (m_nearestZ != CRendererCVars::CV_r_DrawNearZRange)
	{
		m_nearestZ = CRendererCVars::CV_r_DrawNearZRange;

		float renderZ = m_nearestZ - 0.001f;
		if (bReverseDepth)
			renderZ = 1.0f - renderZ;

		CryStackAllocWithSizeVector(SVF_P3F, 3, fullscreenTriVertices, CDeviceBufferManager::AlignBufferSizeForStreaming);
		fullscreenTriVertices[0].xyz = Vec3(-1.0f, 1.0f, renderZ);
		fullscreenTriVertices[1].xyz = Vec3(-1.0f, -3.0f, renderZ);
		fullscreenTriVertices[2].xyz = Vec3(3.0f, 1.0f, renderZ);

		gcpRendD3D->m_DevBufMan.UpdateBuffer(m_nearestFullscreenTri, fullscreenTriVertices, 3 * sizeof(SVF_P3F));
	}

	primitive.SetTechnique(pShader, techSampling, rtFlags | g_HWSR_MaskBit[HWSR_NEAREST]);
	primitive.SetRenderState(gsDepthFunc | GS_BLSRC_ONE | GS_BLDST_ONE | GS_BLEND_OP_MAX);
	primitive.SetCullMode(eCULL_Back);
	primitive.SetCustomVertexStream(m_nearestFullscreenTri, EDefaultInputLayouts::P3F, sizeof(SVF_P3F));
	primitive.SetCustomIndexStream(~0u, RenderIndexType(0));
	primitive.SetTexture(0, pFrustum->pDepthTex ? pFrustum->pDepthTex : CRendererResources::s_ptexFarPlane);
	primitive.SetTexture(1, CRendererResources::s_ptexShadowJitterMap);
	primitive.SetTexture(2, CRendererResources::s_ptexLinearDepth);
	primitive.SetTexture(3, CRendererResources::s_ptexFarPlane);
	primitive.SetTexture(4, CRendererResources::s_ptexSceneNormalsMap);
	primitive.SetTexture(5, CRendererResources::s_ptexSceneDiffuse);
	primitive.SetTexture(6, CRendererResources::s_ptexSceneSpecular);
	primitive.SetSampler(0, EDefaultSamplerStates::LinearCompare);
	primitive.SetSampler(1, EDefaultSamplerStates::PointClamp);
	primitive.SetSampler(2, EDefaultSamplerStates::PointWrap);
	primitive.SetDrawInfo(eptTriangleList, 0, 0, 3);

	PrepareConstantBuffers(primitive, pFrustum, nullptr, false);
}

void CSunShadows::PreparePerObjectPrimitives(CRenderPrimitive& primStencil0, CRenderPrimitive& primStencil1, CRenderPrimitive& primSampling, int& firstUnusedStencilValue, const SCustomPrimitiveContext& context)
{
	const int stencilRef = firstUnusedStencilValue;
	const SRenderViewInfo& viewInfo = shadowMaskStage.m_viewInfo[0];
	const bool bReverseDepth = (viewInfo.flags & SRenderViewInfo::eFlags_ReverseDepth) != 0;
	const int gsDepthTest = (bReverseDepth ? GS_DEPTHFUNC_GEQUAL : GS_DEPTHFUNC_LEQUAL);

	// stencil backfaces
	{
		static CCryNameTSCRC techStencil = "ClipVolumeStencil";
		CShader* pShader = CShaderMan::s_shDeferredShading;

		primStencil0.SetTechnique(pShader, techStencil, 0);
		primStencil0.SetRenderState(gsDepthTest | GS_STENCIL | GS_NOCOLMASK_RGBA);
		primStencil0.SetPrimitiveType(CRenderPrimitive::ePrim_CenteredBox);
		primStencil0.SetCullMode(eCULL_Front);
		primStencil0.SetStencilState(
		  STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
		  STENCOP_PASS(FSS_STENCOP_KEEP), stencilRef);

		PrepareStencilPassConstants(primStencil0, context.pFrustum);
	}

	// stencil frontfaces
	{
		static CCryNameTSCRC techStencil = "ClipVolumeStencil";
		CShader* pShader = CShaderMan::s_shDeferredShading;

		primStencil1.SetTechnique(pShader, techStencil, 0);
		primStencil1.SetRenderState(gsDepthTest | GS_STENCIL | GS_NOCOLMASK_RGBA);
		primStencil1.SetPrimitiveType(CRenderPrimitive::ePrim_CenteredBox);
		primStencil1.SetCullMode(eCULL_Back);
		primStencil1.SetStencilState(
		  STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_ZERO) |
		  STENCOP_PASS(FSS_STENCOP_KEEP), stencilRef);

		PrepareStencilPassConstants(primStencil1, context.pFrustum);
	}

	// now prepare sampling primitive
	{
		static CCryNameTSCRC techSampling = "ShadowMaskVolume";
		CShader* pShader = CShaderMan::s_ShaderShadowMaskGen;

		primSampling.SetTechnique(pShader, techSampling, context.rtFlags);
		primSampling.SetRenderState(GS_NODEPTHTEST | GS_STENCIL | GS_BLSRC_ONE | GS_BLDST_ONE | GS_BLEND_OP_MAX);
		primSampling.SetCullMode(eCULL_Front);
		primSampling.SetStencilState(
		  STENC_FUNC(FSS_STENCFUNC_EQUAL) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
		  STENCOP_PASS(FSS_STENCOP_KEEP), stencilRef);
		primSampling.SetPrimitiveType(CRenderPrimitive::ePrim_CenteredBox);
		primSampling.SetTexture(0, context.pFrustum->pDepthTex ? context.pFrustum->pDepthTex : CRendererResources::s_ptexFarPlane);
		primSampling.SetTexture(1, CRendererResources::s_ptexShadowJitterMap);
		primSampling.SetTexture(2, CRendererResources::s_ptexLinearDepth);
		primSampling.SetTexture(3, CRendererResources::s_ptexFarPlane);
		primSampling.SetTexture(4, CRendererResources::s_ptexSceneNormalsMap);
		primSampling.SetTexture(5, CRendererResources::s_ptexSceneDiffuse);
		primSampling.SetTexture(6, CRendererResources::s_ptexSceneSpecular);
		primSampling.SetSampler(0, EDefaultSamplerStates::LinearCompare);
		primSampling.SetSampler(1, EDefaultSamplerStates::PointClamp);
		primSampling.SetSampler(2, EDefaultSamplerStates::PointWrap);

		PrepareConstantBuffers(primSampling, context.pFrustum, context.pFrustum, false);
	}

	firstUnusedStencilValue += 1;
}

void CSunShadows::PrepareStencilPassConstants(CRenderPrimitive& primitive, ShadowMapFrustum* pFrustum) const
{
	auto constants = primitive.GetConstantManager().BeginTypedConstantUpdate<STypedConstants>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex | EShaderStage_Pixel);

	constants->unitMeshTransform = pFrustum->mLightViewMatrix.GetInverted() * shadowMaskStage.m_viewInfo[0].cameraProjMatrix;

	if (shadowMaskStage.m_viewInfoCount > 1)
	{
		constants.BeginStereoOverride();
		constants->unitMeshTransform = pFrustum->mLightViewMatrix.GetInverted() * shadowMaskStage.m_viewInfo[1].cameraProjMatrix;
	}

	primitive.GetConstantManager().EndTypedConstantUpdate(constants);
}

void CSunShadows::PrepareConstantBuffers(CRenderPrimitive& primitive, ShadowMapFrustum* pFrustum, ShadowMapFrustum* pVolumeProvider, bool bScaledVolume) const
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	SRenderViewShaderConstants& PF =  shadowMaskStage.RenderView()->GetShaderConstants();
	auto& constantManager = primitive.GetConstantManager();

	// assign per view constant buffer
	constantManager.SetTypedConstantBuffer(eConstantBufferShaderSlot_PerView, shadowMaskStage.m_pPerViewConstantBuffer, EShaderStage_Vertex | EShaderStage_Pixel);

	// update typed constants
	auto constants = constantManager.BeginTypedConstantUpdate<STypedConstants>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex | EShaderStage_Pixel);

	for (int i = 0; i < shadowMaskStage.m_viewInfoCount; ++i)
	{
		const SRenderViewInfo& viewInfo = shadowMaskStage.m_viewInfo[i];
		auto shadowSamplingInfo = CShadowUtils::GetDeferredShadowSamplingInfo(pFrustum, 0, *viewInfo.pCamera, viewInfo.viewport, shadowMaskStage.RenderView()->m_vProjMatrixSubPixoffset);

		constants->unitMeshTransform.SetIdentity();
		constants->camPos = shadowSamplingInfo.camPosShadowSpace;
		constants->screenToShadowBasis = shadowSamplingInfo.screenToShadowBasis;
		constants->noiseProjection = shadowSamplingInfo.noiseProjection;
		constants->blendTcNormalize = Vec4(ZERO);
		constants->oneDivFarDist = Vec4(shadowSamplingInfo.oneDivFarDist, shadowSamplingInfo.oneDivFarDistBlend, 0, 0);
		constants->depthTestBias = Vec4(shadowSamplingInfo.depthTestBias);
		memcpy(constants->irreg_kernel_2d, PF.irregularFilterKernel, sizeof(PF.irregularFilterKernel));
		static_assert(sizeof(constants->irreg_kernel_2d) == sizeof(PF.irregularFilterKernel), "Both sizes must be same.");
		constants->kernelRadius = Vec4(shadowSamplingInfo.kernelRadius);
		constants->adaption = Vec4(
			CRendererCVars::CV_r_ShadowsAdaptionRangeClamp,
			CRendererCVars::CV_r_ShadowsAdaptionSize * 250.f,
			CRendererCVars::CV_r_ShadowsAdaptionMin, 0);
		constants->invShadowMapSize = Vec4(shadowSamplingInfo.invShadowMapSize);
		constants->shadowFadingDist = Vec4(shadowSamplingInfo.shadowFadingDist);
		constants->blendTexGen.SetZero();
		constants->blendInfo = Vec4(ZERO);
		constants->lightPos = Vec4(pFrustum->vLightSrcRelPos + pFrustum->vProjTranslation, 0);

		if (pVolumeProvider)
		{
			Matrix44 mScale(IDENTITY);
			if (bScaledVolume)
			{
				mScale.m00 = shadowSamplingInfo.blendInfo.x;
				mScale.m11 = shadowSamplingInfo.blendInfo.x;
			}

			constants->unitMeshTransform = mScale * pVolumeProvider->mLightViewMatrix.GetInverted() * viewInfo.cameraProjMatrix;
		}

		if (pFrustum->bBlendFrustum)
		{
			constants->blendInfo = shadowSamplingInfo.blendInfo;
			constants->blendTexGen = shadowSamplingInfo.blendTexGen;
			constants->blendTcNormalize = shadowSamplingInfo.blendTcNormalize;
		}

		if (shadowMaskStage.m_viewInfoCount > 1 && i == 0)
		{
			constants.BeginStereoOverride(false);
		}
	}

	constantManager.EndTypedConstantUpdate(constants);
}

// apply cloud shadows, SVO shadows, screen space shadows
void CSunShadows::PrepareCustomShadowsPrimitive(CRenderPrimitive& primitive, _smart_ptr<CTexture>& pCloudShadowTex) const
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	CShader* pShader = CShaderMan::s_ShaderShadowMaskGen;
	static CCryNameTSCRC techCustomShadows = "CustomShadows";

	uint64 rtFlags = g_HWSR_MaskBit[HWSR_SAMPLE2];
	pCloudShadowTex = rd->GetCloudShadowTextureId() > 0 ? CTexture::GetByID(rd->GetCloudShadowTextureId()) : CRendererResources::s_ptexWhite;
	SamplerStateHandle cloudSamplerState = EDefaultSamplerStates::BilinearWrap;

	if (rd->m_bVolumetricCloudsEnabled)
	{
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE3];
		pCloudShadowTex = CRendererResources::s_ptexVolCloudShadow;
		cloudSamplerState = EDefaultSamplerStates::TrilinearBorder_Black;
	}

#if defined(FEATURE_SVO_GI)
	CSvoRenderer* pSR = CSvoRenderer::GetInstance();
	if (pSR && pSR->GetTracedSunShadowsRT())
	{
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE4];
	}
#endif

	// apply screen space shadows on entire screen
	if (CRenderer::CV_r_ShadowsScreenSpace == 1)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE5];

	primitive.SetTechnique(pShader, techCustomShadows, rtFlags);
	primitive.SetRenderState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE | GS_BLEND_OP_MAX);
	primitive.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
	primitive.SetTexture(0, pCloudShadowTex);
	primitive.SetTexture(2, CRendererResources::s_ptexLinearDepth);

#if defined(FEATURE_SVO_GI)
	if (pSR && pSR->GetTracedSunShadowsRT())
	{
		primitive.SetTexture(3, pSR->GetTracedSunShadowsRT());
	}
#endif

	// we need transmittance info for SS shadows
	primitive.SetTexture(4, CRendererResources::s_ptexSceneNormalsMap);

	primitive.SetSampler(0, cloudSamplerState);
	primitive.SetDrawInfo(eptTriangleList, 0, 0, 3);

	// udpate constant buffers
	auto& constantManager = primitive.GetConstantManager();

	// per view constant buffer
	constantManager.SetTypedConstantBuffer(eConstantBufferShaderSlot_PerView, shadowMaskStage.m_pPerViewConstantBuffer);

	// cloud shadow specific constants
	SRenderViewShaderConstants& PF = shadowMaskStage.RenderView()->GetShaderConstants();
	auto constants = constantManager.BeginTypedConstantUpdate<SCloudShadowConstants>(eConstantBufferShaderSlot_PerPrimitive);
	constants->params = PF.pCloudShadowParams;
	constants->animParams = PF.pCloudShadowAnimParams;

	// screen space shadows constants
	constants->screenspaceShadowsParams = PF.pScreenspaceShadowsParams;

	constantManager.EndTypedConstantUpdate(constants);
}

bool CSunShadows::PrepareDebugPrimitive(CPrimitiveRenderPass& debugPass, CRenderPrimitive& primitive, const ShadowMapFrustum* pFrustum, int stencilRef) const
{
	const uint32 StencilStateTest =
	  STENC_FUNC(FSS_STENCFUNC_EQUAL) |
	  STENCOP_FAIL(FSS_STENCOP_KEEP) |
	  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
	  STENCOP_PASS(FSS_STENCOP_KEEP);

	const Vec4 cascadeColors[] =
	{
		Vec4(1, 0, 0, 1),
		Vec4(0, 1, 0, 1),
		Vec4(0, 0, 1, 1),
		Vec4(1, 1, 0, 1),
		Vec4(1, 0, 1, 1),
		Vec4(0, 1, 1, 1),
		Vec4(1, 0, 0, 1),
	};

	const int cascadeColorCount = CRY_ARRAY_COUNT(cascadeColors);

	static CCryNameTSCRC DebugTech = "DebugShadowCascades";
	static CCryNameR CascadeColorParam("DebugCascadeColor");

	CShader* pShader = CShaderMan::s_ShaderShadowMaskGen;

	primitive.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
	primitive.SetTechnique(pShader, DebugTech, 0);
	primitive.SetRenderState(GS_STENCIL | GS_NODEPTHTEST);
	primitive.SetStencilState(StencilStateTest, stencilRef);
	primitive.SetDrawInfo(eptTriangleList, 0, 0, 3);

	if (primitive.Compile(debugPass) == CRenderPrimitive::eDirty_None)
	{
		auto& constantManager = primitive.GetConstantManager();
		CRY_ASSERT(constantManager.IsShaderReflectionValid());

		constantManager.BeginNamedConstantUpdate();
		constantManager.SetNamedConstant(CascadeColorParam, cascadeColors[pFrustum->nShadowMapLod % cascadeColorCount], eHWSC_Pixel);
		constantManager.EndNamedConstantUpdate(&debugPass.GetViewport());

		return true;
	}

	return false;
}

CLocalLightShadows::CLocalLightShadows(const CShadowMaskStage& stage)
	: shadowMaskStage(stage)
{
	InitPrimitives();
}

void CLocalLightShadows::InitPrimitives()
{
	for (auto& prim : volumePrimitives)
	{
		CConstantBufferPtr pCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(STypedConstants));
		prim.SetConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pCB, EShaderStage_Vertex | EShaderStage_Pixel);
	}

	for (auto& prim : quadPrimitives)
	{
		CConstantBufferPtr pCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(STypedConstants));
		prim.SetConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pCB, EShaderStage_Vertex | EShaderStage_Pixel);
	}
}

void CLocalLightShadows::ResetPrimitives()
{
	for (auto& prim : volumePrimitives) prim.Reset();
	for (auto& prim : quadPrimitives)   prim.Reset();
}

void CLocalLightShadows::OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater)
{
	if (cvarUpdater.GetCVar("e_ShadowsPoolSize"))
	{
		ResetPrimitives();
		InitPrimitives();
	}
}

int CLocalLightShadows::PreparePrimitives(std::vector<CPrimitiveRenderPass>& maskGenPasses, int& firstUnusedStencilValue, bool usingScreenSpaceShadows, CRenderView* pRenderView, uint64 qualityFlags)
{
	// sort shadow frustums into shadow mask slices such that frustums in each slice don't overlap in screen space
	const uint32 maxSliceCount = shadowMaskStage.m_pShadowMaskRT->StreamGetNumSlices();
	auto &frustumsPerSlice = pRenderView->m_shadows.m_frustumsPerTiledShadingSlice;

	// prepare primitives for each shadow mask slice
	int volumePrimitiveCount = 0;
	int quadPrimitiveCount = 0;
	for (int slice = 0; slice < frustumsPerSlice.size(); ++slice)
	{
		for (auto& frustumPair : frustumsPerSlice[slice])
		{
			if (auto pFrustumToRender = frustumPair.first)
			{
				if (slice < maxSliceCount)
					PreparePrimitivesForLight(
						pRenderView,
						maskGenPasses[slice],
						firstUnusedStencilValue,
						usingScreenSpaceShadows,
						pFrustumToRender,
						qualityFlags,
						volumePrimitiveCount,
						quadPrimitiveCount);
				else
					pFrustumToRender->pFrustum->GetSideSampleMask().store(0);
			}
		}
	}

#if defined(ENABLE_PROFILING_CODE)
	SRenderStatistics::Write().m_NumShadowMaskChannels = (maxSliceCount << 16) | ((uint32)frustumsPerSlice.size() & 0xFFFF);
#endif
	return (volumePrimitiveCount + quadPrimitiveCount) * 3;
}

int CLocalLightShadows::PreparePrimitivesForLight(const CRenderView* pRenderView, CPrimitiveRenderPass& sliceGenPass, int& firstUnusedStencilValue, bool usingScreenSpaceShadows, SShadowFrustumToRender* pFrustumToRender, uint64 qualityFlags, int& volumePrimitiveCount, int& quadPrimitiveCount)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	const SRenderLight* pLight = pFrustumToRender->pLight;
	ShadowMapFrustum* pFrustum = pFrustumToRender->pFrustum;
	const SRenderViewInfo& viewInfo = shadowMaskStage.m_viewInfo[0];

	// determine what's more beneficial: full screen quad or light volume
	bool bStencilMask = true;
	bool bUseLightVolumes = false;
	EShapeMeshType meshType = SHAPE_PROJECTOR;
	CDeferredShading::Instance().GetLightRenderSettings(pRenderView, pLight, bStencilMask, bUseLightVolumes, meshType);

	CRY_ASSERT(meshType >= SHAPE_PROJECTOR && meshType <= SHAPE_PROJECTOR2);
	const CRenderPrimitive::EPrimitiveType meshToPrimtype[] =
	{
		CRenderPrimitive::ePrim_Projector,
		CRenderPrimitive::ePrim_Projector1,
		CRenderPrimitive::ePrim_Projector2
	};

	CRenderPrimitive::EPrimitiveType primitiveType = meshToPrimtype[meshType];

	const int nSides = pFrustum->GetNumSides();
	const bool bAreaLight = (pLight->m_Flags & DLF_AREA_LIGHT) && pLight->m_fAreaWidth && pLight->m_fAreaHeight && pLight->m_fLightFrustumAngle;
	const bool bReverseDepth = (viewInfo.flags & SRenderViewInfo::eFlags_ReverseDepth) != 0;

	/* TheoM TODO: scissor
	   if (CRenderer::CV_r_DeferredShadingScissor)
	   {
	   EF_Scissor(true,
	    static_cast<int>(lightRect.x * m_RP.m_CurDownscaleFactor.x),
	    static_cast<int>(lightRect.y * m_RP.m_CurDownscaleFactor.y),
	    static_cast<int>(lightRect.z * m_RP.m_CurDownscaleFactor.x + 1),
	    static_cast<int>(lightRect.w * m_RP.m_CurDownscaleFactor.y + 1));
	   }
	 */

	CRenderPrimitive* stencilPrimitives[6 * 2], * samplingPrimitives[6];
	int numStencilPrimitives = 0, numSamplingPrimitives = 0;

	for (int nS = 0; nS < nSides; nS++)
	{
		if (pFrustum->ShouldSampleSide(nS))
		{
			SLocalLightPrimitives& primitives = bUseLightVolumes ? volumePrimitives[volumePrimitiveCount++] : quadPrimitives[quadPrimitiveCount++];

			const int stencilRef = firstUnusedStencilValue + nSides - nS;

			static CCryNameTSCRC techStencil = "LightVolumeStencil";
			static CCryNameTSCRC techSamplingQuad = "ShadowMaskGen";
			static CCryNameTSCRC techSamplingVolume = "ShadowMaskGenVolume";

			CShader* pShader = CShaderMan::s_shDeferredShading;
			CCryNameTSCRC& techSampling = bUseLightVolumes ? techSamplingVolume : techSamplingQuad;

			uint64 rtFlagsSampling = qualityFlags;
			rtFlagsSampling |= bUseLightVolumes ? g_HWSR_MaskBit[HWSR_CUBEMAP0] : 0;
			rtFlagsSampling |= usingScreenSpaceShadows ? g_HWSR_MaskBit[HWSR_SAMPLE2] : 0;

			const int gsDepthFunc = bReverseDepth ? GS_DEPTHFUNC_GEQUAL : GS_DEPTHFUNC_LEQUAL;
			const int gsDepthTest = bUseLightVolumes ? 0 : GS_NODEPTHTEST;

			CRenderPrimitive& primStencilBackFace = primitives.stencilBackfaces;
			primStencilBackFace.SetTechnique(pShader, techStencil, 0);
			primStencilBackFace.SetRenderState(GS_STENCIL | GS_NOCOLMASK_RGBA | gsDepthFunc);
			primStencilBackFace.SetPrimitiveType(primitiveType);
			primStencilBackFace.SetCullMode(eCULL_Back); // winding order of primitives is flipped
			primStencilBackFace.SetStencilState(
			  STENC_FUNC(FSS_STENCFUNC_GEQUAL) |
			  STENCOP_FAIL(FSS_STENCOP_KEEP) |
			  STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
			  STENCOP_PASS(FSS_STENCOP_KEEP), stencilRef);

			CRenderPrimitive& primStencilFrontFace = primitives.stencilFrontfaces;
			primStencilFrontFace.SetTechnique(pShader, techStencil, 0);
			primStencilFrontFace.SetRenderState(GS_STENCIL | GS_NOCOLMASK_RGBA | gsDepthFunc);
			primStencilFrontFace.SetPrimitiveType(primitiveType);
			primStencilFrontFace.SetCullMode(eCULL_Front); // winding order of primitives is flipped
			primStencilFrontFace.SetStencilState(
			  STENC_FUNC(FSS_STENCFUNC_GEQUAL) |
			  STENCOP_FAIL(FSS_STENCOP_KEEP) |
			  STENCOP_ZFAIL(FSS_STENCOP_ZERO) |
			  STENCOP_PASS(FSS_STENCOP_KEEP), stencilRef);

			CRenderPrimitive& primSampling = primitives.sampling;
			primSampling.SetTechnique(pShader, techSampling, rtFlagsSampling);
			primSampling.SetRenderState(GS_STENCIL | gsDepthFunc | gsDepthTest);
			primSampling.SetCullMode(bUseLightVolumes ? eCULL_Front : eCULL_Back);
			primSampling.SetStencilState(
			  STENC_FUNC(FSS_STENCFUNC_EQUAL) |
			  STENCOP_FAIL(FSS_STENCOP_KEEP) |
			  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
			  STENCOP_PASS(FSS_STENCOP_KEEP), stencilRef);
			primSampling.SetTexture(0, CRendererResources::s_ptexLinearDepth);
			primSampling.SetTexture(1, CRendererResources::s_ptexRT_ShadowPool);
			primSampling.SetTexture(7, CRendererResources::s_ptexShadowJitterMap);
			primSampling.SetSampler(3, EDefaultSamplerStates::LinearCompare);
			primSampling.SetSampler(7, EDefaultSamplerStates::PointWrap);
			primSampling.SetPrimitiveType(bUseLightVolumes ? primitiveType : CRenderPrimitive::ePrim_ProceduralTriangle);

			PrepareConstantBuffersForPrimitives(primitives, pFrustumToRender, nS, bUseLightVolumes);

			primStencilBackFace.Compile(sliceGenPass);
			stencilPrimitives[numStencilPrimitives++] = &primStencilBackFace;

			primStencilFrontFace.Compile(sliceGenPass);
			stencilPrimitives[numStencilPrimitives++] = &primStencilFrontFace;

			primSampling.Compile(sliceGenPass);
			samplingPrimitives[numSamplingPrimitives++] = &primSampling;
		}
	}

	// now add generated primitives to slice gen pass
	for (int i = 0; i < numStencilPrimitives; ++i)
		sliceGenPass.AddPrimitive(stencilPrimitives[i]);

	for (int i = 0; i < numSamplingPrimitives; ++i)
		sliceGenPass.AddPrimitive(samplingPrimitives[i]);

	firstUnusedStencilValue += nSides + 1;
	return numStencilPrimitives + numSamplingPrimitives;
}

void CLocalLightShadows::PrepareConstantBuffersForPrimitives(SLocalLightPrimitives& primitives, SShadowFrustumToRender* pFrustumToRender, int side, bool bVolumePrimitive) const
{
	SRenderViewShaderConstants& PF = shadowMaskStage.RenderView()->GetShaderConstants();
	const bool bAreaLight = false;   // TheoM TODO

	const auto pLight = pFrustumToRender->pLight;
	const auto pFrustum = pFrustumToRender->pFrustum;

	// shadow clip space to world space transform
	Matrix44A mUnitVolumeToWorld(IDENTITY);
	Vec4 vSphereAdjust(ZERO);
	{
		Matrix44A mProjection, mView;
		if (pFrustum->bOmniDirectionalShadow)
		{
			CShadowUtils::GetCubemapFrustum(FTYP_OMNILIGHTVOLUME, pFrustum, side, &mProjection, &mView);
		}
		else
		{
			mProjection = Matrix44(IDENTITY);
			mView = pFrustum->mLightViewMatrix;
		}

		Matrix44r mViewProj = Matrix44r(mView) * Matrix44r(mProjection);
		mUnitVolumeToWorld = mViewProj.GetInverted();
		vSphereAdjust = Vec4(pLight->m_Origin, pLight->m_fRadius * 1.1f);
	}

	static ICVar* pVar = iConsole->GetCVar("e_ShadowsPoolSize");
	const int nShadowAtlasRes = pVar->GetIVal();
	float kernelSize = pFrustum->bOmniDirectionalShadow ? 2.5f : 1.5f;
	const Vec4 vShadowParams(kernelSize * (float(pFrustum->nTexSize) / nShadowAtlasRes), float(pFrustum->nTexSize), 0.0f, pFrustum->fDepthConstBias);

	Matrix44A shadowMat[2];
	for (int i = 0; i < shadowMaskStage.m_viewInfoCount; ++i)
	{
		const SRenderViewInfo& viewInfo = shadowMaskStage.m_viewInfo[i];
		auto shadowSamplingInfo = CShadowUtils::GetDeferredShadowSamplingInfo(pFrustum, side, *viewInfo.pCamera, viewInfo.viewport, shadowMaskStage.RenderView()->m_vProjMatrixSubPixoffset);

		shadowMat[i] = shadowSamplingInfo.shadowTexGen;
		const Vec4 vEye(viewInfo.cameraOrigin, 0.f);
		Vec4 vecTranslation(vEye.Dot((Vec4&)shadowMat[i].m00), vEye.Dot((Vec4&)shadowMat[i].m10), vEye.Dot((Vec4&)shadowMat[i].m20), vEye.Dot((Vec4&)shadowMat[i].m30));
		shadowMat[i].m03 += vecTranslation.x;
		shadowMat[i].m13 += vecTranslation.y;
		shadowMat[i].m23 += vecTranslation.z;
		shadowMat[i].m33 += vecTranslation.w;

		// pre-multiply by 1/frustrum_far_plane
		(Vec4&)shadowMat[i].m20 *= shadowSamplingInfo.oneDivFarDist;
	}

	// apply per view buffer to all primitives
	primitives.sampling.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, shadowMaskStage.m_pPerViewConstantBuffer, EShaderStage_Vertex | EShaderStage_Pixel);
	primitives.stencilBackfaces.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, shadowMaskStage.m_pPerViewConstantBuffer, EShaderStage_Vertex);
	primitives.stencilFrontfaces.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, shadowMaskStage.m_pPerViewConstantBuffer, EShaderStage_Vertex);

	// update custom constants.NOTE: constant buffer is shared by all three primitives
	auto& constantManager = primitives.sampling.GetConstantManager();

	auto constants = constantManager.BeginTypedConstantUpdate<STypedConstants>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex | EShaderStage_Pixel);
	constants->unitMeshTransform = mUnitVolumeToWorld;
	constants->lightVolumeSphereAdjust = vSphereAdjust;
	constants->lightShadowProj = shadowMat[0];
	constants->params = vShadowParams;
	memcpy(constants->irreg_kernel_2d, PF.irregularFilterKernel, sizeof(PF.irregularFilterKernel));
	static_assert(sizeof(constants->irreg_kernel_2d) == sizeof(PF.irregularFilterKernel), "Both sizes must be same.");
	constants->vLightPos = Vec4(pLight->m_Origin, 0.0f);

	if (shadowMaskStage.m_viewInfoCount > 1)
	{
		constants.BeginStereoOverride();
		constants->lightShadowProj = shadowMat[1];
	}

	constantManager.EndTypedConstantUpdate(constants);
}

}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/GraphicsPipelineStateSet.h"
#include "Common/SceneRenderPass.h"
#include "Common/FullscreenPass.h"
#include "Common/UtilityPasses.h"
#include "SceneGBuffer.h"

class CSceneForwardStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_SceneForward;

	struct SCloudShadingParams
	{
		Vec4 CloudShadingColorSun;
		Vec4 CloudShadingColorSky;
	};

	enum EPerPassTexture
	{
		ePerPassTexture_PerlinNoiseMap   = CSceneGBufferStage::ePerPassTexture_PerlinNoiseMap,
		ePerPassTexture_TerrainElevMap   = CSceneGBufferStage::ePerPassTexture_TerrainElevMap,
		ePerPassTexture_WindGrid         = CSceneGBufferStage::ePerPassTexture_WindGrid,
		ePerPassTexture_TerrainNormMap   = CSceneGBufferStage::ePerPassTexture_TerrainNormMap,
		ePerPassTexture_TerrainBaseMap   = CSceneGBufferStage::ePerPassTexture_TerrainBaseMap,
		ePerPassTexture_NormalsFitting   = CSceneGBufferStage::ePerPassTexture_NormalsFitting,

		ePerPassTexture_SceneLinearDepth = CSceneGBufferStage::ePerPassTexture_SceneLinearDepth,
	};

public:
	enum EPass : uint8
	{
		// limit: MAX_PIPELINE_SCENE_STAGE_PASSES
		ePass_Forward          = 0,
		ePass_ForwardPrepassed = 1,
		ePass_ForwardRecursive = 2,
		ePass_ForwardMobile    = 3,

		ePass_Count
	};

	static_assert(ePass_Count <= MAX_PIPELINE_SCENE_STAGE_PASSES,
		"The pipeline-state array is unable to carry as much pass-permutation as defined here!");

public:
	CSceneForwardStage(CGraphicsPipeline& graphicsPipeline);

	void Init() final;
	void Update() final;

	bool         CreatePipelineStates(DevicePipelineStatesArray* pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache);
	bool         CreatePipelineState(const SGraphicsPipelineStateDescription& desc,
	                                 CDeviceGraphicsPSOPtr& outPSO,
	                                 EPass passId = ePass_Forward,
	                                 const std::function<void(CDeviceGraphicsPSODesc& psoDesc, const SGraphicsPipelineStateDescription& desc)>&customState = nullptr);

	void         ExecuteOpaque();
	void         ExecuteTransparentBelowWater();
	void         ExecuteTransparentAboveWater();
	void         ExecuteTransparentDepthFixup();
	void         ExecuteTransparentLoRes(int subRes);
	void         ExecuteAfterPostProcessHDR();
	void         ExecuteAfterPostProcessLDR();
	void         ExecuteMobile();
	void         ExecuteMinimum(CTexture* pColorTex, CTexture* pDepthTex);

	bool IsTransparentLoResEnabled()      const { return CRendererCVars::CV_r_ParticlesHalfRes > 0; }
	bool IsTransparentDepthFixupEnabled() const { return CRendererCVars::CV_r_TranspDepthFixup > 0; }

	void FillCloudShadingParams(SCloudShadingParams& cloudParams, bool enable = true) const;

private:
	bool UpdatePerPassResources(bool bOnInit, bool bShadowMask = true, bool bFog = true);
	void ExecuteTransparent(bool bBelowWater);

private:
	CDeviceResourceLayoutPtr m_pOpaqueResourceLayout;
	CDeviceResourceLayoutPtr m_pOpaqueResourceLayoutMobile;
	CDeviceResourceLayoutPtr m_pTransparentResourceLayout;
	CDeviceResourceLayoutPtr m_pTransparentResourceLayoutMobile;
	CDeviceResourceLayoutPtr m_pEyeOverlayResourceLayout;

	CDeviceResourceSetDesc   m_opaquePassResources;
	CDeviceResourceSetPtr    m_pOpaquePassResourceSet;
	CDeviceResourceSetDesc   m_opaquePassResourcesMobile;
	CDeviceResourceSetPtr    m_pOpaquePassResourceSetMobile;
	CDeviceResourceSetDesc   m_transparentPassResources;
	CDeviceResourceSetPtr    m_pTransparentPassResourceSet;
	CDeviceResourceSetDesc   m_transparentPassResourcesMobile;
	CDeviceResourceSetPtr    m_pTransparentPassResourceSetMobile;
	CDeviceResourceSetDesc   m_eyeOverlayPassResources;
	CDeviceResourceSetPtr    m_pEyeOverlayPassResourceSet;
	CConstantBufferPtr       m_pPerPassCB;

	CSceneRenderPass         m_forwardOpaquePass;
	CSceneRenderPass         m_forwardOpaquePassMobile;
	CSceneRenderPass         m_forwardOverlayPass;
	CSceneRenderPass         m_forwardOverlayPassMobile;
	CSceneRenderPass         m_forwardTransparentBWPass;
	CSceneRenderPass         m_forwardTransparentAWPass;
	CSceneRenderPass         m_forwardTransparentLoResPass;
	CSceneRenderPass         m_forwardTransparentPassMobile;
	CSceneRenderPass         m_forwardHDRPass;
	CSceneRenderPass         m_forwardLDRPass;
	CSceneRenderPass         m_forwardEyeOverlayPass;

	CSceneRenderPass         m_forwardOpaqueRecursivePass;
	CSceneRenderPass         m_forwardOverlayRecursivePass;
	CSceneRenderPass         m_forwardTransparentRecursivePass;

	CStretchRectPass         m_copySceneTargetBWPass;
	CStretchRectPass         m_copySceneTargetAWPass;

	CFullscreenPass           m_depthFixupPass;
	CFullscreenPass           m_depthCopyPass;
	CNearestDepthUpsamplePass m_depthUpscalePass;
};

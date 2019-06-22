// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/GraphicsPipelineStateSet.h"
#include "Common/SceneRenderPass.h"
#include "Common/FullscreenPass.h"

struct SGraphicsPipelineStateDescription;

class CSceneGBufferStage : public CGraphicsPipelineStage
{
public:
	enum EExecutionMode
	{
		eZPassMode_Off                = 0,
		eZPassMode_GBufferOnly        = 1,
		eZPassMode_PartialZPrePass    = 2,
		eZPassMode_DiscardingZPrePass = 3,
		eZPassMode_FullZPrePass       = 4
	};

	static const EGraphicsPipelineStage StageID = eStage_SceneGBuffer;

	enum EPerPassTexture
	{
		ePerPassTexture_PerlinNoiseMap = 25,
		ePerPassTexture_WindGrid,
		ePerPassTexture_TerrainElevMap,
		ePerPassTexture_TerrainNormMap,
		ePerPassTexture_TerrainBaseMap,
		ePerPassTexture_NormalsFitting,

		ePerPassTexture_SceneLinearDepth = 32,
	};

	enum EPass : uint8
	{
		// limit: MAX_PIPELINE_SCENE_STAGE_PASSES
		ePass_FullGBufferFill  = 0,
		ePass_DepthBufferFill  = 1,
		ePass_AttrGBufferFill  = 2,
		ePass_MicroGBufferFill = 3,

		ePass_Count
	};

	static_assert(ePass_Count <= MAX_PIPELINE_SCENE_STAGE_PASSES,
		"The pipeline-state array is unable to carry as much pass-permutation as defined here!");

	CSceneGBufferStage(CGraphicsPipeline& graphicsPipeline);

	void Init() final;
	void Update() final;

	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		// TODO: GBuffer shouldn't be responsible for ZPrePass
	//	if (flags & EShaderRenderingFlags::SHDF_FORWARD_MINIMAL)
	//		return false;

		return true;
	}

	void Execute();
	void ExecuteMinimumZpass();
	void ExecuteMicroGBuffer();
	void ExecuteGBufferVisualization();

	bool CreatePipelineStates(DevicePipelineStatesArray* pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache);
	bool CreatePipelineState(const SGraphicsPipelineStateDescription& desc, EPass passID, CDeviceGraphicsPSOPtr& outPSO);

	bool IsGBufferVisualizationEnabled() const { return CRendererCVars::CV_r_DeferredShadingDebugGBuffer > 0; }

private:
	bool UpdatePerPassResources(bool bOnInit);

	void ExecuteDepthPrepass();
	void ExecuteSceneOpaque();
	void ExecuteSceneOverlays();

private:
	CDeviceResourceSetPtr    m_pPerPassResourceSet;
	CDeviceResourceSetDesc   m_perPassResources;
	CDeviceResourceLayoutPtr m_pResourceLayout;

	CSceneRenderPass         m_depthPrepass;
	CSceneRenderPass         m_opaquePass;
	CSceneRenderPass         m_opaqueVelocityPass;
	CSceneRenderPass         m_overlayPass;
	CSceneRenderPass         m_microGBufferPass;

	CFullscreenPass          m_passBufferVisualization;
};

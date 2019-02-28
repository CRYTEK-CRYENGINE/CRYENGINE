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
	enum EPerPassTexture
	{
		ePerPassTexture_PerlinNoiseMap = 25,
		ePerPassTexture_WindGrid,
		ePerPassTexture_TerrainElevMap,
		ePerPassTexture_TerrainNormMap,
		ePerPassTexture_TerrainBaseMap,
		ePerPassTexture_NormalsFitting,
		ePerPassTexture_DissolveNoise,
		ePerPassTexture_SceneLinearDepth,
	};

	enum EPass
	{
		ePass_GBufferFill  = 0,
		ePass_DepthPrepass = 1,
		ePass_MicroGBufferFill = 2,
	};

	CSceneGBufferStage();

	void Init() final;
	void Update() final;
	void Prepare(bool bPostLinearize);

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
	void ExecuteLinearizeDepth();
	void ExecuteGBufferVisualization();

	bool CreatePipelineStates(DevicePipelineStatesArray* pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache);

private:
	bool CreatePipelineState(const SGraphicsPipelineStateDescription& desc, EPass passID, CDeviceGraphicsPSOPtr& outPSO);

	bool SetAndBuildPerPassResources(bool bOnInit);

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

	CFullscreenPass          m_passDepthLinearization;
	CFullscreenPass          m_passBufferVisualization;
};

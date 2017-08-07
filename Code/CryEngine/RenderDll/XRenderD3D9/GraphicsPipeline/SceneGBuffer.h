// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/GraphicsPipelineStateSet.h"
#include "Common/SceneRenderPass.h"
#include "Common/FullscreenPass.h"

struct SGraphicsPipelineStateDescription;

class CSceneGBufferStage : public CGraphicsPipelineStage
{
	enum EPerPassTexture
	{
		ePerPassTexture_PerlinNoiseMap = 25,
		ePerPassTexture_TerrainElevMap,
		ePerPassTexture_WindGrid,
		ePerPassTexture_TerrainNormMap,
		ePerPassTexture_TerrainBaseMap,
		ePerPassTexture_NormalsFitting,
		ePerPassTexture_DissolveNoise,
		ePerPassTexture_SceneLinearDepth,

		ePerPassTexture_Count
	};

	enum EPass
	{
		ePass_GBufferFill  = 0,
		ePass_DepthPrepass = 1,
		ePass_MicroGBufferFill = 2,
	};

public:
	CSceneGBufferStage();

	virtual void Init() override;
	virtual void Prepare(CRenderView* pRenderView) override;
	void         Execute();
	void         ExecuteMicroGBuffer();
	void         ExecuteLinearizeDepth();
	void         ExecuteGBufferVisualization();

	bool         CreatePipelineStates(DevicePipelineStatesArray* pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache);

private:
	bool CreatePipelineState(const SGraphicsPipelineStateDescription& desc, EPass passID, CDeviceGraphicsPSOPtr& outPSO);

	bool SetAndBuildPerPassResources(bool bOnInit);

	void OnResolutionChanged();
	void RenderDepthPrepass();
	void RenderSceneOpaque();
	void RenderSceneOverlays();

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

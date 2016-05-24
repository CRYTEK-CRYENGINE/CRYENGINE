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
		ePerPassTexture_TerrainElevMap = 26,
		ePerPassTexture_WindGrid,
		ePerPassTexture_TerrainNormMap,
		ePerPassTexture_TerrainBaseMap,
		ePerPassTexture_NormalsFitting,
		ePerPassTexture_DissolveNoise,

		ePerPassTexture_Count
	};

	// NOTE: DXOrbis only supports 32 shader slots at this time, don't use t32 or higher if DXOrbis support is desired!
	static_assert(ePerPassTexture_Count <= 32, "Bind slot too high for DXOrbis");

	enum EPass
	{
		ePass_GBufferFill  = 0,
		ePass_DepthPrepass = 1
	};

public:
	virtual void Init() override;
	virtual void ReleaseBuffers() override;
	virtual void Prepare(CRenderView* pRenderView) override;
	void         Execute();
	void         ExecuteLinearizeDepth();

	bool         CreatePipelineStates(DevicePipelineStatesArray* pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache);

private:
	bool CreatePipelineState(const SGraphicsPipelineStateDescription& desc, EPass passID, CDeviceGraphicsPSOPtr& outPSO);

	bool PreparePerPassResources();
	bool PrepareResourceLayout();

	void OnResolutionChanged();
	void RenderDepthPrepass();
	void RenderSceneOpaque();
	void RenderSceneOverlays();

private:
	CDeviceResourceSetPtr    m_pPerPassResources;
	CDeviceResourceLayoutPtr m_pResourceLayout;

	CSceneRenderPass         m_depthPrepass;
	CSceneRenderPass         m_opaquePass;
	CSceneRenderPass         m_opaqueVelocityPass;
	CSceneRenderPass         m_overlayPass;

	CFullscreenPass          m_passDepthLinearization;
};

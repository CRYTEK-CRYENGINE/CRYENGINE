// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/GraphicsPipelineStateSet.h"
#include "Common/SceneRenderPass.h"
#include "Common/FullscreenPass.h"
#include "SceneGBuffer.h"

struct SGraphicsPipelineStateDescription;

class CSceneCustomStage : public CGraphicsPipelineStage
{
	enum EPerPassTexture
	{
		ePerPassTexture_PerlinNoiseMap   = CSceneGBufferStage::ePerPassTexture_PerlinNoiseMap,
		ePerPassTexture_TerrainElevMap   = CSceneGBufferStage::ePerPassTexture_TerrainElevMap,
		ePerPassTexture_WindGrid         = CSceneGBufferStage::ePerPassTexture_WindGrid,
		ePerPassTexture_TerrainNormMap   = CSceneGBufferStage::ePerPassTexture_TerrainNormMap,
		ePerPassTexture_TerrainBaseMap   = CSceneGBufferStage::ePerPassTexture_TerrainBaseMap,
		ePerPassTexture_NormalsFitting   = CSceneGBufferStage::ePerPassTexture_NormalsFitting,

		ePerPassTexture_SceneLinearDepth = CSceneGBufferStage::ePerPassTexture_SceneLinearDepth,

		ePerPassTexture_PaletteTexelsPerMeter = 33,
	};

public:
	static const EGraphicsPipelineStage StageID = eStage_SceneCustom;

	enum EPass : uint8
	{
		ePass_DebugViewSolid = 0,
		ePass_DebugViewWireframe,
		ePass_DebugViewOverdraw,
		ePass_DebugViewDrawModes,
		ePass_SelectionIDs, // draw highlighted objects from editor
		ePass_Silhouette,

		ePass_Count
	};

	static_assert(ePass_Count <= MAX_PIPELINE_SCENE_STAGE_PASSES,
		"The pipeline-state array is unable to carry as much pass-permutation as defined here!");

public:
	CSceneCustomStage(CGraphicsPipeline& graphicsPipeline);

	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		if (flags & EShaderRenderingFlags::SHDF_FORWARD_MINIMAL)
			return false;

		return true;
	}

	void Init() final;
	void Resize(int renderWidth, int renderHeight) final;
	void Update() final;

	void OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater) final;

	void ExecuteSilhouettePass();
	void ExecuteHelpers();
	void ExecuteDebugger();
	void ExecuteDebugOverlay();
	void ExecuteSelectionHighlight();

	bool CreatePipelineStates(DevicePipelineStatesArray* pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache);
	bool CreatePipelineState(const SGraphicsPipelineStateDescription& desc, EPass passID, CDeviceGraphicsPSOPtr& outPSO);

	bool IsDebuggerEnabled()           const;
	bool IsSelectionHighlightEnabled() const { return gEnv->IsEditor() && !gEnv->IsEditorGameMode(); }
	bool IsDebugOverlayEnabled()       const { return CRenderer::CV_e_DebugDraw > 0; }

private:
	bool UpdatePerPassResources(bool bOnInit);

private:
	CDeviceResourceSetDesc   m_perPassResources;
	CDeviceResourceSetPtr    m_pPerPassResourceSet;
	CDeviceResourceLayoutPtr m_pResourceLayout;
	CConstantBufferPtr       m_pPerPassConstantBuffer;

	CSceneRenderPass         m_debugViewPass;
	CSceneRenderPass         m_selectionIDPass;
	CSceneRenderPass         m_silhouetteMaskPass;
	CFullscreenPass          m_highlightPass;
	CFullscreenPass          m_resolvePass;

	CGpuBuffer               m_overdrawCount;
	CGpuBuffer               m_overdrawDepth;
	CGpuBuffer               m_overdrawStats;
	CGpuBuffer               m_overdrawHisto;

	int                      m_samplerPoint;
	int                      m_samplerLinear;
};

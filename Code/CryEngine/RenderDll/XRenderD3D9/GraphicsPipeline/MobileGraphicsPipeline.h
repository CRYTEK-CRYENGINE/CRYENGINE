// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipeline.h"
#include "Common/GraphicsPipelineStateSet.h"
#include "Common/Include_HLSL_CPP_Shared.h"

class CShadowMapStage;
class CSceneGBufferStage;
class CSceneForwardStage;
class CAutoExposureStage;
class CBloomStage;
class CHeightMapAOStage;
class CScreenSpaceObscuranceStage;
class CScreenSpaceReflectionsStage;
class CScreenSpaceSSSStage;
class CVolumetricFogStage;
class CFogStage;
class CVolumetricCloudsStage;
class CWaterStage;
class CSkyStage;
class CWaterRipplesStage;
class CMotionBlurStage;
class CDepthOfFieldStage;
class CToneMappingStage;
class CSunShaftsStage;
class CPostAAStage;
class CClipVolumesStage;
class CDeferredDecalsStage;
class CShadowMaskStage;
class CComputeSkinningStage;
class CComputeParticlesStage;
class CTiledLightVolumesStage;
class CTiledShadingStage;
class CSceneCustomStage;
class CLensOpticsStage;
class CPostEffectStage;
class CRainStage;
class CSnowStage;
class COmniCameraStage;
class CDepthReadbackStage;
class CMobileCompositionStage;
class CDebugRenderTargetsStage;
class CCamera;

struct SRenderViewInfo;

class CMobileGraphicsPipeline : public CGraphicsPipeline
{
public:
	CMobileGraphicsPipeline(const IRenderer::SGraphicsPipelineDescription& desc, const std::string& uniqueIdentifier, const SGraphicsPipelineKey key);

	void         Init() final;
	void         Resize(int renderWidth, int renderHeight) final;
	void         Update(EShaderRenderingFlags renderingFlags) final;
	void         Execute() final;
	void         ShutDown() final;

private:
	std::unique_ptr<CStretchRectPass>      m_HDRToFramePass;
	std::unique_ptr<CStretchRectPass>      m_PostToFramePass;
	std::unique_ptr<CStretchRectPass>      m_FrameToFramePass;

	std::unique_ptr<CDepthDownsamplePass>  m_LZSubResPass[3];
	std::unique_ptr<CStableDownsamplePass> m_HQSubResPass[2];
	std::unique_ptr<CStretchRectPass>      m_LQSubResPass[2];
};

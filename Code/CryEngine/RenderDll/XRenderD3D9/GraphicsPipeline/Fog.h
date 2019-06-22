// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"

class CFogStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_Fog;

	struct SForwardParams
	{
		Vec4  vfParams;
		Vec4  vfRampParams;
		Vec4  vfSunDir;
		Vec3  vfColGradBase;
		float padding0;
		Vec3  vfColGradDelta;
		float padding1;
		Vec4  vfColGradParams;
		Vec4  vfColGradRadial;
		// Fog shadows
		Vec4  vfShadowDarkening;
		Vec4  vfShadowDarkeningSunAmb;
	};

public:
	CFogStage(CGraphicsPipeline& graphicsPipeline)
		: CGraphicsPipelineStage(graphicsPipeline)
#if defined(VOLUMETRIC_FOG_SHADOWS)
		, m_passVolFogShadowRaycast(&graphicsPipeline)
		, m_passVolFogShadowHBlur(&graphicsPipeline)
		, m_passVolFogShadowVBlur(&graphicsPipeline)
#endif
		, m_passFog(&graphicsPipeline)
		, m_passLightning(&graphicsPipeline)
	{}

	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		return RenderView()->IsGlobalFogEnabled() && CRenderer::CV_r_UseZPass != 0;
	}

	void Init() final;
	void Resize(int renderWidth, int renderHeight) final;
	void OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater) final;
	void Execute();

	void FillForwardParams(SForwardParams& forwardParams, bool enable = true) const;

private:
	void Rescale(int resolutionScale);
	void ExecuteVolumetricFogShadow();
	f32  GetFogCullDistance() const;
	void ResizeResource(int resourceWidth, int resourceHeight);

private:
	_smart_ptr<CTexture> m_pTexInterleaveSamplePattern;
	_smart_ptr<CTexture> m_pCloudShadowTex;

#if defined(VOLUMETRIC_FOG_SHADOWS)
	CFullscreenPass m_passVolFogShadowRaycast;
	CFullscreenPass m_passVolFogShadowHBlur;
	CFullscreenPass m_passVolFogShadowVBlur;
#endif
	CFullscreenPass m_passFog;
	CFullscreenPass m_passLightning;

public:
#if defined(VOLUMETRIC_FOG_SHADOWS)
	_smart_ptr<CTexture> m_pTexVolFogShadowBuf[2];
#endif
};

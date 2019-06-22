// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"
#include "Common/UtilityPasses.h"

class CHeightMapAOStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_HeightMapAO;

	CHeightMapAOStage(CGraphicsPipeline& graphicsPipeline)
		: CGraphicsPipelineStage(graphicsPipeline)
		, m_passSampling(&graphicsPipeline)
		, m_passSmoothing(&graphicsPipeline)
		, m_passMipmapGen(&graphicsPipeline) {}

	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		return CRendererCVars::CV_r_HeightMapAO > 0;
	}

	void                    Init() final;
	void                    Resize(int renderWidth, int renderHeight) final;
	void                    OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater) final;
	void                    Update() final;

	void                    Execute();

	bool                    IsValid() const                         { return m_bHeightMapAOExecuted; }

	const ShadowMapFrustum* GetHeightMapFrustum() const             { CRY_ASSERT(m_bHeightMapAOExecuted); return m_pHeightMapFrustum; }
	CTexture*               GetHeightMapAOScreenDepthTex() const    { CRY_ASSERT(m_bHeightMapAOExecuted); return m_pHeightMapAOScreenDepthTex; }
	CTexture*               GetHeightMapAOTex() const               { CRY_ASSERT(m_bHeightMapAOExecuted); return m_pHeightMapAOTex; }
	_smart_ptr<CTexture>    GetHeightMapAODepthTex(int index) const { return m_pHeightMapAODepth[index]; }

private:
	void Rescale(int resolutionScale);

	void ResizeScreenResources(bool shouldApplyHMAO, int resourceWidth, int resourceHeight);
	void ResizeDepthResources(bool shouldApplyHMAO, int resourceWidth, int resourceHeight, ETEX_Format texFormat);

	_smart_ptr<CTexture>    m_pHeightMapAOMask[2];
	_smart_ptr<CTexture>    m_pHeightMapAODepth[2];

	CTexture*               m_pHeightMapAOScreenDepthTex; // references NO_DELETE
	CTexture*               m_pHeightMapAOTex;            // references local tex

	CFullscreenPass         m_passSampling;
	CFullscreenPass         m_passSmoothing;
	CMipmapGenPass          m_passMipmapGen;

	int                     m_nHeightMapAOConfig;
	float                   m_nHeightMapAOAmount;
	bool                    m_bHeightMapAOExecuted = false;
	const ShadowMapFrustum* m_pHeightMapFrustum;
};

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"

class CToneMappingStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_ToneMapping;

	CToneMappingStage(CGraphicsPipeline& graphicsPipeline)
		: CGraphicsPipelineStage(graphicsPipeline)
		, m_passToneMapping(&graphicsPipeline)
		, m_passFixedExposureToneMapping(&graphicsPipeline) {}

	void Execute();
	void ExecuteDebug();
	void ExecuteFixedExposure(CTexture* pColorTex, CTexture* pDepthTex);

	void DisplayDebugInfo();

	bool IsDebugInfoEnabled() const { return CRendererCVars::CV_r_HDRDebug == 1 && !RenderView()->IsRecursive(); }
	bool IsDebugDrawEnabled() const { return CRendererCVars::CV_r_HDRDebug > 1 && !RenderView()->IsRecursive(); }

private:
	_smart_ptr<CTexture> m_pColorChartTex;
	CFullscreenPass      m_passToneMapping;
	CFullscreenPass      m_passFixedExposureToneMapping;
};

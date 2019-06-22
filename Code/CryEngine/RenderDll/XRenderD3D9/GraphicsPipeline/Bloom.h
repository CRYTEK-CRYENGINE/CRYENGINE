// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"

class CBloomStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_Bloom;

	CBloomStage(CGraphicsPipeline& graphicsPipeline)
		: CGraphicsPipelineStage(graphicsPipeline)
		, m_pass1H(&graphicsPipeline)
		, m_pass1V(&graphicsPipeline)
		, m_pass2H(&graphicsPipeline)
		, m_pass2V(&graphicsPipeline) {}

	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		return CRenderer::CV_r_HDRBloom && CRenderer::CV_r_PostProcess;
	}

	void Execute();

private:
	CFullscreenPass m_pass1H;
	CFullscreenPass m_pass1V;
	CFullscreenPass m_pass2H;
	CFullscreenPass m_pass2V;
};

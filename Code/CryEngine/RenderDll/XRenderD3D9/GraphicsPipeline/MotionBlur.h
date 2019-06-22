// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"
#include "Common/UtilityPasses.h"

class CMotionBlurStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_MotionBlur;

	CMotionBlurStage(CGraphicsPipeline& graphicsPipeline)
		: CGraphicsPipelineStage(graphicsPipeline)
		, m_passPacking(&graphicsPipeline)
		, m_passTileGen1(&graphicsPipeline)
		, m_passTileGen2(&graphicsPipeline)
		, m_passNeighborMax(&graphicsPipeline)
		, m_passCopy(&graphicsPipeline)
		, m_passMotionBlur(&graphicsPipeline) {}

	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		return CRenderer::CV_r_MotionBlur && !gRenDev->m_nDisableTemporalEffects;
	}

	void Execute();

private:
	float ComputeMotionScale();

private:
	CFullscreenPass  m_passPacking;
	CFullscreenPass  m_passTileGen1;
	CFullscreenPass  m_passTileGen2;
	CFullscreenPass  m_passNeighborMax;
	CStretchRectPass m_passCopy;
	CFullscreenPass  m_passMotionBlur;
};

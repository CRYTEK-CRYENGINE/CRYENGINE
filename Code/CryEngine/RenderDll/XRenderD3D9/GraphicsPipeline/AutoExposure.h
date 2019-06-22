// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"

class CAutoExposureStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_AutoExposure;

	CAutoExposureStage(CGraphicsPipeline& graphicsPipeline)
		: CGraphicsPipelineStage(graphicsPipeline)
		, m_passLuminanceInitial(&graphicsPipeline)
		, m_passAutoExposure(&graphicsPipeline)
	{
		for (auto& pass : m_passLuminanceIteration)
			pass.SetGraphicsPipeline(&graphicsPipeline);
	}

	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		return RenderView()->GetCurrentEye() != CCamera::eEye_Right;
	}

	void Execute();

private:
	void MeasureLuminance();
	void AdjustExposure();

private:
	CFullscreenPass m_passLuminanceInitial;
	CFullscreenPass m_passLuminanceIteration[NUM_HDR_TONEMAP_TEXTURES];
	CFullscreenPass m_passAutoExposure;
};

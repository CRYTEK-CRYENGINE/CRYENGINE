// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"
#include "Common/UtilityPasses.h"

class CDepthOfFieldStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_DepthOfField;

	CDepthOfFieldStage(CGraphicsPipeline& graphicsPipeline)
		: CGraphicsPipelineStage(graphicsPipeline)
		, m_passCopySceneTarget(&graphicsPipeline)
		, m_passLayerDownscale(&graphicsPipeline)
		, m_passGather0(&graphicsPipeline)
		, m_passGather1(&graphicsPipeline)
		, m_passComposition(&graphicsPipeline)
	{
		for (auto& pass : m_passTileMinCoC)
			pass.SetGraphicsPipeline(&graphicsPipeline);
	}

	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		return CRenderer::CV_r_dof > 0;
	}

	void Execute();

private:
	Vec4 ToUnitDisk(Vec4& origin, float blades, float fstop);

private:
	CStretchRectPass m_passCopySceneTarget;
	CFullscreenPass  m_passLayerDownscale;
	CFullscreenPass  m_passTileMinCoC[MIN_DOF_COC_K];
	CFullscreenPass  m_passGather0;
	CFullscreenPass  m_passGather1;
	CFullscreenPass  m_passComposition;
};

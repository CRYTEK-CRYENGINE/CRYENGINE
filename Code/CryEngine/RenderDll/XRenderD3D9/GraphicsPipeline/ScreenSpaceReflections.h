// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"
#include "Common/UtilityPasses.h"

class CScreenSpaceReflectionsStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_ScreenSpaceReflections;

	CScreenSpaceReflectionsStage(CGraphicsPipeline& graphicsPipeline)
		: CGraphicsPipelineStage(graphicsPipeline)
		, m_passRaytracing(&graphicsPipeline)
		, m_passComposition(&graphicsPipeline)
		, m_passCopy(&graphicsPipeline)
		, m_passDownsample0(&graphicsPipeline)
		, m_passDownsample1(&graphicsPipeline)
		, m_passDownsample2(&graphicsPipeline)
		, m_passBlur0(&graphicsPipeline)
		, m_passBlur1(&graphicsPipeline)
		, m_passBlur2(&graphicsPipeline) {}

	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		return CRendererCVars::CV_r_SSReflections > 0;
	}

	void Init() final;
	void Update() final;
	void Execute();

private:
	CFullscreenPass    m_passRaytracing;
	CFullscreenPass    m_passComposition;
	CStretchRectPass   m_passCopy;
	CStretchRectPass   m_passDownsample0;
	CStretchRectPass   m_passDownsample1;
	CStretchRectPass   m_passDownsample2;
	CGaussianBlurPass  m_passBlur0;
	CGaussianBlurPass  m_passBlur1;
	CGaussianBlurPass  m_passBlur2;

	Matrix44           m_prevViewProj[MAX_GPU_NUM];
};

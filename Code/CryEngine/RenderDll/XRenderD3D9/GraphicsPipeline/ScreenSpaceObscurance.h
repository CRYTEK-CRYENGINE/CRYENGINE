// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"
#include "Common/UtilityPasses.h"

class CScreenSpaceObscuranceStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_ScreenSpaceObscurance;

	CScreenSpaceObscuranceStage(CGraphicsPipeline& graphicsPipeline)
		: CGraphicsPipelineStage(graphicsPipeline)
		, m_passCopyFromESRAM(&graphicsPipeline)
		, m_passObscurance(&graphicsPipeline)
		, m_passFilter(&graphicsPipeline)
		, m_passAlbedoDownsample0(&graphicsPipeline)
		, m_passAlbedoDownsample1(&graphicsPipeline)
		, m_passAlbedoDownsample2(&graphicsPipeline)
		, m_passAlbedoBlur(&graphicsPipeline) {}

	bool IsColorBleeding() const { return CRendererCVars::CV_r_ssdoColorBleeding != 0; }
	bool IsObscuring() const { return CRendererCVars::CV_r_ssdo != 0; }

	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		return CRendererCVars::CV_r_ssdo && !CRendererCVars::CV_r_DeferredShadingDebugGBuffer;
	}

	void Init() final;
	void Update() final;
	void Execute();

private:
	CStretchRectPass  m_passCopyFromESRAM;
	CFullscreenPass   m_passObscurance;
	CFullscreenPass   m_passFilter;
	CStretchRectPass  m_passAlbedoDownsample0;
	CStretchRectPass  m_passAlbedoDownsample1;
	CStretchRectPass  m_passAlbedoDownsample2;
	CGaussianBlurPass m_passAlbedoBlur;
};

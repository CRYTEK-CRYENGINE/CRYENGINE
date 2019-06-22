// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"

class CMobileCompositionStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_MobileComposition;

	CMobileCompositionStage(CGraphicsPipeline& graphicsPipeline)
		: CGraphicsPipelineStage(graphicsPipeline)
		, m_passDepthDownsample2(&graphicsPipeline)
		, m_passDepthDownsample4(&graphicsPipeline)
		, m_passDepthDownsample8(&graphicsPipeline)
		, m_passLighting(&graphicsPipeline)
		, m_passTonemappingTAA(&graphicsPipeline) {}

	void Init() final;
	void ExecuteDeferredLighting();
	void ExecutePostProcessing(CTexture* pOutput);

private:
	CDepthDownsamplePass m_passDepthDownsample2;
	CDepthDownsamplePass m_passDepthDownsample4;
	CDepthDownsamplePass m_passDepthDownsample8;

	CFullscreenPass      m_passLighting;
	CFullscreenPass      m_passTonemappingTAA;
};
// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"

class COmniCameraStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_OmniCamera;

	COmniCameraStage(CGraphicsPipeline& graphicsPipeline)
		: CGraphicsPipelineStage(graphicsPipeline)
		, m_cubemapToScreenPass(&graphicsPipeline)
		, m_downsamplePass(&graphicsPipeline) {};

	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		return RenderView()->GetCamera(CCamera::eEye_Left).m_bOmniCamera;
	}

	void Execute();

protected:
	CTexture*       m_pOmniCameraTexture = nullptr;
	CTexture*       m_pOmniCameraCubeFaceStagingTexture = nullptr;

	CFullscreenPass m_cubemapToScreenPass;
	CDownsamplePass m_downsamplePass;
};

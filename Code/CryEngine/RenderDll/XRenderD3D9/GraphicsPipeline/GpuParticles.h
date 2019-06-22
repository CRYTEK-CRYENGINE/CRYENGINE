// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"

class CComputeParticlesStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_ComputeParticles;

	CComputeParticlesStage(CGraphicsPipeline& graphicsPipeline);
	~CComputeParticlesStage();

	// TODO: Rework gpu particles to allow usage on multiple pipelines
	bool IsStageActive(EShaderRenderingFlags flags) const final
	{
		if (flags & EShaderRenderingFlags::SHDF_FORWARD_MINIMAL)
			return false;

		return true;
	}

	void Init() final;

	void Update() override;
	void PreDraw();
	void PostDraw();

private:
	int m_oldFrameIdExecute;
	int m_oldFrameIdPreDraw;
	int m_oldFrameIdPostDraw;
};

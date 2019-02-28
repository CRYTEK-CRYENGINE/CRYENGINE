// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"

class CComputeParticlesStage : public CGraphicsPipelineStage
{
public:
	CComputeParticlesStage();
	~CComputeParticlesStage();

	void Init() final;

	void Update() override;
	void PreDraw();
	void PostDraw();

	gpu_pfx2::CManager* GetGpuParticleManager() { return m_pGpuParticleManager.get(); }
private:
	std::unique_ptr<gpu_pfx2::CManager> m_pGpuParticleManager;
	int m_oldFrameIdExecute;
	int m_oldFrameIdPreDraw;
	int m_oldFrameIdPostDraw;
};

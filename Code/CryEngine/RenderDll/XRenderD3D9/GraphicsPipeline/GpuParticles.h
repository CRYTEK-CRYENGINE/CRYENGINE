// Copyright 2001-2016 Crytek GmbH. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"

class CGpuParticlesStage : public CGraphicsPipelineStage
{
public:
	CGpuParticlesStage();
	~CGpuParticlesStage();
	virtual void Init() override;
	virtual void Prepare(CRenderView* pRenderView) override;

	void         Execute(CRenderView* pRenderView);
	void         PreDraw(CRenderView* pRenderView);
	void         PostDraw(CRenderView* pRenderView);

	gpu_pfx2::CManager* GetGpuParticleManager() { return m_pGpuParticleManager.get(); }
private:
	std::unique_ptr<gpu_pfx2::CManager> m_pGpuParticleManager;
	int m_oldFrameIdExecute;
	int m_oldFrameIdPreDraw;
	int m_oldFrameIdPostDraw;
};

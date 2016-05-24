// Copyright 2001-2015 Crytek GmbH. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Gpu/GpuComputeBackend.h"

class CComputeSkinningStage : public CGraphicsPipelineStage
{
public:
	CComputeSkinningStage();
	virtual void Init() override;
	virtual void Prepare(CRenderView* pRenderView) override;

	void         Execute(CRenderView* pRenderView);
private:
	void         DispatchComputeShaders(CRenderView* pRenderView);
};

// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"
#include "Common/UtilityPasses.h"

class CMotionBlurStage : public CGraphicsPipelineStage
{
public:
	void Init();
	void Execute();

private:
	float ComputeMotionScale();

private:
	CFullscreenPass    m_passPacking;
	CFullscreenPass    m_passTileGen1;
	CFullscreenPass    m_passTileGen2;
	CFullscreenPass    m_passNeighborMax;
	CStretchRectPass   m_passCopy;
	CFullscreenPass    m_passMotionBlur;
};

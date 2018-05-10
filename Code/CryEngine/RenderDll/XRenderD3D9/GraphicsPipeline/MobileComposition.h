// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"

class CMobileCompositionStage : public CGraphicsPipelineStage
{
public:
	void Init() final;
	void Execute();

private:
	CDepthDownsamplePass m_passDepthDownsample2;
	CDepthDownsamplePass m_passDepthDownsample4;
	CDepthDownsamplePass m_passDepthDownsample8;
	
	CFullscreenPass m_passLighting;
	CFullscreenPass m_passTonemappingTAA;
};
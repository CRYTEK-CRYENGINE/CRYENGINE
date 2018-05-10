// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"

class CAutoExposureStage : public CGraphicsPipelineStage
{
public:
	void Execute();

private:
	void MeasureLuminance();
	void AdjustExposure();

private:
	CFullscreenPass m_passLuminanceInitial;
	CFullscreenPass m_passLuminanceIteration[NUM_HDR_TONEMAP_TEXTURES];
	CFullscreenPass m_passAutoExposure;
};

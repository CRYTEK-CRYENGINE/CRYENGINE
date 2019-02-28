// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"

class CBloomStage : public CGraphicsPipelineStage
{
public:
	void Execute();

private:
	CFullscreenPass m_pass1H;
	CFullscreenPass m_pass1V;
	CFullscreenPass m_pass2H;
	CFullscreenPass m_pass2V;
};

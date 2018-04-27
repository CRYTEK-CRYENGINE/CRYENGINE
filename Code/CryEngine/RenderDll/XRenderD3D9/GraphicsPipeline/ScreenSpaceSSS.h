// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"

// Screen Space Subsurface Scattering
class CScreenSpaceSSSStage : public CGraphicsPipelineStage
{
public:
	void Execute(CTexture* pIrradianceTex);

private:
	CFullscreenPass m_passH;
	CFullscreenPass m_passV;
};

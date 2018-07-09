// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"
#include "Common/UtilityPasses.h"

class CScreenSpaceObscuranceStage : public CGraphicsPipelineStage
{
public:
	void Init();
	void Execute();

private:
	CStretchRectPass  m_passCopyFromESRAM;
	CFullscreenPass   m_passObscurance;
	CFullscreenPass   m_passFilter;
	CStretchRectPass  m_passAlbedoDownsample0;
	CStretchRectPass  m_passAlbedoDownsample1;
	CStretchRectPass  m_passAlbedoDownsample2;
	CGaussianBlurPass m_passAlbedoBlur;
};

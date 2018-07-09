// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"

class COmniCameraStage : public CGraphicsPipelineStage
{
public:
	COmniCameraStage() = default;

	void Execute();
	bool IsEnabled() const;

protected:
	CTexture* m_pOmniCameraTexture = nullptr;
	CTexture* m_pOmniCameraCubeFaceStagingTexture = nullptr;

	CFullscreenPass m_cubemapToScreenPass;
	CDownsamplePass m_downsamplePass;
};

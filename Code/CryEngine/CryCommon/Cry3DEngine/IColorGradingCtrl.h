// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// Front End of 3dEngine for ColorGrading
struct IColorGradingCtrl
{
	virtual ~IColorGradingCtrl() {}

	//! Sets a new main color grading texture. It will be fully opaque in timeToFadeInSeconds
	// In [0; timeToFadeInSeconds] all existing textures will be blended together.
	// After timeToFadeInSeconds texture from this command will be the only one
	// Commands are stored in LILO manner: one frame will issue only one command to the renderer
	
	// If texture is empty or == '0', neutral color chart will be used
	// If timeToFadeInSeconds == 0, texture will be switched without blending transition between current state and new texture
	// Condition when Color Grading Stage is not running at all is here: CColorGradingStage::IsStageActive()
	virtual void SetColorGradingLut(const char* szTexture, float timeToFadeInSeconds) = 0;

	virtual void Serialize(TSerialize& ar) = 0;

	static const int s_colorChartSize = 16;
	static const int s_colorChartWidth = s_colorChartSize * s_colorChartSize;
	static const int s_colorChartHeight = s_colorChartSize;
	static constexpr const char* s_szDefaultIdentityLut = "%ENGINE%/EngineAssets/Textures/default_cch.tif";
};

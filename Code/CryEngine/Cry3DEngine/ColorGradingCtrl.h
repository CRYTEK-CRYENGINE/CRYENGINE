// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Cry3DEngine/IColorGradingCtrl.h>
#include <vector>

struct IRenderView;
struct ITexture;

class CColorGradingTextureLoader
{
public:
	struct STextureToLoad
	{
		ITexture* pTexture;
		float     timeToFadeInSeconds;
		string    texturePath;
	};

	// Command to wait till next frame
	void LoadTexture(const string& colorGradingTexturePath, float timeToFadeInSeconds);
	void Reset();

	// Get valid textures from last frame request
	std::vector<STextureToLoad> Update();

private:
	// Double buffering: one frame delay requires to process texture load
	std::vector<STextureToLoad> m_thisFrameRequest;
	std::vector<STextureToLoad> m_prevFrameRequest;
};

class CColorGradingCtrl : public IColorGradingCtrl
{
public:
	void Init();
	void Reset();

	// Prepares RenderView for rendering
	void UpdateRenderView(IRenderView& renderView, float timeSinceLastFrame);

	void Serialize(TSerialize& ar) override;

private:
	virtual void SetColorGradingLut(const char* szTexture, float timeToFadeInSeconds) override;

	void ProcessNewCharts();
	void Update(float timeSinceLastFrame);
	void FadeOutOtherLayers();
	void RemoveFadedOutLayers();
	void FillRenderView(IRenderView& renderView);

	struct SColorChart
	{
		SColorChart(int texID, float blendAmount, float timeToFadeInInSeconds, const string& texturePath);

		void FadeIn(float timeSinceLastFrame);
		void FadeOut(float blendAmountOfFadingInGradient);

		int    texID;
		float  blendAmount;
		float  timeToFadeInInSeconds;
		float  elapsedTime;
		float  maximumBlendAmount;
		string texturePath;
	};

	CColorGradingTextureLoader m_textureLoader;
	std::vector<SColorChart>   m_charts;
};

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************

-------------------------------------------------------------------------
History:
- 15:05:2009   Created by Federico Rebora

*************************************************************************/

#ifndef COLOR_GRADIENT_MANAGER_H_INCLUDED
#define COLOR_GRADIENT_MANAGER_H_INCLUDED

#include <CryRenderer/IColorGradingController.h>

class CColorGradientManager
{
public:
	CColorGradientManager();

	void TriggerFadingColorGradient(const string& filePath, const CTimeValue& fadeInTimeInSeconds);

	void UpdateForThisFrame(const CTimeValue& frameTimeInSeconds);
	void Reset();
	void Serialize(TSerialize serializer);

private:
	void FadeInLastLayer(const CTimeValue& frameTimeInSeconds);
	void FadeOutCurrentLayers();
	void RemoveZeroWeightedLayers();
	void SetLayersForThisFrame();
	void LoadGradients();

	IColorGradingController& GetColorGradingController();

private:

	class LoadedColorGradient
	{
	public:
		LoadedColorGradient(const string& filePath, const SColorChartLayer& layer, const CTimeValue& fadeInTimeInSeconds);

	public:
		void FadeIn(const CTimeValue& frameTimeInSeconds);
		void FadeOut(const float blendAmountOfFadingInGradient);

		void FreezeMaximumBlendAmount();

		SColorChartLayer m_layer;
		string m_filePath;
		CTimeValue m_fadeInTimeInSeconds;
		CTimeValue m_elapsedTime;
		float m_maximumBlendAmount;
	};

	class LoadingColorGradient
	{
	public:
		LoadingColorGradient(const string& filePath, const CTimeValue& fadeInTimeInSeconds);

		LoadedColorGradient Load(IColorGradingController& colorGradingController) const;

	public:
		string m_filePath;
		CTimeValue m_fadeInTimeInSeconds;
	};

private:

	std::vector<LoadingColorGradient> m_colorGradientsToLoad;
	std::vector<LoadedColorGradient> m_currentGradients;
};

#endif //COLOR_GRADIENT_MANAGER_H_INCLUDED
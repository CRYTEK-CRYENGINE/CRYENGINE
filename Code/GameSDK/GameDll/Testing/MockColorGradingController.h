// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************

-------------------------------------------------------------------------
History:
- 28:05:2009   Created by Federico Rebora

*************************************************************************/

#pragma once

#ifndef MOCK_COLOR_GRADING_CONTROLLER_H_INCLUDED
#define MOCK_COLOR_GRADING_CONTROLLER_H_INCLUDED

#include "EngineFacade/3DEngine.h"

namespace GameTesting
{
	class CMockEngineColorGradingController : public EngineFacade::CNullEngineColorGradingController
	{
	public:
		CMockEngineColorGradingController();

		virtual int LoadColorChart(const char* colorChartFilePath) const;

		virtual void UnloadColorChart(int textureID) const;

		void SetLayers(const SColorChartLayer* layers, uint32 numLayers);

		bool AnyChartsWereLoaded() const;

		int GetLastUnloadedTextureID() const;

		void ClearLoadedCharts();

		const string& GetPathOfLastLoadedColorChart() const;

		const string& GetPathOfLoadedColorChart(const unsigned int index) const;

		bool LayersHaveBeenSet() const;

		uint32 GetLayerCount() const;

		size_t GetNumberOfLayersSet() const;

		const SColorChartLayer& GetCurrentlySetLayerByIndex(const uint32 index) const;

		void SetFakeIDForLoadedTexture(const int id);

		bool WasNullPointerSetOnLayers() const;

	private:
		mutable std::vector<string> m_loadedChartsPaths;
		bool m_wasNullPointerSetOnLayers;
		std::vector<SColorChartLayer> m_currentLayers;
		int m_fakeIDForLoadedTexture;
		mutable int m_lastUnloadedTextureID;
	};
}

#endif

// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ColorGradingCtrl.h"

#include <CryCore/StlUtils.h>
#include <CryRenderer/IRenderView.h>
#include <CryRenderer/IRenderer.h>
#include <CrySystem/ConsoleRegistration.h>

namespace ColorGradingCtrl_Private {

void Command_ColorGradingChartImage(IConsoleCmdArgs* pCmd)
{
	if (!pCmd && 2 != pCmd->GetArgCount())
	{
		return;
	}

	IColorGradingCtrl* pCtrl = gEnv->p3DEngine->GetColorGradingCtrl();
	if (!pCtrl)
	{
		return;
	}

	const char* pArg = pCmd->GetArg(1);
	if (pArg && pArg[0])
	{
		pCtrl->SetColorGradingLut(pArg, 0);
	}
}
} // namespace ColorGradingCtrl_Private

//////////////////////////////////////////////////////////////////////////
void CColorGradingTextureLoader::LoadTexture(const string& colorGradingTexturePath, float timeToFadeInSeconds)
{
	const uint32 COLORCHART_TEXFLAGS = FT_NOMIPS | FT_DONT_STREAM | FT_STATE_CLAMP;
	ITexture* pTexture = gEnv->pRenderer->EF_LoadTexture(colorGradingTexturePath, COLORCHART_TEXFLAGS);

	if (!pTexture)
	{
		gEnv->pLog->Log("Failed to load color grading texture: %s", colorGradingTexturePath.c_str());
		return;
	}

	m_thisFrameRequest.emplace_back(STextureToLoad{ pTexture, timeToFadeInSeconds, colorGradingTexturePath });
}

void CColorGradingTextureLoader::Reset()
{
	stl::free_container(m_thisFrameRequest);
	stl::free_container(m_prevFrameRequest);
}

std::vector<CColorGradingTextureLoader::STextureToLoad> CColorGradingTextureLoader::Update()
{
	std::vector<CColorGradingTextureLoader::STextureToLoad> loadedTexturesSinceLastFrame;
	for (const auto& chart : m_prevFrameRequest)
	{
		if (!chart.pTexture->IsTextureLoaded())
		{
			gEnv->pLog->Log("Failed to load color grading texture: %s", chart.pTexture->GetName());
		}
		else if (chart.pTexture->GetWidth() != IColorGradingCtrl::s_colorChartWidth || chart.pTexture->GetHeight() != IColorGradingCtrl::s_colorChartHeight)
		{
			gEnv->pLog->Log("Unsupported color chart texture dimensions: %s", chart.pTexture->GetName());
		}
		else
		{
			loadedTexturesSinceLastFrame.emplace_back(chart);
		}
	}
	
	m_prevFrameRequest.clear();
	m_thisFrameRequest.swap(m_prevFrameRequest);

	return loadedTexturesSinceLastFrame;
}

//////////////////////////////////////////////////////////////////////////
CColorGradingCtrl::SColorChart::SColorChart(int texID_, float blendAmount_, float timeToFadeInInSeconds_, const string& texturePath_)
	: texID(texID_)
	, blendAmount(blendAmount_)
	, timeToFadeInInSeconds(timeToFadeInInSeconds_)
	, elapsedTime(0.f)
	, maximumBlendAmount(1.f)
	, texturePath(texturePath_)
{}

void CColorGradingCtrl::SColorChart::FadeIn(float timeSinceLastFrame)
{
	if (timeToFadeInInSeconds < 0.f)
	{
		blendAmount = 1.0f;
		return;
	}

	elapsedTime += timeSinceLastFrame;

	blendAmount = elapsedTime / timeToFadeInInSeconds;
	blendAmount = min(blendAmount, 1.0f);
}

void CColorGradingCtrl::SColorChart::FadeOut(float blendAmountOfFadingInGradient)
{
	blendAmount = maximumBlendAmount * (1.0f - blendAmountOfFadingInGradient);
}

void CColorGradingCtrl::Init()
{
	REGISTER_COMMAND("r_ColorGradingChartImage", &ColorGradingCtrl_Private::Command_ColorGradingChartImage, 0,
	                 "If called with a parameter it loads the color chart immediately.\n"
					 "This image will overwrite all current loaded layers.\n"
	                 "To reset to a neutral color chart, call r_ColorGradingChartImage 0.\n"
	                 "Usage: r_ColorGradingChartImage [path of color chart image/ '0']");
}

void CColorGradingCtrl::Reset()
{
	m_textureLoader.Reset();
	stl::reconstruct(m_charts);
}

void CColorGradingCtrl::SetColorGradingLut(const char* szTexture, float timeToFadeInSeconds)
{
	const string texture(szTexture);
	if (!texture.empty() && texture != "0")
	{
		m_textureLoader.LoadTexture(texture, timeToFadeInSeconds);
	}
	else
	{
		Reset();
	}
}

void CColorGradingCtrl::UpdateRenderView(IRenderView& renderView, float timeSinceLastFrame)
{
	ProcessNewCharts();
	Update(timeSinceLastFrame);
	RemoveFadedOutLayers();
	FillRenderView(renderView);
}

void CColorGradingCtrl::ProcessNewCharts()
{
	auto newCharts = m_textureLoader.Update();
	if (newCharts.empty())
	{
		return;
	}

	// Current implementation treats the last layer as it should be the only one in fading time. The rest will be faded out.
	// In this case new layers but first one will not be used (initial their opacity will == 0)
	// Also notice, they can have different blend in timings

	const auto& newChart = newCharts.back();

	if (newChart.timeToFadeInSeconds < 0.01f)
	{
		// Switch to this color chart immediately, clean up all other layers
		m_charts.clear();
		m_charts.emplace_back(newChart.pTexture->GetTextureID(), 1.f, 0.f, newChart.texturePath);
	}
	else
	{
		// Freeze maximum blend amount
		for (auto& chart : m_charts)
		{
			chart.maximumBlendAmount = chart.blendAmount;
		}

		m_charts.emplace_back(newChart.pTexture->GetTextureID(), 0.f, newChart.timeToFadeInSeconds, newChart.texturePath);
	}
}

void CColorGradingCtrl::Update(float timeSinceLastFrame)
{
	// No charts mean don't run the ColorGradingPass on renderer
	if (m_charts.empty())
	{
		return;
	}

	// One chart: no blending is required
	if (m_charts.size() == 1)
	{
		if (m_charts[0].blendAmount != 1.f)
		{
			m_charts[0].blendAmount = 1.f;
		}
		return;
	}

	// Several charts: current (except the last one) are faded out, the last is faded in
	m_charts.back().FadeIn(timeSinceLastFrame);
	FadeOutOtherLayers();
}

void CColorGradingCtrl::FadeOutOtherLayers()
{
	if (m_charts.size() < 2)
	{
		return;
	}

	const float fadingInLayerBlendAmount = m_charts.back().blendAmount;

	const size_t numberOfFadingOutGradients = m_charts.size() - 1;
	for (size_t i = 0; i < numberOfFadingOutGradients; ++i)
	{
		m_charts[i].FadeOut(fadingInLayerBlendAmount);
	}
}

void CColorGradingCtrl::RemoveFadedOutLayers()
{
	auto newEnd = std::remove_if(m_charts.begin(), m_charts.end(), [](const SColorChart& chart) { return !chart.blendAmount; });
	m_charts.erase(newEnd, m_charts.end());
}

void CColorGradingCtrl::FillRenderView(IRenderView& renderView)
{
	auto& output = renderView.GetRendererData().colorGrading.charts;
	output.clear();
	output.reserve(m_charts.size());

	for (const auto& chart : m_charts)
	{
		output.emplace_back(RD::SColorGradingInfo::SChart{ chart.texID, chart.blendAmount });
	}
}

void CColorGradingCtrl::Serialize(TSerialize& ar)
{
	// Works only with the last loaded chart: it will be faded in
	ar.BeginGroup("CColorGradingCtrl");

	if (ar.IsWriting())
	{
		if (ar.BeginOptionalGroup("ColorChart", !m_charts.empty()))
		{
			ar.Value("FilePath", m_charts.back().texturePath);
			ar.EndGroup();
		}
	}
	else
	{
		Reset();

		if (ar.BeginOptionalGroup("ColorChart", true))
		{
			string texturePath;
			ar.Value("FilePath", texturePath);
			if (!texturePath.empty())
			{
				m_textureLoader.LoadTexture(texturePath, 0);
			}
			ar.EndGroup();
		}
	}

	ar.EndGroup();
}

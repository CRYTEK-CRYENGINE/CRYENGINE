// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************

-------------------------------------------------------------------------
History:
- 15:05:2009   Created by Federico Rebora

*************************************************************************/

#include "StdAfx.h"

#include "ColorGradientManager.h"
#include <CryFlowGraph/IFlowBaseNode.h>

CColorGradientManager::CColorGradientManager()
{
}

void CColorGradientManager::Reset()
{
	stl::free_container(m_colorGradientsToLoad);

	for (std::vector<LoadedColorGradient>::iterator it = m_currentGradients.begin(), itEnd = m_currentGradients.end(); it != itEnd; ++ it)
	{
		LoadedColorGradient& cg = *it;
		if (cg.m_layer.m_texID >= 0)
		{
			GetColorGradingController().UnloadColorChart(cg.m_layer.m_texID);
		}
	}

	stl::free_container(m_currentGradients);
}

void CColorGradientManager::Serialize(TSerialize serializer)
{
	if(serializer.IsReading())
		Reset();

	serializer.BeginGroup("ColorGradientManager");
	{
		int numToLoad = (int)m_colorGradientsToLoad.size();
		int numLoaded = (int)m_currentGradients.size();
		int numGradients = numToLoad + numLoaded;
		serializer.Value("ColorGradientCount", numGradients);
		if(serializer.IsWriting())
		{
			for(int i=0; i<numToLoad; ++i)
			{
				LoadingColorGradient& gradient = m_colorGradientsToLoad[i];
				serializer.BeginGroup("ColorGradient");
				serializer.Value("FilePath", gradient.m_filePath);
				serializer.Value("FadeInTime", gradient.m_fadeInTimeInSeconds);
				serializer.EndGroup();
			}
			for(int i=0; i<numLoaded; ++i)
			{
				LoadedColorGradient& gradient = m_currentGradients[i];
				serializer.BeginGroup("ColorGradient");
				serializer.Value("FilePath", gradient.m_filePath);
				serializer.Value("BlendAmount", gradient.m_layer.m_blendAmount);
				serializer.Value("FadeInTime", gradient.m_fadeInTimeInSeconds);
				serializer.Value("ElapsedTime", gradient.m_elapsedTime);
				serializer.Value("MaximumBlendAmount", gradient.m_maximumBlendAmount);
				serializer.EndGroup();
			}
		}
		else
		{
			m_currentGradients.reserve(numGradients);
			for(int i=0; i<numGradients; ++i)
			{
				serializer.BeginGroup("ColorGradient");
				string filePath;
				float blendAmount = 1.0f;
				float fadeInTimeInSeconds = 0.0f;
				serializer.Value("FilePath", filePath);
				serializer.Value("BlendAmount", blendAmount);
				serializer.Value("FadeInTime", fadeInTimeInSeconds);
				const int textureID = GetColorGradingController().LoadColorChart(filePath);
				LoadedColorGradient gradient(filePath, SColorChartLayer(textureID, blendAmount), fadeInTimeInSeconds);

				// Optional
				serializer.ValueWithDefault("ElapsedTime", gradient.m_elapsedTime, 0.0f);
				serializer.ValueWithDefault("MaximumBlendAmount", gradient.m_maximumBlendAmount, 1.0f);

				m_currentGradients.push_back(gradient);
				serializer.EndGroup();
			}
		}
		serializer.EndGroup();
	}
}

void CColorGradientManager::TriggerFadingColorGradient(const string& filePath, const float fadeInTimeInSeconds)
{
	const unsigned int numGradients = (int) m_currentGradients.size();
	for (unsigned int currentGradientIndex = 0; currentGradientIndex < numGradients; ++currentGradientIndex)
	{
		m_currentGradients[currentGradientIndex].FreezeMaximumBlendAmount();
	}

	m_colorGradientsToLoad.push_back(LoadingColorGradient(filePath, fadeInTimeInSeconds));
}

void CColorGradientManager::UpdateForThisFrame(const float frameTimeInSeconds)
{
	RemoveZeroWeightedLayers();
	
	LoadGradients();

	FadeInLastLayer(frameTimeInSeconds);
	FadeOutCurrentLayers();

	SetLayersForThisFrame();
}

void CColorGradientManager::FadeInLastLayer(const float frameTimeInSeconds)
{
	if (m_currentGradients.empty())
	{
		return;
	}

	m_currentGradients.back().FadeIn(frameTimeInSeconds);
}

void CColorGradientManager::FadeOutCurrentLayers()
{
	if (m_currentGradients.size() <= 1u)
	{
		return;
	}

	const unsigned int numberofFadingOutGradients = (int) m_currentGradients.size() - 1;
	const float fadingInLayerBlendAmount = m_currentGradients[numberofFadingOutGradients].m_layer.m_blendAmount;
	for (unsigned int index = 0; index < numberofFadingOutGradients; ++index)
	{
		m_currentGradients[index].FadeOut(fadingInLayerBlendAmount);
	}
}

void CColorGradientManager::RemoveZeroWeightedLayers()
{
	std::vector<LoadedColorGradient>::iterator currentGradient = m_currentGradients.begin();

	while (currentGradient != m_currentGradients.end())
	{
		if (currentGradient->m_layer.m_blendAmount == 0.0f)
		{
			GetColorGradingController().UnloadColorChart(currentGradient->m_layer.m_texID);

			currentGradient = m_currentGradients.erase(currentGradient);
		}
     else
     {
       ++currentGradient;
     }
	}
}

void CColorGradientManager::SetLayersForThisFrame()
{
	std::vector<SColorChartLayer> thisFrameLayers;

	const unsigned int numberOfFadingInGradients = (int) m_currentGradients.size();
	thisFrameLayers.reserve(numberOfFadingInGradients + thisFrameLayers.size());
	for (unsigned int index = 0; index < numberOfFadingInGradients; ++index)
	{
		thisFrameLayers.push_back(m_currentGradients[index].m_layer);
	}

	const uint32 numLayers = (uint32) thisFrameLayers.size();
	const SColorChartLayer* pLayers = numLayers ? &thisFrameLayers.front() : 0;
	GetColorGradingController().SetLayers(pLayers, numLayers);
}

void CColorGradientManager::LoadGradients()
{
	const unsigned int numGradientsToLoad = (int) m_colorGradientsToLoad.size();
	m_currentGradients.reserve(numGradientsToLoad +  m_currentGradients.size());
	for (unsigned int index = 0; index < numGradientsToLoad; ++index)
	{
		LoadedColorGradient loadedGradient = m_colorGradientsToLoad[index].Load(GetColorGradingController());
		
		m_currentGradients.push_back(loadedGradient);
	}

	m_colorGradientsToLoad.clear();
}

IColorGradingController& CColorGradientManager::GetColorGradingController()
{
	return *gEnv->pRenderer->GetIColorGradingController();
}

CColorGradientManager::LoadedColorGradient::LoadedColorGradient(const string& filePath, const SColorChartLayer& layer, const float fadeInTimeInSeconds)
: m_filePath(filePath)
, m_layer(layer)
, m_fadeInTimeInSeconds(fadeInTimeInSeconds)
, m_elapsedTime(0.0f)
, m_maximumBlendAmount(1.0f)
{

}

void CColorGradientManager::LoadedColorGradient::FadeIn(const float frameTimeInSeconds)
{
	if (m_fadeInTimeInSeconds == 0.0f)
	{
		m_layer.m_blendAmount = 1.0f;

		return;
	}

	m_elapsedTime += frameTimeInSeconds;

	const float blendAmount = m_elapsedTime / m_fadeInTimeInSeconds;

	m_layer.m_blendAmount = min(blendAmount, 1.0f);
}

void CColorGradientManager::LoadedColorGradient::FadeOut( const float blendAmountOfFadingInGradient )
{
	m_layer.m_blendAmount = m_maximumBlendAmount * (1.0f - blendAmountOfFadingInGradient);
}

void CColorGradientManager::LoadedColorGradient::FreezeMaximumBlendAmount()
{
	m_maximumBlendAmount = m_layer.m_blendAmount;
}

CColorGradientManager::LoadingColorGradient::LoadingColorGradient(const string& filePath, const float fadeInTimeInSeconds)
: m_filePath(filePath)
, m_fadeInTimeInSeconds(fadeInTimeInSeconds)
{

}

CColorGradientManager::LoadedColorGradient CColorGradientManager::LoadingColorGradient::Load(IColorGradingController& colorGradingController) const
{
	const int textureID = colorGradingController.LoadColorChart(m_filePath);

	return LoadedColorGradient(m_filePath, SColorChartLayer(textureID, 1.0f), m_fadeInTimeInSeconds);
}

class CFlowNode_ColorGradient : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum InputPorts
	{
		eIP_Trigger,
	};
	static const SInputPortConfig inputPorts[];

	CFlowNode_ColorGradient(SActivationInfo* pActivationInformation);
	~CFlowNode_ColorGradient();

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActivationInformation);
	virtual void         GetMemoryUsage(ICrySizer* sizer) const;

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_ColorGradient(pActInfo); }

	enum EInputPorts
	{
		eInputPorts_Trigger,
		eInputPorts_TexturePath,
		eInputPorts_TransitionTime,
		eInputPorts_Count,
	};

private:
	//	IGameEnvironment& m_environment;
	ITexture* m_pTexture;
};

const SInputPortConfig CFlowNode_ColorGradient::inputPorts[] =
{
	InputPortConfig_Void("Trigger",            _HELP("")),
	InputPortConfig<string>("tex_TexturePath", _HELP("Path to the Color Chart texture.")),
	InputPortConfig<float>("TransitionTime",   _HELP("Fade in time (Seconds).")),
	{ 0 },
};

CFlowNode_ColorGradient::CFlowNode_ColorGradient(SActivationInfo* pActivationInformation)
	: m_pTexture(NULL)
{
}

CFlowNode_ColorGradient::~CFlowNode_ColorGradient()
{
	SAFE_RELEASE(m_pTexture);
}

void CFlowNode_ColorGradient::GetConfiguration(SFlowNodeConfig& config)
{
	config.pInputPorts = inputPorts;
	config.SetCategory(EFLN_ADVANCED);
}

void CFlowNode_ColorGradient::ProcessEvent(EFlowEvent event, SActivationInfo* pActivationInformation)
{
	if (!gEnv->pRenderer)
	{
		return;
	}

	//Preload texture
	if (event == IFlowNode::eFE_PrecacheResources && m_pTexture == nullptr)
	{
		const string texturePath = GetPortString(pActivationInformation, eInputPorts_TexturePath);
		const uint32 COLORCHART_TEXFLAGS = FT_NOMIPS | FT_DONT_STREAM | FT_STATE_CLAMP;
		m_pTexture = gEnv->pRenderer->EF_LoadTexture(texturePath.c_str(), COLORCHART_TEXFLAGS);
	}

	if (event == IFlowNode::eFE_Activate && IsPortActive(pActivationInformation, eIP_Trigger))
	{
		const string texturePath = GetPortString(pActivationInformation, eInputPorts_TexturePath);
		const float timeToFade = GetPortFloat(pActivationInformation, eInputPorts_TransitionTime);
		CCryAction::GetCryAction()->GetColorGradientManager()->TriggerFadingColorGradient(texturePath, timeToFade);
	}
}

void CFlowNode_ColorGradient::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
}

REGISTER_FLOW_NODE("Image:ColorGradient", CFlowNode_ColorGradient);
